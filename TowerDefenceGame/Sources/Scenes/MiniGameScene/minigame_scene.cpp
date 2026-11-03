#include "Scenes/MiniGameScene/minigame_scene.h"

#include "custom_pipeline_state.h"
#include "Audio/audio_manager.h"

#include "Scenes/MiniGameScene/minigame_result_scene.h"

#include "object/camera_object.h"
#include "object/model_object.h"
#include "object/post_process_volume_object.h"

#include "object/gameobjects/minigame/roomba_object.h"
#include "object/gameobjects/minigame/roomba_controller_object.h"
#include "object/gameobjects/minigame/roomba_camera_controller_object.h"
#include "object/gameobjects/minigame/roomba_tutorial_controller_object.h"
#include "object/gameobjects/minigame/minigame_record_data.h"

#include "object/gameobjects/ui/common/transition_fade_controller.h"
#include "object/gameobjects/ui/hud/countdown_controller_object.h"

#include "object/gameobjects/system/start_point_object.h"

namespace
{
	//	ポストプロセスマテリアル
	constexpr material_resource::blendable_locations postprocess_material_path_list[] =
	{
		material_resource::blendable_locations::after_dof,
	};
	constexpr std::string_view postprocess_material_texts_list[ARRAYSIZE(postprocess_material_path_list)][2] =
	{
		{
			"Shaders/Materials/PostProcess/color_filter_after_dof.hlsl",
			"被写界深度後ポストプロセスマテリアル",
		},
	};

	//	開始座標
	constexpr float WarmingUpTimer = 4.0f;
	constexpr float DeadStopWaitTimer = 7.0f;
	constexpr float DeadWaitTimer = 3.0f;
	constexpr float ResultTimer = 5.0f;
	constexpr int WetDamage = 400;

	//	サウンド
	constexpr std::string_view BgmSoundPath = "Assets/Resources/Sounds/minigame_bgm.wav";

	//	各種名前
	constexpr std::string_view RoombaObjectName = "ルンバ 制御";
	constexpr std::string_view RoombaControllerObjectName = "ルンバ 入力";
	constexpr std::string_view RoombaCameraControllerObjectName = "ルンバ カメラ";
}

minigame_scene::minigame_scene()
: scene_base()
{
	//	カーソル関係の非表示
	mouse::instance()->set_center_lock(true);
	mouse::instance()->set_cursol_hide(true);

	//	メインカメラ生成
	std::shared_ptr<camera_object> main_camera;
	{
		main_camera = object_manager::instance()->search_object<camera_object>("メインカメラ");
		main_camera->get_camera()->get_parameters().fov_y = math::angle::to_radian(30.0f);
		main_camera->set_world_translation(math::double3(0, 10, 40));
	}

	//	遷移フェードコントローラーにプレイを要求
	transition_fade_controller::instance()->play(fade::directions::in, 0.5f, fade::modes::black);

	//	ポストプロセス生成
	if (pipeline_state_manager* instance = pipeline_state_manager::instance())
	{
		auto post_process_volume = std::make_shared<post_process_volume_object>("ポストプロセスボリューム");
		post_process_volume->set_flag(post_process_volume::flags::unbind, true);
		{
			auto depth_of_field_constant = post_process_volume->get_volume()->get_depth_of_field_constant();
			auto tone_mapping_constant = post_process_volume->get_volume()->get_tone_mapping_constant();
			tone_mapping_constant.set_tone_map_type(tone_mapping_data::tone_mapping_type::aces_krzysztof_narkowicz);
			post_process_volume->get_volume()->set_depth_of_field_constant(depth_of_field_constant);
		}
		register_object(post_process_volume);

		for (UINT index = 0; index < ARRAYSIZE(postprocess_material_path_list); ++index)
		{
			std::shared_ptr<material_resource> post_process_material = std::make_shared<material_resource>();
			post_process_material->set_name(postprocess_material_texts_list[index][1]);
			post_process_material->set_domain(material_resource::domains::post_process);
			post_process_material->set_flag(material_resource::flags::two_sided, true);
			post_process_material->set_blend_mode(material_resource::blend_modes::opaque);
			post_process_material->set_blendable_location(postprocess_material_path_list[index]);
			post_process_material->set_shader_path(postprocess_material_texts_list[index][0]);

			//	マテリアル登録
			post_process_volume->get_volume()->register_postprocess_materials(post_process_material);

			//	連携させるためにポストプロセスマテリアルを保持しておく
			postprocess_material = post_process_material;
		}
	}

	//	ルンバモデル生成
	{
		//	モデルオブジェクト生成
		auto roomba_model = std::make_shared<model_object>("ルンバ モデル", "Assets/Resources/Models/Roomba/Roomba.emo");
		register_object(roomba_model);

		//	ルンバオブジェクト生成
		player_object = std::make_shared<roomba_object>(RoombaObjectName, roomba_model, "player");
		register_object(player_object);

		//	ルンバ制御オブジェクト生成
		auto roomba_controller = std::make_shared<roomba_controller_object>(RoombaControllerObjectName, player_object, main_camera);
		register_object(roomba_controller);
	}

	//	ゲーム用エフェクト類生成
	{
		effect_resource_manager::instance()->load(global_bubble_effect_resource, "Assets/Resources/Effects/GlobalBubble.eme");
		effect_resource_manager::instance()->load(bubble_effect_resource, "Assets/Resources/Effects/BubbleBlock.eme");
		effect_resource_manager::instance()->load(dust_effect_resource, "Assets/Resources/Effects/DustBlock.eme");
	}

	//	タイマーリセット
	game_timer = 0.0f;
	max_speed_kmh = 0.0f;
	countdown_timer = 0.0f;
	trash_count = 0;
	game_state = game_states::startup;
}

