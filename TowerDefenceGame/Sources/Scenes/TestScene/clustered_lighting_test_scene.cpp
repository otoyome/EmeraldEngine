#include "scenes/TestScene/clustered_lighting_test_scene.h"

#if defined(USE_IMGUI)
#include "camera.h"
#include "model_instance.h"

#include "object/light_object.h"
#include "object/camera_object.h"
#include "object/model_object.h"

#include "System/console_manager.h"

#include <cmath>
#include <random>

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
		auto color = directional_light->get_color();
		color.w = directional_intensity;
		directional_light->set_color(color);
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
	lights.clear();
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

void clustered_lighting_test_scene::rebuild_lights()
{
	lights.clear();

	std::mt19937 random(static_cast<std::mt19937::result_type>(settings.seed));
	std::uniform_real_distribution<float> random_01(0.0f, 1.0f);

	const int count = (std::max)(settings.light_count, 0);
	lights.reserve(static_cast<size_t>(count));
	for (int index = 0; index < count; ++index)
	{
		const bool spot = random_01(random) < settings.spot_ratio;

		light_entry entry;
		if (!light_manager::instance()->generate_light(entry.light, spot ? light_type::spot : light_type::point))
			continue;

		//	配置範囲内にばらまく
		const double x = (random_01(random) - 0.5) * settings.area_size;
		const double z = (random_01(random) - 0.5) * settings.area_size;
		entry.base_position = system_units::convert_m_to_unit(math::vector3d(x, settings.height, z));
		entry.phase = random_01(random) * math::constants<float>::pi * 2.0f;

		const math::float3 color = hue_to_color(random_01(random));
		entry.light->set_color({ color.x, color.y, color.z, settings.intensity });
		entry.light->set_range(system_units::convert_m_to_unit(settings.range));
		entry.light->set_flag(light::flags::use_shadow, false);
		if (spot)
		{
			//	真下に向ける
			entry.light->set_rotation(math::rotatorf(math::constants<float>::pi * 0.5f, 0.0f, 0.0f));
			entry.light->set_inner_cone(math::angle::to_radian(settings.spot_inner_cone));
			entry.light->set_outer_cone(math::angle::to_radian(settings.spot_outer_cone));
		}
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
		entry.light->set_position(entry.base_position + offset);
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

	//	平行光源
	if (ImGui::SliderFloat("directional intensity", &directional_intensity, 0.0f, 2.0f) && directional_light)
	{
		auto color = directional_light->get_color();
		color.w = directional_intensity;
		directional_light->set_color(color);
	}

	//	ライト配置
	ImGui::Separator();
	ImGui::Text("lights : %d", static_cast<int>(lights.size()));
	bool changed = false;
	changed |= ImGui::DragInt("light count", &settings.light_count, 4.0f, 0, 65536);
	changed |= ImGui::SliderFloat("spot ratio", &settings.spot_ratio, 0.0f, 1.0f);
	changed |= ImGui::DragFloat("area size [m]", &settings.area_size, 0.5f, 1.0f, 10000.0f);
	changed |= ImGui::DragFloat("height [m]", &settings.height, 0.1f, -100.0f, 1000.0f);
	changed |= ImGui::DragFloat("range [m]", &settings.range, 0.1f, 0.1f, 1000.0f);
	changed |= ImGui::DragFloat("intensity", &settings.intensity, 0.1f, 0.0f, 1000.0f);
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
