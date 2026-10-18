#include "Graphics/material_constant_buffer_pool.h"
#include "render_data.h"
#include "render_job_context.h"

material_constant_buffer_pool::material_constant_buffer_pool()
{
}

material_constant_buffer_pool::~material_constant_buffer_pool()
{
}

material_constant_buffer_pool::handle material_constant_buffer_pool::allocate(UINT size_in_bytes)
{
	if (size_in_bytes == 0)
		return invalid_handle;

	//	@tool 定数バッファの生成
	D3D11_BUFFER_DESC buffer_desc{};
	buffer_desc.Usage = D3D11_USAGE_DEFAULT;
	buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	buffer_desc.CPUAccessFlags = 0;
	buffer_desc.MiscFlags = 0;
	buffer_desc.StructureByteStride = 0;
	buffer_desc.ByteWidth = size_in_bytes;
	Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
	HRESULT hr = render_data::instance()->get_device()->CreateBuffer(&buffer_desc, nullptr, buffer.GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	if (FAILED(hr))
		return invalid_handle;

	std::lock_guard<std::mutex> lock(mutex);

	//	@tool 空いている番号があれば使い回す
	handle value = invalid_handle;
	if (!free_handles.empty())
	{
		value = free_handles.back();
		free_handles.pop_back();
	}
	else
	{
		value = static_cast<handle>(slots.size());
		slots.emplace_back();
	}

	slot& target = slots[value];
	target.buffer = std::move(buffer);
	target.size_in_bytes = size_in_bytes;
	target.pending_data.clear();
	target.used = true;
	target.upload_queued = false;
	return value;
}

void material_constant_buffer_pool::release(handle value)
{
	if (value == invalid_handle)
		return;

	std::lock_guard<std::mutex> lock(mutex);
	if (value >= slots.size() || !slots[value].used)
		return;

	//	@tool 描画中のフレームのプロキシー・転送一覧はバッファを参照で持っているので、ここでは参照を外すだけでよい
	slot& target = slots[value];
	target.buffer.Reset();
	target.size_in_bytes = 0;
	target.pending_data.clear();
	target.used = false;
	target.upload_queued = false;
	free_handles.emplace_back(value);
}

void material_constant_buffer_pool::queue_upload(handle value, const void* data, UINT size_in_bytes)
{
	if (value == invalid_handle || !data)
		return;

	std::lock_guard<std::mutex> lock(mutex);
	if (value >= slots.size() || !slots[value].used)
		return;

	//	@tool バッファ全体を書き込むので、足りない分は 0 で埋める
	slot& target = slots[value];
	const UINT copy_size = std::min(size_in_bytes, target.size_in_bytes);
	target.pending_data.assign(target.size_in_bytes, 0);
	memcpy(target.pending_data.data(), data, copy_size);

	if (!target.upload_queued)
	{
		target.upload_queued = true;
		queued_handles.emplace_back(value);
	}
}

Microsoft::WRL::ComPtr<ID3D11Buffer> material_constant_buffer_pool::get_buffer(handle value) const
{
	if (value == invalid_handle)
		return nullptr;

	std::lock_guard<std::mutex> lock(mutex);
	if (value >= slots.size() || !slots[value].used)
		return nullptr;

	return slots[value].buffer;
}

void material_constant_buffer_pool::flush_uploads(gpu_upload_list& out_uploads)
{
	std::lock_guard<std::mutex> lock(mutex);

	for (handle value : queued_handles)
	{
		//	@tool 解放済み・転送済み（同じ番号が二重に積まれた場合）は飛ばす
		slot& target = slots[value];
		if (!target.used || !target.upload_queued)
			continue;

		target.upload_queued = false;
		out_uploads.add_owned(target.buffer, false, 0, std::move(target.pending_data));
		target.pending_data.clear();
	}
	queued_handles.clear();
}
