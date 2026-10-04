#include "object/gameobjects/MiniGame/roomba_camera_controller_object.h"

#include "object/gameobjects/cinematics/cinematics_object.h"

namespace
{
	//	ヨー回転速度
	constexpr float rotation_speed = math::angle::to_radian(90.0f);
	//	±ピッチ制限角度
	constexpr math::float2 pitch_limit_angle = { math::angle::to_radian(+5.0f), math::angle::to_radian(45.0f) };
	//	開始距離
	constexpr float default_distance = system_units::convert_m_to_unit(7.0f);
	//	角度によるフォーカス位置のオフセット
	constexpr float focus_y_min_offset = -system_units::convert_m_to_unit(0.3f);
	constexpr float focus_y_max_offset = +system_units::convert_m_to_unit(0.3f);
	//	フォーカス追従制限
	constexpr float focus_limit_range_max = system_units::convert_m_to_unit(2.0f); // 基本フォーカス
	//	ダッシュ中の視野角制御
	constexpr float fov_follow_rate = 6.0f;
	constexpr math::float2 fov_range = {math::angle::to_radian(30.0f), math::angle::to_radian(60.0f)};
	constexpr math::easing::type fov_ease = math::easing::type::lerp;

	//	死亡時のカメラ停止時間
	constexpr float dead_wait_lock_time = 0.6f;
}

const math::rotatorf& roomba_camera_controller_object::get_rotation() const
{
	return camera->get_rotation();
}

void roomba_camera_controller_object::set_rotation(const math::rotatorf& rotator)
{
	camera->set_rotation(rotator);
}

const math::float3& roomba_camera_controller_object::get_position() const
{
	return camera->get_position();
}

void roomba_camera_controller_object::set_position(const math::float3& position)
{
	camera->set_position(position);
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

, minimum_distance(system_units::convert_m_to_unit(2.0f))
, current_distance(default_distance)
, maximum_distance(system_units::convert_m_to_unit(10.0f))
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
	focus = player_controller->get_focus_point();
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
		disengage_direction.x = -math::constants<float>::pi / 8.0f;
		disengage_direction.y += ((math::constants<float>::pi * 0.25f)) * elapsed_time;

		math::rotatorf camera_rotation = get_rotation();
		camera_rotation = math::rotatorf(math::slerp(camera_rotation.to_quaternion(), disengage_direction.to_quaternion(), update_disengage_direction_rate));
		camera_rotation.z = 0.0f;

		camera_rotation.x = math::clamp(camera_rotation.x, pitch_limit_angle.x, pitch_limit_angle.y);
		camera_rotation.y = math::angle::normalize(camera_rotation.y);
		set_rotation(camera_rotation);
	}

	//	カメラの姿勢の更新
	{
		math::float3 camera_position, camera_front, camera_focus;
		math::rotatorf camera_rotation = get_rotation();
		camera_front = camera_rotation.front().to_float3();

		float offset = system_units::convert_m_to_unit(1.0f);
		float dist = system_units::convert_m_to_unit(3.0f) - offset;
		camera_focus.x = focus.x - camera_front.x * offset;
		camera_focus.y = focus.y - camera_front.y * offset;
		camera_focus.z = focus.z - camera_front.z * offset;
		camera_position.x = camera_focus.x - camera_front.x * dist;
		camera_position.y = camera_focus.y - camera_front.y * dist;
		camera_position.z = camera_focus.z - camera_front.z * dist;
		set_rotation(camera_rotation);
		set_position(camera_position);
	}
}

