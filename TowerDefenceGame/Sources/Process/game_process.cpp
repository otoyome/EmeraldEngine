#include "Process/game_process.h"
#include "Process/asset_preview_process.h"
#include "Scenes/scene_manager.h"
#include "Scenes/scene_paths.h"
#include "framework.h"
#include "Texture/texture_factory.h"
#include "Graphics/render_data.h"

#include <algorithm>
#include <filesystem>
#include <future>
#include <thread>

#include "Graphics/shader.h"
#include "Material/material_shader_map.h"
#include "Texture/texture.h"
#include "Texture/texture_defaults.h"
#include "Texture/texture_importer.h"
#include "Texture/texture_tracker.h"
#include "Graphics/font_data.h"
#include "Graphics/camera.h"
#include "graphics/occlusion_culling_system.h"
#include "graphics/clustered_lighting_system.h"
#include "Resource/resource_manager.h"
#include "Model/model_resource.h"
#include "Model/model_buffer_pool.h"
#include "Model/model_instance.h"
#include "Effect/effect_resource.h"
#include "Effect/effect_instance.h"
#include "Audio/audio_manager.h"
#include "Audio/sound_player.h"
#include "Input/input_action_system.h"

#include "Animation/ik_resource.h"


#include "Renderer/scene_renderer.h"
#include "Renderer/scene_renderer_forward.h"
#include "Renderer/scene_renderer_postprocess.h"
#include "Renderer/scene_renderer_display.h"
#include "Renderer/ui_renderer.h"
#include "Renderer/render_system.h"
#include "Renderer/world_renderer.h"

#include "Object/object_manager.h"
#include "Object/world.h"
#include "Object/GameObjects/UI/ui_object.h"

#include "System/engine_config_data.h"
#include "System/game_config_data.h"

#if defined(USE_IMGUI)
#include "System/editor_config_data.h"
#include "Graphics/debug_primitive_collector.h"
#include "Editor/editor_items.h"
#include "Editor/editor_camera.h"
#include "Editor/editor_host.h"
#include "Editor/scene_editor.h"
#endif // defined(USE_IMGUI)
#include "System/asset_browser.h"
#include "System/imgui_texture_keeper.h"
#include "Editor/Windows/texture_editor_window.h"

#include "Editor/detail_profiler.h"
#include "System/console_manager.h"
#include "System/project_path.h"


namespace
{
	//	起動時に読むシーン
#if defined(USE_IMGUI)
	console_variable<std::string> editor_startup_scene_variable("editor.startup_scene", "", "エディターで起動時に読むシーン", console_flags::persist);
#endif	//	defined(USE_IMGUI)
	console_variable<std::string> game_startup_scene_variable("game.startup_scene", "", "ゲームで起動時に読むシーン", console_flags::user_setting);

	std::string get_startup_scene_path()
	{
#if defined(USE_IMGUI)
		std::string path = editor_startup_scene_variable.get();
#else	//	defined(USE_IMGUI)
		std::string path = game_startup_scene_variable.get();
#endif	//	defined(USE_IMGUI)
		std::error_code error;
		if (path.empty() || !std::filesystem::exists(std::filesystem::path(path), error))
			path = std::string(scene_paths::default_scene);
		return path;
	}
}

game_process::game_process(framework& owner)
: framework_process(owner)
{
}

game_process::~game_process()
{
}

