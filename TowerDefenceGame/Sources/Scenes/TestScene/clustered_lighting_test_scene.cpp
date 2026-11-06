#include "scenes/TestScene/clustered_lighting_test_scene.h"

#if defined(USE_IMGUI)
#include "camera.h"
#include "model_instance.h"

#include "object/light_object.h"
#include "object/camera_object.h"
#include "object/model_object.h"
#include "object/object_manager.h"
#include "object/world.h"

#include "System/console_manager.h"
#include "graphics/shadow_atlas.h"

#include <cmath>
#include <algorithm>
#include <cstdint>
#include <random>
#include <vector>

namespace
{
	//	地面にするモデル
	constexpr std::string_view ground_model_path = "Assets/Resources/Models/バーダス・コロシアム/バーダス・コロシアム.emo";

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
}

clustered_lighting_test_scene::clustered_lighting_test_scene()
: scene_base()
{
}

clustered_lighting_test_scene::~clustered_lighting_test_scene()
{
}

bool clustered_lighting_test_scene::initialize_core()
{
	//	メインカメラ生成
	{
		auto main_camera = std::make_shared<camera_object>("メインカメラ");
		main_camera->get_camera()->get_parameters().fov_y = math::angle::to_radian(45.0f);
		main_camera->set_world_translation(system_units::convert_m_to_unit(math::double3(0.0, 15.0, 45.0)));
		main_camera->set_world_euler(math::rotatord(0.35, math::constants<double>::pi, 0.0));
		register_object(main_camera);
		mouse::instance()->set_center_lock(false);
		mouse::instance()->set_cursol_hide(false);
	}

	//	エディターカメラ生成
	{
		editor_camera_request = editor_camera_manager::instance()->request(
			system_units::convert_m_to_unit(math::vector3d(0.0, 15.0, 45.0)),
			{ 0.35f, math::constants<float>::pi, 0 },
			0.25f);
	}

	//	平行光源生成
	//	点光源を見やすくするため暗め、影無し
	{
		directional_light = std::make_shared<directional_light_object>("平行光源");
		directional_light->set_intensity(directional_intensity);
		directional_light->set_flag(light::flags::use_shadow, false);
		register_object(directional_light);
	}

	//	地面
	{
		model_resource_ptr resource;
		if (model_resource_manager::instance()->load(resource, ground_model_path))
		{
			auto ground = std::make_shared<model_object>("地面", resource);
			ground->get_model_instance()->set_flag(model_instance::flags::culling, true);
			register_object(ground);
		}
	}

	rebuild_lights();
	return true;
}

bool clustered_lighting_test_scene::uninitialize_core()
{
	//	ライトは参照を外すとライト管理からも外れる
	clear_lights();
	editor_camera_request.reset();
	return true;
}

void clustered_lighting_test_scene::update_core(float elapsed_time)
{
	if (animation.enable)
	{
		animation_time += elapsed_time;
		update_light_positions();
	}
}

void clustered_lighting_test_scene::clear_lights()
{
	for (auto& entry : lights)
		object_manager::instance()->unregister_object(entry.object);
	lights.clear();
}

