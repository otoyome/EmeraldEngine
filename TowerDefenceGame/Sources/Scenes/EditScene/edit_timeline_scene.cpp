#include "scenes/editscene/edit_timeline_scene.h"

#if defined(USE_IMGUI)
#include <algorithm>
#include <cmath>
#include <filesystem>

#include "model_instance.h"
#include "camera.h"

#include "object/light_object.h"
#include "object/model_object.h"
#include "object/post_process_volume_object.h"
#include "object/object_manager.h"

#include "custom_pipeline_state.h"

#include "Editor/editor_camera.h"

namespace
{
	//	モデル
	constexpr TCHAR* ModelFileDialogFilter =
		L"モデルファイル (*.emo)\0*.emo;\0\
		すべてのファイル (*.*)\0*.*\0";

	//	エフェクト
	constexpr TCHAR* EffectFileDialogFilter =
		L"エフェクトファイル (*.eme)\0*.eme;\0\
		すべてのファイル (*.*)\0*.*\0";

	//	タイムライン
	constexpr TCHAR* TimelineFileDialogFilter =
		L"タイムラインファイル (*.etl)\0*.etl;\0\
		すべてのファイル (*.*)\0*.*\0";

	//	アクターごとのシーケンサー表示色
	constexpr unsigned int actor_track_colors[] =
	{
		0xFF3F8AE0,
		0xFF3FC0C0,
		0xFF8ABF3F,
		0xFFBF8A3F,
		0xFFBF3FA0,
		0xFF8A3FBF,
	};
	unsigned int actor_track_color(std::size_t actor_index)
	{
		return actor_track_colors[actor_index % ARRAYSIZE(actor_track_colors)];
	}

