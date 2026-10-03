#include "object/gameobjects/MiniGame/roomba_object.h"

namespace setup_data
{
	constexpr auto world_gravity = DirectX::XMFLOAT3(0.0f, system_units::convert_m_to_unit(-9.8f), 0.0f);	//	ƒ[ƒ‹ƒhd—Í(m/s2)
	constexpr float world_air_density = 1.225f / 1000.0f;	// ƒ[ƒ‹ƒh‹óŠÔ‚Ì‹ó‹C–§“x(kg/m^3)

	constexpr int wet_value = 10000; // ‘Ï…«”\
	constexpr int heat_value = 10000; // ‘Ï”M«”]

	constexpr float mass = system_units::convert_kg_to_unit(4.0f); // d‚³(kg)

	constexpr float capsule_push_radius = system_units::convert_m_to_unit(0.4f); // ‰Ÿ‚µ‡‚¢”»’è”¼Œa
	constexpr float capsule_length = system_units::convert_m_to_unit(0.1f); // “–‚½‚è”»’è’·‚³

	constexpr DirectX::XMFLOAT3 capsule_offset = {0.0f, capsule_length + capsule_push_radius, 0.0f}; // “–‚½‚è”»’è’·‚³ƒIƒtƒZƒbƒg

	//	‹zˆøÅ‘å”ÍˆÍ
	constexpr DirectX::XMFLOAT2 attract_radius_range = {system_units::convert_m_to_unit(1.0f), system_units::convert_m_to_unit(10.0f)};

	//	ƒ‹ƒ“ƒo‚ÌŒ`óî•ñ
	constexpr float drag_coefficient = 0.45f;	// ƒ‹ƒ“ƒo‚ÌR—ÍŒW”Cd
	constexpr float diameter = system_units::convert_m_to_unit(0.34f);	// ƒ‹ƒ“ƒo‚Ì’¼Œa(m)
	constexpr float height = system_units::convert_m_to_unit(0.092f);	// ƒ‹ƒ“ƒo‚Ì‚‚³(m)

	//	•’Êƒ‹ƒ“ƒoƒ‚[ƒh
	constexpr float walk_accelaration = system_units::convert_m_to_unit(14.0f); // •à‚«‚Ì‰Á‘¬“x(m/s2)
	constexpr float walk_max_speed = system_units::convert_m_to_unit(0.75f); // •à‚«‚Ì‘¬“x(m/s)

	//	”š‘¬ƒ‹ƒ“ƒoƒ‚[ƒh
	constexpr float dash_accelaration = system_units::convert_m_to_unit(18.0f); // ƒ_ƒbƒVƒ…‰Á‘¬“x(m/s2)
	constexpr float dash_max_speed = system_units::convert_m_to_unit(16.67f); // ƒ_ƒbƒVƒ…‘¬“x(m/s)

	//	ƒuƒŒ[ƒL
	constexpr float braking_deceleration = system_units::convert_m_to_unit(15.0f); // ƒuƒŒ[ƒLŒ¸‘¬“x(m/s2)

	//	–€CŒW”
	constexpr float base_friction = 2.5f;	// –€CŒW”
	constexpr float wet_friction = 0.01f;	// –€CŒW”(‚ß‚¿‚á‚­‚¿‚áŠŠ‚è‚â‚·‚­‚·‚é)
	constexpr float dead_friction = 80.0f;	// –€CŒW”
}

roomba_object::roomba_object(std::string_view name, std::shared_ptr<model_object> model, std::string_view collision_filter_name)
: game_object(name)
, model(model)
, flag_data(0)
, collision_filter_name(std::string(collision_filter_name))

, wet_value(0)
, max_wet_value(setup_data::wet_value)
, heat_value(0)
, max_heat_value(setup_data::heat_value)

, push_box()

, move_vector(0.0f, 0.0f)
, turn_front_vector(0.0f, 0.0f)
, velocity(0.0f, 0.0f, 0.0f)
, world_gravity(setup_data::world_gravity)
, world_air_density(setup_data::world_air_density)
, gravity_scale(1.0f)

, rotation_speed(DirectX::XMConvertToRadians(1440.0f))

, velocity_tilt_pitch(0.0f)
, velocity_tilt_roll(0.0f)
, ground_tilt_pitch (0.0f)
, ground_tilt_roll(0.0f)

