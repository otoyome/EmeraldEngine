#include "object/gameobjects/MiniGame/minigame000_roomba_movement.h"

#include "Object/Collision/collision_world.h"

namespace
{
	//	“ü—ÍE’â~”»’è‚Ìè‡’l
	constexpr float move_input_dead_zone = 0.01f;
	constexpr float move_input_dead_zone_sq = move_input_dead_zone * move_input_dead_zone;

	constexpr float action_move_input_dead_zone = 0.1f;
	constexpr float action_move_input_dead_zone_sq = action_move_input_dead_zone * action_move_input_dead_zone;

	//	ŒX‚«ŠÖ˜A
	constexpr float ground_tilt_follow_rate = 15.0f;
	constexpr float velocity_tilt_follow_rate = 30.0f;
	constexpr float velocity_roll_max = math::angle::to_radian(+20.0f);
	constexpr float velocity_pitch_max = math::angle::to_radian(20.0f);

	//	’n–Ê‚ÌŒX‚«‚ğŠp“x‚ÅZo
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

	//	‘¬“xƒxƒNƒgƒ‹‚©‚ç‘OŒãE¶‰E‚ÌŒX‚«Šp‚ğZo
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

void minigame000::roomba_movement::simulate(roomba_movement_state& state, const roomba_input_command& command, float elapsed_time, const roomba_movement_environment& environment, collision_world& world, const shape_descs& shape) const
{
	step_datas step;

	step.command = command;
	float mx = step.command.move.x;
	float my = step.command.move.y;
	float mz = step.command.move.z;
	float ml = (mx * mx + my * my + mz * mz);
	if (ml >= action_move_input_dead_zone_sq)
	{
		ml = sqrtf(ml);
		step.command.move.x /= ml;
		step.command.move.y /= ml;
		step.command.move.z /= ml;
	}

	//	ŒX‚«”jŠü
	state.rotation = { state.rotation.x, state.rotation.y, 0 };

	{
		math::float2 move = math::float2(step.command.move.x, step.command.move.z);

		//	“ü—Í‚É‡‚í‚¹‚Ä§Œä
		step.move_vector = move;
		step.turn_front_vector = move;
	}

	//	‘¬“xXV
	update_rotation(state, step, elapsed_time);
	update_velocity(state, step, environment, elapsed_time);
	update_move(state, elapsed_time, world, shape);

	//	ŒX‚«“K‰
	update_tilt(state, step, elapsed_time);

	state.tick = command.tick;
}

float minigame000::roomba_movement::get_max_speed(move_modes mode) const
{
	switch (mode)
	{
	case move_modes::walk: return settings.walk_max_speed;
	case move_modes::dash: return settings.dash_max_speed;
	}
	return 0.0f;
}

float minigame000::roomba_movement::get_accelaration_force(move_modes mode) const
{
	float accelaration = 0.0f;
	switch (mode)
	{
		case move_modes::walk: accelaration = settings.walk_accelaration;	break;
		case move_modes::dash: accelaration = settings.dash_accelaration;	break;
	}
	return settings.mass * accelaration;
}

void minigame000::roomba_movement::add_force(roomba_movement_state& state, math::float3 force) const
{
	//	ŠÔ‚ÅÏ•ªÏ‚İ‚Ì—Í‚ğ‰Á‘¬“x‚Ö
	auto mass = settings.mass;
	state.velocity.x += force.x / mass;
	state.velocity.y += force.y / mass;
	state.velocity.z += force.z / mass;
}

float minigame000::roomba_movement::get_current_accelaration_force(const step_datas& step) const
{
	if (step.command.move.x == 0 && step.command.move.z == 0)
		return 0.0f;

	return get_accelaration_force(step.command.dash ? move_modes::dash : move_modes::walk);
}

float minigame000::roomba_movement::get_current_braking_force() const
{
	return settings.mass * settings.braking_deceleration;
}

float minigame000::roomba_movement::get_current_friction() const
{
	//	Šî–{–€CŒW”
	float friction = settings.base_friction;

	return friction;
}

float minigame000::roomba_movement::calculate_drag_coefficient() const
{
	//	R—ÍŒW”
	float base_drag_coefficient = settings.drag_coefficient;

	return base_drag_coefficient;
}

float minigame000::roomba_movement::calculate_frontal_area(const roomba_movement_state& state) const
{
	//	p¨Y•ûŒü‚Æ‘¬“x•ûŒü‚Ì‚È‚·Šp‚©‚çA‰~’Œ‚Æ‚µ‚Ä‚Ì“Š‰e–ÊÏ‚ğZo
	math::vector3f Velocity(state.velocity);
	float speed_sq = Velocity.square_length();

	//	…•½–Ê
	float horizontal_area = settings.diameter * settings.height;
	if (speed_sq < FLT_EPSILON)
		return horizontal_area;

	float radius = settings.diameter * 0.5f;

	//	‚’¼–Ê
	float vertical_area = math::constants<float>::pi * radius * radius;

	//	”¼ŒarE‚‚³h‚Ì‰~’Œ‚ğ²‚©‚çŠp“xƒÆŒX‚¢‚½•ûŒü‚©‚çŒ©‚½‚Æ‚«‚Ì“Š‰e–ÊÏ‚ğ‹‚ß‚é
	math::vector3f VelocityDirection = Velocity.normalized();
	math::float3 up = state.rotation.up().to_float3();
	math::vector3f UpDir(up);

	float cos_theta = fabsf(math::vector3f::dot(VelocityDirection, UpDir));
	float sin_theta = sqrtf(std::max(1.0f - cos_theta * cos_theta, 0.0f));

	return vertical_area * cos_theta + horizontal_area * sin_theta;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void minigame000::roomba_movement::update_rotation(roomba_movement_state& state, const step_datas& step, float elapsed_time) const
{
	math::float2 vector = {step.turn_front_vector.x, step.turn_front_vector.y};
	float length = (vector.x * vector.x + vector.y * vector.y);
	if (length < 0.001f * 0.001f)
		return;

	length = sqrtf(length);
	vector.x /= length;
	vector.y /= length;

	//	‘OƒxƒNƒgƒ‹Zo
	math::rotatorf rotator = state.rotation;
	math::float2 front = math::float2(sinf(rotator.y), cosf(rotator.y));

	//	‰ñ“]—ÊZo
	float dot = front.x * vector.x + front.y * vector.y;
	float cross = front.x * vector.y - front.y * vector.x;
	float rotation = acosf(math::clamp(dot, -1.0f, 1.0f));
	float speed = settings.rotation_speed * elapsed_time;
	if (rotation > speed)
		rotation = speed;

	rotator.y += ( cross < 0.0f ) ? rotation : -rotation;
	state.rotation = rotator;
}

void minigame000::roomba_movement::update_velocity(roomba_movement_state& state, const step_datas& step, const roomba_movement_environment& environment, float elapsed_time) const
{
	const math::float2& move_vector = step.move_vector;
	const roomba_input_command& command = step.command;

	math::vector3f Velocity(state.velocity);
	float speed_sq = Velocity.square_length();

	//	‰Á‘¬ˆ—
	math::float3 drive_force = {0.0f, 0.0f, 0.0f};
	if (state.on_ground)
	{
		//	‰Á‘¬‚Í…•½•ûŒü‚Ì‚İ‚Ål‚¦‚é
		float move_length = move_vector.x * move_vector.x + move_vector.y * move_vector.y;
		if (move_length > move_input_dead_zone_sq)
		{
			float move_vector_x = move_vector.x;
			float move_vector_z = move_vector.y;
			move_length = sqrtf(move_length);
			move_vector_x /= move_length;
			move_vector_z /= move_length;

			//	•à‚«‚Ì‚İ‰Á‘¬ƒŠƒ~ƒbƒ^[(”š‘¬ƒ‹ƒ“ƒoƒ‚[ƒh‚Å‚Í‘¬“x‚ÌãŒÀ‚Í—^‚¦‚È‚¢)
			float limiter_rate = 1.0f;
			if (!command.dash)
			{
				math::vector3f DriveDirection(move_vector_x, 0.0f, move_vector_z);
				float speed_along_drive = math::vector3f::dot(Velocity, DriveDirection);
				float max_speed = get_max_speed(move_modes::walk);
				float speed_margin = max_speed * 0.1f;
				float over_speed = speed_along_drive - (max_speed - speed_margin);
				limiter_rate = 1.0f - math::saturate(over_speed / speed_margin);
			}

			auto accelaration_force = get_current_accelaration_force(step) * limiter_rate * elapsed_time;
			drive_force = {move_vector_x * accelaration_force, 0, move_vector_z * accelaration_force};
		}
	}

	//	d—Íˆ—
	{
		float scale = settings.mass * gravity_scale * elapsed_time;
		drive_force.x += scale * environment.world_gravity.x;
		drive_force.y += scale * environment.world_gravity.y;
		drive_force.z += scale * environment.world_gravity.z;
	}

	//	‰Á‘¬—Í
	math::vector3f DriveForce(drive_force);

	//	–€Cˆ—
	if (state.on_ground && speed_sq > FLT_MIN)
	{
		math::vector3f Normal(state.ground_normal);

		//	‘¬“x‚ğ–Ê‚É‰ˆ‚¤¬•ª‚Æ–@ü¬•ª‚É•ª‰ğ
		math::vector3f NormalVelocity = Normal * (math::vector3f::dot(Velocity, Normal));
		math::vector3f TangentVelocity = Velocity - NormalVelocity;
		float tangent_speed = TangentVelocity.length();

		if (tangent_speed > FLT_EPSILON)
		{
			math::vector3f Gravity = math::vector3f(environment.world_gravity) * gravity_scale;

			//	‚’¼R—Í
			float gravity_along_normal = fabsf(math::vector3f::dot(Gravity, Normal));
			float normal_force = gravity_along_normal * settings.mass;

			//	‡¬–€CŒW”
			float total_friction = (state.ground_friction + get_current_friction()) * 0.5f;

			//	“®–€C—Í
			float kinetic_friction_force = normal_force * total_friction;

			//	ƒuƒŒ[ƒL—Í‚ğ‡Z
			if (command.brake)
				kinetic_friction_force += get_current_braking_force();

			//	–Ê‚É‰ˆ‚¤‘¬“x‚Æ‹tŒü‚«‚Ì—ÍÏ
			math::vector3f TangentDirection = TangentVelocity * (1.0f / tangent_speed);
			float resistance_impulse = kinetic_friction_force * elapsed_time;
			resistance_impulse = std::min(resistance_impulse, tangent_speed * settings.mass);

			math::vector3f ResistanceForce = TangentDirection * resistance_impulse;
			DriveForce = DriveForce - ResistanceForce;
		}
	}

	//	‹ó‹C’ïR
	if (speed_sq > FLT_EPSILON)
	{
		float speed = sqrtf(speed_sq);

		//	‹ó‹C–§“x
		float air_density = environment.world_air_density;

		//	‘O–Ê“Š‰e–ÊÏ(m^2)
		float frontal_area = calculate_frontal_area(state);

		//	R—ÍŒW”
		float drag_coefficient = calculate_drag_coefficient();

		//	‹ó‹C’ïR
		float air_force = 0.5f * air_density * drag_coefficient * frontal_area * speed_sq;

		//	is•ûŒü‚Æ‹tŒü‚«‚Ì—ÍÏ
		float drag_impulse = air_force * elapsed_time;
		drag_impulse = std::min(drag_impulse, speed * settings.mass);

		math::vector3f DragDirection = Velocity * (1.0f / speed);
		math::vector3f DragForce = DragDirection * drag_impulse;
		DriveForce = DriveForce - DragForce;
	}

	//	—Í’Ç‰Á
	drive_force = DriveForce.to_float3();
	add_force(state, drive_force);
}

void minigame000::roomba_movement::update_move(roomba_movement_state& state, float elapsed_time, collision_world& world, const shape_descs& shape) const
{
	const math::vector3d start_position = state.position;

	state.ground_friction = 1.0f;
	state.ground_normal = {};
	state.on_ground = false;

	math::vector3d resolved_position;
	math::float3 hit_normal;
	float friction = 0.0f;
	if (calculate_position(state, state.velocity, elapsed_time, world, shape, resolved_position, hit_normal, &friction))
	{
		state.on_ground = true;
		state.ground_normal = hit_normal;
		state.ground_friction = friction;
	}

	state.position = resolved_position;

	//	ÀÛ‚Éi‚ß‚½•ÏˆÊ‚©‚ç‘¬“x‚ğÄZo
	if (elapsed_time > FLT_EPSILON)
	{
		const math::float3 displacement = (resolved_position - start_position).to_float3();
		state.velocity.x = displacement.x / elapsed_time;
		state.velocity.y = displacement.y / elapsed_time;
		state.velocity.z = displacement.z / elapsed_time;
	}
}

void minigame000::roomba_movement::update_tilt(roomba_movement_state& state, const step_datas& step, float elapsed_time) const
{
	const math::float2& move_vector = step.move_vector;

	//	‘¬“x‚©‚çŒX‚«Zo
	{
		auto max_speed = get_max_speed(move_modes::dash);
		bool flip_flag = move_vector.x * move_vector.x + move_vector.y * move_vector.y <= 0.0001f;
		math::float2 velocity_tilt_target = calculate_velocity_tilt(state.rotation.y, state.velocity, max_speed, flip_flag);

		float velocity_tilt_alpha = 1.0f - expf(-velocity_tilt_follow_rate * elapsed_time);
		state.velocity_tilt_pitch += (velocity_tilt_target.x - state.velocity_tilt_pitch) * velocity_tilt_alpha;
		state.velocity_tilt_roll  += (velocity_tilt_target.y - state.velocity_tilt_roll) * velocity_tilt_alpha;
	}

	//	’n–Ê‚Ì–@ü‚©‚çŒX‚«Zo
	{
		math::float3 ground_tilt = calculate_ground_tilt(state.rotation.to_vector3().to_float3(), state.ground_normal);

		float ground_tilt_alpha = 1.0f - expf(-ground_tilt_follow_rate * elapsed_time);
		state.ground_tilt_pitch += (ground_tilt.x - state.ground_tilt_pitch) * ground_tilt_alpha;
		state.ground_tilt_roll  += (ground_tilt.z - state.ground_tilt_roll) * ground_tilt_alpha;
	}

	math::rotatorf rot = state.rotation;
	//	@remainder	ƒ‚ƒfƒ‹‚Ì–â‘è‚Åİ’è‚ğ•Ï‚¦‚Ä‚¢‚é
	rot.x = state.ground_tilt_roll + state.velocity_tilt_roll;
	rot.z = state.ground_tilt_pitch + state.velocity_tilt_pitch;
	state.rotation = rot;
}

bool minigame000::roomba_movement::calculate_position(const roomba_movement_state& state, math::float3 velocity, float elapsed_time, collision_world& world, const shape_descs& shape, math::vector3d& out_position, math::float3& out_normal, float* out_friction) const
{
	const math::vector3d base_position = state.position;
	math::float3 relative_position = { 0.0f, 0.0f, 0.0f };

	math::vector3f Offset(shape.offset);

	collision_world::capsule_cast_descs desc;
	desc.radius = shape.radius;
	desc.length = shape.length;
	desc.filter = shape.filter;
	desc.exclude_objects.emplace_back(shape.exclude);

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
		if (world.capsule_cast(desc, intersect_data, hit_collision_object))
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

			//	d—Í‚ª…•½‘¬“x‚É‚È‚é‚Ì‚ğ§Œä
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
