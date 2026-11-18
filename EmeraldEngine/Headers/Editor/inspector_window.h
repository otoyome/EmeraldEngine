#pragma once

#if defined(USE_IMGUI)
class world;
class scene_editor;
class engine_context;

//	インスペクター
class inspector_window
{
public:
	void imgui_render(world& target);
	void imgui_render(scene_editor& editor);

	//	プレハブエディターのパネル用
	void imgui_render_contents(engine_context& target);

private:
	static bool render_selected_object(world& target);
	static bool render_selected_object(engine_context& target);
};
#endif	//	defined(USE_IMGUI)
