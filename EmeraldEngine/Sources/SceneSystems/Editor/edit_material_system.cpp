#include "SceneSystems/Editor/edit_material_system.h"
#include "Texture/texture_resource_utility.h"
#include "System/asset_browser.h"

#include <algorithm>
#include <cctype>
#include <filesystem>

#include "Model/model_instance.h"
#include "Resource/resource_manager.h"
#include "Graphics/View/camera.h"

#include "Object/Graphics/light_object.h"
#include "Object/Graphics/light_probe_object.h"

#if defined(USE_IMGUI)
#include "imgui.h"

namespace
{
	//	マテリアルを含むファイル
	constexpr const TCHAR* SourceFileDialogFilter =
		L"マテリアルを含むファイル (*.emr;*.emo;*.eme)\0*.emr;*.emo;*.eme;\0"
		L"マテリアルファイル (*.emr)\0*.emr;\0"
		L"モデルファイル (*.emo)\0*.emo;\0"
		L"エフェクトファイル (*.eme)\0*.eme;\0"
		L"すべてのファイル (*.*)\0*.*\0";

	//	プレビューモデルを探すディレクトリと、最初に表示するモデル
	constexpr std::string_view preview_model_directory = "Assets/Resources/Models";

	//	内蔵の形の大きさ(直径・一辺)と床の大きさ
	const float preview_shape_size = system_units::convert_m_to_unit(1.0f);
	const float floor_size = system_units::convert_m_to_unit(10.0f);

	//	平面を床と重ならないよう浮かせる高さ
	const double plane_lift = system_units::convert_m_to_unit(0.01);

	//	仮のエフェクトを作り直すまでの時間
	constexpr float domain_preview_effect_time = 10.0f;

	//	プレビューモデルを正面から見るカメラの位置と向き
	const math::vector3d preview_camera_position = system_units::convert_m_to_unit(math::vector3d(0.0, 0.5, 4.0));
	const math::rotatord preview_camera_rotation(0.0, math::constants<double>::pi, 0.0);

	//	ファイルパスを ImGui 表示用の UTF-8 にする
	std::string to_display(std::string_view text)
	{
		return core::convert_utf8(text);
	}

	bool has_extension(const std::string& file_path, std::string_view extension)
	{
		return core::to_lower(std::filesystem::path(file_path).extension().string()) == extension;
	}

	//	編集用の複製を作る
	material_resource_ptr make_working_copy(const material_resource_ptr& original)
	{
		auto working = std::make_shared<material_resource>();
		original->duplicate(*working);
		return working;
	}
}

edit_material_system::edit_material_system()
: scene_system()
{
}

edit_material_system::~edit_material_system()
{
}

bool edit_material_system::initialize_core()
{
	//	メインカメラ生成
	{
		main_camera = std::make_shared<camera_object>("メインカメラ");
		main_camera->get_camera()->get_parameters().fov_y = math::angle::to_radian(30.0f);
		//	エディターカメラは有効になった時にメインカメラの視点を引き継ぐので、メインカメラをプレビューモデルに向けておく
		main_camera->set_local_translation(preview_camera_position);
		main_camera->set_local_euler(preview_camera_rotation);
		main_camera->get_camera()->get_parameters().position = preview_camera_position;
		main_camera->get_camera()->get_parameters().rotation = math::rotatorf(preview_camera_rotation);
		register_object(main_camera);
		mouse::instance()->set_center_lock(false);
		mouse::instance()->set_cursol_hide(false);
	}

	//	ライト生成
	{
		//	平行光源生成
		{
			auto light = std::make_shared<directional_light_object>("平行光源");
			register_object(light);
		}

		//	間接光生成
		{
			auto sky_light = std::make_shared<sky_light_object>("スカイライト");

			texture_resource_ptr ibl_texture;
			ibl_texture = texture_resource_utility::load_texture_resource("Assets/Resources/Textures/hilly_terrain_01_puresky_4k/diffuse_iem.etx");
			sky_light->set_diffuse_ibl_texture(ibl_texture);
			ibl_texture = texture_resource_utility::load_texture_resource("Assets/Resources/Textures/hilly_terrain_01_puresky_4k/specular_pmrem.etx");
			sky_light->set_specular_ibl_texture(ibl_texture);
			ibl_texture = texture_resource_utility::load_texture_resource("Assets/Resources/Textures/lut_ggx.etx");
			sky_light->set_lut_ggx_texture(ibl_texture);

			register_object(sky_light);
		}
	}

	//	エディターカメラ生成
	{
		editor_camera = std::make_shared<editor_camera_object>("エディターカメラ",
			preview_camera_position,
			math::rotatorf(preview_camera_rotation),
			0.25f);
		register_object(editor_camera);
	}

	//	エディターアイテム用コンテキスト生成
	{
		items_context = editor_items::generate_context();
		items_context->set_window_id(get_context().get_editor().get_window_id());
	}

	//	床
	{
		floor_resource = std::make_shared<model_resource>();
		model_resource_utility::generate_primitive_model(*floor_resource, model_resource_utility::primitive_shapes::plane, floor_size);
		floor_model = std::make_shared<model_object>("床", floor_resource);
		floor_model->set_active(show_floor);
		get_context().get_objects().register_object(floor_model);
	}

	//	プレビューモデル
	{
		scan_preview_models();
		change_preview_shape(model_resource_utility::primitive_shapes::sphere);
	}
	return true;
}