bool game_process::enter()
{
	framework& self = get_owner();

	//	当たり判定フィルター生成
	collision_filter_manager::create_instance();
	if (!create_collision_filters())
		return false;

	//	メインワールド登録
#if defined(USE_IMGUI)
	//	エディターでは編集用と再生用のワールドを scene_editor が持つ
	if (self.editor)
		scene_edit = std::make_unique<scene_editor>();
	else
#endif	//	defined(USE_IMGUI)
		main_world = std::make_unique<world>();

	//	メインのワールドの描画
	main_renderer = std::make_unique<world_renderer>(self.renderer->get_executor());
	if (!main_renderer->initialize(render_target_view(render_data::instance()->get_swap_chain())))
		return false;

	//	シーン関係
	{
#if defined(USE_IMGUI)
		//	ゲーム進行のデバッグコマンド
		pause_command = std::make_unique<console_command>("game.pause", "一時停止を切り替える（game.pause [0|1]）",
			[this](const console_command::arguments& args)
			{
				world* target = get_active_world();
				if (!target)
					return;
				object_manager& objects = target->get_objects();
				bool pause = !objects.get_pause();
				if (!args.empty())
					console_detail::from_string(args[0], pause);
				objects.set_pause(pause);
				console_manager::instance().print(pause ? "pause : 1" : "pause : 0");
			});
		step_command = std::make_unique<console_command>("game.step", "一時停止中に1フレーム進める",
			[this](const console_command::arguments&)
			{
				if (world* target = get_active_world())
					target->get_objects().called_step();
			});

		//	アセットブラウザの開き先
		if (self.editor)
		{
			self.editor->register_asset_open_handlers(
				[this](std::string_view file_path) { scene_edit->request_edit(file_path); },
				[this](std::string_view scene_path, std::string_view open_file) { scene_edit->request_play(scene_path, open_file); });
		}


		//	モデルエディター
		if (asset_browser::is_valid())
		{
			asset_browser::open_handler handler;
			handler.label = "Model Editor (Window)";
			handler.extensions = { ".emo", ".pmx", ".gltf", ".glb" };
			handler.open_file = [&self](const cache_string& file_path)
			{
				if (!self.has_process(asset_preview_process::make_key("model_editor", file_path)))
					self.request_process(std::make_unique<asset_preview_process>(self, file_path, "model_editor",
						[](scene_manager& scenes) { scenes.add_scene(scene_paths::model_editor); }));
			};
			asset_browser::instance()->register_open_handler(std::move(handler));
		}

		//	パーティクルエディター
		if (asset_browser::is_valid())
		{
			asset_browser::open_handler handler;
			handler.label = "Particle Editor (Window)";
			handler.extensions = { ".eme" };
			handler.open_file = [&self](const cache_string& file_path)
			{
				if (!self.has_process(asset_preview_process::make_key("particle_editor", file_path)))
					self.request_process(std::make_unique<asset_preview_process>(self, file_path, "particle_editor",
						[](scene_manager& scenes) { scenes.add_scene(scene_paths::particle_editor); }));
			};
			asset_browser::instance()->register_open_handler(std::move(handler));
		}

		//	マテリアルエディター
		if (asset_browser::is_valid())
		{
			asset_browser::open_handler handler;
			handler.label = "Material Editor (Window)";
			handler.extensions = { ".emr", ".emo", ".eme" };
			handler.open_file = [&self](const cache_string& file_path)
			{
				if (!self.has_process(asset_preview_process::make_key("material_editor", file_path)))
					self.request_process(std::make_unique<asset_preview_process>(self, file_path, "material_editor",
						[](scene_manager& scenes) { scenes.add_scene(scene_paths::material_editor); }));
			};
			asset_browser::instance()->register_open_handler(std::move(handler));
		}
#endif	//	defined(USE_IMGUI)
	}

#if defined(USE_IMGUI)
	//	エディターでは編集用に開く
	if (scene_edit)
	{
		scene_edit->request_edit(get_startup_scene_path());
	}
	else
#endif	//	defined(USE_IMGUI)
	{
		//	初回シーン登録
		main_world->get_scenes().add_scene(get_startup_scene_path());
	}
	return true;
}

world* game_process::get_active_world()
{
#if defined(USE_IMGUI)
	//	シーン編集中はシーンエディター側の物を渡す
	if (scene_edit)
		return &scene_edit->get_active_world();
#endif	//	defined(USE_IMGUI)
	return main_world.get();
}

void game_process::exit()
{
#if defined(USE_IMGUI)
	pause_command.reset();
	step_command.reset();
	scene_edit.reset();
#endif	//	defined(USE_IMGUI)
	main_world.reset();

	main_renderer.reset();

	//	シーン関係
	collision_filter_manager::delete_instance();
}

bool game_process::create_display_resources()
{
	//	描画情報生成
	if (!main_renderer)
		return true;
	return main_renderer->create_targets(render_target_view(render_data::instance()->get_swap_chain()));
}

void game_process::release_display_resources()
{
	//	描画情報解放
	if (main_renderer)
		main_renderer->release_targets();
}

