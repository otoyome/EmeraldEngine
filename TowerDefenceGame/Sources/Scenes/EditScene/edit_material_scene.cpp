#include "scenes/editscene/edit_material_scene.h"

#include <algorithm>
#include <cctype>
#include <filesystem>

#include "model_instance.h"
#include "camera.h"

#include "object/light_object.h"

#if defined(USE_IMGUI)
#include "imgui.h"

namespace
{
	//	マテリアルを含むファイル
	constexpr const TCHAR* SourceFileDialogFilter =
		L"マテリアルを含むファイル (*.emo;*.eme)\0*.emo;*.eme;\0"
		L"モデルファイル (*.emo)\0*.emo;\0"
		L"エフェクトファイル (*.eme)\0*.eme;\0"
		L"すべてのファイル (*.*)\0*.*\0";

	//	プレビューモデルを探すディレクトリと、最初に表示するモデル
	constexpr std::string_view preview_model_directory = "Assets/Resources/Models";
	constexpr std::string_view default_preview_model_path = "Assets/Resources/Models/Roomba/roomba.emo";

	//	内蔵の形の大きさ(直径・一辺)と床の大きさ
	const float preview_shape_size = system_units::convert_m_to_unit(1.0f);
	const float floor_size = system_units::convert_m_to_unit(10.0f);

	//	平面を床と重ならないよう浮かせる高さ
	const double plane_lift = system_units::convert_m_to_unit(0.01);

	//	ファイルパスを ImGui 表示用の UTF-8 にする
	std::string to_display(std::string_view text)
	{
		return system_utility::convert_utf8(text);
	}

	bool has_extension(const std::string& file_path, std::string_view extension)
	{
		std::string value = std::filesystem::path(file_path).extension().string();
		std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return value == extension;
	}
}

edit_material_scene::edit_material_scene()
: scene_base()
{
}

edit_material_scene::~edit_material_scene()
{
}

bool edit_material_scene::initialize_core()
{
	//	メインカメラ生成
	{
		main_camera = std::make_shared<camera_object>("メインカメラ");
		main_camera->get_camera()->get_parameters().fov_y = math::angle::to_radian(30.0f);
		register_object(main_camera);
		mouse::instance()->set_center_lock(false);
		mouse::instance()->set_cursol_hide(false);
	}

	//	ライト生成
	{
		//	平行光源生成
		{
			auto light = std::make_shared<directional_light_object>("平行光源");
			auto color = light->get_color();
			color.w = 1.0f;
			light->set_color(color);
			register_object(light);
		}

		//	間接光生成
		{
			auto sky_light = std::make_shared<sky_light_object>("スカイライト");
			auto color = sky_light->get_color();
			color.w = 1.0f;
			sky_light->set_color(color);

			texture_ptr ibl_texture;
			texture_manager::instance()->load_texture_from_file(ibl_texture, "Assets/Resources/Textures/hilly_terrain_01_puresky_4k/diffuse_iem.dds");
			sky_light->set_diffuse_ibl_texture(ibl_texture);
			texture_manager::instance()->load_texture_from_file(ibl_texture, "Assets/Resources/Textures/hilly_terrain_01_puresky_4k/specular_pmrem.dds");
			sky_light->set_specular_ibl_texture(ibl_texture);
			texture_manager::instance()->load_texture_from_file(ibl_texture, "Assets/Resources/Textures/lut_ggx.dds");
			sky_light->set_lut_ggx_texture(ibl_texture);

			register_object(sky_light);
		}
	}

	//	ポストプロセス生成
	{
		auto post_process_volume = std::make_shared<post_process_volume_object>("ポストプロセスボリューム");
		post_process_volume->set_flag(post_process_volume::flags::unbind, true);
		{
			auto bloom_constant = post_process_volume->get_volume()->get_bloom_constant();
			auto lens_ghosts_constant = post_process_volume->get_volume()->get_lens_ghosts_constant();
			auto depth_of_field_constant = post_process_volume->get_volume()->get_depth_of_field_constant();
			auto motion_blur_constant = post_process_volume->get_volume()->get_motion_blur_constant();
			bloom_constant.set_active(false);
			lens_ghosts_constant.set_active(false);
			depth_of_field_constant.set_active(false);
			motion_blur_constant.set_active(false);
			post_process_volume->get_volume()->set_bloom_constant(bloom_constant);
			post_process_volume->get_volume()->set_lens_ghosts_constant(lens_ghosts_constant);
			post_process_volume->get_volume()->set_depth_of_field_constant(depth_of_field_constant);
			post_process_volume->get_volume()->set_motion_blur_constant(motion_blur_constant);
		}
		register_object(post_process_volume);
	}

	//	エディターカメラ生成
	{
		editor_camera_request = editor_camera_manager::instance()->request(
			system_units::convert_m_to_unit(math::vector3d(0.0, 1.0, 8.0)),
			{ 0, math::constants<float>::pi, 0 },
			0.25f);
	}

	//	エディターアイテム用コンテキスト生成
	{
		editor_context = editor_items::generate_context();
	}

	//	床
	{
		floor_resource = std::make_shared<model_resource>();
		model_resource_utility::generate_primitive_model(*floor_resource, model_resource_utility::primitive_shapes::plane, floor_size);
		floor_model = std::make_shared<model_object>("床", floor_resource);
		floor_model->set_active(show_floor);
		object_manager::instance()->register_object(floor_model);
	}

	//	プレビューモデル
	{
		scan_preview_models();
		change_preview_shape(model_resource_utility::primitive_shapes::sphere);
	}
	return true;
}

