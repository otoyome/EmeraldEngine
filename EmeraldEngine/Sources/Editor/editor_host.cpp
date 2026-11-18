#include "Editor/editor_host.h"
#include "Input/mouse.h"

#if defined(USE_IMGUI)
#include "Graphics/Core/render_data.h"
#include "Graphics/Debug/debug_primitive_collector.h"
#include "Model/model_resource.h"
#include "Effect/effect_resource.h"
#include "Material/material_resource_factory.h"

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui/imgui_impl_dx11.h"
#include "imgui/imgui_impl_win32.h"
#include "imPlot/imPlot.h"
#include "ImGuizmo/ImCurveEdit.h"
#include "imtimeline/ImTimeline.h"
#include "im-neo-sequencer/imgui_neo_sequencer.h"
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
extern ImWchar glyphRangesJapanese[];

#include "Editor/editor_items.h"
#include "Editor/detail_profiler.h"
#include "Editor/Windows/log_window.h"
#include "Editor/Windows/engine_config_window.h"
#include "Editor/Windows/model_buffer_pool_window.h"
#include "Editor/Windows/memory_allocator_window.h"
#include "Editor/Windows/console_windows.h"
#include "Editor/Windows/gamepad_window.h"
#include "Editor/Windows/sound_bank_editor_window.h"
#include "Editor/Windows/scene_set_editor_window.h"
#include "Editor/Windows/texture_editor_window.h"
#include "Object/world.h"
#include "Editor/editor_camera.h"
#include "Renderer/debug_view_settings.h"
#include "System/console_manager.h"
#include "System/asset_browser.h"
#include "System/asset_converter.h"
#include "System/imgui_texture_keeper.h"
#include "Resource/resource_manager.h"
#include "Material/material_resource.h"


#include "Scenes/scene_manager.h"
#include "Scenes/scene_instance.h"
#include "Scenes/scene_paths.h"
#include "Scenes/scene_resource.h"
#include "Scenes/scene_set_resource.h"
#include "Resource/asset_registry.h"
#include "Object/prefab_resource.h"
#include "Object/game_object_registry.h"
#include "Object/spatial_object.h"

#include "imPlot/imPlot.h"
#include "imtimeline/ImTimeline.h"

namespace
{
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

	//	アセットブラウザでダブルクリックしたファイルを、シーンを切り替えて開く
	void register_asset_open_handler(const editor_host::play_function& play_scene, std::string_view label, std::vector<cache_string> extensions, std::string_view scene_path)
	{
		asset_browser::open_handler handler;
		handler.label = label;
		handler.extensions = std::move(extensions);
		handler.open_file = [play_scene, scene_path](const cache_string& file_path)
		{
			play_scene(scene_path, file_path.view());
		};
		asset_browser::instance()->register_open_handler(std::move(handler));
	}

	//	アセットブラウザの新規作成でドメインの既定のマテリアルリソースを保存する
	void register_material_create_handler(std::string_view label, material_resource::domains domain)
	{
		asset_browser::create_handler handler;
		handler.label = label;
		handler.default_name = "NewMaterial";
		handler.extension = std::string(".") + std::string(material_resource::extension);
		handler.create = [domain](std::string_view file_path)
		{
			const std::string name = std::filesystem::path(std::string(file_path)).stem().string();
			auto material = material_resource_factory::create_new(name, domain);
			return resource_manager::instance()->save(material, file_path);
		};
		asset_browser::instance()->register_create_handler(std::move(handler));
	}

	//	ツール窓の表示状態
	console_variable<bool> log_window_variable("editor.window.log", true, "ログウィンドウの表示", console_flags::persist);
	console_variable<bool> engine_config_window_variable("editor.window.engine_config", true, "エンジン設定ウィンドウの表示", console_flags::persist);
	console_variable<bool> profiler_window_variable("editor.window.profiler", true, "詳細プロファイラーウィンドウの表示", console_flags::persist);
	console_variable<bool> model_buffer_pool_window_variable("editor.window.model_buffer_pool", true, "モデルバッファプール使用量ウィンドウの表示", console_flags::persist);
	console_variable<bool> memory_allocator_window_variable("editor.window.memory_allocator", true, "メモリアロケータ使用量ウィンドウの表示", console_flags::persist);
	console_variable<bool> console_window_variable("editor.window.console", false, "コンソールウィンドウの表示");
	console_variable<bool> console_commands_window_variable("editor.window.console_commands", true, "コンソール要素リストウィンドウの表示", console_flags::persist);
	console_variable<bool> gamepad_window_variable("editor.window.gamepad", true, "ゲームパッド確認ウィンドウの表示", console_flags::persist);
	console_variable<bool> asset_browser_window_variable("editor.window.asset_browser", true, "アセットブラウザの表示", console_flags::persist);
}

