#include "model_buffer_pool.h"
#include "render_data.h"

namespace
{
	constexpr UINT pool_capacity_in_bytes = 32 * 1024 * 1024;
}

model_buffer_pool::model_buffer_pool(UINT bind_flag, UINT capacity_in_bytes)
: capacity_in_bytes(capacity_in_bytes)
{
	allocator.initialize(capacity_in_bytes);

	D3D11_BUFFER_DESC buffer_desc{};
	buffer_desc.ByteWidth = capacity_in_bytes;
	buffer_desc.Usage = D3D11_USAGE_DEFAULT;
	buffer_desc.BindFlags = bind_flag;
	HRESULT hr = render_data::instance()->get_device()->CreateBuffer(&buffer_desc, nullptr, buffer.GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
}

model_buffer_pool::~model_buffer_pool()
{
}

UINT model_buffer_pool::allocate(UINT size_in_bytes)
{
	UINT offset_in_bytes = allocator.allocate(size_in_bytes);
	_ASSERT_EXPR(offset_in_bytes != variable_position_allocator::allocate_failed_code, L"model_buffer_pool: —e—Ê‚ªŒÍŠ‰");
	used_in_bytes += size_in_bytes;
	return offset_in_bytes;
}

void model_buffer_pool::free(UINT offset_in_bytes, UINT size_in_bytes)
{
	if (allocator.deallocate(offset_in_bytes))
	{
		used_in_bytes -= size_in_bytes;
	}
}

void model_buffer_pool::queue_upload(UINT offset_in_bytes, const void* data, UINT size_in_bytes)
{
	if (size_in_bytes == 0)
		return;

	pending_upload upload;
	upload.offset_in_bytes = offset_in_bytes;
	upload.data.assign(static_cast<const BYTE*>(data), static_cast<const BYTE*>(data) + size_in_bytes);
	pending_uploads.emplace_back(std::move(upload));
}

void model_buffer_pool::flush_uploads()
{
	if (pending_uploads.empty())
		return;

	for (auto& upload : pending_uploads)
	{
		D3D11_BOX box{};
		box.left = upload.offset_in_bytes;
		box.right = upload.offset_in_bytes + static_cast<UINT>(upload.data.size());
		box.top = 0;
		box.bottom = 1;
		box.front = 0;
		box.back = 1;
		render_data::instance()->get_immediate_context()->UpdateSubresource(buffer.Get(), 0, &box, upload.data.data(), 0, 0);
	}
	pending_uploads.clear();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
model_buffer_pool_manager::model_buffer_pool_manager()
: index_pool(std::make_unique<model_buffer_pool>(D3D11_BIND_INDEX_BUFFER, pool_capacity_in_bytes))
{
}

model_buffer_pool_manager::~model_buffer_pool_manager()
{
}

model_buffer_pool& model_buffer_pool_manager::get_vertex_pool(const semantic& target_semantic)
{
	auto it = vertex_pools.find(target_semantic);
	if (it == vertex_pools.end())
	{
		it = vertex_pools.emplace(target_semantic, std::make_unique<model_buffer_pool>(D3D11_BIND_VERTEX_BUFFER, pool_capacity_in_bytes)).first;
	}
	return *it->second;
}

UINT model_buffer_pool_manager::allocate_vertex(const semantic& target_semantic, UINT size_in_bytes)
{
	return get_vertex_pool(target_semantic).allocate(size_in_bytes);
}

void model_buffer_pool_manager::free_vertex(const semantic& target_semantic, UINT offset_in_bytes, UINT size_in_bytes)
{
	get_vertex_pool(target_semantic).free(offset_in_bytes, size_in_bytes);
}

void model_buffer_pool_manager::queue_vertex_upload(const semantic& target_semantic, UINT offset_in_bytes, const void* data, UINT size_in_bytes)
{
	get_vertex_pool(target_semantic).queue_upload(offset_in_bytes, data, size_in_bytes);
}

ID3D11Buffer* model_buffer_pool_manager::get_vertex_buffer(const semantic& target_semantic)
{
	return get_vertex_pool(target_semantic).get_buffer();
}

UINT model_buffer_pool_manager::allocate_index(UINT size_in_bytes)
{
	return index_pool->allocate(size_in_bytes);
}

void model_buffer_pool_manager::free_index(UINT offset_in_bytes, UINT size_in_bytes)
{
	index_pool->free(offset_in_bytes, size_in_bytes);
}

void model_buffer_pool_manager::queue_index_upload(UINT offset_in_bytes, const void* data, UINT size_in_bytes)
{
	index_pool->queue_upload(offset_in_bytes, data, size_in_bytes);
}

ID3D11Buffer* model_buffer_pool_manager::get_index_buffer()
{
	return index_pool->get_buffer();
}

void model_buffer_pool_manager::flush_uploads()
{
	for (auto& pool : vertex_pools)
	{
		pool.second->flush_uploads();
	}
	index_pool->flush_uploads();
}

std::vector<model_buffer_pool_manager::pool_usage> model_buffer_pool_manager::get_pool_usages() const
{
	std::vector<pool_usage> usages;
	usages.reserve(vertex_pools.size() + 1);

	for (auto& pool : vertex_pools)
	{
		pool_usage usage;
		usage.label = std::string(pool.first.get_name()) + std::to_string(pool.first.get_index());
		usage.used_in_bytes = pool.second->get_used_in_bytes();
		usage.capacity_in_bytes = pool.second->get_capacity_in_bytes();
		usages.emplace_back(std::move(usage));
	}

	pool_usage index_usage;
	index_usage.label = "INDEX";
	index_usage.used_in_bytes = index_pool->get_used_in_bytes();
	index_usage.capacity_in_bytes = index_pool->get_capacity_in_bytes();
	usages.emplace_back(std::move(index_usage));

	return usages;
}
