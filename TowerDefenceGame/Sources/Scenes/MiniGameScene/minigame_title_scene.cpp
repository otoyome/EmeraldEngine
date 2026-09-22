#include "Scenes/MiniGameScene/minigame_title_scene.h"

#include "Scenes/MiniGameScene/minigame_scene.h"

namespace
{
	//	タイトルロゴ配置位置
	static constexpr DirectX::XMFLOAT2 title_logo_position = { 0, 0 };
	static constexpr DirectX::XMFLOAT2 title_logo_size = { SCREEN_WIDTH, SCREEN_HEIGHT };

	//	スタート案内アイコン配置位置
	static constexpr DirectX::XMFLOAT2 start_icon_size = { 60, 60 };
	static constexpr DirectX::XMFLOAT2 start_icon_position = { (SCREEN_WIDTH - start_icon_size.x) / 2, SCREEN_HEIGHT - 100 };
}

minigame_title_scene::minigame_title_scene()
: scene_base()
{
	//	マウスカーソルを表示する
	mouse::instance()->set_center_lock(false);
	mouse::instance()->set_cursol_hide(false);

	//	タイトルロゴ
	{
		auto logo = std::make_shared<ui_image>("タイトルロゴ");
		logo->set_screen_rect({ title_logo_position.x, title_logo_position.y, title_logo_size.x, title_logo_size.y });
		logo->set_texture("Assets/Resources/Textures/hud/minigame_title.png");
		logo->set_draw_mode(ui_image::draw_modes::single);
		logo->set_sort_order(0);
		register_object(logo);
	}

	//	スタート案内アイコン
	{
		auto start_icon = std::make_shared<ui_image>("スタート案内アイコン");
		start_icon->set_screen_rect({ start_icon_position.x, start_icon_position.y, start_icon_size.x, start_icon_size.y });
		start_icon->set_texture("Assets/Resources/Textures/hud/keyboard_space_icon.png");
		start_icon->set_draw_mode(ui_image::draw_modes::single);
		start_icon->set_sort_order(1);
		register_object(start_icon);
	}
}

minigame_title_scene::~minigame_title_scene()
{
}

bool minigame_title_scene::initialize_core()
{
	return true;
}

bool minigame_title_scene::uninitialize_core()
{
	return true;
}

void minigame_title_scene::update_core(float elapsed_time)
{
	//	スペースキーか左クリックでゲームを開始する
	bool start_input = keyboard::instance()->get_state().is_key_state(keyboard::key_states::space) == input::state::down
		|| mouse::instance()->get_state().is_key_state(mouse::key_states::left) == input::state::down;
	if (start_input)
	{
		scene_manager::instance()->change_scene<minigame_scene>();
	}
}

#if defined(USE_IMGUI)
void minigame_title_scene::imgui_render_core()
{
	if (ImGui::TreeNode("minigame_title_scene"))
	{
		ImGui::TreePop();
	}
}
#endif	//	defined(USE_IMGUI)
