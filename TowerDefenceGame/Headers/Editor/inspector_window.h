#pragma once

#if defined(USE_IMGUI)
class world;
class scene_editor;

//	インスペクター
class inspector_window
{
public:
	void imgui_render(world& target);
	void imgui_render(scene_editor& editor);

private:
	static bool render_selected_object(world& target);
};
#endif	//	defined(USE_IMGUI)
