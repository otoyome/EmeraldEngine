#include "model_buffer_pool.h"
#include "render_data.h"
#include "common_pipeline_states.h"

namespace
{
	//	頂点・インデックスバッファの初期容量
	constexpr UINT vertex_slot_capacity = 3 * 1000 * 1000;

	//	メッシュレット用頂点・三角形バッファの初期容量
	constexpr UINT meshlet_vertex_pool_capacity = 500 * 1000;
	constexpr UINT meshlet_triangle_pool_capacity = 1500 * 1000;
}

model_gpu_buffer::model_gpu_buffer(UINT bind_flag, UINT capacity_in_bytes, UINT stride_in_bytes)
: capacity_in_bytes(capacity_in_bytes)
, stride_in_bytes(stride_in_bytes)
, bind_flag(bind_flag)
{
	D3D11_BUFFER_DESC buffer_desc{};
	buffer_desc.ByteWidth = capacity_in_bytes;
	buffer_desc.Usage = D3D11_USAGE_DEFAULT;
	buffer_desc.BindFlags = bind_flag | D3D11_BIND_SHADER_RESOURCE;
	buffer_desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
	HRESULT hr = render_data::instance()->get_device()->CreateBuffer(&buffer_desc, nullptr, buffer.GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	create_srv();
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
	upload.data.assign(static_cast<const uint8_t*>(data), static_cast<const uint8_t*>(data) + size_in_bytes);
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
		box.right = upload.offset_in_bytes + static_cast<UINT>(upload.data.size());
		box.top = 0;
		box.bottom = 1;
		box.front = 0;
		box.back = 1;
		render_data::instance()->get_immediate_context()->UpdateSubresource(buffer.Get(), 0, &box, upload.data.data(), 0, 0);
	}
	pending_uploads.clear();
}

