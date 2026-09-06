#include "object/gameobjects/character/character_object.h"

namespace
{
	//	ワールド重力
	static constexpr DirectX::XMFLOAT3 world_gravity = {0.0f, system_units::convert_m_to_unit(-9.8f), 0.0f};
}

void character_object::fixed_update(float elapsed_time)
{
	//	時間遅延関係は除外して時間経過
	hit_stop_time = std::max(hit_stop_time - elapsed_time, 0.0f);
	invisile_timer = std::max(invisile_timer - elapsed_time, 0.0f);

	//	経過時間更新
	elapsed_time *= elapsed_time_scale;

	//	よろけ時間更新
	if (is_flag(flags::stagger))
	{
		stagger_recovery_time += elapsed_time;

		trunk = math::lerp(0, max_trunk, stagger_recovery_time / stagger_recovery_time_max);
		if (stagger_recovery_time >= stagger_recovery_time_max)
		{
			set_flag(flags::stagger, false);
			trunk = max_trunk;
		}
	}

	//	ヒットストップ
	get_model_object()->set_animation_speed(hit_stop_time <= FLT_EPSILON ? elapsed_time_scale : 0.0f);
	if (hit_stop_time > 0.0f)
		return;

	//	傾き破棄
	set_rotation({get_rotation().x, get_rotation().y, 0});

	//	各種フラグを落としておく
	set_flag(character_object::flags::invisible, false);
	set_flag(character_object::flags::guard, false);

	//	更新ステート呼び出し
	state_machine.update(elapsed_time);

	//	無敵処理
	if (invisile_timer > 0.0f)
	{
		set_flag(character_object::flags::invisible, true);
	}

	//	速度更新
	update_rotation(elapsed_time);
	update_vertical_velocity(elapsed_time);
	update_horizontal_velocity(elapsed_time);
	update_horizontal_move(elapsed_time);
	update_vertical_move(elapsed_time);

	//	ルートモーションの速度リセット
	{
		root_motion_velocity.x =
		root_motion_velocity.y =
		root_motion_velocity.z = 0.0f;
	}

	//	傾き適応
	{
		set_rotation({get_rotation().x, get_rotation().y, roll_value});
		roll_value *= 0.1f;
	}

	//	各種速度関係の情報はクリアしておく
	move_vector = {};
	accelaration = {};

	for (auto& box : attack_box)
	{
		if (!box.is_follow_self)
			continue;
		box.object->set_position(get_position());
		box.object->set_rotation(get_rotation());
	}
	for (auto& box : hit_box)
	{
		if (!box.is_follow_self)
			continue;
		box.object->set_position(get_position());
		box.object->set_rotation(get_rotation());
	}
}

void character_object::post_update(float elapsed_time)
{
	//	カメラからの透過処理を起動
	if (camera_distance_translucent_inner_flag == -1 ||
		is_flag(flags::camera_distance_translucent) != static_cast<bool>(camera_distance_translucent_inner_flag))
	{
		camera_distance_translucent_inner_flag = is_flag(flags::camera_distance_translucent);
		bool flag = is_flag(flags::camera_distance_translucent);
		//for (auto& material : get_model_object()->get_model_instance()->get_materials())
		//{
		//	material.set_flag(material::flags::camera_distance_translucent, flag);
		//}
		on_change_camera_translucent(flag);
	}
}

std::string_view character_object::get_current_state() const
{
	if (state_machine.get_current_state())
		return state_machine.get_current_state()->get_name();
	return "";
}

bool character_object::check_same_state(std::string_view state_name) const
{
	return get_current_state() == state_name;
}

bool character_object::change_state(std::string_view state_id)
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

bool character_object::register_change_state_callback(std::string_view state_id, std::string_view identifier, call_back_change_state callback)
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

bool character_object::unregister_change_state_callback(std::string_view state_id, std::string_view identifier)
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

