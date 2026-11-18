#include "SceneSystems/Test/light_shadow_test_system.h"

#include "Graphics/View/camera.h"
#include "Model/model_instance.h"
#include "object/light_object.h"
#include "object/model_object.h"
#include "object/object_manager.h"
#include "Object/game_object_serializer.h"
#include "System/console_manager.h"
#include "Graphics/Lighting/shadow_atlas.h"

#include "cereal/cereal.hpp"
#include "cereal/archives/json.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace
{
	//	色相から色を作る
	math::float3 hue_to_color(float hue)
	{
		const float h = (hue - std::floor(hue)) * 6.0f;
		const float r = std::fabs(h - 3.0f) - 1.0f;
		const float g = 2.0f - std::fabs(h - 2.0f);
		const float b = 2.0f - std::fabs(h - 4.0f);
		return { math::saturate(r), math::saturate(g), math::saturate(b) };
	}

	//	原点を向く回転
	math::rotatord look_at_origin(const math::vector3d& position)
	{
		const math::vector3f direction(static_cast<float>(-position.x), static_cast<float>(-position.y), static_cast<float>(-position.z));
		return math::rotatord(math::rotatorf::from_direction(direction));
	}

#if defined(USE_IMGUI)
	//	コンソール変数の取得
	template<class T>
	console_variable<T>* find_variable(std::string_view name)
	{
		return dynamic_cast<console_variable<T>*>(console_manager::instance().find_variable(name));
	}
#endif	//	defined(USE_IMGUI)
}

light_shadow_test_system::light_shadow_test_system()
{
}

light_shadow_test_system::~light_shadow_test_system()
{
}

void light_shadow_test_system::on_begin()
{
	mouse::instance()->set_center_lock(false);
	mouse::instance()->set_cursol_hide(false);

	collect_scene_lights();

	if (animated_character && animated_character->get_animation_max() > 0)
		animated_character->play_animation(0, true);
	if (moving_character)
		moving_character_position = moving_character->get_world().get_translation();
}

void light_shadow_test_system::on_end()
{
	clear_generated_lights();
	restore_scene_lights();
	scene_lights.clear();
	if (moving_character)
		moving_character->set_world_translation(moving_character_position);
}

void light_shadow_test_system::pre_update(float elapsed_time)
{
	if (motion.rotate_lights)
	{
		light_time += elapsed_time;
		update_lights();
	}
	if (motion.move_character)
	{
		character_time += elapsed_time;
		update_character();
	}
	if (animated_character)
		animated_character->set_animation_speed(motion.animate_character ? 1.0f : 0.0f);
}

void light_shadow_test_system::serialize(cereal::JSONOutputArchive& archive) const
{
	archive(cereal::make_nvp("light_count", settings.light_count),
		cereal::make_nvp("spot_ratio", settings.spot_ratio),
		cereal::make_nvp("ring_radius", settings.ring_radius),
		cereal::make_nvp("height", settings.height),
		cereal::make_nvp("range", settings.range),
		cereal::make_nvp("intensity", settings.intensity),
		cereal::make_nvp("spot_inner_cone", settings.spot_inner_cone),
		cereal::make_nvp("spot_outer_cone", settings.spot_outer_cone),
		cereal::make_nvp("shadow", settings.shadow),
		cereal::make_nvp("rotate_lights", motion.rotate_lights),
		cereal::make_nvp("light_speed", motion.light_speed),
		cereal::make_nvp("move_character", motion.move_character),
		cereal::make_nvp("character_speed", motion.character_speed),
		cereal::make_nvp("character_radius", motion.character_radius),
		cereal::make_nvp("animate_character", motion.animate_character));
	game_object_serializer::save_object_reference(archive, "directional_light", directional_light);
	game_object_serializer::save_object_reference(archive, "animated_character", animated_character);
	game_object_serializer::save_object_reference(archive, "moving_character", moving_character);
}

