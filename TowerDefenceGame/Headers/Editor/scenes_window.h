#pragma once

#if defined(USE_IMGUI)
class scene_editor;

//	•ÒW’†‚ÌƒV[ƒ“‚Ìˆê——‚Ì‘‹
class scenes_window
{
public:
	//	‘‹‚ğ•`‚­
	void imgui_render(scene_editor& editor);
};
#endif	//	defined(USE_IMGUI)
