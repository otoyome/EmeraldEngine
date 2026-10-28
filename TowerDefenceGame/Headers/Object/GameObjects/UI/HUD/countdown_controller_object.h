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
		std::shared_ptr<ui_image> finish_image;
	};

public:
	countdown_controller_object(const object_descs& object_desc);
	~countdown_controller_object() override;

	void set_count(int count);
	void set_start();
	void set_finish();
	void hide();

private:
	//	表示が切り替わったときだけ音を鳴らす
	enum class displays
	{
		none,
		count,
		start,
		finish,
	};
	void change_display(displays display, int count = 0);

private:
	displays current_display = displays::none;	//	今の表示
	int current_count = 0;						//	今表示しているカウント
	std::shared_ptr<ui_text> text;
	std::shared_ptr<ui_image> start_image;
	std::shared_ptr<ui_image> finish_image;
};
