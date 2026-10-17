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
	//	view_stateの破棄フレーム数
	constexpr uint64_t scene_view_state_keep_frames = 60;

	//	描画関係の設定
	console_variable<bool> single_thread_variable("r.single_thread", false, "描画処理をシングルスレッドで行う", console_flags::user_setting);

#if defined(USE_IMGUI)
	console_variable<bool> render_thread_variable("r.render_thread", true, "描画スレッドと提出スレッドで描画する");
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
			const float elapsed_time = tictoc.time_interval();
			update(elapsed_time);
			render(elapsed_time);

			//	詳細プロファイラーのフレーム終了
			DETAIL_PROFILE_END_FRAME();
		}
	}

	//	レンダリングスレッド停止
	stop_render_threads();

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

	render_data::create_instance(hwnd);
	mouse::create_instance(hwnd);
	keyboard::create_instance();
	texture_manager::create_instance();
	font_data_manager::create_instance();
	shader_manager::create_instance();
	pipeline_state_manager::create_instance();
	model_buffer_pool_manager::create_instance();
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

	//	ワールド毎に持つ管理クラス
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

#if defined(USE_IMGUI)
	editor_items::initialize();
#endif // defined(USE_IMGUI)

	//	各種バッファ用意
	render_targets = std::make_shared<scene_render_targets>();
	if (!create_buffers())
		return false;

	//	当たり判定フィルター用意
	if (!create_collision_filters())
		return false;


	//	シーンレンダラー登録
	{
		//	描画コマンドマルチスレッド生成
		render_executor = std::make_shared<render_job_executor>(4);

		//	フレームグラフ生成
		frame_render_graph = std::make_shared<frame_graph>();

		//	描画機能ごとの共有リソース
		scene_features = std::make_unique<scene_rendering::scene_render_features>();

		render_targets->create_output_view();
		main_view_target = std::make_shared<view_target>();
		main_view_target->name = "main";
		main_view_target->state_key = main_scene_view_state_key;
		main_view_target->width = SCREEN_WIDTH;
		main_view_target->height = SCREEN_HEIGHT;
		main_view_target->render_targets = render_targets;
		main_view_target->scene_constant_buffer = main_scene_constant_buffer;
	}

	//	@tool 描画側が参照している資源を書き換える前に、描画中のフレームを待てるようにする
	main_thread_id = std::this_thread::get_id();
	render_data::instance()->set_flush_rendering_function([this] { flush_rendering(); });

	//	初回シーン登録
	scene_manager::instance()->change_scene<load_shader_scene>();
	return true;
}

void framework::update(float elapsed_time/*Elapsed seconds from last frame*/)
{
	//	更新処理全体の計測
	DETAIL_PROFILE_CPU_SCOPE("update");

	//	@tool 前フレームのエフェクト更新（GPU）はエフェクトのインスタンスを読むので、終わるまで待ってから更新する
	wait_effect_update();

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

#if defined(USE_IMGUI)
	//	キー割り当てされたコンソールコマンドを実行
	console_manager::instance().update_bindings();
#endif //	defined(USE_IMGUI)

#if defined(USE_IMGUI)
	//	@tool モデル編集用のワールドの生成・破棄・描画先の作り直し
	update_model_editor_world();
#endif	//	defined(USE_IMGUI)

	//	@tool 各ワールドの画面が入力を受け付けるかを決める
	update_world_viewports();

	//	@tool ワールドごとの更新（メイン → ImGui のウィンドウに映すワールド）
	for_each_world([this, elapsed_time] { update_world(elapsed_time); });

#if defined(USE_IMGUI)
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

	//	@tool モデル編集用のワールドの情報と、それを映すウィンドウ
	if (model_editor && model_editor->world)
	{
		engine_world::scope world_scope(*model_editor->world);
		scene_manager::instance()->imgui_render("scenes (model editor)");
		object_manager::instance()->imgui_render("objects (model editor)");
	}
	imgui_model_editor_window();

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

void framework::update_world_viewports()
{
#if defined(USE_IMGUI)
	//	@tool モデル編集用のワールドの画面の上にマウスがある間・そこで押したボタンを押し続けている間は、そちらが入力を受け取る
	const bool editor_input = model_editor && model_editor->visible && (model_editor->hovered || model_editor->captured);

	//	@tool メイン画面は ImGui のウィンドウの上ではマウスを受け付けない
	world_viewport::instance()->set_accepts_input(!ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow), !editor_input);

	if (model_editor && model_editor->world)
	{
		engine_world::scope world_scope(*model_editor->world);
		world_viewport::instance()->set_accepts_input(editor_input, editor_input);
	}
#endif	//	defined(USE_IMGUI)
}

