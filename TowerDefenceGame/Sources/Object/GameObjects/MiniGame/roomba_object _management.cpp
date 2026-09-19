#include "object/gameobjects/MiniGame/roomba_object.h"

namespace
{
	//	入力・停止判定の閾値
	constexpr float move_input_dead_zone = 0.01f;
	constexpr float move_input_dead_zone_sq = move_input_dead_zone * move_input_dead_zone;

	constexpr float rotation_input_dead_zone = 0.001f;
	constexpr float rotation_input_dead_zone_sq = rotation_input_dead_zone * rotation_input_dead_zone;

	constexpr float horizontal_move_stop_epsilon = 0.001f;
	constexpr float horizontal_move_stop_epsilon_sq = horizontal_move_stop_epsilon * horizontal_move_stop_epsilon;

	constexpr float action_move_input_dead_zone = 0.1f;
	constexpr float action_move_input_dead_zone_sq = action_move_input_dead_zone * action_move_input_dead_zone;

	//	傾き関連
	constexpr float ground_tilt_follow_rate = 15.0f;
	constexpr float velocity_tilt_follow_rate = 30.0f;
	constexpr float velocity_roll_max = DirectX::XMConvertToRadians(+20.0f);
	constexpr float velocity_pitch_max = DirectX::XMConvertToRadians(20.0f);

	//	死亡加速
	constexpr float death_spin_speed = DirectX::XMConvertToRadians(720.0f);
	constexpr DirectX::XMFLOAT3 dead_impulse = system_units::convert_m_to_unit({2.0f, 10.0f, 2.0f});

	//	力 > 加速度
	float convert_force_to_acceleration(float force, float mass)
	{
		return force / mass;
	}

	//	地面の傾きを角度で算出
	DirectX::XMFLOAT3 calculate_ground_tilt(const DirectX::XMFLOAT3& euler, const DirectX::XMFLOAT3& normal)
	{
		DirectX::XMFLOAT3 front = { sinf(euler.y), 0.0f, +cosf(euler.y) };
		DirectX::XMFLOAT3 right = { cosf(euler.y), 0.0f, -sinf(euler.y) };

		float forward_component = normal.x * front.x + normal.z * front.z;
		float right_component   = normal.x * right.x + normal.z * right.z;

		DirectX::XMFLOAT3 out;
		out.x = atan2f(forward_component, normal.y);
		out.y = euler.y;
		out.z = asinf(math::clamp(-right_component, -1.0f, 1.0f));
		return out;
	}

	//	速度ベクトルから前後・左右の傾き角を算出
	DirectX::XMFLOAT2 calculate_velocity_tilt(float yaw, const DirectX::XMFLOAT3& velocity, float max_speed, bool flip_pitch = false)
	{
		DirectX::XMFLOAT2 front = { sinf(yaw), +cosf(yaw) };
		DirectX::XMFLOAT2 right = { cosf(yaw), -sinf(yaw) };

		float forward_speed = velocity.x * front.x + velocity.z * front.y;
		float lateral_speed = velocity.x * right.x + velocity.z * right.y;

		float forward_rate = (max_speed > FLT_EPSILON) ? math::clamp(forward_speed / max_speed, -1.0f, 1.0f) : 0.0f;
		float lateral_rate = (max_speed > FLT_EPSILON) ? math::clamp(lateral_speed / max_speed, -1.0f, 1.0f) : 0.0f;

		DirectX::XMFLOAT2 out;
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
	update_vertical_velocity(elapsed_time);
	update_horizontal_velocity(elapsed_time);
	update_horizontal_move(elapsed_time);
	update_vertical_move(elapsed_time);

	//	各種ライフ情報更新
	{
		//	水分量に寄る死亡判定
		if (!check_same_state(state_names[state_dead]) && get_wet_rate() >= 1.0f)
		{
			change_state(state_names[state_dead]);
		}

		//	熱量/水分量更新
		update_values(elapsed_time);

		//	熱量に寄る死亡判定
		if (!check_same_state(state_names[state_dead]) && get_heat_rate() >= 1.0f)
		{
			change_state(state_names[state_dead]);
		}
	}

	//	傾き適応
	if (check_same_state(state_names[state_dead]))
	{
		//	死亡時はスピンさせる
		DirectX::XMVECTOR Velocity = DirectX::XMLoadFloat3(&get_velocity());
		float speed = DirectX::XMVectorGetX(DirectX::XMVector3Length(Velocity));
		if (speed >= 0.001f)
		{
			Velocity = DirectX::XMVector3Normalize(Velocity);
			DirectX::XMVECTOR UpVector = DirectX::XMVectorNegate(DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&world_gravity)));
			UpVector = DirectX::XMVector3Cross(Velocity, UpVector);

			if (DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(UpVector)) > 0.00001f)
			{
				death_spin_angle = speed;

				DirectX::XMVECTOR Rotation = DirectX::XMLoadFloat4(&death_spin_rotator.convert_quaternion());
				DirectX::XMVECTOR AddRotation = DirectX::XMQuaternionRotationAxis(UpVector, death_spin_angle);
				Rotation = DirectX::XMQuaternionMultiply(Rotation, AddRotation);

				DirectX::XMFLOAT4 rot;
				DirectX::XMStoreFloat4(&rot, Rotation);
				math::rotator new_rot = rot;
				new_rot.y -= DirectX::XM_PIDIV2;	//	補正
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
			DirectX::XMFLOAT2 velocity_tilt_target = calculate_velocity_tilt(get_rotation().y, get_velocity(), max_speed, flip_flag);

			float velocity_tilt_alpha = 1.0f - expf(-velocity_tilt_follow_rate * elapsed_time);
			velocity_tilt_pitch += (velocity_tilt_target.x - velocity_tilt_pitch) * velocity_tilt_alpha;
			velocity_tilt_roll  += (velocity_tilt_target.y - velocity_tilt_roll) * velocity_tilt_alpha;
		}

