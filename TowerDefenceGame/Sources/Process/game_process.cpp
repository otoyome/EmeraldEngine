#include "Process/game_process.h"
#include "Scenes/scene_manager.h"
#include "Scenes/scene_paths.h"
#include "framework.h"
#include "Framework/app_window.h"
#include "Texture/texture_factory.h"
#include "Graphics/Core/render_data.h"

#include <algorithm>
#include <filesystem>
#include <future>
#include <thread>

#include "Graphics/Pipeline/shader.h"
#include "Material/material_shader_map.h"
#include "Texture/texture.h"
#include "Texture/texture_defaults.h"
#include "Texture/texture_importer.h"
#include "Texture/texture_tracker.h"
#include "Graphics/UI/font_data.h"
#include "Graphics/View/camera.h"
#include "Graphics/Culling/occlusion_culling_system.h"
#include "Graphics/Lighting/clustered_lighting_system.h"
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
#include "Graphics/Debug/debug_primitive_collector.h"
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
	console_variable<std::string> game_startup_scene_variable("game.startup_scene", "", "ゲームで起動時に読むシーン", console_flags::user_setting);

	std::string get_startup_scene_path()
	{
		std::string path = game_startup_scene_variable.get();
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

#if defined(USE_IMGUI)
game_process::game_process(framework& owner, std::shared_ptr<play_session> session)
: framework_process(owner)
, session(std::move(session))
{
}
#endif	//	defined(USE_IMGUI)

game_process::~game_process()
{
}

bool game_process::is_closed() const
{
#if defined(USE_IMGUI)
	//	エディターの再生は停止の依頼で終わる
	if (session && session->stop_requested)
		return true;
#endif	//	defined(USE_IMGUI)
	return false;
}

bool game_process::enter()
{
	framework& self = get_owner();

	//	メインワールド登録
	main_world = std::make_unique<world>();

	{
		//	メインのワールドの描画
		main_renderer = std::make_unique<world_renderer>(self.renderer->get_executor());
		if (!main_renderer->initialize(render_target_view(render_data::instance()->get_swap_chain())))
			return false;
	}

#if defined(USE_IMGUI)
	//	エディターの再生中だけ Windows のウィンドウを表示する（終わったら exit で隠す）
	if (session)
		self.window.set_visible(true);
#endif	//	defined(USE_IMGUI)

	//	シーン関係
	{
#if defined(USE_IMGUI)
		//	ゲーム進行のデバッグコマンド
		pause_command = std::make_unique<console_command>("game.pause", "一時停止を切り替える（game.pause [0|1]）",
			[this](const console_command::arguments& args)
			{
				if (!main_world)
					return;
				object_manager& objects = main_world->get_objects();
				bool pause = !objects.get_pause();
				if (!args.empty())
					console_detail::from_string(args[0], pause);
				objects.set_pause(pause);
				console_manager::instance().print(pause ? "pause : 1" : "pause : 0");
			});
		step_command = std::make_unique<console_command>("game.step", "一時停止中に1フレーム進める",
			[this](const console_command::arguments&)
			{
				if (main_world)
					main_world->get_objects().called_step();
			});
#endif	//	defined(USE_IMGUI)
	}

#if defined(USE_IMGUI)
	//	エディターの再生では渡されたシーンから始める
	if (session)
	{
		main_world->get_scenes().add_scene(session->scene);
	}
	else
#endif	//	defined(USE_IMGUI)
	{
		//	初回シーン登録
		main_world->get_scenes().add_scene(get_startup_scene_path());
	}
	return true;
}

void game_process::exit()
{
#if defined(USE_IMGUI)
	pause_command.reset();
	step_command.reset();
#endif	//	defined(USE_IMGUI)
	main_world.reset();

	main_renderer.reset();

	//	シーン関係

#if defined(USE_IMGUI)
	//	エディターの再生が終わったら Windows のウィンドウを隠し、編集用のプロセスに知らせる
	if (session)
	{
		get_owner().window.set_visible(false);
		session->finished = true;
	}
#endif	//	defined(USE_IMGUI)
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
	if (!main_world)
		return;

#if defined(USE_IMGUI)
	//	エディターから届いた「開いてもらうファイル」を再生中のワールドに渡す
	if (session && !session->open_file.empty())
	{
		main_world->get_context().set_open_file(std::move(session->open_file));
		session->open_file.clear();
	}
#endif	//	defined(USE_IMGUI)

	if (input::action_system::is_valid())
	{
		input::action_system::instance()->update(main_world->get_scenes().get_broadcaster());
	}

	//	シーンの更新
	{
		DETAIL_PROFILE_CPU_SCOPE("update.scene_manager");
		main_world->get_scenes().updates(elapsed_time);
	}

	//	オブジェクト更新
	{
		DETAIL_PROFILE_CPU_SCOPE("update.object_manager");
		main_world->get_objects().updates(elapsed_time);
	}

	//	シーンの更新
	{
		DETAIL_PROFILE_CPU_SCOPE("update.scene_manager.post");
		main_world->get_scenes().post_updates(elapsed_time);
	}

	//	エフェクト関係更新
	{
		DETAIL_PROFILE_CPU_SCOPE("update.effect_manager");
		main_world->get_effects().update(elapsed_time);
	}
}

void game_process::render(float elapsed_time)
{
	if (!main_world)
		return;

	//	描画前処理
	main_world->get_objects().pre_render();

	//	描画用の情報収集
	main_renderer->collect(*main_world);
#if defined(USE_IMGUI)
	main_renderer->collect_debug_primitives(*main_world, debug_view_settings_manager::instance()->get("game"));
#endif	//	defined(USE_IMGUI)

	{
		DETAIL_PROFILE_CPU_SCOPE("render.rebuild_pipeline_bindings");
		pipeline_state_manager::instance()->rebuild_pipeline_bindings();
	}
}

void game_process::submit_render_jobs(float elapsed_time)
{
	//	ワールドごとの描画
	if (main_world)
		main_renderer->submit_render_jobs(*main_world, elapsed_time);
}

void game_process::post_render()
{
	//	描画後処理
	if (main_world)
		main_world->get_objects().post_render();
}

#if defined(USE_IMGUI)
void game_process::imgui_render()
{
	if (!main_world)
		return;

	//	シーン情報表示
	main_world->get_scenes().imgui_render();

	//	オブジェクト情報表示
	main_world->get_objects().imgui_render();
}
#endif	//	defined(USE_IMGUI)