#if defined(USE_IMGUI)
void framework::update_model_editor_world()
{
	//	@tool 閉じる（描画中のフレームがワールドの管理クラスを参照しているので、終わるのを待ってから破棄する）
	if (model_editor_close_requested)
	{
		model_editor_close_requested = false;
		if (model_editor)
		{
			flush_rendering();
			model_editor.reset();
		}
	}

	//	@tool 開く（モデル編集シーンを読み込んだワールドを作る）
	if (model_editor_open_requested)
	{
		model_editor_open_requested = false;
		if (!model_editor)
		{
			model_editor = std::make_unique<editor_world_view>();
			model_editor->world = std::make_unique<engine_world>("model_editor");

			engine_world::scope world_scope(*model_editor->world);
			world_viewport::instance()->set_in_window(true);
			world_viewport::instance()->set_accepts_input(false, false);
			scene_manager::instance()->change_scene<edit_model_scene>();
		}
	}

	if (!model_editor)
		return;

	//	@tool ウィンドウの大きさが変わったら描画先を作り直す
	const int width = model_editor->requested_width;
	const int height = model_editor->requested_height;
	if (width <= 0 || height <= 0)
		return;
	if (model_editor->target && model_editor->target->width == width && model_editor->target->height == height)
		return;

	//	@tool 視点の状態（履歴・カリング結果など）は大きさに依存するので、作り直す度に別の物を使う
	const std::string name = "model_editor#" + std::to_string(++model_editor->target_generation);
	model_editor->target = create_view_target(name, width, height);
	model_editor->camera_cut = true;
}

void framework::imgui_model_editor_window()
{
	if (!model_editor)
		return;

	bool open = true;
	ImGui::SetNextWindowSize(ImVec2(640.0f, 360.0f), ImGuiCond_FirstUseEver);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	model_editor->visible = ImGui::Begin("Model Editor View", &open);
	ImGui::PopStyleVar();
	model_editor->hovered = false;
	if (model_editor->visible)
	{
		//	@tool 描画先は描画領域の大きさで作る（次のフレームの更新で作り直す）
		const ImVec2 available = ImGui::GetContentRegionAvail();
		model_editor->requested_width = (std::max)(1, static_cast<int>(available.x));
		model_editor->requested_height = (std::max)(1, static_cast<int>(available.y));

		if (model_editor->target && model_editor->target->render_targets->output_texture)
		{
			const view_target& target = *model_editor->target;
			const ImVec2 position = ImGui::GetCursorScreenPos();
			ImGui::Image(target.render_targets->output_texture->get_shader_resource_view().Get(), ImVec2(static_cast<float>(target.width), static_cast<float>(target.height)));
			model_editor->hovered = ImGui::IsItemHovered();

			//	@tool 画面の位置をクライアント座標にする（ImGui のビューポートが有効な場合はスクリーン座標になっている）
			POINT client_position = { static_cast<LONG>(position.x), static_cast<LONG>(position.y) };
			if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
				::ScreenToClient(hwnd, &client_position);

			engine_world::scope world_scope(*model_editor->world);
			world_viewport::instance()->set_rect({ static_cast<float>(client_position.x), static_cast<float>(client_position.y), static_cast<float>(target.width), static_cast<float>(target.height) });
		}
	}
	ImGui::End();

	//	@tool 画面の上で押したボタンを押している間は、画面の外へ出てもこちらが入力を受け取る
	const bool any_down = ImGui::IsMouseDown(ImGuiMouseButton_Left) || ImGui::IsMouseDown(ImGuiMouseButton_Right) || ImGui::IsMouseDown(ImGuiMouseButton_Middle);
	const bool any_clicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left) || ImGui::IsMouseClicked(ImGuiMouseButton_Right) || ImGui::IsMouseClicked(ImGuiMouseButton_Middle);
	if (!any_down)
		model_editor->captured = false;
	else if (model_editor->hovered && any_clicked)
		model_editor->captured = true;

	if (!open)
		model_editor_close_requested = true;
}
#endif	//	defined(USE_IMGUI)

