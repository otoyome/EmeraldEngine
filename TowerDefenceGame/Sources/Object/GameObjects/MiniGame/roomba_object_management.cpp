#include "object/gameobjects/MiniGame/roomba_object.h"

namespace
{
	//	入力・停止判定の閾値
	constexpr float move_input_dead_zone = 0.01f;
	constexpr float move_input_dead_zone_sq = move_input_dead_zone * move_input_dead_zone;

	constexpr float action_move_input_dead_zone = 0.1f;
	constexpr float action_move_input_dead_zone_sq = action_move_input_dead_zone * action_move_input_dead_zone;

	//	傾き関連
	constexpr float ground_tilt_follow_rate = 15.0f;
	constexpr float velocity_tilt_follow_rate = 30.0f;
	constexpr float velocity_roll_max = math::angle::to_radian(+20.0f);
	constexpr float velocity_pitch_max = math::angle::to_radian(20.0f);

	//	死亡加速
	constexpr float death_spin_speed = math::angle::to_radian(720.0f);
	constexpr math::float3 dead_impulse = system_units::convert_m_to_unit(math::float3(0.0f, 10.0f, 0.0f));

	//	サウンド
	constexpr std::string_view accelerate_sound_path = "Assets/Resources/Sounds/roomba_accelerate.wav";
	constexpr std::string_view slide_sound_path = "Assets/Resources/Sounds/roomba_brake.wav";
	constexpr std::string_view explosion_sound_path = "Assets/Resources/Sounds/roomba_explosion.wav";
	constexpr std::string_view wet_sound_path = "Assets/Resources/Sounds/roomba_wet.wav";
	constexpr std::string_view trash_sound_path = "Assets/Resources/Sounds/roomba_trash.wav";
	constexpr float slide_sound_min_speed = 5.0f;	//	横滑り音を鳴らす最低の横方向速度(m/s)
	constexpr float slide_sound_max_speed = 15.0f;	//	横滑り音が最大音量になる横方向速度(m/s)
	constexpr float slide_sound_min_slip_rate = 0.34f;	//	横滑り音を鳴らす最低の横滑り率（横方向速度/速さ）
	constexpr float wet_sound_interval = 0.15f;		//	濡れる音を鳴らす最短の間隔（秒）
	constexpr float trash_sound_interval = 0.08f;	//	ゴミを拾った音を鳴らす最短の間隔（秒）
	constexpr float hit_sound_full_count = 8.0f;	//	間隔内にこの数だけまとまると最大音量で鳴らす

	//	地面の傾きを角度で算出
	math::float3 calculate_ground_tilt(const math::float3& euler, const math::float3& normal)
	{
		math::float3 front = { sinf(euler.y), 0.0f, +cosf(euler.y) };
		math::float3 right = { cosf(euler.y), 0.0f, -sinf(euler.y) };

		float forward_component = normal.x * front.x + normal.z * front.z;
		float right_component   = normal.x * right.x + normal.z * right.z;

		math::float3 out;
		out.x = atan2f(forward_component, normal.y);
		out.y = euler.y;
		out.z = asinf(math::clamp(-right_component, -1.0f, 1.0f));
		return out;
	}

	//	速度ベクトルから前後・左右の傾き角を算出
	math::float2 calculate_velocity_tilt(float yaw, const math::float3& velocity, float max_speed, bool flip_pitch = false)
	{
		math::float2 front = { sinf(yaw), +cosf(yaw) };
		math::float2 right = { cosf(yaw), -sinf(yaw) };

		float forward_speed = velocity.x * front.x + velocity.z * front.y;
		float lateral_speed = velocity.x * right.x + velocity.z * right.y;

		float forward_rate = (max_speed > FLT_EPSILON) ? math::clamp(forward_speed / max_speed, -1.0f, 1.0f) : 0.0f;
		float lateral_rate = (max_speed > FLT_EPSILON) ? math::clamp(lateral_speed / max_speed, -1.0f, 1.0f) : 0.0f;

		math::float2 out;
		float pitch = velocity_pitch_max * fabsf(forward_rate);
		out.x = (flip_pitch ? (forward_rate < 0.0f) : (forward_rate > 0.0f)) ? -pitch : pitch;
		out.y = lateral_rate * velocity_roll_max;
		return out;
	}
}

