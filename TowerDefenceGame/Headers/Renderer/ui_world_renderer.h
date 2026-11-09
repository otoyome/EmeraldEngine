#pragma once

#include <memory>
#include <vector>

#include "Graphics/render_pass.h"
#include "Renderer/renderer.h"
#include "Renderer/scene_render_targets.h"
#include "Renderer/render_output.h"

struct render_data_context;
struct sprite_container;
class world;

//	@tool UI だけを描くワールドの描画(読み込み画面など。3D の描画先とシェーダーを用意しないで済む)
class ui_world_renderer
{
public:
	const std::shared_ptr<render_output>& get_output() const { return output; }

public:
	ui_world_renderer(std::shared_ptr<render_job_executor> render_executor, std::shared_ptr<render_output> output);
	~ui_world_renderer();

	ui_world_renderer(const ui_world_renderer&) = delete;
	ui_world_renderer& operator=(const ui_world_renderer&) = delete;
	ui_world_renderer(ui_world_renderer&&) noexcept = delete;
	ui_world_renderer& operator=(ui_world_renderer&&) noexcept = delete;

	//	@tool 初期化
	bool initialize();

	//	@tool ワールドから描画情報収集
	void collect(world& target);

	//	@tool 集めた情報で描画ジョブを積む
	void submit_render_jobs(world& target, float elapsed_time);

public:
	//	@tool ワールドの UI オブジェクトのスプライトを描画順に集める(world_renderer と共通)
	static void collect_sprites(world& target, sprite_container& container);

private:
	//	@tool 出力先の大きさで描画先一式を作る
	bool create_targets(int width, int height);

	//	@tool 出力先が作り直されていたら描画先一式を合わせる
	bool sync_output();

private:
	std::shared_ptr<render_job_executor> render_executor;
	std::shared_ptr<render_output> output;
	uint32_t output_generation = 0;

	std::shared_ptr<scene_render_targets> render_targets;
	std::vector<std::unique_ptr<renderer>> ui_renderers;

	//	@tool 出力先のクリア用
	render_pass output_clear_rendering_pass;

	//	@tool UI描画用のシーン定数バッファ(viewport_rectのみ使用)
	Microsoft::WRL::ComPtr<ID3D11Buffer> ui_scene_constant_buffer;

	//	@tool 描画情報
	std::shared_ptr<render_data_context> render_data;
};
