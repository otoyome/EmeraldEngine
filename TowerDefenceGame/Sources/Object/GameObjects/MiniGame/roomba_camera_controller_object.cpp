#include "object/gameobjects/MiniGame/roomba_camera_controller_object.h"

namespace
{
	//	ヨー回転速度
	constexpr double rotation_speed = math::angle::to_radian(90.0);
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
	constexpr double fov_follow_rate = 6.0;
	constexpr math::double2 fov_range = {math::angle::to_radian(30.0), math::angle::to_radian(60.0)};
	constexpr math::easing::type fov_ease = math::easing::type::lerp;

	//	死亡時のカメラ停止時間
	constexpr float dead_wait_lock_time = 0.6f;
}

void roomba_camera_controller_object::set_input_purmit_flag(bool flag)
{
	input_purmit_flag = flag;
}

roomba_camera_controller_object::roomba_camera_controller_object(std::string_view name, std::shared_ptr<roomba_controller_object> player_controller, std::shared_ptr<camera_object> camera)
: game_object(name)
, player_controller(player_controller)
, camera(camera)

, state(states::free)
, sub_state(0)
, work_float(0.0f)
, input_purmit_flag(true)

, minimum_distance(system_units::convert_m_to_unit(2.0))
, current_distance(default_distance)
, maximum_distance(system_units::convert_m_to_unit(10.0))
, game_distance_scale(1.0f)
, game_distance_scale_inner(1.0f)
, game_distance_scale_rate(0.0f)

, disengage_direction(0, 0, 0)
, update_disengage_direction_rate(0.2f)

, focus_distance(default_distance)
, focus_limit_range(focus_limit_range_max)
, update_focus_rate_inner(0.05f)

, update_fov_rate_inner(0.0f)

, dead_wait_time(dead_wait_lock_time)
{
	focus = math::vector3d(player_controller->get_focus_point());
}

roomba_camera_controller_object::~roomba_camera_controller_object()
{
}

void roomba_camera_controller_object::fixed_update(float elapsed_time)
{
	//	ちょっと面倒なのでここで対応
	switch (state)
	{
		case states::entry:
		{
			fixed_update_entry(elapsed_time);
			break;
		}
		//case states::clear_performance:
		//{
		//	fixed_update_clear_performance(elapsed_time);
		//	break;
		//}
		default:
		{
			fixed_update_default(elapsed_time);
			break;
		}
	}
}

