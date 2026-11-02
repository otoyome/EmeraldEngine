#include "framework.h"
#include "render_data.h"

#include <algorithm>
#include <future>
#include <thread>

#include "shader.h"
#include "material_shader_map.h"
#include "texture.h"
#include "font_data.h"
#include "camera.h"
#include "graphics/occlusion_culling_system.h"
#include "graphics/clustered_lighting_system.h"
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
#include "Renderer/ui_renderer.h"

#include "Object/object_manager.h"
#include "Object/GameObjects/UI/ui_object.h"

#include "System/engine_config_data.h"
#include "System/game_config_data.h"

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
	//	view_stateの破棄フレーム数
	constexpr uint64_t scene_view_state_keep_frames = 60;

	void upload_material_constant_buffers(const render_data_context& data)
	{
		gpu_upload_list& upload_list = render_data::instance()->get_upload_list();
		auto upload = [&upload_list](material* target)
		{
			if (target)
				target->upload_constant_buffers(upload_list);
		};

		for (auto& material_data : data.mesh_container.material_datas)
			upload(material_data.material);
		for (auto& sprite_trail : data.effect_container.sprite_trail_draw_datas)
			upload(sprite_trail.material_data.material);
		for (auto& decal : data.decal_container.decal_datas)
			upload(decal.material.material);
		if (const auto& volume = data.post_process_volume_container.selected_volume)
		{
			for (auto& postprocess_material : volume->get_materials())
				upload(postprocess_material.get());
		}
	}

#if defined(USE_IMGUI)
	//	描画関係のデバッグ設定
	console_variable<bool> freeze_culling_variable("r.culling.freeze", false, "カリング結果を固定する（メイン視点）");
	console_variable<bool> visualize_meshlets_variable("r.meshlet.visualize", false, "メッシュレット単位で色分け表示する（メイン視点）");

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

	//	@tool アセットの保存し直し(データ形式を変えた後、古い形式のファイルをまとめて新しい形式にする)
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
				if (model_resource_manager::instance()->load(resource, path, true) && resource->serialize(path))
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
		texture_manager::create_instance();
		shader_manager::create_instance();
		pipeline_state_manager::create_instance();
		material_shader_map::create_instance();
		light_manager::create_instance();
		post_process_volume_manager::create_instance();
		camera_manager::create_instance();
		model_buffer_pool_manager::create_instance();
		model_resource_manager::create_instance();
		model_instance_manger::create_instance();
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
		effect_manager::create_instance();
	}

#if defined(USE_IMGUI)
	//	エディター/デバッグ関係
	{
		editor_config_data::create_instance();
		detail_profiler::create_instance();
		debug_primitive_manager::create_instance();
		editor_camera_manager::create_instance();
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
		object_manager::create_instance();
		scene_manager::create_instance();
	}

	//	各種バッファ用意
	render_targets = std::make_shared<scene_render_targets>();
	if (!create_buffers())
		return false;

	//	当たり判定フィルター用意
	if (!create_collision_filters())
		return false;


	//	シーンレンダラー登録
	{
		render_executor = std::make_shared<render_job_executor>(4);

		scene_renderers.emplace_back(std::make_unique<depth_prepass_renderer>(render_executor, render_targets));
		scene_renderers.emplace_back(std::make_unique<occlusion_culling_renderer>(render_executor, render_targets));
		scene_renderers.emplace_back(std::make_unique<gbuffer_renderer>(render_executor, render_targets));
		scene_renderers.emplace_back(std::make_unique<gbuffer_decal_renderer>(render_executor, render_targets));
		//	SSAO
		scene_renderers.emplace_back(std::make_unique<deffered_lighting_renderer>(render_executor, render_targets));
		scene_renderers.emplace_back(std::make_unique<screen_space_reflection_renderer>(render_executor, render_targets));

		scene_renderers.emplace_back(std::make_unique<before_dof_forward_renderer>(render_executor, render_targets));
		scene_renderers.emplace_back(std::make_unique<depth_of_field_post_process_renderer>(render_executor, render_targets));
		scene_renderers.emplace_back(std::make_unique<before_dof_post_process_renderer>(render_executor, render_targets));
		scene_renderers.emplace_back(std::make_unique<after_dof_post_process_renderer>(render_executor, render_targets));
		scene_renderers.emplace_back(std::make_unique<after_dof_forward_renderer>(render_executor, render_targets));
		scene_renderers.emplace_back(std::make_unique<after_translucency_post_process_renderer>(render_executor, render_targets));

		scene_renderers.emplace_back(std::make_unique<motion_blur_post_process_renderer>(render_executor, render_targets));
		scene_renderers.emplace_back(std::make_unique<after_motion_blur_forward_renderer>(render_executor, render_targets));

		scene_renderers.emplace_back(std::make_unique<before_bloom_post_process_renderer>(render_executor, render_targets));
		scene_renderers.emplace_back(std::make_unique<bloom_and_tone_mapping_post_process_renderer>(render_executor, render_targets));
		scene_renderers.emplace_back(std::make_unique<after_tone_mapping_post_process_renderer>(render_executor, render_targets));

		scene_renderers.emplace_back(std::make_unique<anti_alias_post_process_renderer>(render_executor, render_targets));
		scene_renderers.emplace_back(std::make_unique<finalpass_renderer>(render_executor, render_targets));

#if defined(USE_IMGUI)
		scene_renderers.emplace_back(std::make_unique<debug_draw_pass_renderer>(render_executor, render_targets));
#endif // defined(USE_IMGUI)

		//	ソート
		std::sort(scene_renderers.begin(), scene_renderers.end(),
			[](std::unique_ptr<renderer>& l, std::unique_ptr<renderer>& r)
			{
				return l->get_domain() < r->get_domain();
			});
	}

	//	UIレンダラーの追加
	{
		ui_renderers.emplace_back(std::make_unique<ui_renderer>(render_executor, render_targets));
	}

	//	プロファイラー表示用のレンダラー名設定
	for (auto& render : scene_renderers)
		DETAIL_PROFILE_SET_RENDERER_NAME(*render);
	for (auto& render : ui_renderers)
		DETAIL_PROFILE_SET_RENDERER_NAME(*render);

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
			}
