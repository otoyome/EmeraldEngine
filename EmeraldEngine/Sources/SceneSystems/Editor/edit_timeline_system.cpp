#include "SceneSystems/Editor/edit_timeline_system.h"
#include "Texture/texture_resource_utility.h"
#include "System/asset_browser.h"

#if defined(USE_IMGUI)
#include <algorithm>
#include <cmath>
#include <filesystem>

#include "Model/model_instance.h"
#include "Graphics/View/camera.h"

#include "object/light_object.h"
#include "object/model_object.h"
#include "object/post_process_volume_object.h"
#include "object/object_manager.h"

#include "Graphics/Pipeline/custom_pipeline_state.h"

#include "Editor/editor_camera.h"

namespace
{
	//	モデル
	constexpr const TCHAR* ModelFileDialogFilter =
		L"モデルファイル (*.emo)\0*.emo;\0\
		すべてのファイル (*.*)\0*.*\0";

	//	エフェクト
	constexpr const TCHAR* EffectFileDialogFilter =
		L"エフェクトファイル (*.eme)\0*.eme;\0\
		すべてのファイル (*.*)\0*.*\0";

	//	タイムライン
	constexpr const TCHAR* TimelineFileDialogFilter =
		L"タイムラインファイル (*.etl)\0*.etl;\0\
		すべてのファイル (*.*)\0*.*\0";

	constexpr float sequencer_units_per_second = 100.0f;

	//	オブジェクトごとのシーケンサー表示色
	constexpr unsigned int object_track_colors[] =
	{
		0xFF3F8AE0,
		0xFF3FC0C0,
		0xFF8ABF3F,
		0xFFBF8A3F,
		0xFFBF3FA0,
		0xFF8A3FBF,
	};
	unsigned int object_track_color(std::size_t object_index)
	{
		return object_track_colors[object_index % ARRAYSIZE(object_track_colors)];
	}