void character_object::register_common_states()
{
#define REGISTER_MACRO(STATE) \
{ \
	register_state<function_finite_state_node>("state_"###STATE, \
												[this](){enter_##STATE();}, \
												[this](float elapsed_time) {update_##STATE(elapsed_time);}, \
												[this](){exit_##STATE();}); \
}
	REGISTER_MACRO(entry);
	REGISTER_MACRO(idle);
	REGISTER_MACRO(walk);
	REGISTER_MACRO(dash);
	REGISTER_MACRO(hi_dash);
	REGISTER_MACRO(move_stop);
	REGISTER_MACRO(dodge);
	REGISTER_MACRO(dodge_stop);
	REGISTER_MACRO(dodge_attack);

	REGISTER_MACRO(damage);
	REGISTER_MACRO(dead);
	REGISTER_MACRO(dead_complete);
	REGISTER_MACRO(regenerate);

	REGISTER_MACRO(stagger);
	REGISTER_MACRO(stagger_complete);

	REGISTER_MACRO(break_attack);

	REGISTER_MACRO(attack);
	REGISTER_MACRO(special);
	REGISTER_MACRO(deluxe);

	REGISTER_MACRO(guard);
	REGISTER_MACRO(parry);
	REGISTER_MACRO(parry_attack);
#undef REGISTER_MACRO
}

void character_object::update_rotation(float elapsed_time)
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

float character_object::get_air_drag_coefficient() const
{
	//	空気抵抗係数
	static constexpr float air_drag_coefficient = 0.02f;
	return air_drag_coefficient;
}

void character_object::update_vertical_velocity(float elapsed_time)
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

void character_object::update_vertical_move(float elapsed_time)
{
	const bool was_grounded = is_flag(flags::on_ground);

	ground_friction	= 1.0f;
	slope_rate = 1.0f;
	ground_normal = {};

	set_flag(flags::on_ground, false);

	DirectX::XMFLOAT3 vel = DirectX::XMFLOAT3(0.0f, velocity.y * elapsed_time + root_motion_velocity.y, 0.0f);

	DirectX::XMFLOAT3 position = get_position();
	bool settled = false;

	//	@tool 接地中は step_offset の範囲で settle を優先する（水平パスは Y を変更しないための追従。埋まり込んだ状態から真下へ通常の重力スイープを行うと「開始位置で既に接触している」ため距離0のヒットとして成立してしまい、高さが補正されないまま on_ground 扱いになる問題があったため、settle 側を先に試す）
	if (was_grounded && vel.y <= 0.0f)
	{
		auto collision_object = get_push_box();
		auto bounding_object = std::static_pointer_cast<collision::bounding_capsule>(collision_object->get_bounding_object());
		float capsule_height = bounding_object->get_radius() + bounding_object->get_length() * 0.5f;

		collision_world::capsule_cast_descs settle_desc;
		settle_desc.radius = bounding_object->get_radius();
		settle_desc.length = bounding_object->get_length();
		//	@tool 押し出し用カプセルと同じフィルタ設定
		if (is_flag(flags::invisible))
		{
			settle_desc.filter = collision_filter_manager::instance()->search_filter("invisible");
		}
		else
		{
			settle_desc.filter = collision_filter_manager::instance()->search_filter(collision_filter_name);
		}
		settle_desc.exclude_objects.emplace_back(get_push_box());

		static constexpr float SkinStep = system_units::convert_cm_to_unit(0.1f);

		DirectX::XMVECTOR Up = DirectX::XMVectorSet(0.0f, step_offset, 0.0f, 0.0f);
		DirectX::XMVECTOR SettleFrom = DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&position), DirectX::XMVectorSet(0.0f, capsule_height, 0.0f, 0.0f));
		SettleFrom = DirectX::XMVectorAdd(SettleFrom, Up);
		DirectX::XMVECTOR SettleTo = DirectX::XMVectorSubtract(SettleFrom, DirectX::XMVectorScale(Up, 2.0f));
		DirectX::XMStoreFloat3(&settle_desc.start, SettleFrom);
		DirectX::XMStoreFloat3(&settle_desc.end, SettleTo);

		collision::intersect_data settle_data;
		collision_object_ptr settle_object;
		if (object_manager::instance()->get_world().capsule_cast(settle_desc, settle_data, settle_object))
		{
			DirectX::XMVECTOR SettleNormal = DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&settle_data[0].normal));
			float settle_angle = acosf(math::clamp(DirectX::XMVectorGetY(SettleNormal), -1.0f, 1.0f));
			if (settle_angle < slope_limit)
			{
				float ground_y = DirectX::XMVectorGetY(SettleFrom) - settle_data[0].length - capsule_height;
				position.y = ground_y + SkinStep;

				set_flag(flags::on_ground, true);
				DirectX::XMStoreFloat3(&ground_normal, SettleNormal);
				footstep_position = position;
				slope_rate = ground_normal.y / ( sqrtf(ground_normal.x * ground_normal.x + ground_normal.z * ground_normal.z) + ground_normal.y);
				slope_rate = 1.0f - slope_rate;
				ground_friction = settle_object->get_physical_data().friction;
				velocity.y = 0.0f;
				settled = true;
			}
		}
	}

	//	@tool settle で見つからない場合（本物の落下・ジャンプなど）は通常の重力スイープ
	if (!settled)
	{
		DirectX::XMFLOAT3 normal;
		float fricition = 0;
		if (calculate_position(vel, true, position, normal, &fricition))
		{
			if (vel.y < 0)
			{
				set_flag(flags::on_ground, true);
				ground_normal = normal;
				footstep_position = position;
				slope_rate = normal.y / ( sqrtf(normal.x * normal.x + normal.z * normal.z) + normal.y);
				slope_rate = 1.0f - slope_rate;
				ground_friction	= fricition;
			}
			velocity.y = 0.0f;
		}
	}

	set_position(position);
}

