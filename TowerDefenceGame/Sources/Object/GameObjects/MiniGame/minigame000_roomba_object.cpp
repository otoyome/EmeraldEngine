#include "object/gameobjects/MiniGame/minigame000_roomba_object.h"

#include "cereal/cereal.hpp"
#include "cereal/archives/json.hpp"
#include "cereal/types/string.hpp"

#include "Object/game_object_registry.h"
#include "Object/game_object_serializer.h"
#include "Object/Collision/collision_filter.h"
#include "Object/Collision/collision_world.h"
#if defined(USE_IMGUI)
#include "Object/object_reference_field.h"
#endif	//	defined(USE_IMGUI)

namespace
{
	//	世界の設定
	constexpr auto world_gravity = math::float3(0.0f, system_units::convert_m_to_unit(-9.8f), 0.0f);	//	重力(m/s2)
	constexpr float world_air_density = 1.225f / 1000.0f;	//	空気密度(kg/m^3)

	//	入力・停止判定の閾値
	constexpr float move_input_dead_zone_sq = 0.01f * 0.01f;
	constexpr float action_move_input_dead_zone_sq = 0.1f * 0.1f;

	//	傾き関連
	constexpr float ground_tilt_follow_rate = 15.0f;
	constexpr float velocity_tilt_follow_rate = 30.0f;
	constexpr float velocity_roll_max = math::angle::to_radian(+20.0f);
	constexpr float velocity_pitch_max = math::angle::to_radian(20.0f);

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
	math::float2 calculate_velocity_tilt(float yaw, const math::float3& velocity, float max_speed, bool flip_pitch)
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

minigame000::roomba_object::roomba_object(std::string_view name)
: spatial_object(name)
, model()
, capsule()

, mass(system_units::convert_kg_to_unit(4.0f))
, diameter(system_units::convert_m_to_unit(0.34f))
, height(system_units::convert_m_to_unit(0.092f))
, drag_coefficient(0.45f)

, walk_accelaration(system_units::convert_m_to_unit(14.0f))
, walk_max_speed(system_units::convert_m_to_unit(0.75f))
, dash_accelaration(system_units::convert_m_to_unit(18.0f))
, dash_max_speed(system_units::convert_m_to_unit(16.67f))
, braking_deceleration(system_units::convert_m_to_unit(15.0f))
, base_friction(2.5f)

, rotation_speed(math::angle::to_radian(1440.0f))
, gravity_scale(1.0f)

, action_data()
, move_vector(0.0f, 0.0f)
, turn_vector(0.0f, 0.0f)
, velocity(0.0f, 0.0f, 0.0f)
, on_ground(true)
, ground_normal(0.0f, 1.0f, 0.0f)
, ground_friction(1.0f)

, velocity_tilt_pitch(0.0f)
, velocity_tilt_roll(0.0f)
, ground_tilt_pitch(0.0f)
, ground_tilt_roll(0.0f)
{
}

minigame000::roomba_object::~roomba_object()
{
}

void minigame000::roomba_object::on_initialize()
{
	base::on_initialize();

	//	モデルの向きを本体に合わせる
	set_rotation(get_rotation());
}

void minigame000::roomba_object::on_finalize()
{
	//	モデルは同じシーン（プレハブ）のオブジェクトなので、ここでは外さない
	base::on_finalize();
}

template<class Archive>
void minigame000::roomba_object::serialize_fields(Archive& archive, std::uint32_t version)
{
	switch (version)
	{
		case 0:
		case 1:
		{
			std::string collision_filter_name;
			float capsule_radius = 0.0f;
			float capsule_length = 0.0f;
			game_object_serializer::serialize_object_reference(archive, "model", model);
			archive(CEREAL_NVP(collision_filter_name),
					CEREAL_NVP(mass),
					CEREAL_NVP(capsule_radius),
					CEREAL_NVP(capsule_length),
					CEREAL_NVP(diameter),
					CEREAL_NVP(height),
					CEREAL_NVP(drag_coefficient),
					CEREAL_NVP(walk_accelaration),
					CEREAL_NVP(walk_max_speed),
					CEREAL_NVP(dash_accelaration),
					CEREAL_NVP(dash_max_speed),
					CEREAL_NVP(braking_deceleration),
					CEREAL_NVP(base_friction),
					CEREAL_NVP(rotation_speed),
					CEREAL_NVP(gravity_scale));
			break;
		}
		case 2:
		{
			game_object_serializer::serialize_object_reference(archive, "model", model);
			game_object_serializer::serialize_object_reference(archive, "capsule", capsule);
			archive(CEREAL_NVP(mass),
					CEREAL_NVP(diameter),
					CEREAL_NVP(height),
					CEREAL_NVP(drag_coefficient),
					CEREAL_NVP(walk_accelaration),
					CEREAL_NVP(walk_max_speed),
					CEREAL_NVP(dash_accelaration),
					CEREAL_NVP(dash_max_speed),
					CEREAL_NVP(braking_deceleration),
					CEREAL_NVP(base_friction),
					CEREAL_NVP(rotation_speed),
					CEREAL_NVP(gravity_scale));
			break;
		}
	}
}

void minigame000::roomba_object::serialize(cereal::JSONOutputArchive& archive) const
{
	base::serialize(archive);
	game_object_serializer::save_fields(archive, "roomba_object", *this);
}

void minigame000::roomba_object::deserialize(cereal::JSONInputArchive& archive)
{
	base::deserialize(archive);
	game_object_serializer::load_fields(archive, "roomba_object", *this);
}

#if defined(USE_IMGUI)
void minigame000::roomba_object::imgui_render_core()
{
	base::imgui_render_core();

	object_reference_field::imgui_combo("model", get_context(), model);
	object_reference_field::imgui_combo("capsule", get_context(), capsule);

	ImGui::SeparatorText("body");
	ImGui::DragFloat("mass", &mass, 0.01f);
	ImGui::DragFloat("diameter", &diameter, 0.01f);
	ImGui::DragFloat("height", &height, 0.01f);
	ImGui::DragFloat("drag_coefficient", &drag_coefficient, 0.01f);

	ImGui::SeparatorText("move");
	ImGui::DragFloat("walk_accelaration", &walk_accelaration, 0.1f);
	ImGui::DragFloat("walk_max_speed", &walk_max_speed, 0.1f);
	ImGui::DragFloat("dash_accelaration", &dash_accelaration, 0.1f);
	ImGui::DragFloat("dash_max_speed", &dash_max_speed, 0.1f);
	ImGui::DragFloat("braking_deceleration", &braking_deceleration, 0.1f);
	ImGui::DragFloat("base_friction", &base_friction, 0.01f);
	float degrees = math::angle::to_degree(rotation_speed);
	if (ImGui::DragFloat("rotation_speed", &degrees, 1.0f))
		rotation_speed = math::angle::to_radian(degrees);
	ImGui::DragFloat("gravity_scale", &gravity_scale, 0.01f);

	ImGui::SeparatorText("state");
	ImGui::Text("on_ground : %s", on_ground ? "true" : "false");
	ImGui::Text("velocity : %.2f %.2f %.2f", velocity.x, velocity.y, velocity.z);
}
#endif	//	defined(USE_IMGUI)

void minigame000::roomba_object::input_action_data(const action_datas& data)
{
	action_data = data;

	//	移動方向は一定以上倒していれば長さ 1 にする
	const math::float3& move = action_data.move;
	float length_sq = move.x * move.x + move.y * move.y + move.z * move.z;
	if (length_sq >= action_move_input_dead_zone_sq)
	{
		float length = sqrtf(length_sq);
		action_data.move.x /= length;
		action_data.move.y /= length;
		action_data.move.z /= length;
	}
}

math::rotatorf minigame000::roomba_object::get_rotation() const
{
	return math::rotatorf(get_world_euler());
}

void minigame000::roomba_object::set_rotation(math::rotatorf rotator)
{
	set_world_euler(math::rotatord(rotator));

	//	モデルは子だが、傾きの掛かり方を元のルンバと同じにするため、向きはワールドの角度で直接与える（モデルの正面は +90 度ずれている）
	if (model)
	{
		math::rotatorf model_rotator = rotator;
		model_rotator.y += math::constants<float>::pi_half;
		model->set_world_euler(math::rotatord(model_rotator));
	}
}

math::vector3d minigame000::roomba_object::get_position() const
{
	return get_world_translation();
}

void minigame000::roomba_object::set_position(const math::vector3d& position)
{
	set_world_translation(position);
}

float minigame000::roomba_object::get_max_speed(move_modes mode) const
{
	switch (mode)
	{
	case move_modes::walk: return walk_max_speed;
	case move_modes::dash: return dash_max_speed;
	}
	return 0.0f;
}

float minigame000::roomba_object::get_accelaration_force(move_modes mode) const
{
	const float accelaration = mode == move_modes::dash ? dash_accelaration : walk_accelaration;
	return mass * accelaration;
}

void minigame000::roomba_object::fixed_update(float elapsed_time)
{
	//	当たり判定のカプセルが無ければ動かさない
	if (!capsule || !capsule->get_collision())
		return;

	//	傾き破棄
	{
		math::rotatorf rotator = get_rotation();
		rotator.z = 0.0f;
		set_rotation(rotator);
	}

	//	入力に合わせて制御
	move_vector = math::float2(action_data.move.x, action_data.move.z);
	turn_vector = move_vector;

	update_rotation(elapsed_time);
	update_velocity(elapsed_time);
	update_move(elapsed_time);
	update_tilt(elapsed_time);

	//	入力は使い切る
	move_vector = {};
	action_data = {};
}

void minigame000::roomba_object::update_rotation(float elapsed_time)
{
	math::float2 vector = turn_vector;
	float length = vector.x * vector.x + vector.y * vector.y;
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

	rotator.y += (cross < 0.0f) ? rotation : -rotation;
	set_rotation(rotator);
}

void minigame000::roomba_object::update_velocity(float elapsed_time)
{
	math::vector3f Velocity(velocity);
	float speed_sq = Velocity.square_length();

	//	加速処理（水平方向のみ）
	math::float3 drive_force = { 0.0f, 0.0f, 0.0f };
	if (on_ground)
	{
		float move_length = move_vector.x * move_vector.x + move_vector.y * move_vector.y;
		if (move_length > move_input_dead_zone_sq)
		{
			move_length = sqrtf(move_length);
			float move_x = move_vector.x / move_length;
			float move_z = move_vector.y / move_length;

			//	歩き時のみ加速リミッター（爆速ルンバモードでは速度の上限は与えない）
			float limiter_rate = 1.0f;
			if (!action_data.dash)
			{
				math::vector3f DriveDirection(move_x, 0.0f, move_z);
				float speed_along_drive = math::vector3f::dot(Velocity, DriveDirection);
				float speed_margin = walk_max_speed * 0.1f;
				float over_speed = speed_along_drive - (walk_max_speed - speed_margin);
				limiter_rate = 1.0f - math::saturate(over_speed / speed_margin);
			}

			float accelaration_force = get_accelaration_force(action_data.dash ? move_modes::dash : move_modes::walk) * limiter_rate * elapsed_time;
			drive_force = { move_x * accelaration_force, 0.0f, move_z * accelaration_force };
		}
	}

	//	重力
	{
		float scale = mass * gravity_scale * elapsed_time;
		drive_force.x += scale * world_gravity.x;
		drive_force.y += scale * world_gravity.y;
		drive_force.z += scale * world_gravity.z;
	}

	math::vector3f DriveForce(drive_force);

	//	摩擦（ブレーキを含む）
	if (on_ground && speed_sq > FLT_MIN)
	{
		math::vector3f Normal(ground_normal);
		math::vector3f NormalVelocity = Normal * (math::vector3f::dot(Velocity, Normal));
		math::vector3f TangentVelocity = Velocity - NormalVelocity;
		float tangent_speed = TangentVelocity.length();
		if (tangent_speed > FLT_EPSILON)
		{
			math::vector3f Gravity = math::vector3f(world_gravity) * gravity_scale;
			float normal_force = fabsf(math::vector3f::dot(Gravity, Normal)) * mass;
			float total_friction = (ground_friction + base_friction) * 0.5f;
			float kinetic_friction_force = normal_force * total_friction;
			if (action_data.brake)
				kinetic_friction_force += mass * braking_deceleration;

			math::vector3f TangentDirection = TangentVelocity * (1.0f / tangent_speed);
			float resistance_impulse = std::min(kinetic_friction_force * elapsed_time, tangent_speed * mass);
			DriveForce = DriveForce - TangentDirection * resistance_impulse;
		}
	}

	//	空気抵抗
	if (speed_sq > FLT_EPSILON)
	{
		float speed = sqrtf(speed_sq);
		float air_force = 0.5f * world_air_density * drag_coefficient * calculate_frontal_area() * speed_sq;
		float drag_impulse = std::min(air_force * elapsed_time, speed * mass);
		math::vector3f DragDirection = Velocity * (1.0f / speed);
		DriveForce = DriveForce - DragDirection * drag_impulse;
	}

	//	力積を速度へ
	math::float3 force = DriveForce.to_float3();
	velocity.x += force.x / mass;
	velocity.y += force.y / mass;
	velocity.z += force.z / mass;
}

void minigame000::roomba_object::update_move(float elapsed_time)
{
	const math::vector3d start_position = get_world_translation();

	ground_friction = 1.0f;
	ground_normal = { 0.0f, 0.0f, 0.0f };
	on_ground = false;

	math::vector3d resolved_position;
	math::float3 hit_normal;
	float friction = 0.0f;
	if (calculate_position(velocity, elapsed_time, resolved_position, hit_normal, friction))
	{
		on_ground = true;
		ground_normal = hit_normal;
		ground_friction = friction;
	}
	set_position(resolved_position);

	//	実際に進めた変位から速度を再算出
	if (elapsed_time > FLT_EPSILON)
	{
		const math::float3 displacement = (resolved_position - start_position).to_float3();
		velocity.x = displacement.x / elapsed_time;
		velocity.y = displacement.y / elapsed_time;
		velocity.z = displacement.z / elapsed_time;
	}
}

void minigame000::roomba_object::update_tilt(float elapsed_time)
{
	//	速度から傾き算出
	{
		bool flip_flag = move_vector.x * move_vector.x + move_vector.y * move_vector.y <= 0.0001f;
		math::float2 target = calculate_velocity_tilt(get_rotation().y, velocity, dash_max_speed, flip_flag);
		float alpha = 1.0f - expf(-velocity_tilt_follow_rate * elapsed_time);
		velocity_tilt_pitch += (target.x - velocity_tilt_pitch) * alpha;
		velocity_tilt_roll  += (target.y - velocity_tilt_roll) * alpha;
	}

	//	地面の法線から傾き算出
	{
		math::float3 ground_tilt = calculate_ground_tilt(get_rotation().to_vector3().to_float3(), ground_normal);
		float alpha = 1.0f - expf(-ground_tilt_follow_rate * elapsed_time);
		ground_tilt_pitch += (ground_tilt.x - ground_tilt_pitch) * alpha;
		ground_tilt_roll  += (ground_tilt.z - ground_tilt_roll) * alpha;
	}

	//	モデルの都合で x に左右、z に前後の傾きを入れる
	math::rotatorf rotator = get_rotation();
	rotator.x = ground_tilt_roll + velocity_tilt_roll;
	rotator.z = ground_tilt_pitch + velocity_tilt_pitch;
	set_rotation(rotator);
}

bool minigame000::roomba_object::calculate_position(const math::float3& move_velocity, float elapsed_time, math::vector3d& out_position, math::float3& out_normal, float& out_friction)
{
	const math::float3& shape_scale = capsule->get_shape_scale();

	const math::vector3d base_position = get_world_translation();
	math::float3 relative_position = { 0.0f, 0.0f, 0.0f };

	math::vector3f Offset(capsule->get_spatial()->get_local().get_translation());

	collision_world::capsule_cast_descs desc;
	desc.radius = shape_scale.x;
	desc.length = shape_scale.y;
	desc.filter = collision_filter_manager::instance()->search_filter(capsule->get_filter_name());
	desc.exclude_objects.emplace_back(capsule->get_collision());

	static constexpr float StopFactor = system_units::convert_cm_to_unit(0.01f);
	static constexpr float SkinStep = system_units::convert_cm_to_unit(0.1f);

	bool hit_flag = false;
	math::vector3f Velocity(move_velocity);
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
		if (get_context().get_objects().get_world().capsule_cast(desc, intersect_data, hit_collision_object))
		{
			hit_flag = true;

			math::vector3f Normal = math::vector3f(intersect_data[0].normal).normalized();
			out_normal = Normal.to_float3();

			float hit_distance = std::max(static_cast<float>(intersect_data[0].length) - SkinStep, 0.0f);
			math::vector3f Position = From + (NormalizedVelocity * hit_distance);
			Position = Position + (Normal * SkinStep);
			relative_position = (Position - Offset).to_float3();

			out_friction = hit_collision_object->get_physical_data().friction;

			float traveled_fraction = math::saturate(hit_distance / travel_distance);
			remaining_time -= remaining_time * traveled_fraction;
			if (remaining_time <= 0.0f)
				break;

			//	重力が水平速度になるのを抑える
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

float minigame000::roomba_object::calculate_frontal_area() const
{
	//	姿勢の上方向と速度方向のなす角から、円柱としての投影面積を算出
	math::vector3f Velocity(velocity);
	float horizontal_area = diameter * height;
	if (Velocity.square_length() < FLT_EPSILON)
		return horizontal_area;

	float radius = diameter * 0.5f;
	float vertical_area = math::constants<float>::pi * radius * radius;

	math::vector3f VelocityDirection = Velocity.normalized();
	math::vector3f UpDirection(get_rotation().up().to_float3());
	float cos_theta = fabsf(math::vector3f::dot(VelocityDirection, UpDirection));
	float sin_theta = sqrtf(std::max(1.0f - cos_theta * cos_theta, 0.0f));
	return vertical_area * cos_theta + horizontal_area * sin_theta;
}

namespace minigame000
{
	GAME_OBJECT_REGISTER(roomba_object, "minigame000/Roomba");
}	//	namespace minigame000