#endif // defined(USE_IMGUI)

			//	プレゼント
			UINT sync_interval{ engine_config_data::instance()->windows.vertical_sync };
			render_data::instance()->get_swap_chain()->Present(sync_interval, 0);
		});

	//	初回シーン登録
	scene_manager::instance()->change_scene<load_shader_scene>();
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
		if (texture_manager::is_valid())
		{
			texture_manager::instance()->check_update_and_reload();
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
	if (input::action_system::is_valid() && scene_manager::is_valid())
	{
		input::action_system::instance()->update(scene_manager::instance()->get_broadcaster());
	}

#if defined(USE_IMGUI)
	//	キー割り当てされたコンソールコマンドを実行
	console_manager::instance().update_bindings();
#endif //	defined(USE_IMGUI)

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

	//	サウンド更新
	if (audio_manager::is_valid())
	{
		DETAIL_PROFILE_CPU_SCOPE("update.audio_manager");
		audio_manager::instance()->update(elapsed_time);
	}

#if defined(USE_IMGUI)
	//	エディターカメラ更新
	if (editor_camera_manager::is_valid())
	{
		editor_camera_manager::instance()->update(elapsed_time);
	}

	//	ImGuiウィンドウ構築の計測
	DETAIL_PROFILE_CPU_SCOPE("update.imgui");

	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
	editor_items::begin_contexts();

	//	シーン情報表示
	if (scene_manager::is_valid())
	{
		scene_manager::instance()->imgui_render();
	}

	//	オブジェクト情報表示
	object_manager::instance()->imgui_render();

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

	//	テクスチャリスト情報
	imgui_textures_window();

	//	シーン変更
	imgui_scene_change();

	//	コンソール
	imgui_console_window();
	imgui_console_commands_window();

	//	ゲームパッドの入力状況
	imgui_gamepad_window();

	//	サウンドバンクの編集
	imgui_sound_bank_window();
#endif // defined(USE_IMGUI)
}

void framework::build_particle_render_lists(const render_data_context& render_data, render_particle_indices& particle_indices)
{
	for (auto& particle_index : render_data.effect_container.sprite_trail_color_datas)
	{
		auto& material = render_data.effect_container.sprite_trail_draw_datas[particle_index].material_data.material;

		//	番号登録
		switch (material->get_blend_mode())
		{
			case material::blend_modes::opaque:
			{
				particle_indices.opaque.push_back(particle_index);
				break;
			}
			case material::blend_modes::mask:
			{
				particle_indices.mask.push_back(particle_index);
				break;
			}
			case material::blend_modes::translucent:
			case material::blend_modes::addative:
			{
				int index = static_cast<int>(material->get_translucency_path());
				particle_indices.forward[index].push_back(particle_index);
				break;
			}
		}
	}
}

