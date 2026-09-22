#include "Scenes/GameScene/minigame_ui_scene.h"

#include "custom_pipeline_state.h"

namespace
{
	//	ヒートゲージ左上位置
	static constexpr DirectX::XMFLOAT2 heat_ui_position = {1200, 500};
	static constexpr DirectX::XMFLOAT2 heat_ui_size = {30, 180};
	//	ウェットゲージ左上位置
	static constexpr DirectX::XMFLOAT2 wet_ui_position = {1240, 500};
	static constexpr DirectX::XMFLOAT2 wet_ui_size = {30, 180};
	//	加速アイコン左上位置
	static constexpr DirectX::XMFLOAT2 accel_icon_ui_position = {1130, 540};
	static constexpr DirectX::XMFLOAT2 accel_icon_ui_size = {36, 36};
	static constexpr DirectX::XMFLOAT2 accel_icon_main_ui_size = {72, 36};
	//	減速アイコン左上位置
	static constexpr DirectX::XMFLOAT2 brake_icon_ui_position = {1130, 612};
	static constexpr DirectX::XMFLOAT2 brake_icon_ui_size = {36, 36};
	static constexpr DirectX::XMFLOAT2 brake_icon_main_ui_size = {72, 36};
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
			desc.objects[index] = std::make_shared<ui_image>("ヒートゲージ:カバー");
			desc.objects[index]->set_screen_rect({ heat_ui_position.x, heat_ui_position.y, heat_ui_size.x, heat_ui_size.y });
			desc.objects[index]->set_color({ 1, 1, 1, 1 });
			desc.objects[index]->set_border({ 0, 0, 1, 1 });
			desc.objects[index]->set_texture("Assets/Resources/Textures/hud/hud_cover.png");
			desc.objects[index]->set_draw_mode(ui_image::draw_modes::sliced);
			desc.objects[index]->set_sort_order(10);
			register_object(desc.objects[index]);
		}
		{
			const int index = static_cast<int>(roomba_gauge_controller_object::ui_id::main);
			desc.objects[index] = std::make_shared<ui_image>("ヒートゲージ:メイン");
			desc.objects[index]->set_screen_rect({ heat_ui_position.x, heat_ui_position.y, heat_ui_size.x, heat_ui_size.y });
			desc.objects[index]->set_color({ 1, 0.25f, 0.25f, 1 });
			desc.objects[index]->set_texture("Assets/Resources/Textures/hud/hud_image.png");
			desc.objects[index]->set_draw_mode(ui_image::draw_modes::sliced);
			desc.objects[index]->set_sort_order(5);
			register_object(desc.objects[index]);
		}
		{
			const int index = static_cast<int>(roomba_gauge_controller_object::ui_id::base);
			desc.objects[index] = std::make_shared<ui_image>("ヒートゲージ:ベース");
			desc.objects[index]->set_screen_rect({ heat_ui_position.x, heat_ui_position.y, heat_ui_size.x, heat_ui_size.y });
			desc.objects[index]->set_color({ 1, 1, 1, 1 });
			desc.objects[index]->set_texture("Assets/Resources/Textures/hud/hud_base.png");
			desc.objects[index]->set_draw_mode(ui_image::draw_modes::sliced);
			desc.objects[index]->set_sort_order(0);
			register_object(desc.objects[index]);
		}
		hud_object_desc.heat_bar = std::make_shared<roomba_gauge_controller_object>(desc);
		hud_object_desc.heat_bar->set_animation_direction(roomba_gauge_controller_object::directions::up);
		register_object(hud_object_desc.heat_bar);
	}

	//	ウェットゲージ
	{
		roomba_gauge_controller_object::object_descs desc;
		desc.name = "ルンバUI：ウェットゲージコントローラー";
		{
			const int index = static_cast<int>(roomba_gauge_controller_object::ui_id::cover);
			desc.objects[index] = std::make_shared<ui_image>("ウェットゲージ:カバー");
			desc.objects[index]->set_screen_rect({ wet_ui_position.x, wet_ui_position.y, wet_ui_size.x, wet_ui_size.y });
			desc.objects[index]->set_color({ 1, 1, 1, 1 });
			desc.objects[index]->set_border({ 0, 0, 1, 1 });
			desc.objects[index]->set_texture("Assets/Resources/Textures/hud/hud_cover.png");
			desc.objects[index]->set_draw_mode(ui_image::draw_modes::sliced);
			desc.objects[index]->set_sort_order(10);
			register_object(desc.objects[index]);
		}
		{
			const int index = static_cast<int>(roomba_gauge_controller_object::ui_id::main);
			desc.objects[index] = std::make_shared<ui_image>("ウェットゲージ:メイン");
			desc.objects[index]->set_screen_rect({ wet_ui_position.x, wet_ui_position.y, wet_ui_size.x, wet_ui_size.y });
			desc.objects[index]->set_color({ 0.25f, 0.25f, 1, 1 });
			desc.objects[index]->set_texture("Assets/Resources/Textures/hud/hud_image.png");
			desc.objects[index]->set_draw_mode(ui_image::draw_modes::sliced);
			desc.objects[index]->set_sort_order(5);
			register_object(desc.objects[index]);
		}
		{
			const int index = static_cast<int>(roomba_gauge_controller_object::ui_id::base);
			desc.objects[index] = std::make_shared<ui_image>("ウェットゲージ:ベース");
			desc.objects[index]->set_screen_rect({ wet_ui_position.x, wet_ui_position.y, wet_ui_size.x, wet_ui_size.y });
			desc.objects[index]->set_color({ 1, 1, 1, 1 });
			desc.objects[index]->set_texture("Assets/Resources/Textures/hud/hud_base.png");
			desc.objects[index]->set_draw_mode(ui_image::draw_modes::sliced);
			desc.objects[index]->set_sort_order(0);
			register_object(desc.objects[index]);
		}
		hud_object_desc.wet_bar = std::make_shared<roomba_gauge_controller_object>(desc);
		hud_object_desc.wet_bar->set_animation_direction(roomba_gauge_controller_object::directions::up);
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
