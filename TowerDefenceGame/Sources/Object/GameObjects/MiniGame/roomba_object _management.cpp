#include "object/gameobjects/MiniGame/roomba_object.h"

namespace setup_data
{
	constexpr float walk_accelaration = system_units::convert_m_to_unit(5.0f); // 歩きの加速度(m/s2)
	constexpr float dash_accelaration = system_units::convert_m_to_unit(15.0f); // ダッシュ加速度(m/s2)

	constexpr float walk_breaking_deceleration = system_units::convert_m_to_unit(3.0f); // 歩きの減速度(m/s2)
	constexpr float dash_breaking_deceleration = system_units::convert_m_to_unit(15.0f); // ダッシュ減速度(m/s2)
	constexpr float move_stop_breaking_deceleration = system_units::convert_m_to_unit(24.0f); // ブレーキング減速力(m/s2)

	constexpr float walk_max_speed = system_units::convert_m_to_unit(2.0f); // 歩きの速度(m/s)
	constexpr float dash_max_speed = system_units::convert_m_to_unit(10.0f); // ダッシュ速度(m/s)
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

	//	傾き破棄
	set_rotation({get_rotation().x, get_rotation().y, 0});

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
	update_vertical_velocity(elapsed_time);
	update_horizontal_velocity(elapsed_time);
	update_horizontal_move(elapsed_time);
	update_vertical_move(elapsed_time);

	//	傾き適応
	{
		set_rotation({get_rotation().x, get_rotation().y, roll_value});
		roll_value *= 0.1f;
	}

	//	各種速度関係の情報はクリアしておく
	move_vector = {};
	accelaration = {};

	//	傾き適応
	set_rotation({get_rotation().x, get_rotation().y, roll_value});
	roll_value *= 0.1f;

	//	アクション情報クリア
	action_data = {};
}

