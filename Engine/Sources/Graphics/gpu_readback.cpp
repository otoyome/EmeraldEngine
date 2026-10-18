#include "Graphics/gpu_readback.h"

#include <algorithm>

#include "System/misc.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool gpu_readback_pool::create_slot(slot& out_slot) const
{
	out_slot.buffers.clear();
	out_slot.buffers.reserve(part_sizes.size());
	for (UINT size : part_sizes)
	{
		D3D11_BUFFER_DESC desc = {};
		desc.ByteWidth = size;
		desc.Usage = D3D11_USAGE_STAGING;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
		HRESULT hr = device->CreateBuffer(&desc, nullptr, buffer.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
		if (FAILED(hr))
			return false;
		out_slot.buffers.emplace_back(std::move(buffer));
	}
	return true;
}

gpu_readback_pool::slot* gpu_readback_pool::acquire()
{
	std::lock_guard<std::mutex> lock(mutex);
	for (auto& target : slots)
	{
		if (!target->in_use)
		{
			target->in_use = true;
			return target.get();
		}
	}

	//	@tool 全て使用中なら増やす（GPU が遅れて受け取りが追いつかない場合）
	if (slots.size() >= max_slot_count)
		return nullptr;
	auto added = std::make_unique<slot>();
	if (!create_slot(*added))
		return nullptr;
	added->in_use = true;
	slots.emplace_back(std::move(added));
	return slots.back().get();
}

void gpu_readback_pool::release(slot* target)
{
	std::lock_guard<std::mutex> lock(mutex);
	if (target)
		target->in_use = false;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool gpu_readback_buffer::initialize(ID3D11Device* device, const std::vector<UINT>& part_sizes, UINT initial_slot_count, UINT max_slot_count)
{
	auto created = std::make_shared<gpu_readback_pool>();
	created->device = device;
	created->part_sizes = part_sizes;
	created->max_slot_count = (std::max)(max_slot_count, initial_slot_count);
	for (UINT index = 0; index < initial_slot_count; ++index)
	{
		auto added = std::make_unique<gpu_readback_pool::slot>();
		if (!created->create_slot(*added))
			return false;
		created->slots.emplace_back(std::move(added));
	}
	pool = std::move(created);
	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
gpu_readback_queue& gpu_readback_queue::instance()
{
	static gpu_readback_queue queue;
	return queue;
}

void gpu_readback_queue::submit(std::vector<gpu_readback_request>&& requests)
{
	if (requests.empty())
		return;
	std::lock_guard<std::mutex> lock(mutex);
	for (auto& request : requests)
		pending.emplace_back(std::move(request));
	requests.clear();
}

void gpu_readback_queue::poll(ID3D11DeviceContext* immediate_context)
{
	std::lock_guard<std::mutex> lock(mutex);

	//	@tool 未完了の要求がある読み戻し先は、それ以降の要求も待たせる（登録順に受け取るため）
	std::vector<const gpu_readback_pool*> blocked_pools;
	for (auto it = pending.begin(); it != pending.end();)
	{
		gpu_readback_request& request = *it;
		if (std::find(blocked_pools.begin(), blocked_pools.end(), request.pool.get()) != blocked_pools.end())
		{
			++it;
			continue;
		}

		const UINT part_count = static_cast<UINT>(request.slot->buffers.size());
		std::vector<const void*> parts(part_count, nullptr);
		UINT mapped_count = 0;
		HRESULT hr = S_OK;
		for (; mapped_count < part_count; ++mapped_count)
		{
			D3D11_MAPPED_SUBRESOURCE mapped = {};
			hr = immediate_context->Map(request.slot->buffers[mapped_count].Get(), 0, D3D11_MAP_READ, D3D11_MAP_FLAG_DO_NOT_WAIT, &mapped);
			if (FAILED(hr))
				break;
			parts[mapped_count] = mapped.pData;
		}

		//	@tool 読み出せた場合だけ受け取り関数を呼ぶ
		if (mapped_count == part_count && request.receiver)
		{
			gpu_readback_data data;
			data.parts = parts.data();
			data.part_sizes = request.pool->part_sizes.data();
			data.part_count = part_count;
			request.receiver(data);
		}
		for (UINT index = 0; index < mapped_count; ++index)
			immediate_context->Unmap(request.slot->buffers[index].Get(), 0);

		if (hr == DXGI_ERROR_WAS_STILL_DRAWING)
		{
			blocked_pools.emplace_back(request.pool.get());
			++it;
			continue;
		}

		//	@tool 受け取った（または読み出しに失敗した）組は返す
		request.pool->release(request.slot);
		it = pending.erase(it);
	}
}
