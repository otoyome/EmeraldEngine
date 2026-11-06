#include "Scenes/MiniGameScene/minigame_result_scene.h"

#include "Scenes/MiniGameScene/minigame_title_scene.h"

#include "object/gameobjects/ui/common/transition_fade_controller.h"

#include "Object/GameObjects/MiniGame/minigame_record_data.h"

#include "Audio/audio_manager.h"

namespace
{
	constexpr std::string_view BgmSoundPath = "Assets/Resources/Sounds/minigame_result_bgm.wav";
	constexpr std::string_view DecideSoundPath = "Assets/Resources/Sounds/ui_decide.wav";

	//	リザルト背景配置位置
	static constexpr math::float2 result_back_position = { 0, 0 };
	static constexpr math::float2 result_back_size = { SCREEN_WIDTH, SCREEN_HEIGHT };

	//	プレイ終了メッセージ配置位置
	static constexpr math::float2 thanks_message_size = { 600, 258 };
	static constexpr math::float2 thanks_message_position = { (SCREEN_WIDTH - thanks_message_size.x) / 2, 220 };

	//	タイトルへ戻る案内アイコン配置位置
	static constexpr math::float2 continue_icon_size = { 120, 60 };
	static constexpr math::float2 continue_icon_position = { (SCREEN_WIDTH - continue_icon_size.x) / 2, SCREEN_HEIGHT - 100 };

	//	リザルト成績表示配置位置
	static constexpr math::float2 result_stats_position = { thanks_message_position.x, thanks_message_position.y + thanks_message_size.y + 20 };
	static constexpr math::float2 result_stats_size = { thanks_message_size.x, 32 };
	static constexpr float result_stats_line_height = 40;

	//	プレイ履歴表示配置位置
	static constexpr math::float2 history_position = { 960, 220 };
	static constexpr math::float2 history_size = { 300, 260 };
	static constexpr float history_font_size = 20.0f;
}

minigame_result_scene::result_data minigame_result_scene::result;

void minigame_result_scene::set_result(const result_data& result)
{
	minigame_result_scene::result = result;
}