void roomba_object::post_update(float elapsed_time)
{
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

void roomba_object::input_action_data(action_datas data)
{
	action_data = data;
	float mx = action_data.move.x;
	float my = action_data.move.y;
	float mz = action_data.move.z;
	float ml = (mx * mx + my * my + mz * mz);
	if (ml >= 0.1f * 0.1f)
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
bool roomba_object::input_action(float elapsed_time)
{
	//	吸引処理
	if (action_data.attract
		// && get_special_power() >= get_use_special_power()
		)
	{
		return false;	//	change_state(state_names[state_guard]);
	}

	return false;
}

bool roomba_object::input_move(float elapsed_time, move_mode mode)
{
	//	速度指定
	float accelaration = setup_data::walk_accelaration;
	float max_speed = setup_data::walk_max_speed;
	float deceleration = setup_data::walk_breaking_deceleration;
	switch (mode)
	{
		case move_mode::dash:
		{
			accelaration = setup_data::dash_accelaration;
			max_speed = setup_data::dash_max_speed;
			deceleration = setup_data::dash_breaking_deceleration;
			break;
		}
	}
	accelaration = push_box->get_mass() * setup_data::dash_accelaration;
	max_speed = push_box->get_mass() * setup_data::dash_max_speed;
	deceleration = push_box->get_mass() * setup_data::dash_breaking_deceleration;

	//	入力ベクトル方向に速度ベクトルを修正する
	DirectX::XMFLOAT2 move = DirectX::XMFLOAT2(action_data.move.x, action_data.move.z);
	float adjust_move_velocity_rate = 1.0f;
	adjustment_velocity(move, adjust_move_velocity_rate);

	add_move_vector(move, accelaration);
	set_turn_vector(move);
	set_max_move_speed(max_speed);
	set_breaking_deceleration(deceleration);

	return move.x != 0 || move.y != 0;
}

void roomba_object::register_states()
{
#define REGISTER_MACRO(STATE) \
	register_state<function_finite_state_node>("state_"###STATE, \
												[this](){enter_##STATE();}, \
												[this](float elapsed_time) {update_##STATE(elapsed_time);}, \
												[this](){});
	REGISTER_MACRO(idle);
	REGISTER_MACRO(walk);
	REGISTER_MACRO(dash);
	REGISTER_MACRO(move_stop);
#undef REGISTER_MACRO
}

void roomba_object::enter_idle()
{
	//	何もしない
}

void roomba_object::update_idle(float elapsed_time)
{
	//	入力
	if (input_action(elapsed_time))
	{
		return;
	}

	//	移動処理
	move_mode mode = move_mode::walk;
	if (action_data.dash)
	{
		mode = move_mode::dash;
	}

	if (input_move(elapsed_time, mode))
	{
		if (mode == move_mode::walk)
		{
			change_state(roomba_object::state_names[state_walk]);
		}
		else if (mode == move_mode::dash)
		{
			change_state(roomba_object::state_names[state_dash]);
		}
	}
}

void roomba_object::exit_idle()
{
	//	何もしない
}

void roomba_object::enter_walk()
{
	//	何もしない
}

void roomba_object::update_walk(float elapsed_time)
{
	//	入力
	if (input_action(elapsed_time))
	{
		return;
	}

	//	移動処理
	move_mode mode = move_mode::walk;
	if (action_data.dash)
		mode = move_mode::dash;

	if (input_move(elapsed_time, mode))
	{
		if (mode == move_mode::dash)
			change_state(roomba_object::state_names[state_dash]);
	}
}

void roomba_object::exit_walk()
{
}

void roomba_object::enter_dash()
{
}

void roomba_object::update_dash(float elapsed_time)
{
	//	移動
	if (!input_move(elapsed_time, move_mode::dash))
	{
		change_state(state_names[state_move_stop]);
		return;
	}
}

void roomba_object::exit_dash()
{
}

void roomba_object::enter_move_stop()
{
}

void roomba_object::update_move_stop(float elapsed_time)
{
	//	待機処理も利用
	update_idle(elapsed_time);

	//	ブレーキング減速力
	set_breaking_deceleration(setup_data::move_stop_breaking_deceleration);
}

void roomba_object::exit_move_stop()
{
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void roomba_object::register_effects()
{
	//	エフェクトリソース読込

//	register_effect(static_cast<size_t>(custom_effect_id::break_attack_impact), "Assets/Resources/Effects/EventTest.eme");
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
void roomba_object::update_rotation(float elapsed_time)
{
	DirectX::XMFLOAT2 vector = {turn_front_vector.x, turn_front_vector.y};
	float length = (vector.x * vector.x + vector.y * vector.y);
	if (length < 0.001f * 0.001f)
		return;

	length = sqrtf(length);
	vector.x /= length;
	vector.y /= length;

	//	前ベクトル算出
	math::rotator rotator = get_rotation();
	DirectX::XMFLOAT2 front = DirectX::XMFLOAT2(sinf(rotator.y), cosf(rotator.y));

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

void roomba_object::update_vertical_velocity(float elapsed_time)
{
	frame_gravity.y = world_gravity.y * gravity_scale * elapsed_time;
	velocity.y += frame_gravity.y;

	//	空気抵抗
	float speed_sq = velocity.y * velocity.y;
	if (speed_sq > FLT_EPSILON)
	{
		float speed = sqrtf(speed_sq);
		float drag_force = get_air_drag_coefficient() * speed_sq;
		float drag_deceleration = (drag_force / get_push_box()->get_mass()) * elapsed_time;
		drag_deceleration = std::min(drag_deceleration, speed);

		float scale = (speed - drag_deceleration) / speed;
		velocity.y *= scale;
	}
}

void roomba_object::update_vertical_move(float elapsed_time)
{
	//	初期化
	ground_friction	= 1.0f;
	slope_rate = 1.0f;
	ground_normal = {};

	set_flag(flags::on_ground, false);

	//	ルートモーションの速度加算
	DirectX::XMFLOAT3 vel = DirectX::XMFLOAT3(0.0f, velocity.y * elapsed_time, 0.0f);

	//	座標算出
	DirectX::XMFLOAT3 position, normal;
	float fricition = 0;
	if (calculate_position(vel, true, position, normal, &fricition))
	{
		if (vel.y < 0)
		{
			set_flag(flags::on_ground, true);
			ground_normal = normal;
			slope_rate = normal.y / ( sqrtf(normal.x * normal.x + normal.z * normal.z) + normal.y);
			slope_rate = 1.0f - slope_rate;
			ground_friction	= fricition;
		}
		velocity.y = 0.0f;
	}
	set_position(position);
}

void roomba_object::update_horizontal_velocity(float elapsed_time)
{
	//	入力フラグ
	float move_length = move_vector.x * move_vector.x + move_vector.y * move_vector.y;
	bool input_move = move_length > 0.01f * 0.01f;

	//	加速処理
	if (input_move)
	{
		//	add_force
		float acceleration_force = accelaration * elapsed_time;
		float frame_acceleration = acceleration_force / get_push_box()->get_mass();

		float move_vector_x = move_vector.x;
		float move_vector_z = move_vector.y;
		move_length = sqrtf(move_length);
		move_vector_x /= move_length;
		move_vector_z /= move_length;

		velocity.x += move_vector_x * frame_acceleration;
		velocity.z += move_vector_z * frame_acceleration;
	}

	//	摩擦処理
	if (is_flag(flags::on_ground))
	{
		//	垂直抗力
		float normal_force = fabsf(frame_gravity.y) * get_push_box()->get_mass();

		//	合成摩擦係数
		float total_friction = (ground_friction + get_push_box()->get_physical_data().friction) * 0.5f;

		//	動摩擦力
		float kinetic_friction_force = normal_force * total_friction;

		//	減衰速度
		float kinetic_acceleration = kinetic_friction_force / get_push_box()->get_mass();

		//	速度減衰
		float speed = sqrtf(velocity.x * velocity.x + velocity.z * velocity.z);
		kinetic_acceleration = std::max(speed - fabsf(kinetic_acceleration), 0.0f);
		if (speed < FLT_EPSILON)
		{
			velocity.x = 0.0f;
			velocity.z = 0.0f;
		}
		else
		{
			velocity.x *= kinetic_acceleration / speed;
			velocity.z *= kinetic_acceleration / speed;
		}
	}

	//	空気抵抗
	{
		float speed_sq = velocity.x * velocity.x + velocity.z * velocity.z;
		if (speed_sq > FLT_EPSILON)
		{
			float speed = sqrtf(speed_sq);
			float drag_force = get_air_drag_coefficient() * speed_sq;
			float drag_deceleration = (drag_force / get_push_box()->get_mass()) * elapsed_time;
			drag_deceleration = std::min(drag_deceleration, speed);

			float scale = (speed - drag_deceleration) / speed;
			velocity.x *= scale;
			velocity.z *= scale;
		}
	}

	//	ブレーキング処理
	if (!input_move)
	{
		float breaking_force = breaking_deceleration * elapsed_time;
		float breaking_acceleration = breaking_force / get_push_box()->get_mass();

		float speed_sq = velocity.x * velocity.x + velocity.z * velocity.z;

		if (speed_sq <= breaking_acceleration * breaking_acceleration)
		{
			velocity.x = 0.0f;
			velocity.z = 0.0f;
		}
		else if (speed_sq > breaking_acceleration * breaking_acceleration)
		{
			float speed = sqrtf(speed_sq);
			float scale = (speed - breaking_acceleration) / speed;
			velocity.x *= scale;
			velocity.z *= scale;
		}
	}

	//	最大速度制限
	{
		float speed_sq = velocity.x * velocity.x + velocity.z * velocity.z;
		if (speed_sq > max_move_speed * max_move_speed)
		{
			float scale = max_move_speed / sqrtf(speed_sq);
			velocity.x *= scale;
			velocity.z *= scale;
		}
	}

	//	坂道補正処理
	if (is_flag(flags::on_ground) && slope_rate > 0.0f)
	{
		float speed_sq = velocity.x * velocity.x + velocity.z * velocity.z;
		if (speed_sq > FLT_EPSILON)
		{
			float speed = sqrtf(speed_sq);
			velocity.y -= speed * slope_rate * elapsed_time;
		}
	}
}

void roomba_object::update_horizontal_move(float elapsed_time)
{
	//	ルートモーションの速度加算
	DirectX::XMFLOAT3 vel = DirectX::XMFLOAT3(velocity.x * elapsed_time, 0.0f, velocity.z * elapsed_time);
	if (vel.x * vel.x + vel.z * vel.z <= 0.001f * 0.001f)
		return;

	//	座標算出
	DirectX::XMFLOAT3 new_position, normal;
	calculate_position(vel, false, new_position, normal);
	set_position(new_position);
}

void roomba_object::adjustment_velocity(DirectX::XMFLOAT2& move, float adjust_move_velocity_rate)
{
	if (move.x == 0 && move.y == 0)
		return;

	float vx = get_velocity().x, vy = get_velocity().z;
	float vl = vx * vx + vy * vy;
	if (vl > FLT_EPSILON)
	{
		float mx = move.x, my = move.y;
		float ml = sqrtf(mx * mx + my * my);

		mx /= ml;
		my /= ml;

		vl = sqrtf(vl);
		vx /= vl;
		vy /= vl;

		float sin = math::clamp(vx * my - vy * mx, -1.0f, 1.0f);
		float dot = math::clamp(mx * vx + my * vy, -1.0f, 1.0f);

		//	入力を速度方向に若干変化させる
		{
			float angle = asinf(sin) * adjust_move_velocity_rate;
			float mmx = move.x * cosf(angle) - move.y * sinf(angle);
			float mmy = move.x * sinf(angle) + move.y * cosf(angle);
			move.x = mmx;
			move.y = mmy;
		}

		//	入力方向に合わせて速度ベクトルの修正
		{
			if ((fabsf(sin) < 0.01f) && (dot < -0.1f))
			{
				set_velocity({ mx * vl, get_velocity().y, my * vl });
			}
			else
			{
				float angle = asinf(sin) * adjust_move_velocity_rate;
				float vvx = get_velocity().x * cosf(angle) - get_velocity().z * sinf(angle);
				float vvy = get_velocity().x * sinf(angle) + get_velocity().z * cosf(angle);
				set_velocity({ vvx, get_velocity().y, vvy });
			}

			//	速度に合わせて傾ける
			roll_value += DirectX::XMConvertToRadians(15.0f * sin);
			roll_value = math::clamp(roll_value, -DirectX::XMConvertToRadians(10.0f), DirectX::XMConvertToRadians(10.0f));
		}
	}
}

bool roomba_object::calculate_position(DirectX::XMFLOAT3 velocity, bool vertical, DirectX::XMFLOAT3& out_position, DirectX::XMFLOAT3& out_normal, float* out_friction)
{
	auto collision_object = get_push_box();
	auto bounding_object = std::static_pointer_cast<collision::bounding_capsule>(collision_object->get_bounding_object());

	//	初期座標
	out_position = get_position();

	//	オフセット位置を算出
	float height = bounding_object->get_radius() + bounding_object->get_length() * 0.5f;
	DirectX::XMVECTOR Offset = DirectX::XMVectorSet(0.0f, height, 0.0f, 0.0f);

	collision_world::sphere_cast_descs desc;
	desc.radius = bounding_object->get_radius();
	if (is_flag(flags::invisible))
	{
		desc.filter = collision_filter_manager::instance()->search_filter("invisible");
	}
	else
	{
		desc.filter = collision_filter_manager::instance()->search_filter(collision_filter_name);
	}
	desc.exclude_objects.emplace_back(get_push_box());

	static constexpr float StopFactor = system_units::convert_cm_to_unit(0.01f);
	static constexpr float StopFactorSq = StopFactor * StopFactor;
	static constexpr float SkinStep = system_units::convert_cm_to_unit(0.1f);

	bool hit_flag = false;
	DirectX::XMVECTOR Velocity = DirectX::XMLoadFloat3(&velocity);
	for (size_t index = 0; index < 2; ++index)
	{
		float distance = DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(Velocity));
		if (distance < StopFactorSq)
			break;

		distance = sqrtf(distance) + SkinStep;

		//	進行ベクトル補正
		DirectX::XMVECTOR NormalizedVelocity = DirectX::XMVector3Normalize(Velocity);
		DirectX::XMVECTOR MoveVector = DirectX::XMVectorScale(NormalizedVelocity, distance);
		DirectX::XMVECTOR From, To;
		From = DirectX::XMLoadFloat3(&out_position);
		From = DirectX::XMVectorAdd(From, Offset);
		To = DirectX::XMVectorAdd(From, MoveVector);
		if (vertical)
		{
			if (DirectX::XMVectorGetY(Velocity) < 0)
				To = DirectX::XMVectorSubtract(To, Offset);
			else
				To = DirectX::XMVectorAdd(To, Offset);
		}

		DirectX::XMStoreFloat3(&desc.start, From);
		DirectX::XMStoreFloat3(&desc.end, To);

		//	ヒット処理
		collision::intersect_data intersect_data;
		collision_object_ptr hit_collision_object;
		if (object_manager::instance()->get_world().sphere_cast(desc, intersect_data, hit_collision_object))
		{
			hit_flag = true;

			DirectX::XMVECTOR Normal = DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&intersect_data[0].normal));
			DirectX::XMStoreFloat3(&out_normal, Normal);

			DirectX::XMVECTOR PositionOffset = Offset;
			if (vertical)
			{
				PositionOffset = (DirectX::XMVectorGetY(Velocity) < 0)
					? DirectX::XMVectorSet(0.0f, desc.radius, 0.0f, 0.0f)
					: DirectX::XMVectorSet(0.0f, 2.0f * height - desc.radius, 0.0f, 0.0f);
			}

			DirectX::XMVECTOR Position = DirectX::XMLoadFloat3(&intersect_data[0].point);
			DirectX::XMVECTOR OffsetNormal = Normal;
			if (!vertical)
			{
				//	水平パス時は壁刷り法線調整
				OffsetNormal = DirectX::XMVectorSetY(OffsetNormal, 0.0f);
				if (DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(OffsetNormal)) > 1e-6f)
					OffsetNormal = DirectX::XMVector3Normalize(OffsetNormal);
				else
					OffsetNormal = DirectX::XMVectorZero();
			}
			else
			{
				//	垂直パス時は壁刷り法線調整
				OffsetNormal = DirectX::XMVectorSetX(OffsetNormal, 0.0f);
				OffsetNormal = DirectX::XMVectorSetZ(OffsetNormal, 0.0f);
				if (DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(OffsetNormal)) > 1e-6f)
					OffsetNormal = DirectX::XMVector3Normalize(OffsetNormal);
				else
					OffsetNormal = DirectX::XMVectorZero();
			}
			Position = DirectX::XMVectorAdd(Position, DirectX::XMVectorScale(OffsetNormal, SkinStep));
			Position = DirectX::XMVectorSubtract(Position, PositionOffset);
			DirectX::XMStoreFloat3(&out_position, Position);

			//	摩擦係数を物理マテリアルから取得してくる
			if (out_friction)
			{
				*out_friction = hit_collision_object->get_physical_data().friction;
			}

			if (vertical)
			{
				float dot = DirectX::XMVectorGetX(DirectX::XMVector3Dot(Normal, DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)));
				float slope_angle = acosf(math::clamp(dot, -1.0f, 1.0f));
				if (slope_angle < slope_limit)
					break;
			}

			float hit_distance = intersect_data[0].length - SkinStep;
			MoveVector = DirectX::XMVectorSubtract(To, From);
			float remain_distance = DirectX::XMVectorGetX(DirectX::XMVector3Length(MoveVector)) - hit_distance;
			distance = math::clamp(remain_distance, 0.0f, distance);
			if (distance <= StopFactor)
				break;

			//	壁ずりベクトル算出
			if (!vertical)
			{
				//	水平パス時は法線調整
				Normal = DirectX::XMVectorSetY(Normal, 0.0f);
				DirectX::XMVECTOR length_sq = DirectX::XMVector3LengthSq(Normal);
				if (DirectX::XMVectorGetX(length_sq) > 1e-6f)
					Normal = DirectX::XMVector3Normalize(Normal);
				else
					Normal = DirectX::XMVectorZero();
			}
			MoveVector = DirectX::XMVectorScale(DirectX::XMVector3Normalize(MoveVector), distance);
			DirectX::XMVECTOR Dot = DirectX::XMVector3Dot(MoveVector, Normal);
			DirectX::XMVECTOR NormalComponent = DirectX::XMVectorScale(Normal, DirectX::XMVectorGetX(Dot) * 1.0005f);
			Velocity = DirectX::XMVectorSubtract(MoveVector, NormalComponent);
		}
		else
		{
			DirectX::XMVECTOR Position = DirectX::XMVectorAdd(From, Velocity);
			DirectX::XMStoreFloat3(&out_position, DirectX::XMVectorSubtract(Position, Offset));
			hit_flag = false;
			break;
		}
	}
	return hit_flag;
}
