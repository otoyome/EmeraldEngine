#pragma once

#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

#include "system/parallel_for_executor.h"
#include "Graphics/render_pass.h"
#include "Graphics/camera.h"
#include "graphics/scene_view.h"
#include "graphics/scene_view_state.h"
#include "Renderer/renderer.h"
#include "Renderer/scene_render_targets.h"
#include "Renderer/render_output.h"

namespace collision { class frustum_culling; }
struct render_data_context;
class world;

//	ワールドレンダラー
class world_renderer
{
public:
	using debug_builder = std::function<void(const render_data_context&)>;
	
public:
	const std::shared_ptr<render_output>& get_output() const { return output; }

	void set_debug_builder(debug_builder builder) { this->builder = std::move(builder); }

public:
	world_renderer(std::shared_ptr<render_job_executor> render_executor, std::shared_ptr<render_output> output);
	~world_renderer();

	world_renderer(const world_renderer&) = delete;
	world_renderer& operator=(const world_renderer&) = delete;
	world_renderer(world_renderer&&) noexcept = delete;
	world_renderer& operator=(world_renderer&&) noexcept = delete;

	//	初期化
	bool initialize();

	//	ワールドから描画情報収集
	void collect(world& target);

	//	集めた情報で描画ジョブを積む
	void submit_render_jobs(world& target, float elapsed_time);

private:
	//	出力先の大きさで描画先一式を作る
	bool create_targets(int scene_width, int scene_height);
	void create_renderers();

	//	出力先が作り直されていたら描画先一式を合わせる
	bool sync_output();

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
	std::shared_ptr<render_job_executor> render_executor;
	std::shared_ptr<render_output> output;
	uint32_t output_generation = 0;

	std::shared_ptr<scene_render_targets> render_targets;
	std::vector<std::unique_ptr<renderer>> scene_renderers;
	std::vector<std::unique_ptr<renderer>> ui_renderers;
	debug_builder builder;

	//	出力先のクリア用
	render_pass output_clear_rendering_pass;

	//	UI描画用のシーン定数バッファ(viewport_rectのみ使用)
	Microsoft::WRL::ComPtr<ID3D11Buffer> ui_scene_constant_buffer;

private:
	//	視点情報
	view_selector selector;
	view_parameters current_view_parameters;
	bool camera_cut = false;
	Microsoft::WRL::ComPtr<ID3D11Buffer> scene_constant_buffer;

	scene_view_state_cache view_states;
	uint64_t render_frame_index = 0;
	float total_time = 0.0f;

	//	描画情報
	std::shared_ptr<render_data_context> render_data;
	std::shared_ptr<render_particle_indices> particle_indices;

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
};
