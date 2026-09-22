#include "Scenes/GameScene/minigame_ui_scene.h"

#include "custom_pipeline_state.h"

namespace
{
	//	ヒートゲージ左上位置
	static constexpr DirectX::XMFLOAT2 heat_ui_position = {450, 600};
	static constexpr DirectX::XMFLOAT2 heat_ui_size = {100, 20};
	//	ウェットゲージ左上位置
	static constexpr DirectX::XMFLOAT2 wet_ui_position = {728, 600};
	static constexpr DirectX::XMFLOAT2 wet_ui_size = {100, 20};
	//	加速アイコン左上位置
	static constexpr DirectX::XMFLOAT2 accel_icon_ui_position = {1130, 540};
	static constexpr DirectX::XMFLOAT2 accel_icon_ui_size = {36, 36};
	static constexpr DirectX::XMFLOAT2 accel_icon_main_ui_size = {72, 36};
	//	減速アイコン左上位置
	static constexpr DirectX::XMFLOAT2 brake_icon_ui_position = {1130, 612};
	static constexpr DirectX::XMFLOAT2 brake_icon_ui_size = {36, 36};
	static constexpr DirectX::XMFLOAT2 brake_icon_main_ui_size = {72, 36};
	//	時間表示パネル左上位置
	static constexpr DirectX::XMFLOAT2 timer_ui_position = {1038, 650};
	static constexpr DirectX::XMFLOAT2 timer_ui_size = {243, 50};
	//	速度表示パネル左上位置
	static constexpr DirectX::XMFLOAT2 speed_ui_size = {480, 100};
	static constexpr DirectX::XMFLOAT2 speed_ui_position = {630 - speed_ui_size.x / 2, 580};
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
			const int index = static_cast<int>(roomba_gauge_controller_object::ui_id::main);
			desc.objects[index] = std::make_shared<ui_image>("ヒートゲージ:メイン");
			desc.objects[index]->set_screen_rect({ heat_ui_position.x, heat_ui_position.y, heat_ui_size.x, heat_ui_size.y });
			desc.objects[index]->set_color({ 1, 0.25f, 0.25f, 1 });
			desc.objects[index]->set_border({0.25f,0,0.25f,0});
			desc.objects[index]->set_texture("Assets/Resources/Textures/hud/SpeedMeterLeft.png");
			desc.objects[index]->set_draw_mode(ui_image::draw_modes::sliced);
			desc.objects[index]->set_sort_order(5);
			register_object(desc.objects[index]);
		}
		hud_object_desc.heat_bar = std::make_shared<roomba_gauge_controller_object>(desc);
		hud_object_desc.heat_bar->set_animation_direction(roomba_gauge_controller_object::directions::left);
		register_object(hud_object_desc.heat_bar);
	}

	//	ウェットゲージ
	{
		roomba_gauge_controller_object::object_descs desc;
		desc.name = "ルンバUI：ウェットゲージコントローラー";
		{
			const int index = static_cast<int>(roomba_gauge_controller_object::ui_id::main);
			desc.objects[index] = std::make_shared<ui_image>("ウェットゲージ:メイン");
			desc.objects[index]->set_screen_rect({ wet_ui_position.x, wet_ui_position.y, wet_ui_size.x, wet_ui_size.y });
			desc.objects[index]->set_color({ 0.25f, 0.25f, 1, 1 });
			desc.objects[index]->set_border({0.25f,0,0.25f,0});
			desc.objects[index]->set_texture("Assets/Resources/Textures/hud/SpeedMeterRight.png");
			desc.objects[index]->set_draw_mode(ui_image::draw_modes::sliced);
			desc.objects[index]->set_sort_order(5);
			register_object(desc.objects[index]);
		}
		hud_object_desc.wet_bar = std::make_shared<roomba_gauge_controller_object>(desc);
		hud_object_desc.wet_bar->set_animation_direction(roomba_gauge_controller_object::directions::right);
		register_object(hud_object_desc.wet_bar);
	}

	//	加速アイコン
	{
		key_icon_object::object_descs desc;
		desc.name = "加速キーアイコン";
		desc.icon_type = key_icon_object::key_icon_type::mouse_left_click;
		desc.icon = std::make_shared<ui_image>("加速キーアイコン");
		desc.icon->set_screen_rect({accel_icon_ui_position.x,
									accel_icon_ui_position.y,
									accel_icon_ui_size.x,
									accel_icon_ui_size.y });
		desc.icon->set_draw_mode(ui_image::draw_modes::single);
		desc.icon->set_sort_order(0);
		register_object(desc.icon);
		auto& key_object = std::make_shared<key_icon_object>(desc);
		register_object(key_object);

		auto main_icon = std::make_shared<ui_image>("加速メインアイコン");
		main_icon->set_texture("Assets/Resources/Textures/hud/accel_icon.png");
		main_icon->set_screen_rect({accel_icon_ui_position.x - accel_icon_main_ui_size.x / 4,
									accel_icon_ui_position.y + accel_icon_ui_size.y + 4,
									accel_icon_main_ui_size.x,
									accel_icon_main_ui_size.y });
		main_icon->set_draw_mode(ui_image::draw_modes::single);
		main_icon->set_sort_order(0);
		register_object(main_icon);

		auto button = std::make_shared<ui_button>("加速アイコンボタン", main_icon);
		button->set_color(ui_button::color_type::normal, {1, 1, 1, 1});
		button->set_color(ui_button::color_type::pressed, {1, 1, 1, 1});
		button->set_color(ui_button::color_type::disabled, {1, 1, 1, 1});
		register_object(button);

		key_indicator_object::object_descs object_desc;
		object_desc.name = "加速キーインジケータ";
		object_desc.button = button;
		object_desc.key = key_object;
		hud_object_desc.accel_icon = std::make_shared<key_indicator_object>(object_desc);
		register_object(hud_object_desc.accel_icon);
	}

	//	減速アイコン
	{
		key_icon_object::object_descs desc;
		desc.name = "減速キーアイコン";
		desc.icon_type = key_icon_object::key_icon_type::mouse_right_click;
		desc.icon = std::make_shared<ui_image>("減速キーアイコン");
		desc.icon->set_screen_rect({brake_icon_ui_position.x,
									brake_icon_ui_position.y,
									brake_icon_ui_size.x,
									brake_icon_ui_size.y });
		desc.icon->set_draw_mode(ui_image::draw_modes::single);
		desc.icon->set_sort_order(0);
		register_object(desc.icon);
		auto& key_object = std::make_shared<key_icon_object>(desc);
		register_object(key_object);

		auto main_icon = std::make_shared<ui_image>("減速メインアイコン");
		main_icon->set_texture("Assets/Resources/Textures/hud/brake_icon.png");
		main_icon->set_screen_rect({brake_icon_ui_position.x - brake_icon_main_ui_size.x / 4,
									brake_icon_ui_position.y + brake_icon_ui_size.y + 4,
									brake_icon_main_ui_size.x,
									brake_icon_main_ui_size.y });
		main_icon->set_draw_mode(ui_image::draw_modes::single);
		main_icon->set_sort_order(0);
		register_object(main_icon);

		auto button = std::make_shared<ui_button>("減速アイコンボタン", main_icon);
		button->set_color(ui_button::color_type::normal, {1, 1, 1, 1});
		button->set_color(ui_button::color_type::pressed, {1, 1, 1, 1});
		button->set_color(ui_button::color_type::disabled, {1, 1, 1, 1});
		register_object(button);

		key_indicator_object::object_descs object_desc;
		object_desc.name = "減速キーインジケータ";
		object_desc.button = button;
		object_desc.key = key_object;
		hud_object_desc.brake_icon = std::make_shared<key_indicator_object>(object_desc);
		register_object(hud_object_desc.brake_icon);
	}

	//	タイマーウィンドウ
	{
		timer_controller_object::object_descs object_desc;
		object_desc.name = "経過時間表示コントローラー";

		//	顔画像
		object_desc.image = std::make_shared<ui_image>("タイマー：ベース");
		object_desc.image->set_screen_rect({ timer_ui_position.x, timer_ui_position.y, timer_ui_size.x, timer_ui_size.y });
		object_desc.image->set_texture("Assets/Resources/Textures/Hud/NotificationWindow.png");
		object_desc.image->set_draw_mode(ui_image::draw_modes::sliced);
		object_desc.image->set_sort_order(100);
		register_object(object_desc.image);

		object_desc.text = std::make_shared<ui_text>("タイマー：：数値", font);
		object_desc.text->set_texts("00:00:00");
		object_desc.text->set_screen_rect({ timer_ui_position.x + 59, timer_ui_position.y + 11, 32, 32 });
		object_desc.text->set_sort_order(110);
		register_object(object_desc.text);

		hud_object_desc.timer = std::make_shared<timer_controller_object>(object_desc);
		register_object(hud_object_desc.timer);
	}

	//	速度表示パネル
	{
		roomba_speed_controller_object::object_descs object_desc;
		object_desc.name = "ルンバUI：速度表示コントローラー";

		object_desc.image = std::make_shared<ui_image>("速度表示：ベース");
		object_desc.image->set_screen_rect({ speed_ui_position.x, speed_ui_position.y, speed_ui_size.x, speed_ui_size.y });
		object_desc.image->set_texture("Assets/Resources/Textures/Hud/SpeedMeter.png");
		object_desc.image->set_draw_mode(ui_image::draw_modes::sliced);
		object_desc.image->set_border({0,0,0,0});
		object_desc.image->set_sort_order(100);
		register_object(object_desc.image);

		object_desc.text = std::make_shared<ui_text>("速度表示：数値", font);
		object_desc.text->set_texts("0.0");
		object_desc.text->set_screen_rect({ speed_ui_position.x + 230, speed_ui_position.y + 20, 64, 64 });
		object_desc.text->set_font_size(64);
		object_desc.text->set_sort_order(110);
		register_object(object_desc.text);

		hud_object_desc.speed = std::make_shared<roomba_speed_controller_object>(object_desc);
		register_object(hud_object_desc.speed);
	}

	hud_object_desc.name = "ルンバ：メインHUD";
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