void clustered_lighting_test_scene::rebuild_lights()
{
	clear_lights();

	std::seed_seq seed_sequence{ static_cast<std::uint32_t>(settings.seed), 0x9e3779b9u, 0x85ebca6bu, 0xc2b2ae35u };
	std::mt19937 random(seed_sequence);
	std::uniform_real_distribution<double> random_01(0.0, 1.0);

	const int count = (std::max)(settings.light_count, 0);

	//	格子に分けて各マスに1つずつ置く
	const int grid = (std::max)(1, static_cast<int>(std::ceil(std::sqrt(static_cast<double>((std::max)(count, 1))))));
	std::vector<int> cell_order(static_cast<size_t>(grid * grid));
	for (int index = 0; index < static_cast<int>(cell_order.size()); ++index)
		cell_order[static_cast<size_t>(index)] = index;
	std::shuffle(cell_order.begin(), cell_order.end(), random);

	lights.reserve(static_cast<size_t>(count));
	for (int index = 0; index < count; ++index)
	{
		const bool spot = random_01(random) < settings.spot_ratio;
		const bool shadow = random_01(random) < settings.shadow_ratio;

		light_entry entry;
		const std::string name = std::string(spot ? "spot_light_" : "point_light_") + std::to_string(index);
		if (spot)
			entry.object = std::make_shared<spot_light_object>(name);
		else
			entry.object = std::make_shared<point_light_object>(name);

		//	配置範囲内にばらまく
		double u = random_01(random);
		double v = random_01(random);
		if (settings.stratified)
		{
			const int cell = cell_order[static_cast<size_t>(index)];
			u = (static_cast<double>(cell % grid) + u) / static_cast<double>(grid);
			v = (static_cast<double>(cell / grid) + v) / static_cast<double>(grid);
		}
		const double x = (u - 0.5) * settings.area_size;
		const double z = (v - 0.5) * settings.area_size;
		entry.base_position = system_units::convert_m_to_unit(math::vector3d(x, settings.height, z));
		entry.phase = static_cast<float>(random_01(random)) * math::constants<float>::pi * 2.0f;

		const math::float3 color = hue_to_color(static_cast<float>(random_01(random)));
		const light_ptr target = entry.object->get_light();

		if (!spot)
		{
			const ::light defaults(light_type::point);
			for (UINT bias_index = 0; bias_index < common_pipeline_states::shadow_map_max; ++bias_index)
				target->set_shadow_bias(static_cast<int>(bias_index), defaults.get_shadow_bias(static_cast<int>(bias_index)));
		}
		target->set_color_mode(light::color_modes::color);
		const math::vector3f rec2020 = color_space::convert_gamut(math::vector3f(color.x, color.y, color.z), color_space::gamuts::rec709, color_space::gamuts::rec2020);
		target->set_color({ rec2020[0], rec2020[1], rec2020[2] });
		target->set_intensity(settings.intensity);
		target->set_range(system_units::convert_m_to_unit(settings.range));
		target->set_flag(light::flags::use_shadow, shadow);
		if (spot)
		{
			//	真下に向ける
			entry.object->set_local_euler(math::rotatord(math::constants<double>::pi * 0.5, 0.0, 0.0));
			target->set_inner_cone(math::angle::to_radian(settings.spot_inner_cone));
			target->set_outer_cone(math::angle::to_radian(settings.spot_outer_cone));
		}
		register_object(entry.object);
		lights.emplace_back(std::move(entry));
	}

	update_light_positions();
}

void clustered_lighting_test_scene::update_light_positions()
{
	const double radius = system_units::convert_m_to_unit(static_cast<double>(animation.radius));
	const float angle_base = animation_time * animation.speed * math::constants<float>::pi * 2.0f;
	for (auto& entry : lights)
	{
		const double angle = static_cast<double>(angle_base + entry.phase);
		const math::vector3d offset(std::cos(angle) * radius, 0.0, std::sin(angle) * radius);
		entry.object->set_local_translation(entry.base_position + offset);
	}
}

