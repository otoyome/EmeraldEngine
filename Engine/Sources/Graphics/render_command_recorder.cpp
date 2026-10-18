#include "Graphics/render_command_recorder.h"

#include "pipeline_state.h"
#include "material.h"
#include "render_data.h"
#include "graphics/render_pass_begin_info.h"
#include "Graphics/material_render_states.h"
#include "System/misc.h"

#include <algorithm>

void render_command_recorder::initialize(const Microsoft::WRL::ComPtr<ID3D11DeviceContext>& device_context)
{
	this->device_context = device_context;
	HRESULT hr = device_context.As(&device_context1);
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	auto render_data = render_data::instance();
	constant_allocator = std::make_unique<immediate_constant_allocator>();
	constant_allocator->initialize(render_data->get_device().Get(), render_data->get_feature_supports().constant_buffer_offsetting);

#if defined(USE_IMGUI)
	//	@tool 取得できなければデバッグイベントを出さない
	annotation.Reset();
	device_context.As(&annotation);
#endif	//	defined(USE_IMGUI)
}

void render_command_recorder::begin()
{
	_ASSERT_EXPR(device_context, L"initialize is not called.");

	//	@tool 新しいコマンドリストの記録開始
	constant_allocator->begin();

	//	@tool 前の記録のキャッシュを捨てる
	cache_rasterizer_state = nullptr;
	cache_depth_stencil_state = nullptr;
	cache_blend_state = nullptr;
	cache_stencil_ref = 0xFFFFFFFF;
	cache_primitive_topology = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
	cache_vertex_buffer_start_slot = 0xFFFFFFFF;
	cache_vertex_buffer_count = 0;
	std::fill(std::begin(cache_vertex_buffers), std::end(cache_vertex_buffers), nullptr);
	std::fill(std::begin(cache_vertex_buffer_strides), std::end(cache_vertex_buffer_strides), 0u);
	std::fill(std::begin(cache_vertex_buffer_offsets), std::end(cache_vertex_buffer_offsets), 0u);
	cache_index_buffer = nullptr;
	cache_index_buffer_format = DXGI_FORMAT_UNKNOWN;
	cache_index_buffer_offset = 0xFFFFFFFF;
	parameter_cache.clear();
	current_layout = nullptr;

	bind_table = resource_view_bind_table();
	state_overrides = render_state_overrides();
	invalidate_bind_table(false);
}

void render_command_recorder::end()
{
	//	@tool 割り当て表が持つ資源の参照を次の記録まで残さない
	bind_table = resource_view_bind_table();
	state_overrides = render_state_overrides();
	parameter_cache.clear();
	current_layout = nullptr;
}

Microsoft::WRL::ComPtr<ID3D11CommandList> render_command_recorder::finish_command_list() const
{
	Microsoft::WRL::ComPtr<ID3D11CommandList> command_list;
	device_context->FinishCommandList(false, command_list.GetAddressOf());
	return command_list;
}

