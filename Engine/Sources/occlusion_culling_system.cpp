#include "occlusion_culling_system.h"

#include "render_data.h"

namespace
{
	struct instance_aabb_cpu
	{
		DirectX::XMFLOAT3 bmin;
		float testable;
		DirectX::XMFLOAT3 bmax;
		float dummy;
	};

	struct hiz_constants
	{
		DirectX::XMUINT2 dst_size;
		DirectX::XMUINT2 src_size;
	};

	struct cull_constants
	{
		DirectX::XMFLOAT4X4 view_projection;
		UINT instance_count;
		UINT hiz_mip_count;
		DirectX::XMFLOAT2 hiz_size;
		float depth_bias;
		DirectX::XMFLOAT2 clip_near_far;
		float dummy;
	};
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

occlusion_culling_system::occlusion_culling_system()
{
	//	定数バッファ生成
	{
		D3D11_BUFFER_DESC bd{};
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		{
			bd.ByteWidth = sizeof(hiz_constants);
			HRESULT hr = render_data::instance()->get_device()->CreateBuffer(&bd, nullptr, hiz_constant_buffer.GetAddressOf());
			_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
		}
		{
			bd.ByteWidth = sizeof(cull_constants);
			HRESULT hr = render_data::instance()->get_device()->CreateBuffer(&bd, nullptr, cull_constant_buffer.GetAddressOf());
			_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
		}
	}

	hiz_copy_cs = load_cs("Shaders/Culling/hiz_copy_cs.hlsl");
	hiz_downsample_cs = load_cs("Shaders/Culling/hiz_downsample_cs.hlsl");
	occlusion_culling_cs = load_cs("Shaders/Culling/occlusion_culling_cs.hlsl");
}

occlusion_culling_system::~occlusion_culling_system()
{
}

void occlusion_culling_system::resize_buffer(int width, int height)
{
	if (width <= 0 || height <= 0)
		return;
	if (width == last_width && height == last_height && hiz_texture)
		return;
	last_width = width;
	last_height = height;

	auto device = render_data::instance()->get_device();
	HRESULT hr;

	//	Hi-Zミップマップ数算出
	hiz_mip_count = 1;
	{
		int m = (width > height) ? width : height;
		while (m > 1)
		{
			m >>= 1;
			++hiz_mip_count;
		}
	}
	//	16段階以上はしない
	if (hiz_mip_count > 16)
		hiz_mip_count = 16;

	//	Hi-Z用バッファ類生成
	D3D11_TEXTURE2D_DESC td{};
	td.Width = width;
	td.Height = height;
	td.MipLevels = hiz_mip_count;
	td.ArraySize = 1;
	td.Format = DXGI_FORMAT_R32_FLOAT;
	td.SampleDesc.Count = 1;
	td.Usage = D3D11_USAGE_DEFAULT;
	td.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
	hr = device->CreateTexture2D(&td, nullptr, hiz_texture.ReleaseAndGetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	{
		D3D11_SHADER_RESOURCE_VIEW_DESC sd{};
		sd.Format = DXGI_FORMAT_R32_FLOAT;
		sd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		sd.Texture2D.MostDetailedMip = 0;
		sd.Texture2D.MipLevels = hiz_mip_count;
		hr = device->CreateShaderResourceView(hiz_texture.Get(), &sd, hiz_srv.ReleaseAndGetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	}

	hiz_mip_srvs.assign(hiz_mip_count, nullptr);
	hiz_mip_uavs.assign(hiz_mip_count, nullptr);
	hiz_mip_sizes.assign(hiz_mip_count, {});
	for (UINT mip = 0; mip < hiz_mip_count; ++mip)
	{
		UINT mw = width >> mip;  if (mw == 0) mw = 1;
		UINT mh = height >> mip; if (mh == 0) mh = 1;
		hiz_mip_sizes[mip] = { mw, mh };

		D3D11_SHADER_RESOURCE_VIEW_DESC sd{};
		sd.Format = DXGI_FORMAT_R32_FLOAT;
		sd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		sd.Texture2D.MostDetailedMip = mip;
		sd.Texture2D.MipLevels = 1;
		hr = device->CreateShaderResourceView(hiz_texture.Get(), &sd, hiz_mip_srvs[mip].ReleaseAndGetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

		D3D11_UNORDERED_ACCESS_VIEW_DESC ud{};
		ud.Format = DXGI_FORMAT_R32_FLOAT;
		ud.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
		ud.Texture2D.MipSlice = mip;
		hr = device->CreateUnorderedAccessView(hiz_texture.Get(), &ud, hiz_mip_uavs[mip].ReleaseAndGetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	}
}

void occlusion_culling_system::generate_instance_buffers(UINT count)
{
	if (count <= instance_capacity && instance_buffer)
		return;

	UINT cap = instance_capacity ? instance_capacity : 256;
	while (cap < count) cap *= 2;
	instance_capacity = cap;

	auto device = render_data::instance()->get_device();
	HRESULT hr;

	{
		D3D11_BUFFER_DESC bd{};
		bd.ByteWidth = sizeof(instance_aabb_cpu) * cap;
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		bd.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		bd.StructureByteStride = sizeof(instance_aabb_cpu);
		hr = device->CreateBuffer(&bd, nullptr, instance_buffer.ReleaseAndGetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

		D3D11_SHADER_RESOURCE_VIEW_DESC sd{};
		sd.Format = DXGI_FORMAT_UNKNOWN;
		sd.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		sd.Buffer.FirstElement = 0;
		sd.Buffer.NumElements = cap;
		hr = device->CreateShaderResourceView(instance_buffer.Get(), &sd, instance_srv.ReleaseAndGetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	}
	{
		D3D11_BUFFER_DESC bd{};
		bd.ByteWidth = sizeof(UINT) * cap;
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
		bd.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		bd.StructureByteStride = sizeof(UINT);
		hr = device->CreateBuffer(&bd, nullptr, visibility_buffer.ReleaseAndGetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

		D3D11_UNORDERED_ACCESS_VIEW_DESC ud{};
		ud.Format = DXGI_FORMAT_UNKNOWN;
		ud.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
		ud.Buffer.FirstElement = 0;
		ud.Buffer.NumElements = cap;
		hr = device->CreateUnorderedAccessView(visibility_buffer.Get(), &ud, visibility_uav.ReleaseAndGetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	}
	{
		D3D11_BUFFER_DESC bd{};
		bd.ByteWidth = sizeof(UINT) * cap;
		bd.Usage = D3D11_USAGE_STAGING;
		bd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		hr = device->CreateBuffer(&bd, nullptr, visibility_staging.ReleaseAndGetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	}
}

void occlusion_culling_system::update(graphics_context& context, float animation_test_distance)
{
	auto& buffer_datas = context.render_data->mesh_container.buffer_datas;
	UINT count = static_cast<UINT>(buffer_datas.size());

	last_unique_ids.clear();
	last_instance_count = count;
	if (count == 0)
		return;
	if (!hiz_texture || hiz_mip_count == 0)
		return;

	//	GPU側でオクルージョンカリングを行うモデルを積み込む
	std::vector<instance_aabb_cpu> aabbs;
	aabbs.reserve(count);
	last_unique_ids.reserve(count);
	for (auto& bd : buffer_datas)
	{
		instance_aabb_cpu a{};
		a.bmin = bd.bounds_min;
		a.bmax = bd.bounds_max;
		if (system_utility::is_bitflag(bd.system_flags, buffer_datas::flags::static_mesh))
		{
			a.testable = 1.0f;
		}
		else if (animation_test_distance > 0.0f)
		{
			//	アニメーションモデルは一定距離外なら対象に
			float cx = (bd.bounds_min.x + bd.bounds_max.x) * 0.5f;
			float cy = (bd.bounds_min.y + bd.bounds_max.y) * 0.5f;
			float cz = (bd.bounds_min.z + bd.bounds_max.z) * 0.5f;
			float dx = cx - context.scene_constant.camera_position.x;
			float dy = cy - context.scene_constant.camera_position.y;
			float dz = cz - context.scene_constant.camera_position.z;
			a.testable = (dx * dx + dy * dy + dz * dz) >= (animation_test_distance * animation_test_distance) ? 1.0f : 0.0f;
		}
		else
		{
			a.testable = 0.0f;
		}
		aabbs.emplace_back(a);
		last_unique_ids.emplace_back(bd.unique_id);
	}
	generate_instance_buffers(count);

	DirectX::XMFLOAT4X4 view_projection = context.scene_constant.view_projection_transform;

	auto dc = context.device_context;

	//	判定リソース更新
	D3D11_BOX box{};
	box.right = static_cast<UINT>(sizeof(instance_aabb_cpu) * count);
	box.bottom = 1;
	box.back = 1;
	dc->UpdateSubresource(instance_buffer.Get(), 0, &box, aabbs.data(), 0, 0);

	//	Hi-Z 構築
	{
		//	ミップ0生成
		hiz_constants constant{};
		constant.dst_size = hiz_mip_sizes[0];
		constant.src_size = hiz_mip_sizes[0];
		dc->UpdateSubresource(hiz_constant_buffer.Get(), 0, nullptr, &constant, 0, 0);
		dc->CSSetConstantBuffers(0, 1, hiz_constant_buffer.GetAddressOf());
		dc->CSSetShader(hiz_copy_cs->get_shader().Get(), nullptr, 0);
		dc->CSSetShaderResources(0, 1, context.scene_depth_texture->get_shader_resource_view().GetAddressOf());
		dc->CSSetUnorderedAccessViews(0, 1, hiz_mip_uavs[0].GetAddressOf(), nullptr);
		dc->Dispatch((hiz_mip_sizes[0].x + 7) / 8, (hiz_mip_sizes[0].y + 7) / 8, 1);
		context.clear_unordered_access_views();
		context.clear_shader_resorce_views();

		//	ミップ1以降生成
		dc->CSSetShader(hiz_downsample_cs->get_shader().Get(), nullptr, 0);
		for (UINT mip = 1; mip < hiz_mip_count; ++mip)
		{
			hiz_constants constant{};
			constant.dst_size = hiz_mip_sizes[mip];
			constant.src_size = hiz_mip_sizes[mip - 1];
			dc->UpdateSubresource(hiz_constant_buffer.Get(), 0, nullptr, &constant, 0, 0);
			dc->CSSetConstantBuffers(0, 1, hiz_constant_buffer.GetAddressOf());
			dc->CSSetShaderResources(0, 1, hiz_mip_srvs[mip - 1].GetAddressOf());
			dc->CSSetUnorderedAccessViews(0, 1, hiz_mip_uavs[mip].GetAddressOf(), nullptr);
			dc->Dispatch((hiz_mip_sizes[mip].x + 7) / 8, (hiz_mip_sizes[mip].y + 7) / 8, 1);
			context.clear_unordered_access_views();
			context.clear_shader_resorce_views();
		}
	}

	//	オクルージョン判定
	{
		cull_constants constant{};
		constant.view_projection = view_projection;
		constant.instance_count = count;
		constant.hiz_mip_count = hiz_mip_count;
		constant.hiz_size = DirectX::XMFLOAT2(static_cast<float>(hiz_mip_sizes[0].x), static_cast<float>(hiz_mip_sizes[0].y));
		constant.clip_near_far = { context.scene_constant.camera_clip_distance.x, context.scene_constant.camera_clip_distance.y };
		constant.depth_bias = 5.0f;
		dc->UpdateSubresource(cull_constant_buffer.Get(), 0, nullptr, &constant, 0, 0);
		dc->CSSetConstantBuffers(0, 1, cull_constant_buffer.GetAddressOf());
		dc->CSSetShader(occlusion_culling_cs->get_shader().Get(), nullptr, 0);

		ID3D11ShaderResourceView* srvs[2] = { instance_srv.Get(), hiz_srv.Get() };
		dc->CSSetShaderResources(0, 2, srvs);
		ID3D11SamplerState* sampler = render_data::instance()->get_sampler_states(render_data::SAMPLER_STATE::POINT_CLAMP).Get();
		dc->CSSetSamplers(0, 1, &sampler);
		dc->CSSetUnorderedAccessViews(0, 1, visibility_uav.GetAddressOf(), nullptr);
		dc->Dispatch((count + 63u) / 64u, 1, 1);
		context.clear_unordered_access_views();
		context.clear_shader_resorce_views();
	}

	dc->CSSetShader(nullptr, nullptr, 0);
	dc->CopyResource(visibility_staging.Get(), visibility_buffer.Get());
}

void occlusion_culling_system::read_last_frame(std::unordered_map<UINT, uint8_t>& out_visibility)
{
	if (last_instance_count == 0 || !visibility_staging)
		return;

	auto dc = render_data::instance()->get_immediate_context();
	D3D11_MAPPED_SUBRESOURCE mapped{};
	if (FAILED(dc->Map(visibility_staging.Get(), 0, D3D11_MAP_READ, 0, &mapped)))
		return;

	const UINT* v = static_cast<const UINT*>(mapped.pData);
	UINT n = last_instance_count;
	if (n > static_cast<UINT>(last_unique_ids.size())) n = static_cast<UINT>(last_unique_ids.size());
	for (UINT i = 0; i < n; ++i)
	{
		out_visibility[last_unique_ids[i]] = (v[i] != 0) ? uint8_t(1) : uint8_t(0);
	}

	dc->Unmap(visibility_staging.Get(), 0);
}