bool edit_material_system::uninitialize_core()
{
	clear_preview_effect();
	clear_domain_preview();

	if (preview_model)
	{
		get_context().get_objects().unregister_object(preview_model);
		preview_model.reset();
	}
	if (floor_model)
	{
		get_context().get_objects().unregister_object(floor_model);
		floor_model.reset();
	}
	return true;
}

void edit_material_system::update_core(float elapsed_time)
{
	{
		//	窓に映しているワールドではアセットプレビューから渡されたファイルを開く
		std::string file_path;
		if (get_context().get_editor().take_open_file(file_path))
			open_source(file_path, false);
	}

	apply_preview_materials();
	update_domain_preview();

	//	エフェクトは最後まで再生したら最初から流し直す
	if (preview_effect && loop_preview_effect)
	{
		const float end_time = preview_effect->get_animation_end_time();
		if (end_time > 0.0f && preview_effect->get_animation_time() >= end_time)
			restart_preview_effect();
	}
}

void edit_material_system::imgui_render_core()
{
	//	パネルの描画が全部終わった後なので、操作が終わっていて変更があれば履歴に積む
	change_tracker.end(history);
}

bool edit_material_system::has_panel(panel_regions region) const
{
	switch (region)
	{
		case panel_regions::menu_bar:
		case panel_regions::left:
		case panel_regions::right:
		{
			return true;
		}
		case panel_regions::bottom:
		{
			return false;
		}
	}
	return false;
}

void edit_material_system::imgui_render_panel(panel_regions region)
{
	switch (region)
	{
		case panel_regions::menu_bar:
		{
			imgui_render_menu_bar();
			break;
		}
		case panel_regions::left:
		{
			//	マテリアルの一覧。詳細は右に出す
			imgui_render_material_list();
			break;
		}
		case panel_regions::right:
		{
			imgui_render_material_detail();
			break;
		}
		case panel_regions::bottom:
		{
			break;
		}
	}
}

void edit_material_system::undo()
{
	if (!history.can_undo())
		return;
	history.undo();

	//	戻した中身を次の変更の変更前にする
	change_tracker.resync();
}

void edit_material_system::redo()
{
	if (!history.can_redo())
		return;
	history.redo();
	change_tracker.resync();
}

void edit_material_system::open_source(const std::string& file_path, bool force_load)
{
	model_resource_ptr model;
	effect_resource_ptr effect;
	material_resource_ptr material;
	if (has_extension(file_path, ".emr"))
	{
		auto operation = resource_manager::instance()->load<material_resource>(file_path, force_load);
		if (!operation->is_succeeded())
		{
			message = "load failed: " + to_display(file_path);
			return;
		}
		material = operation->get_resource();
	}
	else if (has_extension(file_path, ".emo"))
	{
		if (!model_resource_manager::instance()->load(model, file_path, force_load))
		{
			message = "load failed: " + to_display(file_path);
			return;
		}
	}
	else if (has_extension(file_path, ".eme"))
	{
		if (!effect_resource_manager::instance()->load(effect, file_path, force_load))
		{
			message = "load failed: " + to_display(file_path);
			return;
		}
	}
	else
	{
		message = "unsupported file: " + to_display(file_path);
		return;
	}

	source_model = model;
	source_effect = effect;
	source_material = material;
	dirty = false;
	message.clear();

	collect_materials();
	selected_material = materials.empty() ? -1 : 0;
	make_preview_effect_resource();

	//	モデルを開いたらそのモデルで、エフェクトを開いたらそのエフェクトを再生してプレビューする
	if (source_model)
	{
		clear_preview_effect();
		change_preview_model(get_source_file_path());
	}
	else
	{
		restart_preview_effect();
	}

	apply_preview_materials();
}

