#include "framework.h"
#include "render_data.h"

#include <algorithm>
#include <future>
#include <thread>
#include <shellapi.h>

#include "shader.h"
#include "texture.h"
#include "font_data.h"
#include "camera.h"
#include "graphics/occlusion_culling_system.h"
#include "graphics/clustered_lighting_system.h"
#include "graphics/shadow_atlas.h"
#include "model_resource.h"
#include "model_buffer_pool.h"
#include "Graphics/material_constant_buffer_pool.h"
#include "model_instance.h"
#include "material.h"
#include "effect_resource.h"
#include "effect_instance.h"

#include "ik_resource.h"

#include "scenes/load_shader_scene.h"
#if defined(USE_IMGUI)
#include "scenes/editscene/edit_model_scene.h"
#endif	//	defined(USE_IMGUI)

#include "Renderer/scene_render_features.h"

#include "Object/object_manager.h"
#include "Object/GameObjects/UI/ui_object.h"
#include "Object/GameObjects/MiniGame/minigame_record_data.h"

#include "System/engine_config_data.h"
#include "System/game_config_data.h"
#include "System/world_viewport.h"

#if defined(USE_IMGUI)
#include "System/editor_config_data.h"
#include "Graphics/debug_primitive_collector.h"
#include "Editor/editor_items.h"
#include "Editor/editor_camera.h"
#endif // defined(USE_IMGUI)

#include "Editor/detail_profiler.h"
#include "System/console_manager.h"

namespace
{
#if defined(USE_IMGUI)
	//	ゲーム進行のデバッグコマンド
	console_command pause_command("game.pause", "一時停止を切り替える（game.pause [0|1]）",
		[](const console_command::arguments& args)
		{
			if (!object_manager::is_valid())
				return;
			bool pause = !object_manager::instance()->get_pause();
			if (!args.empty())
				console_detail::from_string(args[0], pause);
			object_manager::instance()->set_pause(pause);
			console_manager::instance().print(pause ? "pause : 1" : "pause : 0");
		});
	console_command step_command("game.step", "一時停止中に1フレーム進める",
		[](const console_command::arguments&)
		{
			if (object_manager::is_valid())
				object_manager::instance()->called_step();
		});
#endif	//	defined(USE_IMGUI)
}

framework::framework(app_window& window) : window(window), hwnd(window.get_handle())
{
	logger::create_instance();
}

framework::~framework()
{
	//	@tool 登録したメッセージの処理はこのクラスを参照するので、先に外す
	window.clear_message_hooks();
	logger::delete_instance();
}

int framework::run()
{
	MSG msg{};

	//	ウィンドウ名設定
	SetWindowTextW(hwnd, APPLICATION_NAME);

	if (!initialize())
	{
		return uninitialize();
	}

#if defined(USE_IMGUI)
	editor->imgui_initialize();
#endif	//	defined(USE_IMGUI)

	//	@tool 描画スレッドと提出スレッドを開始する（描画は常にこの2つのスレッドで行う）
	renderer->start_render_threads();

#if defined(USE_IMGUI)
	//	@tool 起動引数 -exec "<コマンド>" のコンソールコマンドを、最初の更新の前に実行する（エディター用の別ウィンドウを開くなど）
	for (int index = 1; index + 1 < __argc; ++index)
	{
		if (std::string_view(__argv[index]) == "-exec")
			console_manager::instance().execute(__argv[++index]);
	}
#endif	//	defined(USE_IMGUI)

	while (WM_QUIT != msg.message)
	{
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			tictoc.tick();

			//	詳細プロファイラーのフレーム開始
			DETAIL_PROFILE_BEGIN_FRAME();
			const float elapsed_time = tictoc.time_interval();
			update(elapsed_time);
			render(elapsed_time);

			//	詳細プロファイラーのフレーム終了
			DETAIL_PROFILE_END_FRAME();
		}
	}

	//	レンダリングスレッド停止
	renderer->stop_render_threads();

#if defined(USE_IMGUI)
	editor->imgui_terminate();
#endif	//	defined(USE_IMGUI)

	BOOL fullscreen{};
	render_data::instance()->get_swap_chain()->GetFullscreenState(&fullscreen, 0);
	if (fullscreen)
	{
		render_data::instance()->get_swap_chain()->SetFullscreenState(FALSE, 0);
	}

	return uninitialize() ? static_cast<int>(msg.wParam) : 0;
}

