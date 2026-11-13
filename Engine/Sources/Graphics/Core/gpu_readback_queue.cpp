#include "Graphics/Core/gpu_readback_queue.h"

#include <cstdint>

#include "Graphics/Core/render_data.h"

gpu_readback_queue::staging_buffer gpu_readback_queue::acquire_staging(UINT size_in_bytes)
{
	{
		std::lock_guard<std::mutex> lock(mutex);
		for (auto it = free_stagings.begin(); it != free_stagings.end(); ++it)
		{
			if (it->capacity_in_bytes < size_in_bytes)
				continue;

			staging_buffer staging = *it;
			free_stagings.erase(it);
			return staging;
		}
	}

	UINT capacity = 256;
	while (capacity < size_in_bytes)
		capacity *= 2;

	D3D11_BUFFER_DESC desc{};
	desc.ByteWidth = capacity;
	desc.Usage = D3D11_USAGE_STAGING;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

	staging_buffer staging;
	HRESULT hr = render_data::instance()->get_device()->CreateBuffer(&desc, nullptr, staging.buffer.GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	if (FAILED(hr))
		return staging_buffer();
	staging.capacity_in_bytes = capacity;
	return staging;
}

void gpu_readback_queue::read_back(ID3D11DeviceContext* device_context, ID3D11Buffer* source, UINT size_in_bytes, receive_function receive)
{
	if (!device_context || !source || size_in_bytes == 0 || !receive)
		return;

	staging_buffer staging = acquire_staging(size_in_bytes);
	if (!staging.buffer)
		return;

	D3D11_BOX box{};
	box.left = 0;
	box.right = size_in_bytes;
	box.top = 0;
	box.bottom = 1;
	box.front = 0;
	box.back = 1;
	device_context->CopySubresourceRegion(staging.buffer.Get(), 0, 0, 0, 0, source, 0, &box);

	request req;
	req.staging = staging;
	req.size_in_bytes = size_in_bytes;
	req.receive = std::move(receive);

	std::lock_guard<std::mutex> lock(mutex);
	recorded_requests.emplace_back(std::move(req));
}

void gpu_readback_queue::mark_submitted()
{
	std::lock_guard<std::mutex> lock(mutex);
	for (auto& req : recorded_requests)
	{
		submitted_requests.emplace_back(std::move(req));
	}
	recorded_requests.clear();
}

void gpu_readback_queue::poll(ID3D11DeviceContext* immediate_context)
{
	struct completed_request
	{
		receive_function receive;
		std::vector<uint8_t> data;
	};

	std::vector<completed_request> completed;
	{
		std::lock_guard<std::mutex> lock(mutex);
		for (size_t index = 0; index < submitted_requests.size();)
		{
			auto& req = submitted_requests[index];

			D3D11_MAPPED_SUBRESOURCE mapped{};
			if (FAILED(immediate_context->Map(req.staging.buffer.Get(), 0, D3D11_MAP_READ, D3D11_MAP_FLAG_DO_NOT_WAIT, &mapped)))
			{
				++index;
				continue;
			}

			auto& result = completed.emplace_back();
			result.receive = std::move(req.receive);
			result.data.assign(static_cast<const uint8_t*>(mapped.pData), static_cast<const uint8_t*>(mapped.pData) + req.size_in_bytes);
			immediate_context->Unmap(req.staging.buffer.Get(), 0);

			free_stagings.emplace_back(req.staging);
			submitted_requests.erase(submitted_requests.begin() + index);
		}
	}

	for (auto& result : completed)
	{
		result.receive(result.data.data(), static_cast<UINT>(result.data.size()));
	}
}
