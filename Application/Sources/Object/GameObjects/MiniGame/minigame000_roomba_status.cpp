#include "object/gameobjects/MiniGame/minigame000_roomba_status.h"

#include "cereal/archives/json.hpp"

namespace
{
	//	‰Á‘¬‚É‚æ‚é‰Á”M
	constexpr float walk_heat_rate = +0.0f;		//	•à‚«‚Ì‰Á”MŠ„‡
	constexpr float dash_heat_rate = +0.8f;		//	ƒ_ƒbƒVƒ…‚Å’Ç‰Á‚³‚ê‚é‰Á”MŠ„‡
	constexpr float idle_heat_rate = -0.5f;		//	~‚Ü‚Á‚Ä‚¢‚é‚Ì‰Á”MŠ„‡(•ú”M)

	//	”G‚ê‚É‚æ‚é•ú”M
	constexpr float wet_cooling_rate = 0.75f;

	//	‹ó‹C’ïR‚É‚æ‚é•ú”MEŠ£‘‡
	constexpr float air_drag_heat_rate_max = 0.5f;
	constexpr float air_drag_wet_rate_min = 0.1f;
	constexpr float air_drag_wet_rate_max = 0.75f;
}

template<class Archive>
void minigame000::roomba_status_settings::serialize(Archive& archive, std::uint32_t version)
{
	archive(CEREAL_NVP(max_wet_value),
			CEREAL_NVP(max_heat_value),
			CEREAL_NVP(base_wet_value),
			CEREAL_NVP(base_heat_value));
}

template void minigame000::roomba_status_settings::serialize<cereal::JSONOutputArchive>(cereal::JSONOutputArchive& archive, std::uint32_t version);
template void minigame000::roomba_status_settings::serialize<cereal::JSONInputArchive>(cereal::JSONInputArchive& archive, std::uint32_t version);

float minigame000::roomba_status::get_wet_rate(const roomba_status_snapshot& snapshot) const
{
	return (settings.max_wet_value > FLT_EPSILON) ? math::saturate(snapshot.wet_value / settings.max_wet_value) : 0.0f;
}

float minigame000::roomba_status::get_heat_rate(const roomba_status_snapshot& snapshot) const
{
	return (settings.max_heat_value > FLT_EPSILON) ? math::saturate(snapshot.heat_value / settings.max_heat_value) : 0.0f;
}

void minigame000::roomba_status::simulate(roomba_status_snapshot& snapshot, const roomba_input_command& command, float add_wet_value, const roomba_movement_snapshot& movement_snapshot, const roomba_movement& movement, float elapsed_time) const
{
	//	ŠO•”‚Åó‚¯‚½”G‚ê
	set_wet_value(snapshot, snapshot.wet_value + add_wet_value);

	//	…•ª—Ê‚É‚æ‚é€–S”»’è
	if (!snapshot.dead && get_wet_rate(snapshot) >= 1.0f)
		snapshot.dead = true;

	//	”M—Ê/…•ª—ÊXV
	update_values(snapshot, command, movement_snapshot, movement, elapsed_time);

	//	”M—Ê‚É‚æ‚é€–S”»’è
	if (!snapshot.dead && get_heat_rate(snapshot) >= 1.0f)
		snapshot.dead = true;

	snapshot.tick = command.tick;
}

void minigame000::roomba_status::update_values(roomba_status_snapshot& snapshot, const roomba_input_command& command, const roomba_movement_snapshot& movement_snapshot, const roomba_movement& movement, float elapsed_time) const
{
	float elapsed_frame = system_units::convert_sec_to_fps(elapsed_time);

	//	‰Á‘¬‚É‚æ‚é‰Á”M
	float moved = (command.move.x != 0 || command.move.z != 0) ? 1.0f : 0.0f;
	float dashed = command.dash ? 1.0f : 0.0f;
	float accelaration = moved * walk_heat_rate + (moved * dashed) * dash_heat_rate;
	float accelaration_rate = math::lerp(idle_heat_rate, 1.0f, accelaration);

	//	…•ª—Ê‚É‚æ‚é•ú”M
	float wet_cooling = get_wet_rate(snapshot) * wet_cooling_rate;

	//	‹ó‹C’ïR‚É‚æ‚é•ú”MEŠ£‘‡‚ğZo
	math::vector3f Velocity(movement_snapshot.velocity);
	float speed = Velocity.length();
	float speed_rate = math::smoothstep(0.0f, movement.get_max_speed(roomba_movement::move_modes::dash), speed);
	float air_drag_heat_rate = math::lerp(0.0f, air_drag_heat_rate_max, speed_rate);
	float air_drag_wet_rate = math::lerp(air_drag_wet_rate_min, air_drag_wet_rate_max, speed_rate);

	//	’Pƒ‚È®
	float heat_rate = (accelaration_rate - air_drag_heat_rate - wet_cooling) * elapsed_frame;
	float wet_rate = -fabsf(air_drag_wet_rate) * elapsed_frame;
	set_heat_value(snapshot, snapshot.heat_value + settings.base_heat_value * heat_rate);
	set_wet_value(snapshot, snapshot.wet_value + settings.base_wet_value * wet_rate);
}

void minigame000::roomba_status::set_wet_value(roomba_status_snapshot& snapshot, float value) const
{
	snapshot.wet_value = math::clamp(value, 0.0f, settings.max_wet_value);
}

void minigame000::roomba_status::set_heat_value(roomba_status_snapshot& snapshot, float value) const
{
	snapshot.heat_value = math::clamp(value, 0.0f, settings.max_heat_value);
}