void game_process::update(float elapsed_time)
{
#if defined(USE_IMGUI)
	//	シーンエディターのリクエストを対応
	if (scene_edit)
		scene_edit->apply_requests();
#endif	//	defined(USE_IMGUI)

	world* active_world = get_active_world();
	if (!active_world)
		return;

	if (input::action_system::is_valid())
	{
		input::action_system::instance()->update(active_world->get_scenes().get_broadcaster());
	}

	//	シーンの更新
	{
		DETAIL_PROFILE_CPU_SCOPE("update.scene_manager");
		active_world->get_scenes().updates(elapsed_time);
	}

	//	オブジェクト更新
	{
		DETAIL_PROFILE_CPU_SCOPE("update.object_manager");
		active_world->get_objects().updates(elapsed_time);
	}

	//	シーンの更新
	{
		DETAIL_PROFILE_CPU_SCOPE("update.scene_manager.post");
		active_world->get_scenes().post_updates(elapsed_time);
	}

#if defined(USE_IMGUI)
	//	編集中のシステムの更新
	{
		DETAIL_PROFILE_CPU_SCOPE("update.scene_edit.editor_update");
		if (scene_edit)
			scene_edit->editor_update(elapsed_time);
	}
#endif	//	defined(USE_IMGUI)

	//	エフェクト関係更新
	{
		DETAIL_PROFILE_CPU_SCOPE("update.effect_manager");
		active_world->get_effects().update(elapsed_time);
	}
}

void game_process::render(float elapsed_time)
{
	world* active_world = get_active_world();
	if (!active_world)
		return;

	//	描画前処理
	active_world->get_objects().pre_render();

	//	描画用の情報収集
	main_renderer->collect(*active_world);
#if defined(USE_IMGUI)
	main_renderer->collect_debug_primitives(*active_world, debug_view_settings_manager::instance()->get("game"));
#endif	//	defined(USE_IMGUI)

	{
		DETAIL_PROFILE_CPU_SCOPE("render.rebuild_pipeline_bindings");
		pipeline_state_manager::instance()->rebuild_pipeline_bindings();
	}

}

void game_process::submit_render_jobs(float elapsed_time)
{
	//	ワールドごとの描画
	if (world* active_world = get_active_world())
		main_renderer->submit_render_jobs(*active_world, elapsed_time);
}

void game_process::post_render()
{
	//	描画後処理
	if (world* active_world = get_active_world())
	{
		active_world->get_objects().post_render();
	}
}

#if defined(USE_IMGUI)
void game_process::imgui_render()
{
	framework& self = get_owner();

	//	メインの画面のツールバー
	if (ImGui::BeginMainMenuBar())
	{
		debug_view_settings_manager::instance()->get("game").imgui_menu();
		if (scene_edit)
			scene_edit->imgui_menu();
		ImGui::EndMainMenuBar();
	}

	world* active_world = get_active_world();
	if (!active_world)
		return;

	//	シーン情報表示
	if (!scene_edit || scene_edit->is_playing())
		active_world->get_scenes().imgui_render();

	//	オブジェクト情報表示
	active_world->get_objects().imgui_render();

	//	編集中のシーンの窓
	if (scene_edit)
		scene_edit->imgui_render();
}
#endif	//	defined(USE_IMGUI)