void edit_material_system::save_source()
{
	if (source_material || source_model || source_effect)
	{
		for (auto& entry : materials)
		{
			if (entry.original && entry.material)
				entry.material->duplicate(*entry.original);
		}
	}

	const std::string source_file_path = get_source_file_path();
	bool saved = false;
	if (source_material)
		saved = resource_manager::instance()->save(source_material, source_file_path);
	else if (source_model)
		saved = resource_manager::instance()->save(source_model, source_file_path);
	else if (source_effect)
		saved = resource_manager::instance()->save(source_effect, source_file_path);
	else
		return;

	if (!saved)
	{
		message = "save failed: " + to_display(source_file_path);
		return;
	}
	dirty = false;
	message = "saved: " + to_display(source_file_path);
}

std::string edit_material_system::get_source_file_path() const
{
	if (source_material)
		return std::string(source_material->get_file_path());
	if (source_model)
		return std::string(source_model->get_file_path());
	if (source_effect)
		return std::string(source_effect->get_file_path());
	return {};
}

void edit_material_system::revert_source()
{
	for (auto& entry : materials)
	{
		if (entry.original && entry.material)
			entry.original->duplicate(*entry.material);
	}
	dirty = false;
	message = "reverted: " + to_display(get_source_file_path());

	//	ファイルの中身に戻したので、それまでの操作は捨てる
	history.clear();
	change_tracker.reset();
}

void edit_material_system::collect_materials()
{
	materials.clear();

	//	編集用の複製を作り直すので、前の複製を指す操作は捨てる
	history.clear();
	change_tracker.reset();

	if (source_material)
	{
		materials.push_back({ core::convert_utf8(source_material->get_name()), make_working_copy(source_material), source_material });
		return;
	}

	if (!source_model && !source_effect)
	{
		auto instance = preview_model ? preview_model->get_model_instance() : nullptr;
		auto resource = instance ? instance->get_resource_for_editor() : nullptr;
		if (!resource)
			return;

		auto& resource_materials = resource->get_materials_for_editor();
		for (size_t index = 0; index < resource_materials.size(); ++index)
		{
			auto& material = resource_materials[index];
			if (!material)
				continue;
			materials.push_back({ "preview [" + std::to_string(index) + "] " + core::convert_utf8(material->get_name()), make_working_copy(material), material });
		}
		return;
	}

	if (source_model)
	{
		auto& resource_materials = source_model->get_materials_for_editor();
		for (size_t index = 0; index < resource_materials.size(); ++index)
		{
			auto& material = resource_materials[index];
			if (!material)
				continue;
			materials.push_back({ "[" + std::to_string(index) + "] " + core::convert_utf8(material->get_name()), make_working_copy(material), material });
		}
	}

	if (source_effect)
	{
		auto& decals = source_effect->get_decals_for_editor();
		for (size_t index = 0; index < decals.size(); ++index)
		{
			auto& material = decals[index].material;
			if (!material)
				continue;
			materials.push_back({ "decal[" + std::to_string(index) + "] " + core::convert_utf8(material->get_name()), make_working_copy(material), material });
		}

		auto& particles = source_effect->get_particles_for_editor();
		for (size_t index = 0; index < particles.size(); ++index)
		{
			auto* renderer = particles[index].particle.get_item<::particle::item::renderer>();
			if (!renderer || !renderer->material)
				continue;
			materials.push_back({ "particle[" + std::to_string(index) + "] " + core::convert_utf8(renderer->material->get_name()), make_working_copy(renderer->material), renderer->material });
		}
	}
}

