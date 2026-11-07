#include "framework.h"
#include "texture_factory.h"
#include "render_data.h"

#include <algorithm>
#include <future>
#include <thread>

#include "shader.h"
#include "material_shader_map.h"
#include "texture.h"
#include "texture_defaults.h"
#include "texture_importer.h"
#include "texture_tracker.h"
#include "font_data.h"
#include "camera.h"
#include "graphics/occlusion_culling_system.h"
#include "graphics/clustered_lighting_system.h"
#include "Resource/resource_manager.h"
#include "model_resource.h"
#include "model_buffer_pool.h"
#include "model_instance.h"
#include "effect_resource.h"
#include "effect_instance.h"
#include "Audio/audio_manager.h"
#include "Audio/sound_player.h"
#include "Input/input_action_system.h"

#include "ik_resource.h"

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

namespace
{
#if defined(USE_IMGUI)
	//	アセットの保存し直し(データ形式を変えた後、古い形式のファイルをまとめて新しい形式にする)
	console_command resave_models_command("asset.resave_models", "Assets 以下のモデル(.emo)を読み込み直し、最新の形式で保存し直す",
		[](const console_command::arguments&)
		{
			int saved_count = 0;
			int failed_count = 0;
			std::error_code error;
			for (auto& file : std::filesystem::recursive_directory_iterator("Assets", error))
			{
				if (!file.is_regular_file() || file.path().extension() != ".emo")
					continue;

				const std::string path = file.path().string();
				model_resource_ptr resource;
				if (model_resource_manager::instance()->load(resource, path, true) && resource_manager::instance()->save(resource, path))
				{
					++saved_count;
				}
				else
				{
					++failed_count;
					console_manager::instance().print("failed : " + path);
				}
			}
			console_manager::instance().print("asset.resave_models : saved " + std::to_string(saved_count) + " / failed " + std::to_string(failed_count));
		});
	console_command resave_effects_command("asset.resave_effects", "Assets 以下のエフェクト(.eme)を読み込み直し、最新の形式で保存し直す",
		[](const console_command::arguments&)
		{
			int saved_count = 0;
			int failed_count = 0;
			std::error_code error;
			for (auto& file : std::filesystem::recursive_directory_iterator("Assets", error))
			{
				if (!file.is_regular_file() || file.path().extension() != ".eme")
					continue;

				const std::string path = file.path().string();
				effect_resource_ptr resource;
				if (effect_resource_manager::instance()->load(resource, path, true) && resource_manager::instance()->save(resource, path))
				{
					++saved_count;
				}
				else
				{
					++failed_count;
					console_manager::instance().print("failed : " + path);
				}
			}
			console_manager::instance().print("asset.resave_effects : saved " + std::to_string(saved_count) + " / failed " + std::to_string(failed_count));
		});
#endif	//	defined(USE_IMGUI)
}

framework::framework(HWND hwnd) : hwnd(hwnd)
{
	logger::create_instance();
}

framework::~framework()
{
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
	imgui_initialize();
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

			update(tictoc.time_interval());
			render(tictoc.time_interval());

			//	詳細プロファイラーのフレーム終了
			DETAIL_PROFILE_END_FRAME();
		}
	}

#if defined(USE_IMGUI)
	imgui_terminate();
#endif	//	defined(USE_IMGUI)

	BOOL fullscreen{};
	render_data::instance()->get_swap_chain()->GetFullscreenState(&fullscreen, 0);
	if (fullscreen)
	{
		render_data::instance()->get_swap_chain()->SetFullscreenState(FALSE, 0);
	}

	return uninitialize() ? static_cast<int>(msg.wParam) : 0;
}

LRESULT framework::handle_message(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
#ifdef USE_IMGUI
	if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) { return true; }
