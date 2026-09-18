#include "object/gameobjects/MiniGame/roomba_object.h"

namespace setup_data
{
	constexpr DirectX::XMFLOAT3 world_gravity = {0.0f, system_units::convert_m_to_unit(-9.8f), 0.0f};	//	ワールド重力

	constexpr int health = 15000; // 体力
	constexpr float mass = 4.0f; // 重さ

	constexpr float capsule_push_radius = system_units::convert_m_to_unit(0.4f); // 押し合い判定半径
	constexpr float capsule_length = system_units::convert_m_to_unit(0.1f); // 当たり判定長さ

	constexpr DirectX::XMFLOAT3 capsule_offset = {0.0f, capsule_length + capsule_push_radius, 0.0f}; // 当たり判定長さオフセット

	//	普通ルンバモード
	constexpr float walk_accelaration = system_units::convert_m_to_unit(6.0f); // 歩きの加速度(m/s2)
	constexpr float walk_max_speed = system_units::convert_m_to_unit(0.5f); // 歩きの速度(m/s)
	constexpr float walk_breaking_deceleration = system_units::convert_m_to_unit(0.0f); // 歩きの減速度(m/s2)

	//	爆速ルンバモード
	constexpr float dash_accelaration = system_units::convert_m_to_unit(18.0f); // ダッシュ加速度(m/s2)
	constexpr float dash_max_speed = system_units::convert_m_to_unit(16.0f); // ダッシュ速度(m/s)
	constexpr float dash_breaking_deceleration = system_units::convert_m_to_unit(0.0f); // ダッシュ減速度(m/s2)

	//	停止時の減速力
	constexpr float move_stop_breaking_deceleration = system_units::convert_m_to_unit(6.0f); // ブレーキング減速力(m/s2)

	//	性能低下
	constexpr float health_max_speed = system_units::convert_m_to_unit(64.0f); // 歩きの速度(m/s)
	constexpr float health_breaking_deceleration = system_units::convert_m_to_unit(0.0f); // ブレーキング減速力(m/s2)

	//	摩擦係数
	constexpr float base_friction = 0.995f;	// 摩擦係数(ゴム靴？)
	constexpr float wet_friction = 0.01f;	// 摩擦係数(めちゃくちゃ滑りやすくする)
	constexpr float dead_friction = 10.0f;	// 摩擦係数
}

roomba_object::roomba_object(std::string_view name, std::shared_ptr<model_object> model, std::string_view collision_filter_name)
: game_object(name)
, model(model)
, flag_data(0)
, collision_filter_name(std::string(collision_filter_name))

, health(setup_data::health)
, max_health(setup_data::health)

, push_box()

, move_vector(0.0f, 0.0f)
, turn_front_vector(0.0f, 0.0f)
, velocity(0.0f, 0.0f, 0.0f)
, world_gravity(setup_data::world_gravity)
, gravity_scale(1.0f)

, rotation_speed(DirectX::XMConvertToRadians(1440.0f))

, velocity_tilt_pitch(0.0f)
, velocity_tilt_roll(0.0f)
, ground_tilt_pitch (0.0f)
, ground_tilt_roll(0.0f)

, slope_rate(1.0f)
, step_offset(system_units::convert_m_to_unit(10.0f))
, slope_limit(DirectX::XMConvertToRadians(30.0f))
, ground_friction(0.5f)
{
	set_flag(flags::movable, true);
	set_flag(flags::on_ground, true);

	//	基本サイズ調整
	set_scale({0.2f, 0.2f, 0.2f});

	//	当たり判定の登録
	push_box = std::make_shared<collision_object>();
	push_box->set_filter(collision_filter_name);
	push_box->set_bounding_object(std::make_shared<collision::bounding_capsule>());
	push_box->set_local_scale({setup_data::capsule_push_radius, setup_data::capsule_length, 0.0f});
	push_box->set_local_offset(setup_data::capsule_offset);
	push_box->set_physical_data({setup_data::base_friction});
	push_box->set_mass(setup_data::mass);
	push_box->set_draw_normal_color({0.0f, 1.0f, 0.0f, 1.0f});
	push_box->set_collision_enter([this](collision_object::data data) {on_push_box_enter(data);});
	push_box->set_collision_stay([this](collision_object::data data) {on_push_box_stay(data);});
	object_manager::instance()->register_object(push_box);

	set_elapsed_time_scale(1.0f);

	//	ステートの登録
	register_states();

	//	エフェクトの登録
	register_effects();

	//	起動時は待機ステート
	change_state(roomba_object::state_names[state_idle]);
}

