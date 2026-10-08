#include "Scenes/MiniGameScene/minigame_scene.h"

#include "custom_pipeline_state.h"

#include "object/camera_object.h"
#include "object/model_object.h"
#include "object/post_process_volume_object.h"

#include "object/gameobjects/minigame/roomba_object.h"
#include "object/gameobjects/minigame/roomba_controller_object.h"
#include "object/gameobjects/minigame/roomba_camera_controller_object.h"

#include "Scenes/MiniGameScene/minigame_result_scene.h"
#include "object/gameobjects/ui/common/transition_fade_controller.h"
#include "object/gameobjects/ui/hud/countdown_controller_object.h"
#include "object/gameobjects/minigame/roomba_tutorial_controller_object.h"
#include "object/gameobjects/minigame/minigame_record_data.h"

namespace
{
	//	ポストプロセスマテリアル
	constexpr material::blendable_locations postprocess_material_path_list[] =
	{
		material::blendable_locations::after_dof,
	};
	constexpr std::string_view postprocess_material_texts_list[ARRAYSIZE(postprocess_material_path_list)][4] =
	{
		{
			"after_dof_color_filter",
			"Shaders/FullscreenQuad/fullscreen_quad_vs.hlsl",
			"Shaders/Postprocess/color_filter_after_dof_ps.hlsl",
			"被写界深度後ポストプロセスマテリアル",
		},
	};

