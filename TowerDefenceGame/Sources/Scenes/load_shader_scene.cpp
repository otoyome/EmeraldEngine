#include "Scenes/load_shader_scene.h"

#include "custom_pipeline_state.h"

#include "scenes/minigamescene/minigame_scene.h"
#include "scenes/minigamescene/minigame_title_scene.h"
#include "Scenes/setup_scene.h"

#include "scenes/editscene/edit_model_scene.h"
#include "scenes/editscene/edit_particle_scene.h"

load_shader_scene::load_shader_scene()
: scene_base()
{
}

load_shader_scene::~load_shader_scene()
{
}

bool load_shader_scene::initialize_core()
{
	//	ここで一気にまとめてシェーダーを生成したい
	return true;
}

bool load_shader_scene::uninitialize_core()
{
	return true;
}

void load_shader_scene::update_core(float elapsed_time)
{
	get_context().get_scenes().remove_scene<load_shader_scene>();
	get_context().get_scenes().add_scene<setup_scene>();
}

#if defined(USE_IMGUI)
void load_shader_scene::imgui_render_core()
{
	if (ImGui::TreeNode("load_shader_scene"))
	{
		ImGui::TreePop();
	}
}
#endif	//	defined(USE_IMGUI)