void framework::build_particle_render_lists(const render_data_context& render_data, render_particle_indices& particle_indices)
{
	for (auto& particle_index : render_data.effect_container.particle_color_datas)
	{
		auto& material = render_data.effect_container.particle_proxies[particle_index].material_data.material;

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

	//	@tool 描画スレッドの使用を切り替える（切り替えは描画中のフレームが無い状態で行う）
	if (render_thread_variable.get() != threaded_rendering)
	{
		if (threaded_rendering)
			stop_render_threads();
		else
			start_render_threads();
	}

	//	描画前処理
	for_each_world([]
	{
		if (object_manager::is_valid())
			object_manager::instance()->pre_render();
	});

	//	@tool 描画に必要な情報を抽出して、描画側へ渡す
	frame_packet_ptr packet = extract_frame(elapsed_time);
	dispatch_frame(std::move(packet));

	//	描画後処理
	for_each_world([]
	{
		if (object_manager::is_valid())
			object_manager::instance()->post_render();
	});
}

framework::frame_packet_ptr framework::extract_frame(float elapsed_time)
{
	//	描画コマンドの生成をし終わったフレームの描画データをメインスレッドで破棄
	{
		std::vector<frame_packet_ptr> finished_packets;
		{
			std::lock_guard<std::mutex> lock(frame_mutex);
			finished_packets.swap(retired_packets);
		}
	}

	auto packet = std::make_shared<frame_packet>();
	packet->frame_index = ++dispatched_frame_index;
	packet->elapsed_time = elapsed_time;
	packet->previous_time = total_time;
	total_time += elapsed_time;
	packet->single_thread = single_thread_variable.get();
#if defined(USE_IMGUI)
	packet->freeze_culling = freeze_culling_variable.get();
	packet->visualize_meshlets = visualize_meshlets_variable.get();
#endif	//	defined(USE_IMGUI)
	packet->sync_interval = static_cast<UINT>(engine_config_data::instance()->windows.vertical_sync);
	packet->antialias_mode = engine_config_data::instance()->antialias.mode;

	//	描画用の情報収集
	//	@tool メイン画面のワールド → ImGui のウィンドウに映すワールドの順に抽出する（先頭がメイン画面）
	packet->views.emplace_back(extract_view(main_view_target, main_view_parameters, main_view_camera_cut, false));
#if defined(USE_IMGUI)
	if (model_editor && model_editor->target && model_editor->visible)
	{
		engine_world::scope world_scope(*model_editor->world);
		packet->views.emplace_back(extract_view(model_editor->target, model_editor->view, model_editor->camera_cut, true));
	}
#endif	//	defined(USE_IMGUI)

#if defined(USE_IMGUI)
	//	ImGui描画
	//	@tool ImGui のフレームはここで締め、メインのウィンドウの描画データは複製して提出スレッドで描く
	//	@tool 別ウィンドウ（ビューポート）はメインスレッドで描く。immediate context は提出スレッドと共有しているので排他する
	{
		ImGui::Render();
		const ImDrawData* source = ImGui::GetDrawData();
		auto snapshot = std::make_shared<imgui_draw_snapshot>();
		snapshot->draw_data = *source;
		snapshot->draw_data.CmdLists.resize(0);
		for (ImDrawList* list : source->CmdLists)
		{
			snapshot->draw_data.CmdLists.push_back(list->CloneOutput());

			//	@tool 参照しているテクスチャ（DX11 では SRV）を保持する
			for (const ImDrawCmd& command : list->CmdBuffer)
			{
				auto* view = static_cast<ID3D11ShaderResourceView*>(command.GetTexID());
				if (!view)
					continue;
				if (std::find_if(snapshot->textures.begin(), snapshot->textures.end(), [view](const auto& held) { return held.Get() == view; }) == snapshot->textures.end())
					snapshot->textures.emplace_back(view);
			}
		}
		packet->imgui_draw_data = snapshot;

		if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			std::lock_guard<std::recursive_mutex> immediate_lock(render_data::instance()->get_immediate_context_mutex());
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();
		}
	}
#endif // defined(USE_IMGUI)

	return packet;
}

