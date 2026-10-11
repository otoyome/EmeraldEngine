#include "graphics/clustered_lighting_system.h"

#include "render_data.h"
#include "light.h"
#include "System/console_manager.h"
#include "System/system_units.h"

#include <cmath>
#include <limits>

namespace
{
	//	コンソール変数
#if defined(USE_IMGUI)
	console_variable<bool> enable_variable("r.clustered.enable", true, "影無しの点光源/スポットライトをクラスタードライティングで描画する");
	console_variable<float> far_variable("r.clustered.far", system_units::convert_m_to_unit(500.0f), "クラスターの奥行き分割を行う最大距離（以降は最後の分割にまとめる）");
	bool get_enable() { return enable_variable.get(); }
	float get_far() { return far_variable.get(); }
#else	//	defined(USE_IMGUI)
	bool get_enable() { return true; }
	float get_far() { return system_units::convert_m_to_unit(500.0f); }
#endif	//	defined(USE_IMGUI)

	//	クラスター分割
	constexpr UINT tile_size = 64;		//	タイルの大きさ（ピクセル）
	constexpr UINT slice_count = 24;	//	奥行き分割数

	//	ディスパッチ
	constexpr UINT bounds_thread_count = 64;
	constexpr UINT dispatch_width = 65535;

	//	ライトリストの初期容量
	constexpr UINT initial_index_capacity = 1u << 18;

	//	シェーダーのリソース番号
	constexpr UINT gbuffer_srv_index = 0;
	constexpr UINT clustered_cbv_index = 7;
	constexpr UINT clustered_srv_index = 40;

	//	クラスターAABB
	struct cluster_aabb
	{
		math::float3 bmin;
		float dummy0;
		math::float3 bmax;
		float dummy1;
	};

	std::shared_ptr<compute_shader> load_cs(std::string_view path, bool fill)
	{
		load_shader_file desc{};
		desc.file_path = path;
		if (fill)
			desc.defines.emplace_back("CLUSTERED_FILL", "1");
#if defined(_DEBUG)
		system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::disable_optimize, true);
		system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::debug, true);
#endif	//	defined(_DEBUG)
		std::shared_ptr<compute_shader> shader;
		shader_manager::instance()->load_shader<compute_shader>(shader, desc);
		return shader;
	}

	//	バッファ生成
	void create_structured_buffer(UINT stride, UINT count, UINT bind_flags,
		Microsoft::WRL::ComPtr<ID3D11Buffer>& buffer,
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>* srv,
		Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView>* uav)
	{
		auto device = render_data::instance()->get_device();
		HRESULT hr;

		D3D11_BUFFER_DESC bd{};
		bd.ByteWidth = stride * count;
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.BindFlags = bind_flags;
		bd.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		bd.StructureByteStride = stride;
		hr = device->CreateBuffer(&bd, nullptr, buffer.ReleaseAndGetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

		if (srv)
		{
			D3D11_SHADER_RESOURCE_VIEW_DESC sd{};
			sd.Format = DXGI_FORMAT_UNKNOWN;
			sd.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
			sd.Buffer.FirstElement = 0;
			sd.Buffer.NumElements = count;
			hr = device->CreateShaderResourceView(buffer.Get(), &sd, srv->ReleaseAndGetAddressOf());
			_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
		}
		if (uav)
		{
			D3D11_UNORDERED_ACCESS_VIEW_DESC ud{};
			ud.Format = DXGI_FORMAT_UNKNOWN;
			ud.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
			ud.Buffer.FirstElement = 0;
			ud.Buffer.NumElements = count;
			hr = device->CreateUnorderedAccessView(buffer.Get(), &ud, uav->ReleaseAndGetAddressOf());
			_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
		}
	}

	//	count 以上の2のべき乗
	UINT round_up_power_of_two(UINT count, UINT minimum)
	{
		UINT ret = minimum;
		while (ret < count)
			ret *= 2;
		return ret;
	}
}

clustered_lighting_system::clustered_lighting_system()
{
	auto device = render_data::instance()->get_device();
	HRESULT hr;

	bounds_cs = load_cs("Shaders/ClusteredLighting/clustered_lighting_bounds_cs.hlsl", false);
	count_cs = load_cs("Shaders/ClusteredLighting/clustered_lighting_assign_cs.hlsl", false);
	fill_cs = load_cs("Shaders/ClusteredLighting/clustered_lighting_assign_cs.hlsl", true);
	scan_cs = load_cs("Shaders/ClusteredLighting/clustered_lighting_scan_cs.hlsl", false);
	{
		load_shader_cso desc;
		desc.file_path = "Assets/CSO/fullscreen_quad_vs.cso";
		shader_manager::instance()->load_shader<vertex_shader>(fullscreen_vs, desc);
	}
	{
		load_shader_file desc{};
		desc.file_path = "Shaders/ClusteredLighting/deferred_rendering_clustered_ps.hlsl";
#if defined(_DEBUG)
		system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::disable_optimize, true);
		system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::debug, true);
