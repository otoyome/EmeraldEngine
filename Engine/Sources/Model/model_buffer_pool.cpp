#include "Model/model_buffer_pool.h"
#include "Graphics/render_data.h"
#include "Graphics/common_pipeline_states.h"

#include <iterator>

namespace
{
	//	属性毎の頂点プールの初期容量
	constexpr UINT vertex_pool_initial_capacity = 256 * 1024;

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

	auto& upload_list = render_data::instance()->get_upload_list();
	for (auto& upload : pending_uploads)
	{
		upload_list.update_buffer(buffer.Get(), upload.offset_in_bytes, upload.data.data(), static_cast<UINT>(upload.data.size()));
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

	render_data::instance()->get_upload_list().copy_buffer_region(new_buffer.Get(), 0, buffer.Get(), 0, capacity_in_bytes);

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
	meshlet_vertex_allocator.initialize(meshlet_vertex_pool_capacity);
	meshlet_vertex_capacity_current = meshlet_vertex_pool_capacity;

	meshlet_triangle_allocator.initialize(meshlet_triangle_pool_capacity);
	meshlet_triangle_capacity_current = meshlet_triangle_pool_capacity;

}

model_buffer_pool_manager::~model_buffer_pool_manager()
{
}

common_pipeline_states::vertex_elements model_buffer_pool_manager::find_vertex_element(const semantic& target_semantic)
{
	using common_pipeline_states::vertex_elements;

	static const semantic element_semantics[] =
	{
		semantic("POSITION", 0),
		semantic("NORMAL", 0),
		semantic("TANGENT", 0),
		semantic("COLOR", 0),
		semantic("TEXCOORD", 0),
		semantic("TEXCOORD", 1),
		semantic("JOINTS", 0),
		semantic("JOINTS", 1),
		semantic("JOINTS", 2),
		semantic("JOINTS", 3),
		semantic("WEIGHTS", 0),
		semantic("WEIGHTS", 1),
		semantic("WEIGHTS", 2),
		semantic("WEIGHTS", 3),
	};
	static_assert(std::size(element_semantics) == static_cast<size_t>(vertex_elements::max));

	for (UINT index = 0; index < static_cast<UINT>(vertex_elements::max); ++index)
	{
		if (element_semantics[index] == target_semantic)
			return static_cast<vertex_elements>(index);
	}
	return vertex_elements::max;
}

model_buffer_pool_manager::vertex_pool& model_buffer_pool_manager::get_vertex_pool(const semantic& target_semantic, UINT stride_in_bytes)
{
	auto it = vertex_buffers.find(target_semantic);
	if (it == vertex_buffers.end())
	{
		it = vertex_buffers.emplace(target_semantic, vertex_pool{}).first;
		vertex_pool& pool = it->second;
		pool.capacity = vertex_pool_initial_capacity;
		pool.allocator.initialize(pool.capacity);
		pool.buffer = std::make_unique<model_gpu_buffer>(D3D11_BIND_VERTEX_BUFFER, pool.capacity * stride_in_bytes, stride_in_bytes);
	}
	return it->second;
}

UINT model_buffer_pool_manager::allocate_vertices(vertex_pool& pool, UINT vertex_count)
{
	UINT base_vertex = pool.allocator.allocate(vertex_count);
	if (base_vertex == variable_position_allocator::allocate_failed_code)
	{
		UINT growth = pool.capacity > vertex_count ? pool.capacity : vertex_count;
		if (!pool.allocator.grow(growth))
		{
			_ASSERT_EXPR(false, L"model_buffer_pool_manager: failed to grow vertex pool allocator");
			return variable_position_allocator::allocate_failed_code;
		}
		pool.capacity += growth;
		pool.buffer->grow(pool.capacity * pool.buffer->get_stride_in_bytes());
		base_vertex = pool.allocator.allocate(vertex_count);
	}
	_ASSERT_EXPR(base_vertex != variable_position_allocator::allocate_failed_code, L"model_buffer_pool_manager allocate failed");
	return base_vertex;
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

void model_buffer_pool_manager::build_vertex_buffers(std::unordered_map<semantic, buffer_view>& vertex_buffer_views)
{
	for (auto& view : vertex_buffer_views)
	{
		if (find_vertex_element(view.first) == common_pipeline_states::vertex_elements::max)
			continue;
		if (view.second.stride_in_bytes == 0 || view.second.size_in_bytes == 0)
			continue;

		UINT stride_in_bytes = static_cast<UINT>(view.second.stride_in_bytes);
		vertex_pool& pool = get_vertex_pool(view.first, stride_in_bytes);
		_ASSERT_EXPR(pool.buffer->get_stride_in_bytes() == stride_in_bytes, L"model_buffer_pool_manager: stride mismatch in the same semantic");

		UINT base_vertex = allocate_vertices(pool, static_cast<UINT>(view.second.count()));
		if (base_vertex == variable_position_allocator::allocate_failed_code)
			continue;

		view.second.offset_in_bytes = static_cast<size_t>(base_vertex) * stride_in_bytes;
		pool.buffer->queue_upload(static_cast<UINT>(view.second.offset_in_bytes),
								  view.second.row_data.data(), static_cast<UINT>(view.second.size_in_bytes));
	}
}

void model_buffer_pool_manager::free_vertex_buffers(const std::unordered_map<semantic, buffer_view>& vertex_buffer_views)
{
	for (auto& view : vertex_buffer_views)
	{
		auto it = vertex_buffers.find(view.first);
		if (it == vertex_buffers.end() || view.second.stride_in_bytes == 0 || view.second.size_in_bytes == 0)
			continue;

		it->second.allocator.deallocate(static_cast<UINT>(view.second.offset()));
	}
}

ID3D11Buffer* model_buffer_pool_manager::get_vertex_buffer(const semantic& target_semantic)
{
	auto it = vertex_buffers.find(target_semantic);
	_ASSERT_EXPR(it != vertex_buffers.end(), L"model_buffer_pool_manager unmanaged semantics");
	return it->second.buffer->get_buffer();
}

ID3D11ShaderResourceView* model_buffer_pool_manager::get_vertex_buffer_srv(const semantic& target_semantic)
{
	auto it = vertex_buffers.find(target_semantic);
	if (it == vertex_buffers.end())
		return nullptr;
	return it->second.buffer->get_srv();
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
		pool.second.buffer->flush_uploads();
	}
	meshlet_vertex_buffer->flush_uploads();
	meshlet_triangle_buffer->flush_uploads();
}

std::vector<model_buffer_pool_manager::pool_usage> model_buffer_pool_manager::get_pool_usages() const
{
	std::vector<pool_usage> usages;
	usages.reserve(vertex_buffers.size());

	for (auto& pool : vertex_buffers)
	{
		pool_usage usage;
		usage.label = std::string(pool.first.get_name()) + std::to_string(pool.first.get_index());
		usage.used_in_bytes = pool.second.allocator.get_used_size() * pool.second.buffer->get_stride_in_bytes();
		usage.capacity_in_bytes = pool.second.buffer->get_capacity_in_bytes();
		usages.emplace_back(std::move(usage));
	}

	return usages;
}
