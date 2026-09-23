#pragma once

#include "object/gameobjects/ui/ui_text.h"
#include "object/gameobjects/ui/ui_image.h"

//	カウントダウンUI制御用オブジェクト
class countdown_controller_object : public game_object
{
	GAME_OBJECT_DECRLEAR(countdown_controller_object, game_object)

public:
	struct object_descs
	{
		std::string name;
		std::shared_ptr<ui_text> text;
		std::shared_ptr<ui_image> start_image;
	};

public:
	countdown_controller_object(const object_descs& object_desc);
	~countdown_controller_object() override;

	//	残りカウント(3,2,1など)を表示する
	void set_count(int count);
	//	スタート画像を表示する
	void set_start();
	//	何も表示しない状態にする
	void hide();

private:
	std::shared_ptr<ui_text> text;
	std::shared_ptr<ui_image> start_image;
};