framework::view_packet framework::extract_view(const view_target_ptr& target, view_parameters& parameters, bool& camera_cut, bool in_window)
{
	//	@tool 処理中のワールドから、1つの視点の描画に必要な情報を抽出する
	auto render_data = std::make_shared<render_data_context>();
	auto render_particle_indices = std::make_shared<::render_particle_indices>();
	{
		DETAIL_PROFILE_CPU_SCOPE("render.collect_render_data");

		//	@tool 描画設定（コンソール変数の値）を写す。描画側はコンソール変数を直接読まない
		clustered_lighting_system::capture_settings(render_data->settings.clustered_lighting);
		shadow_atlas::capture_settings(render_data->settings.shadow_atlas);

		//	視点の候補から今フレームのメイン視点を選ぶ
		view_selection selection;
		camera_cut = false;
		if (camera_manager::instance()->select_view(selection))
		{
			parameters = selection.parameters;
			camera_cut = selection.camera_cut;
		}

		//	@tool ImGui のウィンドウに映す場合は、投影をウィンドウの大きさに合わせる
		if (in_window)
			world_viewport::instance()->apply_screen_rect(parameters);

		//	相対描画用の描画原点
		const math::vector3d render_origin = scene_view::compute_render_origin(parameters, engine_config_data::instance()->rendering.camera_relative_rendering);
		render_data->render_origin = render_origin;

		//	ライト情報収集
		{
			//	メイン視点の視錐台と距離・画面上の大きさで間引く
			const scene_view collect_view = scene_view::from_parameters(parameters, render_origin);
			light_view_info view_info;
			view_info.position = parameters.position;
			view_info.tan_half_fov_y = std::tan(parameters.fov_y * 0.5f);
			view_info.frustum = &collect_view.get_frustum_culling();
			light_manager::instance()->get_container(render_data->light_container, render_origin, view_info);
		}

		//	ポストプロセスボリューム取得
		post_process_volume_manager::instance()->get_container(render_data->post_process_volume_container, parameters.position);

		//	メッシュ取得
		model_instance_manger::instance()->get_container(render_data->mesh_container, render_origin);

		//	エフェクトコンテナ取得
		effect_manager::instance()->get_container(render_data->effect_container, render_data->decal_container, render_origin);

		//	UIオブジェクトコンテナ収集
		//	@tool UI はメイン画面にだけ描く
		if (!in_window)
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
				sprite_data.sprite = ui_object->get_sprite().make_render_proxy();
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
					sprite_data.sprite = sprite.make_render_proxy();
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
		model_buffer_pool_manager::instance()->flush_uploads(render_data->upload_list);

		//	@tool パイプラインステートの描画前準備（描画ジョブ内ではメンバを書き換えない）
		pipeline_state_manager::instance()->prepare_pipeline_states();

		//	@tool マテリアルの定数バッファ変更分を転送一覧に積む（描画ジョブ内では更新しない）
		material::flush_parameters(render_data->upload_list);

#if defined(USE_IMGUI)
		//	デバッグ情報取得
		build_debug_primitives(*render_data);
		debug_primitive_manager::instance()->get_container(render_data->debug_draw_container, render_data->render_origin, render_data->upload_list);
#endif // defined(USE_IMGUI)
	}

	view_packet view;
	view.target = target;
	view.render_data = render_data;
	view.particle_indices = render_particle_indices;
	view.parameters = parameters;
	view.camera_cut = camera_cut;
	//	@tool 描画側はワールドの管理クラスを instance() で引かず、抽出時のワールドの物を使う
	view.effects = effect_manager::instance();
	return view;
}

