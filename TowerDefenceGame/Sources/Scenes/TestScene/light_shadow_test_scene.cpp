#include "scenes/TestScene/light_shadow_test_scene.h"

#if defined(USE_IMGUI)
#include "camera.h"
#include "model_instance.h"

#include "object/light_object.h"
#include "object/camera_object.h"
#include "object/model_object.h"

#include "System/console_manager.h"
#include "graphics/shadow_atlas.h"

#include <cmath>

namespace
{
	//	地面にするモデル
	constexpr std::string_view ground_model_path = "Assets/Resources/Models/バーダス・コロシアム/バーダス・コロシアム.emo";

	//	キャラクターのモデル
	constexpr std::string_view character_model_path = "Assets/Resources/Models/Kuroko/Kuroko.emo";

	//	色相から色を作る
	math::float3 hue_to_color(float hue)
	{
		const float h = (hue - std::floor(hue)) * 6.0f;
		const float r = std::fabs(h - 3.0f) - 1.0f;
		const float g = 2.0f - std::fabs(h - 2.0f);
		const float b = 2.0f - std::fabs(h - 4.0f);
		return { math::saturate(r), math::saturate(g), math::saturate(b) };
	}

	//	コンソール変数の取得
	template<class T>
	console_variable<T>* find_variable(std::string_view name)
	{
		return dynamic_cast<console_variable<T>*>(console_manager::instance().find_variable(name));
	}

	//	モデル読み込み
	std::shared_ptr<model_object> load_model(std::string_view name, std::string_view path)
	{
		model_resource_ptr resource;
		if (!model_resource_manager::instance()->load(resource, path))
			return nullptr;

		auto model = std::make_shared<model_object>(name, resource);
		model->get_model_instance()->set_flag(model_instance::flags::culling, true);
		model->get_model_instance()->set_flag(model_instance::flags::shadow_caster, true);
		return model;
	}
}

light_shadow_test_scene::light_shadow_test_scene()
: scene_base()
{
}

light_shadow_test_scene::~light_shadow_test_scene()
{
}

bool light_shadow_test_scene::initialize_core()
{
	//	メインカメラ生成
	{
		auto main_camera = std::make_shared<camera_object>("メインカメラ");
		main_camera->get_camera()->get_parameters().fov_y = math::angle::to_radian(45.0f);
		main_camera->set_world_translation(system_units::convert_m_to_unit(math::double3(0.0, 10.0, 22.0)));
		main_camera->set_world_euler(math::rotatord(0.4, math::constants<double>::pi, 0.0));
		register_object(main_camera);
		mouse::instance()->set_center_lock(false);
		mouse::instance()->set_cursol_hide(false);
	}

	//	エディターカメラ生成
	{
		editor_camera_request = editor_camera_manager::instance()->request(
			system_units::convert_m_to_unit(math::vector3d(0.0, 10.0, 22.0)),
			{ 0.4f, math::constants<float>::pi, 0 },
			0.25f);
	}

	//	平行光源生成
	{
		directional_light = std::make_shared<directional_light_object>("平行光源");
		directional_light->set_intensity(directional_intensity);
		directional_light->set_flag(light::flags::use_shadow, false);
		register_object(directional_light);
	}

	//	地面
	if (auto ground = load_model("地面", ground_model_path))
		register_object(ground);

	//	キャラクター
	animated_character = load_model("アニメーションするキャラクター", character_model_path);
	if (animated_character)
	{
		animated_character->set_world_translation(math::vector3d::zero());
		if (animated_character->get_animation_max() > 0)
			animated_character->play_animation(0, true);
		register_object(animated_character);
	}

	//	円運動させるキャラクター
	moving_character = load_model("動かすキャラクター", character_model_path);
	if (moving_character)
	{
		register_object(moving_character);
		update_character();
	}

	rebuild_lights();
	return true;
}

bool light_shadow_test_scene::uninitialize_core()
{
	lights.clear();
	editor_camera_request.reset();
	return true;
}

void light_shadow_test_scene::update_core(float elapsed_time)
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