void light_shadow_test_system::deserialize(cereal::JSONInputArchive& archive)
{
	archive(cereal::make_nvp("light_count", settings.light_count),
		cereal::make_nvp("spot_ratio", settings.spot_ratio),
		cereal::make_nvp("ring_radius", settings.ring_radius),
		cereal::make_nvp("height", settings.height),
		cereal::make_nvp("range", settings.range),
		cereal::make_nvp("intensity", settings.intensity),
		cereal::make_nvp("spot_inner_cone", settings.spot_inner_cone),
		cereal::make_nvp("spot_outer_cone", settings.spot_outer_cone),
		cereal::make_nvp("shadow", settings.shadow),
		cereal::make_nvp("rotate_lights", motion.rotate_lights),
		cereal::make_nvp("light_speed", motion.light_speed),
		cereal::make_nvp("move_character", motion.move_character),
		cereal::make_nvp("character_speed", motion.character_speed),
		cereal::make_nvp("character_radius", motion.character_radius),
		cereal::make_nvp("animate_character", motion.animate_character));
	game_object_serializer::load_object_reference(archive, "directional_light", directional_light);
	game_object_serializer::load_object_reference(archive, "animated_character", animated_character);
	game_object_serializer::load_object_reference(archive, "moving_character", moving_character);
}

std::vector<std::shared_ptr<light_object>> light_shadow_test_system::create_ring_lights(const light_settings& settings)
{
	std::vector<std::shared_ptr<light_object>> lights;
	const int count = (std::max)(settings.light_count, 0);
	const int spot_count = static_cast<int>(std::round(static_cast<float>(count) * settings.spot_ratio));
	const double radius = system_units::convert_m_to_unit(static_cast<double>(settings.ring_radius));
	const double height = system_units::convert_m_to_unit(static_cast<double>(settings.height));
	lights.reserve(static_cast<size_t>(count));
	for (int index = 0; index < count; ++index)
	{
		//	スポットライトと点光源を交互に近い並びにする
		const bool spot = spot_count > 0 && (index * spot_count / (std::max)(count, 1)) != ((index + 1) * spot_count / (std::max)(count, 1));

		std::shared_ptr<light_object> object;
		const std::string name = std::string(spot ? "spot_light_" : "point_light_") + std::to_string(index);
		if (spot)
			object = std::make_shared<spot_light_object>(name);
		else
			object = std::make_shared<point_light_object>(name);
		const light_ptr target = object->get_light();

		if (!spot)
		{
			const ::light defaults(light_type::point);
			for (UINT bias_index = 0; bias_index < common_pipeline_states::shadow_map_max; ++bias_index)
				target->set_shadow_bias(static_cast<int>(bias_index), defaults.get_shadow_bias(static_cast<int>(bias_index)));
		}

		const math::float3 color = hue_to_color(static_cast<float>(index) / static_cast<float>((std::max)(count, 1)));
		target->set_color_mode(light::color_modes::color);
		const math::vector3f rendering_color = color_space::convert_gamut(math::vector3f(color.x, color.y, color.z), color_space::gamuts::rec709, color_space::rendering_gamut);
		target->set_color({ rendering_color[0], rendering_color[1], rendering_color[2] });
		target->set_intensity(settings.intensity);
		target->set_range(system_units::convert_m_to_unit(settings.range));
		target->set_flag(light::flags::use_shadow, settings.shadow);
		if (spot)
		{
			target->set_inner_cone(math::angle::to_radian(settings.spot_inner_cone));
			target->set_outer_cone(math::angle::to_radian(settings.spot_outer_cone));
		}

		//	円周上に並べて、スポットライトは中心へ向ける
		const double angle = math::constants<double>::pi * 2.0 * static_cast<double>(index) / static_cast<double>((std::max)(count, 1));
		const math::vector3d position(std::cos(angle) * radius, height, std::sin(angle) * radius);
		object->set_local_translation(position);
		if (spot)
			object->set_local_euler(look_at_origin(position));

		lights.emplace_back(std::move(object));
	}
	return lights;
}

