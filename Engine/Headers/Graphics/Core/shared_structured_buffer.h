#pragma once

#include <d3d11.h>
#include <wrl.h>

#include <algorithm>
#include <span>

#include "Core/shared_block_allocator.h"
#include "Core/logger.h"

//	共有 StructuredBuffer
template<class Element>
class shared_structured_buffer
{
public:
	using handle = typename core::shared_block_allocator<Element>::handle;

public:
	//	シェーダーリソースビュー取得
	ID3D11ShaderResourceView* get_shader_resource_view() const { return shader_resource_view.Get(); }
	ID3D11ShaderResourceView* const* get_address_of_shader_resource_view() const { return shader_resource_view.GetAddressOf(); }

	//	容量取得
	UINT get_capacity() const { return allocator.get_capacity(); }

	//	最大容量取得
	UINT get_max_capacity() const { return max_capacity; }

	//	使用量取得
	UINT get_used_size() const { return allocator.get_used_size(); }

	//	ブロック数取得
	size_t get_block_count() const { return allocator.get_block_count(); }

public:
	//	初期化
	bool initialize(ID3D11Device* device, UINT initial_capacity, UINT max_capacity)
	{
		this->max_capacity = std::max(initial_capacity, max_capacity);
		if (!allocator.initialize(initial_capacity))
			return false;
		return create_buffer(device, initial_capacity);
	}

	//	ブロック取得
	handle acquire(std::span<const Element> elements)
	{
		handle result = allocator.acquire(elements);
		while (!result.is_valid() && !elements.empty())
		{
			//	容量が足りなければ倍に拡張
			const UINT64 capacity = allocator.get_capacity();
			const UINT64 required = capacity + elements.size();
			const UINT64 new_capacity = std::min<UINT64>(std::max<UINT64>(capacity * 2, required), max_capacity);
			if (new_capacity <= capacity || !allocator.grow(static_cast<UINT>(new_capacity - capacity)))
				break;

			result = allocator.acquire(elements);
		}
		//	拡張出来なければ無効
		return result;
	}

	//	ブロック解放
	void release(const handle& target)
	{
		allocator.release(target);
	}

	//	GPU 側にアップロード
	void upload_buffer(ID3D11DeviceContext* device_context)
	{
		if (buffer_capacity < allocator.get_capacity())
			resize_buffer(device_context);

		allocator.consume_pending([&](UINT head, std::span<const Element> elements)
		{
			if (head + elements.size() > buffer_capacity)
				return;

			D3D11_BOX write_box = {};
			write_box.left = static_cast<UINT>(sizeof(Element) * head);
			write_box.right = static_cast<UINT>(write_box.left + elements.size_bytes());
			write_box.top = 0;
			write_box.bottom = 1;
			write_box.front = 0;
			write_box.back = 1;
			device_context->UpdateSubresource(buffer.Get(), 0, &write_box, elements.data(), static_cast<UINT>(elements.size_bytes()), 0);
		});
	}

private:
	//	バッファ作成
	bool create_buffer(ID3D11Device* device, UINT capacity)
	{
		D3D11_BUFFER_DESC desc = {};
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		desc.ByteWidth = static_cast<UINT>(sizeof(Element) * capacity);
		desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		desc.StructureByteStride = sizeof(Element);
		desc.Usage = D3D11_USAGE_DEFAULT;

		Microsoft::WRL::ComPtr<ID3D11Buffer> new_buffer;
		HRESULT hr = device->CreateBuffer(&desc, nullptr, new_buffer.GetAddressOf());
		if (FAILED(hr))
			return false;

		D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
		srv_desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		srv_desc.Format = DXGI_FORMAT_UNKNOWN;
		srv_desc.Buffer.FirstElement = 0;
		srv_desc.Buffer.NumElements = capacity;

		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> new_shader_resource_view;
		hr = device->CreateShaderResourceView(new_buffer.Get(), &srv_desc, new_shader_resource_view.GetAddressOf());
		if (FAILED(hr))
			return false;

		buffer = new_buffer;
		shader_resource_view = new_shader_resource_view;
		buffer_capacity = capacity;
		return true;
	}

	//	リサイズ処理
	void resize_buffer(ID3D11DeviceContext* device_context)
	{
		Microsoft::WRL::ComPtr<ID3D11Device> device;
		device_context->GetDevice(device.GetAddressOf());

		Microsoft::WRL::ComPtr<ID3D11Buffer> old_buffer = buffer;
		const UINT old_capacity = buffer_capacity;
		const UINT new_capacity = allocator.get_capacity();
		if (!create_buffer(device.Get(), new_capacity))
		{
			DEBUG_WARNING_CATEGORY(core::logger::categorys::system, "shared_structured_buffer: resize failed (%u -> %u elements)", old_capacity, new_capacity);
			return;
		}

		if (old_buffer && old_capacity > 0)
		{
			D3D11_BOX copy_box = {};
			copy_box.left = 0;
			copy_box.right = static_cast<UINT>(sizeof(Element) * old_capacity);
			copy_box.top = 0;
			copy_box.bottom = 1;
			copy_box.front = 0;
			copy_box.back = 1;
			device_context->CopySubresourceRegion(buffer.Get(), 0, 0, 0, 0, old_buffer.Get(), 0, &copy_box);
		}
	}

private:
	core::shared_block_allocator<Element> allocator;
	Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shader_resource_view;
	UINT buffer_capacity = 0;	//	GPU 側の容量(要素数)
	UINT max_capacity = 0;	//	拡張の上限(要素数)
};
