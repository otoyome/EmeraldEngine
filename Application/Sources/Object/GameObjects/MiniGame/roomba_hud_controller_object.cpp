#include "object/gameobjects/MiniGame/roomba_hud_controller_object.h"

roomba_hud_controller_object::roomba_hud_controller_object(object_descs descs)
: game_object(descs.name)
, setup_player_data(false)
, heat_bar(descs.heat_bar)
, wet_bar(descs.wet_bar)
, speed(descs.speed)
, trash(descs.trash)

, accel_icon(descs.accel_icon)
, brake_icon(descs.brake_icon)
, timer(descs.timer)

, tutorial(descs.tutorial)

, game_timer(descs.game_timer)
{
}

roomba_hud_controller_object::~roomba_hud_controller_object()
{
}

void roomba_hud_controller_object::post_update(float elapsed_time)
{
}

void roomba_hud_controller_object::on_initialize()
{
	roomba = get_context().get_objects().search_object<roomba_controller_object>();
	heat_bar->set_getter(
	[this]() {
		if (auto instance = roomba.lock())
			return instance->get_controll_object()->get_heat_value();
		return 0;
	},
	[this]() {
		if (auto instance = roomba.lock())
			return instance->get_controll_object()->get_max_heat_value();
		return 1;
	});
	wet_bar->set_getter(
	[this]() {
		if (auto instance = roomba.lock())
			return instance->get_controll_object()->get_wet_value();
		return 0;
	},
	[this]() {
		if (auto instance = roomba.lock())
			return instance->get_controll_object()->get_max_wet_value();
		return 1;
	});
	speed->set_getter(
	[this]() {
		if (auto instance = roomba.lock())
			return instance->get_controll_object()->get_velocity();
		return math::float3{ 0.0f, 0.0f, 0.0f };
	});
}

void roomba_hud_controller_object::on_finalize()
{
}

#if defined(USE_IMGUI)
void roomba_hud_controller_object::imgui_render_core()
{
}
#endif	//	defined(USE_IMGUI)
