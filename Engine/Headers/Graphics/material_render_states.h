#pragma once

//	@tool マテリアルの設定から選ぶ描画ステート（全てのパイプラインで共有する）
//	@tool 最初の instance() 呼び出しで作る。カスタムパイプラインの生成時（メインスレッド）に呼ばれる
class material_render_states
{
public:
	enum class depth_stencil_state
	{
		depth_test_and_write,
		depth_test_only,
		depth_write_only,
		depth_off,

		decal,

		max,
	};

	enum class blend_state
	{
		opaque,			//	不透明
		translucent,	//	半透明
		addative,		//	加算
		multiply,		//	乗算

		max,
	};

	enum class raster_state
	{
		solid,
		two_sided,
		wire,
		wire_two_sided,

		max,
	};

public:
	static const material_render_states& instance();

	ID3D11DepthStencilState* get(depth_stencil_state state) const { return depth_stencil_states[static_cast<int>(state)].Get(); }
	ID3D11BlendState* get(blend_state state) const { return blend_states[static_cast<int>(state)].Get(); }
	ID3D11RasterizerState* get(raster_state state) const { return rasterizer_states[static_cast<int>(state)].Get(); }

private:
	material_render_states();

private:
	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depth_stencil_states[static_cast<int>(depth_stencil_state::max)];
	Microsoft::WRL::ComPtr<ID3D11BlendState> blend_states[static_cast<int>(blend_state::max)];
	Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterizer_states[static_cast<int>(raster_state::max)];
};
