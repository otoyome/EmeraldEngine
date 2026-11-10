#include "Graphics/render_pass.h"

#include "Graphics/render_data.h"

namespace
{
	math::uint2 get_resource_size(ID3D11Resource* resource)
	{
		Microsoft::WRL::ComPtr<ID3D11Texture2D> texture2d;
		if (!resource || FAILED(resource->QueryInterface(IID_PPV_ARGS(texture2d.GetAddressOf()))))
			return { 0, 0 };
		D3D11_TEXTURE2D_DESC desc{};
		texture2d->GetDesc(&desc);
		return { desc.Width, desc.Height };
	}
}

std::atomic<std::uint64_t> render_target_view::next_id{ 1 };

render_target_view::render_target_view(Microsoft::WRL::ComPtr<IDXGISwapChain> swap_chain)
: id(next_id.fetch_add(1))
{
	HRESULT hr;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> back_buffer{};
	hr = swap_chain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<LPVOID*>(back_buffer.GetAddressOf()));
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	Microsoft::WRL::ComPtr<ID3D11Device> device;
	hr = swap_chain->GetDevice(IID_PPV_ARGS(device.GetAddressOf()));
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	hr = device->CreateRenderTargetView(back_buffer.Get(), nullptr, view.GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	size = get_resource_size(back_buffer.Get());
}

render_target_view::render_target_view(texture_ptr& texture, D3D11_RENDER_TARGET_VIEW_DESC* desc)
: id(next_id.fetch_add(1))
{
	HRESULT hr;
	Microsoft::WRL::ComPtr<ID3D11Resource>	resource;
	texture->get_shader_resource_view()->GetResource(resource.GetAddressOf());

	hr = render_data::instance()->get_device()->CreateRenderTargetView(resource.Get(), desc, view.GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	size = get_resource_size(resource.Get());
}

depth_stencil_view::depth_stencil_view(texture_ptr& texture, D3D11_DEPTH_STENCIL_VIEW_DESC* desc)
{
	HRESULT hr;
	Microsoft::WRL::ComPtr<ID3D11Resource>	resource;
	texture->get_shader_resource_view()->GetResource(resource.GetAddressOf());

	hr = render_data::instance()->get_device()->CreateDepthStencilView(resource.Get(), desc, view.GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
}

render_pass::render_pass()
: render_targets()
, depth_stencil()
{
}

render_pass::~render_pass()
{
}

void render_pass::activate(ID3D11DeviceContext* context) const
{
	ID3D11RenderTargetView*	rtvs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
	ID3D11DepthStencilView* dsv = depth_stencil.view.Get();
	int count = 0;
	for (auto& rt : render_targets)
	{
		rtvs[count++] = rt.view.Get();
		if (rt.clear_color)
		{
			context->ClearRenderTargetView(rtvs[count - 1], rt.clear_color_value);
		}
	}
	if(dsv && (depth_stencil.clear_depth || depth_stencil.clear_stencil))
	{
		UINT flag = 0;
		if (depth_stencil.clear_depth)
			flag |= D3D11_CLEAR_DEPTH;
		if (depth_stencil.clear_stencil)
			flag |= D3D11_CLEAR_STENCIL;

		context->ClearDepthStencilView(dsv, flag, depth_stencil.clear_depth_value, depth_stencil.clear_stencil_value);
	}
	context->OMSetRenderTargets(count, rtvs, dsv);
}

void render_pass::deactivate(ID3D11DeviceContext* context) const
{
	ID3D11RenderTargetView* rtvs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = {nullptr};
	ID3D11DepthStencilView* dsv = nullptr;
	context->OMSetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, rtvs, dsv);
}

math::uint2 render_pass::get_size() const
{
	if (render_targets.empty())
		return { 0, 0 };
	return render_targets.front().size;
}