void framework::render(float elapsed_time/*Elapsed seconds from last frame*/)
{
	//	描画処理全体の計測
	DETAIL_PROFILE_CPU_SCOPE("render");

	//	描画前処理
	if (object_manager::is_valid())
	{
		object_manager::instance()->pre_render();
	}

	//	描画用の情報収集
	auto render_data = std::make_shared<render_data_context>();
	auto render_particle_indices = std::make_shared<::render_particle_indices>();
	{
		DETAIL_PROFILE_CPU_SCOPE("render.collect_render_data");

		//	視点の候補から今フレームのメイン視点を選ぶ
		view_selection selection;
		main_view_camera_cut = false;
		if (camera_manager::instance()->select_view(selection))
		{
			main_view_parameters = selection.parameters;
			main_view_camera_cut = selection.camera_cut;
		}

		//	相対描画用の描画原点
		const math::vector3d render_origin = scene_view::compute_render_origin(main_view_parameters, engine_config_data::instance()->rendering.camera_relative_rendering);
		render_data->render_origin = render_origin;

		//	ライト情報収集
		{
			//	メイン視点の視錐台と距離・画面上の大きさで間引く
			const scene_view collect_view = scene_view::from_parameters(main_view_parameters, render_origin);
			light_view_info view_info;
			view_info.position = main_view_parameters.position;
			view_info.tan_half_fov_y = std::tan(main_view_parameters.fov_y * 0.5f);
			view_info.frustum = &collect_view.get_frustum_culling();
			light_manager::instance()->get_container(render_data->light_container, render_origin, view_info);
		}

		//	ポストプロセスボリューム取得
		post_process_volume_manager::instance()->get_container(render_data->post_process_volume_container, main_view_parameters.position);

		//	メッシュ取得
		model_instance_manger::instance()->get_container(render_data->mesh_container, render_origin);

		//	エフェクトコンテナ取得
		effect_manager::instance()->get_container(render_data->effect_container, render_data->decal_container, render_origin);

		//	UIオブジェクトコンテナ収集
		{
			render_data->sprite_container.sprite_datas.clear();

			int sort_key = 0;
			auto ui_object_list = object_manager::instance()->search_objects<ui_object>();
			for (auto& ui_object : ui_object_list)
			{
				if (!ui_object->is_active())
					continue;

				sprite_datas sprite_data;
				sprite_data.sort_key = sort_key;
				sprite_data.sort_order = ui_object->get_sort_order();
				sprite_data.sprite = &ui_object->get_sprite();
				render_data->sprite_container.sprite_datas.emplace_back(sprite_data);
				++sort_key;
			}

			auto ui_objects_list = object_manager::instance()->search_objects<ui_objects>();
			for (auto& ui_object : ui_objects_list)
			{
				if (!ui_object->is_active())
					continue;

				for (auto& sprite : ui_object->get_sprites())
				{
					sprite_datas sprite_data;
					sprite_data.sort_key = sort_key;
					sprite_data.sort_order = ui_object->get_sort_order();
					sprite_data.sprite = &sprite;
					render_data->sprite_container.sprite_datas.emplace_back(sprite_data);
				}
				++sort_key;
			}

			//	ソート
			std::sort(render_data->sprite_container.sprite_datas.begin(), render_data->sprite_container.sprite_datas.end(),
				[](const sprite_datas& l, const sprite_datas& r)
				{
					if (l.sort_order < r.sort_order)
						return l.sort_key != r.sort_key;
					return false;
				});
		}

		//	パーティクルの描画振り分け
		build_particle_render_lists(*render_data, *render_particle_indices);

		//	モデルの頂点/インデックスバッファ確保分をGPUへ転送
		model_buffer_pool_manager::instance()->flush_uploads();

		//	マテリアルの定数バッファをGPUへ転送
		upload_material_constant_buffers(*render_data);

#if defined(USE_IMGUI)
		//	デバッグ情報取得
		build_debug_primitives(*render_data);
		debug_primitive_manager::instance()->get_container(render_data->debug_draw_container, render_data->render_origin);
#endif // defined(USE_IMGUI)
	}

	{
		DETAIL_PROFILE_CPU_SCOPE("render.rebuild_pipeline_bindings");
		pipeline_state_manager::instance()->rebuild_pipeline_bindings();
	}

	//	バッファ描画
	{
		//	レンダラー開始処理
		render_executor->begin();

		//	描画準備
		{
			renderer_context context = {};
			context.render_data = render_data;
			render_executor->execute([this, context](graphics_context& context)
			{
				//	画面クリア
				back_buffer_clear_rendering_pass.activate(context.device_context);

				back_buffer_clear_rendering_pass.deactivate(context.device_context);
			}, "begin_draw_pass", context, static_cast<UINT>(renderer_domain::begin_pass));
		}

		static float total_time = 0;

		//	メイン視点で描画
		{
			const scene_view_state_ptr& main_state = scene_view_states.acquire(main_scene_view_state_key, render_frame_index);
#if defined(USE_IMGUI)
			main_state->set_freeze_culling(freeze_culling_variable.get());
			main_state->set_debug_visualize_meshlets(visualize_meshlets_variable.get());
#endif	//	defined(USE_IMGUI)
			const scene_view_ptr main_view = std::make_shared<const scene_view>(scene_view::from_parameters(main_view_parameters, render_data->render_origin));
			main_state->begin_frame(*main_view, main_view_camera_cut);

			//	レンダラーコンテキスト準備
			renderer_context context = {};
			{
				context.render_data = render_data;
				context.render_mesh_indices = std::make_shared<render_mesh_indices>();
				context.render_particle_indices = render_particle_indices;
				context.render_decal_indices = std::make_shared<render_decal_indices>();
				prepare_renderer_context(context, main_view, main_state, main_scene_constant_buffer, render_data, render_particle_indices, total_time, total_time + elapsed_time);
			}

			//	オクルージョンカリング結果を読み出す
			std::unordered_map<UINT, uint8_t> occlusion_visible;
			if (const auto& occlusion = main_state->get_occlusion_culling())
				occlusion->read_last_frame(occlusion_visible);

			//	描画メッシュ／デカールのカリング・ソート
			{
				DETAIL_PROFILE_CPU_SCOPE("render.build_render_lists");
				build_render_lists(*render_data, *main_state, main_view->get_frustum_culling(), occlusion_visible,
					*context.render_mesh_indices, *context.render_decal_indices);
			}

			//	シーン構築
			{
				render_executor->execute([this, context](graphics_context& context)
				{
					//	シーン定数バッファ更新
					context.device_context->UpdateSubresource(context.scene_constant_buffer.Get(), 0, 0, &context.scene_constant, 0, 0);

					//	エフェクト類のGPU側の更新
					context.debug_begin_event(L"update_effect_gpu");
					effect_manager::instance()->update_gpu(context, context.scene_constant.time_data.x - context.scene_constant.time_data.y);
					context.debug_end_event();

				}, "scene_constantract_pass", context, static_cast<UINT>(renderer_domain::scene_constantract_pass), true);
			}

			//	描画
			{
				DETAIL_PROFILE_CPU_SCOPE("render.scene_renderers_issue");
				for (auto& render : scene_renderers)
				{
					DETAIL_PROFILE_CPU_SCOPE_CATEGORY(renderer_issue, render->get_profile_name());
					render->render(context);
				}
			}

			//	描画終了処理
			main_state->end_frame(*main_view);
		}

		//	UI描画
		{
			renderer_context context = {};
			context.scene_constant_buffer = ui_scene_constant_buffer;
			context.render_data = render_data;
			context.render_mesh_indices = std::make_shared<render_mesh_indices>();
			for (auto& render : ui_renderers)
			{
				DETAIL_PROFILE_CPU_SCOPE_CATEGORY(renderer_issue, render->get_profile_name());
				render->render(context);
			}
		}

		//	描画コマンドを実行
		{
			DETAIL_PROFILE_CPU_SCOPE("render.execute_commands");
			render_executor->render();
		}

		//	時間加算
		total_time += elapsed_time;

		//	描画コマンド実行終了処理
		{
			DETAIL_PROFILE_CPU_SCOPE("render.imgui_and_present");
			render_executor->end();
		}
	}

	//	描画後処理
	if (object_manager::is_valid())
	{
		object_manager::instance()->post_render();
	}

	//	使われなくなった view_state を破棄
	scene_view_states.release_unused(render_frame_index, scene_view_state_keep_frames);
	++render_frame_index;
}

