#include "object/gameobjects/MiniGame/minigame000_roomba_camera_controller_object.h"

#include "Graphics/View/camera.h"
#include "object/camera_object.h"
#include "object/gameobjects/MiniGame/minigame000_roomba_object.h"

namespace
{
	//	±ピッチ制限角度
	constexpr math::double2 pitch_limit_angle = { math::angle::to_radian(+5.0), math::angle::to_radian(45.0) };
	//	開始距離
	constexpr double default_distance = system_units::convert_m_to_unit(7.0);
	//	角度によるフォーカス位置のオフセット
	constexpr double focus_y_min_offset = -system_units::convert_m_to_unit(0.3);
	constexpr double focus_y_max_offset = +system_units::convert_m_to_unit(0.3);
	//	フォーカス追従制限
	constexpr double focus_limit_range_max = system_units::convert_m_to_unit(2.0); // 基本フォーカス
	//	ダッシュ中の視野角制御
	constexpr float fov_follow_rate = 6.0f;
	constexpr math::float2 fov_range = { math::angle::to_radian(30.0f), math::angle::to_radian(60.0f) };
	constexpr math::easing::type fov_ease = math::easing::type::lerp;
	//	壁めり込み防止の球の半径
	constexpr float collision_radius = system_units::convert_m_to_unit(0.2f);
	//	マウス1カウントあたりの旋回角度(度)
	constexpr float mouse_sensitivity = 0.1f;
	//	右スティックはマウスの移動量に換算する(倒し切りで毎秒 120 度)
	constexpr float stick_move_per_second = 1200.0f;
}

minigame000::roomba_camera_controller_object::roomba_camera_controller_object(std::string_view name)
: base(name)
, target()
, camera()

, state(states::free)
, input_purmit_flag(true)

, minimum_distance(system_units::convert_m_to_unit(2.0f))
, current_distance(static_cast<float>(default_distance))
, maximum_distance(system_units::convert_m_to_unit(10.0f))

, disengage_direction(0, 0, 0)
, update_disengage_direction_rate(0.2f)

, focus(0.0, 0.0, 0.0)
, focus_distance(static_cast<float>(default_distance))
, focus_limit_range(static_cast<float>(focus_limit_range_max))
, update_focus_rate_inner(0.05f)

, update_fov_rate_inner(0.0f)
{
}

minigame000::roomba_camera_controller_object::~roomba_camera_controller_object()
{
}

void minigame000::roomba_camera_controller_object::set_target(const std::shared_ptr<roomba_object>& value)
{
	target = value;
	if (target)
		focus = target->get_focus_point();
}

void minigame000::roomba_camera_controller_object::fixed_update(float elapsed_time)
{
	base::fixed_update(elapsed_time);

	if (!target || !camera)
		return;

	switch (state)
	{
		case states::entry:
		{
			fixed_update_entry(elapsed_time);
			break;
		}
		default:
		{
			fixed_update_default(elapsed_time);
			break;
		}
	}
}

void minigame000::roomba_camera_controller_object::fixed_update_entry(float elapsed_time)
{
	//	旋回処理
	{
		//	とりあえず旋回処理をさせる
		disengage_direction.x = -math::constants<double>::pi / 8.0;
		disengage_direction.y += ((math::constants<double>::pi * 0.25)) * elapsed_time;

		auto camera_rotation = camera->get_world_rotation();
		camera_rotation = math::slerp(camera_rotation, disengage_direction.to_quaternion(), update_disengage_direction_rate);

		auto camera_rotator = math::rotatord(camera_rotation);
		camera_rotator.z = 0.0f;
		camera_rotator.x = math::clamp(camera_rotator.x, pitch_limit_angle.x, pitch_limit_angle.y);
		camera_rotator.y = math::angle::normalize(camera_rotator.y);
		camera->set_world_euler(camera_rotator);
	}

	//	カメラの姿勢の更新
	{
		auto camera_rotation = camera->get_world_rotation();
		auto camera_front = camera_rotation.front();

		double offset = system_units::convert_m_to_unit(1.0);
		double dist = system_units::convert_m_to_unit(3.0) - offset;
		auto camera_focus = focus - camera_front * offset;
		auto camera_position = camera_focus - camera_front * dist;
		camera->set_world_translation(camera_position);
	}
}

