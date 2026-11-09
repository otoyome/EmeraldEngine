#include "Editor/editor_host.h"

#if defined(USE_IMGUI)
#include "Graphics/render_data.h"
#include "Graphics/debug_primitive_collector.h"
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
#include "Editor/Windows/debug_windows.h"
#include "Editor/Windows/console_windows.h"
#include "Editor/Windows/gamepad_window.h"
#include "Editor/Windows/sound_bank_editor_window.h"
#include "Editor/Windows/texture_editor_window.h"
#include "Object/world.h"
#include "Editor/editor_camera.h"
#include "System/console_manager.h"
#include "System/asset_browser.h"
#include "System/imgui_texture_keeper.h"
#include "Resource/resource_manager.h"
#include "Material/material_resource.h"


#include "scenes/editscene/edit_model_scene.h"
#include "scenes/editscene/edit_particle_scene.h"
#include "scenes/editscene/edit_material_scene.h"
#include "scenes/editscene/edit_timeline_scene.h"
#include "scenes/DummyScene/dummy_scene.h"
#include "scenes/TestScene/collision_test_scene.h"
#include "scenes/TestScene/clustered_lighting_test_scene.h"
#include "scenes/TestScene/light_shadow_test_scene.h"
#include "scenes/NetworkScene/network_sandbox_scene.h"
#include "Scenes/setup_scene.h"

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

	template<typename Type>
	void change_scene(scene_manager& scenes)
	{
		if (ImGui::Button(Type::type_name().data()))
		{
			scenes.change_scene_for_imgui<Type>();
		}
	}

	//	アセットブラウザでダブルクリックしたファイルを、シーンを切り替えて開く
	template<typename Type>
	void register_asset_open_handler(scene_manager& scenes, std::string_view label, std::vector<cache_string> extensions)
	{
		asset_browser::open_handler handler;
		handler.name = Type::type_name();
		handler.label = label;
		handler.extensions = std::move(extensions);
		handler.activate = [&scenes]()
		{
			if (!scenes.valid_scene<Type>())
				scenes.change_scene_for_imgui<Type>();
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

	//	デバッグ表示の設定
	console_variable<bool> grid_xz_variable("debug.grid.xz", true, "XZ平面のグリッド表示", console_flags::persist);
	console_variable<bool> grid_xy_variable("debug.grid.xy", false, "XY平面のグリッド表示", console_flags::persist);
	console_variable<bool> grid_yz_variable("debug.grid.yz", false, "YZ平面のグリッド表示", console_flags::persist);
	console_variable<int> grid_length_variable("debug.grid.length", 100, "グリッドの半径（メートル）", console_flags::persist);
	console_variable<bool> draw_mesh_aabb_variable("debug.draw.mesh_aabb", false, "モデルAABBの表示", console_flags::persist);
	console_variable<bool> draw_collision_variable("debug.draw.collision", false, "衝突判定の表示", console_flags::persist);

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

void editor_host::imgui_initialize()
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImPlot::CreateContext();
	ImGui::GetIO().Fonts->AddFontFromFileTTF("Assets/Resources/Textures/Fonts/Noto_Sans_JP/NotoSansJP-VariableFont_wght.ttf", 24.0f, nullptr, glyphRangesJapanese);
	ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
	ImGui_ImplWin32_Init(hwnd);
	ImGui_ImplDX11_Init(render_data::instance()->get_device().Get(), render_data::instance()->get_immediate_context().Get());
	ImGui::StyleColorsDark();

	ImGuiStyle& style = ImGui::GetStyle();
	if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		style.WindowRounding = 0.0f;
		style.Colors[ImGuiCol_WindowBg].w = 1.0f;
	}

	//	アセットブラウザの開き先
	imgui_texture_keeper::create_instance();
	asset_browser::create_instance();

	//	編集用ウィンドウはエディター用コンテキストで開く
	editor_context = editor_items::generate_context();

	//	移動・削除されたファイルの編集用ウィンドウを合わせる
	asset_browser::path_listener listener;
	listener.moved = [context = editor_context](std::string_view old_path, std::string_view new_path) mutable { editor_items::move_windows(context, old_path, new_path); };
	listener.deleted = [context = editor_context](std::string_view path) mutable { editor_items::discard_windows(context, path); };
	asset_browser::instance()->register_path_listener(std::move(listener));

	//	テクスチャリソースは別窓で編集する
	texture_editor_window::register_open_handler(editor_context);

	//	サウンドバンクはファイルごとの窓で編集する
	sound_bank_editor_window::register_asset_handlers(editor_context);

	//	アセットブラウザの新規作成
	register_material_create_handler("Material (Surface)", material_resource::domains::surface);
	register_material_create_handler("Material (Particle)", material_resource::domains::particle);
	register_material_create_handler("Material (Deferred Decal)", material_resource::domains::deferred_decal);
	register_material_create_handler("Material (Post Process)", material_resource::domains::post_process);
	register_material_create_handler("Material (User Interface)", material_resource::domains::user_interface);

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

void editor_host::register_asset_open_handlers(scene_manager& scenes)
{
	register_asset_open_handler<edit_model_scene>(scenes, "Model Editor", { ".emo", ".pmx", ".gltf", ".glb" });
	register_asset_open_handler<edit_particle_scene>(scenes, "Particle Editor", { ".eme" });
	register_asset_open_handler<edit_timeline_scene>(scenes, "Timeline Editor", { ".etl" });
	register_asset_open_handler<edit_material_scene>(scenes, "Material Editor", { ".emo", ".eme", ".emr" });
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
}


void editor_host::imgui_scene_change(scene_manager& scenes)
{
	if (ImGui::Begin("Scene Change Window"))
	{
		change_scene<edit_model_scene>(scenes);
		change_scene<dummy_scene>(scenes);
		change_scene<collision_test_scene>(scenes);
		change_scene<clustered_lighting_test_scene>(scenes);
		change_scene<light_shadow_test_scene>(scenes);
		change_scene<network_sandbox_scene>(scenes);
		change_scene<edit_particle_scene>(scenes);
		change_scene<edit_material_scene>(scenes);
		change_scene<edit_timeline_scene>(scenes);
		change_scene<setup_scene>(scenes);
	}
	ImGui::End();
}


void editor_host::build_debug_primitives(world* target, const render_data_context& render_data)
{
	if (!target)
		return;
	engine_context& engine = target->get_context();

	//	衝突判定のデバッグ表示
	if (draw_collision_variable.get() && target)
	{
		target->get_objects().get_world().debug_primitive_draw(engine);
	}

	//	モデルAABBのデバッグ表示
	if (draw_mesh_aabb_variable.get())
	{
		math::float4 rotation = {0.0f, 0.0f, 0.0f, 1.0f};
		math::float4 color = {1.0f, 0.0f, 0.0f, 1.0f};
		for (auto& buffer_data : render_data.mesh_container.buffer_datas)
		{
			math::float3 scale((buffer_data.bounds_max.x - buffer_data.bounds_min.x) * 0.5f, (buffer_data.bounds_max.y - buffer_data.bounds_min.y) * 0.5f, (buffer_data.bounds_max.z - buffer_data.bounds_min.z) * 0.5f);
			math::float3 center(buffer_data.bounds_min.x + scale.x, buffer_data.bounds_min.y + scale.y, buffer_data.bounds_min.z + scale.z);
			QUEUE_DEBUG_PRIMITIVE_BOX(engine, center, rotation, scale, color, true);
		}
	}

	const bool display_grid_xz = grid_xz_variable.get();
	const bool display_grid_xy = grid_xy_variable.get();
	const bool display_grid_yz = grid_yz_variable.get();
	const int display_grid_length = grid_length_variable.get();
	if (!display_grid_xz && !display_grid_xy && !display_grid_yz)
		return;

	float grid_length = system_units::convert_m_to_unit(static_cast<float>(display_grid_length));
	for (int index = -display_grid_length; index <= display_grid_length; ++index)
	{
		float len = system_units::convert_m_to_unit(static_cast<float>(index));
		if (display_grid_xz)
		{
			math::float4 line_color = math::float4(1.0f, 0.0f, 1.0f, 1);
			QUEUE_DEBUG_PRIMITIVE_LINE(engine, math::float3(len, 0.0f, -grid_length), math::float3(len, 0.0f, +grid_length), line_color, true);
			QUEUE_DEBUG_PRIMITIVE_LINE(engine, math::float3(-grid_length, 0.0f, len), math::float3(+grid_length, 0.0f, len), line_color, true);
		}
		if (display_grid_xy)
		{
			math::float4 line_color = math::float4(1.0f, 1.0f, 0.0f, 1);
			QUEUE_DEBUG_PRIMITIVE_LINE(engine, math::float3(len, -grid_length, 0.0f), math::float3(len, +grid_length, 0.0f), line_color, true);
			QUEUE_DEBUG_PRIMITIVE_LINE(engine, math::float3(-grid_length, len, 0.0f), math::float3(+grid_length, len, 0.0f), line_color, true);
		}
		if (display_grid_yz)
		{
			math::float4 line_color = math::float4(0.0f, 1.0f, 1.0f, 1);
			QUEUE_DEBUG_PRIMITIVE_LINE(engine, math::float3(0.0f, len, -grid_length), math::float3(0.0f, len, +grid_length), line_color, true);
			QUEUE_DEBUG_PRIMITIVE_LINE(engine, math::float3(0.0f, -grid_length, len), math::float3(0.0f, +grid_length, len), line_color, true);
		}
	}
}

#endif // defined(USE_IMGUI)
