#include "Graphics/immediate_constant_allocator.h"

namespace
{
	UINT align_up(UINT value, UINT alignment)
	{
		return (value + alignment - 1) & ~(alignment - 1);
	}
}

bool immediate_constant_allocator::initialize(ID3D11Device* device, bool use_offsetting, UINT chunk_size)
{
	this->device = device;
	this->use_offsetting = use_offsetting;
	this->chunk_size = align_up(chunk_size, alignment);
	chunks.clear();
	buffers.clear();
	current_chunk = 0;
	return device != nullptr;
}

void immediate_constant_allocator::begin()
{
	for (auto& target : chunks)
	{
		target.offset = 0;
		target.discarded = false;
	}
	current_chunk = 0;
}

constant_range immediate_constant_allocator::allocate(ID3D11DeviceContext* context, const void* data, UINT size)
{
	if (!context || size == 0)
		return {};

	return use_offsetting ? allocate_offset(context, data, size) : allocate_whole(context, data, size);
}

bool immediate_constant_allocator::create_buffer(UINT size, Microsoft::WRL::ComPtr<ID3D11Buffer>& out_buffer) const
{
	D3D11_BUFFER_DESC buffer_desc{};
	buffer_desc.ByteWidth = size;
	buffer_desc.Usage = D3D11_USAGE_DYNAMIC;
	buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	HRESULT hr = device->CreateBuffer(&buffer_desc, nullptr, out_buffer.ReleaseAndGetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	return SUCCEEDED(hr);
}

constant_range immediate_constant_allocator::allocate_offset(ID3D11DeviceContext* context, const void* data, UINT size)
{
	const UINT aligned_size = align_up(size, alignment);
	if (aligned_size > chunk_size)
	{
		return allocate_whole(context, data, size);
	}

	//	今のバッファに入らなければ次のバッファへ
	while (current_chunk < chunks.size() && chunks[current_chunk].offset + aligned_size > chunk_size)
	{
		++current_chunk;
	}
	if (current_chunk >= chunks.size())
	{
		chunk new_chunk;
		if (!create_buffer(chunk_size, new_chunk.buffer))
			return {};
		chunks.emplace_back(std::move(new_chunk));
		current_chunk = chunks.size() - 1;
	}

	auto& target = chunks[current_chunk];
	const D3D11_MAP map_type = target.discarded ? D3D11_MAP_WRITE_NO_OVERWRITE : D3D11_MAP_WRITE_DISCARD;

	D3D11_MAPPED_SUBRESOURCE mapped{};
	HRESULT hr = context->Map(target.buffer.Get(), 0, map_type, 0, &mapped);
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	if (FAILED(hr))
		return {};

	memcpy(static_cast<std::byte*>(mapped.pData) + target.offset, data, size);
	context->Unmap(target.buffer.Get(), 0);
	target.discarded = true;

	constant_range range;
	range.buffer = target.buffer.Get();
	range.first_constant = target.offset / 16;
	range.num_constants = aligned_size / 16;
	range.reusable = true;
	target.offset += aligned_size;
	return range;
}

constant_range immediate_constant_allocator::allocate_whole(ID3D11DeviceContext* context, const void* data, UINT size)
{
	const UINT aligned_size = align_up(size, 16);

	auto it = std::find_if(buffers.begin(), buffers.end(), [&](const auto& pair) { return pair.first == aligned_size; });
	if (it == buffers.end())
	{
		Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
		if (!create_buffer(aligned_size, buffer))
			return {};
		buffers.emplace_back(aligned_size, buffer);
		it = buffers.end() - 1;
	}

	D3D11_MAPPED_SUBRESOURCE mapped{};
	HRESULT hr = context->Map(it->second.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	if (FAILED(hr))
		return {};

	memcpy(mapped.pData, data, size);
	context->Unmap(it->second.Get(), 0);

	constant_range range;
	range.buffer = it->second.Get();
	return range;
}
