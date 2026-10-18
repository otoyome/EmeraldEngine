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
	console_variable<bool> enable_variable("r.clustered.enable", true, "影無しの点光源/スポットライトをクラスタードライティングで描画する", console_flags::user_setting);
	console_variable<float> far_variable("r.clustered.far", system_units::convert_m_to_unit(500.0f), "クラスターの奥行き分割を行う最大距離（以降は最後の分割にまとめる）", console_flags::user_setting);
	console_variable<bool> depth_culling_variable("r.clustered.depth_culling", true, "タイル内で最も奥の面より奥のクラスターにはライトを割り当てない", console_flags::user_setting);
	bool get_enable() { return enable_variable.get(); }
	float get_far() { return far_variable.get(); }
	bool get_depth_culling() { return depth_culling_variable.get(); }

	//	デバッグ表示用のコンソール変数
#if defined(USE_IMGUI)
	console_variable<bool> visualize_variable("r.clustered.visualize", false, "クラスターごとのライト数をヒートマップで表示する");
	console_variable<float> visualize_max_variable("r.clustered.visualize_max", 32.0f, "ヒートマップで最大色になるライト数");
	bool get_visualize() { return visualize_variable.get(); }
	float get_visualize_max() { return (std::max)(visualize_max_variable.get(), 1.0f); }
#endif	//	defined(USE_IMGUI)

	//	定数バッファのフラグ
	constexpr UINT clustered_flag_depth_culling = 1u << 0;

	//	クラスター分割
	constexpr UINT tile_size = 64;		//	タイルの大きさ（ピクセル）
	constexpr UINT slice_count = 24;	//	奥行き分割数

	//	ディスパッチ
	constexpr UINT bounds_thread_count = 64;
	constexpr UINT depth_thread_size = 16;
	constexpr UINT dispatch_width = 65535;

	//	ライトリストの初期容量
	constexpr UINT initial_index_capacity = 1u << 18;

	//	シェーダーのリソース番号
	constexpr UINT gbuffer_srv_index = common_pipeline_states::gbuffer_srv_start;
	constexpr UINT gbuffer_sampler_index = 0;
	//	@tool フレーム・視点層の番号（シェーダーの clustered_lighting.hlsli / clustered_lighting_shading.hlsli と合わせる）
	constexpr UINT clustered_cbv_index = 12;
	constexpr UINT clustered_srv_index = 119;

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

	std::shared_ptr<pixel_shader> load_ps(std::string_view path, bool visualize)
	{
		load_shader_file desc{};
		desc.file_path = path;
		if (visualize)
			desc.defines.emplace_back("CLUSTERED_VISUALIZE", "1");
#if defined(_DEBUG)
		system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::disable_optimize, true);
		system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::debug, true);
#endif	//	defined(_DEBUG)
		std::shared_ptr<pixel_shader> shader;
		shader_manager::instance()->load_shader<pixel_shader>(shader, desc);
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
	depth_cs = load_cs("Shaders/ClusteredLighting/clustered_lighting_depth_cs.hlsl", false);
	{
		load_shader_cso desc;
		desc.file_path = "Assets/CSO/fullscreen_quad_vs.cso";
		shader_manager::instance()->load_shader<vertex_shader>(fullscreen_vs, desc);
	}
	lighting_ps = load_ps("Shaders/ClusteredLighting/deferred_rendering_clustered_ps.hlsl", false);
#if defined(USE_IMGUI)
	visualize_ps = load_ps("Shaders/ClusteredLighting/deferred_rendering_clustered_ps.hlsl", true);
#endif	//	defined(USE_IMGUI)

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
		statistics_readback.initialize(device.Get(), { static_cast<UINT>(sizeof(UINT)) }, readback_count, 8);
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