		//	地面の法線から傾き算出
		{
			DirectX::XMFLOAT3 ground_tilt = calculate_ground_tilt(get_rotation(), ground_normal);

			float ground_tilt_alpha = 1.0f - expf(-ground_tilt_follow_rate * elapsed_time);
			ground_tilt_pitch += (ground_tilt.x - ground_tilt_pitch) * ground_tilt_alpha;
			ground_tilt_roll  += (ground_tilt.z - ground_tilt_roll) * ground_tilt_alpha;
		}

		DirectX::XMFLOAT3 rot = get_rotation();
		//	@remainder	モデルの問題で設定を変えている
		rot.x = ground_tilt_roll + velocity_tilt_roll;
		rot.z = ground_tilt_pitch + velocity_tilt_pitch;
		set_rotation(rot);
	}

	//	各種速度関係の情報はクリアしておく
	move_vector = {};

	//	アクション情報クリア
	action_data = {};
}

void roomba_object::post_update(float elapsed_time)
{
	update_effects(elapsed_time);
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

bool roomba_object::input_move(float elapsed_time)
{
	DirectX::XMFLOAT2 move = DirectX::XMFLOAT2(action_data.move.x, action_data.move.z);

	add_move_vector(move);
	set_turn_vector(move);

	return move.x != 0 || move.y != 0;
}

void roomba_object::register_states()
{
#define REGISTER_MACRO(STATE) \
	register_state<function_finite_state_node>("state_"###STATE, \
												[this](){enter_##STATE();}, \
												[this](float elapsed_time) {update_##STATE(elapsed_time);}, \
												[this](){exit_##STATE();});
	REGISTER_MACRO(idle);
	REGISTER_MACRO(walk);
	REGISTER_MACRO(dash);
	REGISTER_MACRO(move_stop);
	REGISTER_MACRO(dead);
#undef REGISTER_MACRO
}

void roomba_object::enter_idle()
{
	//	何もしない
}

void roomba_object::update_values(float elapsed_time)
{
	float elapsed_frame = system_units::convert_sec_to_fps(elapsed_time);

	//	速度から活動量と、空気抵抗による放熱・乾燥を算出
	DirectX::XMVECTOR Velocity = DirectX::XMLoadFloat3(&get_velocity());
	float speed = DirectX::XMVectorGetX(DirectX::XMVector3Length(Velocity));
	float speed_rate = math::saturate(speed / get_max_speed(move_mode::dash));
	float activity_rate = math::lerp(-1.0f, 2.0f, speed_rate);
	float drag_rate = speed_rate * speed_rate;

	//	単純な式
	float base_wet_value = 10.0f;
	float base_heat_value = 50.0f;
	float wind_cooling_factor = 1.0f;
	float wind_drying_factor = 1.0f;
	float heat_rate = activity_rate * elapsed_frame - get_wet_rate() - drag_rate * wind_cooling_factor;
	float wet_rate = -fabsf(activity_rate) * elapsed_frame - get_heat_rate() - drag_rate * wind_drying_factor;
	set_heat_value(get_heat_value() + static_cast<int>(base_heat_value * heat_rate));
	set_wet_value(get_wet_value() + static_cast<int>(base_wet_value * wet_rate));
}

void roomba_object::update_idle(float elapsed_time)
{
	//	入力
	if (input_action(elapsed_time))
	{
		return;
	}

	//	移動処理
	if (input_move(elapsed_time))
	{
		if (action_data.dash)
		{
			change_state(roomba_object::state_names[state_dash]);
		}
		else
		{
			change_state(roomba_object::state_names[state_walk]);
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
	if (input_move(elapsed_time))
	{
		if (action_data.dash)
			change_state(roomba_object::state_names[state_dash]);
	}
}

void roomba_object::exit_walk()
{
}

void roomba_object::enter_dash()
{
	dash_effect_instance_node_index[0] = dash_effect_instance->search_node_index_by_name("smoke left");
	dash_effect_instance_node_index[1] = dash_effect_instance->search_node_index_by_name("smoke right");
}

void roomba_object::update_dash(float elapsed_time)
{
	//	移動
	if (!input_move(elapsed_time))
	{
		change_state(state_names[state_move_stop]);
		return;
	}

	//	ダッシュ状態じゃなくなれば戻す
	if (!action_data.dash)
		change_state(state_names[state_walk]);
}

void roomba_object::exit_dash()
{
	dash_effect_instance_node_index[0] = effect_instance::invalid_id;
	dash_effect_instance_node_index[1] = effect_instance::invalid_id;
}

void roomba_object::enter_move_stop()
{
}

void roomba_object::update_move_stop(float elapsed_time)
{
	//	待機処理も利用
	update_idle(elapsed_time);
}

void roomba_object::exit_move_stop()
{
}

void roomba_object::enter_dead()
{
	DirectX::XMFLOAT3 vel = get_velocity();
	vel.x *= dead_impulse.x;
	vel.y += dead_impulse.y;
	vel.z *= dead_impulse.z;
	add_impulse(vel);

	//	死亡情報取得
	death_spin_angle = 0.0f;
	death_spin_rotator = get_rotation();

	//	爆発させる
	auto effect = play_effect(static_cast<size_t>(effect_id::dead)).lock();
	if (effect)
	{
		effect->set_position(get_position());
	}
}

void roomba_object::update_dead(float elapsed_time)
{
	if (keyboard::instance()->get_state().is_key_state(keyboard::key_states::a) == input::state::down)
	{
		set_wet_value(0);
		set_heat_value(0);
		change_state(state_names[state_idle]);
	}
}

void roomba_object::exit_dead()
{
}

void roomba_object::on_wet(float rate)
{
	set_wet_value(get_wet_value() + rate);
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
		auto right = get_rotation().get_right();
		if (dash_effect_instance_node_index[0] != effect_instance::invalid_id)
		{
			auto& node = nodes[dash_effect_instance_node_index[0]];
			node.rotation = get_rotation();
			node.position.x = get_position().x - right.x * 1.5f;
			node.position.y = get_position().y - right.y * 1.5f;
			node.position.z = get_position().z - right.z * 1.5f;
		}
		if (dash_effect_instance_node_index[1] != effect_instance::invalid_id)
		{
			auto& node = nodes[dash_effect_instance_node_index[1]];
			node.position = get_position();
			node.rotation = get_rotation();
			node.position.x = get_position().x + right.x * 1.5f;
			node.position.y = get_position().y + right.y * 1.5f;
			node.position.z = get_position().z + right.z * 1.5f;
		}
	}

	//	漏電によるスパーク対応
	if (spark_effect_instance)
	{
		spark_effect_instance->set_flag(effect_instance::flags::spawn_stop, get_wet_rate() >= 0.5f);
		spark_effect_instance->set_position(get_position());
		float scale = get_wet_rate() * 1.5f;
		spark_effect_instance->set_scale({scale, scale, scale});
	}

	//	熱による煙対応
	if (spark_smoke_effect_instance)
	{
		spark_smoke_effect_instance->set_flag(effect_instance::flags::spawn_stop, get_heat_rate() >= 0.5f);
		spark_smoke_effect_instance->set_position(get_position());
		float scale = get_heat_rate() * 4.0f;
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
	bool input_move = move_length > move_input_dead_zone_sq;

	//	加速処理
	if (input_move)
	{
		float move_vector_x = move_vector.x;
		float move_vector_z = move_vector.y;
		move_length = sqrtf(move_length);
		move_vector_x /= move_length;
		move_vector_z /= move_length;

		auto accelaration_force = get_current_accelaration_force() * elapsed_time;
		DirectX::XMFLOAT3 drive_force = {move_vector_x * accelaration_force, 0, move_vector_z * accelaration_force};
		add_force(drive_force);
	}

	//	摩擦処理
	if (is_flag(flags::on_ground))
	{
		//	垂直抗力
		float slope_cos = math::clamp(ground_normal.y, 0.0f, 1.0f);
		float gravity_y = frame_gravity.y;
		float normal_force = fabsf(gravity_y) * get_push_box()->get_mass() * slope_cos;

		//	合成摩擦係数
		float total_friction = (ground_friction + get_current_friction()) * 0.5f;

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
	{
		float breaking_force = get_current_breaking_deceleration_force() * elapsed_time;
		float breaking_acceleration = convert_force_to_acceleration(breaking_force, get_push_box()->get_mass());

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
		if (speed_sq > get_current_max_speed() * get_current_max_speed())
		{
			float scale = get_current_max_speed() / sqrtf(speed_sq);
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
	if (vel.x * vel.x + vel.z * vel.z <= horizontal_move_stop_epsilon_sq)
		return;

	//	座標算出
	DirectX::XMFLOAT3 new_position, normal;
	calculate_position(vel, false, new_position, normal);
	set_position(new_position);
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
