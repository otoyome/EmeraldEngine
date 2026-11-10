#include "Renderer/render_output.h"
#include "Graphics/render_data.h"
#include "Texture/texture_factory.h"

DXGI_SWAP_CHAIN_DESC render_output::get_swap_chain_desc()
{
	DXGI_SWAP_CHAIN_DESC desc{};
	render_data::instance()->get_swap_chain()->GetDesc(&desc);
	return desc;
}

std::shared_ptr<render_output> render_output::create_back_buffer()
{
	std::shared_ptr<render_output> result(new render_output(kinds::back_buffer));
	const DXGI_SWAP_CHAIN_DESC desc = get_swap_chain_desc();
	result->width = static_cast<int>(desc.BufferDesc.Width);
	result->height = static_cast<int>(desc.BufferDesc.Height);
	return result;
}

std::shared_ptr<render_output> render_output::create_texture(int width, int height)
{
	std::shared_ptr<render_output> result(new render_output(kinds::texture));
	if (!result->create_texture_core(width, height))
		return nullptr;
	return result;
}

render_output::render_output(kinds kind)
: kind(kind)
{
}

render_target_view render_output::make_render_target_view()
{
	if (kind == kinds::back_buffer)
		return render_target_view::from_back_buffer();
	return render_target_view(texture);
}

bool render_output::resize(int width, int height)
{
	if (kind == kinds::back_buffer)
	{
		const DXGI_SWAP_CHAIN_DESC desc = get_swap_chain_desc();
		width = static_cast<int>(desc.BufferDesc.Width);
		height = static_cast<int>(desc.BufferDesc.Height);
		if (width == this->width && height == this->height)
			return true;
		this->width = width;
		this->height = height;
		++generation;
		return true;
	}

	if (width == this->width && height == this->height)
		return true;
	if (!create_texture_core(width, height))
		return false;
	++generation;
	return true;
}

bool render_output::create_texture_core(int width, int height)
{
	if (width <= 0 || height <= 0)
		return false;

	D3D11_TEXTURE2D_DESC texture2d_desc{};
	texture2d_desc.Width = static_cast<UINT>(width);
	texture2d_desc.Height = static_cast<UINT>(height);
	texture2d_desc.MipLevels = 1;
	texture2d_desc.ArraySize = 1;
	texture2d_desc.Format = get_swap_chain_desc().BufferDesc.Format;
	texture2d_desc.SampleDesc.Count = 1;
	texture2d_desc.SampleDesc.Quality = 0;
	texture2d_desc.Usage = D3D11_USAGE_DEFAULT;
	texture2d_desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	texture2d_desc.CPUAccessFlags = 0;
	texture2d_desc.MiscFlags = 0;

	texture_ptr created;
	if (!texture_factory::create_texture2d(created, "render_output", texture2d_desc))
		return false;

	texture = created;
	this->width = width;
	this->height = height;
	return true;
}