void framework::render_frame(frame_packet& packet)
{
	const float elapsed_time = packet.elapsed_time;
	const uint64_t frame_index = packet.frame_index;

	//	バッファ描画
	{
		//	レンダラー開始処理
		render_executor->begin_command_list();

		frame_render_graph->begin_frame();

		//	フレームの転送
		for (const view_packet& view : packet.views)
		{
			const std::shared_ptr<render_data_context>& render_data = view.render_data;
			renderer_context context = {};
			context.render_data = render_data;

			struct pass_data
			{
				std::shared_ptr<render_data_context> render_data;
			};
			frame_render_graph->add_pass<pass_data>("frame_upload_pass", context, [&](frame_graph::builder& builder, pass_data& data)
			{
				builder.use_immediate_context(false);
				data.render_data = render_data;
			},
			[](const pass_data& data, const frame_graph::resources&, graphics_context& context)
			{
				data.render_data->upload_list.upload(context.recorder.get_device_context());
				data.render_data->mesh_container.transform_uploads.upload(context.recorder.get_device_context());
			});
		}

		//	描画準備
		{
			renderer_context context = {};
			context.render_data = packet.views.front().render_data;
			struct pass_data { UINT render_pass = 0; };
			frame_render_graph->add_pass<pass_data>("begin_draw_pass", context, [&](frame_graph::builder& builder, pass_data& data)
			{
				//	@tool バックバッファ（グラフの外）をクリアする
				frame_graph::color_attachment back_buffer;
				back_buffer.external_view = back_buffer_clear_rendering_pass.render_targets.front().view;
				back_buffer.clear = true;
				data.render_pass = builder.add_render_pass({ back_buffer });
			},
			[](const pass_data& data, const frame_graph::resources& resources, graphics_context& context)
			{
				//	画面クリア
				const render_pass& back_buffer_clear_rendering_pass = resources.get_render_pass(data.render_pass);
				back_buffer_clear_rendering_pass.activate(context.recorder.get_device_context());

				back_buffer_clear_rendering_pass.deactivate(context.recorder.get_device_context());
			});
		}

		for (std::size_t view_index = 0; view_index < packet.views.size(); ++view_index)
		{
			const view_packet& view = packet.views[view_index];
			const std::shared_ptr<render_data_context>& render_data = view.render_data;
			const std::shared_ptr<::render_particle_indices>& render_particle_indices = view.particle_indices;
			view_target& target = *view.target;

			//	@tool 視点で描画（視点ごとの状態・定数バッファ・レンダラー一式を使う）
			{
				const scene_view_state_ptr& state = scene_view_states.acquire(target.state_key, render_frame_index);
#if defined(USE_IMGUI)
				state->set_freeze_culling(packet.freeze_culling);
				state->set_debug_visualize_meshlets(packet.visualize_meshlets);
#endif	//	defined(USE_IMGUI)
				const scene_view_ptr scene_view_data = std::make_shared<const scene_view>(scene_view::from_parameters(view.parameters, render_data->render_origin));
				state->begin_frame(*scene_view_data, view.camera_cut);

				//	レンダラーコンテキスト準備
				renderer_context context = {};
				{
					context.render_data = render_data;
					context.render_mesh_indices = std::make_shared<render_mesh_indices>();
					context.render_particle_indices = render_particle_indices;
					context.render_decal_indices = std::make_shared<render_decal_indices>();
					prepare_renderer_context(context, scene_view_data, state, target.scene_constant_buffer, render_data, render_particle_indices, packet.previous_time, packet.previous_time + elapsed_time);
				}

				//	オクルージョンカリング結果を読み出す
				std::unordered_map<UINT, uint8_t> occlusion_visible;
				if (const auto& occlusion = state->get_occlusion_culling())
					occlusion->read_last_frame(occlusion_visible);

				//	クラスタードライティングのライト番号総数を読み出す
				if (const auto& clustered_lighting = state->get_clustered_lighting())
					clustered_lighting->read_statistics();

				//	描画メッシュ／デカールのカリング・ソート
				{
					DETAIL_PROFILE_CPU_SCOPE("render.build_render_lists");
					build_render_lists(*render_data, *state, scene_view_data->get_frustum_culling(), occlusion_visible,
						*context.render_mesh_indices, *context.render_decal_indices);
				}

				//	シーン構築
				//	@tool エフェクトの更新（GPU）はエフェクトのインスタンスを読むので、終わったらメインスレッドへ知らせる（最後の視点の分が終われば全て終わっている）
				//	@tool 記録中のジョブとは CPU 側の情報を取り合わないので、記録の完了を待たずに実行する
				{
					const bool notify_effect_update = view_index + 1 == packet.views.size();
					//	@tool エフェクトの更新完了の通知は framework の同期用の情報を使う
					struct pass_data { std::function<void()> notify; effect_manager* effects = nullptr; };
					frame_render_graph->add_pass<pass_data>("scene_constantract_pass", context, [&](frame_graph::builder& builder, pass_data& data)
					{
						builder.use_immediate_context(false);
						data.effects = view.effects;
						if (notify_effect_update)
						{
							data.notify = [this, frame_index]
							{
								{
									std::lock_guard<std::mutex> lock(frame_mutex);
									effect_updated_frame_index = frame_index;
								}
								frame_condition.notify_all();
							};
						}
					},
					[](const pass_data& data, const frame_graph::resources&, graphics_context& context)
					{
						//	シーン定数バッファ更新
						context.recorder.get_device_context()->UpdateSubresource(context.scene_constant_buffer.Get(), 0, 0, &context.scene_constant, 0, 0);

						//	エフェクト類のGPU側の更新
						context.debug_begin_event(L"update_effect_gpu");
						data.effects->update_gpu(context, context.scene_constant.time_data.x - context.scene_constant.time_data.y);
						context.debug_end_event();

						if (data.notify)
							data.notify();
					});
				}

				//	描画
				{
					DETAIL_PROFILE_CPU_SCOPE("render.scene_renderers_issue");
					//	@tool 視点のシーンのパスを実行順に足す（共有資源と視点ごとの資源を使う）
					scene_rendering::scene_frame scene_frame;
					scene_frame.context = context;
					scene_frame.targets = target.render_targets;
					scene_frame.textures = scene_renderer_commons::import_scene_textures(*frame_render_graph, *target.render_targets);
					scene_rendering::add_scene_passes(*frame_render_graph, scene_frame, *scene_features);
				}

				//	描画終了処理
				state->end_frame(*scene_view_data);
			}

			//	UI描画
			//	@tool UI はメイン画面（先頭の視点）にだけ描く
			if (view_index == 0)
			{
				renderer_context context = {};
				context.scene_constant_buffer = ui_scene_constant_buffer;
				context.render_data = render_data;
				context.render_mesh_indices = std::make_shared<render_mesh_indices>();
				DETAIL_PROFILE_CPU_SCOPE_CATEGORY(renderer_issue, "ui");
				frame_graph::owner_scope owner(*frame_render_graph, "ui");
				scene_rendering::add_ui_pass(*frame_render_graph, context, target.render_targets, *scene_features);
			}
		}

		//	描画コマンドを実行
		{
			DETAIL_PROFILE_CPU_SCOPE("render.execute_commands");

			//	
			frame_render_graph->execute(*render_executor);

			//	生成されたコマンドリスト保存
			packet.command_list = render_executor->end_command_list();
		}
	}

	//	使われなくなった view_state を破棄
	scene_view_states.release_unused(render_frame_index, scene_view_state_keep_frames);
	++render_frame_index;
}