minigame_result_scene::minigame_result_scene()
: scene_base()
, decide_action("decide", { keyboard::key_states::space, mouse::key_states::left, gamepad::key_states::a })
{
	//	入力購読
	connections += decide_action.subscribe(get_broadcaster(), [this](const input::action_event& event)
	{
		if (state != states::input_wait)
			return;

		//	決定音を鳴らし、フェードアウトに合わせて BGM も止める
		if (audio_manager::is_valid())
		{
			audio_manager::instance()->play(DecideSoundPath, { audio::bus::ui });
			audio_manager::instance()->stop_bgm(0.5f);
		}
		transition_fade_controller::instance()->play(fade::directions::out, 0.5f, fade::modes::black);
		state = states::fade_out_wait;
	});

	//	マウスカーソルを表示する
	mouse::instance()->set_center_lock(false);
	mouse::instance()->set_cursol_hide(false);

	//	リザルト背景
	{
		auto back = std::make_shared<ui_image>("リザルト背景");
		back->set_screen_rect({ result_back_position.x, result_back_position.y, result_back_size.x, result_back_size.y });
		back->set_texture("Assets/Resources/Textures/hud/result_back.etx");
		back->set_draw_mode(ui_image::draw_modes::single);
		back->set_sort_order(0);
		register_object(back);
	}

	//	プレイ終了メッセージ
	{
		auto message = std::make_shared<ui_image>("プレイ終了メッセージ");
		message->set_screen_rect({ thanks_message_position.x, thanks_message_position.y, thanks_message_size.x, thanks_message_size.y });
		message->set_texture("Assets/Resources/Textures/hud/thunk_you_for_playing.etx");
		message->set_draw_mode(ui_image::draw_modes::single);
		message->set_sort_order(1);
		register_object(message);
	}

	//	成績表示
	{
		font_data_ptr font;
		font_data_manager::instance()->load_from_file(font, "Assets/Resources/Textures/fonts/MS Gothic/MS Gothic.fnt");

		auto clear_time_text = std::make_shared<ui_text>("成績:クリアタイム", font);
		char clear_time_buffer[64];
		sprintf_s(clear_time_buffer, "クリアタイム : %s", system_utility::to_display_time_string(result.clear_time).c_str());
		clear_time_text->set_texts(clear_time_buffer);
		clear_time_text->set_screen_rect({ result_stats_position.x, result_stats_position.y, result_stats_size.x, result_stats_size.y });
		clear_time_text->set_sort_order(1);
		register_object(clear_time_text);

		auto trash_count_text = std::make_shared<ui_text>("成績:回収したゴミ数", font);
		char trash_count_buffer[64];
		sprintf_s(trash_count_buffer, "回収したゴミ : %d / %d", result.trash_count, result.trash_max);
		trash_count_text->set_texts(trash_count_buffer);
		trash_count_text->set_screen_rect({ result_stats_position.x, result_stats_position.y + result_stats_line_height, result_stats_size.x, result_stats_size.y });
		trash_count_text->set_sort_order(1);
		register_object(trash_count_text);

		auto max_speed_text = std::make_shared<ui_text>("成績:最高速度", font);
		char max_speed_buffer[64];
		sprintf_s(max_speed_buffer, "最高速度 : %.1f km/h", result.max_speed_kmh);
		max_speed_text->set_texts(max_speed_buffer);
		max_speed_text->set_screen_rect({ result_stats_position.x, result_stats_position.y + result_stats_line_height * 2, result_stats_size.x, result_stats_size.y });
		max_speed_text->set_sort_order(1);
		register_object(max_speed_text);
	}

	//	プレイ履歴
	{
		font_data_ptr font;
		font_data_manager::instance()->load_from_file(font, "Assets/Resources/Textures/fonts/MS Gothic/MS Gothic.fnt");

		std::string history_text = "プレイ履歴";
		if (auto record_data = minigame_record_data::instance())
		{
			const auto& records = record_data->get_records();
			for (size_t index = 0; index < records.size(); ++index)
			{
				char line[128];
				sprintf_s(line, "%d) %s / %.1fkm/h / %d-%d",
					static_cast<int>(index + 1),
					system_utility::to_display_time_string(records[index].clear_time).c_str(),
					records[index].max_speed_kmh,
					records[index].trash_count,
					records[index].trash_max);
				history_text += "\n";
				history_text += line;
			}
		}

		auto history_text_object = std::make_shared<ui_text>("プレイ履歴表示", font);
		history_text_object->set_texts(history_text);
		history_text_object->set_font_size(history_font_size);
		history_text_object->set_screen_rect({ history_position.x, history_position.y, history_size.x, history_size.y });
		history_text_object->set_sort_order(1);
		register_object(history_text_object);
	}

	//	タイトルへ戻る案内アイコン
	{
		auto continue_icon = std::make_shared<ui_image>("タイトルへ戻る案内アイコン");
		continue_icon->set_screen_rect({ continue_icon_position.x, continue_icon_position.y, continue_icon_size.x, continue_icon_size.y });
		continue_icon->set_texture("Assets/Resources/Textures/hud/keyboard_space_icon.etx");
		continue_icon->set_draw_mode(ui_image::draw_modes::single);
		continue_icon->set_sort_order(1);
		register_object(continue_icon);
	}

	//	遷移プレイ
	transition_fade_controller::instance()->play(fade::directions::in, 0.5f, fade::modes::black);

	//	BGM 開始
	if (audio_manager::is_valid())
	{
		audio_manager::instance()->play_bgm(BgmSoundPath, 0.5f);
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
	if (audio_manager::is_valid())
	{
		audio_manager::instance()->stop_bgm(0.5f);
	}
	return true;
}

void minigame_result_scene::update_core(float elapsed_time)
{
	switch (state)
	{
		case states::fade_in_wait:
		{
			if (!transition_fade_controller::instance()->is_processing())
			{
				state = states::input_wait;
			}
			break;
		}
		case states::input_wait:
		{
			break;
		}
		case states::fade_out_wait:
		{
			if (!transition_fade_controller::instance()->is_processing())
			{
				scene_manager::instance()->remove_scene<minigame_result_scene>();
				scene_manager::instance()->add_scene<minigame_title_scene>();
			}
			break;
		}
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
