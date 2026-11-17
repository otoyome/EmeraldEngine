#include "Graphics/Core/graphics_context.h"

#include "Graphics/Core/render_data.h"
#include "System/misc.h"

namespace
{
	//	StructuredBuffer Ç™ïKóvêîÇñûÇΩÇ≥Ç»ÇØÇÍÇŒ 2 ÇÃÇ◊Ç´èÊÇ≈ämï€ÇµíºÇ∑
	void regenerate_structured_buffer(
		Microsoft::WRL::ComPtr<ID3D11Buffer>& buffer,
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& shader_resource_view,
		UINT& capacity,
		UINT stride,
		std::size_t required_count)
	{
		UINT need = static_cast<UINT>(required_count);
		if (buffer && capacity >= need)
			return;

		UINT new_capacity = capacity ? capacity : 256;
		while (new_capacity < need)
			new_capacity *= 2;

		auto device = render_data::instance()->get_device();

		D3D11_BUFFER_DESC buffer_desc = {};
		buffer_desc.Usage = D3D11_USAGE_DEFAULT;
		buffer_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		buffer_desc.ByteWidth = stride * new_capacity;
		buffer_desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		buffer_desc.StructureByteStride = stride;

		Microsoft::WRL::ComPtr<ID3D11Buffer> new_buffer;
		HRESULT hr = device->CreateBuffer(&buffer_desc, nullptr, new_buffer.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

		D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
		srv_desc.Format = DXGI_FORMAT_UNKNOWN;
		srv_desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		srv_desc.Buffer.FirstElement = 0;
		srv_desc.Buffer.NumElements = new_capacity;

		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> new_srv;
		hr = device->CreateShaderResourceView(new_buffer.Get(), &srv_desc, new_srv.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

		buffer = new_buffer;
		shader_resource_view = new_srv;
		capacity = new_capacity;
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void mesh_container::build_transform_buffers()
{
	if (primitive_header_scratch.empty())
		primitive_header_scratch.emplace_back();
	if (bone_matrix_scratch.empty())
		bone_matrix_scratch.emplace_back();

	regenerate_structured_buffer(primitive_header_buffer, primitive_header_srv, primitive_header_capacity,
		sizeof(common_pipeline_states::primitive_header), primitive_header_scratch.size());
	regenerate_structured_buffer(bone_matrix_buffer, bone_matrix_srv, bone_matrix_capacity,
		sizeof(math::float4x4), bone_matrix_scratch.size());

	auto& upload_list = render_data::instance()->get_upload_list();
	auto upload_range = [&](ID3D11Buffer* buffer, const void* source, UINT byte_size)
	{
		upload_list.update_buffer(buffer, 0, source, byte_size);
	};
	upload_range(primitive_header_buffer.Get(), primitive_header_scratch.data(),
		static_cast<UINT>(primitive_header_scratch.size() * sizeof(common_pipeline_states::primitive_header)));
	upload_range(bone_matrix_buffer.Get(), bone_matrix_scratch.data(),
		static_cast<UINT>(bone_matrix_scratch.size() * sizeof(math::float4x4)));
}

void mesh_container::clear()
{
	draw_datas.clear();
	shadow_datas.clear();
	color_datas.clear();
	primitive_header_scratch.clear();
	bone_matrix_scratch.clear();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void effect_container::clear()
{
	sprite_trail_draw_datas.clear();
	sprite_trail_color_datas.clear();
	sprite_trail_shadow_datas.clear();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void decal_container::clear()
{
	decal_datas.clear();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void graphics_context::set_rasterizer_state(ID3D11RasterizerState* state) const
{
	if (chache_rasterizer_state == state)
		return;
	device_context->RSSetState(state);
	chache_rasterizer_state = state;
}

void graphics_context::set_depth_stencil_state(ID3D11DepthStencilState* state, UINT stencil_ref) const
{
	if (chache_depth_stencil_state == state && chache_stencil_ref == stencil_ref)
		return;
	device_context->OMSetDepthStencilState(state, stencil_ref);
	chache_depth_stencil_state = state;
	chache_stencil_ref = stencil_ref;
}

void graphics_context::set_blend_state(ID3D11BlendState* state) const
{
	if (chache_blend_state == state)
		return;
	device_context->OMSetBlendState(state, nullptr, 0xFFFFFFFF);
	chache_blend_state = state;
}

void graphics_context::set_primitive_topology(D3D_PRIMITIVE_TOPOLOGY topology) const
{
	if (chache_primitive_topology == topology)
		return;
	device_context->IASetPrimitiveTopology(topology);
	chache_primitive_topology = topology;
}

void graphics_context::set_vertex_buffers(UINT start_slot, UINT num_buffers, ID3D11Buffer* const* buffers, const UINT* strides, const UINT* offsets) const
{
	bool is_same = (chache_vertex_buffer_start_slot == start_slot) && (chache_vertex_buffer_count == num_buffers);
	for (UINT index = 0; is_same && index < num_buffers; ++index)
	{
		if (chache_vertex_buffers[index] != buffers[index] ||
			chache_vertex_buffer_strides[index] != strides[index] ||
			chache_vertex_buffer_offsets[index] != offsets[index])
		{
			is_same = false;
		}
	}
	if (is_same)
		return;

	device_context->IASetVertexBuffers(start_slot, num_buffers, buffers, strides, offsets);

	chache_vertex_buffer_start_slot = start_slot;
	chache_vertex_buffer_count = num_buffers;
	for (UINT index = 0; index < num_buffers; ++index)
	{
		chache_vertex_buffers[index] = buffers[index];
		chache_vertex_buffer_strides[index] = strides[index];
		chache_vertex_buffer_offsets[index] = offsets[index];
	}
}

void graphics_context::set_index_buffer(ID3D11Buffer* buffer, DXGI_FORMAT format, UINT offset) const
{
	if (chache_index_buffer == buffer && chache_index_buffer_format == format && chache_index_buffer_offset == offset)
		return;
	device_context->IASetIndexBuffer(buffer, format, offset);
	chache_index_buffer = buffer;
	chache_index_buffer_format = format;
	chache_index_buffer_offset = offset;
}

void graphics_context::set_vertex_streams(UINT num_streams, ID3D11ShaderResourceView* const* streams) const
{
	device_context->VSSetShaderResources(common_pipeline_states::vertex_stream_srv_index, num_streams, streams);
}

void graphics_context::clear_shader_slots() const
{
	device_context->VSSetShader(nullptr, nullptr, 0);
	device_context->HSSetShader(nullptr, nullptr, 0);
	device_context->DSSetShader(nullptr, nullptr, 0);
	device_context->GSSetShader(nullptr, nullptr, 0);
	device_context->PSSetShader(nullptr, nullptr, 0);
	device_context->CSSetShader(nullptr, nullptr, 0);
}

void graphics_context::clear_vertex_buffer() const
{
	ID3D11Buffer* vbs[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT] = {};
	UINT dummy[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT] = {};
	set_vertex_buffers(0, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT, vbs, dummy, dummy);
}

void graphics_context::clear_index_buffer() const
{
	set_index_buffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
}

void graphics_context::clear_render_target_slots() const
{
	device_context->OMSetRenderTargets(0, nullptr, nullptr);
}

void graphics_context::clear_constant_buffers(int start_slot, int num) const
{
	ID3D11Buffer* clear_constant_bufferes[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT]{};
	device_context->VSSetConstantBuffers(start_slot, num, clear_constant_bufferes);
	device_context->HSSetConstantBuffers(start_slot, num, clear_constant_bufferes);
	device_context->DSSetConstantBuffers(start_slot, num, clear_constant_bufferes);
	device_context->GSSetConstantBuffers(start_slot, num, clear_constant_bufferes);
	device_context->PSSetConstantBuffers(start_slot, num, clear_constant_bufferes);
	device_context->CSSetConstantBuffers(start_slot, num, clear_constant_bufferes);
}

void graphics_context::clear_shader_resorce_views(int start_slot, int num) const
{
	ID3D11ShaderResourceView* clear_shader_resource_view[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT]{};
	device_context->VSSetShaderResources(start_slot, num, clear_shader_resource_view);
	device_context->HSSetShaderResources(start_slot, num, clear_shader_resource_view);
	device_context->DSSetShaderResources(start_slot, num, clear_shader_resource_view);
	device_context->GSSetShaderResources(start_slot, num, clear_shader_resource_view);
	device_context->PSSetShaderResources(start_slot, num, clear_shader_resource_view);
	device_context->CSSetShaderResources(start_slot, num, clear_shader_resource_view);
}

void graphics_context::clear_unordered_access_views(int start_slot, int num) const
{
	ID3D11UnorderedAccessView* clear_unordered_access_view[8]{};
	device_context->CSSetUnorderedAccessViews(start_slot, num, clear_unordered_access_view, nullptr);
}

void graphics_context::clear_sampler(int start_slot, int num) const
{
	ID3D11SamplerState* clear_sampler[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT]{};
	device_context->VSSetSamplers(start_slot, num, clear_sampler);
	device_context->HSSetSamplers(start_slot, num, clear_sampler);
	device_context->DSSetSamplers(start_slot, num, clear_sampler);
	device_context->GSSetSamplers(start_slot, num, clear_sampler);
	device_context->PSSetSamplers(start_slot, num, clear_sampler);
	device_context->CSSetSamplers(start_slot, num, clear_sampler);
}

#if defined(USE_IMGUI)
namespace
{
	ID3DUserDefinedAnnotation* get_user_defined_annotation(ID3D11DeviceContext* device_context)
	{
		thread_local ID3D11DeviceContext* cached_context = nullptr;
		thread_local Microsoft::WRL::ComPtr<ID3DUserDefinedAnnotation> cached_annotation;
		if (cached_context != device_context)
		{
			cached_annotation.Reset();
			if (FAILED(device_context->QueryInterface(__uuidof(ID3DUserDefinedAnnotation),
				reinterpret_cast<void**>(cached_annotation.GetAddressOf()))))
			{
				cached_context = nullptr;
				return nullptr;
			}
			cached_context = device_context;
		}
		return cached_annotation.Get();
	}
}
#endif	//	defined(USE_IMGUI)

void graphics_context::debug_begin_event(const wchar_t* event_name)
{
#if defined(USE_IMGUI)
	if (ID3DUserDefinedAnnotation* annotation = get_user_defined_annotation(device_context))
		annotation->BeginEvent(event_name);
#endif	//	defined(USE_IMGUI)
}

void graphics_context::debug_end_event()
{
#if defined(USE_IMGUI)
	if (ID3DUserDefinedAnnotation* annotation = get_user_defined_annotation(device_context))
		annotation->EndEvent();
#endif	//	defined(USE_IMGUI)
}