material_resource_ptr edit_material_system::get_selected_material() const
{
	if (selected_material < 0 || selected_material >= static_cast<int>(materials.size()))
		return nullptr;
	return materials[selected_material].material;
}

void edit_material_system::scan_preview_models()
{
	preview_model_files.clear();

	std::error_code error;
	for (const auto& entry : std::filesystem::recursive_directory_iterator(std::filesystem::path(preview_model_directory), error))
	{
		if (!entry.is_regular_file())
			continue;
		const std::string file_path = entry.path().generic_string();
		if (has_extension(file_path, ".emo"))
			preview_model_files.emplace_back(file_path);
	}
	std::sort(preview_model_files.begin(), preview_model_files.end());

	//	編集中のモデルが一覧の外にあっても選べるようにする
	if (source_model && std::find(preview_model_files.begin(), preview_model_files.end(), get_source_file_path()) == preview_model_files.end())
		preview_model_files.insert(preview_model_files.begin(), get_source_file_path());
}

void edit_material_system::change_preview_model(const std::string& file_path)
{
	if (file_path.empty())
		return;

	//	編集中のモデルは、編集しているリソースそのものを表示する
	model_resource_ptr resource;
	if (source_model && file_path == get_source_file_path())
		resource = source_model;
	else if (!model_resource_manager::instance()->load(resource, file_path))
	{
		message = "load failed: " + to_display(file_path);
		return;
	}

	preview_kind = preview_kinds::file;
	preview_model_path = file_path;
	if (std::find(preview_model_files.begin(), preview_model_files.end(), file_path) == preview_model_files.end())
		preview_model_files.insert(preview_model_files.begin(), file_path);

	set_preview_resource(resource, 0.0);
}

void edit_material_system::change_preview_shape(model_resource_utility::primitive_shapes shape)
{
	auto& resource = shape_resources[static_cast<size_t>(shape)];
	if (!resource)
	{
		resource = std::make_shared<model_resource>();
		model_resource_utility::generate_primitive_model(*resource, shape, preview_shape_size);
	}

	preview_kind = preview_kinds::shape;
	preview_shape = shape;
	set_preview_resource(resource, shape == model_resource_utility::primitive_shapes::plane ? plane_lift : 0.0);
}

void edit_material_system::set_preview_resource(const model_resource_ptr& resource, double lift)
{
	if (preview_model)
		get_context().get_objects().unregister_object(preview_model);

	preview_model = std::make_shared<model_object>("プレビューモデル", resource);
	preview_model->set_world_translation({ 0.0, lift, 0.0 });
	preview_model->set_active(true);
	get_context().get_objects().register_object(preview_model);

	if (!source_model && !source_effect && !source_material)
	{
		collect_materials();
		selected_material = materials.empty() ? -1 : 0;
	}

	apply_preview_materials();
}

std::string edit_material_system::get_preview_label() const
{
	if (preview_kind == preview_kinds::shape)
		return std::string(magic_enum::enum_name(preview_shape)) + " (built-in)";
	return preview_model_path.empty() ? std::string("(none)") : to_display(preview_model_path);
}

void edit_material_system::apply_preview_materials()
{
	if (!preview_model)
		return;

	auto instance = preview_model->get_model_instance();
	auto resource = instance ? instance->get_resource() : nullptr;
	if (!resource)
		return;

	//	サーフェス以外はモデルに当てられないので、モデル本来のマテリアルに戻す
	material_resource_ptr selected = get_selected_material();
	const bool use_selected = override_preview_slots && selected && selected->get_domain() == material_resource::domains::surface;

	//	元のマテリアルの代わりに編集用の複製を当てる
	auto& slots = instance->get_materials();
	const auto& resource_materials = resource->get_materials();
	for (size_t index = 0; index < slots.size() && index < resource_materials.size(); ++index)
	{
		slots[index].set_resource(use_selected ? selected : find_working_material(resource_materials[index]));
	}
}

material_resource_ptr edit_material_system::find_working_material(const material_resource_ptr& original) const
{
	for (const auto& entry : materials)
	{
		if (entry.original == original)
			return entry.material;
	}
	return original;
}

