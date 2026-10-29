#include "Scenes/MiniGameScene/minigame_title_scene.h"

#include "Scenes/MiniGameScene/minigame_scene.h"

#include "object/gameobjects/ui/common/transition_fade_controller.h"

#include "Scenes/MiniGameScene/minigame_title_topics.h"

namespace
{
	//	タイトルロゴ配置位置
	static constexpr math::float2 title_logo_position = { 60, 180 };
	static constexpr math::float2 title_logo_size = { 640, 270 };

	//	チュートリアルパネル左上位置
	static constexpr math::float2 tutorial_ui_text_position = {770, 125};
	static constexpr math::float2 tutorial_ui_text_size = {480, 400};
	static constexpr std::string_view tutorial_text = "WASD : 移動\nマウス左 : 加速\nマウス右：減速\n\n熱と水分両方を管理しながら素早くゴミを収集するのが目的です。\n\nゴミ以外にも泡があり、収集すると濡れます。\n加速すると熱くなるので水分を取る事で加熱を抑えます。\n加熱/水分を取り過ぎると爆発してリスタートします。";

	//	スタート案内アイコン配置位置
	static constexpr math::float2 start_icon_size = { 120, 120 };
	static constexpr math::float2 start_icon_position = { (640 - start_icon_size.x) / 2, SCREEN_HEIGHT - 100 };
}

minigame_title_scene::minigame_title_scene()
: scene_base()
, sound(get_broadcaster())
{
	//	マウスカーソルを表示する
	mouse::instance()->set_center_lock(false);
	mouse::instance()->set_cursol_hide(false);

	font_data_ptr font;
	font_data_manager::instance()->load_from_file(font, "Assets/Resources/Textures/fonts/MS Gothic/MS Gothic.fnt");

	//	タイトルロゴ
	{
		auto logo = std::make_shared<ui_image>("タイトルロゴ");
		logo->set_screen_rect({ title_logo_position.x, title_logo_position.y, title_logo_size.x, title_logo_size.y });
		logo->set_texture("Assets/Resources/Textures/hud/minigame_title.png");
		logo->set_draw_mode(ui_image::draw_modes::single);
		logo->set_sort_order(0);
		register_object(logo);
	}

	//	チュートリアル
	{
		auto text = std::make_shared<ui_text>("ルンバUI：チュートリアル：テキスト", font);
		text->set_screen_rect({ tutorial_ui_text_position.x, tutorial_ui_text_position.y, tutorial_ui_text_size.x, tutorial_ui_text_size.y });
		text->set_texts(tutorial_text);
		text->set_color({ 1, 1, 1, 1 });
		text->set_font_size(32);
		text->set_sort_order(5);
		register_object(text);
	}

	//	スタート案内アイコン
	{
		auto title_click_text = std::make_shared<ui_text>("タイトル:クリックテキスト", font);
		title_click_text->set_screen_rect({ 415, 580.000, 450, 90 });
		title_click_text->set_sort_order(20);
		title_click_text->set_font_size(32);
		title_click_text->set_color({ 1, 1, 0, 1 });
		title_click_text->set_texts("MOUSE LEFT CLICK / SPACE DOWN");
		register_object(title_click_text);
	}

	//	遷移フェードコントローラーにプレイを要求
	transition_fade_controller::instance()->play(fade::directions::in, 0.5f, fade::modes::black);

	//	タイトルが開いたことを知らせる
	get_broadcaster().post(minigame_title_topics::opened);
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
	//	閉じたことを知らせる
	get_broadcaster().publish(minigame_title_topics::closed);
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
			bool start_input = input::get_key_state(keyboard::key_states::space) == input::state::down
				|| input::get_key_state(mouse::key_states::left) == input::state::down
				|| input::get_key_state(gamepad::key_states::a) == input::state::down;
			if (start_input)
			{
				get_broadcaster().post(minigame_title_topics::decided);
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