	//	ポストプロセスマテリアル
	constexpr material::blendable_locations postprocess_material_path_list[] =
	{
		material::blendable_locations::before_dof,
		material::blendable_locations::after_dof,
		material::blendable_locations::after_dof_translucency,
		material::blendable_locations::before_bloom,
		material::blendable_locations::after_tone_mapping,
	};
	constexpr std::string_view postprocess_material_texts_list[ARRAYSIZE(postprocess_material_path_list)][4] =
	{
		{
			"timeline_before_translucent_color_filter",
			"Shaders/FullscreenQuad/fullscreen_quad_vs.hlsl",
			"Shaders/Postprocess/color_filter_before_dof_ps.hlsl",
			"被写界深度前ポストプロセスマテリアル",
		},
		{
			"timeline_before_tonemapping_color_filter",
			"Shaders/FullscreenQuad/fullscreen_quad_vs.hlsl",
			"Shaders/Postprocess/color_filter_after_dof_ps.hlsl",
			"被写界深度後ポストプロセスマテリアル",
		},
		{
			"timeline_before_tonemapping_color_filter_translucency",
			"Shaders/FullscreenQuad/fullscreen_quad_vs.hlsl",
			"Shaders/Postprocess/color_filter_after_dof_translucency_ps.hlsl",
			"被写界深度後半透明描画ポストプロセスマテリアル",
		},
		{
			"timeline_after_tonemapping_color_filter_before_bloom",
			"Shaders/FullscreenQuad/fullscreen_quad_vs.hlsl",
			"Shaders/Postprocess/color_filter_before_bloom_ps.hlsl",
			"ブルーム前ポストプロセスマテリアル",
		},
		{
			"timeline_after_tonemapping_color_filter",
			"Shaders/FullscreenQuad/fullscreen_quad_vs.hlsl",
			"Shaders/Postprocess/color_filter_after_tone_mapping_ps.hlsl",
			"トーンマッピング後ポストプロセスマテリアル",
		}
	};
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
//	sequence_interface（ImSequencer 用）
//	type の並び： [0..actors.size()-1] = 各アクターのアニメーション行 / actors.size() = エフェクト / actors.size()+1 = カメラ

std::size_t edit_timeline_scene::sequence_interface::total_animation_count() const
{
	std::size_t total = 0;
	for (auto& actor : owner->timeline.actors)
		total += actor.animations.size();
	return total;
}

bool edit_timeline_scene::sequence_interface::resolve_animation(int flat_index, std::size_t& out_actor_index, std::size_t& out_entry_index) const
{
	if (flat_index < 0)
		return false;

	std::size_t remaining = static_cast<std::size_t>(flat_index);
	for (std::size_t actor_index = 0; actor_index < owner->timeline.actors.size(); ++actor_index)
	{
		auto& actor = owner->timeline.actors[actor_index];
		if (remaining < actor.animations.size())
		{
			out_actor_index = actor_index;
			out_entry_index = remaining;
			return true;
		}
		remaining -= actor.animations.size();
	}
	return false;
}

int edit_timeline_scene::sequence_interface::GetFrameMin() const
{
	return 0;
}

int edit_timeline_scene::sequence_interface::GetFrameMax() const
{
	return owner->timeline.total_frame;
}

int edit_timeline_scene::sequence_interface::GetItemCount() const
{
	return static_cast<int>(total_animation_count() + owner->timeline.effects.size() + owner->timeline.cameras.size());
}

int edit_timeline_scene::sequence_interface::GetItemTypeCount() const
{
	return static_cast<int>(owner->timeline.actors.size()) + 2;
}

const char* edit_timeline_scene::sequence_interface::GetItemTypeName(int type_index) const
{
	const int actor_count = static_cast<int>(owner->timeline.actors.size());
	if (type_index < actor_count)
	{
		type_name_cache = owner->timeline.actors[type_index].name + " (Animation)";
		return type_name_cache.c_str();
	}
	if (type_index == actor_count)
		return "Effect";
	return "Camera";
}

const char* edit_timeline_scene::sequence_interface::GetItemLabel(int index) const
{
	std::size_t actor_index, entry_index;
	if (resolve_animation(index, actor_index, entry_index))
	{
		auto& actor = owner->timeline.actors[actor_index];
		auto& entry = actor.animations[entry_index];
		label_cache = "[" + actor.name + "] " + (entry.animation_name.empty() ? std::string("(none)") : entry.animation_name);
		return label_cache.c_str();
	}

	const std::size_t animation_total = total_animation_count();
	const std::size_t effect_count = owner->timeline.effects.size();
	const std::size_t local = static_cast<std::size_t>(index) - animation_total;
	if (local < effect_count)
	{
		auto& entry = owner->timeline.effects[local];
		label_cache = "[Fx] " + (entry.effect_path.empty() ? std::string("(none)") : entry.effect_path);
	}
	else
	{
		label_cache = "[Cam] shot " + std::to_string(local - effect_count);
	}
	return label_cache.c_str();
}

void edit_timeline_scene::sequence_interface::Get(int index, int** start, int** end, int* type, unsigned int* color)
{
	std::size_t actor_index, entry_index;
	if (resolve_animation(index, actor_index, entry_index))
	{
		auto& entry = owner->timeline.actors[actor_index].animations[entry_index];
		if (start) *start = &entry.start_frame;
		if (end) *end = &entry.end_frame;
		if (type) *type = static_cast<int>(actor_index);
		if (color) *color = actor_track_color(actor_index);
		return;
	}

	const std::size_t animation_total = total_animation_count();
	const std::size_t effect_count = owner->timeline.effects.size();
	const std::size_t local = static_cast<std::size_t>(index) - animation_total;
	if (local < effect_count)
	{
		auto& entry = owner->timeline.effects[local];
		if (start) *start = &entry.start_frame;
		if (end) *end = &entry.end_frame;
		if (type) *type = static_cast<int>(owner->timeline.actors.size());
		if (color) *color = 0xFF4FBF6F;
	}
	else
	{
		auto& entry = owner->timeline.cameras[local - effect_count];
		if (start) *start = &entry.start_frame;
		if (end) *end = &entry.end_frame;
		if (type) *type = static_cast<int>(owner->timeline.actors.size()) + 1;
		if (color) *color = 0xFFC0708F;
	}
}

void edit_timeline_scene::sequence_interface::Add(int type)
{
	const int actor_count = static_cast<int>(owner->timeline.actors.size());
	const int default_length = static_cast<int>(owner->timeline.fps);

	if (type < actor_count)
	{
		auto& actor = owner->timeline.actors[type];
		auto& entry = actor.animations.emplace_back();
		entry.start_frame = owner->current_frame;
		entry.end_frame = std::min(owner->timeline.total_frame, owner->current_frame + default_length);
		if (static_cast<std::size_t>(type) < owner->actor_models.size() && owner->actor_models[type])
		{
			auto names = owner->actor_models[type]->get_animation_names();
			if (!names.empty())
				entry.animation_name = std::string(names.front());
		}
	}
	else if (type == actor_count)
	{
		auto& entry = owner->timeline.effects.emplace_back();
		entry.start_frame = owner->current_frame;
		entry.end_frame = std::min(owner->timeline.total_frame, owner->current_frame + default_length);
	}
	else
	{
		auto& entry = owner->timeline.cameras.emplace_back();
		entry.start_frame = owner->current_frame;
		entry.end_frame = std::min(owner->timeline.total_frame, owner->current_frame + default_length);
		if (owner->main_camera)
		{
			entry.start.position = owner->main_camera->get_position();
			entry.end.position = entry.start.position;
			entry.start.rotation = owner->main_camera->get_rotation();
			entry.end.rotation = entry.start.rotation;
			entry.start.fov_y = owner->main_camera->get_camera()->get_fov_y();
			entry.end.fov_y = entry.start.fov_y;
		}
	}
}

void edit_timeline_scene::sequence_interface::Del(int index)
{
	std::size_t actor_index, entry_index;
	if (resolve_animation(index, actor_index, entry_index))
	{
		auto& animations = owner->timeline.actors[actor_index].animations;
		animations.erase(animations.begin() + entry_index);
		owner->selected_entry = -1;
		return;
	}

	const std::size_t animation_total = total_animation_count();
	const std::size_t effect_count = owner->timeline.effects.size();
	const std::size_t local = static_cast<std::size_t>(index) - animation_total;
	if (local < effect_count)
		owner->timeline.effects.erase(owner->timeline.effects.begin() + local);
	else
		owner->timeline.cameras.erase(owner->timeline.cameras.begin() + (local - effect_count));
	owner->selected_entry = -1;
}

void edit_timeline_scene::sequence_interface::Duplicate(int index)
{
	std::size_t actor_index, entry_index;
	if (resolve_animation(index, actor_index, entry_index))
	{
		auto& animations = owner->timeline.actors[actor_index].animations;
		animations.push_back(animations[entry_index]);
		return;
	}

	const std::size_t animation_total = total_animation_count();
	const std::size_t effect_count = owner->timeline.effects.size();
	const std::size_t local = static_cast<std::size_t>(index) - animation_total;
	if (local < effect_count)
		owner->timeline.effects.push_back(owner->timeline.effects[local]);
	else
		owner->timeline.cameras.push_back(owner->timeline.cameras[local - effect_count]);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
//	edit_timeline_scene

edit_timeline_scene::edit_timeline_scene()
: scene_base()
{
	sequencer_interface.owner = this;
}

edit_timeline_scene::~edit_timeline_scene()
{
}

bool edit_timeline_scene::initialize_core()
{
	//	メインカメラ生成
	{
		main_camera = std::make_shared<camera_object>("メインカメラ");
		main_camera->get_camera()->set_fov_y(DirectX::XMConvertToRadians(30.0f));
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
			light->set_flag(light::flags::use_shadow, false);
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
	if (pipeline_state_manager* instance = pipeline_state_manager::instance())
	{
		auto post_process_volume_object_ptr = std::make_shared<post_process_volume_object>("ポストプロセスボリューム");
		post_process_volume_object_ptr->set_flag(post_process_volume::flags::unbind, true);
		register_object(post_process_volume_object_ptr);
		post_process_volume = post_process_volume_object_ptr;

		for (UINT index = 0; index < ARRAYSIZE(postprocess_material_path_list); ++index)
		{
			std::shared_ptr<graphics_custom_pipeline_state> pso;
			instance->generate_pipeline_state<graphics_custom_pipeline_state>(postprocess_material_texts_list[index][0], &pso);
			pso->set_domain(pipeline_state_base::domains::post_process);

			//	頂点シェーダー読み込み
			{
				load_shader_file desc;
				desc.file_path = postprocess_material_texts_list[index][1];
#if defined(_DEBUG)
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::disable_optimize, true);
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::debug, true);
#endif	//	defined(_DEBUG)

				std::shared_ptr<vertex_shader> shader;
				shader_manager::instance()->load_shader<vertex_shader>(shader, desc);
				pso->set_shader(shader);
			}

			//	ピクセルシェーダー読み込み
			{
				load_shader_file desc;
				desc.file_path = postprocess_material_texts_list[index][2];
#if defined(_DEBUG)
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::disable_optimize, true);
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::debug, true);
#endif	//	defined(_DEBUG)

				std::shared_ptr<pixel_shader> shader;
				shader_manager::instance()->load_shader<pixel_shader>(shader, desc);
				pso->set_shader(shader);
			}

			std::shared_ptr<material> post_process_material = std::make_shared<material>();
			post_process_material->set_name(postprocess_material_texts_list[index][3]);
			post_process_material->set_domain(material::domains::post_process);
			post_process_material->set_flag(material::flags::two_sided, true);
			post_process_material->set_blend_mode(material::blend_modes::opaque);
			post_process_material->set_blendable_location(postprocess_material_path_list[index]);
			post_process_material->set_pipeline_state(postprocess_material_texts_list[index][0]);

			post_process_volume_object_ptr->get_volume()->register_postprocess_materials(post_process_material);
		}
	}

	//	エディターカメラ生成
	{
		auto camera = std::make_shared<editor_camera>(main_camera);
		camera->set_position(system_units::convert_m_to_unit({ 0, 1, 8 }));
		camera->set_rotation({ 0, DirectX::XM_PI, 0 });
		camera->set_rotation_speed(camera->get_rotation_speed() * 0.25f);
		camera->set_move_speed(camera->get_move_speed() * 0.25f);
		register_object(camera);
		editor_camera_object = camera;
	}

	//	エディターアイテム用コンテキスト生成
	{
		editor_context = editor_items::generate_context();
	}

	new_timeline();

	return true;
}

