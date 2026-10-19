#include	"graphics/cubic_sdf_buffer_pool.h"
#include "Graphics/gpu_upload_list.h"

cubic_sdf_buffer_pool::cubic_sdf_buffer_pool(ID3D11Device* device, UINT voxel_pool_size, UINT header_capacity)
{
	HRESULT hr;
	if (!voxel_allocator.initialize(voxel_pool_size * sizeof(float)))
	{
		DEBUG_ERROR_CATEGORY(logger::categorys::model, "cubic_sdf_gpu_resource initialize failed : voxel_allocator initialize failed");
		return;
	}

	//	ボクセルバッファ本体
	{
		D3D11_BUFFER_DESC desc = {};
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		desc.ByteWidth = voxel_pool_size * sizeof(float);
		desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		desc.StructureByteStride = sizeof(float);
		desc.Usage = D3D11_USAGE_DEFAULT;
		hr = device->CreateBuffer(&desc, nullptr, voxel_buffer.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

		hr = device->CreateShaderResourceView(voxel_buffer.Get(), nullptr, voxel_shader_resource_view.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	}

	//	ヘッダーバッファ
	{
		headers.assign(header_capacity, sdf_header{});
		world_bounds_min.assign(header_capacity, math::vector3d::zero());
		world_bounds_max.assign(header_capacity, math::vector3d::zero());
		free_header_ids.resize(header_capacity);
		for (UINT index = 0; index < header_capacity; ++index)
			free_header_ids[index] = header_capacity - 1 - index;

		D3D11_BUFFER_DESC desc = {};
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		desc.ByteWidth = static_cast<UINT>(sizeof(sdf_header) * header_capacity);
		desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		desc.StructureByteStride = sizeof(sdf_header);
		desc.Usage = D3D11_USAGE_DEFAULT;

		D3D11_SUBRESOURCE_DATA initialize_data = {};
		initialize_data.pSysMem = headers.data();
		initialize_data.SysMemPitch = desc.ByteWidth;
		hr = device->CreateBuffer(&desc, &initialize_data, header_buffer.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

		hr = device->CreateShaderResourceView(header_buffer.Get(), nullptr, header_shader_resource_view.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	}
}

cubic_sdf_buffer_pool::~cubic_sdf_buffer_pool()
{
	voxel_shader_resource_view.Reset();
	voxel_buffer.Reset();
	header_shader_resource_view.Reset();
	header_buffer.Reset();
	headers.clear();
	free_header_ids.clear();
}

uint32_t cubic_sdf_buffer_pool::register_sdf(gpu_upload_list& upload_list, const collision::cubic_sdf& sdf)
{
	if (!sdf.is_valid())
	{
		DEBUG_ERROR_CATEGORY(logger::categorys::model, "cubic_sdf_gpu_resource::register_sdf failed : sdf is not valid");
		return invalid_id;
	}
	if (free_header_ids.empty())
	{
		DEBUG_ERROR_CATEGORY(logger::categorys::model, "cubic_sdf_gpu_resource::register_sdf failed : header pool is full");
		return invalid_id;
	}

	const UINT voxel_byte_size = static_cast<UINT>(sdf.voxels.size() * sizeof(float));
	const UINT voxel_byte_offset = voxel_allocator.allocate(voxel_byte_size);
	if (voxel_byte_offset == variable_position_allocator::allocate_failed_code)
	{
		DEBUG_ERROR_CATEGORY(logger::categorys::model, "cubic_sdf_gpu_resource::register_sdf failed : voxel pool is full");
		return invalid_id;
	}

	const uint32_t sdf_id = free_header_ids.back();
	free_header_ids.pop_back();

	//	ボクセル書き込み
	D3D11_BOX voxel_write_box = {};
	voxel_write_box.left = voxel_byte_offset;
	voxel_write_box.right = voxel_byte_offset + voxel_byte_size;
	voxel_write_box.top = 0;
	voxel_write_box.bottom = 1;
	voxel_write_box.front = 0;
	voxel_write_box.back = 1;
	upload_list.update_buffer(voxel_buffer.Get(), voxel_write_box.left, sdf.voxels.data(), voxel_byte_size);

	//	ヘッダーバッファ更新
	sdf_header header;
	header.voxel_offset = voxel_byte_offset / sizeof(float);
	header.resolution = sdf.resolution;
	//	GPU には基準原点からの相対で渡す
	world_bounds_min[sdf_id] = math::vector3d(sdf.bounds_min);
	world_bounds_max[sdf_id] = math::vector3d(sdf.bounds_max);
	header.bounds_min = (world_bounds_min[sdf_id] - space_origin).to_float3();
	header.bounds_max = (world_bounds_max[sdf_id] - space_origin).to_float3();
	headers[sdf_id] = header;

	D3D11_BOX header_write_box = {};
	header_write_box.left = static_cast<UINT>(sizeof(sdf_header) * sdf_id);
	header_write_box.right = header_write_box.left + static_cast<UINT>(sizeof(sdf_header));
	header_write_box.top = 0;
	header_write_box.bottom = 1;
	header_write_box.front = 0;
	header_write_box.back = 1;
	upload_list.update_buffer(header_buffer.Get(), header_write_box.left, &header, static_cast<UINT>(sizeof(sdf_header)));

	return sdf_id;
}

void cubic_sdf_buffer_pool::unregister_sdf(gpu_upload_list& upload_list, uint32_t sdf_id)
{
	if (sdf_id >= headers.size())
		return;

	auto& header = headers[sdf_id];
	if (header.resolution == 0)
		return;

	voxel_allocator.deallocate(header.voxel_offset * sizeof(float));
	header = sdf_header{};
	free_header_ids.emplace_back(sdf_id);

	D3D11_BOX header_write_box = {};
	header_write_box.left = static_cast<UINT>(sizeof(sdf_header) * sdf_id);
	header_write_box.right = header_write_box.left + static_cast<UINT>(sizeof(sdf_header));
	header_write_box.top = 0;
	header_write_box.bottom = 1;
	header_write_box.front = 0;
	header_write_box.back = 1;
	upload_list.update_buffer(header_buffer.Get(), header_write_box.left, &header, static_cast<UINT>(sizeof(sdf_header)));
}

void cubic_sdf_buffer_pool::set_space_origin(ID3D11DeviceContext* device_context, const math::vector3d& origin)
{
	space_origin = origin;
	for (size_t sdf_id = 0; sdf_id < headers.size(); ++sdf_id)
	{
		auto& header = headers[sdf_id];
		if (header.resolution == 0)
			continue;

		header.bounds_min = (world_bounds_min[sdf_id] - space_origin).to_float3();
		header.bounds_max = (world_bounds_max[sdf_id] - space_origin).to_float3();
	}
	device_context->UpdateSubresource(header_buffer.Get(), 0, nullptr, headers.data(), 0, 0);
}