void character_object::update_horizontal_velocity(float elapsed_time)
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

void character_object::update_horizontal_move(float elapsed_time)
{
	//	ルートモーションの速度加算
	DirectX::XMFLOAT3 vel = DirectX::XMFLOAT3(velocity.x * elapsed_time + root_motion_velocity.x, 0.0f, velocity.z * elapsed_time + root_motion_velocity.z);
	if (vel.x * vel.x + vel.z * vel.z <= 0.001f * 0.001f)
		return;

	//	座標算出
	DirectX::XMFLOAT3 new_position, normal;
	calculate_position(vel, false, new_position, normal);
	set_position(new_position);
}

void character_object::adjustment_velocity(DirectX::XMFLOAT2& move, float adjust_move_velocity_rate, bool* flip_flag)
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

		//	ターン処理
		if (flip_flag && dot < -0.5f)
		{
			*flip_flag = true;

			//	ターンさせる場合は今の移動方向に進めておく
			move.x = vx;
			move.y = vy;
		}
		else
		{
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
}

bool character_object::calculate_position(DirectX::XMFLOAT3 velocity, bool vertical, DirectX::XMFLOAT3& out_position, DirectX::XMFLOAT3& out_normal, float* out_friction)
{
	auto collision_object = get_push_box();
	auto bounding_object = std::static_pointer_cast<collision::bounding_capsule>(collision_object->get_bounding_object());

	//	初期座標
	out_position = get_position();

	//	オフセット位置を算出
	float height = bounding_object->get_radius() + bounding_object->get_length() * 0.5f;
	DirectX::XMVECTOR Offset = DirectX::XMVectorSet(0.0f, height, 0.0f, 0.0f);

	collision_world::capsule_cast_descs desc;
	desc.radius = bounding_object->get_radius();
	desc.length = bounding_object->get_length();
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
	for (size_t index = 0; index < 3; ++index)
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

		DirectX::XMStoreFloat3(&desc.start, From);
		DirectX::XMStoreFloat3(&desc.end, To);

		//	ヒット処理
		collision::intersect_data intersect_data;
		collision_object_ptr hit_collision_object;
		if (object_manager::instance()->get_world().capsule_cast(desc, intersect_data, hit_collision_object))
		{
			hit_flag = true;

			DirectX::XMVECTOR Normal = DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&intersect_data[0].normal));
			DirectX::XMStoreFloat3(&out_normal, Normal);

			if (!vertical)
			{
				const DirectX::XMVECTOR MoveDir = NormalizedVelocity;
				float probe_x = intersect_data[0].length + desc.radius + SkinStep;

				bool climbable = false;

				collision_world::capsule_cast_descs probe_desc = desc;
				probe_desc.radius = SkinStep;
				probe_desc.length = 0.0f;
				DirectX::XMVECTOR DownFrom = DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&out_position), DirectX::XMVectorSet(0.0f, step_offset + SkinStep, 0.0f, 0.0f));
				DownFrom = DirectX::XMVectorAdd(DownFrom, DirectX::XMVectorScale(MoveDir, probe_x));
				DirectX::XMVECTOR DownTo = DirectX::XMVectorSubtract(DownFrom, DirectX::XMVectorSet(0.0f, step_offset + SkinStep * 3.0f, 0.0f, 0.0f));
				DirectX::XMStoreFloat3(&probe_desc.start, DownFrom);
				DirectX::XMStoreFloat3(&probe_desc.end, DownTo);
				collision::intersect_data down_data;
				collision_object_ptr down_object;
				if (object_manager::instance()->get_world().capsule_cast(probe_desc, down_data, down_object))
				{
					DirectX::XMVECTOR DownNormal = DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&down_data[0].normal));
					float down_slope = acosf(math::clamp(DirectX::XMVectorGetY(DownNormal), -1.0f, 1.0f));
					float top_y = DirectX::XMVectorGetY(DownFrom) - down_data[0].length - probe_desc.radius;
					float rise = top_y - out_position.y;
					if (down_slope <= slope_limit && rise <= step_offset && rise > -SkinStep)
					{
						//	@tool 2) 実際に立つ高さ（step_offset の嵩上げ無し）で本来のカプセルを使い前方の空きを検証する。半径ぶん高い位置から probe_x 分掃引し、段の角にカプセルの丸い底面が触れて誤検知するのを防ぐ。薄い壁や本物の壁はここで弾く
						float landing_y = (rise > SkinStep) ? (top_y + SkinStep) : out_position.y;
						collision_world::capsule_cast_descs verify_desc = desc;
						DirectX::XMVECTOR VerifyFrom = DirectX::XMVectorSetY(From, landing_y + desc.radius + height);
						DirectX::XMVECTOR VerifyTo = DirectX::XMVectorAdd(VerifyFrom, DirectX::XMVectorScale(MoveDir, probe_x));
						DirectX::XMStoreFloat3(&verify_desc.start, VerifyFrom);
						DirectX::XMStoreFloat3(&verify_desc.end, VerifyTo);
						collision::intersect_data verify_data;
						collision_object_ptr verify_object;
						climbable = !object_manager::instance()->get_world().capsule_cast(verify_desc, verify_data, verify_object);
					}
				}

				if (climbable)
				{
					//	@tool 歩ける段差/坂と判断できたので壁扱いせず素通りさせる。Y はここでは変えず、水平移動をそのまま消費して終了（実際の高さ合わせは垂直パスに任せる）
					DirectX::XMVECTOR FullMove = DirectX::XMVectorAdd(From, DirectX::XMVectorScale(NormalizedVelocity, distance));
					DirectX::XMStoreFloat3(&out_position, DirectX::XMVectorSubtract(FullMove, Offset));
					hit_flag = false;
					break;
				}
			}

			//	@tool ヒット位置はサンプル非依存で算出：カプセル中心(From)を進行方向へ hit_distance 進めて Offset を引く。intersect_data[0].point は軸サンプル球（複数球のうち当たった1つ）の中心であり、カプセル中心ではないため、そこから直接 Offset を引くと Y がズレて埋まり込む原因になっていた
			float hit_distance = math::clamp(intersect_data[0].length - SkinStep, 0.0f, distance);
			DirectX::XMVECTOR Center = DirectX::XMVectorAdd(From, DirectX::XMVectorScale(NormalizedVelocity, hit_distance));
			DirectX::XMStoreFloat3(&out_position, DirectX::XMVectorSubtract(Center, Offset));

			if (out_friction)
			{
				//	摩擦係数を物理マテリアルから取得してくる
				*out_friction = hit_collision_object->get_physical_data().friction;
			}

			if (vertical)
			{
				float dot = DirectX::XMVectorGetX(DirectX::XMVector3Dot(Normal, DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)));
				float slope_angle = acosf(math::clamp(dot, -1.0f, 1.0f));
				if (slope_angle < slope_limit)
					break;
			}

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
				DirectX::XMVECTOR LengthSq = DirectX::XMVector3LengthSq(Normal);
				Normal = (DirectX::XMVectorGetX(LengthSq) > 1e-6f) ? DirectX::XMVector3Normalize(Normal) : DirectX::XMVectorZero();
			}
			MoveVector = DirectX::XMVectorScale(DirectX::XMVector3Normalize(MoveVector), distance);
			DirectX::XMVECTOR Dot = DirectX::XMVector3Dot(MoveVector, Normal);
			DirectX::XMVECTOR NormalComponent = DirectX::XMVectorScale(Normal, DirectX::XMVectorGetX(Dot) * 1.005f);
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