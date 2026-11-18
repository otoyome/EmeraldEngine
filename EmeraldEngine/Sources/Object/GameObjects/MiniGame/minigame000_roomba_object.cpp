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
: base(name)
, model()
, collision()
, movement()

, movement_environment()
, movement_state()
, input_command()
, next_tick(0)
{
}

minigame000::roomba_object::~roomba_object()
{
}

void minigame000::roomba_object::on_initialize()
{
	base::on_initialize();

	//	置かれた位置と向きから状態を始める
	movement_state.position = get_world_translation();
	movement_state.rotation = get_rotation();

	//	モデルの向きを本体に合わせる
	set_rotation(get_rotation());
}

void minigame000::roomba_object::on_finalize()
{
	base::on_finalize();
}

template<class Archive>
void minigame000::roomba_object::serialize_fields(Archive& archive, std::uint32_t version)
{
	roomba_movement_settings& settings = movement.get_settings();

	switch (version)
	{
		case 0:
		case 1:
		{
			std::string collision_filter_name;
			float capsule_radius = 0.0f;
			float capsule_length = 0.0f;
			float gravity_scale = 0.0f;
			game_object_serializer::serialize_object_reference(archive, "model", model);
			archive(CEREAL_NVP(collision_filter_name),
					cereal::make_nvp("mass", settings.mass),
					CEREAL_NVP(capsule_radius),
					CEREAL_NVP(capsule_length),
					cereal::make_nvp("diameter", settings.diameter),
					cereal::make_nvp("height", settings.height),
					cereal::make_nvp("drag_coefficient", settings.drag_coefficient),
					cereal::make_nvp("walk_accelaration", settings.walk_accelaration),
					cereal::make_nvp("walk_max_speed", settings.walk_max_speed),
					cereal::make_nvp("dash_accelaration", settings.dash_accelaration),
					cereal::make_nvp("dash_max_speed", settings.dash_max_speed),
					cereal::make_nvp("braking_deceleration", settings.braking_deceleration),
					cereal::make_nvp("base_friction", settings.base_friction),
					cereal::make_nvp("rotation_speed", settings.rotation_speed),
					CEREAL_NVP(gravity_scale));
			break;
		}
		case 2:
		{
			float gravity_scale = 0.0f;
			game_object_serializer::serialize_object_reference(archive, "model", model);
			game_object_serializer::serialize_object_reference(archive, "capsule", collision);
			archive(cereal::make_nvp("mass", settings.mass),
					cereal::make_nvp("diameter", settings.diameter),
					cereal::make_nvp("height", settings.height),
					cereal::make_nvp("drag_coefficient", settings.drag_coefficient),
					cereal::make_nvp("walk_accelaration", settings.walk_accelaration),
					cereal::make_nvp("walk_max_speed", settings.walk_max_speed),
					cereal::make_nvp("dash_accelaration", settings.dash_accelaration),
					cereal::make_nvp("dash_max_speed", settings.dash_max_speed),
					cereal::make_nvp("braking_deceleration", settings.braking_deceleration),
					cereal::make_nvp("base_friction", settings.base_friction),
					cereal::make_nvp("rotation_speed", settings.rotation_speed),
					CEREAL_NVP(gravity_scale));
			break;
		}
		case 3:
		{
			game_object_serializer::serialize_object_reference(archive, "model", model);
			game_object_serializer::serialize_object_reference(archive, "collision", collision);
			archive(cereal::make_nvp("mass", settings.mass),
					cereal::make_nvp("diameter", settings.diameter),
					cereal::make_nvp("height", settings.height),
					cereal::make_nvp("drag_coefficient", settings.drag_coefficient),
					cereal::make_nvp("walk_accelaration", settings.walk_accelaration),
					cereal::make_nvp("walk_max_speed", settings.walk_max_speed),
					cereal::make_nvp("dash_accelaration", settings.dash_accelaration),
					cereal::make_nvp("dash_max_speed", settings.dash_max_speed),
					cereal::make_nvp("braking_deceleration", settings.braking_deceleration),
					cereal::make_nvp("base_friction", settings.base_friction),
					cereal::make_nvp("rotation_speed", settings.rotation_speed));
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
	object_reference_field::imgui_combo("collision", get_context(), collision);

	roomba_movement_settings& settings = movement.get_settings();

	ImGui::SeparatorText("body");
	ImGui::DragFloat("mass", &settings.mass, 0.01f);
	ImGui::DragFloat("diameter", &settings.diameter, 0.01f);
	ImGui::DragFloat("height", &settings.height, 0.01f);
	ImGui::DragFloat("drag_coefficient", &settings.drag_coefficient, 0.01f);

	ImGui::SeparatorText("move");
	ImGui::DragFloat("walk_accelaration", &settings.walk_accelaration, 0.1f);
	ImGui::DragFloat("walk_max_speed", &settings.walk_max_speed, 0.1f);
	ImGui::DragFloat("dash_accelaration", &settings.dash_accelaration, 0.1f);
	ImGui::DragFloat("dash_max_speed", &settings.dash_max_speed, 0.1f);
	ImGui::DragFloat("braking_deceleration", &settings.braking_deceleration, 0.1f);
	ImGui::DragFloat("base_friction", &settings.base_friction, 0.01f);
	float degrees = math::angle::to_degree(settings.rotation_speed);
	if (ImGui::DragFloat("rotation_speed", &degrees, 1.0f))
		settings.rotation_speed = math::angle::to_radian(degrees);

	ImGui::SeparatorText("state");
	ImGui::Text("on_ground : %s", movement_state.on_ground ? "true" : "false");
	ImGui::Text("velocity : %.2f %.2f %.2f", movement_state.velocity.x, movement_state.velocity.y, movement_state.velocity.z);
}
#endif	//	defined(USE_IMGUI)

void minigame000::roomba_object::input_action_data(const action_datas& data)
{
	input_command.move = data.move;
	input_command.dash = data.dash;
	input_command.brake = data.brake;
}

math::rotatorf minigame000::roomba_object::get_rotation() const
{
	return math::rotatorf(get_world_euler());
}

void minigame000::roomba_object::set_rotation(math::rotatorf rotator)
{
	movement_state.rotation = rotator;

	set_world_euler(math::rotatord(rotator));

	//	モデルは
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
	movement_state.position = position;

	set_world_translation(position);
}

void minigame000::roomba_object::fixed_update(float elapsed_time)
{
	//	当たり判定ルが無ければ動かさない
	if (!collision || !collision->get_collision())
		return;

	//	ルンバの当たり判定形状を伝達
	roomba_movement::shape_descs shape;
	shape.radius = collision->get_shape_scale().x;
	shape.length = collision->get_shape_scale().y;
	shape.offset = collision->get_spatial()->get_local().get_translation().to_float3();
	shape.filter = collision_filter_manager::instance()->search_filter(collision->get_filter_name());
	shape.exclude = collision->get_collision();

	input_command.tick = ++next_tick;

	//	移動処理
	movement.simulate(movement_state, input_command, elapsed_time, movement_environment, get_context().get_objects().get_world(), shape);

	input_command = {};

	//	移動情報適応
	apply_movement_state();
}

void minigame000::roomba_object::apply_movement_state()
{
	set_position(movement_state.position);
	set_rotation(movement_state.rotation);
}

namespace minigame000
{
	GAME_OBJECT_REGISTER(roomba_object, "minigame000/Roomba");
}	//	namespace minigame000