bool edit_timeline_scene::uninitialize_core()
{
	clear_active_effects();
	clear_actor_models();
	return true;
}

void edit_timeline_scene::update_core(float elapsed_time)
{
	advance_playback(elapsed_time);
	apply_timeline_state();
}

void edit_timeline_scene::imgui_render_core()
{
	if (ImGui::Begin("Timeline"))
	{
		imgui_render_actors();

		ImGui::Separator();

		//	タイムラインファイル
		if (ImGui::Button("New Timeline"))
			new_timeline();
		ImGui::SameLine();
		if (ImGui::Button("Load Timeline"))
			load_timeline();
		ImGui::SameLine();
		if (ImGui::Button("Save Timeline"))
			save_timeline();

		ImGui::Separator();
		imgui_render_transport();

		ImGui::Separator();
		imgui_render_sequencer();

		ImGui::Separator();
		imgui_render_selected_entry();
	}
	ImGui::End();

	apply_timeline_state();
}

void edit_timeline_scene::add_actor()
{
	std::string file_path;
	if (!editor_items::open_file_dialog(ModelFileDialogFilter, file_path))
		return;

	model_resource_ptr resource;
	if (!model_resource_manager::instance()->load(resource, file_path))
		return;

	auto model = std::make_shared<model_object>(file_path, resource);
	object_manager::instance()->register_object(model);

	timeline_resource::actor_entry actor;
	actor.model_path = file_path;
	actor.name = std::filesystem::path(file_path).stem().string();

	timeline.actors.push_back(actor);
	actor_models.push_back(model);
	last_active_animation_per_actor.push_back(-1);
}