void minigame000::roomba_camera_controller_object::fixed_update_default(float elapsed_time)
{
	//	フォーカスポイント算出
	math::vector3d new_focus = target->get_focus_point();

	float update_focus_rate = update_focus_rate_inner;

	//	フォーカス距離算出
	focus_distance = math::clamp(current_distance, minimum_distance, maximum_distance);

	//	フォーカス更新
	{
		float focus_rate = update_focus_rate;
		{
			//	一時座標算出
			auto temp_focus = focus + (new_focus - focus) * update_focus_rate;

			//	距離制限
			auto focus_vector = new_focus - temp_focus;
			double length_sq = focus_vector.square_length();

			focus_rate = math::lerp(focus_rate, 1.0f, static_cast<float>(math::saturate(length_sq / (focus_limit_range * focus_limit_range))));
		}
		focus += (new_focus - focus) * focus_rate;
	}

	//	旋回処理
	{
		auto camera_rotation = camera->get_world_rotation();
		camera_rotation = math::slerp(camera_rotation, disengage_direction.to_quaternion(), update_disengage_direction_rate);

		auto camera_rotator = math::rotatord(camera_rotation);
		camera_rotator.z = 0.0f;
		camera_rotator.x = math::clamp(camera_rotator.x, pitch_limit_angle.x, pitch_limit_angle.y);
		camera_rotator.y = math::angle::normalize(camera_rotator.y);
		camera->set_world_rotation(camera_rotator);
	}

	//	カメラの姿勢の更新
	{
		auto camera_rotation = camera->get_world_euler();
		auto camera_front = camera_rotation.front();
		auto camera_position = focus - camera_front * focus_distance;
		double min_y_offset = math::linearstep(0.0, pitch_limit_angle.x, camera_rotation.x);
		double max_y_offset = math::linearstep(0.0, pitch_limit_angle.y, camera_rotation.x);
		camera_position.y += min_y_offset * focus_y_min_offset + max_y_offset * focus_y_max_offset;

		//	物体と当たり判定
		{
			collision_world::sphere_cast_descs desc;
			desc.filter = collision_filter_manager::instance()->search_filter("camera");
			desc.start = focus.to_double3();
			desc.end = camera_position.to_double3();
			desc.radius = collision_radius;
			for (auto collision : target->get_collision_object()->get_collisions())
				desc.exclude_objects.emplace_back(collision);

			//	ヒット処理
			collision::intersect_data intersect_data;
			collision_object_ptr hit_collision_object;
			if (get_context().get_objects().get_world().sphere_cast(desc, intersect_data, hit_collision_object))
			{
				float length = static_cast<float>(intersect_data[0].length);
				auto vector = (camera_position - focus).normalized();
				camera_position = focus + vector * length;
			}
		}
		camera->set_world_euler(camera_rotation);
		camera->set_world_translation(camera_position);
	}

	//	視野角制御
	{
		auto velocity = target->get_velocity();
		auto speed = sqrtf(velocity.x * velocity.x + velocity.y * velocity.y + velocity.z * velocity.z);
		auto max_speed = target->get_movement().get_max_speed(roomba_movement::move_modes::dash);
		float rate = max_speed > 0.0f ? math::saturate(speed / max_speed) : 0.0f;
		float alpha = 1.0f - expf(-fov_follow_rate * elapsed_time);
		update_fov_rate_inner = math::lerp(update_fov_rate_inner, rate, alpha);

		rate = math::easing::calculate(update_fov_rate_inner, fov_ease);
		camera->get_camera()->get_parameters().fov_y = static_cast<float>(math::lerp(fov_range.x, fov_range.y, static_cast<double>(rate)));
	}
}

void minigame000::roomba_camera_controller_object::update(float elapsed_time)
{
	base::update(elapsed_time);

	if (!input_purmit_flag)
		return;

	//	旋回加速処理
	math::float2 move;
	move.x = input::get_key_value(mouse::axis_states::x);
	move.y = input::get_key_value(mouse::axis_states::y);

	//	右スティックはマウスの移動量に換算する
	move.x += input::get_key_value(gamepad::axis_states::right_x) * stick_move_per_second * elapsed_time;
	move.y -= input::get_key_value(gamepad::axis_states::right_y) * stick_move_per_second * elapsed_time;
	float move_length = (move.x * move.x + move.y * move.y);
	if (move_length > 0.001f * 0.001f)
	{
		disengage_direction.x += math::angle::to_radian(move.y) * mouse_sensitivity;
		disengage_direction.y += math::angle::to_radian(move.x) * mouse_sensitivity;

		disengage_direction.x = math::clamp(disengage_direction.x, pitch_limit_angle.x, pitch_limit_angle.y);
		disengage_direction.y = math::angle::normalize(disengage_direction.y);
	}
}

#if defined(USE_IMGUI)
void minigame000::roomba_camera_controller_object::imgui_render_core()
{
	base::imgui_render_core();

	ImGui::Text("state = %s", NAMEOF_ENUM(state).data());
	ImGui::Checkbox("input_purmit_flag", &input_purmit_flag);
	ImGui::SliderFloat("minimum_distance", &minimum_distance, 0.01f, 100.0f);
	ImGui::SliderFloat("maximum_distance", &maximum_distance, 0.01f, 100.0f);
	ImGui::SliderFloat("current_distance", &current_distance, minimum_distance, maximum_distance);
	ImGui::SliderFloat("update_focus_rate", &update_focus_rate_inner, 0.01f, 1.0f);
}
#endif	//	defined(USE_IMGUI)

void minigame000::roomba_camera_controller_object::change_state(states state)
{
	this->state = state;
	disengage_direction = { 0, 0, 0 };
}

void minigame000::roomba_camera_controller_object::on_finalize()
{
	target.reset();
	camera.reset();

	base::on_finalize();
}