#include "Scenes/MiniGameScene/minigame_ui_scene.h"

#include "custom_pipeline_state.h"
#include "object/gameobjects/ui/hud/countdown_controller_object.h"
#include "object/gameobjects/minigame/roomba_tutorial_controller_object.h"

namespace
{
	//	ヒートゲージ左上位置
	static constexpr DirectX::XMFLOAT2 heat_ui_position = {850, 500};
	static constexpr DirectX::XMFLOAT2 heat_ui_size = {50, 220};
	//	ウェットゲージ左上位置
	static constexpr DirectX::XMFLOAT2 wet_ui_position = {800, 500};
	static constexpr DirectX::XMFLOAT2 wet_ui_size = {50, 220};
	//	ゴミ収集率左上位置
	static constexpr DirectX::XMFLOAT2 trash_base_ui_position = {900, 500};
	static constexpr DirectX::XMFLOAT2 trash_base_ui_size = {359, 70};
	static constexpr DirectX::XMFLOAT2 trash_ui_position = {1050, 510};
	static constexpr DirectX::XMFLOAT2 trash_ui_size = {100, 60};
	//	時間表示パネル左上位置
	static constexpr DirectX::XMFLOAT2 timer_ui_position = {520, 100};
	static constexpr DirectX::XMFLOAT2 timer_ui_size = {500, 64};
	//	速度表示パネル左上位置
	static constexpr DirectX::XMFLOAT2 speed_base_ui_position = {900, 570};
	static constexpr DirectX::XMFLOAT2 speed_base_ui_size = {360, 100};
	static constexpr DirectX::XMFLOAT2 speed_text_ui_position = {1040, 580};
	static constexpr DirectX::XMFLOAT2 speed_text_ui_size = {360, 80};
	static constexpr float speed_text_font_size = 80.0f;
	//	チュートリアルパネル左上位置
	static constexpr DirectX::XMFLOAT2 tutorial_ui_position = {360, 200};
	static constexpr DirectX::XMFLOAT2 tutorial_ui_size = {500, 400};
	static constexpr DirectX::XMFLOAT2 tutorial_ui_text_position = {370, 240};
	static constexpr DirectX::XMFLOAT2 tutorial_ui_text_size = {480, 350};
	static constexpr std::string_view tutorial_text = "なるべく早い時間でゴミを収集するのが目的です。\n\nゴミ以外にも泡があり、収集すると濡れます。\n加速すると熱くなるので水分を取る事で加熱を抑えます。\n加熱・水分を取り過ぎると爆発してリスタートします。";

	//	加速アイコン左上位置
	static constexpr DirectX::XMFLOAT2 accel_icon_ui_position = {900, 680};
	static constexpr DirectX::XMFLOAT2 accel_icon_ui_size = {36, 36};
	static constexpr DirectX::XMFLOAT2 accel_icon_main_ui_position = {936, 680};
	static constexpr DirectX::XMFLOAT2 accel_icon_main_ui_size = {72, 36};
	//	減速アイコン左上位置
	static constexpr DirectX::XMFLOAT2 brake_icon_ui_position = {1010, 680};
	static constexpr DirectX::XMFLOAT2 brake_icon_ui_size = {36, 36};
	static constexpr DirectX::XMFLOAT2 brake_icon_main_ui_position = {1046, 680};
	static constexpr DirectX::XMFLOAT2 brake_icon_main_ui_size = {72, 36};
	//	チュートリアルアイコン左上位置
	static constexpr DirectX::XMFLOAT2 tutorial_icon_ui_position = {1010, 680};
	static constexpr DirectX::XMFLOAT2 tutorial_icon_ui_size = {72, 36};
	static constexpr DirectX::XMFLOAT2 tutorial_icon_main_ui_position = {1200, 680};
	static constexpr DirectX::XMFLOAT2 tutorial_icon_main_ui_size = {72, 36};
}