#if defined(USE_IMGUI)
		D3D11_BLEND_DESC visualize_blend_desc{};
		visualize_blend_desc.RenderTarget[0].BlendEnable = FALSE;
		visualize_blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		hr = device->CreateBlendState(&visualize_blend_desc, visualize_blend_state.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
#endif	//	defined(USE_IMGUI)

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

void clustered_lighting_system::capture_settings(render_settings::clustered_lighting_settings& out_settings)
{
	out_settings.enable = get_enable();
	out_settings.far_distance = get_far();
	out_settings.depth_culling = get_depth_culling();
#if defined(USE_IMGUI)
	out_settings.visualize = get_visualize();
	out_settings.visualize_max = get_visualize_max();
#endif	//	defined(USE_IMGUI)
}

bool clustered_lighting_system::is_visualize(const render_settings& settings)
{
#if defined(USE_IMGUI)
	return settings.clustered_lighting.visualize;
#else	//	defined(USE_IMGUI)
	(void)settings;
	return false;
#endif	//	defined(USE_IMGUI)
}

bool clustered_lighting_system::is_target(const light_render_proxy& light, const render_settings& settings)
{
	if (!settings.clustered_lighting.enable)
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

void clustered_lighting_system::generate_tile_buffer(UINT count)
{
	if (count <= tile_capacity && tile_depth_buffer)
		return;

	tile_capacity = round_up_power_of_two(count, 256);
	create_structured_buffer(sizeof(UINT), tile_capacity, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, tile_depth_buffer, &tile_depth_srv, &tile_depth_uav);
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
	//	@tool 読み戻しで届いた必要数を取り込む（受け取りは提出スレッドで行われる）
	required_index_count = (std::max)(required_index_count, received_index_count->exchange(0));
}

void clustered_lighting_system::prepare(const view_render_data& render_data, const common_pipeline_states::scene_constants& scene)
{
	//	ライト収集
	lights.clear();
	for (const auto& light : render_data.light_container.lights)
	{
		if (is_target(light, render_data.settings))
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
	tile_total = cluster_count.x * cluster_count.y;

	//	バッファ準備
	if (light_count > 0)
	{
		generate_light_buffer(light_count);
		generate_cluster_buffers(cluster_total);
		generate_tile_buffer(tile_total);
		if (required_index_count > index_capacity)
			generate_index_buffer(required_index_count + required_index_count / 4);
	}

	//	奥行き分割距離算出
	const float cluster_near = (std::max)(scene.camera_clip_distance.x, 1.0e-4f);
	float cluster_far = render_data.settings.clustered_lighting.far_distance;
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
	constants.flags = render_data.settings.clustered_lighting.depth_culling ? clustered_flag_depth_culling : 0u;
#if defined(USE_IMGUI)
	constants.visualize_max = render_data.settings.clustered_lighting.visualize_max;
#endif	//	defined(USE_IMGUI)
}

void clustered_lighting_system::build(render_job_context& context)
{
	const auto& recorder = context.recorder;

	//	ライトが無くてもフォワード描画がライト数を参照できるよう定数は毎回更新する
	recorder.update_subresource(constant_buffer.Get(), 0, nullptr, &constants, 0, 0);
	if (light_count == 0)
		return;

	context.recorder.begin_event(L"clustered_lighting_build");

	{
		D3D11_BOX box{};
		box.right = static_cast<UINT>(sizeof(light_data) * light_count);
		box.bottom = 1;
		box.back = 1;
		recorder.update_subresource(light_buffer.Get(), 0, &box, lights.data(), 0, 0);
	}

	ID3D11Buffer* constant_buffers[] = { context.scene_constant_buffer.Get() };
	recorder.set_constant_buffers(shader_stage::compute, common_pipeline_states::scene_cbv_index, 1, constant_buffers);
	recorder.set_constant_buffers(shader_stage::compute, clustered_cbv_index, 1, constant_buffer.GetAddressOf());

	const UINT light_group_x = (std::min)(light_count, dispatch_width);
	const UINT light_group_y = (light_count + dispatch_width - 1) / dispatch_width;

	//	クラスターの範囲
	//  クラスターごとの view 空間 AABB を求める
	{
		recorder.set_compute_shader(bounds_cs->get_shader().Get());
		recorder.set_unordered_access_views(0, 1, bounds_uav.GetAddressOf());
		recorder.dispatch((cluster_total + bounds_thread_count - 1) / bounds_thread_count, 1, 1);
		context.recorder.clear_unordered_access_views();
	}

	//	タイルごとの最も奥の奥行き分割を G-Buffer の深度から求める
	if (constants.flags & clustered_flag_depth_culling)
	{
		const texture_ptr normal_depth = context.recorder.get_bind_table().get_texture(common_pipeline_states::gbuffer_hash_keys[static_cast<UINT>(common_pipeline_states::gbuffer_id::normal_depth)]);
		if (normal_depth)
		{
			const UINT zero[4] = {};
			recorder.clear_unordered_access_view_uint(tile_depth_uav.Get(), zero);

			const UINT width = static_cast<UINT>(normal_depth->get_width());
			const UINT height = static_cast<UINT>(normal_depth->get_height());
			recorder.set_compute_shader(depth_cs->get_shader().Get());
			recorder.set_shader_resource_views(shader_stage::compute, 0, 1, normal_depth->get_shader_resource_view().GetAddressOf());
			recorder.set_unordered_access_views(0, 1, tile_depth_uav.GetAddressOf());
			recorder.dispatch((width + depth_thread_size - 1) / depth_thread_size, (height + depth_thread_size - 1) / depth_thread_size, 1);
			context.recorder.clear_unordered_access_views();
			context.recorder.clear_shader_resorce_views();
		}
		else
		{
			//	深度が無ければ全クラスターを対象にする
			const UINT all_slices[4] = { slice_count - 1, slice_count - 1, slice_count - 1, slice_count - 1 };
			recorder.clear_unordered_access_view_uint(tile_depth_uav.Get(), all_slices);
		}
	}

	//	クラスターごとのライト数を数える
	{
		const UINT zero[4] = {};
		recorder.clear_unordered_access_view_uint(count_uav.Get(), zero);
		recorder.clear_unordered_access_view_uint(cursor_uav.Get(), zero);

		ID3D11ShaderResourceView* srvs[] = { light_srv.Get(), bounds_srv.Get(), nullptr, tile_depth_srv.Get() };
		recorder.set_compute_shader(count_cs->get_shader().Get());
		recorder.set_shader_resource_views(shader_stage::compute, 0, ARRAYSIZE(srvs), srvs);
		recorder.set_unordered_access_views(0, 1, count_uav.GetAddressOf());
		recorder.dispatch(light_group_x, light_group_y, 1);
		context.recorder.clear_unordered_access_views();
		context.recorder.clear_shader_resorce_views();
	}

	//	↑で数えたので書き込み開始位置を決める
	{
		ID3D11UnorderedAccessView* uavs[] = { offset_uav.Get(), statistics_uav.Get() };
		recorder.set_compute_shader(scan_cs->get_shader().Get());
		recorder.set_shader_resource_views(shader_stage::compute, 0, 1, count_srv.GetAddressOf());
		recorder.set_unordered_access_views(0, ARRAYSIZE(uavs), uavs);
		recorder.dispatch(1, 1, 1);
		context.recorder.clear_unordered_access_views();
		context.recorder.clear_shader_resorce_views();
	}

	//	ライト番号を書き込む
	{
		ID3D11ShaderResourceView* srvs[] = { light_srv.Get(), bounds_srv.Get(), offset_srv.Get(), tile_depth_srv.Get() };
		ID3D11UnorderedAccessView* uavs[] = { cursor_uav.Get(), index_uav.Get() };
		recorder.set_compute_shader(fill_cs->get_shader().Get());
		recorder.set_shader_resource_views(shader_stage::compute, 0, ARRAYSIZE(srvs), srvs);
		recorder.set_unordered_access_views(0, ARRAYSIZE(uavs), uavs);
		recorder.dispatch(light_group_x, light_group_y, 1);
		context.recorder.clear_unordered_access_views();
		context.recorder.clear_shader_resorce_views();
	}

	recorder.set_compute_shader(nullptr);
	context.recorder.clear_constant_buffers();

	//	ライト番号総数をコピー
	recorder.read_back(statistics_readback, { { statistics_buffer.Get(), nullptr } },
		[received = received_index_count](const gpu_readback_data& data)
		{
			const UINT count = *data.get<UINT>(0);
			UINT current = received->load();
			while (count > current && !received->compare_exchange_weak(current, count))
			{
			}
		});

	context.recorder.end_event();
}

void clustered_lighting_system::render(render_job_context& context)
{
	const bool visualize = is_visualize(context.render_data->settings);
	if (light_count == 0 && !visualize)
		return;

	//	@tool ヒートマップ表示（デバッグ機能）の時は専用のシェーダーとブレンドステートを使う
	pixel_shader* ps = lighting_ps.get();
	ID3D11BlendState* blend = blend_state.Get();
#if defined(USE_IMGUI)
	if (visualize)
	{
		ps = visualize_ps.get();
		blend = visualize_blend_state.Get();
	}
#endif	//	defined(USE_IMGUI)

	const auto& recorder = context.recorder;
	context.recorder.begin_event(L"clustered_lighting");

	recorder.set_vertex_shader(fullscreen_vs->get_shader().Get());
	recorder.set_hull_shader(nullptr);
	recorder.set_domain_shader(nullptr);
	recorder.set_geometry_shader(nullptr);
	recorder.set_pixel_shader(ps->get_shader().Get());
	recorder.set_input_layout(fullscreen_vs->get_input_layout().Get());
	recorder.set_blend_state(blend);
	recorder.set_depth_stencil_state(depth_stencil_state.Get(), 0);
	recorder.set_rasterizer_state(rasterizer_state.Get());

	recorder.set_constant_buffers(shader_stage::vertex, common_pipeline_states::scene_cbv_index, 1, context.scene_constant_buffer.GetAddressOf());
	recorder.set_constant_buffers(shader_stage::pixel, common_pipeline_states::scene_cbv_index, 1, context.scene_constant_buffer.GetAddressOf());
	recorder.set_constant_buffers(shader_stage::pixel, clustered_cbv_index, 1, constant_buffer.GetAddressOf());

	ID3D11ShaderResourceView* g_buffer_srvs[static_cast<UINT>(common_pipeline_states::gbuffer_id::max)];
	for (UINT index = 0; index < static_cast<UINT>(common_pipeline_states::gbuffer_id::max); ++index)
	{
		g_buffer_srvs[index] = context.recorder.get_bind_table().get_texture(common_pipeline_states::gbuffer_hash_keys[index])->get_shader_resource_view().Get();
	}
	recorder.set_shader_resource_views(shader_stage::pixel, gbuffer_srv_index, ARRAYSIZE(g_buffer_srvs), g_buffer_srvs);
	recorder.set_samplers(shader_stage::pixel, gbuffer_sampler_index, 1, render_data::instance()->get_sampler_states(render_data::SAMPLER_STATE::LINEAR).GetAddressOf());

	ID3D11ShaderResourceView* cluster_srvs[] = { light_srv.Get(), count_srv.Get(), offset_srv.Get(), index_srv.Get() };
	recorder.set_shader_resource_views(shader_stage::pixel, clustered_srv_index, ARRAYSIZE(cluster_srvs), cluster_srvs);

	context.recorder.set_index_buffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
	context.recorder.set_primitive_topology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	recorder.draw(4, 0);

	context.recorder.clear_vertex_buffer();
	context.recorder.clear_index_buffer();
	context.recorder.clear_shader_slots();
	context.recorder.clear_shader_resorce_views();
	context.recorder.clear_sampler();
	context.recorder.clear_constant_buffers();

	context.recorder.end_event();
}