void clustered_lighting_test_scene::imgui_render_core()
{
	if (!ImGui::CollapsingHeader("clustered lighting test", ImGuiTreeNodeFlags_DefaultOpen))
		return;

	//	クラスタードライティングの設定
	if (auto enable = find_variable<bool>("r.clustered.enable"))
	{
		bool value = enable->get();
		if (ImGui::Checkbox("clustered lighting (r.clustered.enable)", &value))
			enable->set(value);
	}
	if (auto far_distance = find_variable<float>("r.clustered.far"))
	{
		float value = system_units::convert_unit_to_m(far_distance->get());
		if (ImGui::DragFloat("cluster far [m] (r.clustered.far)", &value, 1.0f, 1.0f, 100000.0f))
			far_distance->set(system_units::convert_m_to_unit(value));
	}
	if (auto depth_culling = find_variable<bool>("r.clustered.depth_culling"))
	{
		bool value = depth_culling->get();
		if (ImGui::Checkbox("depth culling (r.clustered.depth_culling)", &value))
			depth_culling->set(value);
	}

	//	ライト数のヒートマップ表示
	if (auto visualize = find_variable<bool>("r.clustered.visualize"))
	{
		bool value = visualize->get();
		if (ImGui::Checkbox("heatmap (r.clustered.visualize)", &value))
			visualize->set(value);
	}
	if (auto visualize_max = find_variable<float>("r.clustered.visualize_max"))
	{
		float value = visualize_max->get();
		if (ImGui::DragFloat("heatmap max lights (r.clustered.visualize_max)", &value, 0.5f, 1.0f, 4096.0f))
			visualize_max->set(value);
		ImGui::TextDisabled("black : 0, blue -> green -> yellow -> red : max");
	}

	//	ライトの LOD と空間分割
	ImGui::Separator();
	{
		const auto& statistics = world::get_main_world()->get_light_collector().get_statistics();
		ImGui::Text("light collector : total %u / candidates %u / visible %u / cells %u",
			statistics.total_lights, statistics.candidate_lights, statistics.visible_lights, statistics.occupied_cells);
	}
	auto drag_meter = [](const char* label, std::string_view name, float speed)
	{
		if (auto variable = find_variable<float>(name))
		{
			float value = system_units::convert_unit_to_m(variable->get());
			if (ImGui::DragFloat(label, &value, speed, 0.0f, 100000.0f))
				variable->set(system_units::convert_m_to_unit(value));
		}
	};
	drag_meter("light max draw distance [m] (r.light.max_draw_distance)", "r.light.max_draw_distance", 1.0f);
	drag_meter("light grid cell size [m] (r.light.grid_cell_size)", "r.light.grid_cell_size", 0.5f);
	if (auto variable = find_variable<float>("r.light.fade_ratio"))
	{
		float value = variable->get();
		if (ImGui::SliderFloat("light fade ratio (r.light.fade_ratio)", &value, 0.0f, 1.0f))
			variable->set(value);
	}
	if (auto variable = find_variable<float>("r.light.min_screen_size"))
	{
		float value = variable->get();
		if (ImGui::DragFloat("light min screen size (r.light.min_screen_size)", &value, 0.0001f, 0.0f, 1.0f, "%.4f"))
			variable->set(value);
	}

	//	シャドウマップアトラス
	if (auto variable = find_variable<bool>("r.shadow.atlas.enable"))
	{
		bool value = variable->get();
		if (ImGui::Checkbox("shadow atlas (r.shadow.atlas.enable)", &value))
			variable->set(value);
	}
	auto drag_int = [](const char* label, std::string_view name, int minimum, int maximum)
	{
		if (auto variable = find_variable<int>(name))
		{
			int value = variable->get();
			if (ImGui::DragInt(label, &value, 1.0f, minimum, maximum))
				variable->set(value);
		}
	};
	drag_int("shadow atlas resolution (r.shadow.atlas.resolution)", "r.shadow.atlas.resolution", 64, 4096);
	drag_int("shadow atlas slices (r.shadow.atlas.slices)", "r.shadow.atlas.slices", 6, 2048);
	drag_int("shadow atlas max lights (r.shadow.atlas.max_lights)", "r.shadow.atlas.max_lights", 0, 2048);
	if (auto variable = find_variable<bool>("r.shadow.atlas.cache"))
	{
		bool value = variable->get();
		if (ImGui::Checkbox("shadow cache (r.shadow.atlas.cache)", &value))
			variable->set(value);
	}
	drag_int("shadow cache keep frames (r.shadow.atlas.keep_frames)", "r.shadow.atlas.keep_frames", 0, 100000);
	{
		const auto& statistics = shadow_atlas::get_last_statistics();
		ImGui::Text("shadow atlas : lights %u (rendered %u) / cached %u / slices %u / %u",
			statistics.allocated_lights, statistics.rendered_lights, statistics.cached_lights, statistics.used_slices, statistics.slice_count);
	}

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
	changed |= ImGui::DragInt("light count", &settings.light_count, 4.0f, 0, 65536);
	changed |= ImGui::SliderFloat("spot ratio", &settings.spot_ratio, 0.0f, 1.0f);
	changed |= ImGui::SliderFloat("shadow ratio", &settings.shadow_ratio, 0.0f, 1.0f);
	changed |= ImGui::Checkbox("stratified placement", &settings.stratified);
	changed |= ImGui::DragFloat("area size [m]", &settings.area_size, 0.5f, 1.0f, 10000.0f);
	changed |= ImGui::DragFloat("height [m]", &settings.height, 0.1f, -100.0f, 1000.0f);
	changed |= ImGui::DragFloat("range [m]", &settings.range, 0.1f, 0.1f, 1000.0f);
	changed |= ImGui::DragFloat("intensity (lm)", &settings.intensity, 10.0f, 0.0f, 100000.0f, "%.0f");
	changed |= ImGui::SliderFloat("spot inner cone [deg]", &settings.spot_inner_cone, 0.0f, 89.0f);
	changed |= ImGui::SliderFloat("spot outer cone [deg]", &settings.spot_outer_cone, 0.0f, 89.0f);
	changed |= ImGui::DragInt("seed", &settings.seed);
	settings.spot_inner_cone = (std::min)(settings.spot_inner_cone, settings.spot_outer_cone);
	if (changed)
		rebuild_lights();

	//	ライトの動き
	ImGui::Separator();
	ImGui::Checkbox("animate", &animation.enable);
	ImGui::DragFloat("animate speed [rev/s]", &animation.speed, 0.01f, -10.0f, 10.0f);
	if (ImGui::DragFloat("animate radius [m]", &animation.radius, 0.1f, 0.0f, 100.0f))
		update_light_positions();
}
#endif	//	defined(USE_IMGUI)