void roomba_object::fixed_update(float elapsed_time)
{
	//	傾き破棄
	set_rotation({get_rotation().x, get_rotation().y, 0});

	//	時間遅延関係は除外して時間経過
	//hit_stop_time = std::max(hit_stop_time - elapsed_time, 0.0f);
	//invisile_timer = std::max(invisile_timer - elapsed_time, 0.0f);

	//	経過時間更新
	elapsed_time *= elapsed_time_scale;	

	//	ヒットストップ
	//get_model_object()->set_animation_speed(hit_stop_time <= FLT_EPSILON ? elapsed_time_scale : 0.0f);
	//if (hit_stop_time > 0.0f)
	//	return;

	//	各種フラグを落としておく
	set_flag(roomba_object::flags::invisible, false);

	//	更新ステート呼び出し
	state_machine.update(elapsed_time);

	//	無敵処理
	//if (invisile_timer > 0.0f)
	//{
	//	set_flag(roomba_object::flags::invisible, true);
	//}

	//	速度更新
	update_rotation(elapsed_time);
	update_velocity(elapsed_time);
	update_move(elapsed_time);

	//	各種ライフ情報更新
	{
		//	水分量に寄る死亡判定
		if (!check_same_state(roomba_object::states::dead) && get_wet_rate() >= 1.0f)
		{
			change_state(roomba_object::states::dead);
		}

		//	熱量/水分量更新
		update_values(elapsed_time);

		//	熱量に寄る死亡判定
		if (!check_same_state(roomba_object::states::dead) && get_heat_rate() >= 1.0f)
		{
			change_state(roomba_object::states::dead);
		}
	}

	//	傾き適応
	if (check_same_state(roomba_object::states::dead))
	{
		//	死亡時はスピンさせる
		auto velocity = get_velocity();
		math::vector3f Velocity(velocity);
		float speed = Velocity.length();
		if (speed >= 0.001f)
		{
			Velocity = Velocity.normalized();
			math::vector3f UpVector = -math::vector3f(world_gravity).normalized();
			UpVector = math::vector3f::cross(Velocity, UpVector);

			if (UpVector.square_length() > 0.00001f)
			{
				death_spin_angle = speed;

				math::quaternionf Rotation = death_spin_rotator.to_quaternion();
				math::quaternionf AddRotation = math::quaternionf::rotation_axis(UpVector, death_spin_angle);
				Rotation = Rotation * AddRotation;

				math::float4 rot;
				rot = Rotation.to_float4();
				math::rotatorf new_rot{ math::quaternionf(rot) };
				new_rot.y -= math::constants<float>::pi_half;	//	補正
				set_rotation(new_rot);
			}
		}
	}
	else
	{
		//	速度から傾き算出
		{
			auto max_speed = get_max_speed(roomba_object::move_mode::dash);
			bool flip_flag = move_vector.x * move_vector.x + move_vector.y * move_vector.y <= 0.0001f;
			math::float2 velocity_tilt_target = calculate_velocity_tilt(get_rotation().y, get_velocity(), max_speed, flip_flag);

			float velocity_tilt_alpha = 1.0f - expf(-velocity_tilt_follow_rate * elapsed_time);
			velocity_tilt_pitch += (velocity_tilt_target.x - velocity_tilt_pitch) * velocity_tilt_alpha;
			velocity_tilt_roll  += (velocity_tilt_target.y - velocity_tilt_roll) * velocity_tilt_alpha;
		}

		//	地面の法線から傾き算出
		{
			math::float3 ground_tilt = calculate_ground_tilt(get_rotation().to_vector3().to_float3(), ground_normal);

			float ground_tilt_alpha = 1.0f - expf(-ground_tilt_follow_rate * elapsed_time);
			ground_tilt_pitch += (ground_tilt.x - ground_tilt_pitch) * ground_tilt_alpha;
			ground_tilt_roll  += (ground_tilt.z - ground_tilt_roll) * ground_tilt_alpha;
		}

		math::rotatorf rot = get_rotation();
		//	@remainder	モデルの問題で設定を変えている
		rot.x = ground_tilt_roll + velocity_tilt_roll;
		rot.z = ground_tilt_pitch + velocity_tilt_pitch;
		set_rotation(rot);
	}

	//	各種速度関係の情報はクリアしておく
	move_vector = {};

	//	サウンド制御用に入力状態を残しておく
	last_dash_input = (action_data.move.x != 0 || action_data.move.z != 0) && action_data.dash;

	//	アクション情報クリア
	action_data = {};
}

