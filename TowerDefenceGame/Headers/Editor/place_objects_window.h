#pragma once

#if defined(USE_IMGUI)
class scene_editor;

//	置くオブジェクトの一覧の窓
class place_objects_window
{
public:
	//	@tool place objects 窓からビューポートへドラッグする時の型名の受け渡し名
	static constexpr const char* payload_type = "SCENE_EDITOR_PLACE_OBJECT";

	//	@tool 窓を描く（選んだ型は editor の開いているシーンに置く）
	void imgui_render(scene_editor& editor);

private:
	char filter[64] = {};					//	place objects 窓の検索文字
};
#endif	//	defined(USE_IMGUI)
