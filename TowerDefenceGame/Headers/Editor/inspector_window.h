#pragma once

#if defined(USE_IMGUI)
class world;

//	インスペクター
class inspector_window
{
public:
	void imgui_render(world& target);
};
#endif	//	defined(USE_IMGUI)