void edit_timeline_scene::remove_actor(std::size_t actor_index)
{
	if (actor_index >= timeline.actors.size())
		return;

	if (actor_models[actor_index])
		object_manager::instance()->unregister_object(actor_models[actor_index]);

	timeline.actors.erase(timeline.actors.begin() + actor_index);
	actor_models.erase(actor_models.begin() + actor_index);
	last_active_animation_per_actor.erase(last_active_animation_per_actor.begin() + actor_index);

	for (auto& effect : timeline.effects)
	{
		if (effect.actor_index == static_cast<int>(actor_index))
			effect.actor_index = -1;
		else if (effect.actor_index > static_cast<int>(actor_index))
			--effect.actor_index;
	}

	selected_entry = -1;
}

void edit_timeline_scene::rebuild_actor_models()
{
	clear_actor_models();

	actor_models.reserve(timeline.actors.size());
	last_active_animation_per_actor.assign(timeline.actors.size(), -1);

	for (auto& actor : timeline.actors)
	{
		model_resource_ptr resource;
		model_resource_manager::instance()->load(resource, actor.model_path);
		std::shared_ptr<model_object> model;
		if (resource)
		{
			model = std::make_shared<model_object>(actor.model_path, resource);
			object_manager::instance()->register_object(model);
		}
		actor_models.push_back(model);
	}
}