minigame_ui_scene::minigame_ui_scene()
: scene_base()
{
	font_data_ptr font;
	font_data_manager::instance()->load_from_file(font, "Assets/Resources/Textures/fonts/MS Gothic/MS Gothic.fnt");

	//	UIオブジェクト設置
	roomba_hud_controller_object::object_descs hud_object_desc;

	//	ヒートゲージ
	{
		roomba_gauge_controller_object::object_descs desc;
		desc.name = "ルンバUI：ヒートゲージコントローラー";
		{
			const int index = static_cast<int>(roomba_gauge_controller_object::ui_id::cover);
			desc.objects[index] = std::make_shared<ui_image>("ルンバUI：ヒートゲージ:カバー");
			desc.objects[index]->set_screen_rect({ heat_ui_position.x, heat_ui_position.y, heat_ui_size.x, heat_ui_size.y });
			desc.objects[index]->set_color({ 1, 0.25f, 0.25f, 1 });
			desc.objects[index]->set_border({0,0,1,1});
			desc.objects[index]->set_texture("Assets/Resources/Textures/hud/HeatMeter.png");
			desc.objects[index]->set_draw_mode(ui_image::draw_modes::sliced);
			desc.objects[index]->set_sort_order(10);
			register_object(desc.objects[index]);
		}
		{
			const int index = static_cast<int>(roomba_gauge_controller_object::ui_id::main);
			desc.objects[index] = std::make_shared<ui_image>("ルンバUI：ヒートゲージ:メイン");
			desc.objects[index]->set_screen_rect({ heat_ui_position.x, heat_ui_position.y, heat_ui_size.x, heat_ui_size.y });
			desc.objects[index]->set_color({ 1, 0.25f, 0.25f, 1 });
			desc.objects[index]->set_border({0,0,1,1});
			desc.objects[index]->set_texture("Assets/Resources/Textures/hud/HeatMeterMain.png");
			desc.objects[index]->set_draw_mode(ui_image::draw_modes::filled);
			desc.objects[index]->set_fill_method(ui_image::fill_methods::axis);
			desc.objects[index]->set_fill_origin(ui_image::fill_origins::bottom);
			desc.objects[index]->set_sort_order(5);
			register_object(desc.objects[index]);
		}
		hud_object_desc.heat_bar = std::make_shared<roomba_gauge_controller_object>(desc);
		register_object(hud_object_desc.heat_bar);
	}

	//	ウェットゲージ
	{
		roomba_gauge_controller_object::object_descs desc;
		desc.name = "ルンバUI：ウェットゲージコントローラー";
		{
			const int index = static_cast<int>(roomba_gauge_controller_object::ui_id::cover);
			desc.objects[index] = std::make_shared<ui_image>("ルンバUI：ウェットゲージ:カバー");
			desc.objects[index]->set_screen_rect({ wet_ui_position.x, wet_ui_position.y, wet_ui_size.x, wet_ui_size.y });
			desc.objects[index]->set_color({ 0.25f, 0.25f, 1, 1 });
			desc.objects[index]->set_border({0,0,1,1});
			desc.objects[index]->set_texture("Assets/Resources/Textures/hud/WetMeter.png");
			desc.objects[index]->set_draw_mode(ui_image::draw_modes::sliced);
			desc.objects[index]->set_sort_order(10);
			register_object(desc.objects[index]);
		}
		{
			const int index = static_cast<int>(roomba_gauge_controller_object::ui_id::main);
			desc.objects[index] = std::make_shared<ui_image>("ルンバUI：ウェットゲージ:メイン");
			desc.objects[index]->set_screen_rect({ wet_ui_position.x, wet_ui_position.y, wet_ui_size.x, wet_ui_size.y });
			desc.objects[index]->set_color({ 0.25f, 0.25f, 1, 1 });
			desc.objects[index]->set_border({0,0,1,1});
			desc.objects[index]->set_texture("Assets/Resources/Textures/hud/WetMeterMain.png");
			desc.objects[index]->set_draw_mode(ui_image::draw_modes::filled);
			desc.objects[index]->set_fill_method(ui_image::fill_methods::axis);
			desc.objects[index]->set_fill_origin(ui_image::fill_origins::bottom);
			desc.objects[index]->set_sort_order(5);
			register_object(desc.objects[index]);
		}
		hud_object_desc.wet_bar = std::make_shared<roomba_gauge_controller_object>(desc);
		register_object(hud_object_desc.wet_bar);
	}

	//	ゴミ収集率UI
	{
		roomba_trash_controller_object::object_descs desc;
		desc.name = "ルンバUI：ゴミ収集率コントローラー";
		{
			desc.image = std::make_shared<ui_image>("ルンバUI：ゴミ収集率:ベース");
			desc.image->set_screen_rect({ trash_base_ui_position.x, trash_base_ui_position.y, trash_base_ui_size.x, trash_base_ui_size.y });
			desc.image->set_color({ 0.25f, 0.25f, 1, 1 });
			desc.image->set_border({0,0,1,1});
			desc.image->set_texture("Assets/Resources/Textures/hud/DustMeter.png");
			desc.image->set_draw_mode(ui_image::draw_modes::sliced);
			desc.image->set_sort_order(0);
			register_object(desc.image);
		}
		{
			desc.text = std::make_shared<ui_text>("ルンバUI：ゴミ収集率:メイン", font);
			desc.text->set_screen_rect({trash_ui_position.x,
										trash_ui_position.y,
										trash_ui_size.x,
										trash_ui_size.y });
			desc.text->set_color({ 1, 1, 1, 1 });
			desc.text->set_texts("0%");
			desc.text->set_font_size(48);
			desc.text->set_sort_order(5);
			register_object(desc.text);
		}
		hud_object_desc.trash = std::make_shared<roomba_trash_controller_object>(desc);
		register_object(hud_object_desc.trash);
	}

	//	タイマーウィンドウ
	{
		timer_controller_object::object_descs object_desc;
		object_desc.name = "ルンバUI：経過時間表示コントローラー";

		{
			object_desc.text = std::make_shared<ui_text>("ルンバUI：タイマー：：数値", font);
			object_desc.text->set_texts("00:00:00");
			object_desc.text->set_screen_rect({ timer_ui_position.x,
											    timer_ui_position.y,
											    timer_ui_size.x,
											    timer_ui_size.y});
			object_desc.text->set_font_size(64);
			object_desc.text->set_sort_order(110);
			register_object(object_desc.text);
		}

		hud_object_desc.timer = std::make_shared<timer_controller_object>(object_desc);
		register_object(hud_object_desc.timer);
	}

	//	速度表示パネル
	{
		roomba_speed_controller_object::object_descs desc;
		desc.name = "ルンバUI：速度表示コントローラー";

		desc.image = std::make_shared<ui_image>("ルンバUI：速度表示：ベース");
		desc.image->set_screen_rect({ speed_base_ui_position.x, speed_base_ui_position.y, speed_base_ui_size.x, speed_base_ui_size.y });
		desc.image->set_texture("Assets/Resources/Textures/Hud/SpeedMeter.png");
		desc.image->set_draw_mode(ui_image::draw_modes::sliced);
		desc.image->set_border({0,0,0,0});
		desc.image->set_sort_order(0);
		register_object(desc.image);

		desc.text = std::make_shared<ui_text>("ルンバUI：速度表示：数値", font);
		desc.text->set_texts("0.0");
		desc.text->set_screen_rect({ speed_text_ui_position.x, speed_text_ui_position.y, speed_text_ui_size.x, speed_text_ui_size.y });
		desc.text->set_font_size(speed_text_font_size);
		desc.text->set_sort_order(5);
		register_object(desc.text);

		hud_object_desc.speed = std::make_shared<roomba_speed_controller_object>(desc);
		register_object(hud_object_desc.speed);
	}

	//	カウントダウン表示
	{
		countdown_controller_object::object_descs desc;
		desc.name = "ルンバUI：カウントダウン表示コントローラー";

		desc.text = std::make_shared<ui_text>("ルンバUI：カウントダウン表示：数値", font);
		desc.text->set_texts("");
		desc.text->set_font_size(220);
		desc.text->set_screen_rect({ 640 - 110, 360 - 110, 220, 220 });
		desc.text->set_sort_order(200);
		register_object(desc.text);

		desc.start_image = std::make_shared<ui_image>("ルンバUI：カウントダウン表示：スタート画像");
		desc.start_image->set_texture("Assets/Resources/Textures/hud/Start.png");
		desc.start_image->set_screen_rect({ 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT });
		desc.start_image->set_draw_mode(ui_image::draw_modes::single);
		desc.start_image->set_color({ 1, 1, 1, 0 });
		desc.start_image->set_sort_order(200);
		register_object(desc.start_image);

		desc.finish_image = std::make_shared<ui_image>("ルンバUI：カウントダウン表示：フィニッシュ画像");
		desc.finish_image->set_texture("Assets/Resources/Textures/hud/Finish.png");
		desc.finish_image->set_screen_rect({ 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT });
		desc.finish_image->set_draw_mode(ui_image::draw_modes::single);
		desc.finish_image->set_color({ 1, 1, 1, 0 });
		desc.finish_image->set_sort_order(200);
		register_object(desc.finish_image);

		auto countdown = std::make_shared<countdown_controller_object>(desc);
		register_object(countdown);
	}

	//	チュートリアル
	{
		roomba_tutorial_controller_object::object_descs desc;
		desc.name = "ルンバUI：チュートリアル表示コントローラー";
		{
			desc.base_image = std::make_shared<ui_image>("ルンバUI：チュートリアル：ベース");
			desc.base_image->set_screen_rect({ tutorial_ui_position.x, tutorial_ui_position.y, tutorial_ui_size.x, tutorial_ui_size.y });
			desc.base_image->set_texture("Assets/Resources/Textures/hud/NotificationWindow.png");
			desc.base_image->set_draw_mode(ui_image::draw_modes::sliced);
			desc.base_image->set_color({ 1, 1, 1, 0.5f });
			desc.base_image->set_border({ 0.25f, 0.25f, 0.75f, 0.75f });
			desc.base_image->set_sort_order(0);
			register_object(desc.base_image);
		}
		{
			desc.text = std::make_shared<ui_text>("ルンバUI：チュートリアル：テキスト", font);
			desc.text->set_screen_rect({ tutorial_ui_text_position.x, tutorial_ui_text_position.y, tutorial_ui_text_size.x, tutorial_ui_text_size.y });
			desc.text->set_texts(tutorial_text);
			desc.text->set_color({ 1, 1, 1, 0.5f });
			desc.text->set_font_size(32);
			desc.text->set_sort_order(5);
			register_object(desc.text);
		}

		hud_object_desc.tutorial = std::make_shared<roomba_tutorial_controller_object>(desc);
		//	既定では非表示(スペースキーを押している間だけ表示する)
		hud_object_desc.tutorial->set_alpha(0.0f);
		register_object(hud_object_desc.tutorial);
	}

	//	加速アイコン
	{
		key_icon_object::object_descs desc;
		desc.name = "ルンバUI：加速キーアイコン";
		desc.icon_type = key_icon_object::key_icon_type::mouse_left_click;
		desc.icon = std::make_shared<ui_image>("ルンバUI：加速キーアイコン");
		desc.icon->set_screen_rect({accel_icon_ui_position.x,
									accel_icon_ui_position.y,
									accel_icon_ui_size.x,
									accel_icon_ui_size.y });
		desc.icon->set_draw_mode(ui_image::draw_modes::single);
		desc.icon->set_sort_order(0);
		register_object(desc.icon);
		auto& key_object = std::make_shared<key_icon_object>(desc);
		register_object(key_object);

		auto main_icon = std::make_shared<ui_image>("ルンバUI：加速メインアイコン");
		main_icon->set_texture("Assets/Resources/Textures/hud/accel_icon.png");
		main_icon->set_screen_rect({accel_icon_main_ui_position.x,
									accel_icon_main_ui_position.y,
									accel_icon_main_ui_size.x,
									accel_icon_main_ui_size.y });
		main_icon->set_draw_mode(ui_image::draw_modes::single);
		main_icon->set_sort_order(0);
		register_object(main_icon);

		auto button = std::make_shared<ui_button>("ルンバUI：加速アイコンボタン", main_icon);
		button->set_color(ui_button::color_type::normal, {1, 1, 1, 1});
		button->set_color(ui_button::color_type::pressed, {1, 1, 1, 1});
		button->set_color(ui_button::color_type::disabled, {1, 1, 1, 1});
		register_object(button);

		key_indicator_object::object_descs object_desc;
		object_desc.name = "ルンバUI：加速キーインジケータ";
		object_desc.button = button;
		object_desc.key = key_object;
		hud_object_desc.accel_icon = std::make_shared<key_indicator_object>(object_desc);
		register_object(hud_object_desc.accel_icon);
	}

	//	減速アイコン
	{
		key_icon_object::object_descs desc;
		desc.name = "ルンバUI：減速キーアイコン";
		desc.icon_type = key_icon_object::key_icon_type::mouse_right_click;
		desc.icon = std::make_shared<ui_image>("ルンバUI：減速キーアイコン");
		desc.icon->set_screen_rect({brake_icon_ui_position.x,
									brake_icon_ui_position.y,
									brake_icon_ui_size.x,
									brake_icon_ui_size.y });
		desc.icon->set_draw_mode(ui_image::draw_modes::single);
		desc.icon->set_sort_order(0);
		register_object(desc.icon);
		auto& key_object = std::make_shared<key_icon_object>(desc);
		register_object(key_object);

		auto main_icon = std::make_shared<ui_image>("ルンバUI：減速メインアイコン");
		main_icon->set_texture("Assets/Resources/Textures/hud/brake_icon.png");
		main_icon->set_screen_rect({brake_icon_main_ui_position.x,
									brake_icon_main_ui_position.y,
									brake_icon_main_ui_size.x,
									brake_icon_main_ui_size.y });
		main_icon->set_draw_mode(ui_image::draw_modes::single);
		main_icon->set_sort_order(0);
		register_object(main_icon);

		auto button = std::make_shared<ui_button>("ルンバUI：減速アイコンボタン", main_icon);
		button->set_color(ui_button::color_type::normal, {1, 1, 1, 1});
		button->set_color(ui_button::color_type::pressed, {1, 1, 1, 1});
		button->set_color(ui_button::color_type::disabled, {1, 1, 1, 1});
		register_object(button);

		key_indicator_object::object_descs object_desc;
		object_desc.name = "ルンバUI：減速キーインジケータ";
		object_desc.button = button;
		object_desc.key = key_object;
		hud_object_desc.brake_icon = std::make_shared<key_indicator_object>(object_desc);
		register_object(hud_object_desc.brake_icon);
	}

	//	チュートリアルアイコン
	{
		key_icon_object::object_descs desc;
		desc.name = "ルンバUI：チュートリアルキーアイコン";
		desc.icon_type = key_icon_object::key_icon_type::space;
		desc.icon = std::make_shared<ui_image>("ルンバUI：チュートリアルキーアイコン");
		desc.icon->set_screen_rect({tutorial_icon_ui_position.x,
									tutorial_icon_ui_position.y,
									tutorial_icon_ui_size.x,
									tutorial_icon_ui_size.y });
		desc.icon->set_draw_mode(ui_image::draw_modes::single);
		desc.icon->set_sort_order(0);
		register_object(desc.icon);
		auto& key_object = std::make_shared<key_icon_object>(desc);
		register_object(key_object);

		auto main_icon = std::make_shared<ui_image>("ルンバUI：チュートリアルメインアイコン");
		main_icon->set_texture("Assets/Resources/Textures/hud/tutorial_icon.png");
		main_icon->set_screen_rect({tutorial_icon_main_ui_position.x,
									tutorial_icon_main_ui_position.y,
									tutorial_icon_main_ui_size.x,
									tutorial_icon_main_ui_size.y });
		main_icon->set_draw_mode(ui_image::draw_modes::single);
		main_icon->set_sort_order(0);
		register_object(main_icon);

		auto button = std::make_shared<ui_button>("ルンバUI：チュートリアルアイコンボタン", main_icon);
		button->set_color(ui_button::color_type::normal, {1, 1, 1, 1});
		button->set_color(ui_button::color_type::pressed, {1, 1, 1, 1});
		button->set_color(ui_button::color_type::disabled, {1, 1, 1, 1});
		register_object(button);

		key_indicator_object::object_descs object_desc;
		object_desc.name = "ルンバUI：チュートリアルキーインジケータ";
		object_desc.button = button;
		object_desc.key = key_object;
		hud_object_desc.brake_icon = std::make_shared<key_indicator_object>(object_desc);
		register_object(hud_object_desc.brake_icon);
	}

	hud_object_desc.name = "ルンバUI：メインHUD";
	auto hud = std::make_shared<roomba_hud_controller_object>(hud_object_desc);
	register_object(hud);
}

minigame_ui_scene::~minigame_ui_scene()
{
}

bool minigame_ui_scene::initialize_core()
{
	return true;
}

bool minigame_ui_scene::uninitialize_core()
{
	return true;
}

void minigame_ui_scene::update_core(float elapsed_time)
{
}

#if defined(USE_IMGUI)
void minigame_ui_scene::imgui_render_core()
{
	if (ImGui::TreeNode("minigame_ui_scene"))
	{
		ImGui::TreePop();
	}
}
#endif	//	defined(USE_IMGUI)