editor_host::editor_host(HWND hwnd)
: hwnd(hwnd)
{
	create_display_resources();
}

void editor_host::release_display_resources()
{
	imgui_rendering_pass.render_targets.clear();
}

void editor_host::create_display_resources()
{
	imgui_rendering_pass.render_targets.clear();
	imgui_rendering_pass.render_targets.emplace_back(render_data::instance()->get_ui_render_target_view());
	imgui_rendering_pass.render_targets.back().clear_color = false;
}

editor_host::~editor_host()
{
}

bool editor_host::handle_message(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	return ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam) != 0;
}

void editor_host::begin_frame()
{
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
	editor_items::begin_contexts();
	frame_begun = true;
}

void editor_host::render_windows()
{
	//	ツール窓とファイルごとの編集用ウィンドウ
	editor_items::render_windows(editor_context);
}

void editor_host::render_overlay()
{
	if (!frame_begun)
		return;
	frame_begun = false;

	//	ImGui描画
	ID3D11DeviceContext* immediate_context = render_data::instance()->get_immediate_context().Get();
	imgui_rendering_pass.activate(immediate_context);

	// ビューポートの設定
	D3D11_VIEWPORT imgui_viewport{};
	imgui_viewport.TopLeftX = 0.0f;
	imgui_viewport.TopLeftY = 0.0f;
	imgui_viewport.Width = static_cast<float>(imgui_rendering_pass.get_size().x);
	imgui_viewport.Height = static_cast<float>(imgui_rendering_pass.get_size().y);
	imgui_viewport.MinDepth = 0.0f;
	imgui_viewport.MaxDepth = 1.0f;
	immediate_context->RSSetViewports(1, &imgui_viewport);

	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
	imgui_rendering_pass.deactivate(immediate_context);

	if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();
	}

	//	ImGui の描画が終わったので、表示テクスチャを手放す
	imgui_texture_keeper::instance()->release();
}