void edit_timeline_scene::clear_actor_models()
{
	for (auto& model : actor_models)
	{
		if (model)
			object_manager::instance()->unregister_object(model);
	}
	actor_models.clear();
	last_active_animation_per_actor.clear();
}

void edit_timeline_scene::new_timeline()
{
	clear_active_effects();
	clear_actor_models();

	timeline = timeline_resource();
	is_playing = false;
	current_frame = 0;
	current_frame_float = 0.0f;
	selected_entry = -1;
}

void edit_timeline_scene::load_timeline()
{
	std::string file_path;
	if (!editor_items::open_file_dialog(TimelineFileDialogFilter, file_path))
		return;

	timeline_resource loaded;
	if (!loaded.deserialize(file_path))
		return;

	clear_active_effects();
	timeline = loaded;
	is_playing = false;
	current_frame = 0;
	current_frame_float = 0.0f;
	selected_entry = -1;

	rebuild_actor_models();
}

void edit_timeline_scene::save_timeline()
{
	std::string file_path;
	const std::wstring default_extension = L"." + std::wstring(timeline_resource::extension.begin(), timeline_resource::extension.end());
	if (!editor_items::save_file_dialog(TimelineFileDialogFilter, file_path, default_extension.c_str()))
		return;

	timeline.serialize(file_path);
}

void edit_timeline_scene::advance_playback(float elapsed_time)
{
	if (!is_playing)
		return;

	current_frame_float += elapsed_time * timeline.fps;

	const float total = static_cast<float>(std::max(1, timeline.total_frame));
	if (current_frame_float >= total)
	{
		if (loop_playback)
			current_frame_float = std::fmod(current_frame_float, total);
		else
		{
			current_frame_float = total;
			is_playing = false;
		}
	}

	current_frame = static_cast<int>(current_frame_float);
}

