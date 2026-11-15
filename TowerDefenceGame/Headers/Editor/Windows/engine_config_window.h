#pragma once

#if defined(USE_IMGUI)
#include "Editor/editor_items.h"
#include "System/engine_config_data.h"

//	エンジンの設定
class engine_config_window : public editor_items::ibase_editor
{
public:
	std::string get_label() const override { return "engine_config"; }
	void render_window(std::shared_ptr<editor_items::context>& context, bool& open) override;

private:
	//	解像度と表示の方法の選択欄
	void render_display();

	//	当たり判定のチャンネルとプロファイルの編集欄
	void render_collision();
	//	チャンネルの一覧
	bool render_collision_channels(engine_config_data::collision_data& data);
	//	プロファイルの一覧
	bool render_collision_profiles(engine_config_data::collision_data& data);

	//	names と重ならない名前
	static std::string make_unique_name(const std::vector<std::string>& names, std::string_view base);

private:
	int display_resolution_index = -1;	//	選んでいる解像度
	int display_window_mode = -1;		//	選んでいる表示の方法
};
#endif	//	defined(USE_IMGUI)
