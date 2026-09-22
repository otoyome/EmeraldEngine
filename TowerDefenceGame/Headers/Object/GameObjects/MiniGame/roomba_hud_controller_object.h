#pragma once

#include "object/gameobjects/ui/hud/key_icon_controller_object.h"
#include "object/gameobjects/ui/hud/timer_controller_object.h"

#include "roomba_gauge_controller_object.h"
#include "roomba_speed_controller_object.h"
#include "roomba_controller_object.h"

//	ルンバ：UI管理用オブジェクト
class roomba_hud_controller_object : public game_object
{
	GAME_OBJECT_DECRLEAR(roomba_hud_controller_object, game_object)

public:
	struct object_descs
	{
		std::string name;
		std::shared_ptr<roomba_gauge_controller_object> heat_bar;
		std::shared_ptr<roomba_gauge_controller_object> wet_bar;
		std::shared_ptr<roomba_speed_controller_object> speed;

		std::shared_ptr<key_indicator_object> accel_icon;
		std::shared_ptr<key_indicator_object> brake_icon;

		std::shared_ptr<timer_controller_object> timer;
	};

public:
	roomba_hud_controller_object(object_descs descs);
	~roomba_hud_controller_object() override;

	void post_update(float elapsed_time) override;

	void on_initialize() override;
	void on_finalize() override;

#if defined(USE_IMGUI)
	void imgui_render_core() override;
#endif	//	defined(USE_IMGUI)

private:
	bool setup_player_data;
	std::weak_ptr<roomba_controller_object> roomba;
	std::shared_ptr<roomba_gauge_controller_object> heat_bar;
	std::shared_ptr<roomba_gauge_controller_object> wet_bar;
	std::shared_ptr<roomba_speed_controller_object> speed;

	std::shared_ptr<key_indicator_object> accel_icon;
	std::shared_ptr<key_indicator_object> brake_icon;
};
