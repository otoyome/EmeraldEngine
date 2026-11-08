#include "Process/game_process.h"
#include "framework.h"
#include "Texture/texture_factory.h"
#include "Graphics/render_data.h"

#include <algorithm>
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

#include "scenes/load_shader_scene.h"

#include "Renderer/scene_renderer.h"
#include "Renderer/scene_renderer_forward.h"
#include "Renderer/scene_renderer_postprocess.h"
#include "Renderer/scene_renderer_display.h"
#include "Renderer/ui_renderer.h"
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
#endif // defined(USE_IMGUI)
#include "System/asset_browser.h"
#include "System/imgui_texture_keeper.h"
#include "Editor/texture_editor.h"

#include "Editor/detail_profiler.h"
#include "System/console_manager.h"
#include "System/project_path.h"


bool game_process::enter()
{
	framework& self = get_owner();

	//	シーン関係
	{
		//	メインのワールド
		self.main_world = std::make_unique<world>();

#if defined(USE_IMGUI)
		//	ゲーム進行のデバッグコマンド
		self.pause_command = std::make_unique<console_command>("game.pause", "一時停止を切り替える（game.pause [0|1]）",
			[&self](const console_command::arguments& args)
			{
				if (!self.main_world)
					return;
				object_manager& objects = self.main_world->get_objects();
				bool pause = !objects.get_pause();
				if (!args.empty())
					console_detail::from_string(args[0], pause);
				objects.set_pause(pause);
				console_manager::instance().print(pause ? "pause : 1" : "pause : 0");
			});
		self.step_command = std::make_unique<console_command>("game.step", "一時停止中に1フレーム進める",
			[&self](const console_command::arguments&)
			{
				if (self.main_world)
					self.main_world->get_objects().called_step();
			});
#endif	//	defined(USE_IMGUI)
	}

	//	シーンレンダラー登録
	{
		self.main_renderer = std::make_unique<world_renderer>(self.render_executor, render_output::create_back_buffer());
		if (!self.main_renderer->initialize())
			return false;
#if defined(USE_IMGUI)
		self.main_renderer->set_debug_builder([&self](const render_data_context& data) { self.build_debug_primitives(data); });
#endif	//	defined(USE_IMGUI)
	}

	//	初回シーン登録
	self.main_world->get_scenes().change_scene<load_shader_scene>();
	return true;
}

void game_process::exit()
{
	framework& self = get_owner();
#if defined(USE_IMGUI)
	self.pause_command.reset();
	self.step_command.reset();
#endif	//	defined(USE_IMGUI)
	self.main_world.reset();
	self.main_renderer.reset();
}

void game_process::update(float elapsed_time)
{
	framework& self = get_owner();

	if (input::action_system::is_valid() && self.main_world)
	{
		input::action_system::instance()->update(self.main_world->get_scenes().get_broadcaster());
	}

	//	シーン内の更新
	if (self.main_world)
	{
		DETAIL_PROFILE_CPU_SCOPE("update.scene_manager");
		self.main_world->get_scenes().updates(elapsed_time);
	}

	//	オブジェクト更新
	if (self.main_world)
	{
		DETAIL_PROFILE_CPU_SCOPE("update.object_manager");
		self.main_world->get_objects().updates(elapsed_time);
	}

	//	エフェクト関係更新
	if (self.main_world)
	{
		DETAIL_PROFILE_CPU_SCOPE("update.effect_manager");
		self.main_world->get_effects().update(elapsed_time);
	}
}

void game_process::render(float elapsed_time)
{
	framework& self = get_owner();

	//	描画前処理
	if (self.main_world)
	{
		self.main_world->get_objects().pre_render();
	}

	//	描画用の情報収集
	self.main_renderer->collect(*self.main_world);

	{
		DETAIL_PROFILE_CPU_SCOPE("render.rebuild_pipeline_bindings");
		pipeline_state_manager::instance()->rebuild_pipeline_bindings();
	}

	//	バッファ描画
	{
		//	レンダラー開始処理
		self.render_executor->begin();

		//	ワールドごとの描画
		self.main_renderer->submit_render_jobs(*self.main_world, elapsed_time);

		//	描画コマンドを実行
		{
			DETAIL_PROFILE_CPU_SCOPE("render.execute_commands");
			self.render_executor->render();
		}

		//	描画コマンド実行終了処理
		{
			DETAIL_PROFILE_CPU_SCOPE("render.imgui_and_present");
			self.render_executor->end();
		}
	}

	//	描画後処理
	if (self.main_world)
	{
		self.main_world->get_objects().post_render();
	}
}

#if defined(USE_IMGUI)
void game_process::imgui_render()
{
	framework& self = get_owner();

	//	シーン情報表示
	if (self.main_world)
	{
		self.main_world->get_scenes().imgui_render();
	}

	//	オブジェクト情報表示
	self.main_world->get_objects().imgui_render();

	//	シーン変更
	self.imgui_scene_change();
}
#endif	//	defined(USE_IMGUI)