void edit_material_system::make_preview_effect_resource()
{
	preview_effect_resource.reset();
	if (!source_effect)
		return;

	//	編集用のマテリアルを当てて再生する
	auto resource = resource_manager::instance()->load_uncached<effect_resource>(get_source_file_path());
	if (!resource)
		return;

	auto& source_decals = source_effect->get_decals_for_editor();
	auto& decals = resource->get_decals_for_editor();
	for (size_t index = 0; index < decals.size() && index < source_decals.size(); ++index)
	{
		if (source_decals[index].material)
			decals[index].material = find_working_material(source_decals[index].material);
	}

	auto& source_particles = source_effect->get_particles_for_editor();
	auto& particles = resource->get_particles_for_editor();
	for (size_t index = 0; index < particles.size() && index < source_particles.size(); ++index)
	{
		auto* source_renderer = source_particles[index].particle.get_item<::particle::item::renderer>();
		auto* renderer = particles[index].particle.get_item<::particle::item::renderer>();
		if (source_renderer && renderer && source_renderer->material)
			renderer->material = find_working_material(source_renderer->material);
	}

	preview_effect_resource = resource;
}

void edit_material_system::restart_preview_effect()
{
	clear_preview_effect();
	if (!source_effect)
		return;

	preview_effect = std::make_shared<effect_instance>(preview_effect_resource ? preview_effect_resource : source_effect);
	get_context().get_effects().register_instance(preview_effect);
}

void edit_material_system::clear_preview_effect()
{
	if (!preview_effect)
		return;

	get_context().get_effects().unregister_instance(preview_effect);
	preview_effect.reset();
}

void edit_material_system::update_domain_preview()
{
	//	サーフェスはプレビューモデルに当て、エフェクトを開いている時はそのエフェクトで確かめるので、仮の表示は要らない
	material_resource_ptr selected = get_selected_material();
	if (selected && (source_effect || selected->get_domain() == material_resource::domains::surface))
		selected = nullptr;

	if (selected != domain_preview_material)
	{
		clear_domain_preview();
		if (selected)
			make_domain_preview(selected);
	}

	//	仮のエフェクトは最後まで再生したら作り直す
	if (domain_preview_effect)
	{
		const float end_time = domain_preview_effect->get_animation_end_time();
		if (end_time > 0.0f && domain_preview_effect->get_animation_time() >= end_time)
		{
			const effect_resource_ptr resource = domain_preview_effect->get_resource();
			get_context().get_effects().unregister_instance(domain_preview_effect);
			domain_preview_effect = std::make_shared<effect_instance>(resource);
			get_context().get_effects().register_instance(domain_preview_effect);
		}
	}
}

void edit_material_system::make_domain_preview(const material_resource_ptr& material)
{
	domain_preview_material = material;
	switch (material->get_domain())
	{
		case material_resource::domains::particle:
		case material_resource::domains::deferred_decal:
		{
			domain_preview_effect = std::make_shared<effect_instance>(make_domain_preview_effect(material));
			get_context().get_effects().register_instance(domain_preview_effect);
			break;
		}
		case material_resource::domains::post_process:
		{
			attach_preview_post_process(material);
			break;
		}
		case material_resource::domains::user_interface:
		{
			//	画面の真ん中に四角を出す
			preview_ui = std::make_shared<ui_image>("プレビューUI");
			preview_ui->set_transient(true);
			preview_ui->set_screen_rect({ ui::virtual_screen_width * 0.5f - 180.0f, ui::virtual_screen_height * 0.5f - 180.0f, 360.0f, 360.0f });
			preview_ui->get_sprite().get_material().set_resource(material);
			get_context().get_objects().register_object(preview_ui);
			break;
		}
		default:
		{
			break;
		}
	}
}

void edit_material_system::clear_domain_preview()
{
	if (domain_preview_effect)
	{
		get_context().get_effects().unregister_instance(domain_preview_effect);
		domain_preview_effect.reset();
	}

	if (preview_ui)
	{
		get_context().get_objects().unregister_object(preview_ui);
		preview_ui.reset();
	}

	//	足したマテリアルだけをボリュームから外す
	if (preview_volume)
	{
		const post_process_volume_ptr volume = preview_volume->get_volume();
		const auto& volume_materials = volume->get_materials();
		for (std::size_t index = 0; index < volume_materials.size(); ++index)
		{
			if (volume_materials[index]->get_resource() == domain_preview_material)
			{
				volume->unregister_postprocess_material(index);
				break;
			}
		}
		preview_volume.reset();
	}

	if (own_preview_volume)
	{
		get_context().get_objects().unregister_object(own_preview_volume);
		own_preview_volume.reset();
	}

	domain_preview_material.reset();
}