#endif

	if (mouse::instance()->window_proccess(hwnd, msg, wparam, lparam)) { return true; }

	switch (msg)
	{
	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		BeginPaint(hwnd, &ps);

		EndPaint(hwnd, &ps);
		break;
	}
	case WM_DESTROY:
	{
		PostQuitMessage(0);
		break;
	}
	case WM_CREATE:
	{
		break;
	}
	case WM_KEYDOWN:
	{
		if (wparam == VK_ESCAPE)
		{
			PostMessage(hwnd, WM_CLOSE, 0, 0);
		}
		break;
	}
	case WM_ENTERSIZEMOVE:
	{
		tictoc.stop();
		break;
	}
	case WM_EXITSIZEMOVE:
	{
		tictoc.start();
		break;
	}
	case WM_MOUSEWHEEL:
	{
		wheel = GET_WHEEL_DELTA_WPARAM(wparam);
		break;
	}
	case WM_MOVE:
	{
		engine_config_data::instance()->windows.window_left = (int)(short)LOWORD(lparam);
		engine_config_data::instance()->windows.window_top = (int)(short)HIWORD(lparam);
		break;
	}
	case WM_WINDOWPOSCHANGED:
	{
		WINDOWPOS* pos = (WINDOWPOS*)lparam;
		engine_config_data::instance()->windows.window_left = pos->x;
		engine_config_data::instance()->windows.window_top = pos->y;
		break;
	}

	default:
		return DefWindowProc(hwnd, msg, wparam, lparam);
	}
	return 0;
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

	//	プロジェクトのルートディレクトリを控える
	project_path::initialize();

	//	ゲームコンフィグ初期化
	{
		//	@remainder	内部でコンソールマネージャーに情報を受け渡しているだけ。
		game_config_data::create_instance();
	}

	//	描画関係初期化
	{
		render_data::create_instance(hwnd);
		if (!render_data::instance()->is_supported_device())
		{
			MessageBoxW(hwnd, L"この GPU は必要な機能(VS からの描画先配列番号の出力)に対応していないため起動できません。", L"起動エラー", MB_OK | MB_ICONERROR);
			return false;
		}
		texture_defaults::create_instance();
#if defined(USE_IMGUI)
		texture_tracker::create_instance();
		texture_importer::create_instance();
#endif	//	defined(USE_IMGUI)
		shader_manager::create_instance();
		pipeline_state_manager::create_instance();
		material_shader_map::create_instance();
		model_buffer_pool_manager::create_instance();
		asset_registry::create_instance();
		asset_registry::instance()->scan();
		resource_manager::create_instance();
		model_resource_manager::create_instance();
		ik_resource_manager::create_instance();	//	@remainder	これは不要では？
		font_data_manager::create_instance();
	}

	//	入力関係初期化
	{
		mouse::create_instance(hwnd);
		keyboard::create_instance();
		gamepad::create_instance();
		input::action_system::create_instance();
	}

	//	エフェクト関係
	{
		effect_resource_manager::create_instance();
	}

#if defined(USE_IMGUI)
	//	エディター/デバッグ関係
	{
		editor_config_data::create_instance();
		detail_profiler::create_instance();
		debug_primitive_manager::create_instance();
		editor_items::initialize();
	}
#endif // defined(USE_IMGUI)

	//	サウンド関係
	{
		audio_manager::create_instance();
		audio::sound_player::create_instance();
	}

	//	シーン関係
	{
		collision_filter_manager::create_instance();

		//	メインのワールド
		main_world = std::make_unique<world>();

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

	//	各種バッファ用意
	if (!create_buffers())
		return false;

	//	当たり判定フィルター用意
	if (!create_collision_filters())
		return false;


	//	シーンレンダラー登録
	//	@tool メインのワールドをバックバッファに描く
	{
		render_executor = std::make_shared<render_job_executor>(4);
		main_renderer = std::make_unique<world_renderer>(render_executor, render_output::create_back_buffer());
		if (!main_renderer->initialize())
			return false;
#if defined(USE_IMGUI)
		main_renderer->set_debug_builder([this](const render_data_context& data) { build_debug_primitives(data); });
#endif	//	defined(USE_IMGUI)
	}

	//	全描画コマンド処理後に呼び出される関数登録
	render_executor->register_post_rendering(
		[this]
		{
#if defined(USE_IMGUI)
			//	ImGui描画
			{
				imgui_rendering_pass.activate(render_data::instance()->get_immediate_context().Get());
				render_data::instance()->get_immediate_context()->RSSetViewports(1, &imgui_viewport);
					
				ImGui::Render();
				ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
				imgui_rendering_pass.deactivate(render_data::instance()->get_immediate_context().Get());

				if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
				{
					ImGui::UpdatePlatformWindows();
					ImGui::RenderPlatformWindowsDefault();
				}

				//	ImGui の描画が終わったので、表示テクスチャを手放す
				imgui_texture_keeper::instance()->release();
			}
#endif // defined(USE_IMGUI)

			//	プレゼント
			UINT sync_interval{ engine_config_data::instance()->windows.vertical_sync };
			render_data::instance()->get_swap_chain()->Present(sync_interval, 0);
		});

	//	初回シーン登録
	main_world->get_scenes().change_scene<load_shader_scene>();
	return true;
}