void roomba_object::post_update(float elapsed_time)
{
	update_effects(elapsed_time);
	update_sounds(elapsed_time);
}

void roomba_object::input_action_data(action_datas data)
{
	action_data = data;
	float mx = action_data.move.x;
	float my = action_data.move.y;
	float mz = action_data.move.z;
	float ml = (mx * mx + my * my + mz * mz);
	if (ml >= action_move_input_dead_zone_sq)
	{
		ml = sqrtf(ml);
		action_data.move.x /= ml;
		action_data.move.y /= ml;
		action_data.move.z /= ml;
	}
}

std::string_view roomba_object::get_current_state() const
{
	if (state_machine.get_current_state())
		return state_machine.get_current_state()->get_name();
	return "";
}

bool roomba_object::check_same_state(std::string_view state_name) const
{
	return get_current_state() == state_name;
}

bool roomba_object::check_same_state(states state) const
{
	return check_same_state(magic_enum::enum_name(state));
}

bool roomba_object::change_state(std::string_view state_id)
{
	if (state_machine.change_state(state_id))
	{
		//	変更時呼び出し
		auto it_id = on_change_states.find(std::string(state_id));
		if (it_id != on_change_states.end())
		{
			for (auto& it : it_id->second)
			{
				it.second(this);
			}
		}
		return true;
	}
	return false;
}

bool roomba_object::change_state(states state)
{
	return change_state(magic_enum::enum_name(state));
}

bool roomba_object::register_change_state_callback(std::string_view state_id, std::string_view identifier, call_back_change_state callback)
{
	auto it = on_change_states.find(std::string(state_id));
	if (it == on_change_states.end())
	{
		on_change_states.insert_or_assign(std::string(state_id), call_back_change_state_inner_map());
		it = on_change_states.find(std::string(state_id));
	}

	auto id = std::string(identifier);
	auto it_inner = it->second.find(id);
	if (it_inner != it->second.end())
		return false;

	it->second.insert_or_assign(id, callback);
	return true;
}

