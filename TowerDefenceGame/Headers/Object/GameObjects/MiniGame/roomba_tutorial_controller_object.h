#pragma once

#include "object/gameobjects/ui/ui_image.h"
#include "object/gameobjects/ui/ui_text.h"

//	ルンバ：チュートリアル表示用オブジェクト
class roomba_tutorial_controller_object : public game_object
{
	GAME_OBJECT_DECRLEAR(roomba_tutorial_controller_object, game_object)

public:
	struct object_descs
	{
		std::string name;
		std::shared_ptr<ui_image> base_image;
		std::shared_ptr<ui_text> text;
	};

public:
	roomba_tutorial_controller_object(const object_descs& object_desc);
	~roomba_tutorial_controller_object() override;

	void set_alpha(float alpha);

private:
	std::shared_ptr<ui_image> base_image;
	std::shared_ptr<ui_text> text;
	DirectX::XMFLOAT4 origin_colors[2];
};
