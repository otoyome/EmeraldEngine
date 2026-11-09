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

#include "Scenes/setup_scene.h"

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
#endif // defined(USE_IMGUI)
#include "System/asset_browser.h"
#include "System/imgui_texture_keeper.h"
#include "Editor/Windows/texture_editor_window.h"

#include "Editor/detail_profiler.h"
#include "System/console_manager.h"
#include "System/project_path.h"


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
	self.main_world = std::make_unique<world>();

	//	シーン関係
	{
#if defined(USE_IMGUI)
		//	ゲーム進行のデバッグコマンド
		pause_command = std::make_unique<console_command>("game.pause", "一時停止を切り替える（game.pause [0|1]）",
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
		step_command = std::make_unique<console_command>("game.step", "一時停止中に1フレーム進める",
			[&self](const console_command::arguments&)
			{
				if (self.main_world)
					self.main_world->get_objects().called_step();
			});

		//	アセットブラウザの開き先
		if (self.editor)
			self.editor->register_asset_open_handlers(self.main_world->get_scenes());

		//	デバッグ表示の収集
		self.main_renderer->set_debug_builder([&self](const render_data_context& data)
			{
				if (self.editor)
					self.editor->build_debug_primitives(self.main_world.get(), data);
			});
#endif	//	defined(USE_IMGUI)
	}

	//	初回シーン登録
	self.main_world->get_scenes().change_scene<setup_scene>();
	return true;
}

void game_process::exit()
{
	framework& self = get_owner();
#if defined(USE_IMGUI)
	pause_command.reset();
	step_command.reset();
#endif	//	defined(USE_IMGUI)
	self.main_world.reset();

	self.main_renderer.reset();

	//	シーン関係
	collision_filter_manager::delete_instance();
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

}

void game_process::submit_render_jobs(float elapsed_time)
{
	framework& self = get_owner();

	//	ワールドごとの描画
	self.main_renderer->submit_render_jobs(*self.main_world, elapsed_time);
}

void game_process::post_render()
{
	framework& self = get_owner();

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
	if (self.editor)
		self.editor->imgui_scene_change(self.main_world->get_scenes());
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
