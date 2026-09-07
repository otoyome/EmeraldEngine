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

#include "System/cpu_timer.h"
#endif

#include "render_pass.h"

#include "Scenes/scene_base.h"
#include "Renderer/renderer.h"
#include "Renderer/scene_render_targets.h"
#include "Renderer/occlusion_culling_system.h"
#include "Renderer/occlusion_culling_renderer.h"
#include "camera.h"
#include "parallel_for_executor.h"

namespace collision { class frustum_culling; }
struct render_data_context;

class framework
{
public:
	static constexpr WCHAR* APPLICATION_NAME = L"Sample Engine";

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

	void calculate_frame_stats();

	bool create_buffers();
	bool create_collision_filters();

	void prepare_render_context(
		renderer_context& context,
		const camera_container::camera_data& camera,
		const std::shared_ptr<render_data_context>& render_data,
		const std::shared_ptr<render_particle_indices>& particle_indices,
		float previous_time, float current_time);

	void build_render_lists(
		const render_data_context& render_data,
		const collision::frustum_culling& frustum_culling,
		const std::unordered_map<UINT, uint8_t>& occlusion_visible,
		render_mesh_indices& mesh_indices,
		render_decal_indices& decal_indices);

private:
	CONST HWND hwnd;
	std::shared_ptr<scene_render_targets> render_targets;
	std::vector<std::unique_ptr<renderer>> scene_renderers;
	std::vector<std::unique_ptr<renderer>> ui_renderers;
	std::shared_ptr<render_job_executor> render_executor;

	//	描画用の一部情報
	render_pass back_buffer_clear_rendering_pass;

	//	オクルージョンカリングシステム
	std::shared_ptr<occlusion_culling_system> occlusion_culling;

private:	//	描画情報振り分け
	struct cull_bucket
	{
		std::vector<std::size_t> opaque;
		std::vector<std::size_t> mask;
		std::vector<std::size_t> forward[render_mesh_indices::forward_size];
		void clear()
		{
			opaque.clear();
			mask.clear();
			for (auto& indices : forward)
				indices.clear();
		}
	};
	std::vector<cull_bucket> cull_buckets;
	std::vector<std::uintptr_t> sort_pipeline_keys;
	parallel_for_executor parallel_executor;

private:	//	その他
	float wheel{ 0 };
	high_resolution_timer tictoc;
#if defined(_DEBUG) || defined(USE_IMGUI)
	uint32_t frames{ 0 };
	float display_frame{ 0.0f };
	float current_time{ 0.0f };
	float previous_time{ 0.0f };
#endif	//	defined(_DEBUG) || defined(USE_IMGUI)

#if defined(USE_IMGUI)
	//	ImGui用
	render_pass imgui_rendering_pass;
	D3D11_VIEWPORT imgui_viewport;

	//	3D空間にグリッド表示用
	bool display_grid_xz{ true };
	bool display_grid_xy{ false };
	bool display_grid_yz{ false };
	int display_grid_length{ 100 };

	//	ログ系
	logger::categorys display_log_category = logger::categorys::max;
	UINT display_log_flags = (1 << static_cast<UINT>(logger::levels::error)) | (1 << static_cast<UINT>(logger::levels::warning));
	bool is_log_descending_order = false;

	//	常駐タイマー
	enum class timer
	{
		update,
		render,
		max,
	};
	struct stamp_data
	{
		bool flag = true;
		std::vector<float> datas;
	};
	cpu_timer stationed_cpu_timer;
	bool display_cpu_flags[static_cast<UINT>(timer::max)];
	stamp_data stationed_cpu_stamp_datas[static_cast<UINT>(timer::max)];
	stamp_data stationed_gpu_stamp_datas;

	//	GPUのスナップショット処理用
	bool is_gpu_snap_shot = false;
	bool is_gpu_snap_shot_inner = false;
	bool show_gpu_profiler = false;
	std::vector<gpu_time_stamp_data> gpu_time_stamp_datas;

	void imgui_initialize();
	void imgui_terminate();
	void imgui_main_window();
	void imgui_engine_config_window();
	void imgui_log_window();
	void imgui_profiler_window(float elapsed_time);
	void imgui_render_infos_window();
	void imgui_textures_window();
	void imgui_scene_change();

	void plane_line_draw();
	void gpu_snap_shot_begin();
	void gpu_snap_shot_end();
#endif // defined(USE_IMGUI)
};