effect_resource_ptr edit_material_system::make_domain_preview_effect(const material_resource_ptr& material) const
{
	auto resource = std::make_shared<effect_resource>();
	auto& nodes = resource->get_nodes_for_editor();
	auto& animations = resource->get_animations_for_editor();

	//	ルートノードとアニメーション
	nodes.emplace_back().name = "root";
	{
		auto& animation_data = animations.emplace_back();
		animation_data.name = "default";
		animation_data.end_time = domain_preview_effect_time;
		animation_data.nodes.emplace_back().node_name = nodes.back().name;
	}

	auto& node = nodes.emplace_back();
	node.name = "preview";
	node.parent_index = 0;
	const size_t node_index = nodes.size() - 1;

	if (material->get_domain() == material_resource::domains::particle)
	{
		//	プレビューモデルの周りに出し続ける
		auto& particle_data = resource->get_particles_for_editor().emplace_back();
		particle_data.node_index = node_index;

		auto renderer = particle_data.particle.register_item<::particle::item::renderer>();
		renderer->type = ::particle::item::renderer::types::sprite;
		renderer->total_particle_count = 1024;
		renderer->material = material;

		auto spawn = particle_data.particle.register_item<::particle::item::spawn>();
		spawn->life_time_range = { 1.0f, 2.0f };
		spawn->scale_range = { 2.0f, 4.0f };
		spawn->area_min = system_units::convert_m_to_unit(math::float3(-1.0f, 0.0f, -1.0f));
		spawn->area_max = system_units::convert_m_to_unit(math::float3(1.0f, 2.0f, 1.0f));
		spawn->range = { 1, 1 };
	}
	else
	{
		//	床とプレビューモデルに掛かる箱
		node.scale = system_units::convert_m_to_unit(math::float3(2.0f, 2.0f, 2.0f));

		auto& decal = resource->get_decals_for_editor().emplace_back();
		decal.node_index = node_index;
		decal.material = material;
	}
	return resource;
}

void edit_material_system::attach_preview_post_process(const material_resource_ptr& material)
{
	//	シーンにある境界無しのボリュームに足す。無ければ仮のボリュームを作る
	for (const auto& volume_object : get_context().get_objects().search_objects<post_process_volume_object>())
	{
		if (volume_object->is_flag(post_process_volume::flags::unbind))
		{
			preview_volume = volume_object;
			break;
		}
	}
	if (!preview_volume)
	{
		own_preview_volume = std::make_shared<post_process_volume_object>("プレビューポストプロセスボリューム");
		own_preview_volume->set_transient(true);
		own_preview_volume->set_flag(post_process_volume::flags::unbind, true);
		get_context().get_objects().register_object(own_preview_volume);
		preview_volume = own_preview_volume;
	}
	preview_volume->get_volume()->register_postprocess_materials(material);
}