void edit_timeline_scene::apply_timeline_state()
{
	//	アクター
	for (std::size_t actor_index = 0; actor_index < timeline.actors.size(); ++actor_index)
	{
		auto& actor = timeline.actors[actor_index];
		auto model = (actor_index < actor_models.size()) ? actor_models[actor_index] : nullptr;

		int active_index = -1;
		for (std::size_t entry_index = 0; entry_index < actor.animations.size(); ++entry_index)
		{
			auto& entry = actor.animations[entry_index];
			if (current_frame >= entry.start_frame && current_frame < entry.end_frame)
			{
				active_index = static_cast<int>(entry_index);
				break;
			}
		}

		if (model)
		{
			if (active_index >= 0)
			{
				auto& entry = actor.animations[active_index];
				const std::size_t animation_index = model->get_animation_index_from_name(entry.animation_name);
				if (animation_index != static_cast<std::size_t>(-1))
				{
					if (active_index != last_active_animation_per_actor[actor_index])
						model->play_animation(animation_index, entry.loop, 0.0f);

					model->set_animation_loop(entry.loop);

					const float local_time = static_cast<float>(current_frame - entry.start_frame) / std::max(1.0f, timeline.fps);
					model->set_animation_time(local_time);
				}
			}
		}
		last_active_animation_per_actor[actor_index] = active_index;
	}

	//	エフェクト
	for (std::size_t index = 0; index < timeline.effects.size(); ++index)
	{
		auto& entry = timeline.effects[index];
		const bool should_be_active = current_frame >= entry.start_frame && current_frame <= entry.end_frame;
		auto it = active_effects.find(index);
		const bool currently_active = (it != active_effects.end());

		auto resolve_base_position = [&]() -> DirectX::XMFLOAT3
		{
			if (entry.actor_index >= 0 &&
				static_cast<std::size_t>(entry.actor_index) < actor_models.size() &&
				actor_models[entry.actor_index])
			{
				return actor_models[entry.actor_index]->get_position();
			}
			return { 0, 0, 0 };
		};

		if (should_be_active && !currently_active)
		{
			if (entry.effect_path.empty())
				continue;

			effect_resource_ptr resource;
			if (effect_resource_manager::instance()->load(resource, entry.effect_path))
			{
				auto instance = std::make_shared<effect_instance>(resource);
				const DirectX::XMFLOAT3 base_position = resolve_base_position();
				instance->set_position({
					base_position.x + entry.offset_position.x,
					base_position.y + entry.offset_position.y,
					base_position.z + entry.offset_position.z });
				effect_manager::instance()->register_instance(instance);
				active_effects.emplace(index, instance);
			}
		}
		else if (!should_be_active && currently_active)
		{
			effect_manager::instance()->unregister_instance(it->second);
			active_effects.erase(it);
		}
		else if (should_be_active && currently_active)
		{
			const float local_time = static_cast<float>(current_frame - entry.start_frame) / std::max(1.0f, timeline.fps);
			it->second->set_animation_time(local_time);

			//	アクター追従
			const DirectX::XMFLOAT3 base_position = resolve_base_position();
			it->second->set_position({
				base_position.x + entry.offset_position.x,
				base_position.y + entry.offset_position.y,
				base_position.z + entry.offset_position.z });
		}
	}

	//	区間内なら開始姿勢→終了姿勢へ補間してタイムライン側が直接カメラを制御する。
	int active_camera_index = -1;
	for (std::size_t index = 0; index < timeline.cameras.size(); ++index)
	{
		auto& entry = timeline.cameras[index];
		if (current_frame >= entry.start_frame && current_frame < entry.end_frame)
		{
			active_camera_index = static_cast<int>(index);
			break;
		}
	}

	if (active_camera_index >= 0 && main_camera)
	{
		auto& entry = timeline.cameras[active_camera_index];

		if (editor_camera_object && editor_camera_object->is_active())
			editor_camera_object->set_active(false);

		const float denom = static_cast<float>(std::max(1, entry.end_frame - entry.start_frame));
		const float t = math::saturate(static_cast<float>(current_frame - entry.start_frame) / denom);

		const DirectX::XMFLOAT3 position =
		{
			entry.start.position.x + (entry.end.position.x - entry.start.position.x) * t,
			entry.start.position.y + (entry.end.position.y - entry.start.position.y) * t,
			entry.start.position.z + (entry.end.position.z - entry.start.position.z) * t,
		};

		const DirectX::XMVECTOR quaternion_start = DirectX::XMLoadFloat4(&math::rotator(entry.start.rotation).convert_quaternion());
		const DirectX::XMVECTOR quaternion_end = DirectX::XMLoadFloat4(&math::rotator(entry.end.rotation).convert_quaternion());
		DirectX::XMFLOAT4 quaternion;
		DirectX::XMStoreFloat4(&quaternion, DirectX::XMQuaternionSlerp(quaternion_start, quaternion_end, t));

		const float fov_y = entry.start.fov_y + (entry.end.fov_y - entry.start.fov_y) * t;

		main_camera->set_position(position);
		main_camera->set_rotation(math::rotator(quaternion));
		main_camera->get_camera()->set_fov_y(fov_y);
	}
	else if (editor_camera_object && !editor_camera_object->is_active())
	{
		//	タイムライン区間から抜けたら、現在のカメラ姿勢を引き継いでフリーカメラへ戻す
		if (main_camera)
		{
			editor_camera_object->set_position(main_camera->get_position());
			editor_camera_object->set_rotation(main_camera->get_rotation());
		}
		editor_camera_object->set_active(true);
	}
}

