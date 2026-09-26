
#include "graphics/meshlet_culling_system.h"

#include "render_data.h"
#include "System/math_utility.h"

meshlet_culling_system::meshlet_culling_system()
{
	//	シェーダー生成
	{
		load_shader_file desc;
		desc.file_path = "Shaders/Culling/meshlet_culling_cs.hlsl";
#if defined(_DEBUG)
		system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::disable_optimize, true);
		system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::debug, true);
#endif	//	defined(_DEBUG)
		shader_manager::instance()->load_shader<compute_shader>(meshlet_culling_cs, desc);
	}

	//	定数バッファ生成
	{
		D3D11_BUFFER_DESC desc{};
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		desc.ByteWidth = sizeof(cull_constants);
		HRESULT hr = render_data::instance()->get_device()->CreateBuffer(&desc, nullptr, cull_constant_buffer.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	}
}

meshlet_culling_system::~meshlet_culling_system()
{
}

void meshlet_culling_system::generate_meshlet_instance_buffer(UINT count)
{
	if (count <= meshlet_instance_capacity && meshlet_instance_buffer)
		return;

	UINT cap = meshlet_instance_capacity ? meshlet_instance_capacity : 1024;
	while (cap < count) cap *= 2;
	meshlet_instance_capacity = cap;

	auto device = render_data::instance()->get_device();

	D3D11_BUFFER_DESC bd{};
	bd.ByteWidth = sizeof(meshlet_instance_gpu) * cap;
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	bd.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	bd.StructureByteStride = sizeof(meshlet_instance_gpu);
	HRESULT hr = device->CreateBuffer(&bd, nullptr, meshlet_instance_buffer.ReleaseAndGetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	D3D11_SHADER_RESOURCE_VIEW_DESC sd{};
	sd.Format = DXGI_FORMAT_UNKNOWN;
	sd.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	sd.Buffer.FirstElement = 0;
	sd.Buffer.NumElements = cap;
	hr = device->CreateShaderResourceView(meshlet_instance_buffer.Get(), &sd, meshlet_instance_srv.ReleaseAndGetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
}

void meshlet_culling_system::generate_submesh_transform_buffer(UINT count)
{
	if (count <= submesh_transform_capacity && submesh_transform_buffer)
		return;

	UINT cap = submesh_transform_capacity ? submesh_transform_capacity : 256;
	while (cap < count) cap *= 2;
	submesh_transform_capacity = cap;

	auto device = render_data::instance()->get_device();

	D3D11_BUFFER_DESC bd{};
	bd.ByteWidth = sizeof(DirectX::XMFLOAT4X4) * cap;
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	bd.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	bd.StructureByteStride = sizeof(DirectX::XMFLOAT4X4);
	HRESULT hr = device->CreateBuffer(&bd, nullptr, submesh_transform_buffer.ReleaseAndGetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	D3D11_SHADER_RESOURCE_VIEW_DESC sd{};
	sd.Format = DXGI_FORMAT_UNKNOWN;
	sd.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	sd.Buffer.FirstElement = 0;
	sd.Buffer.NumElements = cap;
	hr = device->CreateShaderResourceView(submesh_transform_buffer.Get(), &sd, submesh_transform_srv.ReleaseAndGetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
}

void meshlet_culling_system::generate_visible_meshlet_buffer(UINT count)
{
	if (count <= visible_meshlet_capacity && visible_meshlet_buffer)
		return;

	UINT cap = visible_meshlet_capacity ? visible_meshlet_capacity : 1024;
	while (cap < count) cap *= 2;
	visible_meshlet_capacity = cap;

	auto device = render_data::instance()->get_device();

	D3D11_BUFFER_DESC bd{};
	bd.ByteWidth = sizeof(meshlet_instance_gpu) * cap;
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
	bd.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	bd.StructureByteStride = sizeof(meshlet_instance_gpu);
	HRESULT hr = device->CreateBuffer(&bd, nullptr, visible_meshlet_buffer.ReleaseAndGetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	D3D11_UNORDERED_ACCESS_VIEW_DESC ud{};
	ud.Format = DXGI_FORMAT_UNKNOWN;
	ud.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	ud.Buffer.FirstElement = 0;
	ud.Buffer.NumElements = cap;
	ud.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_APPEND;
	hr = device->CreateUnorderedAccessView(visible_meshlet_buffer.Get(), &ud, visible_meshlet_uav.ReleaseAndGetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	D3D11_SHADER_RESOURCE_VIEW_DESC sd{};
	sd.Format = DXGI_FORMAT_UNKNOWN;
	sd.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	sd.Buffer.FirstElement = 0;
	sd.Buffer.NumElements = cap;
	hr = device->CreateShaderResourceView(visible_meshlet_buffer.Get(), &sd, visible_meshlet_srv.ReleaseAndGetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
}

void meshlet_culling_system::build(graphics_context& context, const std::vector<std::size_t>& visible_draw_indices, const std::vector<float>& lengths)
{
	meshlet_instance_count = 0;

	meshlet_instance_scratch.clear();
	submesh_transform_scratch.clear();

	auto& mesh_container = context.render_data->mesh_container;
	auto draw_data_ptr = mesh_container.draw_datas.data();
	auto buffer_data_ptr = mesh_container.buffer_datas.data();

	for (auto& draw_index : visible_draw_indices)
	{
		auto draw_data = draw_data_ptr + draw_index;
		auto& buffer_data = buffer_data_ptr[draw_data->buffer_index];

		//	描画対象のLODメッシュを選択
		//	@remainder	ココがEngine/アプリ側双方で同じコードなのやめた方が良い( ;∀;)
		std::size_t lod_count = buffer_data.color_meshs.size();
		if (lod_count == 0)
			continue;

		std::size_t lod_mesh_index = 0;
		if (lod_count > 1)
		{
			float mesh_length = lengths[draw_index];
			float lod_range_min = buffer_data.lod_range_rate.x * context.scene_constant.camera_clip_distance.x;
			float lod_range_max = buffer_data.lod_range_rate.y * context.scene_constant.camera_clip_distance.y;
			float lod_range = lod_range_max - lod_range_min;
			float lod_rate = (lod_range > 1e-6f) ? math::saturate((mesh_length - lod_range_min) / lod_range) : 0.0f;
			lod_mesh_index = static_cast<std::size_t>(lod_rate * static_cast<float>(lod_count - 1) + 0.5f);
			if (lod_mesh_index >= lod_count)
				lod_mesh_index = lod_count - 1;
		}

		auto& sub_mesh = buffer_data.color_meshs[lod_mesh_index];
		if (!sub_mesh.meshlets || sub_mesh.meshlets->empty())
			continue;

		UINT transform_index = static_cast<UINT>(submesh_transform_scratch.size());
		submesh_transform_scratch.emplace_back(buffer_data.world_transform);

		UINT primitive_index = draw_data->buffer_index;
		UINT no_cull = system_utility::is_bitflag(buffer_data.system_flags, buffer_datas::flags::culling) ? 0 : 1;

		for (auto& meshlet : *sub_mesh.meshlets)
		{
			auto& instance = meshlet_instance_scratch.emplace_back();
			instance.vertex_offset = meshlet.vertex_offset;
			instance.triangle_offset = meshlet.triangle_offset;
			instance.triangle_count = meshlet.triangle_count;
			instance.primitive_index = primitive_index;
			instance.bounds_center = meshlet.bounds_center;
			instance.bounds_radius = meshlet.bounds_radius;
			instance.transform_index = transform_index;
			instance.no_cull = no_cull;
			instance.pipeline_group = 0;
		}
	}

	meshlet_instance_count = static_cast<UINT>(meshlet_instance_scratch.size());
	if (meshlet_instance_count == 0)
		return;

	generate_meshlet_instance_buffer(meshlet_instance_count);
	generate_submesh_transform_buffer(static_cast<UINT>(submesh_transform_scratch.size()));

	auto dc = context.device_context;

	{
		D3D11_BOX box{};
		box.right = static_cast<UINT>(sizeof(meshlet_instance_gpu) * meshlet_instance_scratch.size());
		box.bottom = 1;
		box.back = 1;
		dc->UpdateSubresource(meshlet_instance_buffer.Get(), 0, &box, meshlet_instance_scratch.data(), 0, 0);
	}
	{
		D3D11_BOX box{};
		box.right = static_cast<UINT>(sizeof(DirectX::XMFLOAT4X4) * submesh_transform_scratch.size());
		box.bottom = 1;
		box.back = 1;
		dc->UpdateSubresource(submesh_transform_buffer.Get(), 0, &box, submesh_transform_scratch.data(), 0, 0);
	}
}

void meshlet_culling_system::cull(graphics_context& context, hiz_buffer& hiz)
{
	if (meshlet_instance_count == 0)
		return;

	generate_visible_meshlet_buffer(meshlet_instance_count);

	auto dc = context.device_context;

	cull_constants constant{};
	constant.view_projection = context.scene_constant.view_projection_transform;
	constant.meshlet_instance_count = meshlet_instance_count;
	constant.hiz_mip_count = hiz.get_mip_count();
	auto hiz_size = hiz.get_size();
	constant.hiz_size = DirectX::XMFLOAT2(static_cast<float>(hiz_size.x), static_cast<float>(hiz_size.y));
	constant.clip_near_far = { context.scene_constant.camera_clip_distance.x, context.scene_constant.camera_clip_distance.y };
	constant.depth_bias = 5.0f;
	dc->UpdateSubresource(cull_constant_buffer.Get(), 0, nullptr, &constant, 0, 0);
	dc->CSSetConstantBuffers(0, 1, cull_constant_buffer.GetAddressOf());
	dc->CSSetShader(meshlet_culling_cs->get_shader().Get(), nullptr, 0);

	ID3D11ShaderResourceView* srvs[3] = { meshlet_instance_srv.Get(), submesh_transform_srv.Get(), hiz.get_srv() };
	dc->CSSetShaderResources(0, 3, srvs);
	ID3D11SamplerState* sampler = render_data::instance()->get_sampler_states(render_data::SAMPLER_STATE::POINT_CLAMP).Get();
	dc->CSSetSamplers(0, 1, &sampler);

	UINT initial_count = 0;
	dc->CSSetUnorderedAccessViews(0, 1, visible_meshlet_uav.GetAddressOf(), &initial_count);
	dc->Dispatch((meshlet_instance_count + 63u) / 64u, 1, 1);

	context.clear_unordered_access_views();
	context.clear_shader_resorce_views();
	dc->CSSetShader(nullptr, nullptr, 0);
}

void meshlet_culling_system::copy_visible_count_to(graphics_context& context, ID3D11Buffer* dest_buffer, UINT dest_offset)
{
	if (!visible_meshlet_uav)
		return;
	context.device_context->CopyStructureCount(dest_buffer, dest_offset, visible_meshlet_uav.Get());
}