minigame_scene::~minigame_scene()
{
}

bool minigame_scene::initialize_core()
{
	return true;
}

bool minigame_scene::uninitialize_core()
{
	//	BGM を止める
	if (audio_manager::is_valid())
	{
		audio_manager::instance()->stop_bgm(0.5f);
	}

	player_object = nullptr;
	for (auto& effect : bubble_effect)
	{
		if (effect)
			effect_manager::instance()->unregister_instance(effect);
	}
	for (auto& effect : dust_effect)
	{
		if (effect)
			effect_manager::instance()->unregister_instance(effect);
	}
	return true;
}

void minigame_scene::update_core(float elapsed_time)
{
	switch (game_state)
	{	
		case game_states::startup:
		{
			//	BGM 開始
			if (audio_manager::is_valid())
			{
				audio_manager::instance()->play_bgm(BgmSoundPath, 1.0f);
			}

			//	フェードイン開始
			transition_fade_controller::instance()->play(fade::directions::in, 0.5f, fade::modes::black);

			//	関数登録
			if (auto trash_object = object_manager::instance()->search_object<roomba_trash_controller_object>("ルンバUI：ゴミ収集率コントローラー"))
			{
				trash_object->set_getter([this](){ return trash_count; }, [this](){ return trash_max; });
			}

			//	開始処理
			{
				auto start_points = object_manager::instance()->search_objects<start_point_object>();
				for (auto& start_point : start_points)
				{
					if (start_point->get_tag().find("Player Start") != std::string::npos)
					{
						player_object->set_position(start_point->get_world_translation().to_float3());
						auto rot = math::rotatorf(start_point->get_world_euler());
						rot.y -= math::constants<float>::pi_half;
						player_object->set_rotation(rot);
						player_object->get_model_object()->update_transform();
					}
					else if (start_point->get_tag().find("Bubble Point") != std::string::npos)
					{
						bubble_effect.emplace_back(std::make_shared<effect_instance>(bubble_effect_resource));
						bubble_effect.back()->set_position(start_point->get_world_translation());
						bubble_effect.back()->set_rotation(math::rotatorf(start_point->get_world_euler()));
						effect_manager::instance()->register_instance(bubble_effect.back());
					}
					else if (start_point->get_tag().find("Dust Point") != std::string::npos)
					{
						dust_effect.emplace_back(std::make_shared<effect_instance>(dust_effect_resource));
						dust_effect.back()->set_position(start_point->get_world_translation());
						dust_effect.back()->set_rotation(math::rotatorf(start_point->get_world_euler()));
						effect_manager::instance()->register_instance(dust_effect.back());
					}
				}

				//	グローバルバブルを作成
				{
					bubble_effect.emplace_back(std::make_shared<effect_instance>(global_bubble_effect_resource));
					effect_manager::instance()->register_instance(bubble_effect.back());
				}

				//	Dustノードのパーティクル設定からゴミの最大数を収集する
				for (auto& effect : dust_effect)
				{
					auto dust_index = effect->search_node_index_by_name("Dust");
					for (auto& particle : effect->get_particles())
					{
						if (particle.node_index != dust_index)
							continue;

						if (auto renderer = particle.particle.get_item<particle::item::renderer>())
						{
							trash_max += static_cast<int>(renderer->total_particle_count);
						}
					}
				}
			}

			//	プレイヤー入力を止める
			if (auto controller = object_manager::instance()->search_object<roomba_controller_object>(RoombaControllerObjectName))
			{
				controller->set_input_purmit_flag(false);

				//	このタイミングでカメラを作成
				if (auto main_camera = object_manager::instance()->search_object<camera_object>("メインカメラ"))
				{
					auto camera_controller = std::make_shared<roomba_camera_controller_object>(RoombaCameraControllerObjectName, controller, main_camera);
					camera_controller->set_input_purmit_flag(false);
					camera_controller->change_state(roomba_camera_controller_object::states::entry);
					register_object(camera_controller);
				}
			}

			countdown_timer = 0.0f;
			game_state = game_states::warming_up;
			break;
		}
		case game_states::warming_up:
		{
			if (transition_fade_controller::instance()->is_processing())
			{
				return;
			}

			//	ウォーミングアップ待機
			countdown_timer += elapsed_time;
			if (countdown_timer >= WarmingUpTimer)
			{
				countdown_timer = 0.0f;
				transition_fade_controller::instance()->play(fade::directions::out, 0.5f, fade::modes::black);
				game_state = game_states::warming_up_end;
			}
			break;
		}
		case game_states::warming_up_end:
		{
			if (transition_fade_controller::instance()->is_processing())
			{
				return;
			}

			//	カメラの入力のみ許可
			if (auto controller = object_manager::instance()->search_object<roomba_camera_controller_object>(RoombaCameraControllerObjectName))
			{
				controller->set_input_purmit_flag(true);
				controller->change_state(roomba_camera_controller_object::states::free);
			}
			transition_fade_controller::instance()->play(fade::directions::in, 0.5f, fade::modes::black);
			game_state = game_states::countdown_setup;
			break;
		}
		case game_states::countdown_setup:
		{
			//	プレイヤーをアクティブ状態に
			player_object->set_heat_value(0);
			player_object->set_wet_value(0);
			if (auto start_point = object_manager::instance()->search_object<start_point_object>())
			{
				player_object->set_position(start_point->get_world_translation().to_float3());
				player_object->set_rotation(math::rotatorf(start_point->get_world_euler()));
			}
			player_object->change_state(roomba_object::states::active);

			//	カウントダウン中はプレイヤー入力を止める
			if (auto controller = object_manager::instance()->search_object<roomba_controller_object>(RoombaControllerObjectName))
			{
				controller->set_input_purmit_flag(false);
			}

			countdown_timer = 0.0f;
			game_state = game_states::countdown;
			break;
		}
		case game_states::countdown:
		{
			if (transition_fade_controller::instance()->is_processing())
			{
				return;
			}

			countdown_timer += elapsed_time;

			constexpr float count_interval = 1.0f;
			constexpr float start_display_time = 0.8f;
			constexpr float countdown_finish_time = count_interval * 3.0f + start_display_time;
			if (auto countdown = object_manager::instance()->search_object<countdown_controller_object>("ルンバUI：カウントダウン表示コントローラー"))
			{
				if (countdown_timer < count_interval * 1.0f)
					countdown->set_count(3);
				else if (countdown_timer < count_interval * 2.0f)
					countdown->set_count(2);
				else if (countdown_timer < count_interval * 3.0f)
					countdown->set_count(1);
				else if (countdown_timer < countdown_finish_time)
					countdown->set_start();
				else
					countdown->hide();
			}
			if (countdown_timer >= countdown_finish_time)
			{
				if (auto controller = object_manager::instance()->search_object<roomba_controller_object>(RoombaControllerObjectName))
				{
					controller->set_input_purmit_flag(true);
				}
				game_state = game_states::main_game;
			}
			break;
		}
		case game_states::main_game:
		{
			//	スペースキーを押している間だけチュートリアルを表示する
			if (auto tutorial = object_manager::instance()->search_object<roomba_tutorial_controller_object>("ルンバUI：チュートリアル表示コントローラー"))
			{
				auto space_state = input::get_key_state(keyboard::key_states::space);
				auto pad_state = input::get_key_state(gamepad::key_states::back);
				bool space_held = space_state == input::state::down || space_state == input::state::hold ||
								  pad_state == input::state::down || pad_state == input::state::hold;
				tutorial->set_alpha(space_held ? 1.0f : 0.0f);
			}

			//	時間表示更新
			game_timer += elapsed_time;
			if (auto timer_object = object_manager::instance()->search_object<timer_controller_object>("ルンバUI：経過時間表示コントローラー"))
			{
				timer_object->set_time(game_timer);
			}

			//	最高速度を記録
			if (player_object)
			{
				auto velocity = player_object->get_velocity();
				float speed_m_s = system_units::convert_unit_to_m(math::vector3f(velocity).length());
				float speed_kmh = speed_m_s * 3.6f;
				max_speed_kmh = std::max(max_speed_kmh, speed_kmh);
			}

			//	プレイヤー死亡でリスタート処理へ
			if (player_object && player_object->check_same_state(roomba_object::states::dead))
			{
				countdown_timer = 0.0f;
				game_state = game_states::dead_stop_wait;
			}

			//	ゴミ収集率が99%を超えたのでリザルト画面へ
			if (static_cast<float>(trash_count) / trash_max >= 0.99f)
			{
				countdown_timer = 0.0f;
				game_state = game_states::result_start;
			}
			break;
		}
		case game_states::dead_stop_wait:
		{
			float velocity = player_object->get_velocity().x * player_object->get_velocity().x
						   + player_object->get_velocity().y * player_object->get_velocity().y
						   + player_object->get_velocity().z * player_object->get_velocity().z;
			countdown_timer += elapsed_time;

			if (countdown_timer >= DeadStopWaitTimer || velocity <= 0.2f)
			{
				countdown_timer = 0.0f;
				game_state = game_states::dead_time_wait;
			}
			break;
		}
		case game_states::dead_time_wait:
		{
			countdown_timer += elapsed_time;
			if (countdown_timer >= DeadWaitTimer)
			{
				game_state = game_states::countdown_setup;
			}
			break;
		}
		case game_states::result_start:
		{
			if (auto controller = object_manager::instance()->search_object<roomba_controller_object>(RoombaControllerObjectName))
			{
				controller->set_input_purmit_flag(false);
			}
			if (auto controller = object_manager::instance()->search_object<roomba_camera_controller_object>(RoombaCameraControllerObjectName))
			{
				controller->set_input_purmit_flag(false);
				//controller->change_state(roomba_camera_controller_object::states::entry);
			}
			countdown_timer = 0;
			game_state = game_states::result_wait;
			break;
		}
		case game_states::result_wait:
		{
			countdown_timer += elapsed_time;

			constexpr float count_interval = 1.0f;
			constexpr float start_display_time = 0.8f;
			constexpr float countdown_finish_time = count_interval * 4.0f + start_display_time;
			if (auto countdown = object_manager::instance()->search_object<countdown_controller_object>("ルンバUI：カウントダウン表示コントローラー"))
			{
				if (countdown_timer < count_interval * 4.0f)
					countdown->set_finish();
				else
					countdown->hide();
			}
			if (countdown_timer >= countdown_finish_time)
			{
				//	フェードアウト開始
				transition_fade_controller::instance()->play(fade::directions::out, 0.5f, fade::modes::black);

				//	フェードアウトに合わせて BGM も止める
				if (audio_manager::is_valid())
				{
					audio_manager::instance()->stop_bgm(0.5f);
				}
				game_state = game_states::result_fade_out;
			}

			break;
		}
		case game_states::result_fade_out:
		{
			if (!transition_fade_controller::instance()->is_processing())
			{
				//	不要なシーンを破棄
				auto scene_names = scene_manager::instance()->get_valid_scene_names();
				for (auto& name : scene_names)
				{
					if (name.find("minigame_stage") != std::string::npos ||
						name.find("minigame_ui_scene") != std::string::npos ||
						name.find("minigame_scene") != std::string::npos)
					{
						scene_manager::instance()->remove_scene(name);
					}
				}

				//	リザルトシーンへ成績を渡す
				minigame_result_scene::result_data result;
				result.clear_time = game_timer;
				result.trash_count = trash_count;
				result.trash_max = trash_max;
				result.max_speed_kmh = max_speed_kmh;
				minigame_result_scene::set_result(result);

				//	過去の記録として保存
				if (auto record_data = minigame_record_data::instance())
				{
					minigame_record_entry entry;
					entry.clear_time = game_timer;
					entry.max_speed_kmh = max_speed_kmh;
					entry.trash_count = trash_count;
					entry.trash_max = trash_max;
					record_data->add_record(entry);
				}
				scene_manager::instance()->add_scene<minigame_result_scene>();
			}
			break;
		}
	}

	//	プレイヤーの情報を各種機能に伝達
	if (player_object && game_state < game_states::result_start)
	{
		//	吸引座標/範囲設/イベント基準座標設定
		{
			math::vector3d position = player_object->get_push_box()->get_transform().get_translation();
			position.y += player_object->get_push_box()->get_local_scale().y * 0.5f;
			for (auto& effect : bubble_effect)
			{
				if (!effect)
					continue;

				effect->set_attract_position(position);
				effect->set_event_position(position);

				for (auto& particle : effect->get_particles())
				{
					auto attract = particle.particle.get_item<particle::item::attract>();
					if (attract)
					{
						attract->radius = player_object->get_attract_range();
					}
				}
			}
			for (auto& effect : dust_effect)
			{
				if (!effect)
					continue;

				effect->set_attract_position(position);
				effect->set_event_position(position);

				for (auto& particle : effect->get_particles())
				{
					auto attract = particle.particle.get_item<particle::item::attract>();
					if (attract)
					{
						attract->radius = player_object->get_attract_range();
					}
				}
			}
		}

		//	1F前のパーティクルイベント情報を取得
		auto particle_events = effect_manager::instance()->get_particle_events();
		if (!particle_events.empty())
		{
			bubble_emitter_ids.clear();
			dust_emitter_ids.clear();
			//	ゴミか泡のイベントのみ収集
			for (auto& item : effect_manager::instance()->get_items())
			{
				//	泡
				for (auto& effect : bubble_effect)
				{
					if (item.effect_instance == effect)
					{
						for (auto& emitter_item : item.emitter_items)
							bubble_emitter_ids.push_back(emitter_item.system_index);
					}
				}
				//	ゴミ
				for (auto& effect : dust_effect)
				{
					if (item.effect_instance == effect)
					{
						for (auto& emitter_item : item.emitter_items)
							dust_emitter_ids.push_back(emitter_item.system_index);
					}
				}
			}

			//	カウンター関係処理
			int bubble_hits = 0;
			int dust_hits = 0;
			for (auto& event_info : particle_events)
			{
				//	泡の場合はそのまま濡れカウントとして
				bool is_bubble = false;
				for (auto emitter_id : bubble_emitter_ids)
				{
					if (emitter_id == event_info.target_emitter_id)
					{
						is_bubble = true;
						break;
					}
				}
				if (is_bubble)
				{
					player_object->on_wet(WetDamage);
					++bubble_hits;
					continue;
				}

				//	塵の場合はそのまま収集カウントとして
				bool is_dust = false;
				for (auto emitter_id : dust_emitter_ids)
				{
					if (emitter_id == event_info.target_emitter_id)
					{
						is_dust = true;
						break;
					}
				}
				if (is_dust)
				{
					player_object->on_wet(1);
					++trash_count;
					++dust_hits;
					continue;
				}
			}

			//	音は1フレーム分をまとめて渡す
			if (bubble_hits > 0)
				player_object->on_bubble(bubble_hits);
			if (dust_hits > 0)
				player_object->on_trash(dust_hits);
		}

		//	エフェクトと連動
		if (postprocess_material)
		{
			//	熱量に応じて赤く
			float heat_rate = math::saturate(player_object->get_heat_rate() - 0.6f);
			float wet_rate = player_object->get_wet_rate();
			postprocess_material->set_float("vg_intensity", heat_rate);
			postprocess_material->set_float("vg_radius", 0.4f);
			postprocess_material->set_float("vg_softness", 0.6f);
			postprocess_material->set_float3("vg_color", {0.75f, 0.0f, 0.0f});

			//	陽炎もだす
			postprocess_material->set_float("hz_intensity", heat_rate);
			postprocess_material->set_float("hz_scale", 20.0f);
			postprocess_material->set_float("hz_speed", 1.5f);

			//	水分量に応じて水滴を出す
			postprocess_material->set_float("wd_intensity", wet_rate);
			postprocess_material->set_float("wd_scale", 2.0f);
			postprocess_material->set_float("wd_wind_x", 0.0f);
		}
	}
}

#if defined(USE_IMGUI)
void minigame_scene::imgui_render_core()
{
	if (ImGui::TreeNode("minigame_scene"))
	{
		ImGui::TreePop();
	}
}
#endif	//	defined(USE_IMGUI)