#endif	//	defined(_DEBUG)
		shader_manager::instance()->load_shader<pixel_shader>(lighting_ps, desc);
	}

	//	定数バッファ
	{
		D3D11_BUFFER_DESC bd{};
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		bd.ByteWidth = sizeof(clustered_constants);
		D3D11_SUBRESOURCE_DATA initial_data{};
		initial_data.pSysMem = &constants;
		hr = device->CreateBuffer(&bd, &initial_data, constant_buffer.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	}

	//	ライト番号総数の読み戻し用
	{
		create_structured_buffer(sizeof(UINT), 1, D3D11_BIND_UNORDERED_ACCESS, statistics_buffer, nullptr, &statistics_uav);
		for (auto& staging : statistics_staging)
		{
			D3D11_BUFFER_DESC bd{};
			bd.ByteWidth = sizeof(UINT);
			bd.Usage = D3D11_USAGE_STAGING;
			bd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
			hr = device->CreateBuffer(&bd, nullptr, staging.GetAddressOf());
			_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
		}
	}

	//	ステート
	{
		D3D11_BLEND_DESC blend_desc{};
		blend_desc.RenderTarget[0].BlendEnable = TRUE;
		blend_desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
		blend_desc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
		blend_desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		blend_desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ZERO;
		blend_desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
		blend_desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		hr = device->CreateBlendState(&blend_desc, blend_state.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

		D3D11_DEPTH_STENCIL_DESC depth_stencil_desc{};
		depth_stencil_desc.DepthEnable = FALSE;
		hr = device->CreateDepthStencilState(&depth_stencil_desc, depth_stencil_state.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

		D3D11_RASTERIZER_DESC rasterizer_desc{};
		rasterizer_desc.FillMode = D3D11_FILL_SOLID;
		rasterizer_desc.CullMode = D3D11_CULL_NONE;
		rasterizer_desc.FrontCounterClockwise = TRUE;
		rasterizer_desc.DepthClipEnable = TRUE;
		hr = device->CreateRasterizerState(&rasterizer_desc, rasterizer_state.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	}

	generate_index_buffer(initial_index_capacity);
}

clustered_lighting_system::~clustered_lighting_system()
{
}

bool clustered_lighting_system::is_enable()
{
	return get_enable();
}

bool clustered_lighting_system::is_target(const light_render_data& light)
{
	if (!is_enable())
		return false;
	//	点光源とスポットライト
	if (light.type != light_type::point && light.type != light_type::spot)
		return false;

	//	影を生成しないオブジェクト限定
	return light.is_flag(light::flags::active) && !light.is_flag(light::flags::use_shadow) && light.range > 0.0f;
}

void clustered_lighting_system::generate_light_buffer(UINT count)
{
	if (count <= light_capacity && light_buffer)
		return;

	light_capacity = round_up_power_of_two(count, 64);
	create_structured_buffer(sizeof(light_data), light_capacity, D3D11_BIND_SHADER_RESOURCE, light_buffer, &light_srv, nullptr);
}

void clustered_lighting_system::generate_cluster_buffers(UINT count)
{
	if (count <= cluster_capacity && bounds_buffer)
		return;

	cluster_capacity = round_up_power_of_two(count, 1024);
	const UINT read_write = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
	create_structured_buffer(sizeof(cluster_aabb), cluster_capacity, read_write, bounds_buffer, &bounds_srv, &bounds_uav);
	create_structured_buffer(sizeof(UINT), cluster_capacity, read_write, count_buffer, &count_srv, &count_uav);
	create_structured_buffer(sizeof(UINT), cluster_capacity, read_write, offset_buffer, &offset_srv, &offset_uav);
	create_structured_buffer(sizeof(UINT), cluster_capacity, D3D11_BIND_UNORDERED_ACCESS, cursor_buffer, nullptr, &cursor_uav);
}

void clustered_lighting_system::generate_index_buffer(UINT capacity)
{
	if (capacity <= index_capacity && index_buffer)
		return;

	index_capacity = round_up_power_of_two(capacity, initial_index_capacity);
	create_structured_buffer(sizeof(UINT), index_capacity, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, index_buffer, &index_srv, &index_uav);
}

void clustered_lighting_system::read_statistics()
{
	auto dc = render_data::instance()->get_immediate_context();
	for (UINT index = 0; index < readback_count; ++index)
	{
		if (!statistics_pending[index])
			continue;

		D3D11_MAPPED_SUBRESOURCE mapped{};
		if (FAILED(dc->Map(statistics_staging[index].Get(), 0, D3D11_MAP_READ, D3D11_MAP_FLAG_DO_NOT_WAIT, &mapped)))
			continue;

		required_index_count = (std::max)(required_index_count, *static_cast<const UINT*>(mapped.pData));
		dc->Unmap(statistics_staging[index].Get(), 0);
		statistics_pending[index] = false;
	}
}

void clustered_lighting_system::prepare(const render_data_context& render_data, const common_pipeline_states::scene_constants& scene)
{
	//	ライト収集
	lights.clear();
	for (const auto& light : render_data.light_container.lights)
	{
		if (is_target(light))
			lights.emplace_back(light.data);
	}
	light_count = static_cast<UINT>(lights.size());

	//	クラスター数算出
	const float screen_width = (std::max)(scene.viewport_rect.z - scene.viewport_rect.x, 1.0f);
	const float screen_height = (std::max)(scene.viewport_rect.w - scene.viewport_rect.y, 1.0f);
	const math::uint3 cluster_count(
		(static_cast<UINT>(screen_width) + tile_size - 1) / tile_size,
		(static_cast<UINT>(screen_height) + tile_size - 1) / tile_size,
		slice_count);
	cluster_total = cluster_count.x * cluster_count.y * cluster_count.z;

	//	バッファ準備
	if (light_count > 0)
	{
		generate_light_buffer(light_count);
		generate_cluster_buffers(cluster_total);
		if (required_index_count > index_capacity)
			generate_index_buffer(required_index_count + required_index_count / 4);
	}

	//	奥行き分割距離算出
	const float cluster_near = (std::max)(scene.camera_clip_distance.x, 1.0e-4f);
	float cluster_far = get_far();
	if (std::isfinite(scene.camera_clip_distance.y))
		cluster_far = (std::min)(cluster_far, scene.camera_clip_distance.y);
	cluster_far = (std::max)(cluster_far, cluster_near * 2.0f);

	constants = {};
	constants.cluster_count = cluster_count;
	constants.light_count = light_count;
	constants.screen_size = math::float2(screen_width, screen_height);
	constants.tile_size = static_cast<float>(tile_size);
	constants.index_capacity = index_capacity;
	constants.cluster_near = cluster_near;
	constants.cluster_far = cluster_far;
	constants.slice_scale = static_cast<float>(slice_count) / std::log(cluster_far / cluster_near);
	constants.slice_bias = -std::log(cluster_near) * constants.slice_scale;
}

void clustered_lighting_system::build(graphics_context& context)
{
	auto dc = context.device_context;

	//	ライトが無くてもフォワード描画がライト数を参照できるよう定数は毎回更新する
	dc->UpdateSubresource(constant_buffer.Get(), 0, nullptr, &constants, 0, 0);
	if (light_count == 0)
		return;

	context.debug_begin_event(L"clustered_lighting_build");

	{
		D3D11_BOX box{};
		box.right = static_cast<UINT>(sizeof(light_data) * light_count);
		box.bottom = 1;
		box.back = 1;
		dc->UpdateSubresource(light_buffer.Get(), 0, &box, lights.data(), 0, 0);
	}

	ID3D11Buffer* constant_buffers[] = { context.scene_constant_buffer.Get() };
	dc->CSSetConstantBuffers(common_pipeline_states::scene_cbv_index, 1, constant_buffers);
	dc->CSSetConstantBuffers(clustered_cbv_index, 1, constant_buffer.GetAddressOf());

	const UINT light_group_x = (std::min)(light_count, dispatch_width);
	const UINT light_group_y = (light_count + dispatch_width - 1) / dispatch_width;

	//	クラスターの範囲
	//  クラスターごとの view 空間 AABB を求める
	{
		dc->CSSetShader(bounds_cs->get_shader().Get(), nullptr, 0);
		dc->CSSetUnorderedAccessViews(0, 1, bounds_uav.GetAddressOf(), nullptr);
		dc->Dispatch((cluster_total + bounds_thread_count - 1) / bounds_thread_count, 1, 1);
		context.clear_unordered_access_views();
	}

	//	クラスターごとのライト数を数える
	{
		const UINT zero[4] = {};
		dc->ClearUnorderedAccessViewUint(count_uav.Get(), zero);
		dc->ClearUnorderedAccessViewUint(cursor_uav.Get(), zero);

		ID3D11ShaderResourceView* srvs[] = { light_srv.Get(), bounds_srv.Get() };
		dc->CSSetShader(count_cs->get_shader().Get(), nullptr, 0);
		dc->CSSetShaderResources(0, ARRAYSIZE(srvs), srvs);
		dc->CSSetUnorderedAccessViews(0, 1, count_uav.GetAddressOf(), nullptr);
		dc->Dispatch(light_group_x, light_group_y, 1);
		context.clear_unordered_access_views();
		context.clear_shader_resorce_views();
	}

	//	↑で数えたので書き込み開始位置を決める
	{
		ID3D11UnorderedAccessView* uavs[] = { offset_uav.Get(), statistics_uav.Get() };
		dc->CSSetShader(scan_cs->get_shader().Get(), nullptr, 0);
		dc->CSSetShaderResources(0, 1, count_srv.GetAddressOf());
		dc->CSSetUnorderedAccessViews(0, ARRAYSIZE(uavs), uavs, nullptr);
		dc->Dispatch(1, 1, 1);
		context.clear_unordered_access_views();
		context.clear_shader_resorce_views();
	}

	//	ライト番号を書き込む
	{
		ID3D11ShaderResourceView* srvs[] = { light_srv.Get(), bounds_srv.Get(), offset_srv.Get() };
		ID3D11UnorderedAccessView* uavs[] = { cursor_uav.Get(), index_uav.Get() };
		dc->CSSetShader(fill_cs->get_shader().Get(), nullptr, 0);
		dc->CSSetShaderResources(0, ARRAYSIZE(srvs), srvs);
		dc->CSSetUnorderedAccessViews(0, ARRAYSIZE(uavs), uavs, nullptr);
		dc->Dispatch(light_group_x, light_group_y, 1);
		context.clear_unordered_access_views();
		context.clear_shader_resorce_views();
	}

	dc->CSSetShader(nullptr, nullptr, 0);
	context.clear_constant_buffers();

	//	ライト番号総数をコピー
	dc->CopyResource(statistics_staging[readback_index].Get(), statistics_buffer.Get());
	statistics_pending[readback_index] = true;
	readback_index = (readback_index + 1) % readback_count;

	context.debug_end_event();
}

void clustered_lighting_system::render(graphics_context& context)
{
	if (light_count == 0)
		return;

	auto dc = context.device_context;
	context.debug_begin_event(L"clustered_lighting");

	dc->VSSetShader(fullscreen_vs->get_shader().Get(), nullptr, 0);
	dc->HSSetShader(nullptr, nullptr, 0);
	dc->DSSetShader(nullptr, nullptr, 0);
	dc->GSSetShader(nullptr, nullptr, 0);
	dc->PSSetShader(lighting_ps->get_shader().Get(), nullptr, 0);
	dc->IASetInputLayout(fullscreen_vs->get_input_layout().Get());
	dc->OMSetBlendState(blend_state.Get(), nullptr, 0xFFFFFFFF);
	dc->OMSetDepthStencilState(depth_stencil_state.Get(), 0);
	dc->RSSetState(rasterizer_state.Get());

	dc->VSSetConstantBuffers(common_pipeline_states::scene_cbv_index, 1, context.scene_constant_buffer.GetAddressOf());
	dc->PSSetConstantBuffers(common_pipeline_states::scene_cbv_index, 1, context.scene_constant_buffer.GetAddressOf());
	dc->PSSetConstantBuffers(clustered_cbv_index, 1, constant_buffer.GetAddressOf());

	ID3D11ShaderResourceView* g_buffer_srvs[static_cast<UINT>(common_pipeline_states::gbuffer_id::max)];
	for (UINT index = 0; index < static_cast<UINT>(common_pipeline_states::gbuffer_id::max); ++index)
	{
		g_buffer_srvs[index] = context.g_buffer_textures[index]->get_shader_resource_view().Get();
	}
	dc->PSSetShaderResources(gbuffer_srv_index, ARRAYSIZE(g_buffer_srvs), g_buffer_srvs);
	dc->PSSetSamplers(gbuffer_srv_index, 1, render_data::instance()->get_sampler_states(render_data::SAMPLER_STATE::LINEAR).GetAddressOf());

	ID3D11ShaderResourceView* cluster_srvs[] = { light_srv.Get(), count_srv.Get(), offset_srv.Get(), index_srv.Get() };
	dc->PSSetShaderResources(clustered_srv_index, ARRAYSIZE(cluster_srvs), cluster_srvs);

	context.set_index_buffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
	context.set_primitive_topology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	dc->Draw(4, 0);

	context.clear_vertex_buffer();
	context.clear_index_buffer();
	context.clear_shader_slots();
	context.clear_shader_resorce_views();
	context.clear_sampler();
	context.clear_constant_buffers();

	context.debug_end_event();
}
