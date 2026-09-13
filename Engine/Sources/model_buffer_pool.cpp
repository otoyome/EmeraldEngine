#include "model_buffer_pool.h"
#include "render_data.h"

namespace
{
	constexpr UINT vertex_slot_capacity = 3 * 1000 * 1000;
	constexpr UINT index_pool_capacity_in_bytes = 32 * 1024 * 1024;
}

model_gpu_buffer::model_gpu_buffer(UINT bind_flag, UINT capacity_in_bytes, UINT stride_in_bytes)
: capacity_in_bytes(capacity_in_bytes)
, stride_in_bytes(stride_in_bytes)
{
	D3D11_BUFFER_DESC buffer_desc{};
	buffer_desc.ByteWidth = capacity_in_bytes;
	buffer_desc.Usage = D3D11_USAGE_DEFAULT;
	buffer_desc.BindFlags = bind_flag;
	HRESULT hr = render_data::instance()->get_device()->CreateBuffer(&buffer_desc, nullptr, buffer.GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
}

model_gpu_buffer::~model_gpu_buffer()
{
}

void model_gpu_buffer::queue_upload(UINT offset_in_bytes, const void* data, UINT size_in_bytes)
{
	if (size_in_bytes == 0)
		return;

	pending_upload upload;
	upload.offset_in_bytes = offset_in_bytes;
	upload.data = data;
	upload.size_in_bytes = size_in_bytes;
	pending_uploads.emplace_back(std::move(upload));
}

void model_gpu_buffer::flush_uploads()
{
	if (pending_uploads.empty())
		return;

	for (auto& upload : pending_uploads)
	{
		D3D11_BOX box{};
		box.left = upload.offset_in_bytes;
		box.right = upload.offset_in_bytes + upload.size_in_bytes;
		box.top = 0;
		box.bottom = 1;
		box.front = 0;
		box.back = 1;
		render_data::instance()->get_immediate_context()->UpdateSubresource(buffer.Get(), 0, &box, upload.data, 0, 0);
	}
	pending_uploads.clear();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
model_buffer_pool_manager::model_buffer_pool_manager()
: index_buffer(std::make_unique<model_gpu_buffer>(D3D11_BIND_INDEX_BUFFER, index_pool_capacity_in_bytes))
{
	vertex_slot_allocator.initialize(vertex_slot_capacity);
	index_position_allocator.initialize(index_pool_capacity_in_bytes);
}

model_buffer_pool_manager::~model_buffer_pool_manager()
{
}

UINT model_buffer_pool_manager::allocate_vertex_slots(UINT vertex_count)
{
	UINT base_vertex = vertex_slot_allocator.allocate(vertex_count);
	_ASSERT_EXPR(base_vertex != variable_position_allocator::allocate_failed_code, L"model_buffer_pool_manager: 頂点番号空間が枯渇しました");
	return base_vertex;
}

void model_buffer_pool_manager::free_vertex_slots(UINT base_vertex)
{
	vertex_slot_allocator.deallocate(base_vertex);
}

model_gpu_buffer& model_buffer_pool_manager::get_vertex_buffer_pool(const semantic& target_semantic, UINT stride_in_bytes)
{
	auto it = vertex_buffers.find(target_semantic);
	if (it == vertex_buffers.end())
	{
		UINT capacity_in_bytes = vertex_slot_capacity * stride_in_bytes;
		it = vertex_buffers.emplace(target_semantic, std::make_unique<model_gpu_buffer>(D3D11_BIND_VERTEX_BUFFER, capacity_in_bytes, stride_in_bytes)).first;
	}
	return *it->second;
}

void model_buffer_pool_manager::queue_vertex_upload(const semantic& target_semantic, UINT byte_offset, const void* data, UINT size_in_bytes, UINT stride_in_bytes)
{
	get_vertex_buffer_pool(target_semantic, stride_in_bytes).queue_upload(byte_offset, data, size_in_bytes);
}

ID3D11Buffer* model_buffer_pool_manager::get_vertex_buffer(const semantic& target_semantic)
{
	auto it = vertex_buffers.find(target_semantic);
	_ASSERT_EXPR(it != vertex_buffers.end(), L"model_buffer_pool_manager: 未確保のセマンティクスです");
	return it->second->get_buffer();
}

UINT model_buffer_pool_manager::allocate_index(UINT size_in_bytes)
{
	UINT offset_in_bytes = index_position_allocator.allocate(size_in_bytes);
	_ASSERT_EXPR(offset_in_bytes != variable_position_allocator::allocate_failed_code, L"model_buffer_pool_manager: インデックスバッファ容量が枯渇しました");
	return offset_in_bytes;
}

void model_buffer_pool_manager::free_index(UINT offset_in_bytes)
{
	index_position_allocator.deallocate(offset_in_bytes);
}

void model_buffer_pool_manager::queue_index_upload(UINT offset_in_bytes, const void* data, UINT size_in_bytes)
{
	index_buffer->queue_upload(offset_in_bytes, data, size_in_bytes);
}

ID3D11Buffer* model_buffer_pool_manager::get_index_buffer()
{
	return index_buffer->get_buffer();
}

void model_buffer_pool_manager::flush_uploads()
{
	for (auto& pool : vertex_buffers)
	{
		pool.second->flush_uploads();
	}
	index_buffer->flush_uploads();
}

std::vector<model_buffer_pool_manager::pool_usage> model_buffer_pool_manager::get_pool_usages() const
{
	std::vector<pool_usage> usages;
	usages.reserve(vertex_buffers.size() + 1);

	UINT used_vertex_slots = vertex_slot_allocator.get_used_size();
	for (auto& pool : vertex_buffers)
	{
		pool_usage usage;
		usage.label = std::string(pool.first.get_name()) + std::to_string(pool.first.get_index());
		usage.used_in_bytes = used_vertex_slots * pool.second->get_stride_in_bytes();
		usage.capacity_in_bytes = pool.second->get_capacity_in_bytes();
		usages.emplace_back(std::move(usage));
	}

	pool_usage index_usage;
	index_usage.label = "INDEX";
	index_usage.used_in_bytes = index_position_allocator.get_used_size();
	index_usage.capacity_in_bytes = index_buffer->get_capacity_in_bytes();
	usages.emplace_back(std::move(index_usage));

	return usages;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
namespace model_gpu_buffer_builder
{
	void build_vertex_buffers(std::unordered_map<semantic, buffer_view>& vertex_buffer_views)
	{
		if (vertex_buffer_views.empty())
			return;

		auto& first_view = vertex_buffer_views.begin()->second;
		UINT vertex_count = static_cast<UINT>(first_view.count());
		UINT base_vertex = model_buffer_pool_manager::instance()->allocate_vertex_slots(vertex_count);

		for (auto& view : vertex_buffer_views)
		{
			UINT stride_in_bytes = static_cast<UINT>(view.second.stride_in_bytes);
			view.second.offset_in_bytes = static_cast<size_t>(base_vertex) * stride_in_bytes;
			model_buffer_pool_manager::instance()->queue_vertex_upload(view.first, static_cast<UINT>(view.second.offset_in_bytes),
				view.second.row_data.data(), static_cast<UINT>(view.second.size_in_bytes), stride_in_bytes);
		}
	}

	void free_vertex_buffers(const std::unordered_map<semantic, buffer_view>& vertex_buffer_views)
	{
		if (vertex_buffer_views.empty())
			return;

		UINT base_vertex = static_cast<UINT>(vertex_buffer_views.begin()->second.offset());
		model_buffer_pool_manager::instance()->free_vertex_slots(base_vertex);
	}

	void build_index_buffer(buffer_view& index_buffer_view)
	{
		if (index_buffer_view.size_in_bytes == 0)
			return;

		index_buffer_view.offset_in_bytes = model_buffer_pool_manager::instance()->allocate_index(static_cast<UINT>(index_buffer_view.size_in_bytes));
		model_buffer_pool_manager::instance()->queue_index_upload(static_cast<UINT>(index_buffer_view.offset_in_bytes),
			index_buffer_view.row_data.data(), static_cast<UINT>(index_buffer_view.size_in_bytes));
	}

	void free_index_buffer(const buffer_view& index_buffer_view)
	{
		if (index_buffer_view.size_in_bytes == 0)
			return;

		model_buffer_pool_manager::instance()->free_index(static_cast<UINT>(index_buffer_view.offset_in_bytes));
	}
}
