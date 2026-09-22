#include "Scenes/MiniGameScene/minigame_result_scene.h"

#include "Scenes/MiniGameScene/minigame_title_scene.h"

namespace
{
	//	リザルト背景配置位置
	static constexpr DirectX::XMFLOAT2 result_back_position = { 0, 0 };
	static constexpr DirectX::XMFLOAT2 result_back_size = { SCREEN_WIDTH, SCREEN_HEIGHT };

	//	プレイ終了メッセージ配置位置
	static constexpr DirectX::XMFLOAT2 thanks_message_size = { 600, 258 };
	static constexpr DirectX::XMFLOAT2 thanks_message_position = { (SCREEN_WIDTH - thanks_message_size.x) / 2, 220 };

	//	タイトルへ戻る案内アイコン配置位置
	static constexpr DirectX::XMFLOAT2 continue_icon_size = { 60, 60 };
	static constexpr DirectX::XMFLOAT2 continue_icon_position = { (SCREEN_WIDTH - continue_icon_size.x) / 2, SCREEN_HEIGHT - 100 };
}

minigame_result_scene::minigame_result_scene()
: scene_base()
{
	//	マウスカーソルを表示する
	mouse::instance()->set_center_lock(false);
	mouse::instance()->set_cursol_hide(false);

	//	リザルト背景
	{
		auto back = std::make_shared<ui_image>("リザルト背景");
		back->set_screen_rect({ result_back_position.x, result_back_position.y, result_back_size.x, result_back_size.y });
		back->set_texture("Assets/Resources/Textures/hud/result_back.png");
		back->set_draw_mode(ui_image::draw_modes::single);
		back->set_sort_order(0);
		register_object(back);
	}

	//	プレイ終了メッセージ
	{
		auto message = std::make_shared<ui_image>("プレイ終了メッセージ");
		message->set_screen_rect({ thanks_message_position.x, thanks_message_position.y, thanks_message_size.x, thanks_message_size.y });
		message->set_texture("Assets/Resources/Textures/hud/thunk_you_for_playing.png");
		message->set_draw_mode(ui_image::draw_modes::single);
		message->set_sort_order(1);
		register_object(message);
	}

	//	タイトルへ戻る案内アイコン
	{
		auto continue_icon = std::make_shared<ui_image>("タイトルへ戻る案内アイコン");
		continue_icon->set_screen_rect({ continue_icon_position.x, continue_icon_position.y, continue_icon_size.x, continue_icon_size.y });
		continue_icon->set_texture("Assets/Resources/Textures/hud/keyboard_space_icon.png");
		continue_icon->set_draw_mode(ui_image::draw_modes::single);
		continue_icon->set_sort_order(1);
		register_object(continue_icon);
	}
}

minigame_result_scene::~minigame_result_scene()
{
}

bool minigame_result_scene::initialize_core()
{
	return true;
}

bool minigame_result_scene::uninitialize_core()
{
	return true;
}

void minigame_result_scene::update_core(float elapsed_time)
{
	//	スペースキーか左クリックでタイトルへ戻る
	bool continue_input = keyboard::instance()->get_state().is_key_state(keyboard::key_states::space) == input::state::down
		|| mouse::instance()->get_state().is_key_state(mouse::key_states::left) == input::state::down;
	if (continue_input)
	{
		scene_manager::instance()->change_scene<minigame_title_scene>();
	}
}

#if defined(USE_IMGUI)
void minigame_result_scene::imgui_render_core()
{
	if (ImGui::TreeNode("minigame_result_scene"))
	{
		ImGui::TreePop();
	}
}
#endif	//	defined(USE_IMGUI)
