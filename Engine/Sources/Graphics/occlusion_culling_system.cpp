#include "graphics/occlusion_culling_system.h"

#include "render_data.h"

namespace
{
	struct instance_aabb_cpu
	{
		math::float3 bmin;
		float testable;	//	1:判定対象 0:常に可視
		math::float3 bmax;
		float dummy;
	};

	struct cull_constants
	{
		math::float4x4 view_projection;
		UINT instance_count;
		UINT hiz_mip_count;
		math::float2 hiz_size;
		float depth_bias;
		math::float2 clip_near_far;
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
		bd.ByteWidth = sizeof(cull_constants);
		HRESULT hr = render_data::instance()->get_device()->CreateBuffer(&bd, nullptr, cull_constant_buffer.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	}

	occlusion_culling_cs = load_cs("Shaders/Culling/occlusion_culling_cs.hlsl");
}

occlusion_culling_system::~occlusion_culling_system()
{
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
	//	@tool 判定結果の読み戻し先（大きさが変わったら作り直す。読み戻し中の組は要求が持っている）
	visibility_readback.initialize(device.Get(), { static_cast<UINT>(sizeof(UINT) * cap) }, 3, 8);
}

void occlusion_culling_system::update(render_job_context& context, hiz_buffer& hiz)
{
	auto& draw_items = context.render_data->mesh_container.draw_items;
	UINT count = static_cast<UINT>(draw_items.size());

	last_unique_ids.clear();
	last_instance_count = count;
	if (count == 0)
		return;
	if (!hiz.get_srv() || hiz.get_mip_count() == 0)
		return;

	//	GPU側でオクルージョンカリングを行う対象を積み込む
	std::vector<instance_aabb_cpu> aabbs;
	aabbs.reserve(count);
	last_unique_ids.reserve(count);
	for (auto& bd : draw_items)
	{
		instance_aabb_cpu a{};
		a.bmin = bd.bounds_min;
		a.bmax = bd.bounds_max;
		a.testable = system_utility::is_bitflag(bd.system_flags, mesh_draw_item::flags::culling) ? 1.0f : 0.0f;
		aabbs.emplace_back(a);
		last_unique_ids.emplace_back(bd.unique_id);
	}
	generate_instance_buffers(count);

	math::float4x4 view_projection = context.scene_constant.view_projection_transform;

	const auto& recorder = context.recorder;

	//	判定リソース更新
	D3D11_BOX box{};
	box.right = static_cast<UINT>(sizeof(instance_aabb_cpu) * count);
	box.bottom = 1;
	box.back = 1;
	recorder.update_subresource(instance_buffer.Get(), 0, &box, aabbs.data(), 0, 0);

	//	オクルージョン判定
	{
		auto hiz_size = hiz.get_size();
		cull_constants constant{};
		constant.view_projection = view_projection;
		constant.instance_count = count;
		constant.hiz_mip_count = hiz.get_mip_count();
		constant.hiz_size = math::float2(static_cast<float>(hiz_size.x), static_cast<float>(hiz_size.y));
		constant.clip_near_far = { context.scene_constant.camera_clip_distance.x, context.scene_constant.camera_clip_distance.y };
		constant.depth_bias = 5.0f;
		recorder.update_subresource(cull_constant_buffer.Get(), 0, nullptr, &constant, 0, 0);
		recorder.set_constant_buffers(shader_stage::compute, 0, 1, cull_constant_buffer.GetAddressOf());
		recorder.set_compute_shader(occlusion_culling_cs->get_shader().Get());

		ID3D11ShaderResourceView* srvs[2] = { instance_srv.Get(), hiz.get_srv() };
		recorder.set_shader_resource_views(shader_stage::compute, 0, 2, srvs);
		ID3D11SamplerState* sampler = render_data::instance()->get_sampler_states(render_data::SAMPLER_STATE::POINT_CLAMP).Get();
		recorder.set_samplers(shader_stage::compute, 0, 1, &sampler);
		recorder.set_unordered_access_views(0, 1, visibility_uav.GetAddressOf());
		recorder.dispatch((count + 63u) / 64u, 1, 1);
		context.recorder.clear_unordered_access_views();
		context.recorder.clear_shader_resorce_views();
	}

	recorder.set_compute_shader(nullptr);
	//	@tool 判定結果を読み戻す（どのインスタンスの結果かは記録時の番号表で決める）
	recorder.read_back(visibility_readback, { { visibility_buffer.Get(), nullptr } },
		[received = received_visibility, ids = last_unique_ids, count](const gpu_readback_data& data)
		{
			const UINT* visible = data.get<UINT>(0);
			UINT n = (std::min)(count, static_cast<UINT>(ids.size()));
			n = (std::min)(n, data.size(0) / static_cast<UINT>(sizeof(UINT)));
			std::lock_guard<std::mutex> lock(received->mutex);
			received->visibility.clear();
			for (UINT i = 0; i < n; ++i)
				received->visibility.emplace_back(ids[i], visible[i] != 0 ? uint8_t(1) : uint8_t(0));
		});
}

void occlusion_culling_system::read_last_frame(std::unordered_map<UINT, uint8_t>& out_visibility)
{
	//	@tool 最後に届いた判定結果を使う（記録から 1～2 フレーム遅れ。待たない）
	std::lock_guard<std::mutex> lock(received_visibility->mutex);
	for (const auto& [unique_id, visible] : received_visibility->visibility)
		out_visibility[unique_id] = visible;
}
