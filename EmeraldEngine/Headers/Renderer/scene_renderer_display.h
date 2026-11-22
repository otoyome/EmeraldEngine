#pragma once

#include <atomic>

#include "renderer.h"
#include "Graphics/Core/render_pass.h"
#include "scene_render_targets.h"

#include "Graphics/View/exposure_controller.h"

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
//	ディスプレイマッピングパス
class display_mapping_renderer : public renderer
{
public:
	renderer_domain get_domain() const override { return renderer_domain::display_mapping_pass; }

	//	出力の伝達関数
	enum class output_curves : UINT
	{
		srgb,	//	sRGB
		st2084,	//	BT.2100 PQ
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
};