void model_gpu_buffer::grow(UINT new_capacity_in_bytes)
{
	if (new_capacity_in_bytes <= capacity_in_bytes)
		return;

	D3D11_BUFFER_DESC buffer_desc{};
	buffer_desc.ByteWidth = new_capacity_in_bytes;
	buffer_desc.Usage = D3D11_USAGE_DEFAULT;
	buffer_desc.BindFlags = bind_flag | D3D11_BIND_SHADER_RESOURCE;
	buffer_desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;

	Microsoft::WRL::ComPtr<ID3D11Buffer> new_buffer;
	HRESULT hr = render_data::instance()->get_device()->CreateBuffer(&buffer_desc, nullptr, new_buffer.GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	if (FAILED(hr))
		return;

	D3D11_BOX box{};
	box.left = 0;
	box.right = capacity_in_bytes;
	box.top = 0;
	box.bottom = 1;
	box.front = 0;
	box.back = 1;
	render_data::instance()->get_immediate_context()->CopySubresourceRegion(new_buffer.Get(), 0, 0, 0, 0, buffer.Get(), 0, &box);

	buffer = new_buffer;
	capacity_in_bytes = new_capacity_in_bytes;

	create_srv();
}

void model_gpu_buffer::create_srv()
{
	D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc{};
	srv_desc.Format = DXGI_FORMAT_R32_TYPELESS;
	srv_desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
	srv_desc.BufferEx.FirstElement = 0;
	srv_desc.BufferEx.NumElements = capacity_in_bytes / 4;
	srv_desc.BufferEx.Flags = D3D11_BUFFEREX_SRV_FLAG_RAW;
	HRESULT hr = render_data::instance()->get_device()->CreateShaderResourceView(buffer.Get(), &srv_desc, srv.ReleaseAndGetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
model_buffer_pool_manager::model_buffer_pool_manager()
: meshlet_vertex_buffer(std::make_unique<model_gpu_buffer>(D3D11_BIND_SHADER_RESOURCE, static_cast<UINT>(meshlet_vertex_pool_capacity * sizeof(UINT)), static_cast<UINT>(sizeof(UINT))))
, meshlet_triangle_buffer(std::make_unique<model_gpu_buffer>(D3D11_BIND_SHADER_RESOURCE, static_cast<UINT>(meshlet_triangle_pool_capacity * sizeof(UINT)), static_cast<UINT>(sizeof(UINT))))
{
	vertex_slot_allocator.initialize(vertex_slot_capacity);
	vertex_slot_capacity_current = vertex_slot_capacity;

	meshlet_vertex_allocator.initialize(meshlet_vertex_pool_capacity);
	meshlet_vertex_capacity_current = meshlet_vertex_pool_capacity;

	meshlet_triangle_allocator.initialize(meshlet_triangle_pool_capacity);
	meshlet_triangle_capacity_current = meshlet_triangle_pool_capacity;

}

model_buffer_pool_manager::~model_buffer_pool_manager()
{
}

UINT model_buffer_pool_manager::allocate_vertex_slots(UINT vertex_count)
{
	UINT base_vertex = vertex_slot_allocator.allocate(vertex_count);
	if (base_vertex == variable_position_allocator::allocate_failed_code)
	{
		UINT growth = vertex_slot_capacity_current > vertex_count ? vertex_slot_capacity_current : vertex_count;
		grow_vertex_capacity(growth);
		base_vertex = vertex_slot_allocator.allocate(vertex_count);
	}
	_ASSERT_EXPR(base_vertex != variable_position_allocator::allocate_failed_code, L"model_buffer_pool_manager allocate failed");
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
		UINT capacity_in_bytes = vertex_slot_capacity_current * stride_in_bytes;
		it = vertex_buffers.emplace(target_semantic, std::make_unique<model_gpu_buffer>(D3D11_BIND_VERTEX_BUFFER, capacity_in_bytes, stride_in_bytes)).first;
	}
	return *it->second;
}

void model_buffer_pool_manager::grow_vertex_capacity(UINT additional_vertex_count)
{
	if (!vertex_slot_allocator.grow(additional_vertex_count))
	{
		_ASSERT_EXPR(false, L"model_buffer_pool_manager: failed to grow vertex slot allocator");
		return;
	}
	vertex_slot_capacity_current += additional_vertex_count;

	for (auto& pool : vertex_buffers)
		pool.second->grow(vertex_slot_capacity_current * pool.second->get_stride_in_bytes());
}

UINT model_buffer_pool_manager::allocate_meshlet_vertices(UINT count)
{
	UINT base = meshlet_vertex_allocator.allocate(count);
	if (base == variable_position_allocator::allocate_failed_code)
	{
		UINT growth = meshlet_vertex_capacity_current > count ? meshlet_vertex_capacity_current : count;
		grow_meshlet_vertex_capacity(growth);
		base = meshlet_vertex_allocator.allocate(count);
	}
	_ASSERT_EXPR(base != variable_position_allocator::allocate_failed_code, L"model_buffer_pool_manager allocate failed");
	return base;
}

void model_buffer_pool_manager::free_meshlet_vertices(UINT base)
{
	meshlet_vertex_allocator.deallocate(base);
}

UINT model_buffer_pool_manager::allocate_meshlet_triangles(UINT count)
{
	UINT base = meshlet_triangle_allocator.allocate(count);
	if (base == variable_position_allocator::allocate_failed_code)
	{
		UINT growth = meshlet_triangle_capacity_current > count ? meshlet_triangle_capacity_current : count;
		grow_meshlet_triangle_capacity(growth);
		base = meshlet_triangle_allocator.allocate(count);
	}
	_ASSERT_EXPR(base != variable_position_allocator::allocate_failed_code, L"model_buffer_pool_manager allocate failed");
	return base;
}

void model_buffer_pool_manager::free_meshlet_triangles(UINT base)
{
	meshlet_triangle_allocator.deallocate(base);
}

void model_buffer_pool_manager::grow_meshlet_vertex_capacity(UINT additional_count)
{
	if (!meshlet_vertex_allocator.grow(additional_count))
	{
		_ASSERT_EXPR(false, L"model_buffer_pool_manager: failed to grow meshlet vertex pool allocator");
		return;
	}
	meshlet_vertex_capacity_current += additional_count;
	meshlet_vertex_buffer->grow(meshlet_vertex_capacity_current * sizeof(UINT));
}

void model_buffer_pool_manager::grow_meshlet_triangle_capacity(UINT additional_count)
{
	if (!meshlet_triangle_allocator.grow(additional_count))
	{
		_ASSERT_EXPR(false, L"model_buffer_pool_manager: failed to grow meshlet triangle pool allocator");
		return;
	}
	meshlet_triangle_capacity_current += additional_count;
	meshlet_triangle_buffer->grow(meshlet_triangle_capacity_current * sizeof(UINT));
}

void model_buffer_pool_manager::queue_vertex_upload(const semantic& target_semantic, UINT byte_offset, const void* data, UINT size_in_bytes, UINT stride_in_bytes)
{
	get_vertex_buffer_pool(target_semantic, stride_in_bytes).queue_upload(byte_offset, data, size_in_bytes);
}

void model_buffer_pool_manager::build_vertex_buffers(std::unordered_map<semantic, buffer_view>& vertex_buffer_views)
{
	if (vertex_buffer_views.empty())
		return;

	auto& first_view = vertex_buffer_views.begin()->second;
	UINT vertex_count = static_cast<UINT>(first_view.count());
	UINT base_vertex = allocate_vertex_slots(vertex_count);

	for (auto& view : vertex_buffer_views)
	{
		UINT stride_in_bytes = static_cast<UINT>(view.second.stride_in_bytes);
		view.second.offset_in_bytes = static_cast<size_t>(base_vertex) * stride_in_bytes;
		queue_vertex_upload(view.first, static_cast<UINT>(view.second.offset_in_bytes),
			view.second.row_data.data(), static_cast<UINT>(view.second.size_in_bytes), stride_in_bytes);
	}
}

void model_buffer_pool_manager::free_vertex_buffers(const std::unordered_map<semantic, buffer_view>& vertex_buffer_views)
{
	if (vertex_buffer_views.empty())
		return;

	UINT base_vertex = static_cast<UINT>(vertex_buffer_views.begin()->second.offset());
	free_vertex_slots(base_vertex);
}

ID3D11Buffer* model_buffer_pool_manager::get_vertex_buffer(const semantic& target_semantic)
{
	auto it = vertex_buffers.find(target_semantic);
	_ASSERT_EXPR(it != vertex_buffers.end(), L"model_buffer_pool_manager unmanaged semantics");
	return it->second->get_buffer();
}

ID3D11ShaderResourceView* model_buffer_pool_manager::get_vertex_buffer_srv(const semantic& target_semantic)
{
	auto it = vertex_buffers.find(target_semantic);
	if (it == vertex_buffers.end())
		return nullptr;
	return it->second->get_srv();
}

void model_buffer_pool_manager::build_meshlet_data(const std::vector<UINT>& meshlet_vertices, const std::vector<uint8_t>& meshlet_triangles, UINT& out_vertex_base, UINT& out_triangle_base)
{
	out_vertex_base = 0;
	out_triangle_base = 0;

	if (!meshlet_vertices.empty())
	{
		UINT count = static_cast<UINT>(meshlet_vertices.size());
		out_vertex_base = allocate_meshlet_vertices(count);
		meshlet_vertex_buffer->queue_upload(out_vertex_base * sizeof(UINT), meshlet_vertices.data(), count * sizeof(UINT));
	}

	if (!meshlet_triangles.empty())
	{
		std::vector<UINT> widened(meshlet_triangles.begin(), meshlet_triangles.end());
		UINT count = static_cast<UINT>(widened.size());
		out_triangle_base = allocate_meshlet_triangles(count);
		meshlet_triangle_buffer->queue_upload(out_triangle_base * sizeof(UINT), widened.data(), count * sizeof(UINT));
	}
}

void model_buffer_pool_manager::free_meshlet_data(UINT vertex_base, UINT vertex_count, UINT triangle_base, UINT triangle_count)
{
	if (vertex_count > 0)
		free_meshlet_vertices(vertex_base);
	if (triangle_count > 0)
		free_meshlet_triangles(triangle_base);
}

ID3D11ShaderResourceView* model_buffer_pool_manager::get_meshlet_vertex_buffer_srv()
{
	return meshlet_vertex_buffer->get_srv();
}

ID3D11ShaderResourceView* model_buffer_pool_manager::get_meshlet_triangle_buffer_srv()
{
	return meshlet_triangle_buffer->get_srv();
}

void model_buffer_pool_manager::get_meshlet_instance_batch(UINT min_capacity, ID3D11Buffer** out_buffer, ID3D11ShaderResourceView** out_srv)
{
	std::lock_guard<std::mutex> lock(meshlet_instance_batch_mutex);

	if (min_capacity > meshlet_instance_batch_capacity || !meshlet_instance_batch_buffer)
	{
		UINT cap = meshlet_instance_batch_capacity ? meshlet_instance_batch_capacity : 256;
		while (cap < min_capacity) cap *= 2;
		meshlet_instance_batch_capacity = cap;

		auto device = render_data::instance()->get_device();

		D3D11_BUFFER_DESC bd{};
		bd.ByteWidth = sizeof(common_pipeline_states::meshlet_instance_record) * cap;
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		bd.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		bd.StructureByteStride = sizeof(common_pipeline_states::meshlet_instance_record);
		HRESULT hr = device->CreateBuffer(&bd, nullptr, meshlet_instance_batch_buffer.ReleaseAndGetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

		D3D11_SHADER_RESOURCE_VIEW_DESC sd{};
		sd.Format = DXGI_FORMAT_UNKNOWN;
		sd.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		sd.Buffer.FirstElement = 0;
		sd.Buffer.NumElements = cap;
		hr = device->CreateShaderResourceView(meshlet_instance_batch_buffer.Get(), &sd, meshlet_instance_batch_srv.ReleaseAndGetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	}

	*out_buffer = meshlet_instance_batch_buffer.Get();
	*out_srv = meshlet_instance_batch_srv.Get();
}

void model_buffer_pool_manager::flush_uploads()
{
	for (auto& pool : vertex_buffers)
	{
		pool.second->flush_uploads();
	}
	meshlet_vertex_buffer->flush_uploads();
	meshlet_triangle_buffer->flush_uploads();
}

std::vector<model_buffer_pool_manager::pool_usage> model_buffer_pool_manager::get_pool_usages() const
{
	std::vector<pool_usage> usages;
	usages.reserve(vertex_buffers.size());

	UINT used_vertex_slots = vertex_slot_allocator.get_used_size();
	for (auto& pool : vertex_buffers)
	{
		pool_usage usage;
		usage.label = std::string(pool.first.get_name()) + std::to_string(pool.first.get_index());
		usage.used_in_bytes = used_vertex_slots * pool.second->get_stride_in_bytes();
		usage.capacity_in_bytes = pool.second->get_capacity_in_bytes();
		usages.emplace_back(std::move(usage));
	}

	return usages;
}
