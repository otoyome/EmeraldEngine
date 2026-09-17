#include	"Collision/cubic_sdf_gpu_resource.h"

bool collision::cubic_sdf_gpu_resource::initialize(ID3D11Device* device, UINT voxel_pool_size, UINT header_capacity)
{
	HRESULT hr;

	if (!voxel_allocator.initialize(voxel_pool_size * sizeof(float)))
	{
		DEBUG_ERROR_CATEGORY(logger::categorys::model, "cubic_sdf_gpu_resource initialize failed : voxel_allocator initialize failed");
		return false;
	}

	//	voxelバッファ本体(CPUから部分更新するだけなのでUAVは不要、SRVのみで良い)
	{
		D3D11_BUFFER_DESC desc = {};
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		desc.ByteWidth = voxel_pool_size * sizeof(float);
		desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		desc.StructureByteStride = sizeof(float);
		desc.Usage = D3D11_USAGE_DEFAULT;
		hr = device->CreateBuffer(&desc, nullptr, voxel_buffer.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
		if (FAILED(hr))
			return false;

		hr = device->CreateShaderResourceView(voxel_buffer.Get(), nullptr, voxel_shader_resource_view.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
		if (FAILED(hr))
			return false;
	}

	//	ヘッダーバッファ(登録されているcubic_sdfの一覧)
	{
		headers.assign(header_capacity, sdf_header{});
		free_header_ids.resize(header_capacity);
		for (UINT index = 0; index < header_capacity; ++index)
			free_header_ids[index] = header_capacity - 1 - index;	//	先頭のIDから使われるように逆順で積んでおく

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
		if (FAILED(hr))
			return false;

		hr = device->CreateShaderResourceView(header_buffer.Get(), nullptr, header_shader_resource_view.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
		if (FAILED(hr))
			return false;
	}

	return true;
}

void collision::cubic_sdf_gpu_resource::uninitialize()
{
	voxel_shader_resource_view.Reset();
	voxel_buffer.Reset();
	header_shader_resource_view.Reset();
	header_buffer.Reset();
	headers.clear();
	free_header_ids.clear();
}

uint32_t collision::cubic_sdf_gpu_resource::register_sdf(ID3D11DeviceContext* device_context, const cubic_sdf& sdf)
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

	//	@tool voxel本体は該当領域だけを部分更新する
	D3D11_BOX voxel_write_box = {};
	voxel_write_box.left = voxel_byte_offset;
	voxel_write_box.right = voxel_byte_offset + voxel_byte_size;
	voxel_write_box.top = 0;
	voxel_write_box.bottom = 1;
	voxel_write_box.front = 0;
	voxel_write_box.back = 1;
	device_context->UpdateSubresource(voxel_buffer.Get(), 0, &voxel_write_box, sdf.voxels.data(), voxel_byte_size, 0);

	//	@tool ヘッダーも1件分だけ部分更新する
	sdf_header header;
	header.voxel_offset = voxel_byte_offset / sizeof(float);
	header.resolution = sdf.resolution;
	header.bounds_min = sdf.bounds_min;
	header.bounds_max = sdf.bounds_max;
	headers[sdf_id] = header;

	D3D11_BOX header_write_box = {};
	header_write_box.left = static_cast<UINT>(sizeof(sdf_header) * sdf_id);
	header_write_box.right = header_write_box.left + static_cast<UINT>(sizeof(sdf_header));
	header_write_box.top = 0;
	header_write_box.bottom = 1;
	header_write_box.front = 0;
	header_write_box.back = 1;
	device_context->UpdateSubresource(header_buffer.Get(), 0, &header_write_box, &header, sizeof(sdf_header), 0);

	return sdf_id;
}

void collision::cubic_sdf_gpu_resource::unregister_sdf(ID3D11DeviceContext* device_context, uint32_t sdf_id)
{
	if (sdf_id >= headers.size())
		return;

	auto& header = headers[sdf_id];
	if (header.resolution == 0)
		return;	//	@tool 未登録のIDに対する二重解放を無視する

	voxel_allocator.deallocate(header.voxel_offset * sizeof(float));
	header = sdf_header{};
	free_header_ids.emplace_back(sdf_id);

	//	@tool スロットが再利用される前に参照されても無効値として扱われるよう、GPU側のヘッダーも空にしておく
	D3D11_BOX header_write_box = {};
	header_write_box.left = static_cast<UINT>(sizeof(sdf_header) * sdf_id);
	header_write_box.right = header_write_box.left + static_cast<UINT>(sizeof(sdf_header));
	header_write_box.top = 0;
	header_write_box.bottom = 1;
	header_write_box.front = 0;
	header_write_box.back = 1;
	device_context->UpdateSubresource(header_buffer.Get(), 0, &header_write_box, &header, sizeof(sdf_header), 0);
}
