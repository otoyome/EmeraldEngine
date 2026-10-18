#pragma once

#if defined(USE_IMGUI)
#include <memory>
#include <vector>

#include "imgui/imgui_impl_dx11.h"
#include "imgui/imgui_impl_win32.h"
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
extern ImWchar glyphRangesJapanese[];

#include "imtimeline/ImTimeline.h"

#include "System/engine_world.h"
#include "System/logger.h"
#include "Renderer/render_system.h"
#include "Editor/imgui_present.h"
#include "Editor/editor_window.h"

struct ImPlotContext;

//	@tool エディター機能をまとめる（ImGui・デバッグ用のウィンドウ・エディター用の別ウィンドウ）
//	@tool 全てメインスレッドから呼ぶ。ImGui の描画は描画データを複製して、提出スレッドで描く
//	@tool ImGui のコンテキストはメインとエディター用の別ウィンドウごとに持つ。メインスレッドの現在のコンテキストは、別ウィンドウの処理中以外は常にメインの物にしておく
class editor_host
{
public:
	//	@tool エディター用の別ウィンドウの種類
	enum class editor_kinds
	{
		model,		//	@tool モデル編集
		particle,	//	@tool パーティクル（エフェクト）編集
	};

public:
	editor_host(HWND hwnd, render_system& renderer);
	~editor_host();

	editor_host(const editor_host&) = delete;
	editor_host& operator=(const editor_host&) = delete;
	editor_host(editor_host&&) noexcept = delete;
	editor_host& operator=(editor_host&&) noexcept = delete;

	//	@tool ImGui の初期化・終了（描画スレッドを開始する前・停止した後に呼ぶ。終了ではエディター用の別ウィンドウも破棄する）
	void imgui_initialize();
	void imgui_terminate();

	//	@tool メインのウィンドウのメッセージを ImGui へ渡す（ImGui が処理したら true）
	static bool handle_message(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

	//	@tool エディター用の別ウィンドウを開く（次の更新の最初に開く）
	void request_open(editor_kinds kind);
	//	@tool 種類の名前（model・particle）で開く（コンソールコマンド editor.open から呼ぶ）
	static bool request_open_by_name(std::string_view kind_name);
	//	@tool エディター用の別ウィンドウの生成・破棄・描画先の作り直し（更新の最初に行う）
	void update_editor_windows();
	//	@tool 各ワールドの画面が入力を受け付けるかを決める（ImGui の判定は前のフレームの物を使う）
	void update_world_viewports();
	//	@tool ImGui のフレームを始めて、ウィンドウを組み立てる（処理中のワールドはゲームのワールド）
	void build_windows();

	//	@tool エディター用のワールドで順に処理する
	template<class Function>
	void for_each_world(Function& function)
	{
		for (const auto& window : editor_windows)
			window->with_world(function);
	}

	//	@tool エディター用のワールドの視点を抽出中のフレームに足す
	void add_views(render_system& renderer);
	//	@tool ImGui のフレームを締め、提出スレッドでバックバッファへ重ねて描く処理を返す
	//	@tool 別ウィンドウの生成・リサイズ・破棄はここ（メインスレッド）で行い、描画と表示は提出スレッドで行う
	render_system::overlay_function end_frame();

private:
	//	@tool メインの ImGui の別ウィンドウ（ビューポート）の表示先の生成・破棄・リサイズ（DX11 バックエンドの物と置き換える）
	static void imgui_create_viewport_target(ImGuiViewport* viewport);
	static void imgui_destroy_viewport_target(ImGuiViewport* viewport);
	static void imgui_resize_viewport_target(ImGuiViewport* viewport, ImVec2 size);

	//	@tool エディター用の別ウィンドウを作る
	std::unique_ptr<editor_window> create_editor_window(editor_kinds kind);

	void imgui_engine_config_window();
	void imgui_log_window();
	void imgui_profiler_window();
	void imgui_model_buffer_pool_window();
	void imgui_memory_allocator_window();
	void imgui_textures_window();
	void imgui_scene_change();

	void imgui_console_window();
	void imgui_console_commands_window();

	//	@tool 視点ごとのデバッグ表示を積む（抽出の途中で render_system から呼ばれる）
	static void build_debug_primitives(const view_render_data& render_data);

private:
	CONST HWND hwnd;
	render_system& renderer;

	//	@tool メインの ImGui・ImPlot のコンテキスト（メッセージの処理と提出スレッドの描画で使う）
	static inline ImGuiContext* main_imgui_context = nullptr;
	//	@tool コンソールコマンドから開く際の対象
	static inline editor_host* active_host = nullptr;
	ImPlotContext* main_plot_context = nullptr;

	//	@tool エディター用の別ウィンドウ
	std::vector<std::unique_ptr<editor_window>> editor_windows;
	//	@tool 開く要求（次の更新の最初に開く）
	std::vector<editor_kinds> open_requests;
	//	@tool 開いた数（ワールド・視点の名前を重ならないようにする）
	uint32_t opened_count = 0;

	//	ImGui用
	D3D11_VIEWPORT imgui_viewport;

	//	ログ系
	logger::categorys display_log_category = logger::categorys::max;
	UINT display_log_flags = (1 << static_cast<UINT>(logger::levels::error)) | (1 << static_cast<UINT>(logger::levels::warning));
	bool is_log_descending_order = false;
};
#endif	//	defined(USE_IMGUI)