void roomba_camera_controller_object::fixed_update_default(float elapsed_time)
{
	//	フォーカスポイント算出
	math::float3 new_focus = player_controller->get_controll_object()->get_focus_point();
	if (player_controller->get_controll_object()->check_same_state(roomba_object::state_names[roomba_object::state_dead]))
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
			new_focus = dead_focus = math::lerp(math::vector3f(new_focus), math::vector3f(dead_focus), 0.95f).to_float3();

			//	現在のフォーカスとカメラの位置関係から回転を算出
			math::float3 direction;
			direction.x = dead_focus.x - get_position().x;
			direction.y = dead_focus.y - get_position().y;
			direction.z = dead_focus.z - get_position().z;
			disengage_direction = math::rotatorf::from_direction(math::vector3f(direction));
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
			math::float3 temp_focus;
			temp_focus.x = focus.x + (new_focus.x - focus.x) * update_focus_rate;
			temp_focus.y = focus.y + (new_focus.y - focus.y) * update_focus_rate;
			temp_focus.z = focus.z + (new_focus.z - focus.z) * update_focus_rate;

			//	距離制限
			math::float3 focus_vector;
			focus_vector.x = new_focus.x - temp_focus.x;
			focus_vector.y = new_focus.y - temp_focus.y;
			focus_vector.z = new_focus.z - temp_focus.z;
			float length_sq = focus_vector.x * focus_vector.x + focus_vector.y * focus_vector.y + focus_vector.z * focus_vector.z;

			focus_rate = math::lerp(focus_rate, 1.0f, math::saturate(length_sq / (focus_limit_range * focus_limit_range)));
		}
		focus.x += (new_focus.x - focus.x) * focus_rate;
		focus.y += (new_focus.y - focus.y) * focus_rate;
		focus.z += (new_focus.z - focus.z) * focus_rate;
	}

	//	旋回処理
	{
		math::rotatorf camera_rotation = get_rotation();
		camera_rotation = math::rotatorf(math::slerp(camera_rotation.to_quaternion(), disengage_direction.to_quaternion(), update_disengage_direction_rate));
		camera_rotation.z = 0.0f;

		camera_rotation.x = math::clamp(camera_rotation.x, pitch_limit_angle.x, pitch_limit_angle.y);
		camera_rotation.y = math::angle::normalize(camera_rotation.y);
		set_rotation(camera_rotation);
	}

	//	カメラの姿勢の更新
	{
		math::float3 camera_position, camera_front, camera_focus;
		math::rotatorf camera_rotation = get_rotation();
		camera_front = camera_rotation.front().to_float3();

		float offset = system_units::convert_m_to_unit(1.0f);
		float dist = focus_distance - offset;
		camera_focus.x = focus.x - camera_front.x * offset;
		camera_focus.y = focus.y - camera_front.y * offset;
		camera_focus.z = focus.z - camera_front.z * offset;
		camera_position.x = camera_focus.x - camera_front.x * dist;
		camera_position.y = camera_focus.y - camera_front.y * dist;
		camera_position.z = camera_focus.z - camera_front.z * dist;
		float min_y_offset = math::linearstep(0.0f, pitch_limit_angle.x, camera_rotation.x);
		float max_y_offset = math::linearstep(0.0f, pitch_limit_angle.y, camera_rotation.x);
		camera_position.y += min_y_offset * focus_y_min_offset + max_y_offset * focus_y_max_offset;

		//	物体と当たり判定
		{
			collision_world::sphere_cast_descs desc;
			desc.filter = collision_filter_manager::instance()->search_filter("camera");
			desc.start = camera_focus;
			desc.end = camera_position;
			desc.radius = system_units::convert_m_to_unit(0.5f);

			//	ヒット処理
			collision::intersect_data intersect_data;
			collision_object_ptr hit_collision_object;
			if (object_manager::instance()->get_world().sphere_cast(desc, intersect_data, hit_collision_object))
			{
				float length = intersect_data[0].length;
				math::float3 vector;
				vector.x = camera_position.x - camera_focus.x;
				vector.y = camera_position.y - camera_focus.y;
				vector.z = camera_position.z - camera_focus.z;
				vector = math::vector3f(vector).normalized().to_float3();

				camera_position.x = camera_focus.x + vector.x * length;
				camera_position.y = camera_focus.y + vector.y * length;
				camera_position.z = camera_focus.z + vector.z * length;
			}
		}
		set_rotation(camera_rotation);
		set_position(camera_position);
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
		camera->get_camera()->set_fov_y(math::lerp(fov_range.x, fov_range.y, rate));
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