void render_command_recorder::invalidate_bind_table(bool keep_frame_layer) const
{
	for (UINT stage = 0; stage < recorded_stage_count; ++stage)
	{
		const bool keep = keep_frame_layer && stage < graphics_shader_stage_count;
		for (UINT slot = 0; slot < bound_constant_buffers[stage].size(); ++slot)
			if (!keep || !is_frame_layer_constant_buffer(slot))
				bound_constant_buffers[stage][slot] = { unknown_binding, 0, 0 };
		for (UINT slot = 0; slot < bound_shader_resources[stage].size(); ++slot)
			if (!keep || !is_frame_layer_shader_resource(slot))
				bound_shader_resources[stage][slot] = unknown_binding;
		for (UINT slot = 0; slot < bound_samplers[stage].size(); ++slot)
			if (!keep || !is_frame_layer_sampler(slot))
				bound_samplers[stage][slot] = unknown_binding;
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void render_command_recorder::set_rasterizer_state(ID3D11RasterizerState* state) const
{
	if (cache_rasterizer_state == state)
		return;
	device_context->RSSetState(state);
	cache_rasterizer_state = state;
}

void render_command_recorder::set_depth_stencil_state(ID3D11DepthStencilState* state, UINT stencil_ref) const
{
	if (cache_depth_stencil_state == state && cache_stencil_ref == stencil_ref)
		return;
	device_context->OMSetDepthStencilState(state, stencil_ref);
	cache_depth_stencil_state = state;
	cache_stencil_ref = stencil_ref;
}

void render_command_recorder::set_blend_state(ID3D11BlendState* state) const
{
	if (cache_blend_state == state)
		return;
	device_context->OMSetBlendState(state, nullptr, 0xFFFFFFFF);
	cache_blend_state = state;
}

void render_command_recorder::set_primitive_topology(D3D_PRIMITIVE_TOPOLOGY topology) const
{
	if (cache_primitive_topology == topology)
		return;
	device_context->IASetPrimitiveTopology(topology);
	cache_primitive_topology = topology;
}

void render_command_recorder::set_vertex_buffers(UINT start_slot, UINT num_buffers, ID3D11Buffer* const* buffers, const UINT* strides, const UINT* offsets) const
{
	bool is_same = (cache_vertex_buffer_start_slot == start_slot) && (cache_vertex_buffer_count == num_buffers);
	for (UINT index = 0; is_same && index < num_buffers; ++index)
	{
		if (cache_vertex_buffers[index] != buffers[index] ||
			cache_vertex_buffer_strides[index] != strides[index] ||
			cache_vertex_buffer_offsets[index] != offsets[index])
		{
			is_same = false;
		}
	}
	if (is_same)
		return;

	device_context->IASetVertexBuffers(start_slot, num_buffers, buffers, strides, offsets);

	cache_vertex_buffer_start_slot = start_slot;
	cache_vertex_buffer_count = num_buffers;
	for (UINT index = 0; index < num_buffers; ++index)
	{
		cache_vertex_buffers[index] = buffers[index];
		cache_vertex_buffer_strides[index] = strides[index];
		cache_vertex_buffer_offsets[index] = offsets[index];
	}
}

void render_command_recorder::set_index_buffer(ID3D11Buffer* buffer, DXGI_FORMAT format, UINT offset) const
{
	if (cache_index_buffer == buffer && cache_index_buffer_format == format && cache_index_buffer_offset == offset)
		return;
	device_context->IASetIndexBuffer(buffer, format, offset);
	cache_index_buffer = buffer;
	cache_index_buffer_format = format;
	cache_index_buffer_offset = offset;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void render_command_recorder::clear_shader_slots() const
{
	device_context->VSSetShader(nullptr, nullptr, 0);
	device_context->HSSetShader(nullptr, nullptr, 0);
	device_context->DSSetShader(nullptr, nullptr, 0);
	device_context->GSSetShader(nullptr, nullptr, 0);
	device_context->PSSetShader(nullptr, nullptr, 0);
	device_context->CSSetShader(nullptr, nullptr, 0);
}

void render_command_recorder::clear_vertex_buffer() const
{
	ID3D11Buffer* vbs[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT] = {};
	UINT dummy[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT] = {};
	set_vertex_buffers(0, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT, vbs, dummy, dummy);
}

void render_command_recorder::clear_index_buffer() const
{
	set_index_buffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
}

void render_command_recorder::clear_render_target_slots() const
{
	device_context->OMSetRenderTargets(0, nullptr, nullptr);
}

void render_command_recorder::clear_constant_buffers(int start_slot, int num) const
{
	for (auto& stage : bound_constant_buffers)
		for (int slot = start_slot; slot < start_slot + num && slot < static_cast<int>(stage.size()); ++slot)
			stage[slot] = {};

	ID3D11Buffer* clear_constant_bufferes[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT]{};
	device_context->VSSetConstantBuffers(start_slot, num, clear_constant_bufferes);
	device_context->HSSetConstantBuffers(start_slot, num, clear_constant_bufferes);
	device_context->DSSetConstantBuffers(start_slot, num, clear_constant_bufferes);
	device_context->GSSetConstantBuffers(start_slot, num, clear_constant_bufferes);
	device_context->PSSetConstantBuffers(start_slot, num, clear_constant_bufferes);
	device_context->CSSetConstantBuffers(start_slot, num, clear_constant_bufferes);
}

void render_command_recorder::clear_shader_resorce_views(int start_slot, int num) const
{
	for (auto& stage : bound_shader_resources)
		for (int slot = start_slot; slot < start_slot + num && slot < static_cast<int>(stage.size()); ++slot)
			stage[slot] = nullptr;

	ID3D11ShaderResourceView* clear_shader_resource_view[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT]{};
	device_context->VSSetShaderResources(start_slot, num, clear_shader_resource_view);
	device_context->HSSetShaderResources(start_slot, num, clear_shader_resource_view);
	device_context->DSSetShaderResources(start_slot, num, clear_shader_resource_view);
	device_context->GSSetShaderResources(start_slot, num, clear_shader_resource_view);
	device_context->PSSetShaderResources(start_slot, num, clear_shader_resource_view);
	device_context->CSSetShaderResources(start_slot, num, clear_shader_resource_view);
}

void render_command_recorder::clear_unordered_access_views(int start_slot, int num) const
{
	ID3D11UnorderedAccessView* clear_unordered_access_view[8]{};
	device_context->CSSetUnorderedAccessViews(start_slot, num, clear_unordered_access_view, nullptr);
}

void render_command_recorder::clear_sampler(int start_slot, int num) const
{
	for (auto& stage : bound_samplers)
		for (int slot = start_slot; slot < start_slot + num && slot < static_cast<int>(stage.size()); ++slot)
			stage[slot] = nullptr;

	ID3D11SamplerState* clear_sampler[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT]{};
	device_context->VSSetSamplers(start_slot, num, clear_sampler);
	device_context->HSSetSamplers(start_slot, num, clear_sampler);
	device_context->DSSetSamplers(start_slot, num, clear_sampler);
	device_context->GSSetSamplers(start_slot, num, clear_sampler);
	device_context->PSSetSamplers(start_slot, num, clear_sampler);
	device_context->CSSetSamplers(start_slot, num, clear_sampler);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
constant_range render_command_recorder::upload_constants(const void* data, UINT size) const
{
	_ASSERT_EXPR(constant_allocator, L"constant_allocator is not set.");
	if (!constant_allocator)
		return {};
	return constant_allocator->allocate(device_context.Get(), data, size);
}

bool render_command_recorder::resolve_constant_buffer(const resource_view_bind_entry& entry, bool has_draw_data, constant_range& out_range) const
{
	if (entry.requires_draw_data && !has_draw_data)
		return false;

	switch (entry.kind)
	{
		case resource_view_bind_entry::kinds::constant_buffer:
		{
			out_range = {};
			out_range.buffer = entry.constant_buffer.Get();
			return true;
		}
		case resource_view_bind_entry::kinds::constant_data:
		{
			if (!entry.uploaded_range.is_valid() || !entry.uploaded_range.reusable)
				entry.uploaded_range = upload_constants(entry.constant_data.data(), static_cast<UINT>(entry.constant_data.size()));
			out_range = entry.uploaded_range;
			return true;
		}
		default:
			return false;
	}
}

void render_command_recorder::set_constant_buffer(shader_stage stage, UINT slot, const constant_range& range) const
{
	if (range.num_constants == 0)
	{
		set_constant_buffer(stage, slot, range.buffer);
		return;
	}

	if (static_cast<UINT>(stage) < recorded_stage_count && slot < D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT)
	{
		auto& bound = bound_constant_buffers[static_cast<UINT>(stage)][slot];
		if (bound.buffer == range.buffer && bound.first_constant == range.first_constant && bound.num_constants == range.num_constants)
		{
			return;
		}
		bound = { range.buffer, range.first_constant, range.num_constants };
	}

	_ASSERT_EXPR(device_context1, L"device_context1 is not set.");
	if (!device_context1)
		return;

	ID3D11Buffer* buffer = range.buffer;
	const UINT* first = &range.first_constant;
	const UINT* num = &range.num_constants;
	switch (stage)
	{
		case shader_stage::vertex:		device_context1->VSSetConstantBuffers1(slot, 1, &buffer, first, num); break;
		case shader_stage::hull:		device_context1->HSSetConstantBuffers1(slot, 1, &buffer, first, num); break;
		case shader_stage::domain:		device_context1->DSSetConstantBuffers1(slot, 1, &buffer, first, num); break;
		case shader_stage::geometry:	device_context1->GSSetConstantBuffers1(slot, 1, &buffer, first, num); break;
		case shader_stage::pixel:		device_context1->PSSetConstantBuffers1(slot, 1, &buffer, first, num); break;
		case shader_stage::compute:		device_context1->CSSetConstantBuffers1(slot, 1, &buffer, first, num); break;
	}
}

void render_command_recorder::set_constant_buffer(shader_stage stage, UINT slot, ID3D11Buffer* buffer) const
{
	if (static_cast<UINT>(stage) < recorded_stage_count && slot < D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT)
	{
		auto& bound = bound_constant_buffers[static_cast<UINT>(stage)][slot];
		if (bound.buffer == buffer && bound.num_constants == 0)
		{
			return;
		}
		bound = { buffer, 0, 0 };
	}

	switch (stage)
	{
		case shader_stage::vertex:		device_context->VSSetConstantBuffers(slot, 1, &buffer); break;
		case shader_stage::hull:		device_context->HSSetConstantBuffers(slot, 1, &buffer); break;
		case shader_stage::domain:		device_context->DSSetConstantBuffers(slot, 1, &buffer); break;
		case shader_stage::geometry:	device_context->GSSetConstantBuffers(slot, 1, &buffer); break;
		case shader_stage::pixel:		device_context->PSSetConstantBuffers(slot, 1, &buffer); break;
		case shader_stage::compute:		device_context->CSSetConstantBuffers(slot, 1, &buffer); break;
	}
}

void render_command_recorder::set_shader_resource_view(shader_stage stage, UINT slot, ID3D11ShaderResourceView* view) const
{
	if (static_cast<UINT>(stage) < recorded_stage_count && slot < D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT)
	{
		auto& bound = bound_shader_resources[static_cast<UINT>(stage)][slot];
		if (bound == view)
		{
			return;
		}
		bound = view;
	}

	switch (stage)
	{
		case shader_stage::vertex:		device_context->VSSetShaderResources(slot, 1, &view); break;
		case shader_stage::hull:		device_context->HSSetShaderResources(slot, 1, &view); break;
		case shader_stage::domain:		device_context->DSSetShaderResources(slot, 1, &view); break;
		case shader_stage::geometry:	device_context->GSSetShaderResources(slot, 1, &view); break;
		case shader_stage::pixel:		device_context->PSSetShaderResources(slot, 1, &view); break;
		case shader_stage::compute:		device_context->CSSetShaderResources(slot, 1, &view); break;
	}
}

void render_command_recorder::set_sampler(shader_stage stage, UINT slot, ID3D11SamplerState* sampler) const
{
	if (static_cast<UINT>(stage) < recorded_stage_count && slot < D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT)
	{
		auto& bound = bound_samplers[static_cast<UINT>(stage)][slot];
		if (bound == sampler)
		{
			return;
		}
		bound = sampler;
	}

	switch (stage)
	{
		case shader_stage::vertex:		device_context->VSSetSamplers(slot, 1, &sampler); break;
		case shader_stage::hull:		device_context->HSSetSamplers(slot, 1, &sampler); break;
		case shader_stage::domain:		device_context->DSSetSamplers(slot, 1, &sampler); break;
		case shader_stage::geometry:	device_context->GSSetSamplers(slot, 1, &sampler); break;
		case shader_stage::pixel:		device_context->PSSetSamplers(slot, 1, &sampler); break;
		case shader_stage::compute:		device_context->CSSetSamplers(slot, 1, &sampler); break;
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
namespace
{
	//  マテリアルフラグの対応表
	constexpr std::pair<material::flags, UINT> convert_material_flags[] =
	{
		{material::flags::indirect_lighting_off, common_pipeline_states::material_attribute_indirect_lighting_off},
		{material::flags::shadow_receiver, common_pipeline_states::material_attribute_shadow_receiver},
		{material::flags::decal_off, common_pipeline_states::material_attribute_decal_off},
		//	[エンジン改造]
		{material::flags::ligting_xz_plane, common_pipeline_states::material_attribute_ligting_xz_plane},
		//	[エンジン改造]
	};
}

void render_command_recorder::set_pipeline(const graphics_pipeline_state& pipeline) const
{
	invalidate_bind_table(true);
	current_layout = &pipeline.get_layout();

	//	シェーダー設定
	const graphics_pipeline_shaders& shaders = pipeline.get_shaders();
	if (shaders.vs)
	{
		device_context->VSSetShader(shaders.vs->get_shader().Get(), nullptr, 0);
		device_context->IASetInputLayout(shaders.vs->get_input_layout().Get());
	}
	else
	{
		device_context->VSSetShader(nullptr, nullptr, 0);
		device_context->IASetInputLayout(nullptr);
	}
	device_context->HSSetShader(shaders.hs ? shaders.hs->get_shader().Get() : nullptr, nullptr, 0);
	device_context->DSSetShader(shaders.ds ? shaders.ds->get_shader().Get() : nullptr, nullptr, 0);
	device_context->GSSetShader(shaders.gs ? shaders.gs->get_shader().Get() : nullptr, nullptr, 0);
	device_context->PSSetShader(shaders.ps ? shaders.ps->get_shader().Get() : nullptr, nullptr, 0);

	//	@tool 図形の指定があれば設定する（キャッシュに関わらず設定し、キャッシュも合わせる）
	const D3D11_PRIMITIVE_TOPOLOGY topology = pipeline.get_primitive_topology();
	if (topology != D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED)
	{
		device_context->IASetPrimitiveTopology(topology);
		cache_primitive_topology = topology;
	}

	//	@tool 固定のステート（キャッシュに関わらず設定し、キャッシュも合わせる）
	if (pipeline.get_render_state_mode() == graphics_pipeline_state::render_state_modes::fixed)
	{
		device_context->OMSetBlendState(pipeline.get_blend_state(), nullptr, 0xFFFFFFFF);
		device_context->OMSetDepthStencilState(pipeline.get_depth_stencil_state(), pipeline.get_stencil_ref());
		device_context->RSSetState(pipeline.get_rasterizer_state());
		cache_blend_state = pipeline.get_blend_state();
		cache_depth_stencil_state = pipeline.get_depth_stencil_state();
		cache_stencil_ref = pipeline.get_stencil_ref();
		cache_rasterizer_state = pipeline.get_rasterizer_state();
	}

	//	@tool 割り当て表・固定のリソースの割り当て（描画毎に変わらないもの）
	const pipeline_layout& layout = pipeline.get_layout();
	for (const auto& binding : layout.get_constant_buffers())
	{
		if (binding.source != binding_source::table)
			continue;

		const resource_view_bind_entry* entry = bind_table.find(binding.key);
		constant_range range;
		if (!entry || !resolve_constant_buffer(*entry, false, range))
			continue;

		for (UINT stage = 0; stage < graphics_shader_stage_count; ++stage)
		{
			if (binding.slots[stage] != pipeline_binding::unused_slot)
				set_constant_buffer(static_cast<shader_stage>(stage), binding.slots[stage], range);
		}
	}
	for (const auto& binding : layout.get_shader_resources())
	{
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> view;
		if (binding.source == binding_source::fixed)
		{
			if (!binding.fixed_texture)
				continue;
			view = binding.fixed_texture->get_shader_resource_view();
		}
		else if (binding.source == binding_source::table)
		{
			const resource_view_bind_entry* entry = bind_table.find(binding.key);
			if (!entry || (entry->kind != resource_view_bind_entry::kinds::texture && entry->kind != resource_view_bind_entry::kinds::shader_resource_view))
				continue;
			view = entry->view.shader_resource_view;
		}
		else
		{
			continue;
		}

		for (UINT stage = 0; stage < graphics_shader_stage_count; ++stage)
		{
			if (binding.slots[stage] != pipeline_binding::unused_slot)
				set_shader_resource_view(static_cast<shader_stage>(stage), binding.slots[stage], view.Get());
		}
	}
	for (const auto& binding : layout.get_samplers())
	{
		if (binding.source != binding_source::fixed)
			continue;

		for (UINT stage = 0; stage < graphics_shader_stage_count; ++stage)
		{
			if (binding.slots[stage] != pipeline_binding::unused_slot)
				set_sampler(static_cast<shader_stage>(stage), binding.slots[stage], binding.fixed_sampler.Get());
		}
	}
}

void render_command_recorder::apply_material_render_states(const graphics_pipeline_state& pipeline, const material_render_proxy& material, const render_state_overrides& overrides) const
{
	using states = material_render_states;
	const states& render_states = states::instance();

	//	ラスタライザーステート設定
	{
		bool is_two_sided = material.is_flag(material::flags::two_sided);
		if (material.get_domain() == material::domains::user_interface || overrides.force_two_sided)
		{
			is_two_sided = true;
		}

		bool is_wire = material.is_flag(material::flags::wire);
		if (overrides.force_wire)
		{
			is_wire = true;
		}

		states::raster_state rs;
		if (is_wire)
		{
			rs = is_two_sided ? states::raster_state::wire_two_sided : states::raster_state::wire;
		}
		else
		{
			rs = is_two_sided ? states::raster_state::two_sided : states::raster_state::solid;
		}
		set_rasterizer_state(render_states.get(rs));
	}

	//	ブレンドステート&深度ステンシルステート設定
	{
		auto depth_stencil_state = states::depth_stencil_state::depth_test_and_write;
		auto blend_state = states::blend_state::opaque;
		auto stencil_ref_value = pipeline.get_stencil_ref();

		if (overrides.is_shadow_rendering)
		{
			//	シャドウマップ生成時
			depth_stencil_state = states::depth_stencil_state::depth_test_and_write;
			blend_state = states::blend_state::opaque;
			stencil_ref_value = 0;
		}
		else if (material.get_domain() == material::domains::deferred_decal)
		{
			//	デカール処理
			depth_stencil_state = states::depth_stencil_state::decal;
			blend_state = states::blend_state::opaque;
			stencil_ref_value = 0;
		}
		else
		{
			switch (material.get_blend_mode())
			{
				case material::blend_modes::opaque:
				case material::blend_modes::mask:
				{
					break;
				}
				case material::blend_modes::translucent:
				{
					blend_state = states::blend_state::translucent;
					depth_stencil_state = states::depth_stencil_state::depth_test_only;
					break;
				}
				case material::blend_modes::addative:
				{
					blend_state = states::blend_state::addative;
					depth_stencil_state = states::depth_stencil_state::depth_test_only;
					break;
				}
				//case material::blend_modes::modulate:
				//{
				//	blend_state = blend_state::multiply;
				//	break;
				//}
			}
		}
		set_depth_stencil_state(render_states.get(depth_stencil_state), stencil_ref_value);
		set_blend_state(render_states.get(blend_state));
	}
}

void render_command_recorder::bind_draw(const graphics_pipeline_state& pipeline, const material_render_proxy& material, const mesh_draw_item* draw_item) const
{
	if (pipeline.get_render_state_mode() == graphics_pipeline_state::render_state_modes::material)
		apply_material_render_states(pipeline, material, state_overrides);

	constexpr UINT stage_count = graphics_shader_stage_count;
	const pipeline_layout& layout = pipeline.get_layout();

	//	定数バッファビュー登録
	const pipeline_binding* parameter_binding = nullptr;
	for (const auto& binding : layout.get_constant_buffers())
	{
		switch (binding.source)
		{
			case binding_source::parameter:
			{
				//	@tool 内容が揃った後（SRV 登録の後）に書き込んで設定する
				parameter_binding = &binding;
				break;
			}
			case binding_source::primitive:
			{
				if (!draw_item)
					break;

				ID3D11Buffer* primitive_buffer = draw_item->primitive_buffer.Get();
				for (UINT index = 0; index < stage_count; ++index)
				{
					if (binding.slots[index] != pipeline_binding::unused_slot)
						set_constant_buffer(static_cast<shader_stage>(index), binding.slots[index], primitive_buffer);
				}
				break;
			}
			case binding_source::material:
			{
				//	@tool 割り当て表（パス層 → フレーム・視点層）に有ればそれを、無ければマテリアルのものを使う
				if (const resource_view_bind_entry* entry = bind_table.find(binding.key))
				{
					constant_range range;
					if (!resolve_constant_buffer(*entry, draw_item != nullptr, range))
						break;

					for (UINT index = 0; index < stage_count; ++index)
					{
						if (binding.slots[index] != pipeline_binding::unused_slot)
							set_constant_buffer(static_cast<shader_stage>(index), binding.slots[index], range);
					}
					break;
				}

				//	@tool マテリアルの定数バッファ
				ID3D11Buffer* constant_buffer = material.find_constant_buffer(binding.key);
				for (UINT index = 0; index < stage_count; ++index)
				{
					if (binding.slots[index] != pipeline_binding::unused_slot)
						set_constant_buffer(static_cast<shader_stage>(index), binding.slots[index], constant_buffer);
				}
				break;
			}
			default:
			{
				//	@tool 割り当て表・固定のものは set_pipeline で設定済み
				break;
			}
		}
	}

	//	サンプラステート登録
	//	@tool テクスチャと対になるサンプラーは、この後のシェーダーリソースビュー登録でテクスチャの設定に上書きする
	const auto& samplers = layout.get_samplers();
	ID3D11SamplerState* default_sampler_state = render_data::instance()->get_sampler_states(render_data::SAMPLER_STATE::LINEAR).Get();
	for (const auto& binding : samplers)
	{
		if (binding.source != binding_source::material)
			continue;

		for (UINT index = 0; index < stage_count; ++index)
		{
			if (binding.slots[index] != pipeline_binding::unused_slot)
				set_sampler(static_cast<shader_stage>(index), binding.slots[index], default_sampler_state);
		}
	}

	//	シェーダーリソースビュー登録
	common_pipeline_states::parameter_constants parameter_constant_buffer[stage_count];
	for (const auto& binding : layout.get_shader_resources())
	{
		if (binding.source != binding_source::material)
			continue;

		//	登録用関数
		//	@tool テクスチャから作ったものなら、フラグと対になるサンプラーも設定する（どちらも登録時に確定済み）
		auto register_srv_function = [&](const texture_bind& view)
		{
			ID3D11SamplerState* texture_sampler = (view.is_texture && binding.paired_sampler >= 0) ? view.sampler_state.Get() : nullptr;

			for (UINT index = 0; index < stage_count; ++index)
			{
				const UINT register_index = binding.slots[index];
				if (register_index == pipeline_binding::unused_slot)
					continue;

				set_shader_resource_view(static_cast<shader_stage>(index), register_index, view.shader_resource_view.Get());
				if (!view.is_texture)
					continue;

				int* shader_resource_view_index = &parameter_constant_buffer[index].shader_resource_view_index[register_index / 4].x;
				UINT* shader_resource_view_flags = &parameter_constant_buffer[index].shader_resource_view_flags[register_index / 4].x;
				shader_resource_view_index[register_index % 4] = 1;
				shader_resource_view_flags[register_index % 4] = view.shader_resource_view_flags;

				//	同じステージの対になるサンプラーも設定する
				if (texture_sampler)
				{
					const UINT sampler_register_index = samplers[binding.paired_sampler].slots[index];
					if (sampler_register_index != pipeline_binding::unused_slot)
						set_sampler(static_cast<shader_stage>(index), sampler_register_index, texture_sampler);
				}
			}
		};

		//	@tool 割り当て表（G-Buffer、シーンテクスチャ、シャドウマップ、IBL、クラスターなど）に有ればそれを、無ければマテリアルのテクスチャを使う
		if (const resource_view_bind_entry* entry = bind_table.find(binding.key))
		{
			if (entry->kind == resource_view_bind_entry::kinds::texture || entry->kind == resource_view_bind_entry::kinds::shader_resource_view)
				register_srv_function(entry->view);
			continue;
		}

		//	@tool マテリアルのテクスチャ
		const std::vector<texture_bind>* textures = material.find_textures(binding.key);
		if (!textures)
			continue;

		for (const auto& value : *textures)
		{
			register_srv_function(value);
		}
	}

	//	マテリアルのパラメーター系登録
	if (parameter_binding)
	{
		//	@tool 同じ記録の中で、マテリアル・パイプライン・割り当て表の内容が同じなら、前に書き込んだ範囲を使い回す
		const parameter_cache_key cache_key{ &material, &pipeline, &bind_table, bind_table.get_revision() };
		if (auto it = parameter_cache.find(cache_key); it != parameter_cache.end())
		{
			for (UINT index = 0; index < stage_count; ++index)
			{
				if (parameter_binding->slots[index] != pipeline_binding::unused_slot)
					set_constant_buffer(static_cast<shader_stage>(index), parameter_binding->slots[index], it->second[index]);
			}
			return;
		}

		UINT material_flag = 0;
		for (auto& flag : convert_material_flags)
		{
			system_utility::set_bitflag(material_flag, flag.second, material.is_flag(flag.first));
		}

		std::array<constant_range, graphics_shader_stage_count> ranges{};
		bool reusable = true;
		for (UINT index = 0; index < stage_count; ++index)
		{
			if (parameter_binding->slots[index] == pipeline_binding::unused_slot)
				continue;

			//	揮発性定数バッファへ書き込んで割り当てる
			auto& cb_data = parameter_constant_buffer[index];
			cb_data.material_shading_model = static_cast<UINT>(material.get_shading_model());
			cb_data.material_alpha_clip_threshold = material.get_alpha_clip_threshold();
			cb_data.material_flags = material_flag;
			cb_data.material_blend_mode = static_cast<UINT>(material.get_blend_mode());

			ranges[index] = upload_constants(&cb_data, sizeof(cb_data));
			reusable = reusable && ranges[index].reusable;
			set_constant_buffer(static_cast<shader_stage>(index), parameter_binding->slots[index], ranges[index]);
		}

		//	@tool 使い回せる範囲（オフセット方式）の場合だけ覚えておく
		if (reusable)
			parameter_cache.emplace(cache_key, ranges);
	}
}

void render_command_recorder::end_pipeline() const
{
	clear_vertex_buffer();
	clear_index_buffer();
	clear_shader_slots();

	//	@tool 全てのスロットを外すのはやめ、このパイプラインが使ったシェーダーリソースだけを外す
	//	@tool （描画先・UAV と同じテクスチャが残らないようにするため。定数バッファとサンプラーは衝突しないので残す）
	//	@tool シェーダー段ごとに、使った番号の範囲を1回の呼び出しで外す
	if (current_layout)
	{
		for (UINT stage = 0; stage < graphics_shader_stage_count; ++stage)
		{
			UINT first = UINT_MAX;
			UINT last = 0;
			for (const auto& binding : current_layout->get_shader_resources())
			{
				const UINT slot = binding.slots[stage];
				if (slot == pipeline_binding::unused_slot || slot >= D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT || bound_shader_resources[stage][slot] == nullptr)
					continue;
				//	@tool フレーム・視点層は外さない（描画先にならない資源なので衝突しない）
				if (is_frame_layer_shader_resource(slot))
					continue;
				first = (std::min)(first, slot);
				last = (std::max)(last, slot);
			}
			if (first == UINT_MAX)
				continue;

			ID3D11ShaderResourceView* nulls[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT] = {};
			const UINT count = last - first + 1;
			switch (static_cast<shader_stage>(stage))
			{
				case shader_stage::vertex:		device_context->VSSetShaderResources(first, count, nulls); break;
				case shader_stage::hull:		device_context->HSSetShaderResources(first, count, nulls); break;
				case shader_stage::domain:		device_context->DSSetShaderResources(first, count, nulls); break;
				case shader_stage::geometry:	device_context->GSSetShaderResources(first, count, nulls); break;
				case shader_stage::pixel:		device_context->PSSetShaderResources(first, count, nulls); break;
				default: break;
			}
			for (UINT slot = first; slot <= last; ++slot)
				bound_shader_resources[stage][slot] = nullptr;
		}
	}
	current_layout = nullptr;

	invalidate_bind_table(true);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void render_command_recorder::set_vertex_shader(ID3D11VertexShader* shader) const
{
	device_context->VSSetShader(shader, nullptr, 0);
}

void render_command_recorder::set_hull_shader(ID3D11HullShader* shader) const
{
	device_context->HSSetShader(shader, nullptr, 0);
}

void render_command_recorder::set_domain_shader(ID3D11DomainShader* shader) const
{
	device_context->DSSetShader(shader, nullptr, 0);
}

void render_command_recorder::set_geometry_shader(ID3D11GeometryShader* shader) const
{
	device_context->GSSetShader(shader, nullptr, 0);
}

void render_command_recorder::set_pixel_shader(ID3D11PixelShader* shader) const
{
	device_context->PSSetShader(shader, nullptr, 0);
}

void render_command_recorder::set_compute_shader(ID3D11ComputeShader* shader) const
{
	device_context->CSSetShader(shader, nullptr, 0);
}

void render_command_recorder::set_input_layout(ID3D11InputLayout* layout) const
{
	device_context->IASetInputLayout(layout);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void render_command_recorder::set_constant_buffers(shader_stage stage, UINT start_slot, UINT count, ID3D11Buffer* const* buffers) const
{
	switch (stage)
	{
		case shader_stage::vertex:		device_context->VSSetConstantBuffers(start_slot, count, buffers); break;
		case shader_stage::hull:		device_context->HSSetConstantBuffers(start_slot, count, buffers); break;
		case shader_stage::domain:		device_context->DSSetConstantBuffers(start_slot, count, buffers); break;
		case shader_stage::geometry:	device_context->GSSetConstantBuffers(start_slot, count, buffers); break;
		case shader_stage::pixel:		device_context->PSSetConstantBuffers(start_slot, count, buffers); break;
		case shader_stage::compute:		device_context->CSSetConstantBuffers(start_slot, count, buffers); break;
		default: break;
	}
}

void render_command_recorder::set_shader_resource_views(shader_stage stage, UINT start_slot, UINT count, ID3D11ShaderResourceView* const* views) const
{
	switch (stage)
	{
		case shader_stage::vertex:		device_context->VSSetShaderResources(start_slot, count, views); break;
		case shader_stage::hull:		device_context->HSSetShaderResources(start_slot, count, views); break;
		case shader_stage::domain:		device_context->DSSetShaderResources(start_slot, count, views); break;
		case shader_stage::geometry:	device_context->GSSetShaderResources(start_slot, count, views); break;
		case shader_stage::pixel:		device_context->PSSetShaderResources(start_slot, count, views); break;
		case shader_stage::compute:		device_context->CSSetShaderResources(start_slot, count, views); break;
		default: break;
	}
}

void render_command_recorder::set_samplers(shader_stage stage, UINT start_slot, UINT count, ID3D11SamplerState* const* samplers) const
{
	switch (stage)
	{
		case shader_stage::vertex:		device_context->VSSetSamplers(start_slot, count, samplers); break;
		case shader_stage::hull:		device_context->HSSetSamplers(start_slot, count, samplers); break;
		case shader_stage::domain:		device_context->DSSetSamplers(start_slot, count, samplers); break;
		case shader_stage::geometry:	device_context->GSSetSamplers(start_slot, count, samplers); break;
		case shader_stage::pixel:		device_context->PSSetSamplers(start_slot, count, samplers); break;
		case shader_stage::compute:		device_context->CSSetSamplers(start_slot, count, samplers); break;
		default: break;
	}
}

void render_command_recorder::set_unordered_access_views(UINT start_slot, UINT count, ID3D11UnorderedAccessView* const* views, const UINT* initial_counts) const
{
	device_context->CSSetUnorderedAccessViews(start_slot, count, views, initial_counts);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void render_command_recorder::begin_render_pass(const render_pass_begin_info& info) const
{
	//	@tool 開始時のクリアを行ってから描画先を設定する
	ID3D11RenderTargetView* views[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = {};
	UINT count = 0;
	for (const auto& target : info.color_targets)
	{
		if (count >= D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT)
			break;
		views[count++] = target.view.Get();
		if (target.clear)
			device_context->ClearRenderTargetView(target.view.Get(), target.clear_value.data());
	}

	const render_pass_depth_target& depth = info.depth_target;
	if (depth.view && (depth.clear_depth || depth.clear_stencil))
	{
		UINT flags = 0;
		if (depth.clear_depth)
			flags |= D3D11_CLEAR_DEPTH;
		if (depth.clear_stencil)
			flags |= D3D11_CLEAR_STENCIL;
		device_context->ClearDepthStencilView(depth.view.Get(), flags, depth.clear_depth_value, depth.clear_stencil_value);
	}
	device_context->OMSetRenderTargets(count, views, depth.view.Get());
}

void render_command_recorder::end_render_pass() const
{
	//	@tool 描画先を外す（後のパスでシェーダーリソースとして読む時に、描画先として残らないようにする）
	ID3D11RenderTargetView* views[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = {};
	device_context->OMSetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, views, nullptr);
}

void render_command_recorder::set_render_targets(UINT count, ID3D11RenderTargetView* const* views, ID3D11DepthStencilView* depth_stencil_view) const
{
	device_context->OMSetRenderTargets(count, views, depth_stencil_view);
}

void render_command_recorder::set_viewport(const D3D11_VIEWPORT& viewport) const
{
	device_context->RSSetViewports(1, &viewport);
}

void render_command_recorder::clear_render_target_view(ID3D11RenderTargetView* view, const FLOAT color[4]) const
{
	device_context->ClearRenderTargetView(view, color);
}

void render_command_recorder::clear_depth_stencil_view(ID3D11DepthStencilView* view, UINT clear_flags, FLOAT depth, UINT8 stencil) const
{
	device_context->ClearDepthStencilView(view, clear_flags, depth, stencil);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void render_command_recorder::draw(UINT vertex_count, UINT start_vertex_location) const
{
	device_context->Draw(vertex_count, start_vertex_location);
}

void render_command_recorder::draw_indexed(UINT index_count, UINT start_index_location, INT base_vertex_location) const
{
	device_context->DrawIndexed(index_count, start_index_location, base_vertex_location);
}

void render_command_recorder::draw_instanced(UINT vertex_count_per_instance, UINT instance_count, UINT start_vertex_location, UINT start_instance_location) const
{
	device_context->DrawInstanced(vertex_count_per_instance, instance_count, start_vertex_location, start_instance_location);
}

void render_command_recorder::draw_instanced_indirect(ID3D11Buffer* args_buffer, UINT aligned_byte_offset) const
{
	device_context->DrawInstancedIndirect(args_buffer, aligned_byte_offset);
}

void render_command_recorder::dispatch(UINT thread_group_x, UINT thread_group_y, UINT thread_group_z) const
{
	device_context->Dispatch(thread_group_x, thread_group_y, thread_group_z);
}

void render_command_recorder::dispatch_indirect(ID3D11Buffer* args_buffer, UINT aligned_byte_offset) const
{
	device_context->DispatchIndirect(args_buffer, aligned_byte_offset);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void render_command_recorder::update_subresource(ID3D11Resource* resource, UINT subresource, const D3D11_BOX* box, const void* data, UINT row_pitch, UINT depth_pitch) const
{
	device_context->UpdateSubresource(resource, subresource, box, data, row_pitch, depth_pitch);
}

HRESULT render_command_recorder::map_write_discard(ID3D11Resource* resource, D3D11_MAPPED_SUBRESOURCE& out_mapped) const
{
	return device_context->Map(resource, 0, D3D11_MAP_WRITE_DISCARD, 0, &out_mapped);
}

void render_command_recorder::unmap(ID3D11Resource* resource) const
{
	device_context->Unmap(resource, 0);
}

void render_command_recorder::copy_resource(ID3D11Resource* destination, ID3D11Resource* source) const
{
	device_context->CopyResource(destination, source);
}

void render_command_recorder::copy_subresource_region(ID3D11Resource* destination, UINT destination_subresource, UINT x, UINT y, UINT z, ID3D11Resource* source, UINT source_subresource, const D3D11_BOX* source_box) const
{
	device_context->CopySubresourceRegion(destination, destination_subresource, x, y, z, source, source_subresource, source_box);
}

void render_command_recorder::copy_structure_count(ID3D11Buffer* destination, UINT destination_aligned_byte_offset, ID3D11UnorderedAccessView* source) const
{
	device_context->CopyStructureCount(destination, destination_aligned_byte_offset, source);
}

void render_command_recorder::clear_unordered_access_view_uint(ID3D11UnorderedAccessView* view, const UINT values[4]) const
{
	device_context->ClearUnorderedAccessViewUint(view, values);
}

void render_command_recorder::clear_unordered_access_view_float(ID3D11UnorderedAccessView* view, const FLOAT values[4]) const
{
	device_context->ClearUnorderedAccessViewFloat(view, values);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void render_command_recorder::begin_event(const wchar_t* name) const
{
#if defined(USE_IMGUI)
	if (annotation)
		annotation->BeginEvent(name);
#else
	name;
#endif	//	defined(USE_IMGUI)
}

void render_command_recorder::end_event() const
{
#if defined(USE_IMGUI)
	if (annotation)
		annotation->EndEvent();
#endif	//	defined(USE_IMGUI)
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool render_command_recorder::read_back(const gpu_readback_buffer& target, std::initializer_list<gpu_readback_source> sources, gpu_readback_receiver receiver) const
{
	const auto& pool = target.get_pool();
	if (!pool || sources.size() != pool->part_sizes.size())
	{
		_ASSERT_EXPR(false, L"read_back : sources do not match the readback buffer.");
		return false;
	}

	gpu_readback_pool::slot* slot = pool->acquire();
	if (!slot)
		return false;

	UINT part = 0;
	for (const auto& source : sources)
	{
		ID3D11Buffer* destination = slot->buffers[part++].Get();
		if (source.structure_count)
			device_context->CopyStructureCount(destination, 0, source.structure_count);
		else if (source.resource)
			device_context->CopyResource(destination, source.resource);
	}

	gpu_readback_request request;
	request.pool = pool;
	request.slot = slot;
	request.receiver = std::move(receiver);
	readback_requests.emplace_back(std::move(request));
	return true;
}

std::vector<gpu_readback_request> render_command_recorder::take_readback_requests() const
{
	std::vector<gpu_readback_request> result;
	result.swap(readback_requests);
	return result;
}
