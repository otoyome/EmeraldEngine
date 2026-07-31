#pragma once

#include "renderer.h"
#include "render_pass.h"
#include "scene_render_targets.h"
#include "geometric_primitive.h"

#include "collision/frustum_culling.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////////
//	フォワードレンダリングベース
class forward_renderer_base : public renderer
{
public:
	virtual material::translucency_paths get_material_translucency_path() const = 0;

public:
	forward_renderer_base(render_job_registrar job_executor, std::shared_ptr<scene_render_targets> render_targets);
	~forward_renderer_base() override;

	void render(renderer_context& context) override;

	void resize_buffer(int width, int height) override;

protected:
	std::shared_ptr<scene_render_targets> render_targets;
	render_pass rendering_pass;
	D3D11_VIEWPORT viewport;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////
//	被写界深度前フォワードレンダリング
class before_dof_forward_renderer : public renderer
{
public:
	renderer_domain get_domain() const override { return renderer_domain::before_dof_translucency_pass; }

public:
	before_dof_forward_renderer(render_job_registrar job_executor, std::shared_ptr<scene_render_targets> render_targets);
	~before_dof_forward_renderer() override;

	void render(renderer_context& context) override;

	void resize_buffer(int width, int height) override;

protected:
	std::shared_ptr<scene_render_targets> render_targets;
	render_pass rendering_pass;
	D3D11_VIEWPORT viewport;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////
//	被写界深度後フォワードレンダリング
class after_dof_forward_renderer : public renderer
{
public:
	renderer_domain get_domain() const override { return renderer_domain::after_dof_translucency_pass; }

public:
	after_dof_forward_renderer(render_job_registrar job_executor, std::shared_ptr<scene_render_targets> render_targets);
	~after_dof_forward_renderer() override;

	void render(renderer_context& context) override;

	void resize_buffer(int width, int height) override;

protected:
	std::shared_ptr<scene_render_targets> render_targets;
	render_pass rendering_pass;
	D3D11_VIEWPORT viewport;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////
//	モーションブラー後フォワードレンダリング
class after_motion_blur_forward_renderer : public renderer
{
public:
	renderer_domain get_domain() const override { return renderer_domain::after_motion_blur_forward_pass; }

public:
	after_motion_blur_forward_renderer(render_job_registrar job_executor, std::shared_ptr<scene_render_targets> render_targets);
	~after_motion_blur_forward_renderer() override;

	void render(renderer_context& context) override;

	void resize_buffer(int width, int height) override;

protected:
	std::shared_ptr<scene_render_targets> render_targets;
	render_pass rendering_pass;
	D3D11_VIEWPORT viewport;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////
//	エフェクトレンダリング
//	未対応
//	方針：パーティクルの描画はモデルの描画と同じように行う。
//		ただし、αの書き込みなどは何とも言えない
#include "graphics/compute_particle_system.h"
class particle_effect_renderer : public renderer
{
public:
	renderer_domain get_domain() const override { return renderer_domain::effect_render_pass; }

public:
	particle_effect_renderer(render_job_registrar job_executor, std::shared_ptr<scene_render_targets> render_targets);
	~particle_effect_renderer() override;

	void render(renderer_context& renderer_context) override;

	void resize_buffer(int width, int height) override;

private:
	std::shared_ptr<scene_render_targets> render_targets;
	render_pass rendering_pass;
	D3D11_VIEWPORT viewport;

	std::unique_ptr<compute_particle_system> particle_system;

	enum particle_index
	{
		Bomb = 1,
		Trail = 0,

		Max = 2,
	};

#define	TRAIL_BODY_MAX 60
	struct emitter_data
	{
		UINT emitter_index;	// エミッター管理番号
		DirectX::XMFLOAT3 scale = { 1, 1, 1 }; // 拡縮情報
		DirectX::XMFLOAT3 rotation = { 0, 0, 0 }; // 回転情報
		DirectX::XMFLOAT3 translation = { 0, 0, 0 }; // 移動情報
	};
	emitter_data emitter_datas[particle_index::Max];
};