, step_offset(system_units::convert_m_to_unit(10.0f))
, slope_limit(DirectX::XMConvertToRadians(30.0f))
, ground_friction(0.5f)
{
	set_flag(flags::movable, true);
	set_flag(flags::on_ground, true);

	//	Šî–{ƒTƒCƒY’²®
	set_scale({0.2f, 0.2f, 0.2f});

	//	“–‚½‚è”»’è‚Ì“o˜^
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

	//	ƒXƒe[ƒg‚Ì“o˜^
	register_states();

	//	ƒGƒtƒFƒNƒg‚Ì“o˜^
	register_effects();

	//	‹N“®‚Í‘Ò‹@ƒXƒe[ƒg
	change_state(roomba_object::state_names[state_active]);
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
	ImGui::DragInt("wet_value", &wet_value);
	ImGui::DragInt("max_wet_value", &max_wet_value);
	ImGui::DragInt("heat_value", &heat_value);
	ImGui::DragInt("max_heat_value", &max_heat_value);

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

int roomba_object::get_wet_value() const
{
	return wet_value;
}

void roomba_object::set_wet_value(int wet_value)
{
	this->wet_value = math::clamp(wet_value, 0, max_wet_value);
}

int roomba_object::get_max_wet_value() const
{
	return max_wet_value;
}

void roomba_object::set_max_wet_value(int max_wet_value)
{
	this->max_wet_value = max_wet_value;
	//	Å‘å’l‚ªŒ¸‚Á‚½‚çŒ¸‚ç‚·
	if (this->wet_value > this->max_wet_value)
	{
		this->wet_value = this->max_wet_value;
	}
}

int roomba_object::get_heat_value() const
{
	return heat_value;
}

void roomba_object::set_heat_value(int heat_value)
{
	this->heat_value = math::clamp(heat_value, 0, max_heat_value);
}

int roomba_object::get_max_heat_value() const
{
	return max_heat_value;
}

void roomba_object::set_max_heat_value(int max_heat_value)
{
	this->max_heat_value = max_heat_value;
	//	Å‘å’l‚ªŒ¸‚Á‚½‚çŒ¸‚ç‚·
	if (this->heat_value > this->max_heat_value)
	{
		this->heat_value = this->max_heat_value;
	}
}

float roomba_object::get_wet_rate() const
{
	return math::saturate(static_cast<float>(wet_value) / max_wet_value);
}

float roomba_object::get_heat_rate() const
{
	return math::saturate(static_cast<float>(heat_value) / max_heat_value);
}

float roomba_object::get_attract_range() const
{
	//	€–S‚Í‹z‚¢‚Ü‚È‚¢
	if (check_same_state(state_names[state_dead]))
		return 0.0f;

	auto velocity = get_velocity();
	auto Velocity = DirectX::XMLoadFloat3(&velocity);
	float speed = DirectX::XMVectorGetX(DirectX::XMVector3Length(Velocity));
	float speed_rate = math::saturate(speed / get_max_speed(move_mode::dash));
	return math::lerp(setup_data::attract_radius_range.x, setup_data::attract_radius_range.y, speed_rate);
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
	//	ŠÔ‚ÅÏ•ªÏ‚İ‚Ì—Í‚ğ‰Á‘¬“x‚Ö
	auto mass = get_push_box()->get_mass();
	velocity.x += force.x / mass;
	velocity.y += force.y / mass;
	velocity.z += force.z / mass;
}

void roomba_object::add_impulse(DirectX::XMFLOAT3 delta_velocity)
{
	//	’Pƒ‚É‘¬“x‚É‰ÁZ‚·‚é
	velocity.x += delta_velocity.x;
	velocity.y += delta_velocity.y;
	velocity.z += delta_velocity.z;
}

float roomba_object::get_current_accelaration_force() const
{
	if (action_data.move.x == 0 && action_data.move.z == 0)
		return 0.0f;

	return get_accelaration_force(action_data.dash ? move_mode::dash : move_mode::walk);
}

float roomba_object::get_current_braking_force() const
{
	return get_push_box()->get_mass() * setup_data::braking_deceleration;
}

float roomba_object::get_current_friction() const
{
	//	Šî–{–€CŒW”
	float friction = setup_data::base_friction;

	if (check_same_state(state_names[state_dead]))
	{
		//	€–S‚Í‹}Œƒ‚ÉŒ¸‘¬‚³‚¹‚é
		friction = setup_data::dead_friction;
	}
	else
	{
		//	”G‚ê‚É‚æ‚é–€CŒW”Œ¸Š
		float rate = math::ease::in_out_expo(get_wet_rate());
		friction = math::lerp(friction, setup_data::wet_friction, rate);
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

float roomba_object::calculate_drag_coefficient() const
{
	float base_drag_coefficient = setup_data::drag_coefficient;

	//	”G‚ê‚É‚æ‚éR—ÍŒvZŒW”(2%‚­‚ç‚¢‚Ål‚¦‚Ä‚¨‚­)
	float wet_rate = get_wet_rate();

	return base_drag_coefficient * math::lerp(1.0f, 1.02f, wet_rate);
}

float roomba_object::calculate_frontal_area() const
{
	//	p¨Y•ûŒü‚Æ‘¬“x•ûŒü‚Ì‚È‚·Šp‚©‚çA‰~’Œ‚Æ‚µ‚Ä‚Ì“Š‰e–ÊÏ‚ğZo
	DirectX::XMVECTOR Velocity = DirectX::XMLoadFloat3(&velocity);
	float speed_sq = DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(Velocity));

	//	…•½–Ê
	float horizontal_area = setup_data::diameter * setup_data::height;
	if (speed_sq < FLT_EPSILON)
		return horizontal_area;

	float radius = setup_data::diameter * 0.5f;

	//	‚’¼–Ê
	float vertical_area = DirectX::XM_PI * radius * radius;

	//	”¼ŒarE‚‚³h‚Ì‰~’Œ‚ğ²‚©‚çŠp“xƒÆŒX‚¢‚½•ûŒü‚©‚çŒ©‚½‚Æ‚«‚Ì“Š‰e–ÊÏ‚ğ‹‚ß‚é
	DirectX::XMVECTOR VelocityDirection = DirectX::XMVector3Normalize(Velocity);
	DirectX::XMFLOAT3 up = get_rotation().get_up();
	DirectX::XMVECTOR UpDir = DirectX::XMLoadFloat3(&up);

	float cos_theta = fabsf(DirectX::XMVectorGetX(DirectX::XMVector3Dot(VelocityDirection, UpDir)));
	float sin_theta = sqrtf(std::max(1.0f - cos_theta * cos_theta, 0.0f));

	return vertical_area * cos_theta + horizontal_area * sin_theta;
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
	//	À•WŒn‚ğÄİ’è
	if (!data.from->is_flag(collision_object::flags::detect_only))
		set_position(push_box->get_position());
}

void roomba_object::on_push_box_stay(collision_object::data data)
{
	//	À•WŒn‚ğÄİ’è
	if (!data.from->is_flag(collision_object::flags::detect_only))
		set_position(push_box->get_position());
}