bool roomba_object::unregister_change_state_callback(std::string_view state_id, std::string_view identifier)
{
	auto it = on_change_states.find(std::string(state_id));
	if (it == on_change_states.end())
		return false;

	auto id = std::string(identifier);

	auto it_inner = it->second.find(id);
	if (it_inner == it->second.end())
		return false;

	it->second.erase(it_inner);
	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void roomba_object::register_states()
{
#define REGISTER_MACRO(STATE) \
	register_state<function_finite_state_node>(#STATE, \
												[this](){enter_##STATE();}, \
												[this](float elapsed_time) {update_##STATE(elapsed_time);}, \
												[this](){exit_##STATE();});
	REGISTER_MACRO(active);
	REGISTER_MACRO(dead);
#undef REGISTER_MACRO
}

void roomba_object::update_values(float elapsed_time)
{
	float elapsed_frame = system_units::convert_sec_to_fps(elapsed_time);

	//	加速による加熱
	float moved = action_data.move.x != 0 || action_data.move.z != 0;
	float accelaration = moved * 0.2f + (moved * action_data.dash) * 0.8f;
	float accelaration_rate = math::lerp(-0.5f, 1.0f, accelaration);

	//	水分量による放熱
	float wet_cooling_rate = get_wet_rate() * 0.75f;

	//	空気抵抗による放熱・乾燥を算出
	auto velocity = get_velocity();
	auto Velocity = math::vector3f(velocity);
	float speed = Velocity.length();
	float speed_rate = math::smoothstep(0.0f, get_max_speed(move_mode::dash), speed);
	float air_drag_heat_rate = math::lerp(0.0f, 0.5f, speed_rate);
	float air_drag_wet_rate = math::lerp(0.1f, 0.75f, speed_rate);

	//	単純な式
	float base_wet_value = 30.0f;
	float base_heat_value = 30.0f;
	float heat_rate = (accelaration_rate - air_drag_heat_rate - wet_cooling_rate) * elapsed_frame;
	float wet_rate = -fabsf(air_drag_wet_rate) * elapsed_frame;
	set_heat_value(get_heat_value() + static_cast<int>(base_heat_value * heat_rate));
	set_wet_value(get_wet_value() + static_cast<int>(base_wet_value * wet_rate));
}

void roomba_object::enter_active()
{
	//	何もしない
}

void roomba_object::update_active(float elapsed_time)
{
	math::float2 move = math::float2(action_data.move.x, action_data.move.z);

	//	入力に合わせて制御
	add_move_vector(move);
	set_turn_vector(move);

	bool dashing = (move.x != 0 || move.y != 0) && action_data.dash;
	if (dashing)
	{
		if (dash_effect_instance_node_index[0] == effect_instance::invalid_id)
		{
			dash_effect_instance_node_index[0] = dash_effect_instance->search_node_index_by_name("smoke left");
			dash_effect_instance_node_index[1] = dash_effect_instance->search_node_index_by_name("smoke right");
		}
	}
	else if (dash_effect_instance_node_index[0] != effect_instance::invalid_id)
	{
		dash_effect_instance_node_index[0] = effect_instance::invalid_id;
		dash_effect_instance_node_index[1] = effect_instance::invalid_id;
	}
}

void roomba_object::exit_active()
{
	dash_effect_instance_node_index[0] = effect_instance::invalid_id;
	dash_effect_instance_node_index[1] = effect_instance::invalid_id;
}

void roomba_object::enter_dead()
{
	add_impulse(dead_impulse);

	//	死亡情報取得
	death_spin_angle = 0.0f;
	death_spin_rotator = get_rotation();

	//	爆発させる
	auto effect = play_effect(static_cast<size_t>(effect_id::dead)).lock();
	if (effect)
	{
		effect->set_position(math::vector3d(get_position()));
	}

	//	爆発音
	if (audio_manager::is_valid())
	{
		audio_manager::instance()->play(explosion_sound_path);
	}
}

void roomba_object::update_dead(float elapsed_time)
{
	//	特に何もしない
}

void roomba_object::exit_dead()
{
}

void roomba_object::on_wet(float rate)
{
	set_wet_value(get_wet_value() + rate);
}

void roomba_object::on_trash(int count)
{
	//	音は update_sounds() でまとめて鳴らす
	trash_sound_requests += count;
}

void roomba_object::on_bubble(int count)
{
	//	音は update_sounds() でまとめて鳴らす
	wet_sound_requests += count;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void roomba_object::update_rotation(float elapsed_time)
{
	math::float2 vector = {turn_front_vector.x, turn_front_vector.y};
	float length = (vector.x * vector.x + vector.y * vector.y);
	if (length < 0.001f * 0.001f)
		return;

	length = sqrtf(length);
	vector.x /= length;
	vector.y /= length;

	//	前ベクトル算出
	math::rotatorf rotator = get_rotation();
	math::float2 front = math::float2(sinf(rotator.y), cosf(rotator.y));

	//	回転量算出
	float dot = front.x * vector.x + front.y * vector.y;
	float cross = front.x * vector.y - front.y * vector.x;
	float rotation = acosf(math::clamp(dot, -1.0f, 1.0f));
	float speed = rotation_speed * elapsed_time;
	if (rotation > speed)
		rotation = speed;

	rotator.y += ( cross < 0.0f ) ? rotation : -rotation;
	set_rotation(rotator);
}

void roomba_object::update_velocity(float elapsed_time)
{
	math::vector3f Velocity(velocity);
	float speed_sq = Velocity.square_length();

	//	加速処理
	math::float3 drive_force = {0.0f, 0.0f, 0.0f};
	if (is_flag(flags::on_ground))
	{
		//	加速は水平方向のみで考える
		float move_length = move_vector.x * move_vector.x + move_vector.y * move_vector.y;
		if (move_length > move_input_dead_zone_sq)
		{
			float move_vector_x = move_vector.x;
			float move_vector_z = move_vector.y;
			move_length = sqrtf(move_length);
			move_vector_x /= move_length;
			move_vector_z /= move_length;

			//	歩き時のみ加速リミッター(爆速ルンバモードでは速度の上限は与えない)
			float limiter_rate = 1.0f;
			if (!action_data.dash)
			{
				math::vector3f DriveDirection(move_vector_x, 0.0f, move_vector_z);
				float speed_along_drive = math::vector3f::dot(Velocity, DriveDirection);
				float max_speed = get_max_speed(move_mode::walk);
				float speed_margin = max_speed * 0.1f;
				float over_speed = speed_along_drive - (max_speed - speed_margin);
				limiter_rate = 1.0f - math::saturate(over_speed / speed_margin);
			}

			auto accelaration_force = get_current_accelaration_force() * limiter_rate * elapsed_time;
			drive_force = {move_vector_x * accelaration_force, 0, move_vector_z * accelaration_force};
		}
	}

	//	重力処理
	{
		float scale = push_box->get_mass() * gravity_scale * elapsed_time;
		drive_force.y += scale * world_gravity.x;
		drive_force.y += scale * world_gravity.y;
		drive_force.z += scale * world_gravity.z;
	}

	//	加速力
	math::vector3f DriveForce(drive_force);

	//	摩擦処理
	if (is_flag(flags::on_ground) && speed_sq > FLT_MIN)
	{
		math::vector3f Normal(ground_normal);

		//	速度を面に沿う成分と法線成分に分解
		math::vector3f NormalVelocity = Normal * (math::vector3f::dot(Velocity, Normal));
		math::vector3f TangentVelocity = Velocity - NormalVelocity;
		float tangent_speed = TangentVelocity.length();

		if (tangent_speed > FLT_EPSILON)
		{
			math::vector3f Gravity = math::vector3f(world_gravity) * gravity_scale;

			//	垂直抗力
			float gravity_along_normal = fabsf(math::vector3f::dot(Gravity, Normal));
			float normal_force = gravity_along_normal * get_push_box()->get_mass();

			//	合成摩擦係数
			float total_friction = (ground_friction + get_current_friction()) * 0.5f;

			//	動摩擦力
			float kinetic_friction_force = normal_force * total_friction;

			//	ブレーキ力を合算
			if (action_data.brake)
				kinetic_friction_force += get_current_braking_force();

			//	面に沿う速度と逆向きの力積
			math::vector3f TangentDirection = TangentVelocity * (1.0f / tangent_speed);
			float resistance_impulse = kinetic_friction_force * elapsed_time;
			resistance_impulse = std::min(resistance_impulse, tangent_speed * get_push_box()->get_mass());

			math::vector3f ResistanceForce = TangentDirection * resistance_impulse;
			DriveForce = DriveForce - ResistanceForce;
		}
	}

	//	空気抵抗
	if (speed_sq > FLT_EPSILON)
	{
		float speed = sqrtf(speed_sq);

		//	空気密度
		float air_density = world_air_density;

		//	前面投影面積(m^2)
		float frontal_area = calculate_frontal_area();

		//	抗力係数
		float drag_coefficient = calculate_drag_coefficient();

		//	空気抵抗
		float air_force = 0.5f * air_density * drag_coefficient * frontal_area * speed_sq;

		//	進行方向と逆向きの力積
		float drag_impulse = air_force * elapsed_time;
		drag_impulse = std::min(drag_impulse, speed * get_push_box()->get_mass());

		math::vector3f DragDirection = Velocity * (1.0f / speed);
		math::vector3f DragForce = DragDirection * drag_impulse;
		DriveForce = DriveForce - DragForce;
	}

	//	力追加
	drive_force = DriveForce.to_float3();
	add_force(drive_force);
}

void roomba_object::update_move(float elapsed_time)
{
	const math::vector3d start_position = model->get_world_translation();

	ground_friction = 1.0f;
	ground_normal = {};
	set_flag(flags::on_ground, false);

	math::vector3d resolved_position;
	math::float3 hit_normal;
	float friction = 0.0f;
	if (calculate_position(velocity, elapsed_time, resolved_position, hit_normal, &friction))
	{
		set_flag(flags::on_ground, true);
		ground_normal = hit_normal;
		ground_friction = friction;
	}

	model->set_world_translation(resolved_position);
	if (push_box)
		push_box->set_transform(model->get_world().to_rigid());

	//	実際に進めた変位から速度を再算出
	if (elapsed_time > FLT_EPSILON)
	{
		const math::float3 displacement = (resolved_position - start_position).to_float3();
		velocity.x = displacement.x / elapsed_time;
		velocity.y = displacement.y / elapsed_time;
		velocity.z = displacement.z / elapsed_time;
	}
}

bool roomba_object::calculate_position(math::float3 velocity, float elapsed_time, math::vector3d& out_position, math::float3& out_normal, float* out_friction)
{
	auto collision_object = get_push_box();
	auto bounding_object = std::static_pointer_cast<collision::bounding_capsule>(collision_object->get_bounding_object());

	const math::vector3d base_position = model->get_world_translation();
	math::float3 relative_position = { 0.0f, 0.0f, 0.0f };

	float height = static_cast<float>(bounding_object->get_radius() + bounding_object->get_length() * 0.5);
	math::vector3f Offset(0.0f, height, 0.0f);

	collision_world::capsule_cast_descs desc;
	desc.radius = static_cast<float>(bounding_object->get_radius());
	desc.length = static_cast<float>(bounding_object->get_length());
	desc.filter = is_flag(flags::invisible)
		? collision_filter_manager::instance()->search_filter("invisible")
		: collision_filter_manager::instance()->search_filter(collision_filter_name);
	desc.exclude_objects.emplace_back(get_push_box());

	static constexpr float StopFactor = system_units::convert_cm_to_unit(0.01f);
	static constexpr float SkinStep = system_units::convert_cm_to_unit(0.1f);

	bool hit_flag = false;
	math::vector3f Velocity(velocity);
	float remaining_time = elapsed_time;

	for (size_t iteration = 0; iteration < 3; ++iteration)
	{
		float speed_sq = Velocity.square_length();
		if (speed_sq < StopFactor * StopFactor || remaining_time <= 0.0f)
			break;

		float speed = sqrtf(speed_sq);
		math::vector3f NormalizedVelocity = Velocity.normalized();
		float travel_distance = speed * remaining_time + SkinStep;
		math::vector3f MoveVector = NormalizedVelocity * travel_distance;

		math::vector3f From = math::vector3f(relative_position) + Offset;
		math::vector3f To = From + MoveVector;

		desc.start = (base_position + math::vector3d(From)).to_double3();
		desc.end = (base_position + math::vector3d(To)).to_double3();

		collision::intersect_data intersect_data;
		collision_object_ptr hit_collision_object;
		if (object_manager::instance()->get_world().capsule_cast(desc, intersect_data, hit_collision_object))
		{
			hit_flag = true;

			math::vector3f Normal = math::vector3f(intersect_data[0].normal).normalized();
			out_normal = Normal.to_float3();

			float hit_distance = std::max(static_cast<float>(intersect_data[0].length) - SkinStep, 0.0f);
			math::vector3f Position = From + (NormalizedVelocity * hit_distance);
			Position = Position + (Normal * SkinStep);
			relative_position = (Position - Offset).to_float3();

			if (out_friction)
				*out_friction = hit_collision_object->get_physical_data().friction;

			float traveled_fraction = math::saturate(hit_distance / travel_distance);
			remaining_time -= remaining_time * traveled_fraction;
			if (remaining_time <= 0.0f)
				break;

			//	重力が水平速度になるのを制御
			float dot = math::vector3f::dot(Velocity, Normal);
			Velocity = Velocity - (Normal * dot);
		}
		else
		{
			relative_position = ((From + MoveVector) - Offset).to_float3();
			break;
		}
	}

	out_position = base_position + math::vector3d(relative_position);
	return hit_flag;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void roomba_object::register_effects()
{
	//	エフェクトリソース読込
	register_effect(static_cast<size_t>(effect_id::dash_dust), "Assets/Resources/Effects/DashDust.eme");
	register_effect(static_cast<size_t>(effect_id::spark), "Assets/Resources/Effects/Spark.eme");
	register_effect(static_cast<size_t>(effect_id::spark_smoke), "Assets/Resources/Effects/SparkSmoke.eme");
	register_effect(static_cast<size_t>(effect_id::dead), "Assets/Resources/Effects/Dead.eme");

	//	走りエフェクトは常時必要なので
	auto dash_effect = play_effect(static_cast<size_t>(effect_id::dash_dust));
	{
		dash_effect_instance = dash_effect.lock();
		if (dash_effect_instance)
		{
			dash_effect_instance_node_index[0] = effect_instance::invalid_id;
			dash_effect_instance_node_index[1] = effect_instance::invalid_id;
		}
	}

	//	スパークエフェクト
	auto spark_effect = play_effect(static_cast<size_t>(effect_id::spark));
	{
		spark_effect_instance = spark_effect.lock();
	}

	//	煙エフェクト
	auto spark_smoke_effect = play_effect(static_cast<size_t>(effect_id::spark_smoke));
	{
		spark_smoke_effect_instance = spark_smoke_effect.lock();
	}
}

void roomba_object::update_effects(float elapsed_time)
{
	//	走り関係の対応
	if (dash_effect_instance)
	{
		//	スポーンフラグ
		dash_effect_instance->set_flag(effect_instance::flags::spawn_stop,
									   dash_effect_instance_node_index[0] == effect_instance::invalid_id &&
									   dash_effect_instance_node_index[1] == effect_instance::invalid_id);

		auto& nodes = dash_effect_instance->get_nodes();
		auto right = get_rotation().right().to_float3();
		if (dash_effect_instance_node_index[0] != effect_instance::invalid_id)
		{
			auto& node = nodes[dash_effect_instance_node_index[0]];
			node.rotation = get_rotation().to_vector3().to_float3();
			node.position.x = get_position().x - right.x * 1.5f;
			node.position.y = get_position().y - right.y * 1.5f;
			node.position.z = get_position().z - right.z * 1.5f;
		}
		if (dash_effect_instance_node_index[1] != effect_instance::invalid_id)
		{
			auto& node = nodes[dash_effect_instance_node_index[1]];
			node.position = get_position();
			node.rotation = get_rotation().to_vector3().to_float3();
			node.position.x = get_position().x + right.x * 1.5f;
			node.position.y = get_position().y + right.y * 1.5f;
			node.position.z = get_position().z + right.z * 1.5f;
		}
	}

	//	死亡判定
	bool dead_flag = check_same_state(roomba_object::states::dead);
	float dead_rate = 0.5f;
	//	漏電によるスパーク対応
	if (spark_effect_instance)
	{
		if (dead_flag)
			spark_effect_instance->set_flag(effect_instance::flags::spawn_stop, false);
		else
			spark_effect_instance->set_flag(effect_instance::flags::spawn_stop, get_wet_rate() < 0.5f);
		spark_effect_instance->set_position(math::vector3d(get_position()));
		float scale = dead_flag ? dead_rate : get_wet_rate() * 1.5f;
		spark_effect_instance->set_scale({scale, scale, scale});
	}

	//	熱による煙対応
	if (spark_smoke_effect_instance)
	{
		if (dead_flag)
			spark_smoke_effect_instance->set_flag(effect_instance::flags::spawn_stop, false);
		else
			spark_smoke_effect_instance->set_flag(effect_instance::flags::spawn_stop, get_wet_rate() < 0.5f);
		spark_smoke_effect_instance->set_position(math::vector3d(get_position()));
		float scale = dead_flag ? dead_rate : get_heat_rate() * 1.5f;
		spark_smoke_effect_instance->set_scale({scale, scale, scale});
	}

	//	エフェクトインスタンスを監視して破棄処理
	auto it_effect = effect_instances.begin();
	while (it_effect != effect_instances.end())
	{
		auto effect_instance = (*it_effect);
		if (effect_instance->get_animation_time() >= effect_instance->get_animation_end_time())
		{
			effect_manager::instance()->unregister_instance(effect_instance);
			it_effect = effect_instances.erase(it_effect);
		}
		else
		{
			++it_effect;
		}
	}
}

void roomba_object::unregister_effects()
{
	//	エフェクト破棄
	auto it_effect = effect_instances.begin();
	while (it_effect != effect_instances.end())
	{
		effect_manager::instance()->unregister_instance((*it_effect));
		++it_effect;
	}
	effect_instances.clear();
	effect_resources.clear();
}

bool roomba_object::register_effect(size_t effect_id, std::string_view file_path)
{
	if (effect_resources.find(effect_id) != effect_resources.end())
		return false;

	effect_resource_ptr resource;
	if (effect_resource_manager::instance()->load(resource, file_path))
	{
		effect_resources.insert_or_assign(effect_id, resource);
		return true;
	}
	return false;
}

std::weak_ptr<effect_instance> roomba_object::play_effect(size_t effect_id)
{
	auto it = effect_resources.find(effect_id);
	if (it == effect_resources.end())
		return std::weak_ptr<effect_instance>();

	auto instance = std::make_shared<effect_instance>(it->second);
	if (effect_manager::instance()->register_instance(instance))
	{
		effect_instances.emplace_back(instance);
	}
	return instance;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void roomba_object::update_sounds(float elapsed_time)
{
	if (!audio_manager::is_valid())
		return;

	audio_manager* audio = audio_manager::instance();

	//	生存中だけ鳴らす
	bool active = check_same_state(roomba_object::states::active);
	float speed_m_s = system_units::convert_unit_to_m(math::vector3f(velocity).length());

	//	加速音：ダッシュ中はループさせ、速度に合わせてピッチと音量を上げる
	if (active && last_dash_input)
	{
		if (!audio->is_playing(accelerate_sound))
		{
			audio::play_parameters parameters;
			parameters.loop = true;
			parameters.fade_in_time = 0.1f;
			accelerate_sound = audio->play(accelerate_sound_path, parameters);
		}
		float max_speed_m_s = system_units::convert_unit_to_m(get_max_speed(move_mode::dash));
		float speed_rate = math::saturate(speed_m_s / max_speed_m_s);
		audio->set_pitch(accelerate_sound, math::lerp(0.8f, 2.0f, speed_rate));
		audio->set_volume(accelerate_sound, math::lerp(0.5f, 1.0f, speed_rate));
	}
	else if (accelerate_sound.is_valid())
	{
		audio->stop(accelerate_sound, 0.3f);
		accelerate_sound = {};
	}

	//	横滑り音：接地中に向きと横方向へ滑っている間だけループさせ、横方向に速いほど大きく鳴らす
	float yaw = get_rotation().y;
	float lateral_m_s = fabsf(system_units::convert_unit_to_m(velocity.x * cosf(yaw) - velocity.z * sinf(yaw)));
	bool sliding = lateral_m_s >= slide_sound_min_speed && lateral_m_s >= speed_m_s * slide_sound_min_slip_rate;
	if (active && is_flag(flags::on_ground) && sliding)
	{
		if (!audio->is_playing(slide_sound))
		{
			audio::play_parameters parameters;
			parameters.loop = true;
			parameters.fade_in_time = 0.05f;
			slide_sound = audio->play(slide_sound_path, parameters);
		}
		float slide_rate = math::saturate((lateral_m_s - slide_sound_min_speed) / (slide_sound_max_speed - slide_sound_min_speed));
		audio->set_pitch(slide_sound, math::lerp(0.9f, 1.1f, slide_rate));
		audio->set_volume(slide_sound, math::lerp(0.3f, 1.0f, slide_rate));
	}
	else if (slide_sound.is_valid())
	{
		audio->stop(slide_sound, 0.15f);
		slide_sound = {};
	}

	//	泡・ゴミの音：一度に大量に当たっても間隔をあけて 1 回ずつ鳴らし、まとまった数に応じて音量を上げる
	auto play_hit_sound = [&](std::string_view path, audio::sound_handle& handle, int& requests, float& cooldown, float interval)
	{
		cooldown = cooldown > elapsed_time ? cooldown - elapsed_time : 0.0f;
		if (!active)
		{
			requests = 0;
			return;
		}
		if (requests <= 0 || cooldown > 0.0f)
			return;

		if (handle.is_valid())
			audio->stop(handle, 0.05f);

		audio::play_parameters parameters;
		parameters.volume = math::lerp(0.4f, 1.0f, math::saturate(static_cast<float>(requests) / hit_sound_full_count));
		parameters.pitch = math::lerp(0.9f, 1.1f, static_cast<float>(rand()) / RAND_MAX);
		handle = audio->play(path, parameters);
		requests = 0;
		cooldown = interval;
	};
	play_hit_sound(wet_sound_path, wet_sound, wet_sound_requests, wet_sound_cooldown, wet_sound_interval);
	play_hit_sound(trash_sound_path, trash_sound, trash_sound_requests, trash_sound_cooldown, trash_sound_interval);
}

void roomba_object::stop_sounds()
{
	if (audio_manager::is_valid())
	{
		audio_manager::instance()->stop(accelerate_sound, 0.1f);
		audio_manager::instance()->stop(slide_sound, 0.1f);
		audio_manager::instance()->stop(wet_sound, 0.1f);
		audio_manager::instance()->stop(trash_sound, 0.1f);
	}
	accelerate_sound = {};
	slide_sound = {};
	wet_sound = {};
	trash_sound = {};
	wet_sound_requests = 0;
	trash_sound_requests = 0;
}