void framework::update(float elapsed_time/*Elapsed seconds from last frame*/)
{
	//	更新処理全体の計測
	DETAIL_PROFILE_CPU_SCOPE("update");

#if defined(USE_IMGUI)
	//	各種データの更新チェック
	{
		DETAIL_PROFILE_CPU_SCOPE("update.asset_hot_reload");
		if (shader_manager::is_valid())
		{
			shader_manager::instance()->check_shader_update_and_recompile();
		}
	}

#endif //	defined(USE_IMGUI)

	//	入力関係の更新
	if (mouse::is_valid())
	{
		mouse::instance()->update_state();
	}
	if (keyboard::is_valid())
	{
		keyboard::instance()->update_state();
	}
	if (gamepad::is_valid())
	{
		gamepad::instance()->update_state();
	}
	if (input::action_system::is_valid() && main_world)
	{
		input::action_system::instance()->update(main_world->get_scenes().get_broadcaster());
	}

#if defined(USE_IMGUI)
	//	キー割り当てされたコンソールコマンドを実行
	console_manager::instance().update_bindings();
#endif //	defined(USE_IMGUI)

	//	リソースの構築と読込完了の通知
	if (resource_manager::is_valid())
	{
		DETAIL_PROFILE_CPU_SCOPE("update.resource_manager");
		resource_manager::instance()->update();
	}

	//	シーン内の更新
	if (main_world)
	{
		DETAIL_PROFILE_CPU_SCOPE("update.scene_manager");
		main_world->get_scenes().updates(elapsed_time);
	}

	//	オブジェクト更新
	if (main_world)
	{
		DETAIL_PROFILE_CPU_SCOPE("update.object_manager");
		main_world->get_objects().updates(elapsed_time);
	}

	//	エフェクト関係更新
	if (main_world)
	{
		DETAIL_PROFILE_CPU_SCOPE("update.effect_manager");
		main_world->get_effects().update(elapsed_time);
	}

	//	サウンド更新
	if (audio_manager::is_valid())
	{
		DETAIL_PROFILE_CPU_SCOPE("update.audio_manager");
		audio_manager::instance()->update(elapsed_time);
	}

#if defined(USE_IMGUI)
	//	ImGuiウィンドウ構築の計測
	DETAIL_PROFILE_CPU_SCOPE("update.imgui");

	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
	editor_items::begin_contexts();

	//	シーン情報表示
	if (main_world)
	{
		main_world->get_scenes().imgui_render();
	}

	//	オブジェクト情報表示
	main_world->get_objects().imgui_render();

	//	ログ情報
	imgui_log_window();

	//	エンジンの設定情報
	imgui_engine_config_window();

	//	GPU計測時間表示
	imgui_profiler_window();

	//	モデルバッファプール使用量表示
	imgui_model_buffer_pool_window();

	//	メモリアロケータ使用量表示
	imgui_memory_allocator_window();

	//	シーン変更
	imgui_scene_change();

	//	コンソール
	imgui_console_window();
	imgui_console_commands_window();

	//	ゲームパッドの入力状況
	imgui_gamepad_window();

	//	サウンドバンクの編集
	imgui_sound_bank_window();

	//	アセットブラウザ
	asset_browser::instance()->imgui_render();
	texture_editor::instance()->imgui_render();
#endif // defined(USE_IMGUI)
}

