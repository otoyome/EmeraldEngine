#include "Graphics/render_pass.h"

#include "Graphics/render_data.h"

render_target_view render_target_view::from_back_buffer()
{
	render_target_view result;
	result.back_buffer = true;
	return result;
}

ID3D11RenderTargetView* render_target_view::get_view() const
{
	if (back_buffer)
		return render_data::instance()->get_back_buffer_view();
	return view.Get();
}

render_target_view::render_target_view(texture_ptr& texture, D3D11_RENDER_TARGET_VIEW_DESC* desc)
{
	HRESULT hr;
	Microsoft::WRL::ComPtr<ID3D11Resource>	resource;
	texture->get_shader_resource_view()->GetResource(resource.GetAddressOf());

	hr = render_data::instance()->get_device()->CreateRenderTargetView(resource.Get(), desc, view.GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
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

void render_pass::activate(ID3D11DeviceContext* context)
{
	ID3D11RenderTargetView*	rtvs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
	ID3D11DepthStencilView* dsv = depth_stencil.view.Get();
	int count = 0;
	for (auto& rt : render_targets)
	{
		rtvs[count++] = rt.get_view();
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

void render_pass::deactivate(ID3D11DeviceContext* context)
{
	ID3D11RenderTargetView* rtvs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = {nullptr};
	ID3D11DepthStencilView* dsv = nullptr;
	context->OMSetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, rtvs, dsv);
}