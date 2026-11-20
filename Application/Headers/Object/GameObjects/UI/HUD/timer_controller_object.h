#pragma once

#include "object/ui/ui_object.h"
#include "object/ui/ui_image.h"

#include "object/ui/common/fade_controller_object.h"
#include "object/gameobjects/ui/hud/key_icon_controller_object.h"

//	タイマーUI制御用オブジェクト
class timer_controller_object : public game_object
{
	GAME_OBJECT_DECRLEAR(timer_controller_object, game_object)

public:
	struct object_descs
	{
		std::string name;
		std::shared_ptr<ui_image> image;
		std::shared_ptr<ui_text> text;
	};

public:
	timer_controller_object(const object_descs& object_desc);
	~timer_controller_object() override;

	void set_time(float time);
	void set_alpha(float alpha);

private:
	std::shared_ptr<ui_image> image;
	std::shared_ptr<ui_text> text;
	math::float4 origin_colors[2];
};
