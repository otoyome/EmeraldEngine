#pragma once

#include "object/gameobjects/ui/ui_image.h"
#include "object/gameobjects/ui/ui_text.h"

//	ルンバ：速度表示用オブジェクト
class roomba_speed_controller_object : public game_object
{
	GAME_OBJECT_DECRLEAR(roomba_speed_controller_object, game_object)

public:
	struct object_descs
	{
		std::string name;
		std::shared_ptr<ui_image> image;
		std::shared_ptr<ui_text> text;
	};

public:
	roomba_speed_controller_object(const object_descs& object_desc);
	~roomba_speed_controller_object() override;

	void fixed_update(float elapsed_time) override;

	void set_getter(std::function<math::float3()> get_velocity);
	void set_alpha(float alpha);

private:
	std::function<math::float3()> get_velocity;

	std::shared_ptr<ui_image> image;
	std::shared_ptr<ui_text> text;
	math::float4 origin_colors[2];
};