void edit_material_system::imgui_render_menu_bar()
{
	//	メニューバーはこの窓で最初に描かれるので、編集する UI を描く前にここで選んでいるマテリアルを追跡する物にする(前と違うマテリアルなら変更前を取り直す)
	if (const material_resource_ptr material = get_selected_material())
		change_tracker.set_target("Edit Material " + materials[selected_material].label, editor_change_tracker::make_material_target(material, [this]() { dirty = true; }));
	else
		change_tracker.reset();

	//	Ctrl+Z / Ctrl+Y はこの窓がフォーカスしている時だけ受ける
	switch (editor_edit_tracker::check_shortcuts(ImGuiInputFlags_RouteFocused))
	{
		case editor_edit_tracker::shortcuts::undo:
		{
			undo();
			break;
		}
		case editor_edit_tracker::shortcuts::redo:
		{
			redo();
			break;
		}
		default:
		{
			break;
		}
	}

	//	保存・ファイルから読み直して編集を捨てる
	const bool has_source = !get_source_file_path().empty();
	if (ImGui::MenuItem("Save", nullptr, false, has_source && dirty))
		save_source();
	if (ImGui::MenuItem("Revert", nullptr, false, has_source && dirty))
		revert_source();

	//	元に戻す・やり直す(名前が変わっても同じ項目になるよう ### で ID を固定)
	const std::string undo_label = history.can_undo() ? "Undo (" + std::string(history.get_undo_label()) + ")###material_undo" : std::string("Undo###material_undo");
	if (ImGui::MenuItem(undo_label.c_str(), "Ctrl+Z", false, history.can_undo()))
		undo();
	const std::string redo_label = history.can_redo() ? "Redo (" + std::string(history.get_redo_label()) + ")###material_redo" : std::string("Redo###material_redo");
	if (ImGui::MenuItem(redo_label.c_str(), "Ctrl+Y", false, history.can_redo()))
		redo();

	//	プレビューの設定
	if (ImGui::BeginMenu("Preview"))
	{
		imgui_render_preview();
		ImGui::EndMenu();
	}

	//	開いているファイルと結果のメッセージ
	if (has_source)
		ImGui::TextDisabled("%s%s", to_display(get_source_file_path()).c_str(), dirty ? " *" : "");
	else
		ImGui::TextDisabled("load a .emr / .emo / .eme file");
	if (!message.empty())
		ImGui::TextDisabled("%s", message.c_str());
}

void edit_material_system::imgui_render_preview()
{
	if (ImGui::BeginCombo("preview model", get_preview_label().c_str()))
	{
		for (const auto shape : magic_enum::enum_values<model_resource_utility::primitive_shapes>())
		{
			const bool selected = preview_kind == preview_kinds::shape && preview_shape == shape;
			const std::string label = std::string(magic_enum::enum_name(shape)) + " (built-in)";
			if (ImGui::Selectable(label.c_str(), selected))
				change_preview_shape(shape);
			if (selected)
				ImGui::SetItemDefaultFocus();
		}
		ImGui::Separator();

		for (const auto& file_path : preview_model_files)
		{
			const bool selected = preview_kind == preview_kinds::file && file_path == preview_model_path;
			std::string label = to_display(file_path);
			if (source_model && file_path == get_source_file_path())
				label += " (editing)";
			if (ImGui::Selectable(label.c_str(), selected))
				change_preview_model(file_path);
			if (selected)
				ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}
	ImGui::SameLine();
	if (ImGui::Button("rescan"))
		scan_preview_models();

	ImGui::Checkbox("apply selected surface material to all slots", &override_preview_slots);
	ImGui::SameLine();
	if (ImGui::Checkbox("floor", &show_floor) && floor_model)
		floor_model->set_active(show_floor);

	if (source_effect)
	{
		if (ImGui::Button("replay effect"))
			restart_preview_effect();
		ImGui::SameLine();
		ImGui::Checkbox("loop", &loop_preview_effect);
	}
}

void edit_material_system::imgui_render_material_list()
{
	if (materials.empty())
	{
		ImGui::TextDisabled("no material");
		return;
	}

	for (int index = 0; index < static_cast<int>(materials.size()); ++index)
	{
		ImGui::PushID(index);
		if (ImGui::Selectable(materials[index].label.c_str(), index == selected_material))
			selected_material = index;
		ImGui::PopID();
	}
}

void edit_material_system::imgui_render_material_detail()
{
	material_resource_ptr material = get_selected_material();
	if (!material)
	{
		ImGui::TextDisabled("select a material");
		return;
	}

	ImGui::TextDisabled("domain: %s", std::string(magic_enum::enum_name(material->get_domain())).c_str());
	if (material->get_domain() != material_resource::domains::surface && preview_model && override_preview_slots)
		ImGui::TextDisabled("(not a surface material: preview model keeps its own materials)");
	if (!source_model && !source_effect && !source_material)
		ImGui::TextDisabled("preview material: changes are not saved (Load a .emr / .emo / .eme to save)");

	//	リソースを直接編集するので、同じマテリアルを使う全てのインスタンスへバージョン番号で伝わる
	if (editor_items::edit_shared_material(items_context, material))
	{
		dirty = true;
		change_tracker.notify_changed();
	}
}

#endif // defined(USE_IMGUI)