bool edit_material_scene::uninitialize_core()
{
	clear_preview_effect();

	if (preview_model)
	{
		object_manager::instance()->unregister_object(preview_model);
		preview_model.reset();
	}
	if (floor_model)
	{
		object_manager::instance()->unregister_object(floor_model);
		floor_model.reset();
	}
	return true;
}

void edit_material_scene::update_core(float elapsed_time)
{
	apply_preview_materials();

	//	エフェクトは最後まで再生したら最初から流し直す
	if (preview_effect && loop_preview_effect)
	{
		const float end_time = preview_effect->get_animation_end_time();
		if (end_time > 0.0f && preview_effect->get_animation_time() >= end_time)
			restart_preview_effect();
	}
}

void edit_material_scene::imgui_render_core()
{
	ImGui::SetNextWindowSize(ImVec2(820.0f, 640.0f), ImGuiCond_FirstUseEver);
	if (ImGui::Begin("Material Editor"))
	{
		imgui_render_source();
		ImGui::Separator();
		imgui_render_preview();
		ImGui::Separator();

		if (ImGui::BeginChild("materials", ImVec2(260.0f, 0.0f), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX))
			imgui_render_material_list();
		ImGui::EndChild();
		ImGui::SameLine();
		if (ImGui::BeginChild("detail", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders))
			imgui_render_material_detail();
		ImGui::EndChild();
	}
	ImGui::End();
}

void edit_material_scene::open_source(const std::string& file_path, bool force_load)
{
	model_resource_ptr model;
	effect_resource_ptr effect;
	if (has_extension(file_path, ".emo"))
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

	source_file_path = file_path;
	source_model = model;
	source_effect = effect;
	dirty = false;
	message.clear();

	collect_materials();
	selected_material = materials.empty() ? -1 : 0;

	//	モデルを開いたらそのモデルで、エフェクトを開いたらそのエフェクトを再生してプレビューする
	if (source_model)
	{
		clear_preview_effect();
		change_preview_model(source_file_path);
	}
	else
	{
		restart_preview_effect();
	}
}

void edit_material_scene::save_source()
{
	bool saved = false;
	if (source_model)
		saved = source_model->serialize(source_file_path);
	else if (source_effect)
		saved = source_effect->serialize(source_file_path);
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

void edit_material_scene::collect_materials()
{
	materials.clear();

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
			materials.push_back({ "preview [" + std::to_string(index) + "] " + system_utility::convert_utf8(material->get_name()), material });
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
			materials.push_back({ "[" + std::to_string(index) + "] " + system_utility::convert_utf8(material->get_name()), material });
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
			materials.push_back({ "decal[" + std::to_string(index) + "] " + system_utility::convert_utf8(material->get_name()), material });
		}

		auto& particles = source_effect->get_particles_for_editor();
		for (size_t index = 0; index < particles.size(); ++index)
		{
			auto* renderer = particles[index].particle.get_item<::particle::item::renderer>();
			if (!renderer || !renderer->material)
				continue;
			materials.push_back({ "particle[" + std::to_string(index) + "] " + system_utility::convert_utf8(renderer->material->get_name()), renderer->material });
		}
	}
}