void light_shadow_test_scene::rebuild_lights()
{
	lights.clear();

	const int count = (std::max)(settings.light_count, 0);
	const int spot_count = static_cast<int>(std::round(static_cast<float>(count) * settings.spot_ratio));
	lights.reserve(static_cast<size_t>(count));
	for (int index = 0; index < count; ++index)
	{
		//	スポットライトと点光源を交互に近い並びにする
		const bool spot = spot_count > 0 && (index * spot_count / (std::max)(count, 1)) != ((index + 1) * spot_count / (std::max)(count, 1));

		light_ptr light;
		if (!light_manager::instance()->generate_light(light, spot ? light_type::spot : light_type::point))
			continue;

		const math::float3 color = hue_to_color(static_cast<float>(index) / static_cast<float>((std::max)(count, 1)));
		light->set_color_mode(light::color_modes::color);
		const math::vector3f rec2020 = color_space::convert_gamut(math::vector3f(color.x, color.y, color.z), color_space::gamuts::rec709, color_space::gamuts::rec2020);
		light->set_color({ rec2020[0], rec2020[1], rec2020[2] });
		light->set_intensity(settings.intensity);
		light->set_range(system_units::convert_m_to_unit(settings.range));
		light->set_flag(light::flags::use_shadow, settings.shadow);
		if (spot)
		{
			light->set_inner_cone(math::angle::to_radian(settings.spot_inner_cone));
			light->set_outer_cone(math::angle::to_radian(settings.spot_outer_cone));
		}
		lights.emplace_back(light);
	}

	update_lights();
}

void light_shadow_test_scene::update_lights()
{
	const double radius = system_units::convert_m_to_unit(static_cast<double>(settings.ring_radius));
	const double height = system_units::convert_m_to_unit(static_cast<double>(settings.height));
	const double offset = static_cast<double>(light_time * motion.light_speed) * math::constants<double>::pi * 2.0;
	const size_t count = lights.size();
	for (size_t index = 0; index < count; ++index)
	{
		const double angle = offset + math::constants<double>::pi * 2.0 * static_cast<double>(index) / static_cast<double>((std::max)(count, size_t(1)));
		const math::vector3d position(std::cos(angle) * radius, height, std::sin(angle) * radius);
		lights[index]->set_position(position);

		if (lights[index]->get_light_type() == light_type::spot)
		{
			const math::vector3f direction(static_cast<float>(-position.x), static_cast<float>(-position.y), static_cast<float>(-position.z));
			lights[index]->set_rotation(math::rotatorf::from_direction(direction));
		}
	}
}

void light_shadow_test_scene::update_character()
{
	if (!moving_character)
		return;

	const double radius = system_units::convert_m_to_unit(static_cast<double>(motion.character_radius));
	const double angle = static_cast<double>(character_time * motion.character_speed) * math::constants<double>::pi * 2.0;
	moving_character->set_world_translation(math::vector3d(std::cos(angle) * radius, 0.0, std::sin(angle) * radius));
}

void light_shadow_test_scene::imgui_render_core()
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
	if (ImGui::SliderFloat("directional intensity (lx)", &directional_intensity, 0.0f, 150000.0f, "%.0f") && directional_light)
	{
		directional_light->set_intensity(directional_intensity);
	}

	//	ライト配置
	ImGui::Separator();
	ImGui::Text("lights : %d", static_cast<int>(lights.size()));
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
	if (changed)
		rebuild_lights();

	//	動き
	ImGui::Separator();
	ImGui::Checkbox("rotate lights", &motion.rotate_lights);
	ImGui::DragFloat("light speed [rev/s]", &motion.light_speed, 0.01f, -10.0f, 10.0f);
	ImGui::Checkbox("move character", &motion.move_character);
	ImGui::DragFloat("character speed [rev/s]", &motion.character_speed, 0.01f, -10.0f, 10.0f);
	if (ImGui::DragFloat("character radius [m]", &motion.character_radius, 0.1f, 0.0f, 100.0f))
		update_character();
	ImGui::Checkbox("animate character", &motion.animate_character);
}
#endif	//	defined(USE_IMGUI)