	//	Objects パネルの種類コンボ用
	constexpr const char* object_kind_names[] = { "Actor", "Camera", "Effect", "Event" };
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
std::size_t edit_timeline_system::sequence_interface::total_animation_count() const
{
	std::size_t total = 0;
	for (auto& entry : owner->timeline.objects)
	{
		if (auto* actor = std::get_if<timeline_resource::object_entry::actor_data>(&entry.data))
			total += actor->animations.size();
	}
	return total;
}

bool edit_timeline_system::sequence_interface::resolve_animation(int flat_index, std::size_t& out_object_index, std::size_t& out_entry_index) const
{
	if (flat_index < 0)
		return false;

	std::size_t remaining = static_cast<std::size_t>(flat_index);
	for (std::size_t object_index = 0; object_index < owner->timeline.objects.size(); ++object_index)
	{
		auto* actor = std::get_if<timeline_resource::object_entry::actor_data>(&owner->timeline.objects[object_index].data);
		if (!actor)
			continue;
		if (remaining < actor->animations.size())
		{
			out_object_index = object_index;
			out_entry_index = remaining;
			return true;
		}
		remaining -= actor->animations.size();
	}
	return false;
}

std::size_t edit_timeline_system::sequence_interface::total_transform_count() const
{
	std::size_t total = 0;
	for (auto& entry : owner->timeline.objects)
	{
		if (auto* actor = std::get_if<timeline_resource::object_entry::actor_data>(&entry.data))
			total += actor->transforms.size();
	}
	return total;
}

bool edit_timeline_system::sequence_interface::resolve_transform(int flat_index, std::size_t& out_object_index, std::size_t& out_entry_index) const
{
	const std::size_t animation_total = total_animation_count();
	if (flat_index < 0 || static_cast<std::size_t>(flat_index) < animation_total)
		return false;

	std::size_t remaining = static_cast<std::size_t>(flat_index) - animation_total;
	for (std::size_t object_index = 0; object_index < owner->timeline.objects.size(); ++object_index)
	{
		auto* actor = std::get_if<timeline_resource::object_entry::actor_data>(&owner->timeline.objects[object_index].data);
		if (!actor)
			continue;
		if (remaining < actor->transforms.size())
		{
			out_object_index = object_index;
			out_entry_index = remaining;
			return true;
		}
		remaining -= actor->transforms.size();
	}
	return false;
}

std::size_t edit_timeline_system::sequence_interface::total_camera_count() const
{
	std::size_t total = 0;
	for (auto& entry : owner->timeline.objects)
	{
		if (std::holds_alternative<timeline_resource::object_entry::camera_data>(entry.data))
			++total;
	}
	return total;
}

bool edit_timeline_system::sequence_interface::resolve_camera(int flat_index, std::size_t& out_object_index) const
{
	const std::size_t entry_total = total_animation_count() + total_transform_count();
	if (flat_index < 0 || static_cast<std::size_t>(flat_index) < entry_total)
		return false;

	std::size_t remaining = static_cast<std::size_t>(flat_index) - entry_total;
	for (std::size_t object_index = 0; object_index < owner->timeline.objects.size(); ++object_index)
	{
		if (!std::holds_alternative<timeline_resource::object_entry::camera_data>(owner->timeline.objects[object_index].data))
			continue;
		if (remaining == 0)
		{
			out_object_index = object_index;
			return true;
		}
		--remaining;
	}
	return false;
}

std::size_t edit_timeline_system::sequence_interface::total_effect_count() const
{
	std::size_t total = 0;
	for (auto& entry : owner->timeline.objects)
	{
		if (std::holds_alternative<timeline_resource::object_entry::effect_data>(entry.data))
			++total;
	}
	return total;
}

bool edit_timeline_system::sequence_interface::resolve_effect(int flat_index, std::size_t& out_object_index) const
{
	const std::size_t entry_total = total_animation_count() + total_transform_count() + total_camera_count();
	if (flat_index < 0 || static_cast<std::size_t>(flat_index) < entry_total)
		return false;

	std::size_t remaining = static_cast<std::size_t>(flat_index) - entry_total;
	for (std::size_t object_index = 0; object_index < owner->timeline.objects.size(); ++object_index)
	{
		if (!std::holds_alternative<timeline_resource::object_entry::effect_data>(owner->timeline.objects[object_index].data))
			continue;
		if (remaining == 0)
		{
			out_object_index = object_index;
			return true;
		}
		--remaining;
	}
	return false;
}

std::size_t edit_timeline_system::sequence_interface::total_event_count() const
{
	std::size_t total = 0;
	for (auto& entry : owner->timeline.objects)
	{
		if (std::holds_alternative<timeline_resource::object_entry::event_data>(entry.data))
			++total;
	}
	return total;
}

bool edit_timeline_system::sequence_interface::resolve_event(int flat_index, std::size_t& out_object_index) const
{
	const std::size_t entry_total = total_animation_count() + total_transform_count() + total_camera_count() + total_effect_count();
	if (flat_index < 0 || static_cast<std::size_t>(flat_index) < entry_total)
		return false;

	std::size_t remaining = static_cast<std::size_t>(flat_index) - entry_total;
	for (std::size_t object_index = 0; object_index < owner->timeline.objects.size(); ++object_index)
	{
		if (!std::holds_alternative<timeline_resource::object_entry::event_data>(owner->timeline.objects[object_index].data))
			continue;
		if (remaining == 0)
		{
			out_object_index = object_index;
			return true;
		}
		--remaining;
	}
	return false;
}

void edit_timeline_system::sequence_interface::sync_from_entries()
{
	const float fps = sequencer_units_per_second;
	const int item_count = GetItemCount();
	frame_cache.assign(item_count, { 0, 0 });

	for (int index = 0; index < item_count; ++index)
	{
		float start_time = 0.0f, end_time = 0.0f;

		std::size_t object_index, entry_index;
		if (resolve_animation(index, object_index, entry_index))
		{
			auto* actor = std::get_if<timeline_resource::object_entry::actor_data>(&owner->timeline.objects[object_index].data);
			auto& entry = actor->animations[entry_index];
			start_time = entry.start_time;
			end_time = entry.end_time;
		}
		else if (resolve_transform(index, object_index, entry_index))
		{
			auto* actor = std::get_if<timeline_resource::object_entry::actor_data>(&owner->timeline.objects[object_index].data);
			auto& entry = actor->transforms[entry_index];
			start_time = entry.start_time;
			end_time = entry.end_time;
		}
		else if (resolve_camera(index, object_index))
		{
			auto* entry = std::get_if<timeline_resource::object_entry::camera_data>(&owner->timeline.objects[object_index].data);
			start_time = entry->start_time;
			end_time = entry->end_time;
		}
		else if (resolve_effect(index, object_index))
		{
			auto* entry = std::get_if<timeline_resource::object_entry::effect_data>(&owner->timeline.objects[object_index].data);
			start_time = entry->start_time;
			end_time = entry->end_time;
		}
		else if (resolve_event(index, object_index))
		{
			auto* entry = std::get_if<timeline_resource::object_entry::event_data>(&owner->timeline.objects[object_index].data);
			start_time = entry->time;
			end_time = entry->time;
		}

		frame_cache[index] =
		{
			static_cast<int>(std::lround(start_time * fps)),
			static_cast<int>(std::lround(end_time * fps)),
		};
	}
}

void edit_timeline_system::sequence_interface::sync_to_entries()
{
	const float fps = sequencer_units_per_second;
	const int item_count = std::min(static_cast<int>(frame_cache.size()), GetItemCount());

	for (int index = 0; index < item_count; ++index)
	{
		const float start_time = frame_cache[index].first / fps;
		const float end_time = frame_cache[index].second / fps;

		std::size_t object_index, entry_index;
		if (resolve_animation(index, object_index, entry_index))
		{
			auto* actor = std::get_if<timeline_resource::object_entry::actor_data>(&owner->timeline.objects[object_index].data);
			auto& entry = actor->animations[entry_index];
			entry.start_time = start_time;
			entry.end_time = end_time;
			continue;
		}
		if (resolve_transform(index, object_index, entry_index))
		{
			auto* actor = std::get_if<timeline_resource::object_entry::actor_data>(&owner->timeline.objects[object_index].data);
			auto& entry = actor->transforms[entry_index];
			entry.start_time = start_time;
			entry.end_time = end_time;
			continue;
		}
		if (resolve_camera(index, object_index))
		{
			auto* entry = std::get_if<timeline_resource::object_entry::camera_data>(&owner->timeline.objects[object_index].data);
			entry->start_time = start_time;
			entry->end_time = end_time;
			continue;
		}
		if (resolve_effect(index, object_index))
		{
			auto* entry = std::get_if<timeline_resource::object_entry::effect_data>(&owner->timeline.objects[object_index].data);
			entry->start_time = start_time;
			entry->end_time = end_time;
			continue;
		}
		if (resolve_event(index, object_index))
		{
			auto* entry = std::get_if<timeline_resource::object_entry::event_data>(&owner->timeline.objects[object_index].data);
			entry->time = start_time;
		}
	}
}

int edit_timeline_system::sequence_interface::GetFrameMin() const
{
	return 0;
}

int edit_timeline_system::sequence_interface::GetFrameMax() const
{
	const float fps = sequencer_units_per_second;
	return static_cast<int>(std::lround(owner->timeline.total_time * fps));
}

int edit_timeline_system::sequence_interface::GetItemCount() const
{
	return static_cast<int>(total_animation_count() + total_transform_count() + total_camera_count() + total_effect_count() + total_event_count());
}

int edit_timeline_system::sequence_interface::GetItemTypeCount() const
{
	return static_cast<int>(owner->timeline.objects.size()) * 5;
}

const char* edit_timeline_system::sequence_interface::GetItemTypeName(int type_index) const
{
	const int object_count = static_cast<int>(owner->timeline.objects.size());
	if (type_index < object_count)
	{
		type_name_cache = owner->timeline.objects[type_index].name + " (Animation)";
		return type_name_cache.c_str();
	}
	if (type_index < object_count * 2)
	{
		type_name_cache = owner->timeline.objects[type_index - object_count].name + " (Transform)";
		return type_name_cache.c_str();
	}
	if (type_index < object_count * 3)
	{
		type_name_cache = owner->timeline.objects[type_index - object_count * 2].name + " (Camera)";
		return type_name_cache.c_str();
	}
	if (type_index < object_count * 4)
	{
		type_name_cache = owner->timeline.objects[type_index - object_count * 3].name + " (Effect)";
		return type_name_cache.c_str();
	}
	type_name_cache = owner->timeline.objects[type_index - object_count * 4].name + " (Event)";
	return type_name_cache.c_str();
}

const char* edit_timeline_system::sequence_interface::GetItemLabel(int index) const
{
	std::size_t object_index, entry_index;
	if (resolve_animation(index, object_index, entry_index))
	{
		auto& object = owner->timeline.objects[object_index];
		auto* actor = std::get_if<timeline_resource::object_entry::actor_data>(&object.data);
		auto& entry = actor->animations[entry_index];
		label_cache = "[" + object.name + "] " + (entry.animation_name.empty() ? std::string("(none)") : entry.animation_name);
		return label_cache.c_str();
	}
	if (resolve_transform(index, object_index, entry_index))
	{
		auto& object = owner->timeline.objects[object_index];
		label_cache = "[" + object.name + "] transform " + std::to_string(entry_index);
		return label_cache.c_str();
	}
	if (resolve_camera(index, object_index))
	{
		auto& object = owner->timeline.objects[object_index];
		label_cache = "[" + object.name + "] camera";
		return label_cache.c_str();
	}
	if (resolve_effect(index, object_index))
	{
		auto& object = owner->timeline.objects[object_index];
		auto* entry = std::get_if<timeline_resource::object_entry::effect_data>(&object.data);
		label_cache = "[" + object.name + "] " + (entry->effect_path.empty() ? std::string("(none)") : entry->effect_path);
		return label_cache.c_str();
	}

	resolve_event(index, object_index);
	auto& object = owner->timeline.objects[object_index];
	auto* entry = std::get_if<timeline_resource::object_entry::event_data>(&object.data);
	label_cache = "[" + object.name + "] " + (entry->identifier.empty() ? std::string("(none)") : entry->identifier);
	return label_cache.c_str();
}

void edit_timeline_system::sequence_interface::Get(int index, int** start, int** end, int* type, unsigned int* color)
{
	if (static_cast<std::size_t>(index) >= frame_cache.size())
		frame_cache.resize(index + 1, { 0, 0 });

	if (start) *start = &frame_cache[index].first;
	if (end) *end = &frame_cache[index].second;

	const int object_count = static_cast<int>(owner->timeline.objects.size());

	std::size_t object_index, entry_index;
	if (resolve_animation(index, object_index, entry_index))
	{
		if (type) *type = static_cast<int>(object_index);
		if (color) *color = object_track_color(object_index);
		return;
	}
	if (resolve_transform(index, object_index, entry_index))
	{
		if (type) *type = object_count + static_cast<int>(object_index);
		if (color) *color = object_track_color(object_index) ^ 0x00404040;
		return;
	}
	if (resolve_camera(index, object_index))
	{
		if (type) *type = object_count * 2 + static_cast<int>(object_index);
		if (color) *color = 0xFFC0708F;
		return;
	}
	if (resolve_effect(index, object_index))
	{
		if (type) *type = object_count * 3 + static_cast<int>(object_index);
		if (color) *color = 0xFF4FBF6F;
		return;
	}
	if (resolve_event(index, object_index))
	{
		if (type) *type = object_count * 4 + static_cast<int>(object_index);
		if (color) *color = 0xFFE0C040;
	}
}

void edit_timeline_system::sequence_interface::Add(int type)
{
	const int object_count = static_cast<int>(owner->timeline.objects.size());
	constexpr float default_length = 1.0f;

	if (type < object_count)
	{
		auto* actor = std::get_if<timeline_resource::object_entry::actor_data>(&owner->timeline.objects[type].data);
		if (!actor)
			return;

		auto& animation = actor->animations.emplace_back();
		animation.start_time = owner->current_time;
		animation.end_time = std::min(owner->timeline.total_time, owner->current_time + default_length);
		if (static_cast<std::size_t>(type) < owner->object_models.size() && owner->object_models[type])
		{
			auto names = owner->object_models[type]->get_animation_names();
			if (!names.empty())
				animation.animation_name = std::string(names.front());
		}
	}
	else if (type < object_count * 2)
	{
		const int object_index = type - object_count;
		auto* actor = std::get_if<timeline_resource::object_entry::actor_data>(&owner->timeline.objects[object_index].data);
		if (!actor)
			return;

		auto& transform = actor->transforms.emplace_back();
		transform.start_time = owner->current_time;
		transform.end_time = std::min(owner->timeline.total_time, owner->current_time + default_length);
		if (static_cast<std::size_t>(object_index) < owner->object_models.size() && owner->object_models[object_index])
		{
			transform.start.position = owner->object_models[object_index]->get_world_translation().to_float3();
			transform.end.position = transform.start.position;
			transform.start.rotation = math::rotatorf(owner->object_models[object_index]->get_world_euler());
			transform.end.rotation = transform.start.rotation;
		}
	}
}

void edit_timeline_system::sequence_interface::Del(int index)
{
	std::size_t object_index, entry_index;
	if (resolve_animation(index, object_index, entry_index))
	{
		auto* actor = std::get_if<timeline_resource::object_entry::actor_data>(&owner->timeline.objects[object_index].data);
		actor->animations.erase(actor->animations.begin() + entry_index);
		owner->selected_entry = -1;
		return;
	}
	if (resolve_transform(index, object_index, entry_index))
	{
		auto* actor = std::get_if<timeline_resource::object_entry::actor_data>(&owner->timeline.objects[object_index].data);
		actor->transforms.erase(actor->transforms.begin() + entry_index);
		owner->selected_entry = -1;
		return;
	}
}

void edit_timeline_system::sequence_interface::Duplicate(int index)
{
	std::size_t object_index, entry_index;
	if (resolve_animation(index, object_index, entry_index))
	{
		auto* actor = std::get_if<timeline_resource::object_entry::actor_data>(&owner->timeline.objects[object_index].data);
		actor->animations.push_back(actor->animations[entry_index]);
		return;
	}
	if (resolve_transform(index, object_index, entry_index))
	{
		auto* actor = std::get_if<timeline_resource::object_entry::actor_data>(&owner->timeline.objects[object_index].data);
		actor->transforms.push_back(actor->transforms[entry_index]);
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
//	edit_timeline_system

edit_timeline_system::edit_timeline_system()
: scene_system()
{
	sequencer_interface.owner = this;
}

edit_timeline_system::~edit_timeline_system()
{
}

const view_parameters* edit_timeline_system::find_current_view_parameters() const
{
	if (editor_camera && editor_camera->is_active())
		return &editor_camera->get_camera()->get_parameters();
	if (main_camera)
		return &main_camera->get_camera()->get_parameters();
	return nullptr;
}

bool edit_timeline_system::initialize_core()
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
			light->set_flag(light::flags::use_shadow, false);
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
		for (const auto& object : get_context().get_objects().search_objects<editor_camera_object>())
		{
			editor_camera = object;
			break;
		}
		if (!editor_camera)
		{
			editor_camera = std::make_shared<editor_camera_object>("エディターカメラ",
				system_units::convert_m_to_unit(math::vector3d(0.0, 1.0, 8.0)),
				math::rotatorf(0, math::constants<float>::pi, 0),
				0.25f);
			register_object(editor_camera);
		}
	}

	//	エディターアイテム用コンテキスト生成
	{
		editor_context = editor_items::generate_context();
	}

	new_timeline();

	return true;
}

bool edit_timeline_system::uninitialize_core()
{
	clear_active_effects();
	clear_object_models();
	editor_camera.reset();
	return true;
}

void edit_timeline_system::update_core(float elapsed_time)
{
	{
		std::string file_path;
		if (get_context().take_open_file(file_path))
			open_timeline(file_path);
	}

	advance_playback(elapsed_time);
	apply_timeline_state();
}

void edit_timeline_system::imgui_render_core()
{
	if (ImGui::Begin("Timeline"))
	{
		imgui_render_objects();
	}
	ImGui::End();

	if (ImGui::Begin("Timeline Sequencer"))
	{
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
	}
	ImGui::End();

	if (ImGui::Begin("Timeline Parameters"))
	{
		imgui_render_selected_entry();
	}
	ImGui::End();

	apply_timeline_state();
}

void edit_timeline_system::add_object()
{
	timeline_resource::object_entry entry;
	entry.name = "Object " + std::to_string(timeline.objects.size());

	timeline.objects.push_back(entry);
	object_models.push_back(nullptr);
	last_active_animation_per_object.push_back(-1);
}

void edit_timeline_system::remove_object(std::size_t object_index)
{
	if (object_index >= timeline.objects.size())
		return;

	if (object_models[object_index])
		get_context().get_objects().unregister_object(object_models[object_index]);

	std::unordered_map<size_t, effect_instance_ptr> shifted;
	for (auto iterator = active_effects.begin(); iterator != active_effects.end();)
	{
		if (iterator->first == object_index)
		{
			get_context().get_effects().unregister_instance(iterator->second);
			iterator = active_effects.erase(iterator);
			continue;
		}
		if (iterator->first > object_index)
		{
			shifted.emplace(iterator->first - 1, iterator->second);
			iterator = active_effects.erase(iterator);
			continue;
		}
		++iterator;
	}
	for (auto& pair : shifted)
		active_effects.emplace(pair.first, pair.second);

	timeline.objects.erase(timeline.objects.begin() + object_index);
	object_models.erase(object_models.begin() + object_index);
	last_active_animation_per_object.erase(last_active_animation_per_object.begin() + object_index);

	selected_entry = -1;
}

void edit_timeline_system::on_object_kind_changed(std::size_t object_index, timeline_resource::object_entry::kind previous_kind)
{
	if (previous_kind == timeline_resource::object_entry::kind::actor &&
		object_index < object_models.size() && object_models[object_index])
	{
		get_context().get_objects().unregister_object(object_models[object_index]);
		object_models[object_index] = nullptr;
		if (object_index < last_active_animation_per_object.size())
			last_active_animation_per_object[object_index] = -1;
	}

	if (previous_kind == timeline_resource::object_entry::kind::effect)
	{
		auto it = active_effects.find(object_index);
		if (it != active_effects.end())
		{
			get_context().get_effects().unregister_instance(it->second);
			active_effects.erase(it);
		}
	}

	selected_entry = -1;
}

void edit_timeline_system::rebuild_object_models()
{
	clear_object_models();

	object_models.reserve(timeline.objects.size());
	last_active_animation_per_object.assign(timeline.objects.size(), -1);

	for (auto& entry : timeline.objects)
	{
		std::shared_ptr<model_object> model;
		if (auto* actor = std::get_if<timeline_resource::object_entry::actor_data>(&entry.data))
		{
			if (!actor->model_path.empty())
			{
				model_resource_ptr resource;
				model_resource_manager::instance()->load(resource, actor->model_path);
				if (resource)
				{
					model = std::make_shared<model_object>(actor->model_path, resource);
					get_context().get_objects().register_object(model);
				}
			}
		}
		object_models.push_back(model);
	}
}

void edit_timeline_system::clear_object_models()
{
	for (auto& model : object_models)
	{
		if (model)
			get_context().get_objects().unregister_object(model);
	}
	object_models.clear();
	last_active_animation_per_object.clear();
}

void edit_timeline_system::new_timeline()
{
	clear_active_effects();
	clear_object_models();

	timeline = timeline_resource();
	is_playing = false;
	current_time = 0.0f;
	current_frame = 0;
	selected_entry = -1;
}

void edit_timeline_system::load_timeline()
{
	std::string file_path;
	if (!editor_items::open_file_dialog(TimelineFileDialogFilter, file_path))
		return;

	open_timeline(file_path);
}

void edit_timeline_system::open_timeline(const std::string& file_path)
{
	timeline_resource loaded;
	if (!loaded.deserialize(file_path))
		return;

	clear_active_effects();
	timeline = loaded;
	is_playing = false;
	current_time = 0.0f;
	current_frame = 0;
	selected_entry = -1;

	rebuild_object_models();
}

void edit_timeline_system::save_timeline()
{
	std::string file_path;
	const std::wstring default_extension = L"." + system_utility::convert_shift_jis_to_utf16(timeline_resource::extension);
	if (!editor_items::save_file_dialog(TimelineFileDialogFilter, file_path, default_extension.c_str()))
		return;

	timeline.serialize(file_path);
}

void edit_timeline_system::advance_playback(float elapsed_time)
{
	previous_time = current_time;

	if (!is_playing)
		return;

	current_time += elapsed_time;

	const float total = std::max(0.01f, timeline.total_time);
	if (current_time >= total)
	{
		if (loop_playback)
			current_time = std::fmod(current_time, total);
		else
		{
			current_time = total;
			is_playing = false;
		}
	}
}

void edit_timeline_system::apply_timeline_state()
{
	for (std::size_t object_index = 0; object_index < timeline.objects.size(); ++object_index)
	{
		auto* actor = std::get_if<timeline_resource::object_entry::actor_data>(&timeline.objects[object_index].data);
		if (!actor)
			continue;

		auto model = (object_index < object_models.size()) ? object_models[object_index] : nullptr;

		int active_index = -1;
		for (std::size_t entry_index = 0; entry_index < actor->animations.size(); ++entry_index)
		{
			auto& animation = actor->animations[entry_index];
			if (current_time >= animation.start_time && current_time < animation.end_time)
			{
				active_index = static_cast<int>(entry_index);
				break;
			}
		}

		if (model)
		{
			if (active_index >= 0)
			{
				auto& animation = actor->animations[active_index];
				const std::size_t animation_index = model->get_animation_index_from_name(animation.animation_name);
				if (animation_index != static_cast<std::size_t>(-1))
				{
					if (active_index != last_active_animation_per_object[object_index])
						model->play_animation(animation_index, animation.loop, 0.0f);

					model->set_animation_loop(animation.loop);

					const float local_time = current_time - animation.start_time;
					model->set_animation_time(local_time);
				}
			}
		}
		last_active_animation_per_object[object_index] = active_index;

		if (model && is_playing)
		{
			for (auto& transform : actor->transforms)
			{
				if (current_time < transform.start_time || current_time >= transform.end_time)
					continue;

				const float denom = std::max(0.0001f, transform.end_time - transform.start_time);
				const float t = math::saturate((current_time - transform.start_time) / denom);

				const math::float3 position =
				{
					transform.start.position.x + (transform.end.position.x - transform.start.position.x) * t,
					transform.start.position.y + (transform.end.position.y - transform.start.position.y) * t,
					transform.start.position.z + (transform.end.position.z - transform.start.position.z) * t,
				};

				const math::quaternionf quaternion_start = math::rotatorf(transform.start.rotation).to_quaternion();
				const math::quaternionf quaternion_end = math::rotatorf(transform.end.rotation).to_quaternion();
				const math::float4 quaternion = math::slerp(quaternion_start, quaternion_end, t).to_float4();

				model->set_world_translation(math::vector3d(position));
				model->set_world_rotation(math::quaterniond(math::quaternionf(quaternion)));
				break;
			}
		}
	}

	for (std::size_t object_index = 0; object_index < timeline.objects.size(); ++object_index)
	{
		auto* effect_entry = std::get_if<timeline_resource::object_entry::effect_data>(&timeline.objects[object_index].data);
		if (!effect_entry)
			continue;

		const bool should_be_active = current_time >= effect_entry->start_time && current_time <= effect_entry->end_time;
		auto it = active_effects.find(object_index);
		const bool currently_active = (it != active_effects.end());

		if (should_be_active && !currently_active)
		{
			if (effect_entry->effect_path.empty())
				continue;

			effect_resource_ptr resource;
			if (effect_resource_manager::instance()->load(resource, effect_entry->effect_path))
			{
				auto instance = std::make_shared<effect_instance>(resource);
				instance->set_position(math::vector3d(effect_entry->position));
				get_context().get_effects().register_instance(instance);
				active_effects.emplace(object_index, instance);
			}
		}
		else if (!should_be_active && currently_active)
		{
			get_context().get_effects().unregister_instance(it->second);
			active_effects.erase(it);
		}
		else if (should_be_active && currently_active)
		{
			const float local_time = current_time - effect_entry->start_time;
			it->second->set_animation_time(local_time);
			it->second->set_position(math::vector3d(effect_entry->position));
		}
	}

	timeline_resource::object_entry::camera_data* active_camera_entry = nullptr;
	if (is_playing)
	{
		for (auto& entry : timeline.objects)
		{
			auto* camera = std::get_if<timeline_resource::object_entry::camera_data>(&entry.data);
			if (!camera)
				continue;
			if (current_time >= camera->start_time && current_time < camera->end_time)
			{
				active_camera_entry = camera;
				break;
			}
		}
	}

	if (active_camera_entry && main_camera)
	{
		auto& entry = *active_camera_entry;

		//	タイムラインのカメラを見せるためエディターカメラを一時停止
		if (editor_camera)
			editor_camera->set_active(false);

		const float denom = std::max(0.0001f, entry.end_time - entry.start_time);
		const float t = math::saturate((current_time - entry.start_time) / denom);

		const math::float3 position =
		{
			entry.start.position.x + (entry.end.position.x - entry.start.position.x) * t,
			entry.start.position.y + (entry.end.position.y - entry.start.position.y) * t,
			entry.start.position.z + (entry.end.position.z - entry.start.position.z) * t,
		};

		const math::quaternionf quaternion_start = math::rotatorf(entry.start.rotation).to_quaternion();
		const math::quaternionf quaternion_end = math::rotatorf(entry.end.rotation).to_quaternion();
		const math::float4 quaternion = math::slerp(quaternion_start, quaternion_end, t).to_float4();

		const float fov_y = entry.start.fov_y + (entry.end.fov_y - entry.start.fov_y) * t;

		main_camera->set_world_translation(math::vector3d(position));
		main_camera->set_world_rotation(math::quaterniond(math::quaternionf(quaternion)));
		main_camera->get_camera()->get_parameters().fov_y = fov_y;
	}
	else if (editor_camera && !editor_camera->is_active())
	{
		//	タイムライン区間から抜けたら、現在のカメラ姿勢を引き継いでフリーカメラへ戻す
		//	再開時にエディターカメラ側が直前のゲーム視点（メインカメラ）を引き継ぐ
		editor_camera->set_active(true);
	}

	if (is_playing)
	{
		for (auto& entry : timeline.objects)
		{
			auto* event = std::get_if<timeline_resource::object_entry::event_data>(&entry.data);
			if (!event)
				continue;

			const bool crossed = (previous_time <= current_time)
				? (event->time > previous_time && event->time <= current_time)
				: (event->time > previous_time || event->time <= current_time);
			if (crossed)
				get_context().get_objects().get_messenger().send_data(event->identifier, nullptr);
		}
	}

	previous_time = current_time;
}

void edit_timeline_system::clear_active_effects()
{
	for (auto& pair : active_effects)
		get_context().get_effects().unregister_instance(pair.second);
	active_effects.clear();
}

void edit_timeline_system::imgui_render_objects()
{
	ImGui::Text("Objects");
	ImGui::SameLine();
	if (ImGui::Button("Add Object"))
		add_object();

	int remove_index = -1;
	for (std::size_t index = 0; index < timeline.objects.size(); ++index)
	{
		auto& entry = timeline.objects[index];

		ImGui::PushID(static_cast<int>(index));
		ImGui::Separator();

		ImGui::SetNextItemWidth(100.0f);
		int kind_index = static_cast<int>(entry.object_kind());
		if (ImGui::Combo("kind", &kind_index, object_kind_names, ARRAYSIZE(object_kind_names)))
		{
			const auto previous_kind = entry.object_kind();
			const auto new_kind = static_cast<timeline_resource::object_entry::kind>(kind_index);
			if (new_kind != previous_kind)
			{
				switch (new_kind)
				{
					case timeline_resource::object_entry::kind::actor:
						entry.data = timeline_resource::object_entry::actor_data{};
						break;
					case timeline_resource::object_entry::kind::camera:
						entry.data = timeline_resource::object_entry::camera_data{};
						break;
					case timeline_resource::object_entry::kind::effect:
						entry.data = timeline_resource::object_entry::effect_data{};
						break;
					case timeline_resource::object_entry::kind::event:
						entry.data = timeline_resource::object_entry::event_data{};
						break;
				}
				on_object_kind_changed(index, previous_kind);
			}
		}
		ImGui::SameLine();
		ImGui::SetNextItemWidth(160.0f);
		ImGui::InputTextUtf8("name", entry.name);

		if (auto* actor = std::get_if<timeline_resource::object_entry::actor_data>(&entry.data))
		{
			ImGui::Text("model: %s", actor->model_path.c_str());
			ImGui::SameLine();
			if (ImGui::SmallButton("Load Model"))
			{
				std::string file_path;
				if (editor_items::open_file_dialog(ModelFileDialogFilter, file_path))
				{
					model_resource_ptr resource;
					if (model_resource_manager::instance()->load(resource, file_path))
					{
						if (object_models[index])
							get_context().get_objects().unregister_object(object_models[index]);

						auto model = std::make_shared<model_object>(file_path, resource);
						get_context().get_objects().register_object(model);

						actor->model_path = file_path;
						if (entry.name.empty())
							entry.name = std::filesystem::path(file_path).stem().string();
						object_models[index] = model;
					}
				}
			}

			if (ImGui::SmallButton("+Anim"))
				add_entry_and_select(static_cast<int>(index));
			ImGui::SameLine();
			if (ImGui::SmallButton("+Transform"))
				add_entry_and_select(static_cast<int>(timeline.objects.size()) + static_cast<int>(index));
		}

		if (ImGui::SmallButton("Remove"))
			remove_index = static_cast<int>(index);

		ImGui::PopID();
	}

	if (remove_index >= 0)
		remove_object(static_cast<std::size_t>(remove_index));
}

void edit_timeline_system::add_entry_and_select(int type)
{
	sequencer_interface.Add(type);

	const int object_count = static_cast<int>(timeline.objects.size());
	int flat_index = 0;
	if (type < object_count)
	{
		for (int object_index = 0; object_index < type; ++object_index)
		{
			if (auto* actor = std::get_if<timeline_resource::object_entry::actor_data>(&timeline.objects[object_index].data))
				flat_index += static_cast<int>(actor->animations.size());
		}
		if (auto* actor = std::get_if<timeline_resource::object_entry::actor_data>(&timeline.objects[type].data))
			flat_index += static_cast<int>(actor->animations.size()) - 1;
	}
	else
	{
		const int object_index = type - object_count;
		flat_index = static_cast<int>(sequencer_interface.total_animation_count());
		for (int i = 0; i < object_index; ++i)
		{
			if (auto* actor = std::get_if<timeline_resource::object_entry::actor_data>(&timeline.objects[i].data))
				flat_index += static_cast<int>(actor->transforms.size());
		}
		if (auto* actor = std::get_if<timeline_resource::object_entry::actor_data>(&timeline.objects[object_index].data))
			flat_index += static_cast<int>(actor->transforms.size()) - 1;
	}
	selected_entry = flat_index;
}

void edit_timeline_system::imgui_render_transport()
{
	if (ImGui::Button(is_playing ? "Pause" : "Play"))
		is_playing = !is_playing;

	ImGui::SameLine();
	if (ImGui::Button("Stop"))
	{
		is_playing = false;
		current_time = 0.0f;
	}

	ImGui::SameLine();
	ImGui::Checkbox("Loop", &loop_playback);

	if (ImGui::DragFloat("total_time(sec)", &timeline.total_time, 0.05f, 0.01f, 100000.0f))
		timeline.total_time = std::max(0.01f, timeline.total_time);

	ImGui::SliderFloat("time(sec)", &current_time, 0.0f, timeline.total_time);
}

void edit_timeline_system::imgui_render_sequencer()
{
	const float fps = sequencer_units_per_second;
	current_frame = static_cast<int>(std::lround(current_time * fps));

	sequencer_interface.sync_from_entries();

	ImSequencer::Sequencer(&sequencer_interface, &current_frame, &sequencer_expanded, &selected_entry, &sequencer_first_frame,
		ImSequencer::SEQUENCER_EDIT_STARTEND | ImSequencer::SEQUENCER_CHANGE_FRAME |
		ImSequencer::SEQUENCER_ADD | ImSequencer::SEQUENCER_DEL | ImSequencer::SEQUENCER_COPYPASTE);

	sequencer_interface.sync_to_entries();
	current_time = static_cast<float>(current_frame) / fps;
}

void edit_timeline_system::imgui_render_selected_entry()
{
	if (selected_entry < 0)
	{
		ImGui::Text("(select an entry in the sequencer)");
		return;
	}

	std::size_t remaining = static_cast<std::size_t>(selected_entry);

	//	アニメーション
	for (std::size_t object_index = 0; object_index < timeline.objects.size(); ++object_index)
	{
		auto& object = timeline.objects[object_index];
		auto* actor = std::get_if<timeline_resource::object_entry::actor_data>(&object.data);
		if (!actor)
			continue;
		if (remaining < actor->animations.size())
		{
			auto& entry = actor->animations[remaining];
			ImGui::Text("Animation Entry (%s)", object.name.c_str());

			auto model = (object_index < object_models.size()) ? object_models[object_index] : nullptr;
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
			ImGui::DragFloat("start_time(sec)", &entry.start_time, 0.01f, 0.0f, timeline.total_time);
			ImGui::DragFloat("end_time(sec)", &entry.end_time, 0.01f, 0.0f, timeline.total_time);
			return;
		}
		remaining -= actor->animations.size();
	}

	for (std::size_t object_index = 0; object_index < timeline.objects.size(); ++object_index)
	{
		auto& object = timeline.objects[object_index];
		auto* actor = std::get_if<timeline_resource::object_entry::actor_data>(&object.data);
		if (!actor)
			continue;
		if (remaining < actor->transforms.size())
		{
			auto& entry = actor->transforms[remaining];
			auto model = (object_index < object_models.size()) ? object_models[object_index] : nullptr;
			ImGui::Text("Transform Entry (%s)", object.name.c_str());

			ImGui::Text("Start");
			ImGui::DragFloat3("start_position", &entry.start.position.x, 0.01f);
			ImGui::DragFloat3("start_rotation(rad)", &entry.start.rotation.x, 0.01f);
			if (ImGui::Button("Set Start = Current Position"))
			{
				if (model)
				{
					entry.start.position = model->get_world_translation().to_float3();
					entry.start.rotation = math::rotatorf(model->get_world_euler());
				}
			}

			ImGui::Separator();
			ImGui::Text("End");
			ImGui::DragFloat3("end_position", &entry.end.position.x, 0.01f);
			ImGui::DragFloat3("end_rotation(rad)", &entry.end.rotation.x, 0.01f);
			if (ImGui::Button("Set End = Current Position"))
			{
				if (model)
				{
					entry.end.position = model->get_world_translation().to_float3();
					entry.end.rotation = math::rotatorf(model->get_world_euler());
				}
			}

			ImGui::Separator();
			ImGui::DragFloat("start_time(sec)##transform", &entry.start_time, 0.01f, 0.0f, timeline.total_time);
			ImGui::DragFloat("end_time(sec)##transform", &entry.end_time, 0.01f, 0.0f, timeline.total_time);
			return;
		}
		remaining -= actor->transforms.size();
	}

	//	カメラ
	for (std::size_t object_index = 0; object_index < timeline.objects.size(); ++object_index)
	{
		auto& object = timeline.objects[object_index];
		auto* entry = std::get_if<timeline_resource::object_entry::camera_data>(&object.data);
		if (!entry)
			continue;
		if (remaining == 0)
		{
			ImGui::Text("Camera Entry (%s)", object.name.c_str());

			ImGui::Text("Start");
			ImGui::DragFloat3("start_position", &entry->start.position.x, 0.01f);
			ImGui::DragFloat3("start_rotation(rad)", &entry->start.rotation.x, 0.01f);
			ImGui::SliderAngle("start_fov_y", &entry->start.fov_y, 1.0f, 179.0f);
			if (ImGui::Button("Set Start = Current Camera"))
			{
				if (auto current = find_current_view_parameters())
				{
					entry->start.position = current->position.to_float3();
					entry->start.rotation = current->rotation;
					entry->start.fov_y = current->fov_y;
				}
			}

			ImGui::Separator();
			ImGui::Text("End");
			ImGui::DragFloat3("end_position", &entry->end.position.x, 0.01f);
			ImGui::DragFloat3("end_rotation(rad)", &entry->end.rotation.x, 0.01f);
			ImGui::SliderAngle("end_fov_y", &entry->end.fov_y, 1.0f, 179.0f);
			if (ImGui::Button("Set End = Current Camera"))
			{
				if (auto current = find_current_view_parameters())
				{
					entry->end.position = current->position.to_float3();
					entry->end.rotation = current->rotation;
					entry->end.fov_y = current->fov_y;
				}
			}

			ImGui::Separator();
			ImGui::DragFloat("start_time(sec)##camera", &entry->start_time, 0.01f, 0.0f, timeline.total_time);
			ImGui::DragFloat("end_time(sec)##camera", &entry->end_time, 0.01f, 0.0f, timeline.total_time);
			return;
		}
		--remaining;
	}

	//	エフェクト
	for (std::size_t object_index = 0; object_index < timeline.objects.size(); ++object_index)
	{
		auto& object = timeline.objects[object_index];
		auto* entry = std::get_if<timeline_resource::object_entry::effect_data>(&object.data);
		if (!entry)
			continue;
		if (remaining == 0)
		{
			ImGui::Text("Effect Entry (%s)", object.name.c_str());
			ImGui::Text("path: %s", entry->effect_path.c_str());
			ImGui::SameLine();
			if (ImGui::Button("Load##effect_path"))
			{
				std::string file_path;
				if (editor_items::open_file_dialog(EffectFileDialogFilter, file_path))
					entry->effect_path = file_path;
			}

			ImGui::DragFloat3("position", &entry->position.x, 0.01f);
			ImGui::DragFloat("start_time(sec)##effect", &entry->start_time, 0.01f, 0.0f, timeline.total_time);
			ImGui::DragFloat("end_time(sec)##effect", &entry->end_time, 0.01f, 0.0f, timeline.total_time);
			return;
		}
		--remaining;
	}

	for (std::size_t object_index = 0; object_index < timeline.objects.size(); ++object_index)
	{
		auto& object = timeline.objects[object_index];
		auto* entry = std::get_if<timeline_resource::object_entry::event_data>(&object.data);
		if (!entry)
			continue;
		if (remaining == 0)
		{
			ImGui::Text("Event Entry (%s)", object.name.c_str());
			ImGui::InputTextUtf8("identifier", entry->identifier);
			ImGui::DragFloat("time(sec)", &entry->time, 0.01f, 0.0f, timeline.total_time);
			return;
		}
		--remaining;
	}
}

#endif	//	defined(USE_IMGUI)