void roomba_camera_controller_object::fixed_update_entry(float elapsed_time)
{
	//	フォーカスポイント算出
	math::float3 new_focus = player_controller->get_controll_object()->get_focus_point();

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

void roomba_camera_controller_object::fixed_update_default(float elapsed_time)
{
	//	フォーカスポイント算出
	math::vector3d new_focus = math::vector3d(player_controller->get_controll_object()->get_focus_point());
	if (player_controller->get_controll_object()->check_same_state(roomba_object::states::dead))
	{
		if (dead_wait_time >= 0.0f && dead_wait_time < dead_wait_lock_time)
		{
			new_focus = dead_focus;
		}
		else if (dead_wait_time == dead_wait_lock_time)
		{
			dead_focus = new_focus;
		}
		else if (dead_wait_time < 0.0f)
		{
			new_focus = dead_focus = math::lerp(new_focus, dead_focus, 0.95f);

			//	現在のフォーカスとカメラの位置関係から回転を算出
			auto direction = dead_focus - camera->get_world_translation();
			disengage_direction = math::rotatord::from_direction(direction);
		}
		dead_wait_time -= elapsed_time;
	}
	else
	{
		dead_wait_time = dead_wait_lock_time;
	}

	float update_focus_rate = update_focus_rate_inner;

	//	物体との距離に応じて調整
	{
		//	ゲーム側からの指定スケール
		if (game_distance_scale_inner != game_distance_scale)
		{
			float rate = (update_focus_rate != game_distance_scale_rate) ? update_focus_rate : game_distance_scale_rate;
			game_distance_scale += (game_distance_scale_inner - game_distance_scale) * rate;
			game_distance_scale_inner = 1.0f;
			game_distance_scale_rate = update_focus_rate;
		}

		//	攻撃によるスケール
		float attack_scale = 1.0f;

		//	フォーカス距離算出
		float scale = game_distance_scale * attack_scale;
		focus_distance = math::clamp(current_distance * scale, minimum_distance, maximum_distance);
	}

	//	フォーカス更新
	{
		float focus_rate = update_focus_rate;
		{
			//	一時座標算出
			auto temp_focus = focus + (new_focus - focus) * update_focus_rate;

			//	距離制限
			auto focus_vector = new_focus - temp_focus;
			double length_sq = focus_vector.square_length();

			focus_rate = math::lerp(focus_rate, 1.0f, math::saturate(length_sq / (focus_limit_range * focus_limit_range)));
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

		double offset = system_units::convert_m_to_unit(1.0f);
		double dist = focus_distance - offset;
		auto camera_focus = focus - camera_front * offset;
		auto camera_position = camera_focus - camera_front * dist;
		double min_y_offset = math::linearstep(0.0, pitch_limit_angle.x, camera_rotation.x);
		double max_y_offset = math::linearstep(0.0, pitch_limit_angle.y, camera_rotation.x);
		camera_position.y += min_y_offset * focus_y_min_offset + max_y_offset * focus_y_max_offset;

		//	物体と当たり判定
		{
			collision_world::sphere_cast_descs desc;
			desc.filter = collision_filter_manager::instance()->search_filter("camera");
			desc.start = camera_focus.to_double3();
			desc.end = camera_position.to_double3();
			desc.radius = system_units::convert_m_to_unit(0.5);

			//	ヒット処理
			collision::intersect_data intersect_data;
			collision_object_ptr hit_collision_object;
			if (object_manager::instance()->get_world().sphere_cast(desc, intersect_data, hit_collision_object))
			{
				float length = static_cast<float>(intersect_data[0].length);
				auto vector = camera_position - camera_focus;

				camera_position.x = camera_focus.x + vector.x * length;
				camera_position.y = camera_focus.y + vector.y * length;
				camera_position.z = camera_focus.z + vector.z * length;
			}
		}
		camera->set_world_euler(camera_rotation);
		camera->set_world_translation(camera_position);
	}

	//	視野角制御
	{
		auto velocity = player_controller->get_controll_object()->get_velocity();
		auto speed = sqrtf(velocity.x * velocity.x + velocity.y * velocity.y + velocity.z * velocity.z);
		auto max_speed = player_controller->get_controll_object()->get_max_speed(roomba_object::move_mode::dash);
		float rate = math::saturate(speed / max_speed);
		float alpha = 1.0f - expf(-fov_follow_rate * elapsed_time);
		update_fov_rate_inner = math::lerp(update_fov_rate_inner, rate, alpha);

		rate = math::easing::calculate(update_fov_rate_inner, fov_ease);
		camera->get_camera()->set_fov_y(math::lerp(fov_range.x, fov_range.y, static_cast<double>(rate)));
	}
}

void roomba_camera_controller_object::update(float elapsed_time)
{
	//	ちょっと面倒なのでこれで対応
	if (!input_purmit_flag)
		return;

	//	旋回加速処理
	math::float2 move;
	move.x = static_cast<float>(mouse::instance()->get_move_cursol().x);
	move.y = static_cast<float>(mouse::instance()->get_move_cursol().y);
	float move_length = (move.x * move.x + move.y * move.y);
	if (move_length > 0.001f * 0.001f)
	{
		disengage_direction.x += math::angle::to_radian(move.y) * 0.1f;
		disengage_direction.y += math::angle::to_radian(move.x) * 0.1f;

		disengage_direction.x = math::clamp(disengage_direction.x, pitch_limit_angle.x, pitch_limit_angle.y);
		disengage_direction.y = math::angle::normalize(disengage_direction.y);
	}
}

#if defined(USE_IMGUI)
#define CHECK_BOX(FLAG) \
{\
	bool is_##FLAG = is_flag(flags::##FLAG);\
	ImGui::Checkbox(#FLAG, &is_##FLAG);\
	set_flag(flags::##FLAG, is_##FLAG);\
}

void roomba_camera_controller_object::imgui_render_core()
{
	ImGui::Text("state = %s", NAMEOF_ENUM(state).data());
	ImGui::SliderFloat("minimum_distance", &minimum_distance, 0.01f, 100.0f);
	ImGui::SliderFloat("maximum_distance", &maximum_distance, 0.01f, 100.0f);
	ImGui::SliderFloat("focus_distance", &focus_distance, minimum_distance, maximum_distance);
	ImGui::SliderFloat("update_focus_rate", &update_focus_rate_inner, 0.01f, 1.0f);
}
#undef CHECK_BOX
#endif	//	defined(USE_IMGUI)

void roomba_camera_controller_object::change_state(states state)
{
	this->state = state;
	sub_state = 0;
	work_float = 0;
	disengage_direction = {0, 0, 0};
}

void roomba_camera_controller_object::on_finalize()
{
	object_manager::instance()->unregister_object(shared_from_this());
	player_controller.reset();
}
