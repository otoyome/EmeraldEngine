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

	//	死亡時に加える速度
	constexpr math::float3 dead_impulse = system_units::convert_m_to_unit(math::float3(0.0f, 10.0f, 0.0f));

	//	エフェクト関連
	constexpr float effect_spawn_rate = 0.5f;		//	この割合以上でスパーク/煙を出す
	constexpr float effect_scale_rate = 1.5f;		//	割合からスケールへの倍率

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
, movement_snapshot()
, status()
, status_snapshot()
, pending_wet_value(0.0f)

, input_source()
, next_tick(0)

, effect_dead(false)
, dead_effect()
, spark_effect()
, smoke_effect()
{
}

minigame000::roomba_object::~roomba_object()
{
}

void minigame000::roomba_object::on_initialize()
{
	base::on_initialize();

	//	置かれた位置と向きから状態を始める
	movement_snapshot.position = get_world_translation();
	movement_snapshot.rotation = get_rotation();

	//	モデルの向きを本体に合わせる
	set_rotation(get_rotation());

	//	ステータス毎のエフェクトの初期状態を設定
	effect_dead = false;
	apply_status_snapshot();
}

void minigame000::roomba_object::on_finalize()
{
	base::on_finalize();
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

	roomba_status_settings& status_settings = status.get_settings();

	ImGui::SeparatorText("status");
	ImGui::DragFloat("max_wet_value", &status_settings.max_wet_value, 1.0f);
	ImGui::DragFloat("max_heat_value", &status_settings.max_heat_value, 1.0f);
	ImGui::DragFloat("base_wet_value", &status_settings.base_wet_value, 0.1f);
	ImGui::DragFloat("base_heat_value", &status_settings.base_heat_value, 0.1f);

	ImGui::SeparatorText("effect");
	object_reference_field::imgui_combo("dead_effect", get_context(), dead_effect);
	object_reference_field::imgui_combo("spark_effect", get_context(), spark_effect);
	object_reference_field::imgui_combo("smoke_effect", get_context(), smoke_effect);

	ImGui::SeparatorText("state");
	ImGui::Text("on_ground : %s", movement_snapshot.on_ground ? "true" : "false");
	ImGui::Text("velocity : %.2f %.2f %.2f", movement_snapshot.velocity.x, movement_snapshot.velocity.y, movement_snapshot.velocity.z);
	ImGui::Text("wet : %.1f (%.2f)", status_snapshot.wet_value, status.get_wet_rate(status_snapshot));
	ImGui::Text("heat : %.1f (%.2f)", status_snapshot.heat_value, status.get_heat_rate(status_snapshot));
	ImGui::Text("dead : %s", status_snapshot.dead ? "true" : "false");
	if (ImGui::Button("reset status"))
		status_snapshot = {};
}
#endif	//	defined(USE_IMGUI)

math::rotatorf minigame000::roomba_object::get_rotation() const
{
	return math::rotatorf(get_world_euler());
}

void minigame000::roomba_object::set_rotation(math::rotatorf rotator)
{
	movement_snapshot.rotation = rotator;

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
	movement_snapshot.position = position;

	set_world_translation(position);
}

math::vector3d minigame000::roomba_object::get_focus_point() const
{
	if (collision)
		return collision->get_world_translation();
	return get_position();
}

void minigame000::roomba_object::fixed_update(float elapsed_time)
{
	//	当たり判定が無ければ動かさない
	if (!collision || !collision->get_collision())
		return;

	//	判定形状
	roomba_movement::shape_descs shape;
	shape.radius = collision->get_shape_scale().x;
	shape.length = collision->get_shape_scale().y;
	shape.offset = collision->get_spatial()->get_local().get_translation().to_float3();
	shape.filter = collision_filter_manager::instance()->search_filter(collision->get_filter_name());
	shape.exclude = collision->get_collision();

	//	入力取得
	const std::uint32_t tick = ++next_tick;
	roomba_input_command command;
	if (const std::shared_ptr<roomba_input_source> source = input_source.lock())
	{
		if (!source->make_command(tick, command))
			command = {};
	}
	command.tick = tick;

	//	死亡中は操作を受け付けない
	if (status_snapshot.dead)
	{
		command = {};
		command.tick = tick;
	}

	//	移動処理
	movement.simulate(movement_snapshot, command, elapsed_time, movement_environment, get_context().get_objects().get_world(), shape);

	//	状態処理
	const bool was_dead = status_snapshot.dead;
	status.simulate(status_snapshot, command, pending_wet_value, movement_snapshot, movement, elapsed_time);
	pending_wet_value = 0.0f;

	//	死亡した瞬間に吹き飛ばす
	if (!was_dead && status_snapshot.dead)
	{
		movement_snapshot.velocity.x += dead_impulse.x;
		movement_snapshot.velocity.y += dead_impulse.y;
		movement_snapshot.velocity.z += dead_impulse.z;
	}

	//	移動情報適応
	apply_movement_snapshot();

	//	ステータス情報適応
	apply_status_snapshot();
}

