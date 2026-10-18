#pragma once

#include "texture.h"
#include "common_pipeline_states.h"

#include <array>
#include <vector>

//	@tool 描画パスの色の描画先（ビューと、開始時にクリアするか）
struct render_pass_color_target
{
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> view;
	bool clear = false;
	std::array<FLOAT, 4> clear_value{ 0.0f, 0.0f, 0.0f, 0.0f };
};

//	@tool 描画パスの深度・ステンシルの描画先（ビューと、開始時にクリアするか）
struct render_pass_depth_target
{
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView> view;
	bool clear_depth = false;
	bool clear_stencil = false;
	FLOAT clear_depth_value = common_pipeline_states::depth_clear_value;
	UINT8 clear_stencil_value = 0;
};

//	@tool 描画パスを開始する時の描画先（Vulkan の VkRenderPassBeginInfo・D3D12 の BeginRenderPass に渡す情報に相当するデータ）
//	@tool フレームグラフがパスごとに用意し、記録器の begin_render_pass / end_render_pass でパスの前後に設定する
struct render_pass_begin_info
{
	std::vector<render_pass_color_target> color_targets;
	render_pass_depth_target depth_target;
};
