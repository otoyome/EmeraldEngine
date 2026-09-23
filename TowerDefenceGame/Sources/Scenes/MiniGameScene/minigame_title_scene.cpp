#include "Scenes/MiniGameScene/minigame_title_scene.h"

#include "Scenes/MiniGameScene/minigame_scene.h"

#include "object/gameobjects/ui/common/transition_fade_controller.h"

namespace
{
	//	タイトルロゴ配置位置
	static constexpr DirectX::XMFLOAT2 title_logo_position = { 16 * 20, 9 * 20 };
	static constexpr DirectX::XMFLOAT2 title_logo_size = { 640 - title_logo_position.x / 2, 360 - title_logo_position.y / 2 };

	//	スタート案内アイコン配置位置
	static constexpr DirectX::XMFLOAT2 start_icon_size = { 120, 120 };
	static constexpr DirectX::XMFLOAT2 start_icon_position = { (640 - start_icon_size.x) / 2, SCREEN_HEIGHT - 100 };
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
		font_data_ptr font;
		font_data_manager::instance()->load_from_file(font, "Assets/Resources/Textures/fonts/MS Gothic/MS Gothic.fnt");

		auto title_click_text = std::make_shared<ui_text>("タイトル:クリックテキスト", font);
		title_click_text->set_screen_rect({ 490, 508.000, 120, 64 });
		title_click_text->set_sort_order(20);
		title_click_text->set_color({ 1, 1, 0, 1 });
		title_click_text->set_texts("MOUSE LEFT CLICK / SPACE DOWN");
		register_object(title_click_text);
	}

	//	遷移フェードコントローラーにプレイを要求
	transition_fade_controller::instance()->play(fade::directions::in, 0.5f, fade::modes::black);
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
	switch (state)
	{
		case states::fade_in_wait:
		{
			//	終了待機
			if (!transition_fade_controller::instance()->is_processing())
			{
				state = states::input_wait;
			}
			break;
		}
		case states::input_wait:
		{
			//	スペースキーか左クリックでゲームを開始する
			bool start_input = keyboard::instance()->get_state().is_key_state(keyboard::key_states::space) == input::state::down
				|| mouse::instance()->get_state().is_key_state(mouse::key_states::left) == input::state::down;
			if (start_input)
			{
				transition_fade_controller::instance()->play(fade::directions::out, 0.5f, fade::modes::black);
				state = states::fade_out_wait;
			}
			break;
		}
		case states::fade_out_wait:
		{
			if (!transition_fade_controller::instance()->is_processing())
			{
				scene_manager::instance()->remove_scene<minigame_title_scene>();
				scene_manager::instance()->add_scene<minigame_scene>();
			}
			break;
		}
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