void edit_timeline_scene::clear_active_effects()
{
	for (auto& pair : active_effects)
		effect_manager::instance()->unregister_instance(pair.second);
	active_effects.clear();
}

void edit_timeline_scene::imgui_render_actors()
{
	ImGui::Text("Actors");
	ImGui::SameLine();
	if (ImGui::Button("Add Actor"))
		add_actor();

	int remove_index = -1;
	for (std::size_t index = 0; index < timeline.actors.size(); ++index)
	{
		ImGui::PushID(static_cast<int>(index));
		ImGui::BulletText("%s : %s", timeline.actors[index].name.c_str(), timeline.actors[index].model_path.c_str());
		ImGui::SameLine();
		if (ImGui::SmallButton("Remove"))
			remove_index = static_cast<int>(index);
		ImGui::PopID();
	}

	if (remove_index >= 0)
		remove_actor(static_cast<std::size_t>(remove_index));
}

void edit_timeline_scene::imgui_render_transport()
{
	if (ImGui::Button(is_playing ? "Pause" : "Play"))
		is_playing = !is_playing;

	ImGui::SameLine();
	if (ImGui::Button("Stop"))
	{
		is_playing = false;
		current_frame = 0;
		current_frame_float = 0.0f;
	}

	ImGui::SameLine();
	ImGui::Checkbox("Loop", &loop_playback);

	ImGui::DragFloat("fps", &timeline.fps, 0.1f, 1.0f, 240.0f);

	if (ImGui::DragInt("total_frame", &timeline.total_frame, 1.0f, 1, 100000))
		timeline.total_frame = std::max(1, timeline.total_frame);

	if (ImGui::SliderInt("frame", &current_frame, 0, timeline.total_frame))
		current_frame_float = static_cast<float>(current_frame);
}

void edit_timeline_scene::imgui_render_sequencer()
{
	ImSequencer::Sequencer(&sequencer_interface, &current_frame, &sequencer_expanded, &selected_entry, &sequencer_first_frame,
		ImSequencer::SEQUENCER_EDIT_STARTEND | ImSequencer::SEQUENCER_CHANGE_FRAME |
		ImSequencer::SEQUENCER_ADD | ImSequencer::SEQUENCER_DEL | ImSequencer::SEQUENCER_COPYPASTE);

	current_frame_float = static_cast<float>(current_frame);
}

