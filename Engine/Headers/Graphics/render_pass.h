#pragma once

#include <atomic>
#include <cstdint>

#include "Texture/texture.h"
#include "Graphics/common_pipeline_states.h"

struct render_target_view
{
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> view;
	bool clear_color{true};
	FLOAT clear_color_value[4]{0.0f, 0.0f, 0.0f, 0.0f};
	math::uint2 size{ 0, 0 };
	std::uint64_t id{ 0 };

	render_target_view(){};
	render_target_view(Microsoft::WRL::ComPtr<IDXGISwapChain> swap_chain);
	render_target_view(texture_ptr& texture, D3D11_RENDER_TARGET_VIEW_DESC* desc = nullptr);
	~render_target_view() {};

private:
	static std::atomic<std::uint64_t> next_id;
};

struct depth_stencil_view
{
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView> view;
	bool clear_depth{true};
	bool clear_stencil{true};
	FLOAT clear_depth_value{common_pipeline_states::depth_clear_value};
	UINT8 clear_stencil_value{0};

	depth_stencil_view(){};
	depth_stencil_view(texture_ptr& texture, D3D11_DEPTH_STENCIL_VIEW_DESC* desc = nullptr);
	~depth_stencil_view() {};
};

struct render_pass
{
	std::vector<render_target_view>	render_targets;
	depth_stencil_view depth_stencil;

	render_pass();
	~render_pass();

	void activate(ID3D11DeviceContext* context) const;
	void deactivate(ID3D11DeviceContext* context) const;
	math::uint2 get_size() const;
};
