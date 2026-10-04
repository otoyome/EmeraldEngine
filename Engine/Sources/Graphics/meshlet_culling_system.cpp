#include "graphics/meshlet_culling_system.h"

#include "render_data.h"
#include "Math/math_all.h"

namespace
{
	static constexpr UINT ThreadCount = 64;
	std::shared_ptr<compute_shader> load_cs(std::string_view path)
	{
		load_shader_file desc;
		desc.file_path = path;
#if defined(_DEBUG)
		system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::disable_optimize, true);
		system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::debug, true);
#endif	//	defined(_DEBUG)
		std::shared_ptr<compute_shader> shader;
		shader_manager::instance()->load_shader<compute_shader>(shader, desc);
		return shader;
	}
}

meshlet_culling_system::meshlet_culling_system()
{
	meshlet_culling_cs = load_cs("Shaders/Culling/meshlet_culling_cs.hlsl");

	D3D11_BUFFER_DESC bd{};
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bd.ByteWidth = sizeof(cull_constants);
	HRESULT hr = render_data::instance()->get_device()->CreateBuffer(&bd, nullptr, cull_constant_buffer.GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
}

meshlet_culling_system::~meshlet_culling_system()
{
}

void meshlet_culling_system::generate_submesh_transform_buffer(UINT count)
{
	if (count <= submesh_transform_capacity && submesh_transform_buffer)
		return;

	UINT capacity = submesh_transform_capacity ? submesh_transform_capacity : 256;
	while (capacity < count)
		capacity *= 2;
	submesh_transform_capacity = capacity;

	auto device = render_data::instance()->get_device();

	D3D11_BUFFER_DESC bd{};
	bd.ByteWidth = sizeof(math::float4x4) * capacity;
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	bd.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	bd.StructureByteStride = sizeof(math::float4x4);
	HRESULT hr = device->CreateBuffer(&bd, nullptr, submesh_transform_buffer.ReleaseAndGetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	D3D11_SHADER_RESOURCE_VIEW_DESC sd{};
	sd.Format = DXGI_FORMAT_UNKNOWN;
	sd.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	sd.Buffer.FirstElement = 0;
	sd.Buffer.NumElements = capacity;
	hr = device->CreateShaderResourceView(submesh_transform_buffer.Get(), &sd, submesh_transform_srv.ReleaseAndGetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
}

void meshlet_culling_system::generate_meshlet_instance_buffer(material_resources& res, UINT count)
{
	if (count <= res.meshlet_instance_capacity && res.meshlet_instance_buffer)
		return;

	UINT capacity = res.meshlet_instance_capacity ? res.meshlet_instance_capacity : 256;
	while (capacity < count)
		capacity *= 2;
	res.meshlet_instance_capacity = capacity;

	auto device = render_data::instance()->get_device();

	D3D11_BUFFER_DESC bd{};
	bd.ByteWidth = sizeof(meshlet_instance_gpu) * capacity;
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	bd.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	bd.StructureByteStride = sizeof(meshlet_instance_gpu);
	HRESULT hr = device->CreateBuffer(&bd, nullptr, res.meshlet_instance_buffer.ReleaseAndGetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	D3D11_SHADER_RESOURCE_VIEW_DESC sd{};
	sd.Format = DXGI_FORMAT_UNKNOWN;
	sd.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	sd.Buffer.FirstElement = 0;
	sd.Buffer.NumElements = capacity;
	hr = device->CreateShaderResourceView(res.meshlet_instance_buffer.Get(), &sd, res.meshlet_instance_srv.ReleaseAndGetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
}

void meshlet_culling_system::generate_visible_meshlet_buffer(material_resources& res, UINT count)
{
	if (count <= res.visible_meshlet_capacity && res.visible_meshlet_buffer)
		return;

	UINT capacity = res.visible_meshlet_capacity ? res.visible_meshlet_capacity : 256;
	while (capacity < count)
		capacity *= 2;
	res.visible_meshlet_capacity = capacity;

	auto device = render_data::instance()->get_device();

	D3D11_BUFFER_DESC bd{};
	bd.ByteWidth = sizeof(meshlet_instance_gpu) * capacity;
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
	bd.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	bd.StructureByteStride = sizeof(meshlet_instance_gpu);
	HRESULT hr = device->CreateBuffer(&bd, nullptr, res.visible_meshlet_buffer.ReleaseAndGetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	D3D11_UNORDERED_ACCESS_VIEW_DESC ud{};
	ud.Format = DXGI_FORMAT_UNKNOWN;
	ud.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	ud.Buffer.FirstElement = 0;
	ud.Buffer.NumElements = capacity;
	ud.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_APPEND;
	hr = device->CreateUnorderedAccessView(res.visible_meshlet_buffer.Get(), &ud, res.visible_meshlet_uav.ReleaseAndGetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	D3D11_SHADER_RESOURCE_VIEW_DESC sd{};
	sd.Format = DXGI_FORMAT_UNKNOWN;
	sd.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	sd.Buffer.FirstElement = 0;
	sd.Buffer.NumElements = capacity;
	hr = device->CreateShaderResourceView(res.visible_meshlet_buffer.Get(), &sd, res.visible_meshlet_srv.ReleaseAndGetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	if (!res.indirect_args_buffer)
	{
		D3D11_BUFFER_DESC ibd{};
		ibd.ByteWidth = sizeof(UINT) * 4;
		ibd.Usage = D3D11_USAGE_DEFAULT;
		ibd.MiscFlags = D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS;
		hr = device->CreateBuffer(&ibd, nullptr, res.indirect_args_buffer.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	}
}

void meshlet_culling_system::build_and_cull(graphics_context& context, const std::vector<std::size_t>& visible_draw_indices, const std::vector<float>& lengths, hiz_buffer& hiz)
{
	current_results.clear();
	submesh_transform_scratch.clear();
	material_scratch_buffer.clear();

	auto& mesh_container = context.render_data->mesh_container;
	auto draw_data_ptr = mesh_container.draw_datas.data();
	auto buffer_data_ptr = mesh_container.buffer_datas.data();
	auto material_data_ptr = mesh_container.material_datas.data();

	//	マテリアルごとにメッシュレットをグルーピング
	for (auto& draw_index : visible_draw_indices)
	{
		auto draw_data = draw_data_ptr + draw_index;
		auto& buffer_data = buffer_data_ptr[draw_data->buffer_index];
		auto material_data = material_data_ptr + draw_data->material_index;

		//	描画対象のLODメッシュを選択
		if (buffer_data.color_meshs.empty())
			continue;

		std::size_t lod_mesh_index = buffer_data.select_lod_mesh_index(lengths[draw_index], context.scene_constant.camera_clip_distance);
		auto& sub_mesh = buffer_data.color_meshs[lod_mesh_index];
		if (!sub_mesh.meshlets || sub_mesh.meshlets->empty())
			continue;

		//	モーフィング持ちは別で処理
		if (!sub_mesh.morph_meshes.empty())
			continue;

		//	サブメッシュのワールド行列を設定
		UINT transform_index = static_cast<UINT>(submesh_transform_scratch.size());
		submesh_transform_scratch.emplace_back(buffer_data.world_transform);

		//	カリング判定
		UINT no_cull = system_utility::is_bitflag(buffer_data.system_flags, buffer_datas::flags::culling) ? 0 : 1;

		//	メッシュレット分登録
		UINT primitive_index = draw_data->buffer_index;
		auto& instances = material_scratch_buffer[material_data->material];
		for (auto& meshlet : *sub_mesh.meshlets)
		{
			auto& instance = instances.emplace_back();
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

	if (submesh_transform_scratch.empty())
		return;

	//	サブメッシュの変換行列を登録
	generate_submesh_transform_buffer(static_cast<UINT>(submesh_transform_scratch.size()));
	{
		D3D11_BOX box{};
		box.right = static_cast<UINT>(sizeof(math::float4x4) * submesh_transform_scratch.size());
		box.bottom = 1;
		box.back = 1;
		context.device_context->UpdateSubresource(submesh_transform_buffer.Get(), 0, &box, submesh_transform_scratch.data(), 0, 0);
	}

	//	マテリアルグループ毎にカリング
	for (auto& group : material_scratch_buffer)
	{
		cull_material_group(context, group.first, group.second, hiz);
	}
}

void meshlet_culling_system::cull_material_group(graphics_context& context, material* material, const std::vector<meshlet_instance_gpu>& instances, hiz_buffer& hiz)
{
	if (instances.empty())
		return;

	auto dc = context.device_context;

	//	マテリアル単位のリソース取得
	material_resources& resource = material_resource_map[material];
	UINT instance_count = static_cast<UINT>(instances.size());
	generate_meshlet_instance_buffer(resource, instance_count);
	generate_visible_meshlet_buffer(resource, instance_count);

	//	メッシュレットのインデックス情報転送
	{
		D3D11_BOX box{};
		box.right = static_cast<UINT>(sizeof(meshlet_instance_gpu) * instances.size());
		box.bottom = 1;
		box.back = 1;
		dc->UpdateSubresource(resource.meshlet_instance_buffer.Get(), 0, &box, instances.data(), 0, 0);
	}

	//	カリング設定
	cull_constants constant{};
	constant.view_projection = context.scene_constant.view_projection_transform;
	constant.meshlet_instance_count = instance_count;
	constant.hiz_mip_count = hiz.get_mip_count();
	auto hiz_size = hiz.get_size();
	constant.hiz_size = math::float2(static_cast<float>(hiz_size.x), static_cast<float>(hiz_size.y));
	constant.clip_near_far = { context.scene_constant.camera_clip_distance.x, context.scene_constant.camera_clip_distance.y };
	constant.depth_bias = 1.0f;
	dc->UpdateSubresource(cull_constant_buffer.Get(), 0, nullptr, &constant, 0, 0);
	dc->CSSetConstantBuffers(0, 1, cull_constant_buffer.GetAddressOf());
	dc->CSSetShader(meshlet_culling_cs->get_shader().Get(), nullptr, 0);

	ID3D11ShaderResourceView* srvs[3] = { resource.meshlet_instance_srv.Get(), submesh_transform_srv.Get(), hiz.get_srv() };
	dc->CSSetShaderResources(0, 3, srvs);
	dc->CSSetSamplers(0, 1, render_data::instance()->get_sampler_states(render_data::SAMPLER_STATE::POINT_CLAMP).GetAddressOf());

	//	カリング処理
	UINT initial_count = 0;
	dc->CSSetUnorderedAccessViews(0, 1, resource.visible_meshlet_uav.GetAddressOf(), &initial_count);
	dc->Dispatch((instance_count + ThreadCount - 1) / ThreadCount, 1, 1);

	//	結果を取得して保存
	UINT arguments[4] = { max_meshlet_vertices_per_draw, 0, 0, 0 };
	dc->UpdateSubresource(resource.indirect_args_buffer.Get(), 0, nullptr, arguments, 0, 0);
	dc->CopyStructureCount(resource.indirect_args_buffer.Get(), sizeof(UINT), resource.visible_meshlet_uav.Get());
	cull_result result{};
	result.visible_meshlet_srv = resource.visible_meshlet_srv.Get();
	result.indirect_args_buffer = resource.indirect_args_buffer.Get();
	current_results[material] = result;

	context.clear_unordered_access_views();
	context.clear_shader_resorce_views();
	dc->CSSetShader(nullptr, nullptr, 0);
}

const meshlet_culling_system::cull_result* meshlet_culling_system::find_result(material* mat) const
{
	auto it = current_results.find(mat);
	if (it == current_results.end())
		return nullptr;
	return &it->second;
}