void minigame000::roomba_object::apply_movement_snapshot()
{
	set_position(movement_snapshot.position);
	set_rotation(movement_snapshot.rotation);
}

void minigame000::roomba_object::apply_status_snapshot()
{
	const float wet_rate = status.get_wet_rate(status_snapshot);
	const float heat_rate = status.get_heat_rate(status_snapshot);

	//	漏電によるスパーク
	if (spark_effect)
	{
		spark_effect->set_spawn_stop(wet_rate < effect_spawn_rate);
		const double scale = wet_rate * effect_scale_rate;
		spark_effect->set_local_scale(math::vector3d(scale, scale, scale));
	}

	//	熱による煙
	if (smoke_effect)
	{
		smoke_effect->set_spawn_stop(heat_rate < effect_spawn_rate);
		const double scale = heat_rate * effect_scale_rate;
		smoke_effect->set_local_scale(math::vector3d(scale, scale, scale));
	}

	//	死亡状態が変わった瞬間に再生/停止
	if (effect_dead != status_snapshot.dead)
	{
		effect_dead = status_snapshot.dead;
		dead_transform = get_world();
		if (dead_effect)
		{
			if (status_snapshot.dead)
			{
				dead_effect->play();
			}
			else
			{
				dead_effect->stop();
			}
		}
	}
	dead_effect->set_world(dead_transform);
}

template<class Archive>
void minigame000::roomba_object::serialize_fields(Archive& archive, std::uint32_t version)
{
	roomba_movement_settings& settings = movement.get_settings();
	roomba_status_settings& status_settings = status.get_settings();

	float& mass = settings.mass;
	float& diameter = settings.diameter;
	float& height = settings.height;
	float& drag_coefficient = settings.drag_coefficient;
	float& walk_accelaration = settings.walk_accelaration;
	float& walk_max_speed = settings.walk_max_speed;
	float& dash_accelaration = settings.dash_accelaration;
	float& dash_max_speed = settings.dash_max_speed;
	float& braking_deceleration = settings.braking_deceleration;
	float& base_friction = settings.base_friction;
	float& rotation_speed = settings.rotation_speed;

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
			float gravity_scale = 0.0f;
			game_object_serializer::serialize_object_reference(archive, "model", model);
			game_object_serializer::serialize_object_reference(archive, "capsule", collision);
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
		case 3:
		{
			game_object_serializer::serialize_object_reference(archive, "model", model);
			game_object_serializer::serialize_object_reference(archive, "collision", collision);
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
					CEREAL_NVP(rotation_speed));
			break;
		}
		case 4:
		{
			game_object_serializer::serialize_object_reference(archive, "model", model);
			game_object_serializer::serialize_object_reference(archive, "collision", collision);
			archive(CEREAL_NVP(settings));
			break;
		}
		case 5:
		{
			game_object_serializer::serialize_object_reference(archive, "model", model);
			game_object_serializer::serialize_object_reference(archive, "collision", collision);
			archive(CEREAL_NVP(settings),
					CEREAL_NVP(status_settings));
			break;
		}
		case 6:
		{
			game_object_serializer::serialize_object_reference(archive, "model", model);
			game_object_serializer::serialize_object_reference(archive, "collision", collision);
			game_object_serializer::serialize_object_reference(archive, "dead_effect", dead_effect);
			game_object_serializer::serialize_object_reference(archive, "spark_effect", spark_effect);
			game_object_serializer::serialize_object_reference(archive, "smoke_effect", smoke_effect);
			archive(CEREAL_NVP(settings), CEREAL_NVP(status_settings));
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

namespace minigame000
{
	GAME_OBJECT_REGISTER(roomba_object, "minigame000/Roomba");
}	//	namespace minigame000
