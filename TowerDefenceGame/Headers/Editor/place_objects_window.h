#pragma once

#if defined(USE_IMGUI)
#include <string_view>

class scene_editor;

//	置くオブジェクトの一覧の窓
class place_objects_window
{
public:
	//	ビューポートへドラッグする時の型名の受け渡し名
	static constexpr const char* payload_type = "SCENE_EDITOR_PLACE_OBJECT";

public:
	static bool is_placeable_type(std::string_view type);

public:
	void imgui_render(scene_editor& editor);

private:
	char filter[64] = {};
};
#endif	//	defined(USE_IMGUI)