void edit_timeline_scene::imgui_render_selected_entry()
{
	if (selected_entry < 0)
	{
		ImGui::Text("(select an entry in the sequencer)");
		return;
	}

	std::size_t remaining = static_cast<std::size_t>(selected_entry);

	//	アニメーション
	for (std::size_t actor_index = 0; actor_index < timeline.actors.size(); ++actor_index)
	{
		auto& actor = timeline.actors[actor_index];
		if (remaining < actor.animations.size())
		{
			auto& entry = actor.animations[remaining];
			ImGui::Text("Animation Entry (%s)", actor.name.c_str());

			auto model = (actor_index < actor_models.size()) ? actor_models[actor_index] : nullptr;
			if (model && ImGui::BeginCombo("animation_name", entry.animation_name.c_str()))
			{
				for (auto& name : model->get_animation_names())
				{
					const bool is_selected = (entry.animation_name == name);
					if (ImGui::Selectable(std::string(name).c_str(), is_selected))
						entry.animation_name = std::string(name);
				}
				ImGui::EndCombo();
			}
			ImGui::Checkbox("loop", &entry.loop);
			ImGui::DragInt("start_frame", &entry.start_frame, 1.0f, 0, timeline.total_frame);
			ImGui::DragInt("end_frame", &entry.end_frame, 1.0f, 0, timeline.total_frame);
			return;
		}
		remaining -= actor.animations.size();
	}

	//	エフェクト
	if (remaining < timeline.effects.size())
	{
		auto& entry = timeline.effects[remaining];
		ImGui::Text("Effect Entry");
		ImGui::Text("path: %s", entry.effect_path.c_str());
		ImGui::SameLine();
		if (ImGui::Button("Load##effect_path"))
		{
			std::string file_path;
			if (editor_items::open_file_dialog(EffectFileDialogFilter, file_path))
				entry.effect_path = file_path;
		}

		//	追従先アクター
		{
			const std::string preview = (entry.actor_index >= 0 && static_cast<std::size_t>(entry.actor_index) < timeline.actors.size())
				? timeline.actors[entry.actor_index].name
				: std::string("World");
			if (ImGui::BeginCombo("follow_actor", preview.c_str()))
			{
				if (ImGui::Selectable("World", entry.actor_index < 0))
					entry.actor_index = -1;
				for (std::size_t actor_index = 0; actor_index < timeline.actors.size(); ++actor_index)
				{
					const bool is_selected = (entry.actor_index == static_cast<int>(actor_index));
					if (ImGui::Selectable(timeline.actors[actor_index].name.c_str(), is_selected))
						entry.actor_index = static_cast<int>(actor_index);
				}
				ImGui::EndCombo();
			}
		}

		ImGui::DragFloat3(entry.actor_index < 0 ? "world_position" : "offset_position", &entry.offset_position.x, 0.01f);
		ImGui::DragInt("start_frame##effect", &entry.start_frame, 1.0f, 0, timeline.total_frame);
		ImGui::DragInt("end_frame##effect", &entry.end_frame, 1.0f, 0, timeline.total_frame);
		return;
	}
	remaining -= timeline.effects.size();

	//	カメラ
	if (remaining < timeline.cameras.size())
	{
		auto& entry = timeline.cameras[remaining];
		ImGui::Text("Camera Entry");

		ImGui::Text("Start");
		ImGui::DragFloat3("start_position", &entry.start.position.x, 0.01f);
		ImGui::DragFloat3("start_rotation(rad)", &entry.start.rotation.x, 0.01f);
		ImGui::SliderAngle("start_fov_y", &entry.start.fov_y, 1.0f, 179.0f);
		if (ImGui::Button("Set Start = Current Camera"))
		{
			if (main_camera)
			{
				entry.start.position = main_camera->get_position();
				entry.start.rotation = main_camera->get_rotation();
				entry.start.fov_y = main_camera->get_camera()->get_fov_y();
			}
		}

		ImGui::Separator();
		ImGui::Text("End");
		ImGui::DragFloat3("end_position", &entry.end.position.x, 0.01f);
		ImGui::DragFloat3("end_rotation(rad)", &entry.end.rotation.x, 0.01f);
		ImGui::SliderAngle("end_fov_y", &entry.end.fov_y, 1.0f, 179.0f);
		if (ImGui::Button("Set End = Current Camera"))
		{
			if (main_camera)
			{
				entry.end.position = main_camera->get_position();
				entry.end.rotation = main_camera->get_rotation();
				entry.end.fov_y = main_camera->get_camera()->get_fov_y();
			}
		}

		ImGui::Separator();
		ImGui::DragInt("start_frame##camera", &entry.start_frame, 1.0f, 0, timeline.total_frame);
		ImGui::DragInt("end_frame##camera", &entry.end_frame, 1.0f, 0, timeline.total_frame);
	}
}
#endif	//	defined(USE_IMGUI)