void framework::submit_frame(frame_packet& packet)
{
	ID3D11DeviceContext* immediate_context = render_data::instance()->get_immediate_context().Get();

	//	@tool 提出物を順に実行する（記録が終わっていないものは待つ）
	{
		DETAIL_PROFILE_CPU_SCOPE("render.execute_command_list");
		packet.command_list->execute(immediate_context);
	}

#if defined(USE_IMGUI)
	//	ImGui描画
	if (packet.imgui_draw_data)
	{
		DETAIL_PROFILE_CPU_SCOPE("render.imgui_and_present");
		imgui_rendering_pass.activate(immediate_context);
		immediate_context->RSSetViewports(1, &imgui_viewport);
		ImGui_ImplDX11_RenderDrawData(&packet.imgui_draw_data->draw_data);
		imgui_rendering_pass.deactivate(immediate_context);
	}
#endif // defined(USE_IMGUI)

	//	プレゼント
	render_data::instance()->get_swap_chain()->Present(packet.sync_interval, 0);
}

void framework::dispatch_frame(frame_packet_ptr packet)
{
	if (!threaded_rendering)
	{
		//	@tool 描画スレッドを使わない場合は、メインスレッドで順に行う
		render_frame(*packet);
		{
			std::lock_guard<std::recursive_mutex> immediate_lock(render_data::instance()->get_immediate_context_mutex());
			submit_frame(*packet);
		}
		std::lock_guard<std::mutex> lock(frame_mutex);
		submitted_frame_index = packet->frame_index;
		return;
	}

	{
		std::unique_lock<std::mutex> lock(frame_mutex);
		frame_condition.wait(lock, [&] { return frames_in_flight < max_frames_in_flight; });
		++frames_in_flight;
		render_queue.emplace_back(std::move(packet));
	}
	frame_condition.notify_all();
}

void framework::flush_rendering()
{
	//	@tool 描画スレッド・提出スレッドから呼ばれた場合は待たない（自分自身を待つことになるため）
	if (!threaded_rendering || std::this_thread::get_id() != main_thread_id)
		return;

	std::unique_lock<std::mutex> lock(frame_mutex);
	frame_condition.wait(lock, [&] { return frames_in_flight == 0; });
}

void framework::wait_effect_update()
{
	if (!threaded_rendering)
		return;

	std::unique_lock<std::mutex> lock(frame_mutex);
	frame_condition.wait(lock, [&] { return effect_updated_frame_index >= dispatched_frame_index || submitted_frame_index >= dispatched_frame_index; });
}

void framework::start_render_threads()
{
	if (render_threads_running)
		return;

	render_threads_running = true;
	threaded_rendering = true;
	render_thread = std::thread(&framework::render_thread_main, this);
	submit_thread = std::thread(&framework::submit_thread_main, this);
}