void framework::prepare_renderer_context(
	renderer_context& context,
	const scene_view_ptr& target_view,
	const scene_view_state_ptr& target_state,
	const Microsoft::WRL::ComPtr<ID3D11Buffer>& scene_constant_buffer,
	const std::shared_ptr<render_data_context>& render_data,
	const std::shared_ptr<render_particle_indices>& particle_indices,
	float previous_time, float current_time)
{
	//	シーン定数バッファ設定
	{
		context.scene_constant_buffer = scene_constant_buffer;
		context.scene_constant.time_data.x = current_time;
		context.scene_constant.time_data.y = previous_time;
#if defined(USE_IMGUI)
		context.scene_constant.time_data.z = target_state->is_debug_visualize_meshlets() ? 1.0f : 0.0f;
#endif	//	defined(USE_IMGUI)

		target_view->fill_scene_constants(context.scene_constant, target_state.get());
	}

	//	描画視点情報と描画制御情報設定
	{
		context.current_view = target_view;
		context.current_view_state = target_state;
	}
}

void framework::build_render_lists(
	const render_data_context& render_data,
	scene_view_state& target_state,
	const collision::frustum_culling& frustum_culling,
	const std::unordered_map<UINT, uint8_t>& occlusion_visible,
	render_mesh_indices& mesh_indices,
	render_decal_indices& decal_indices)
{
	const auto& mesh_container = render_data.mesh_container;
	const auto* draw_data_ptr = mesh_container.draw_datas.data();
	const auto* material_data_ptr = mesh_container.material_datas.data();
	const auto* buffer_data_ptr = mesh_container.buffer_datas.data();
	const auto& color_datas = mesh_container.color_datas;
	const std::size_t count = color_datas.size();

	//	ソート情報
	auto& lengths = mesh_indices.lengths;
	lengths.assign(mesh_container.buffer_datas.size(), 0.0f);
	sort_pipeline_keys.assign(mesh_container.buffer_datas.size(), std::uintptr_t(0));
	const auto& near_plane = frustum_culling.get_plane(collision::frustum_culling::surface_near);

	//	描画判定
	auto draw_classify = [&](std::size_t list_index, cull_bucket& bucket)
	{
		const std::size_t mesh_index = color_datas[list_index];
		const auto* draw_data = draw_data_ptr + mesh_index;
		const auto* material = (material_data_ptr + draw_data->material_index)->material;
		const auto* buffer_data = buffer_data_ptr + draw_data->buffer_index;

		//	カリング処理
		if (system_utility::is_bitflag(buffer_data->system_flags, buffer_datas::flags::culling))
		{
			//	フラスタムカリング
			bool visible = frustum_culling.to_aabb(buffer_data->bounds_min, buffer_data->bounds_max);

			//	オクルージョンカリング
			if (visible)
			{
				auto it = occlusion_visible.find(buffer_data->unique_id);
				if (it != occlusion_visible.end() && it->second == 0)
					visible = false;
			}

#if defined(USE_IMGUI)
			//	判定結果を過去のもので固定
			visible = target_state.resolve_debug_visible(buffer_data->unique_id, visible);
#endif	//	defined(USE_IMGUI)
			if (!visible)
				return;
		}
		//	カメラからの距離算出
		math::float3 center;
		center.x = (buffer_data->bounds_min.x + buffer_data->bounds_max.x) * 0.5f;
		center.y = (buffer_data->bounds_min.y + buffer_data->bounds_max.y) * 0.5f;
		center.z = (buffer_data->bounds_min.z + buffer_data->bounds_max.z) * 0.5f;
		lengths[mesh_index] = near_plane.calculate_distance(center);

		//	番号登録
		switch (material->get_blend_mode())
		{
			case material::blend_modes::opaque:
				sort_pipeline_keys[mesh_index] = reinterpret_cast<std::uintptr_t>(material->get_pipeline_state(material::surface_paths::g_buffer_pass).get());
				bucket.opaque.push_back(mesh_index);
				break;
			case material::blend_modes::mask:
				sort_pipeline_keys[mesh_index] = reinterpret_cast<std::uintptr_t>(material->get_pipeline_state(material::surface_paths::g_buffer_pass).get());
				bucket.mask.push_back(mesh_index);
				break;
			case material::blend_modes::translucent:
			case material::blend_modes::addative:
			{
				int path = static_cast<int>(material->get_translucency_path());
				bucket.forward[path].push_back(mesh_index);
				break;
			}
		}
	};

	//	仕分け
	{
		constexpr std::size_t min_items_per_thread = 512;
		const std::size_t worker_slots = parallel_executor.max_workers();
		if (cull_buckets.size() < worker_slots)
			cull_buckets.resize(worker_slots);
		for (auto& bucket : cull_buckets)
			bucket.clear();

		if (count < min_items_per_thread * 2 || worker_slots <= 1)
		{
			for (std::size_t i = 0; i < count; ++i)
				draw_classify(i, cull_buckets[0]);
		}
		else
		{
			parallel_executor.parallel_for(count, min_items_per_thread,
				[&](std::size_t worker_index, std::size_t begin, std::size_t end)
				{
					cull_bucket& bucket = cull_buckets[worker_index];
					for (std::size_t i = begin; i < end; ++i)
						draw_classify(i, bucket);
				});
		}
	}

	std::size_t total_opaque = 0;
	std::size_t total_mask = 0;
	std::size_t total_forward[render_mesh_indices::forward_size] = {};
	for (auto& bucket : cull_buckets)
	{
		total_opaque += bucket.opaque.size();
		total_mask += bucket.mask.size();
		for (int path = 0; path < render_mesh_indices::forward_size; ++path)
			total_forward[path] += bucket.forward[path].size();
	}

	mesh_indices.opaque.reserve(total_opaque);
	mesh_indices.mask.reserve(total_mask);
	for (int path = 0; path < render_mesh_indices::forward_size; ++path)
		mesh_indices.forward[path].reserve(total_forward[path]);
	for (auto& bucket : cull_buckets)
	{
		mesh_indices.opaque.insert(mesh_indices.opaque.end(), bucket.opaque.begin(), bucket.opaque.end());
		mesh_indices.mask.insert(mesh_indices.mask.end(), bucket.mask.begin(), bucket.mask.end());
		for (int path = 0; path < render_mesh_indices::forward_size; ++path)
			mesh_indices.forward[path].insert(mesh_indices.forward[path].end(), bucket.forward[path].begin(), bucket.forward[path].end());
	}

	//	同一パイプラインステートで固まる用に優先しつつ、
	//	不透明・マスク手前から距離でソート
	auto pipeline_then_near = [this, &lengths](std::size_t left_index, std::size_t right_index)
	{
		if (sort_pipeline_keys[left_index] != sort_pipeline_keys[right_index])
			return sort_pipeline_keys[left_index] < sort_pipeline_keys[right_index];
		return lengths[left_index] < lengths[right_index];
	};
	std::sort(mesh_indices.opaque.begin(), mesh_indices.opaque.end(), pipeline_then_near);
	std::sort(mesh_indices.mask.begin(), mesh_indices.mask.end(), pipeline_then_near);

	//	半透明・加算は奥から距離ソート
	auto far_to_near = [&lengths](std::size_t left_index, std::size_t right_index)
	{
		return lengths[left_index] > lengths[right_index];
	};
	for (auto& indices : mesh_indices.forward)
		std::sort(indices.begin(), indices.end(), far_to_near);

	//	デカール
	const auto& decal_datas = render_data.decal_container.decal_datas;
	decal_indices.draw_list.reserve(decal_datas.size());
	for (UINT decal_index = 0; decal_index < decal_datas.size(); ++decal_index)
	{
		//	フラスタムカリング
		if (!frustum_culling.to_box(decal_datas[decal_index].box))
			continue;

		decal_indices.draw_list.emplace_back(decal_index);
	}
}