roomba_object::~roomba_object()
{
}

void roomba_object::on_finalize()
{
	unregister_effects();

	object_manager::instance()->unregister_object(push_box);
	object_manager::instance()->unregister_object(model);
	model.reset();
}

#if defined(USE_IMGUI)
#define CHECK_BOX(FLAG) \
{\
	bool is_##FLAG = is_flag(flags::##FLAG);\
	ImGui::Checkbox(#FLAG, &is_##FLAG);\
	set_flag(flags::##FLAG, is_##FLAG);\
}

void roomba_object::imgui_render_core()
{
	float degrees;
	CHECK_BOX(movable);
	CHECK_BOX(on_ground);
	{
		std::string current_state_name = "none";
		if (state_machine.get_current_state())
			current_state_name = state_machine.get_current_state()->get_name();
		ImGui::Text("current_state : %s", current_state_name.data(), current_state_name.size());
	}
	ImGui::DragInt("health", &health);
	ImGui::DragInt("max_health", &max_health);

	{
		auto scale = get_scale();
		auto rotator = get_rotation();
		auto position = get_position();
		ImGui::DragFloat3("scale", &scale.x, 0.01f);
		ImGui::DragFloat3("rotation", &rotator.x, 0.01f);
		ImGui::DragFloat3("position", &position.x, 0.01f);
		set_scale(scale);
		set_rotation(rotator);
		set_position(position);
	}

	ImGui::DragFloat3("velocity", &velocity.x, 0.01f);

	degrees = DirectX::XMConvertToDegrees(rotation_speed);
	ImGui::DragFloat("rotation_speed", &degrees, 0.01f);
	rotation_speed = DirectX::XMConvertToRadians(degrees);

	ImGui::DragFloat("gravity_scale", &gravity_scale, 0.01f);
	ImGui::DragFloat("step_offset", &step_offset, 0.01f);

	degrees = DirectX::XMConvertToDegrees(slope_limit);
	ImGui::DragFloat("slope_limit", &degrees, 0.01f);
	slope_limit = DirectX::XMConvertToRadians(degrees);

	if (push_box)
	{
		if (ImGui::TreeNode("push_box"))
		{
			push_box->imgui_render();

			ImGui::TreePop();
		}
	}
}
#undef CHECK_BOX
#endif	//	defined(USE_IMGUI)

const std::shared_ptr<model_object>& roomba_object::get_model_object() const
{
	return model;
}

void roomba_object::set_model_object(std::shared_ptr<model_object> model)
{
	this->model = model;
}

const collision_object_ptr& roomba_object::get_push_box() const
{
	return push_box;
}

const DirectX::XMFLOAT3& roomba_object::get_scale() const
{
	return model->get_scale();
}

void roomba_object::set_scale(const DirectX::XMFLOAT3& scale)
{
	model->set_scale(scale);
}

math::rotator roomba_object::get_rotation() const
{
	auto rotator = model->get_rotation();
	rotator.y -= DirectX::XM_PIDIV2;
	return rotator;
}

void roomba_object::set_rotation(math::rotator rotator)
{
	rotator.y += DirectX::XM_PIDIV2;
	model->set_rotation(rotator);
	if (push_box)
		push_box->set_rotation(rotator);
}

float roomba_object::get_rotation_speed() const
{
	return rotation_speed;
}

void roomba_object::set_rotation_speed(float rotation_speed)
{
	this->rotation_speed = rotation_speed;
}

const DirectX::XMFLOAT3& roomba_object::get_position() const
{
	return model->get_position();
}

void roomba_object::set_position(const DirectX::XMFLOAT3& position)
{
	model->set_position(position);
	if (push_box)
		push_box->set_position(position);
}

DirectX::XMFLOAT3 roomba_object::get_focus_point() const
{
	auto transform = push_box->get_bounding_object()->get_transform_float4x4();
	return {transform._41, transform._42, transform._43};
}

void roomba_object::add_move_vector(DirectX::XMFLOAT2 move_vector)
{
	this->move_vector = move_vector;
}

void roomba_object::set_turn_vector(DirectX::XMFLOAT2 direction)
{
	this->turn_front_vector = direction;
}

DirectX::XMFLOAT2 roomba_object::get_turn_vector() const
{
	return this->turn_front_vector;
}

DirectX::XMFLOAT3 roomba_object::get_velocity() const
{
	return velocity;
}

void roomba_object::set_velocity(DirectX::XMFLOAT3 velocity)
{
	this->velocity = velocity;
}

float roomba_object::get_gravity_scale() const
{
	return gravity_scale;
}

void roomba_object::set_gravity_scale(float gravity_scale)
{
	this->gravity_scale = gravity_scale;
}

float roomba_object::get_step_offset() const
{
	return step_offset;
}

void roomba_object::set_step_offset(float step_offset)
{
	this->step_offset = step_offset;
}

int roomba_object::get_health() const
{
	return health;
}

void roomba_object::set_health(int health)
{
	this->health = math::clamp(health, 0, max_health);
}

int roomba_object::get_max_health() const
{
	return max_health;
}

void roomba_object::set_max_health(int max_health)
{
	this->max_health = max_health;
	//	最大値が減ったら減らす
	if (this->health > this->max_health)
	{
		this->health = this->max_health;
	}
}

float roomba_object::get_elapsed_time_scale() const
{
	return elapsed_time_scale;
}

void roomba_object::set_elapsed_time_scale(float elapsed_time_scale)
{
	this->elapsed_time_scale = elapsed_time_scale;
	if (get_model_object())
	{
		get_model_object()->set_animation_speed(elapsed_time_scale);
	}
}

void roomba_object::add_force(DirectX::XMFLOAT3 force)
{
	//	時間で積分済みの力を加速度へ
	auto mass = get_push_box()->get_mass();
	velocity.x += force.x / mass;
	velocity.y += force.y / mass;
	velocity.z += force.z / mass;
}

void roomba_object::add_impulse(DirectX::XMFLOAT3 delta_velocity)
{
	//	単純に速度に加算する
	velocity.x += delta_velocity.x;
	velocity.y += delta_velocity.y;
	velocity.z += delta_velocity.z;
}

float roomba_object::get_current_accelaration_force() const
{
	return get_accelaration_force(check_same_state(roomba_object::state_names[roomba_object::state_dash]) ? move_mode::dash : move_mode::walk);
}

float roomba_object::get_current_max_speed() const
{
	float max_speed = get_max_speed(check_same_state(roomba_object::state_names[roomba_object::state_dash]) ? move_mode::dash : move_mode::walk);
	//	ライフによる摩擦係数減衰
	{
		float life_rate = math::saturate(static_cast<float>(health) / max_health);
		life_rate = math::ease::in_out_expo(life_rate);
		max_speed = math::lerp(setup_data::health_max_speed, max_speed, life_rate);
	}
	return max_speed;
}

float roomba_object::get_current_breaking_deceleration_force() const
{
	float deceleration = setup_data::walk_breaking_deceleration;

	auto hash_lkey = system_utility::generate_hash(get_current_state());
	switch (hash_lkey)
	{
		case system_utility::generate_hash(roomba_object::state_names[roomba_object::state_dash]):
		{
			deceleration = setup_data::dash_breaking_deceleration;
			break;
		}
		case system_utility::generate_hash(roomba_object::state_names[roomba_object::state_move_stop]):
		{
			deceleration = setup_data::move_stop_breaking_deceleration;
			break;
		}
	}

	//	ライフによる減速力変動
	{
		float life_rate =  math::saturate(static_cast<float>(health) / max_health);
		life_rate = math::ease::in_out_expo(life_rate);
		deceleration = math::lerp(setup_data::health_breaking_deceleration, deceleration, life_rate);
	}

	return push_box->get_mass() * deceleration;
}

float roomba_object::get_current_friction() const
{
	//	基本摩擦係数
	float friction = setup_data::base_friction;

	if (check_same_state(state_names[state_dead]))
	{
		//	死亡時は急激に減速させる
		friction = setup_data::dead_friction;
	}
	else
	{
		//	ライフによる摩擦係数減衰
		float life_rate = math::saturate(static_cast<float>(health) / max_health);
		life_rate = math::ease::in_out_expo(life_rate);
		friction = math::lerp(setup_data::wet_friction, friction, life_rate);
	}
	return friction;
}

float roomba_object::get_accelaration_force(move_mode mode) const
{
	float accelaration = 0.0f;
	switch (mode)
	{
		case move_mode::walk: accelaration = setup_data::walk_accelaration;	break;
		case move_mode::dash: accelaration = setup_data::dash_accelaration;	break;
	}
	return push_box->get_mass() * accelaration;
}

float roomba_object::get_max_speed(move_mode mode) const
{
	float max_speed = 0.0f;
	switch (mode)
	{
		case move_mode::walk: max_speed = setup_data::walk_max_speed;	break;
		case move_mode::dash: max_speed = setup_data::dash_max_speed;	break;
	}
	return max_speed;
}

float roomba_object::get_air_drag_coefficient() const
{
	//	空気抵抗係数
	static constexpr float air_drag_coefficient = 0.02f;
	return air_drag_coefficient;
}

void roomba_object::generate_box_data(std::vector<bounding_inner_datas>& datas, const bounding_datas& bounding_data)
{
	datas.emplace_back();
	auto& data = datas.back();
	data.is_follow_self = bounding_data.desc.is_follow_self;

	data.object = std::make_shared<collision_object>();
	switch (bounding_data.desc.type)
	{
		case collision::bounding_object::types::box:
		{
			data.object->set_bounding_object(std::make_shared<collision::bounding_box>());
			break;
		}
		case collision::bounding_object::types::capsule:
		{
			data.object->set_bounding_object(std::make_shared<collision::bounding_capsule>());
			break;
		}
		case collision::bounding_object::types::sphere:
		{
			data.object->set_bounding_object(std::make_shared<collision::bounding_sphere>());
			break;
		}
	}

#if defined(USE_IMGUI)
	data.object->set_name(get_name());
#endif	//	defined(USE_IMGUI)
	data.object->set_filter(collision_filter_name);
	data.object->set_local_scale(bounding_data.desc.local_scale);
	data.object->set_local_rotation(bounding_data.desc.local_rotator);
	data.object->set_local_offset(bounding_data.desc.local_offset);
	data.object->set_rotation(bounding_data.rotator);
	data.object->set_position(bounding_data.position);
	data.object->set_flag(collision_object::flags::detect_only, true);
}

void roomba_object::on_push_box_enter(collision_object::data data)
{
	//	座標系を再設定
	if (!data.from->is_flag(collision_object::flags::detect_only))
		set_position(push_box->get_position());
}

void roomba_object::on_push_box_stay(collision_object::data data)
{
	//	座標系を再設定
	if (!data.from->is_flag(collision_object::flags::detect_only))
		set_position(push_box->get_position());
}