void framework::stop_render_threads()
{
	if (!render_threads_running)
		return;

	flush_rendering();
	{
		std::lock_guard<std::mutex> lock(frame_mutex);
		render_threads_running = false;
	}
	frame_condition.notify_all();
	render_thread.join();
	submit_thread.join();
	threaded_rendering = false;
	retired_packets.clear();
}

void framework::render_thread_main()
{
	while (true)
	{
		frame_packet_ptr packet;
		{
			std::unique_lock<std::mutex> lock(frame_mutex);
			frame_condition.wait(lock, [&] { return !render_queue.empty() || !render_threads_running; });
			if (render_queue.empty())
				break;
			packet = std::move(render_queue.front());
			render_queue.pop_front();
		}

		//	@tool 描画パスを組み立てて記録を始め、提出スレッドへ渡す
		render_frame(*packet);
		const uint64_t frame_index = packet->frame_index;
		{
			std::lock_guard<std::mutex> lock(frame_mutex);
			submit_queue.emplace_back(std::move(packet));
		}
		frame_condition.notify_all();

		//	@tool レンダラーの状態は提出中のパスも参照するので、提出が終わってから次のフレームを組み立てる
		std::unique_lock<std::mutex> lock(frame_mutex);
		frame_condition.wait(lock, [&] { return submitted_frame_index >= frame_index; });
	}
}

void framework::submit_thread_main()
{
	while (true)
	{
		frame_packet_ptr packet;
		{
			std::unique_lock<std::mutex> lock(frame_mutex);
			frame_condition.wait(lock, [&] { return !submit_queue.empty() || (!render_threads_running && render_queue.empty() && frames_in_flight == 0); });
			if (submit_queue.empty())
				break;
			packet = std::move(submit_queue.front());
			submit_queue.pop_front();
		}

		{
			std::lock_guard<std::recursive_mutex> immediate_lock(render_data::instance()->get_immediate_context_mutex());
			submit_frame(*packet);
		}

		const uint64_t frame_index = packet->frame_index;
		{
			std::lock_guard<std::mutex> lock(frame_mutex);
			//	@tool 描画データの破棄（モデルリソースなどのデストラクタ）はメインスレッドで行う
			retired_packets.emplace_back(std::move(packet));
			submitted_frame_index = frame_index;
			--frames_in_flight;
		}
		frame_condition.notify_all();
	}
}

#if defined(USE_IMGUI)
framework::imgui_draw_snapshot::~imgui_draw_snapshot()
{
	for (ImDrawList* list : draw_data.CmdLists)
	{
		IM_DELETE(list);
	}
}
#endif	//	defined(USE_IMGUI)

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
		const auto* material = (material_data_ptr + draw_data->material_index)->material.get();
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
	const auto& decal_proxies = render_data.decal_container.proxies;
	decal_indices.draw_list.reserve(decal_proxies.size());
	for (UINT decal_index = 0; decal_index < decal_proxies.size(); ++decal_index)
	{
		//	フラスタムカリング
		if (!frustum_culling.to_box(decal_proxies[decal_index].box))
			continue;

		decal_indices.draw_list.emplace_back(decal_index);
	}
}