void light_shadow_test_system::collect_scene_lights()
{
	scene_lights.clear();
	for (const game_object_ptr& object : get_scene_objects())
	{
		if (!object->check_type(point_light_object::type_name()) && !object->check_type(spot_light_object::type_name()))
			continue;

		light_entry& entry = scene_lights.emplace_back();
		entry.object = std::static_pointer_cast<light_object>(object);
		entry.base_position = entry.object->get_local().get_translation();
		entry.active = entry.object->is_active();
	}
}

void light_shadow_test_system::restore_scene_lights()
{
	for (light_entry& entry : scene_lights)
	{
		entry.object->set_local_translation(entry.base_position);
		if (entry.object->get_light()->get_light_type() == light_type::spot)
			entry.object->set_local_euler(look_at_origin(entry.base_position));
		entry.object->set_active(entry.active);
	}
}

void light_shadow_test_system::set_scene_lights_active(bool active)
{
	for (light_entry& entry : scene_lights)
		entry.object->set_active(entry.active && active);
}

void light_shadow_test_system::regenerate_lights()
{
	clear_generated_lights();
	set_scene_lights_active(false);

	for (auto& object : create_ring_lights(settings))
	{
		light_entry& entry = generated_lights.emplace_back();
		entry.object = object;
		entry.base_position = object->get_local().get_translation();
		register_object(object);
	}
	update_lights();
}

void light_shadow_test_system::clear_generated_lights()
{
	if (generated_lights.empty())
		return;

	//	ライトは参照を外すとライト管理からも外れる
	for (auto& entry : generated_lights)
		unregister_object(entry.object);
	generated_lights.clear();
	set_scene_lights_active(true);
}

std::vector<light_shadow_test_system::light_entry>& light_shadow_test_system::get_current_lights()
{
	return generated_lights.empty() ? scene_lights : generated_lights;
}

void light_shadow_test_system::update_lights()
{
	//	Y軸まわりに回す
	const double offset = static_cast<double>(light_time * motion.light_speed) * math::constants<double>::pi * 2.0;
	const double c = std::cos(offset);
	const double s = std::sin(offset);
	for (auto& entry : get_current_lights())
	{
		const math::vector3d& base = entry.base_position;
		const math::vector3d position(base.x * c - base.z * s, base.y, base.x * s + base.z * c);
		entry.object->set_local_translation(position);
		if (entry.object->get_light()->get_light_type() == light_type::spot)
			entry.object->set_local_euler(look_at_origin(position));
	}
}

void light_shadow_test_system::update_character()
{
	if (!moving_character)
		return;

	const double radius = system_units::convert_m_to_unit(static_cast<double>(motion.character_radius));
	const double angle = static_cast<double>(character_time * motion.character_speed) * math::constants<double>::pi * 2.0;
	moving_character->set_world_translation(math::vector3d(std::cos(angle) * radius, 0.0, std::sin(angle) * radius));
}

