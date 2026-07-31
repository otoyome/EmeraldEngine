#include "Scenes/GameScene/game_main_scene.h"

#include "Object/camera_object.h"
#include "Object/model_object.h"

game_main_scene::game_main_scene()
: scene_base()
{
	//	メインカメラ生成
	std::shared_ptr<camera_object> main_camera;
	{
		main_camera = std::make_shared<camera_object>("メインカメラ");
		register_object(main_camera);
	}

	//	主人公モデル生成
	{
		//	モデルオブジェクト生成
		auto model = std::make_shared<model_object>("プレイヤー", "Assets/Resources/Models/Kuroko/Kuroko.emo");

		register_object(model);
	}
}

game_main_scene::~game_main_scene()
{
}

bool game_main_scene::initialize_core()
{
	return true;
}

bool game_main_scene::uninitialize_core()
{
	return true;
}

void game_main_scene::update_core(float elapsed_time)
{
}

#if defined(USE_IMGUI)
void game_main_scene::imgui_render_core()
{
	if (ImGui::TreeNode("game_main_scene"))
	{
		ImGui::TreePop();
	}
}
#endif	//	defined(USE_IMGUI)