void framework::render(float elapsed_time/*Elapsed seconds from last frame*/)
{
	//	描画処理全体の計測
	DETAIL_PROFILE_CPU_SCOPE("render");

	//	描画前処理
	if (main_world)
	{
		main_world->get_objects().pre_render();
	}

	//	描画用の情報収集
	main_renderer->collect(*main_world);

	{
		DETAIL_PROFILE_CPU_SCOPE("render.rebuild_pipeline_bindings");
		pipeline_state_manager::instance()->rebuild_pipeline_bindings();
	}

	//	バッファ描画
	{
		//	レンダラー開始処理
		render_executor->begin();

		//	@tool ワールドごとの描画
		main_renderer->submit_render_jobs(*main_world, elapsed_time);

		//	描画コマンドを実行
		{
			DETAIL_PROFILE_CPU_SCOPE("render.execute_commands");
			render_executor->render();
		}

		//	描画コマンド実行終了処理
		{
			DETAIL_PROFILE_CPU_SCOPE("render.imgui_and_present");
			render_executor->end();
		}
	}

	//	描画後処理
	if (main_world)
	{
		main_world->get_objects().post_render();
	}
}


bool framework::uninitialize()
{
	//	シーン関係
	{
#if defined(USE_IMGUI)
		pause_command.reset();
		step_command.reset();
#endif	//	defined(USE_IMGUI)
		main_world.reset();
		collision_filter_manager::delete_instance();
	}

	//	サウンド関係
	{
		audio::sound_player::delete_instance();
		audio_manager::delete_instance();
	}

#if defined(USE_IMGUI)
	//	エディター/デバッグ関係
	{
		editor_items::terminate();
		debug_primitive_manager::delete_instance();
		editor_config_data::delete_instance();
		detail_profiler::delete_instance();
	}
#endif // defined(USE_IMGUI)

	//	描画関係
	{
		font_data_manager::delete_instance();
		ik_resource_manager::delete_instance();
		effect_resource_manager::delete_instance();
		resource_manager::delete_instance();
		asset_registry::delete_instance();
		model_resource_manager::delete_instance();
		model_buffer_pool_manager::delete_instance();
		material_shader_map::delete_instance();
		pipeline_state_manager::delete_instance();
		shader_manager::delete_instance();
#if defined(USE_IMGUI)
		texture_importer::delete_instance();
#endif	//	defined(USE_IMGUI)
		texture_defaults::delete_instance();
#if defined(USE_IMGUI)
		texture_tracker::delete_instance();
#endif	//	defined(USE_IMGUI)
		render_data::delete_instance();
	}

	//	入力関係終了
	{
		input::action_system::delete_instance();
		gamepad::delete_instance();
		keyboard::delete_instance();
		mouse::delete_instance();
	}

	//	ゲームコンフィグ終了
	{
		game_config_data::delete_instance();
	}

	//	文字コード変換器破棄
	system_utility::uninitialize_charset_converter();

	//	COMオブジェクト破棄
	CoUninitialize();
	return true;
}

bool framework::create_buffers()
{
#if defined(USE_IMGUI)
	//	ImGui
	{
		imgui_rendering_pass.render_targets.emplace_back(render_data::instance()->get_swap_chain());
		imgui_rendering_pass.render_targets.back().clear_color = false;

		DXGI_SWAP_CHAIN_DESC desc;
		render_data::instance()->get_swap_chain()->GetDesc(&desc);

		// ビューポートの設定
		imgui_viewport.TopLeftX = 0.0f;
		imgui_viewport.TopLeftY = 0.0f;
		imgui_viewport.Width = static_cast<float>(desc.BufferDesc.Width);
		imgui_viewport.Height = static_cast<float>(desc.BufferDesc.Height);
		imgui_viewport.MinDepth = 0.0f;
		imgui_viewport.MaxDepth = 1.0f;
	}
#endif // defined(USE_IMGUI)

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
