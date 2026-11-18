#pragma once

#if defined(USE_IMGUI)
#include <functional>
#include <string>
#include <string_view>

class object_edit_operations;

//	置くオブジェクトの一覧の窓
class place_objects_window
{
public:
	//	ビューポートへドラッグする時の型名の受け渡し名
	static constexpr const char* payload_type = "SCENE_EDITOR_PLACE_OBJECT";

public:
	static void render_placeable_types(std::string_view filter,
		const std::function<bool(const char* category)>& begin_category,
		const std::function<void()>& end_category,
		const std::function<void(const std::string& type)>& render_type);

public:
	void imgui_render(object_edit_operations& editor);

	//	プレハブエディターのパネル用
	void imgui_render_contents(object_edit_operations& editor);

private:
	char filter[64] = {};
};
#endif	//	defined(USE_IMGUI)