bool framework::uninitialize()
{
	//	@tool ワールド（シーン・オブジェクト・ワールドごとの管理クラス）を破棄する
#if defined(USE_IMGUI)
	model_editor.reset();
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
	pipeline_state_manager::delete_instance();
	shader_manager::delete_instance();
	font_data_manager::delete_instance();
	minigame_record_data::delete_instance();
	texture_manager::delete_instance();
	keyboard::delete_instance();
	mouse::delete_instance();
	render_data::delete_instance();

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
		hr = render_data::instance()->get_device()->CreateBuffer(&buffer_desc, nullptr, ui_scene_constant_buffer.ReleaseAndGetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
		//	viewport_rectは変化しないので初回のみアップロードしておく
		common_pipeline_states::scene_constants ui_scene_constant{};
		ui_scene_constant.viewport_rect = { 0.0f, 0.0f, static_cast<float>(scene_width), static_cast<float>(scene_height) };
		render_data::instance()->get_immediate_context()->UpdateSubresource(ui_scene_constant_buffer.Get(), 0, 0, &ui_scene_constant, 0, 0);
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
	//	@tool シーン用のレンダーターゲット（メイン画面の物は名前で登録する）
	if (!create_scene_render_targets(*render_targets, scene_width, scene_height, true))
		return false;

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

framework::view_target_ptr framework::create_view_target(std::string_view name, int width, int height)
{
	auto target = std::make_shared<view_target>();
	target->name = name;
	target->state_key = scene_view_state_cache::make_key(name);
	target->width = width;
	target->height = height;

	//	@tool シーン用のレンダーターゲット（名前で登録しない。描画先と一緒に解放される）
	target->render_targets = std::make_shared<scene_render_targets>();
	if (!create_scene_render_targets(*target->render_targets, width, height, false))
		return nullptr;
	//	@tool 影は視点ごとに描き直すので、メイン画面の物を共有する
	target->render_targets->shadowmap_depth_texture = render_targets->shadowmap_depth_texture;

	//	@tool 最終的な出力先（ImGui で表示する。バックバッファと同じ形式）
	{
		D3D11_TEXTURE2D_DESC texture2d_desc{};
		texture2d_desc.Width = width;
		texture2d_desc.Height = height;
		texture2d_desc.MipLevels = 1;
		texture2d_desc.ArraySize = 1;
		texture2d_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		texture2d_desc.SampleDesc.Count = 1;
		texture2d_desc.Usage = D3D11_USAGE_DEFAULT;
		texture2d_desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
		if (!texture_manager::instance()->generate_unregistered_texture2d(target->render_targets->output_texture, texture2d_desc))
			return nullptr;
	}

	//	@tool 視点のシーン定数バッファ
	{
		D3D11_BUFFER_DESC buffer_desc{};
		buffer_desc.Usage = D3D11_USAGE_DEFAULT;
		buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		buffer_desc.ByteWidth = sizeof(common_pipeline_states::scene_constants);
		HRESULT hr = render_data::instance()->get_device()->CreateBuffer(&buffer_desc, nullptr, target->scene_constant_buffer.ReleaseAndGetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
		if (FAILED(hr))
			return nullptr;
	}

	//	@tool 出力先の描画先を作り置く
	target->render_targets->create_output_view();
	return target;
}

bool framework::create_scene_render_targets(scene_render_targets& targets, int scene_width, int scene_height, bool register_names)
{
	//	@tool 名前で登録しない物（エディターの画面など）は、持ち主の破棄と一緒に解放される
	auto generate = [register_names](texture_ptr& out_texture, std::string_view name, D3D11_TEXTURE2D_DESC desc, D3D11_SHADER_RESOURCE_VIEW_DESC* srv_desc = nullptr)
	{
		if (register_names)
			return texture_manager::instance()->generate_texture2d(out_texture, name, desc, srv_desc);
		return texture_manager::instance()->generate_unregistered_texture2d(out_texture, desc, srv_desc);
	};

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
			if (!generate(targets.scene_color_texture, common_pipeline_states::scene_color_map_name, texture2d_desc))
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
			if (!generate(targets.scene_depth_stencil_texture, common_pipeline_states::scene_depth_map_name, texture2d_desc, &shader_resource_view_desc))
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
			for (int index = 0; index < static_cast<int>(common_pipeline_states::gbuffer_id::max); ++index)
			{
				texture_ptr texture;
				texture2d_desc.Format = formats[index];
				if (generate(texture,
																	 common_pipeline_states::gbuffer_names[index],
																	 texture2d_desc))
				{
					targets.g_buffer_textures.emplace_back(texture);
				}
			}
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

		//	@tool 作業用テクスチャは各レンダラーがフレーム毎にレンダーグラフの一時テクスチャとして要求する（ここでは作らない）
	}

	//	SSRデノイズ用情報
	{
		static constexpr std::string_view SSRAccumateTextureName = "SSR Accumate Texture ";
		ssr_texture2d_desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		for (size_t index = 0; index < ARRAYSIZE(targets.ssr_accumlate_textures); ++index)
		{
			std::string name = std::string(SSRAccumateTextureName) + std::to_string(index);
			if (!generate(targets.ssr_accumlate_textures[index], name, ssr_texture2d_desc))
			{
				return false;
			}
		}
		static constexpr std::string_view SSRCounterTextureName = "SSR Counter Texture";
		ssr_texture2d_desc.Format = DXGI_FORMAT_R16_FLOAT;
		for (size_t index = 0; index < ARRAYSIZE(targets.ssr_counter_textures); ++index)
		{
			std::string name = std::string(SSRCounterTextureName) + std::to_string(index);
			if (!generate(targets.ssr_counter_textures[index], name, ssr_texture2d_desc))
			{
				return false;
			}
		}
	}

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
