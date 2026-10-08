#include "Scenes/stationed_scene.h"

#include "object/camera_object.h"

#include "object/gameobjects/ui/ui_image.h"
#include "object/gameobjects/ui/common/transition_fade_controller.h"

stationed_scene::stationed_scene()
: scene_base()
{
	//	メインカメラ生成
	std::shared_ptr<camera_object> main_camera;
	{
		main_camera = std::make_shared<camera_object>("メインカメラ");
		register_object(main_camera);
	}

	//	フェードイン/アウト用オブジェクト生成
	std::shared_ptr<fade_controller_object> fade_controller;
	{
		auto fade_in_out = std::make_shared<ui_image>("フェードイン/アウト用オブジェクト");
		fade_in_out->set_sort_order(0x00FFFFFF);
		register_object(fade_in_out);

		fade_controller = std::make_shared<fade_controller_object>("フェードイン/アウト管理オブジェクト", fade_in_out);
		register_object(fade_controller);

		transition_fade_controller::create_instance();
		transition_fade_controller::instance()->set_fade_object(fade_controller);

		//	真っ黒な状態から始まる
		transition_fade_controller::instance()->play(fade::directions::out, 0.0f, fade::modes::black);
	}
}

stationed_scene::~stationed_scene()
{
}

bool stationed_scene::initialize_core()
{
	return true;
}

bool stationed_scene::uninitialize_core()
{
	transition_fade_controller::delete_instance();
	return true;
}

void stationed_scene::update_core(float elapsed_time)
{
}

#if defined(USE_IMGUI)
void stationed_scene::imgui_render_core()
{
	if (ImGui::TreeNode("stationed_scene"))
	{
		ImGui::TreePop();
	}
}
#endif	//	defined(USE_IMGUI)