bool game_process::create_collision_filters()
{
	//	プレイヤー用
	{
		std::shared_ptr<collision_filter> filter = std::make_shared<collision_filter>();
		filter->set_name("player");
		filter->set_category(collision_category::player);
		filter->set_flag(collision_category::player, false);
		filter->set_flag(collision_category::player_attack, false);

		filter->set_flag(collision_category::enemy, true);
		filter->set_flag(collision_category::enemy_attack, false);

		filter->set_flag(collision_category::stage, true);
		filter->set_flag(collision_category::camera, false);

		filter->set_flag(collision_category::ik, false);
		collision_filter_manager::instance()->register_filter(filter);
	}
	//	プレイヤー攻撃用
	{
		std::shared_ptr<collision_filter> filter = std::make_shared<collision_filter>();
		filter->set_name("player_attack");
		filter->set_category(collision_category::player_attack);
		filter->set_flag(collision_category::player, false);
		filter->set_flag(collision_category::player_attack, false);

		filter->set_flag(collision_category::enemy, true);
		filter->set_flag(collision_category::enemy_attack, false);

		filter->set_flag(collision_category::stage, false);
		filter->set_flag(collision_category::camera, false);

		filter->set_flag(collision_category::ik, false);
		collision_filter_manager::instance()->register_filter(filter);
	}
	//	エネミー用
	{
		std::shared_ptr<collision_filter> filter = std::make_shared<collision_filter>();
		filter->set_name("enemy");
		filter->set_category(collision_category::enemy);
		filter->set_flag(collision_category::player, true);
		filter->set_flag(collision_category::player_attack, true);

		filter->set_flag(collision_category::enemy, false);
		filter->set_flag(collision_category::enemy_attack, false);

		filter->set_flag(collision_category::stage, true);
		filter->set_flag(collision_category::camera, false);

		filter->set_flag(collision_category::ik, false);
		collision_filter_manager::instance()->register_filter(filter);
	}
	//	エネミー攻撃用
	{
		std::shared_ptr<collision_filter> filter = std::make_shared<collision_filter>();
		filter->set_name("enemy_attack");
		filter->set_category(collision_category::enemy_attack);
		filter->set_flag(collision_category::player, true);
		filter->set_flag(collision_category::player_attack, false);

		filter->set_flag(collision_category::enemy, false);
		filter->set_flag(collision_category::enemy_attack, false);

		filter->set_flag(collision_category::stage, true);
		filter->set_flag(collision_category::camera, false);

		filter->set_flag(collision_category::ik, false);
		collision_filter_manager::instance()->register_filter(filter);
	}
	//	ステージ用
	{
		std::shared_ptr<collision_filter> filter = std::make_shared<collision_filter>();
		filter->set_name("stage");
		filter->set_category(collision_category::stage);
		filter->set_flag(collision_category::player, true);
		filter->set_flag(collision_category::player_attack, true);

		filter->set_flag(collision_category::enemy, true);
		filter->set_flag(collision_category::enemy_attack, true);

		filter->set_flag(collision_category::stage, true);
		filter->set_flag(collision_category::camera, true);

		filter->set_flag(collision_category::ik, false);
		collision_filter_manager::instance()->register_filter(filter);
	}
	//	カメラ用
	{
		std::shared_ptr<collision_filter> filter = std::make_shared<collision_filter>();
		filter->set_name("camera");
		filter->set_category(collision_category::camera);
		filter->set_flag(collision_category::player, false);
		filter->set_flag(collision_category::player_attack, false);

		filter->set_flag(collision_category::enemy, false);
		filter->set_flag(collision_category::enemy_attack, false);

		filter->set_flag(collision_category::stage, true);
		filter->set_flag(collision_category::camera, false);

		filter->set_flag(collision_category::ik, false);
		collision_filter_manager::instance()->register_filter(filter);
	}
	//	非表示中用
	{
		std::shared_ptr<collision_filter> filter = std::make_shared<collision_filter>();
		filter->set_name("invisible");
		filter->set_category(collision_category::invisible);
		filter->set_flag(collision_category::player, false);
		filter->set_flag(collision_category::player_attack, false);

		filter->set_flag(collision_category::enemy, false);
		filter->set_flag(collision_category::enemy_attack, false);

		filter->set_flag(collision_category::stage, true);
		filter->set_flag(collision_category::camera, false);

		filter->set_flag(collision_category::ik, false);
		collision_filter_manager::instance()->register_filter(filter);
	}
	//	IK用
	{
		std::shared_ptr<collision_filter> filter = std::make_shared<collision_filter>();
		filter->set_name("ik");
		filter->set_category(collision_category::ik);
		filter->set_flag(collision_category::player, false);
		filter->set_flag(collision_category::player_attack, false);

		filter->set_flag(collision_category::enemy, false);
		filter->set_flag(collision_category::enemy_attack, false);

		filter->set_flag(collision_category::stage, true);
		filter->set_flag(collision_category::camera, false);

		filter->set_flag(collision_category::ik, false);
		collision_filter_manager::instance()->register_filter(filter);
	}

	return true;
}
