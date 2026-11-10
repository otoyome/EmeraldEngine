#pragma once

#if defined(USE_IMGUI)
#include <memory>

#include "imgui/imgui_impl_dx11.h"
#include "imgui/imgui_impl_win32.h"
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
extern ImWchar glyphRangesJapanese[];

#include "imtimeline/ImTimeline.h"

#include "Graphics/render_pass.h"
#include "Editor/editor_items.h"

struct render_data_context;
class scene_manager;
class world;

//	エディター機能
class editor_host
{
public:
	explicit editor_host(HWND hwnd);
	~editor_host();

	editor_host(const editor_host&) = delete;
	editor_host& operator=(const editor_host&) = delete;
	editor_host(editor_host&&) noexcept = delete;
	editor_host& operator=(editor_host&&) noexcept = delete;

	//	ImGui の初期化・終了
	void imgui_initialize();
	void imgui_terminate();

	void begin_frame();
	//	ImGui のフレームを始めたか
	bool is_frame_begun() const { return frame_begun; }
	void render_windows();
	void render_overlay();

	//	アセットブラウザの開き先を登録
	void register_asset_open_handlers(scene_manager& scenes);

	//	シーン切り替えの窓
	void imgui_scene_change(scene_manager& scenes);

	//	デバッグ表示を積む
	void build_debug_primitives(world* target, const render_data_context& render_data);

public:
	//	メインのウィンドウのメッセージを ImGui へ渡す(ImGui が処理したら true)
	static bool handle_message(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

private:
	CONST HWND hwnd;
	std::shared_ptr<editor_items::context> editor_context;

	bool frame_begun = false;
};
#endif	//	defined(USE_IMGUI)