#if defined(USE_IMGUI)
void light_shadow_test_system::imgui_render()
{
	if (!ImGui::CollapsingHeader("light shadow test", ImGuiTreeNodeFlags_DefaultOpen))
		return;

	//	影のアトラスと統計
	{
		const auto& statistics = shadow_atlas::get_last_statistics();
		ImGui::Text("shadow atlas : lights %u (rendered %u) / cached %u / slices %u / %u",
			statistics.allocated_lights, statistics.rendered_lights, statistics.cached_lights, statistics.used_slices, statistics.slice_count);
		ImGui::TextDisabled("rendered = 0 : cached (no redraw)");
	}
	auto checkbox = [](const char* label, std::string_view name)
	{
		if (auto variable = find_variable<bool>(name))
		{
			bool value = variable->get();
			if (ImGui::Checkbox(label, &value))
				variable->set(value);
		}
	};
	auto drag_int = [](const char* label, std::string_view name, int minimum, int maximum)
	{
		if (auto variable = find_variable<int>(name))
		{
			int value = variable->get();
			if (ImGui::DragInt(label, &value, 1.0f, minimum, maximum))
				variable->set(value);
		}
	};
	checkbox("shadow atlas (r.shadow.atlas.enable)", "r.shadow.atlas.enable");
	checkbox("shadow cache (r.shadow.atlas.cache)", "r.shadow.atlas.cache");
	drag_int("shadow atlas resolution (r.shadow.atlas.resolution)", "r.shadow.atlas.resolution", 64, 4096);
	drag_int("shadow atlas slices (r.shadow.atlas.slices)", "r.shadow.atlas.slices", 6, 2048);
	drag_int("shadow atlas max lights (r.shadow.atlas.max_lights)", "r.shadow.atlas.max_lights", 0, 2048);
	drag_int("shadow cache keep frames (r.shadow.atlas.keep_frames)", "r.shadow.atlas.keep_frames", 0, 100000);
	checkbox("clustered lighting (r.clustered.enable)", "r.clustered.enable");
	ImGui::TextDisabled("lights casting shadows are not clustered");

	//	平行光源
	ImGui::Separator();
	if (directional_light)
	{
		float directional_intensity = directional_light->get_intensity();
		if (ImGui::SliderFloat("directional intensity (lx)", &directional_intensity, 0.0f, 150000.0f, "%.0f"))
			directional_light->set_intensity(directional_intensity);
	}

	//	ライト配置
	ImGui::Separator();
	if (generated_lights.empty())
		ImGui::Text("lights : %d (scene)", static_cast<int>(scene_lights.size()));
	else
		ImGui::Text("lights : %d (generated)", static_cast<int>(generated_lights.size()));
	bool changed = false;
	changed |= ImGui::DragInt("light count", &settings.light_count, 0.2f, 0, 256);
	changed |= ImGui::SliderFloat("spot ratio", &settings.spot_ratio, 0.0f, 1.0f);
	changed |= ImGui::Checkbox("cast shadow", &settings.shadow);
	changed |= ImGui::DragFloat("ring radius [m]", &settings.ring_radius, 0.1f, 0.0f, 1000.0f);
	changed |= ImGui::DragFloat("height [m]", &settings.height, 0.1f, -100.0f, 1000.0f);
	changed |= ImGui::DragFloat("range [m]", &settings.range, 0.1f, 0.1f, 1000.0f);
	changed |= ImGui::DragFloat("intensity (lm)", &settings.intensity, 10.0f, 0.0f, 100000.0f, "%.0f");
	changed |= ImGui::SliderFloat("spot inner cone [deg]", &settings.spot_inner_cone, 0.0f, 89.0f);
	changed |= ImGui::SliderFloat("spot outer cone [deg]", &settings.spot_outer_cone, 0.0f, 89.0f);
	settings.spot_inner_cone = (std::min)(settings.spot_inner_cone, settings.spot_outer_cone);

	//	作り直しは再生中だけ
	if (changed && !is_editing())
		regenerate_lights();
	ImGui::BeginDisabled(is_editing());
	if (ImGui::Button("regenerate"))
		regenerate_lights();
	ImGui::SameLine();
	ImGui::BeginDisabled(generated_lights.empty());
	if (ImGui::Button("use scene lights"))
	{
		clear_generated_lights();
		update_lights();
	}
	ImGui::EndDisabled();
	ImGui::EndDisabled();

	//	動き
	ImGui::Separator();
	ImGui::Checkbox("rotate lights", &motion.rotate_lights);
	ImGui::DragFloat("light speed [rev/s]", &motion.light_speed, 0.01f, -10.0f, 10.0f);
	ImGui::Checkbox("move character", &motion.move_character);
	ImGui::DragFloat("character speed [rev/s]", &motion.character_speed, 0.01f, -10.0f, 10.0f);
	if (ImGui::DragFloat("character radius [m]", &motion.character_radius, 0.1f, 0.0f, 100.0f) && !is_editing())
		update_character();
	ImGui::Checkbox("animate character", &motion.animate_character);
}
#endif	//	defined(USE_IMGUI)

SCENE_SYSTEM_REGISTER(light_shadow_test_system);