material_resource_ptr edit_material_scene::get_selected_material() const
{
	if (selected_material < 0 || selected_material >= static_cast<int>(materials.size()))
		return nullptr;
	return materials[selected_material].material;
}

void edit_material_scene::scan_preview_models()
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
	if (source_model && std::find(preview_model_files.begin(), preview_model_files.end(), source_file_path) == preview_model_files.end())
		preview_model_files.insert(preview_model_files.begin(), source_file_path);
}

void edit_material_scene::change_preview_model(const std::string& file_path)
{
	if (file_path.empty())
		return;

	//	編集中のモデルは、編集しているリソースそのものを表示する
	model_resource_ptr resource;
	if (source_model && file_path == source_file_path)
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

void edit_material_scene::change_preview_shape(model_resource_utility::primitive_shapes shape)
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

void edit_material_scene::set_preview_resource(const model_resource_ptr& resource, double lift)
{
	if (preview_model)
		object_manager::instance()->unregister_object(preview_model);

	preview_model = std::make_shared<model_object>("プレビューモデル", resource);
	preview_model->set_world_translation({ 0.0, lift, 0.0 });
	preview_model->set_active(true);
	object_manager::instance()->register_object(preview_model);

	if (!source_model && !source_effect)
	{
		collect_materials();
		selected_material = materials.empty() ? -1 : 0;
	}

	apply_preview_materials();
}

std::string edit_material_scene::get_preview_label() const
{
	if (preview_kind == preview_kinds::shape)
		return std::string(magic_enum::enum_name(preview_shape)) + " (built-in)";
	return preview_model_path.empty() ? std::string("(none)") : to_display(preview_model_path);
}

void edit_material_scene::apply_preview_materials()
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

	auto& slots = instance->get_materials();
	const auto& resource_materials = resource->get_materials();
	for (size_t index = 0; index < slots.size() && index < resource_materials.size(); ++index)
	{
		slots[index].set_resource(use_selected ? selected : resource_materials[index]);
	}
}

void edit_material_scene::restart_preview_effect()
{
	clear_preview_effect();
	if (!source_effect)
		return;

	preview_effect = std::make_shared<effect_instance>(source_effect);
	effect_manager::instance()->register_instance(preview_effect);
}

void edit_material_scene::clear_preview_effect()
{
	if (!preview_effect)
		return;

	if (effect_manager::is_valid())
		effect_manager::instance()->unregister_instance(preview_effect);
	preview_effect.reset();
}

void edit_material_scene::imgui_render_source()
{
	if (ImGui::Button("Load"))
	{
		std::string file_path;
		if (editor_items::open_file_dialog(SourceFileDialogFilter, file_path))
			open_source(file_path, false);
	}

	if (!source_file_path.empty())
	{
		ImGui::SameLine();
		ImGui::BeginDisabled(!dirty);
		if (ImGui::Button("Save"))
			save_source();
		ImGui::SameLine();
		//	ファイルから読み直して編集を捨てる
		if (ImGui::Button("Revert"))
			open_source(std::string(source_file_path), true);
		ImGui::EndDisabled();

		ImGui::SameLine();
		ImGui::Text("%s%s", to_display(source_file_path).c_str(), dirty ? " *" : "");
	}
	else
	{
		ImGui::SameLine();
		ImGui::TextDisabled("load a .emo / .eme file");
	}

	if (!message.empty())
		ImGui::TextUnformatted(message.c_str());
}

void edit_material_scene::imgui_render_preview()
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
			if (source_model && file_path == source_file_path)
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

void edit_material_scene::imgui_render_material_list()
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

void edit_material_scene::imgui_render_material_detail()
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
	if (!source_model && !source_effect)
		ImGui::TextDisabled("preview material: changes are not saved (Load a .emo / .eme to save)");

	//	リソースを直接編集するので、同じマテリアルを使う全てのインスタンスへバージョン番号で伝わる
	if (editor_items::edit_material(editor_context, *material))
		dirty = true;
}
#endif // defined(USE_IMGUI)
