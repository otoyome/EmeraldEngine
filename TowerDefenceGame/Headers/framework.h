#pragma once

#include <vector>
#include <unordered_map>
#include <cstdint>

#if defined(USE_IMGUI)
#include "imgui/imgui_impl_dx11.h"
#include "imgui/imgui_impl_win32.h"
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
extern ImWchar glyphRangesJapanese[];

#include "imtimeline/ImTimeline.h"
#endif

#include "system/parallel_for_executor.h"
#include "Graphics/render_pass.h"

#include "Scenes/scene_base.h"
#include "Renderer/renderer.h"
#include "Renderer/scene_render_targets.h"
#include "Renderer/occlusion_culling_renderer.h"
#include "Graphics/camera.h"
#include "graphics/scene_view.h"
#include "graphics/scene_view_state.h"

namespace collision { class frustum_culling; }
struct render_data_context;
class world;
class console_command;
class world_renderer;
class framework_process;

class framework
{
	//	@tool プロセスは framework が持つ共通のサービスを使って段取りをする
	friend class boot_process;
	friend class game_process;

public:
	static constexpr const TCHAR* APPLICATION_NAME = _T("Sample Engine");

public:
	framework(HWND hwnd);
	~framework();

	framework(const framework&) = delete;
	framework& operator=(const framework&) = delete;
	framework(framework&&) noexcept = delete;
	framework& operator=(framework&&) noexcept = delete;

	int run();

	LRESULT CALLBACK handle_message(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

private:
	bool initialize();
	void update(float elapsed_time);
	void render(float elapsed_time);
	bool uninitialize();

	bool create_buffers();
	bool create_collision_filters();

private:
	//	プロセスを切り替える
	bool change_process(std::unique_ptr<framework_process> next);

	//	今のプロセスが次のプロセスを返す間、切り替えを進める
	bool advance_process();

private:
	CONST HWND hwnd;
	std::shared_ptr<render_job_executor> render_executor;	//	描画ジョブ実行者
	std::unique_ptr<world> main_world;	//	メインのワールド
	std::unique_ptr<world_renderer> main_renderer;	//	メインのワールドの描画
	std::unique_ptr<framework_process> current_process;	//	今のプロセス

#if defined(USE_IMGUI)
	std::unique_ptr<console_command> pause_command;
	std::unique_ptr<console_command> step_command;
#endif	//	defined(USE_IMGUI)

private:	//	その他
	float wheel{ 0 };
	high_resolution_timer tictoc;

#if defined(USE_IMGUI)
	//	ImGui用
	render_pass imgui_rendering_pass;
	D3D11_VIEWPORT imgui_viewport;

	//	ログ系
	logger::categorys display_log_category = logger::categorys::max;
	UINT display_log_flags = (1 << static_cast<UINT>(logger::levels::error)) | (1 << static_cast<UINT>(logger::levels::warning));
	bool is_log_descending_order = false;

	void imgui_initialize();
	void imgui_terminate();
	void imgui_engine_config_window();
	void imgui_log_window();
	void imgui_profiler_window();
	void imgui_model_buffer_pool_window();
	void imgui_memory_allocator_window();
	void imgui_scene_change();

	void imgui_console_window();
	void imgui_console_commands_window();
	void imgui_gamepad_window();
	void imgui_sound_bank_window();

	void build_debug_primitives(const render_data_context& render_data);
#endif // defined(USE_IMGUI)
};