bool framework::uninitialize()
{
	//	シーン関係
	{
		collision_filter_manager::delete_instance();
		scene_manager::delete_instance();
		if (object_manager::is_valid())
		{
			object_manager::instance()->finalize();
			object_manager::delete_instance();
		}
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
		editor_camera_manager::delete_instance();
		debug_primitive_manager::delete_instance();
		editor_config_data::delete_instance();
		detail_profiler::delete_instance();
	}
#endif // defined(USE_IMGUI)

	//	描画関係
	{
		font_data_manager::delete_instance();
		ik_resource_manager::delete_instance();
		effect_manager::delete_instance();
		effect_resource_manager::delete_instance();
		model_instance_manger::delete_instance();
		model_resource_manager::delete_instance();
		model_buffer_pool_manager::delete_instance();
		camera_manager::delete_instance();
		post_process_volume_manager::delete_instance();
		light_manager::delete_instance();
		material_shader_map::delete_instance();
		pipeline_state_manager::delete_instance();
		shader_manager::delete_instance();
		texture_manager::delete_instance();
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
	render_targets = std::make_shared<scene_render_targets>();
	int scene_width = SCREEN_WIDTH;
	int scene_height = SCREEN_HEIGHT;

	//	バックバッファクリア用
	{
		back_buffer_clear_rendering_pass.render_targets.emplace_back(render_data::instance()->get_swap_chain());
	}

	//	UI描画用のシーン定数バッファ生成(viewport_rectのみ使用するため他の項目は未使用)
	{
		HRESULT hr;
		D3D11_BUFFER_DESC buffer_desc{};
		buffer_desc.Usage = D3D11_USAGE_DEFAULT;
		buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		buffer_desc.CPUAccessFlags = 0;
		buffer_desc.ByteWidth = sizeof(common_pipeline_states::scene_constants);
		//	viewport_rectは変化しないので初回のみアップロードしておく
		common_pipeline_states::scene_constants ui_scene_constant{};
		ui_scene_constant.viewport_rect = { 0.0f, 0.0f, static_cast<float>(scene_width), static_cast<float>(scene_height) };
		D3D11_SUBRESOURCE_DATA initial_data{};
		initial_data.pSysMem = &ui_scene_constant;
		hr = render_data::instance()->get_device()->CreateBuffer(&buffer_desc, &initial_data, ui_scene_constant_buffer.ReleaseAndGetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	}

	//	メイン視点のシーン定数バッファ生成
	{
		HRESULT hr;
		D3D11_BUFFER_DESC buffer_desc{};
		buffer_desc.Usage = D3D11_USAGE_DEFAULT;
		buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		buffer_desc.CPUAccessFlags = 0;
		buffer_desc.ByteWidth = sizeof(common_pipeline_states::scene_constants);
		hr = render_data::instance()->get_device()->CreateBuffer(&buffer_desc, nullptr, main_scene_constant_buffer.ReleaseAndGetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	}
	{
		D3D11_TEXTURE2D_DESC texture2d_desc{};
		texture2d_desc.Width = scene_width;
		texture2d_desc.Height = scene_height;
		texture2d_desc.MipLevels = 1;
		texture2d_desc.ArraySize = 1;
		texture2d_desc.SampleDesc.Count = 1;
		texture2d_desc.SampleDesc.Quality = 0;
		texture2d_desc.Usage = D3D11_USAGE_DEFAULT;
		texture2d_desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
		texture2d_desc.CPUAccessFlags = 0;
		texture2d_desc.MiscFlags = 0;

		//	シーンテクスチャ生成
		{
			texture2d_desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
			texture2d_desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
			if (!texture_manager::instance()->generate_texture2d(render_targets->scene_color_texture, common_pipeline_states::scene_color_map_name, texture2d_desc))
			{
				return false;
			}
		}

		//	深度ステンシルバッファを生成
		{
			texture2d_desc.Format = common_pipeline_states::scene_depth_texture_format;
			texture2d_desc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

			D3D11_SHADER_RESOURCE_VIEW_DESC shader_resource_view_desc{};
			shader_resource_view_desc.Format = common_pipeline_states::scene_depth_shader_resource_view_format;
			shader_resource_view_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			shader_resource_view_desc.Texture2D.MostDetailedMip = 0;
			shader_resource_view_desc.Texture2D.MipLevels = 1;
			if (!texture_manager::instance()->generate_texture2d(render_targets->scene_depth_stencil_texture, common_pipeline_states::scene_depth_map_name, texture2d_desc, &shader_resource_view_desc))
			{
				return false;
			}
		}

		//	GBuffer生成
		{
			texture2d_desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
			DXGI_FORMAT formats[] =
			{
				DXGI_FORMAT_R8G8B8A8_UNORM,			//	BaseColor
				DXGI_FORMAT_R32G32B32A32_FLOAT,		//	Normal_Depth
				DXGI_FORMAT_R8G8B8A8_UNORM,			//	Parameters
				DXGI_FORMAT_R32G32B32A32_FLOAT,		//	Velocity
				DXGI_FORMAT_R32G32B32A32_UINT,		//	UniqueData

				DXGI_FORMAT_R8G8B8A8_UNORM,			//	[エンジン改造:G-Buffer]
				DXGI_FORMAT_R8G8B8A8_UNORM,			//	[エンジン改造:G-Buffer]
			};
			for (int index = 0; index < static_cast<int>(magic_enum::enum_count<common_pipeline_states::gbuffer_id>()); ++index)
			{
				texture_ptr texture;
				texture2d_desc.Format = formats[index];
				if (texture_manager::instance()->generate_texture2d(texture,
																	 common_pipeline_states::gbuffer_names[index],
																	 texture2d_desc))
				{
					render_targets->g_buffer_textures.emplace_back(texture);
				}
			}
		}
	}

	//	シャドウマップ準備
	{
		Microsoft::WRL::ComPtr<ID3D11Texture2D> depth_buffer{};
		D3D11_TEXTURE2D_DESC texture2d_desc{};
		texture2d_desc.Width = 2048;
		texture2d_desc.Height = 2048;
		texture2d_desc.MipLevels = 1;
		texture2d_desc.ArraySize = 6;
		texture2d_desc.Format = common_pipeline_states::shadow_map_texture_format;
		texture2d_desc.SampleDesc.Count = 1;
		texture2d_desc.SampleDesc.Quality = 0;
		texture2d_desc.Usage = D3D11_USAGE_DEFAULT;
		texture2d_desc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
		texture2d_desc.CPUAccessFlags = 0;
		texture2d_desc.MiscFlags = 0;

		D3D11_SHADER_RESOURCE_VIEW_DESC shader_resource_view_desc{};
		shader_resource_view_desc.Format = common_pipeline_states::shadow_map_shader_resource_view_format;
		shader_resource_view_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
		shader_resource_view_desc.Texture2DArray.MostDetailedMip = 0;
		shader_resource_view_desc.Texture2DArray.MipLevels = 1;
		shader_resource_view_desc.Texture2DArray.FirstArraySlice = 0;
		shader_resource_view_desc.Texture2DArray.ArraySize = texture2d_desc.ArraySize;

		if (!texture_manager::instance()->generate_texture2d(render_targets->shadowmap_depth_texture, common_pipeline_states::shadow_maps_name, texture2d_desc, &shader_resource_view_desc))
		{
			return false;
		}
	}

	//	ワークバッファ準備
	D3D11_TEXTURE2D_DESC ssr_texture2d_desc{};
	{
		D3D11_TEXTURE2D_DESC texture2d_desc{};
		texture2d_desc.Width = scene_width;
		texture2d_desc.Height = scene_height;
		texture2d_desc.MipLevels = 1;
		texture2d_desc.ArraySize = 1;
		texture2d_desc.SampleDesc.Count = 1;
		texture2d_desc.SampleDesc.Quality = 0;
		texture2d_desc.Usage = D3D11_USAGE_DEFAULT;
		texture2d_desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
		texture2d_desc.CPUAccessFlags = 0;
		texture2d_desc.MiscFlags = 0;
		ssr_texture2d_desc = texture2d_desc;	//	現時点の物をコピっておく

		static constexpr std::string_view WorkColorName = "Work Scene Color ";
		texture2d_desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		for (size_t index = 0; index < ARRAYSIZE(render_targets->work_buffer_scene_textures); ++index)
		{
			std::string name = std::string(WorkColorName) + std::to_string(index);
			if (!texture_manager::instance()->generate_texture2d(render_targets->work_buffer_scene_textures[index], name, texture2d_desc))
			{
				return false;
			}
		}

		static constexpr std::string_view WorkRGBA32FloatName = "Work RGBA32 Float";
		texture2d_desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		if (!texture_manager::instance()->generate_texture2d(render_targets->work_buffer_rgba32float_texture, WorkRGBA32FloatName, texture2d_desc))
		{
			return false;
		}

		static constexpr std::string_view WorkHalfColorName = "Work Half Scene Color ";
		texture2d_desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		texture2d_desc.Width = scene_width / 2;
		texture2d_desc.Height = scene_height / 2;
		for (size_t index = 0; index < ARRAYSIZE(render_targets->work_buffer_half_scene_textures); ++index)
		{
			std::string name = std::string(WorkHalfColorName) + std::to_string(index);
			if (!texture_manager::instance()->generate_texture2d(render_targets->work_buffer_half_scene_textures[index], name, texture2d_desc))
			{
				return false;
			}
		}
	}

	//	SSRデノイズ用情報
	{
		static constexpr std::string_view SSRAccumateTextureName = "SSR Accumate Texture ";
		ssr_texture2d_desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		for (size_t index = 0; index < ARRAYSIZE(render_targets->ssr_accumlate_textures); ++index)
		{
			std::string name = std::string(SSRAccumateTextureName) + std::to_string(index);
			if (!texture_manager::instance()->generate_texture2d(render_targets->ssr_accumlate_textures[index], name, ssr_texture2d_desc))
			{
				return false;
			}
		}
		static constexpr std::string_view SSRCounterTextureName = "SSR Counter Texture";
		ssr_texture2d_desc.Format = DXGI_FORMAT_R16_FLOAT;
		for (size_t index = 0; index < ARRAYSIZE(render_targets->ssr_counter_textures); ++index)
		{
			std::string name = std::string(SSRCounterTextureName) + std::to_string(index);
			if (!texture_manager::instance()->generate_texture2d(render_targets->ssr_counter_textures[index], name, ssr_texture2d_desc))
			{
				return false;
			}
		}
	}

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
