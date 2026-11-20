#pragma once

#include "object/ui/ui_image.h"
#include "object/ui/ui_text.h"

//	ルンバ：ゴミ収集率表示用オブジェクト
class roomba_trash_controller_object : public game_object
{
	GAME_OBJECT_DECRLEAR(roomba_trash_controller_object, game_object)

public:
	struct object_descs
	{
		std::string name;
		std::shared_ptr<ui_image> image;
		std::shared_ptr<ui_text> text;
	};

public:
	roomba_trash_controller_object(const object_descs& object_desc);
	~roomba_trash_controller_object() override;

	void fixed_update(float elapsed_time) override;

	void set_getter(std::function<int()> get_value, std::function<int()> get_max_value);
	void set_alpha(float alpha);

private:
	std::function<int()> get_value;
	std::function<int()> get_max_value;

	std::shared_ptr<ui_image> image;
	std::shared_ptr<ui_text> text;
	math::float4 origin_colors[2];
};
