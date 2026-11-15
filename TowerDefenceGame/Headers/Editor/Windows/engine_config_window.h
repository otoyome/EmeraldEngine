#pragma once

#if defined(USE_IMGUI)
#include "Editor/editor_items.h"

//	エンジンの設定
class engine_config_window : public editor_items::ibase_editor
{
public:
	std::string get_label() const override { return "engine_config"; }
	void render_window(std::shared_ptr<editor_items::context>& context, bool& open) override;

private:
	//	解像度と表示の方法の選択欄
	void render_display();

private:
	int display_resolution_index = -1;	//	選んでいる解像度
	int display_window_mode = -1;		//	選んでいる表示の方法
};
#endif	//	defined(USE_IMGUI)
