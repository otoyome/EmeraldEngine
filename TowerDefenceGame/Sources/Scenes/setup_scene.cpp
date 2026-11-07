#include "Scenes/setup_scene.h"

#include "Scenes/MiniGameScene/minigame_title_scene.h"

setup_scene::setup_scene()
: scene_base()
, one_shot(false)
{
}

setup_scene::~setup_scene()
{
}

bool setup_scene::initialize_core()
{
	//	他のシーンをセットアップするだけのシーン
	return true;
}

bool setup_scene::uninitialize_core()
{
	return true;
}

void setup_scene::update_core(float elapsed_time)
{
	//	シーンセレクト的な物があっても良い
	if (!one_shot)
	{
		one_shot = true;
		return;
	}

	get_context().get_scenes().remove_scene<setup_scene>();
#if defined(USE_IMGUI)
	//scene_manager::instance()->add_scene<edit_model_scene>();
	//scene_manager::instance()->add_scene<game_main_scene>();
	//scene_manager::instance()->add_scene<edit_particle_scene>();
	get_context().get_scenes().add_scene<minigame_title_scene>();
#else	//	defined(USE_IMGUI)
	get_context().get_scenes().add_scene<minigame_title_scene>();
#endif	//	defined(USE_IMGUI)
}

#if defined(USE_IMGUI)
void setup_scene::imgui_render_core()
{
	if (ImGui::TreeNode("setup_scene"))
	{
		ImGui::TreePop();
	}
}
#endif	//	defined(USE_IMGUI)
