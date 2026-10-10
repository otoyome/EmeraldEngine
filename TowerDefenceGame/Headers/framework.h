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
#include "render_pass.h"

#include "Scenes/scene_base.h"
#include "Renderer/renderer.h"
#include "Renderer/scene_render_targets.h"
#include "Renderer/occlusion_culling_renderer.h"
#include "camera.h"
#include "graphics/scene_view.h"
#include "graphics/scene_view_state.h"

namespace collision { class frustum_culling; }
struct render_data_context;

class framework
{
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

	void prepare_renderer_context(
		renderer_context& context,
		const scene_view_ptr& target_view,
		const scene_view_state_ptr& target_state,
		const Microsoft::WRL::ComPtr<ID3D11Buffer>& scene_constant_buffer,
		const std::shared_ptr<render_data_context>& render_data,
		const std::shared_ptr<render_particle_indices>& particle_indices,
		float previous_time, float current_time);

	void build_render_lists(
		const render_data_context& render_data,
		scene_view_state& target_state,
		const collision::frustum_culling& frustum_culling,
		const std::unordered_map<UINT, uint8_t>& occlusion_visible,
		render_mesh_indices& mesh_indices,
		render_decal_indices& decal_indices);

	void build_particle_render_lists(
		const render_data_context& render_data,
		render_particle_indices& particle_indices);

private:
	CONST HWND hwnd;
	std::shared_ptr<scene_render_targets> render_targets;
	std::vector<std::unique_ptr<renderer>> scene_renderers;
	std::vector<std::unique_ptr<renderer>> ui_renderers;
	std::shared_ptr<render_job_executor> render_executor;

	//	描画用の一部情報
	render_pass back_buffer_clear_rendering_pass;

	//	UI描画用のシーン定数バッファ(viewport_rectのみ使用)
	Microsoft::WRL::ComPtr<ID3D11Buffer> ui_scene_constant_buffer;

private:
	//	メイン視点情報
	static constexpr scene_view_state_key main_scene_view_state_key = scene_view_state_cache::make_key("main");
	view_parameters main_view_parameters;
	bool main_view_camera_cut = false;
	Microsoft::WRL::ComPtr<ID3D11Buffer> main_scene_constant_buffer;

	scene_view_state_cache scene_view_states;
	uint64_t render_frame_index = 0;

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
	void imgui_main_window();
	void imgui_engine_config_window();
	void imgui_log_window();
	void imgui_profiler_window();
	void imgui_render_infos_window();
	void imgui_model_buffer_pool_window();
	void imgui_memory_allocator_window();
	void imgui_textures_window();
	void imgui_scene_change();

	void imgui_console_window();
	void imgui_console_commands_window();

	void build_debug_primitives(const render_data_context& render_data);
#endif // defined(USE_IMGUI)
};

