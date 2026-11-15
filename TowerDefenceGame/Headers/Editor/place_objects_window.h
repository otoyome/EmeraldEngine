#pragma once

#if defined(USE_IMGUI)
class scene_editor;

//	置くオブジェクトの一覧の窓
class place_objects_window
{
public:
	//	place objects 窓からビューポートへドラッグする時の型名の受け渡し名
	static constexpr const char* payload_type = "SCENE_EDITOR_PLACE_OBJECT";

	//	窓を描く
	void imgui_render(scene_editor& editor);

private:
	char filter[64] = {};
};
#endif	//	defined(USE_IMGUI)