	//	開始座標
	constexpr math::double3 StartPosition = system_units::convert_m_to_unit(math::double3(0.0, 0.0, -60.0));
	constexpr math::rotatorf StartRotation = {0, 0, 0};
	constexpr float WarmingUpTimer = 4.0f;
	constexpr float DeadStopWaitTimer = 7.0f;
	constexpr float DeadWaitTimer = 3.0f;
	constexpr float ResultTimer = 5.0f;
	constexpr int WetDamage = 400;
	constexpr float DustSetLength = 40;
	constexpr float BubbleSetLength = DustSetLength / 2;
	constexpr math::double3 DustBlock[] =
	{
		system_units::convert_m_to_unit(math::double3(-DustSetLength, 0.0, -DustSetLength)),
		system_units::convert_m_to_unit(math::double3(+DustSetLength, 0.0, -DustSetLength)),
		system_units::convert_m_to_unit(math::double3(           0.0, 0.0,            0.0)),
		system_units::convert_m_to_unit(math::double3(-DustSetLength, 0.0, +DustSetLength)),
		system_units::convert_m_to_unit(math::double3(+DustSetLength, 0.0, +DustSetLength)),
	};
	constexpr math::double3 BubbleBlock[] =
	{
		system_units::convert_m_to_unit(math::double3(-BubbleSetLength, 0, -BubbleSetLength)),
		system_units::convert_m_to_unit(math::double3(+BubbleSetLength, 0, -BubbleSetLength)),
		system_units::convert_m_to_unit(math::double3(-BubbleSetLength, 0, +BubbleSetLength)),
		system_units::convert_m_to_unit(math::double3(+BubbleSetLength, 0, +BubbleSetLength)),
	};

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
		main_camera->get_camera()->set_fov_y(math::angle::to_radian(30.0f));
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
			auto bloom_constant = post_process_volume->get_volume()->get_bloom_constant();
			auto lens_ghosts_constant = post_process_volume->get_volume()->get_lens_ghosts_constant();
			auto depth_of_field_constant = post_process_volume->get_volume()->get_depth_of_field_constant();
			auto screen_space_reflection_constant = post_process_volume->get_volume()->get_screen_space_reflection_constant();
			auto motion_blur_constant = post_process_volume->get_volume()->get_motion_blur_constant();
			auto exposure_compensation_constant = post_process_volume->get_volume()->get_exposure_compensation_constant();
			bloom_constant.set_active(false);
			lens_ghosts_constant.set_active(false);
			depth_of_field_constant.set_active(false);
			screen_space_reflection_constant.set_active(false);
			exposure_compensation_constant.set_active(false);
			post_process_volume->get_volume()->set_bloom_constant(bloom_constant);
			post_process_volume->get_volume()->set_lens_ghosts_constant(lens_ghosts_constant);
			post_process_volume->get_volume()->set_depth_of_field_constant(depth_of_field_constant);
			post_process_volume->get_volume()->set_screen_space_reflection_constant(screen_space_reflection_constant);
			post_process_volume->get_volume()->set_motion_blur_constant(motion_blur_constant);
			post_process_volume->get_volume()->set_exposure_compensation_constant(exposure_compensation_constant);
		}
		register_object(post_process_volume);

		for (UINT index = 0; index < ARRAYSIZE(postprocess_material_path_list); ++index)
		{
			std::shared_ptr<graphics_custom_pipeline_state> pso;
			instance->generate_pipeline_state<graphics_custom_pipeline_state>(postprocess_material_texts_list[index][0], &pso);
			pso->set_domain(pipeline_state_base::domains::post_process);

			//	頂点シェーダー読み込み
			{
				load_shader_file desc;
				desc.file_path = postprocess_material_texts_list[index][1];
#if defined(_DEBUG)
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::disable_optimize, true);
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::debug, true);
#endif	//	defined(_DEBUG)

				std::shared_ptr<vertex_shader> shader;
				shader_manager::instance()->load_shader<vertex_shader>(shader, desc);
				pso->set_shader(shader);
			}

			//	ピクセルシェーダー読み込み
			{
				load_shader_file desc;
				desc.file_path = postprocess_material_texts_list[index][2];
#if defined(_DEBUG)
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::disable_optimize, true);
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::debug, true);
#endif	//	defined(_DEBUG)

				std::shared_ptr<pixel_shader> shader;
				shader_manager::instance()->load_shader<pixel_shader>(shader, desc);
				pso->set_shader(shader);
			}

			std::shared_ptr<material> post_process_material = std::make_shared<material>();
			post_process_material->set_name(postprocess_material_texts_list[index][3]);
			post_process_material->set_domain(material::domains::post_process);
			post_process_material->set_flag(material::flags::two_sided, true);
			post_process_material->set_blend_mode(material::blend_modes::opaque);
			post_process_material->set_blendable_location(postprocess_material_path_list[index]);
			post_process_material->set_pipeline_state(postprocess_material_texts_list[index][0]);

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
		player_object->set_position(math::float3(StartPosition));
		player_object->set_rotation(StartRotation);
		register_object(player_object);

		//	ルンバ制御オブジェクト生成
		auto roomba_controller = std::make_shared<roomba_controller_object>(RoombaControllerObjectName, player_object, main_camera);
		register_object(roomba_controller);

		//	ルンバカメラ制御オブジェクト
		auto camera_controller = std::make_shared<roomba_camera_controller_object>(RoombaCameraControllerObjectName, roomba_controller, main_camera);
		register_object(camera_controller);
	}

	//	ゲーム用エフェクト類生成
	{
		effect_resource_ptr resource;
		if (effect_resource_manager::instance()->load(resource, "Assets/Resources/Effects/GlobalBubble.eme"))
		{
			bubble_effect.emplace_back(std::make_shared<effect_instance>(resource));
			effect_manager::instance()->register_instance(bubble_effect.back());
		}
		if (effect_resource_manager::instance()->load(resource, "Assets/Resources/Effects/BubbleBlock.eme"))
		{
			for (auto& block : BubbleBlock)
			{
				bubble_effect.emplace_back(std::make_shared<effect_instance>(resource));
				bubble_effect.back()->set_position(math::float3(block));
				effect_manager::instance()->register_instance(bubble_effect.back());
			}
		}
		if (effect_resource_manager::instance()->load(resource, "Assets/Resources/Effects/DustBlock.eme"))
		{
			for (auto& block : DustBlock)
			{
				dust_effect.emplace_back(std::make_shared<effect_instance>(resource));
				dust_effect.back()->set_position(math::float3(block));
				effect_manager::instance()->register_instance(dust_effect.back());
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
			//	フェードイン開始
			transition_fade_controller::instance()->play(fade::directions::in, 0.5f, fade::modes::black);

			//	関数登録
			if (auto trash_object = object_manager::instance()->search_object<roomba_trash_controller_object>("ルンバUI：ゴミ収集率コントローラー"))
			{
				trash_object->set_getter([this](){ return trash_count; }, [this](){ return trash_max; });
			}

			//	プレイヤー入力を止める
			if (auto controller = object_manager::instance()->search_object<roomba_controller_object>(RoombaControllerObjectName))
			{
				controller->set_input_purmit_flag(false);
			}
			if (auto controller = object_manager::instance()->search_object<roomba_camera_controller_object>(RoombaCameraControllerObjectName))
			{
				controller->set_input_purmit_flag(false);
				controller->change_state(roomba_camera_controller_object::states::entry);
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
			player_object->set_position(math::float3(StartPosition));
			player_object->set_rotation(StartRotation);
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
				auto space_state = keyboard::instance()->get_state().is_key_state(keyboard::key_states::space);
				bool space_held = space_state == input::state::down || space_state == input::state::hold;
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
				game_state = game_states::result_fade_out;
			}

			break;
		}
		case game_states::result_fade_out:
		{
			if (!transition_fade_controller::instance()->is_processing())
			{
				scene_manager::instance()->remove_scene<stage_00_scene>();
				scene_manager::instance()->remove_scene<minigame_ui_scene>();
				scene_manager::instance()->remove_scene<minigame_scene>();

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
			auto position = player_object->get_push_box()->get_transform().get_translation().to_float3();
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
					++trash_count;
					continue;
				}
			}
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