bool framework::initialize()
{
	HRESULT hr{ S_OK };

	//	COMオブジェクト初期化
	{
		hr = CoInitializeEx(NULL, 0);
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
		if (FAILED(hr))
		{
			return false;
		}
	}

	//	文字コード変換器所持
	system_utility::initialize_charset_converter();

	render_data::create_instance(hwnd);
	texture_manager::create_instance();
	font_data_manager::create_instance();
	shader_manager::create_instance();
	pipeline_state_manager::create_instance();
	model_buffer_pool_manager::create_instance();
	material_constant_buffer_pool::create_instance();
	model_resource_manager::create_instance();
	effect_resource_manager::create_instance();
	ik_resource_manager::create_instance();
	minigame_record_data::create_instance();
	game_config_data::create_instance();
#if defined(USE_IMGUI)
	editor_config_data::create_instance();
	detail_profiler::create_instance();
#endif // defined(USE_IMGUI)

	collision_filter_manager::create_instance();

	//	@tool ウィンドウのメッセージを受け取る処理（ImGui → マウス → 移動・サイズ変更中は経過時間を止める）
#if defined(USE_IMGUI)
	window.add_message_hook(&editor_host::handle_message);
#endif	//	defined(USE_IMGUI)
	window.add_message_hook([this](HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
	{
		//	@tool マウスはワールドごとに持つので、ゲームのワールドの物へ渡す
		if (!main_world)
			return false;
		engine_world::scope world_scope(*main_world);
		return mouse::is_valid() && mouse::instance()->window_proccess(hwnd, msg, wparam, lparam);
	});
	window.add_message_hook([this](HWND, UINT msg, WPARAM, LPARAM)
	{
		if (msg == WM_ENTERSIZEMOVE)
			tictoc.stop();
		else if (msg == WM_EXITSIZEMOVE)
			tictoc.start();
		return false;
	});

	//	ワールド毎に持つ管理クラス
	//	@tool 入力はワールドごとに持ち、ワールドを映すウィンドウに結び付ける
	engine_world::register_scoped_singleton<mouse>();
	engine_world::register_scoped_singleton<keyboard>();
	engine_world::register_scoped_singleton<light_manager>();
	engine_world::register_scoped_singleton<post_process_volume_manager>();
	engine_world::register_scoped_singleton<camera_manager>();
	engine_world::register_scoped_singleton<world_viewport>();
	engine_world::register_scoped_singleton<model_instance_manger>();
	engine_world::register_scoped_singleton<effect_manager>();
#if defined(USE_IMGUI)
	engine_world::register_scoped_singleton<debug_primitive_manager>();
	engine_world::register_scoped_singleton<editor_camera_manager>();
#endif // defined(USE_IMGUI)
	engine_world::register_scoped_singleton<object_manager>([](object_manager* target) { target->finalize(); });
	engine_world::register_scoped_singleton<scene_manager>();

	//	ゲームのワールド
	main_world = std::make_unique<engine_world>("main");
	main_world->make_current();
	mouse::instance()->set_window(hwnd);
	keyboard::instance()->set_window(hwnd);

#if defined(USE_IMGUI)
	editor_items::initialize();
#endif // defined(USE_IMGUI)

	//	@tool 描画（各種バッファ・シーンレンダラー・描画中のフレームを待つ仕組み）
	renderer = std::make_unique<render_system>();
	if (!renderer->initialize())
		return false;

#if defined(USE_IMGUI)
	//	@tool エディター機能
	editor = std::make_unique<editor_host>(hwnd, *renderer);
#endif	//	defined(USE_IMGUI)

	//	当たり判定フィルター用意
	if (!create_collision_filters())
		return false;

	//	初回シーン登録
	scene_manager::instance()->change_scene<load_shader_scene>();
	return true;
}

void framework::update(float elapsed_time/*Elapsed seconds from last frame*/)
{
	//	更新処理全体の計測
	DETAIL_PROFILE_CPU_SCOPE("update");

	//	@tool 前フレームのエフェクト更新（GPU）はエフェクトのインスタンスを読むので、終わるまで待ってから更新する
	renderer->wait_effect_update();

	//	@tool モデルを読み直したら、古い中身を指しているインスタンスを全ワールドで作り直す（古い中身は描画中のフレームが使い終わってから破棄される）
	if (assets.update())
	{
		for_each_world([]
		{
			if (model_instance_manger::is_valid())
				model_instance_manger::instance()->sync_resources();
		});
	}

	//	入力関係の更新
	//	@tool 入力はワールドごとに持つ（それぞれのウィンドウが前面にある時だけ受け付ける）
	for_each_world([]
	{
		if (mouse::is_valid())
		{
			mouse::instance()->update_state();
		}
		if (keyboard::is_valid())
		{
			keyboard::instance()->update_state();
		}
	});

#if defined(USE_IMGUI)
	//	キー割り当てされたコンソールコマンドを実行
	console_manager::instance().update_bindings();
#endif //	defined(USE_IMGUI)

#if defined(USE_IMGUI)
	//	@tool エディター用の別ウィンドウの生成・破棄・描画先の作り直し
	editor->update_editor_windows();
#endif	//	defined(USE_IMGUI)

	//	@tool 各ワールドの画面が入力を受け付けるかを決める
#if defined(USE_IMGUI)
	editor->update_world_viewports();
#endif	//	defined(USE_IMGUI)

	//	@tool ワールドごとの更新（メイン → ImGui のウィンドウに映すワールド）
	for_each_world([this, elapsed_time] { update_world(elapsed_time); });

#if defined(USE_IMGUI)
	editor->build_windows();
#endif // defined(USE_IMGUI)
}

void framework::update_world(float elapsed_time)
{
	//	シーン内の更新
	if (scene_manager::is_valid())
	{
		DETAIL_PROFILE_CPU_SCOPE("update.scene_manager");
		scene_manager::instance()->updates(elapsed_time);
	}

	//	オブジェクト更新
	if (object_manager::is_valid())
	{
		DETAIL_PROFILE_CPU_SCOPE("update.object_manager");
		object_manager::instance()->updates(elapsed_time);
	}

	//	エフェクト関係更新
	if (effect_manager::is_valid())
	{
		DETAIL_PROFILE_CPU_SCOPE("update.effect_manager");
		effect_manager::instance()->update(elapsed_time);
	}

#if defined(USE_IMGUI)
	//	エディターカメラ更新
	if (editor_camera_manager::is_valid())
	{
		editor_camera_manager::instance()->update(elapsed_time);
	}
#endif //	defined(USE_IMGUI)
}

void framework::render(float elapsed_time/*Elapsed seconds from last frame*/)
{
	//	描画処理全体の計測
	DETAIL_PROFILE_CPU_SCOPE("render");

	//	描画前処理
	for_each_world([]
	{
		if (object_manager::is_valid())
			object_manager::instance()->pre_render();
	});

	//	@tool 描画に必要な情報を抽出して、描画側へ渡す
	//	描画用の情報収集
	//	@tool メイン画面のワールド → ImGui のウィンドウに映すワールドの順に抽出する（先頭がメイン画面）
	renderer->begin_frame(elapsed_time);
	renderer->add_view(renderer->get_main_view_target(), main_view_parameters, main_view_camera_cut, false);
#if defined(USE_IMGUI)
	editor->add_views(*renderer);
	//	ImGui描画
	renderer->set_overlay(editor->end_frame());
#endif	//	defined(USE_IMGUI)
	renderer->end_frame();

	//	描画後処理
	for_each_world([]
	{
		if (object_manager::is_valid())
			object_manager::instance()->post_render();
	});
}

bool framework::uninitialize()
{
	//	@tool ワールド（シーン・オブジェクト・ワールドごとの管理クラス）を破棄する
#if defined(USE_IMGUI)
	editor.reset();
#endif	//	defined(USE_IMGUI)
	main_world.reset();
	collision_filter_manager::delete_instance();

#if defined(USE_IMGUI)
	editor_items::terminate();
	editor_config_data::delete_instance();
	detail_profiler::delete_instance();
#endif // defined(USE_IMGUI)
	game_config_data::delete_instance();
	ik_resource_manager::delete_instance();
	effect_resource_manager::delete_instance();
	model_resource_manager::delete_instance();
	model_buffer_pool_manager::delete_instance();
	material_constant_buffer_pool::delete_instance();
	pipeline_state_manager::delete_instance();
	shader_manager::delete_instance();
	font_data_manager::delete_instance();
	minigame_record_data::delete_instance();
	texture_manager::delete_instance();
	render_data::delete_instance();

	//	文字コード変換器破棄
	system_utility::uninitialize_charset_converter();

	//	COMオブジェクト破棄
	CoUninitialize();
	return true;
}

bool framework::create_collision_filters()
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