void editor_host::imgui_initialize()
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImPlot::CreateContext();
	ImGui::GetIO().Fonts->AddFontFromFileTTF("Assets/Resources/Textures/Fonts/Noto_Sans_JP/NotoSansJP-VariableFont_wght.ttf", 24.0f, nullptr, glyphRangesJapanese);
	ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
	ImGui::GetIO().ConfigViewportsNoAutoMerge = true;
	ImGui::GetIO().ConfigViewportsNoDefaultParent = true;
	ImGui::GetIO().ConfigWindowsMoveFromTitleBarOnly = true;
	ImGui_ImplWin32_Init(hwnd);
	ImGui_ImplDX11_Init(render_data::instance()->get_device().Get(), render_data::instance()->get_immediate_context().Get());
	ImGui::StyleColorsDark();

	ImGuiStyle& style = ImGui::GetStyle();
	if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		style.WindowRounding = 0.0f;
		style.Colors[ImGuiCol_WindowBg].w = 1.0f;
	}

	//	ImGui用のテクスチャキーパー生成
	imgui_texture_keeper::create_instance();

	//	アセットブラウザの開き先
	asset_browser::create_instance();

	//	ウィンドウ毎のデバッグ表示の設定
	debug_view_settings_manager::create_instance();

	//	編集用ウィンドウはエディター用コンテキストで開く
	editor_context = editor_items::generate_context();

	//	移動・削除されたファイルの編集用ウィンドウを合わせる
	asset_browser::path_listener listener;
	listener.moved = [context = editor_context](std::string_view old_path, std::string_view new_path) mutable { editor_items::move_windows(context, old_path, new_path); };
	listener.deleted = [context = editor_context](std::string_view path) mutable { editor_items::discard_windows(context, path); };
	asset_browser::instance()->register_path_listener(std::move(listener));

	//	テクスチャリソースは別窓で編集する
	texture_editor_window::register_open_handler(editor_context);

	//	元データの変換を登録
	asset_converter::register_handlers();

	//	サウンドバンクはファイルごとの窓で編集する
	sound_bank_editor_window::register_asset_handlers(editor_context);

	//	アセットブラウザの新規作成
	register_material_create_handler("Material (Surface)", material_resource::domains::surface);
	register_material_create_handler("Material (Particle)", material_resource::domains::particle);
	register_material_create_handler("Material (Deferred Decal)", material_resource::domains::deferred_decal);
	register_material_create_handler("Material (Post Process)", material_resource::domains::post_process);
	register_material_create_handler("Material (User Interface)", material_resource::domains::user_interface);

	//	シーンの新規作成
	{
		asset_browser::create_handler handler;
		handler.label = "Scene";
		handler.default_name = "NewScene";
		handler.extension = std::string(".") + std::string(scene_resource::extension);
		handler.create = [](std::string_view file_path)
		{
			auto scene = std::make_shared<scene_resource>();
			if (!scene->set_contents({}))
				return false;
			return resource_manager::instance()->save(scene, file_path);
		};
		asset_browser::instance()->register_create_handler(std::move(handler));
	}

	//	プレハブの新規作成
	{
		asset_browser::create_handler handler;
		handler.label = "Prefab";
		handler.default_name = "NewPrefab";
		handler.extension = std::string(".") + std::string(prefab_resource::extension);
		handler.create = [](std::string_view file_path)
		{
			if (!game_object_registry::is_valid())
				return false;
			game_object_ptr root = game_object_registry::instance()->create(spatial_object::type_name());
			if (!root)
				return false;
			root->set_name(std::filesystem::path(std::string(file_path)).stem().string());

			auto prefab = std::make_shared<prefab_resource>();
			if (!prefab->set_contents({ root }))
				return false;
			return resource_manager::instance()->save(prefab, file_path);
		};
		asset_browser::instance()->register_create_handler(std::move(handler));
	}

	//	シーンセットの新規作成
	{
		asset_browser::create_handler handler;
		handler.label = "Scene Set";
		handler.default_name = "NewSceneSet";
		handler.extension = std::string(".") + std::string(scene_set_resource::extension);
		handler.create = [](std::string_view file_path)
		{
			return resource_manager::instance()->save(std::make_shared<scene_set_resource>(), file_path);
		};
		asset_browser::instance()->register_create_handler(std::move(handler));
	}

	//	ツール窓
	editor_items::register_tool_window(editor_context, std::make_unique<editor_items::function_window>("Asset Browser", [](bool& open) { asset_browser::instance()->imgui_render(open); }), asset_browser_window_variable);
	editor_items::register_tool_window(editor_context, std::make_unique<log_window>(), log_window_variable);
	editor_items::register_tool_window(editor_context, std::make_unique<console_window>(console_window_variable), console_window_variable);
	editor_items::register_tool_window(editor_context, std::make_unique<console_commands_window>(), console_commands_window_variable);
	editor_items::register_tool_window(editor_context, std::make_unique<engine_config_window>(), engine_config_window_variable);
	editor_items::register_tool_window(editor_context, std::make_unique<editor_items::function_window>("detail_profiler",
		[](bool& open)
		{
			if (detail_profiler::is_valid())
				detail_profiler::instance()->imgui_render(open);
		}), profiler_window_variable);
	editor_items::register_tool_window(editor_context, std::make_unique<model_buffer_pool_window>(), model_buffer_pool_window_variable);
	editor_items::register_tool_window(editor_context, std::make_unique<memory_allocator_window>(), memory_allocator_window_variable);
	editor_items::register_tool_window(editor_context, std::make_unique<gamepad_window>(), gamepad_window_variable);
}

void editor_host::register_asset_open_handlers(scene_function edit_scene, play_function play_scene)
{
	register_asset_open_handler(play_scene, "Timeline Editor", { ".etl" }, scene_paths::timeline_editor);

	//	シーンを開く
	{
		asset_browser::open_handler handler;
		handler.label = "Open Scene";
		handler.extensions = { ".escene" };
		handler.open_file = [edit_scene](const cache_string& file_path)
		{
			edit_scene(file_path.view());
		};
		asset_browser::instance()->register_open_handler(std::move(handler));
	}

	//	シーンセットは中のシーンを全部開く
	{
		asset_browser::open_handler handler;
		handler.label = "Open Scene Set";
		handler.extensions = { std::string(".") + std::string(scene_set_resource::extension) };
		handler.open_file = [edit_scene](const cache_string& file_path)
		{
			edit_scene(file_path.view());
		};
		asset_browser::instance()->register_open_handler(std::move(handler));
	}

	//	シーンセットの中身はファイルごとの窓で編集する
	scene_set_editor_window::register_asset_handlers(editor_context, [edit_scene](std::string_view file_path) { edit_scene(file_path); });
}

void editor_host::imgui_terminate()
{
	//	編集用ウィンドウが持つ試し再生などを先に止める
	if (editor_context)
		editor_items::close_all_windows(editor_context);
	editor_context.reset();
	asset_browser::delete_instance();
	imgui_texture_keeper::delete_instance();

	ImGui::ShutdownNeoSequencer();

	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImPlot::DestroyContext();
	ImGui::DestroyContext();

	//	終了時の imgui.ini の保存で使うので、コンテキストを破棄した後に消す
	debug_view_settings_manager::delete_instance();
}

#endif // defined(USE_IMGUI)
