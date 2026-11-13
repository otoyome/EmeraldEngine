#pragma once

#include <atomic>

#include "renderer.h"
#include "Graphics/Core/render_pass.h"
#include "scene_render_targets.h"

#include "Graphics/View/exposure_controller.h"

//	@tool 露出・トーンマッピング・ディスプレイマッピング(ライティングサンプルと同じ流れ)
//	@tool 描画は放射量で行い、露出でセンサーの飽和をディスプレイの最大輝度に対応させ、トーンマップしてディスプレイの信号にする

//	@tool SDR のディスプレイの最大輝度(基準白)[nit]。HDR10 対応時にディスプレイの情報から取る
constexpr float sdr_reference_white_nits = 100.0f;

//////////////////////////////////////////////////////////////////////////////////////////////////////////
//	露出パス
class exposure_renderer : public renderer
{
public:
	renderer_domain get_domain() const override { return renderer_domain::exposure_pass; }

public:
	exposure_renderer(render_job_registrar job_executor, std::shared_ptr<scene_render_targets> render_targets);
	~exposure_renderer() override;

	void render(renderer_context& context) override;
	void resize_buffer(int width, int height) override;

private:
	std::shared_ptr<scene_render_targets> render_targets;

	std::shared_ptr<compute_shader> average_luminance;
	std::shared_ptr<compute_shader> exposure_adjust;
	Microsoft::WRL::ComPtr<ID3D11Buffer> constant_buffer;
	Microsoft::WRL::ComPtr<ID3D11Buffer> average_luminance_buffer;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> average_luminance_view;

	//	露出制御(物理カメラの代わり)
	exposure_controller controller;
	bool initialized = false;

	//	GPU から読み戻した前のフレームの平均輝度[cd/m^2]
	std::atomic<float> measured_luminance{ 0.0f };
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////
//	トーンマッピングパス
class tone_mapping_renderer : public renderer
{
public:
	renderer_domain get_domain() const override { return renderer_domain::tone_mapping_pass; }

public:
	tone_mapping_renderer(render_job_registrar job_executor, std::shared_ptr<scene_render_targets> render_targets);
	~tone_mapping_renderer() override;

	void render(renderer_context& context) override;
	void resize_buffer(int width, int height) override {}

private:
	std::shared_ptr<scene_render_targets> render_targets;
	std::shared_ptr<compute_shader> tone_mapping;
	Microsoft::WRL::ComPtr<ID3D11Buffer> constant_buffer;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////
//	ディスプレイマッピングパス(バックバッファへ描き戻す)
class display_mapping_renderer : public renderer
{
public:
	renderer_domain get_domain() const override { return renderer_domain::display_mapping_pass; }

	//	出力の伝達関数(シェーダーと同じ値)
	enum class output_curves : UINT
	{
		srgb,
	};

public:
	display_mapping_renderer(render_job_registrar job_executor, std::shared_ptr<scene_render_targets> render_targets);
	~display_mapping_renderer() override;

	void render(renderer_context& context) override;
	void resize_buffer(int width, int height) override;

private:
	std::shared_ptr<scene_render_targets> render_targets;
	D3D11_VIEWPORT viewport;

	graphics_pipeline_state_ptr display_mapping_pipeline_states;
	Microsoft::WRL::ComPtr<ID3D11Buffer> constant_buffer;
};
