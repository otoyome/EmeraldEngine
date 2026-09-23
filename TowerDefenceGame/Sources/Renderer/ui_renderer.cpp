#include "Renderer/ui_renderer.h"

#include "sprite.h"

namespace
{
	//	頂点フォーマット
	struct format_data
	{
		semantic s;
		DXGI_FORMAT f;
	};
	enum format_id
	{
		position,
		color_0,
		texcoord_0,
	};
	const format_data format_datas[] =
	{
		{{"POSITION", 0}, DXGI_FORMAT_R32G32B32_FLOAT},
		{{"COLOR", 0}, DXGI_FORMAT_R32G32B32A32_FLOAT},
		{{"TEXCOORD", 0}, DXGI_FORMAT_R32G32_FLOAT},
	};
}

ui_renderer::ui_renderer(render_job_registrar job_executor, std::shared_ptr<scene_render_targets> render_targets)
: renderer(job_executor)
, render_targets(render_targets)
, viewport({ 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f })
{
	HRESULT hr;
	auto device = render_data::instance()->get_device();

	resize_buffer(0, 0);

	//	頂点バッファビュー生成
	{
		for (auto& format : format_datas)
		{
			buffer_view view;
			view.format = format.f;
			view.stride_in_bytes = graphics_utility::get_byte_offset_from_format(view.format);
			view.size_in_bytes = view.stride_in_bytes * vertex_max;

			D3D11_BUFFER_DESC buffer_desc{};
			buffer_desc.ByteWidth = static_cast<UINT>(view.size_in_bytes);
			buffer_desc.Usage = D3D11_USAGE_DYNAMIC;
			buffer_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
			buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

			hr = device->CreateBuffer(&buffer_desc, nullptr, view.buffer.ReleaseAndGetAddressOf());
			_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

			vertex_buffer_views.insert_or_assign(format.s, view);
		}
	}

	//	インデックスバッファビュー生成
	{
		auto& view = index_buffer_view;
		view.format = DXGI_FORMAT_R32_UINT;
		view.stride_in_bytes = graphics_utility::get_byte_offset_from_format(view.format);
		view.size_in_bytes = view.stride_in_bytes * index_max;

		D3D11_BUFFER_DESC buffer_desc{};
		buffer_desc.ByteWidth = static_cast<UINT>(view.size_in_bytes);
		buffer_desc.Usage = D3D11_USAGE_DYNAMIC;
		buffer_desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
		buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

		hr = device->CreateBuffer(&buffer_desc, nullptr, view.buffer.ReleaseAndGetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	}

	//	定数バッファ生成
	{
		D3D11_BUFFER_DESC buffer_desc{};
		buffer_desc.ByteWidth = sizeof(DirectX::XMFLOAT4X4);
		buffer_desc.Usage = D3D11_USAGE_DEFAULT;
		buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		buffer_desc.CPUAccessFlags = 0;

		hr = device->CreateBuffer(&buffer_desc, nullptr, primitive_constant_buffer.ReleaseAndGetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	}
}

ui_renderer::~ui_renderer()
{
}

void ui_renderer::render(renderer_context& context)
{
	job_executor.execute([this](graphics_context& context)
	{
		context.debug_begin_event(L"ui_rendering");

		//	描画先設定
		rendering_pass.activate(context.device_context);

		//	ビューポートの設定
		context.device_context->RSSetViewports(1, &viewport);

		//	バッファ生成
		std::vector<sprite_buffer_views> vbvs;
		std::vector<buffer_view> ibvs;
		{
			HRESULT hr;
			D3D11_MAPPED_SUBRESOURCE mapped_vertices[3];
			D3D11_MAPPED_SUBRESOURCE mapped_indices;
			for (auto& it : vertex_buffer_views)
			{
				if (it.first == format_datas[format_id::position].s)
					hr = context.device_context->Map(it.second.buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_vertices[format_id::position]);
				else if (it.first == format_datas[format_id::color_0].s)
					hr = context.device_context->Map(it.second.buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_vertices[format_id::color_0]);
				else if (it.first == format_datas[format_id::texcoord_0].s)
					hr = context.device_context->Map(it.second.buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_vertices[format_id::texcoord_0]);
				_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
			}
			hr = context.device_context->Map(index_buffer_view.buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_indices);
			_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

			DirectX::XMFLOAT3* positions = static_cast<DirectX::XMFLOAT3*>(mapped_vertices[format_id::position].pData);
			DirectX::XMFLOAT4* colors = static_cast<DirectX::XMFLOAT4*>(mapped_vertices[format_id::color_0].pData);
			DirectX::XMFLOAT2* texcoords = static_cast<DirectX::XMFLOAT2*>(mapped_vertices[format_id::texcoord_0].pData);
			UINT* indices = static_cast<UINT*>(mapped_indices.pData);

			//	各セマンティックのバッファ/フォーマット/ストライドはフレーム内で共通
			buffer_view view_templates[3];
			for (auto& it : vertex_buffer_views)
			{
				if (it.first == format_datas[format_id::position].s)
					view_templates[format_id::position] = it.second;
				else if (it.first == format_datas[format_id::color_0].s)
					view_templates[format_id::color_0] = it.second;
				else if (it.first == format_datas[format_id::texcoord_0].s)
					view_templates[format_id::texcoord_0] = it.second;
			}

			//	スプライト数分をまとめて確保
			size_t sprite_count = context.render_data->sprite_container.sprite_datas.size();
			vbvs.reserve(sprite_count);
			ibvs.reserve(sprite_count);

			//	各バッファに詰め込む
			UINT vertex_size = 0, index_size = 0;
			for (auto& sprite_data : context.render_data->sprite_container.sprite_datas)
			{
				auto& sprite = sprite_data.sprite;

				//	頂点情報書き込み
				size_t vertex_offset = vertex_size;
				vertex_size += static_cast<UINT>(sprite->get_positions().size());
				assert(vertex_size < vertex_max);
				{
					memcpy_s(positions + vertex_offset, sizeof(DirectX::XMFLOAT3) * (vertex_max - vertex_offset),
								sprite->get_positions().data(), sizeof(DirectX::XMFLOAT3) * (vertex_size - vertex_offset));
					memcpy_s(colors + vertex_offset, sizeof(DirectX::XMFLOAT4) * (vertex_max - vertex_offset),
								sprite->get_colors().data(), sizeof(DirectX::XMFLOAT4) * (vertex_size - vertex_offset));
					memcpy_s(texcoords + vertex_offset, sizeof(DirectX::XMFLOAT2) * (vertex_max - vertex_offset),
								sprite->get_texcoords().data(), sizeof(DirectX::XMFLOAT2) * (vertex_size - vertex_offset));
				}
				//	インデックス情報書き込み
				size_t index_offset = index_size;
				index_size += static_cast<UINT>(sprite->get_indices().size());
				assert(index_size < index_max);
				{
					memcpy_s(indices + index_offset, sizeof(UINT) * (index_max - index_offset),
								sprite->get_indices().data(), sizeof(UINT) * (index_size - index_offset));
				}
				//	各view構築
				{
					sprite_buffer_views views;
					for (int format_index = 0; format_index < 3; ++format_index)
					{
						auto& view = views.views[format_index];
						auto& view_template = view_templates[format_index];
						view.format = view_template.format;
						view.buffer = view_template.buffer;
						view.stride_in_bytes = view_template.stride_in_bytes;
						view.size_in_bytes = (vertex_size - vertex_offset) * view_template.stride_in_bytes;
						view.offset_in_bytes = vertex_offset * view_template.stride_in_bytes;
					}
					vbvs.emplace_back(views);

					buffer_view ibv;
					ibv.format = index_buffer_view.format;
					ibv.buffer = index_buffer_view.buffer;
					ibv.stride_in_bytes = index_buffer_view.stride_in_bytes;
					ibv.size_in_bytes = (index_size - index_offset) * ibv.stride_in_bytes;
					ibv.offset_in_bytes = index_offset * ibv.stride_in_bytes;
					ibvs.emplace_back(ibv);
				}
			}

			for (auto& it : vertex_buffer_views)
				context.device_context->Unmap(it.second.buffer.Get(), 0);
			context.device_context->Unmap(index_buffer_view.buffer.Get(), 0);
		}

		//	描画
		sprite_render(context, vbvs, ibvs);

		rendering_pass.deactivate(context.device_context);

		context.debug_end_event();
	}, "ui_rendering_pass", context, get_render_basis_sort_order());
}

void ui_renderer::sprite_render(graphics_context& context, const std::vector<sprite_buffer_views>& vbvs, const std::vector<buffer_view>& ibvs)
{
	static const semantic base_vertex_semantic("POSITION", 0);

	UINT buffer_index = 0;
	graphics_pipeline_state_ptr current_pipeline_state;
	for (auto& sprite_data : context.render_data->sprite_container.sprite_datas)
	{
		auto& vbs = vbvs[buffer_index];
		auto& ibs = ibvs[buffer_index];
		++buffer_index;

		auto& sprite = sprite_data.sprite;

		//	ドメイン&ブレンドモードチェック
		if (sprite->get_material().get_domain() != material::domains::user_interface ||
			sprite->get_material().get_blend_mode() != material::blend_modes::translucent)
			continue;

		auto pipeline_state = sprite->get_material().get_pipeline_state();
		if (!pipeline_state)
			continue;

		if (pipeline_state != current_pipeline_state)
		{
			if (current_pipeline_state)
				current_pipeline_state->end(context);

			current_pipeline_state = pipeline_state;
			current_pipeline_state->begin(context);
		}

		//	プリミティブ定数バッファ更新
		DirectX::XMFLOAT4X4 transform_matrix = sprite->get_transform_matrix();
		context.device_context->UpdateSubresource(primitive_constant_buffer.Get(), 0, nullptr, &transform_matrix, 0, 0);

		buffer_datas buffer_data{};
		buffer_data.primitive_buffer = primitive_constant_buffer;
		current_pipeline_state->update_parameters(context, sprite->get_material(), &buffer_data);

		//	頂点ストリーム設定
		ID3D11Buffer* stream_buffers[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT] = {};
		UINT stream_strides[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT] = {};
		UINT stream_offsets[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT] = {};
		UINT stream_index = 0;
		UINT base_vertex_location = 0;
		for (auto& semantic : current_pipeline_state->get_semantic_list())
		{
			if (stream_index >= D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT)
				break;
			const buffer_view* view = nullptr;
			if (semantic.first == format_datas[format_id::position].s)
				view = &vbs.views[format_id::position];
			else if (semantic.first == format_datas[format_id::color_0].s)
				view = &vbs.views[format_id::color_0];
			else if (semantic.first == format_datas[format_id::texcoord_0].s)
				view = &vbs.views[format_id::texcoord_0];

			if (view && view->buffer)
			{
				stream_buffers[stream_index] = view->buffer.Get();
				stream_strides[stream_index] = static_cast<UINT>(view->stride_in_bytes);
				stream_offsets[stream_index] = 0;
				if (semantic.first == base_vertex_semantic)
					base_vertex_location = static_cast<UINT>(view->offset());
			}
			++stream_index;
		}
		if (stream_index > 0)
			context.set_vertex_buffers(0, stream_index, stream_buffers, stream_strides, stream_offsets);

		//	描画
		context.set_index_buffer(ibs.buffer.Get(), ibs.format, 0);
		context.set_primitive_topology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		context.device_context->DrawIndexed(static_cast<UINT>(ibs.count()), static_cast<UINT>(ibs.offset()), base_vertex_location);
	}
	if (current_pipeline_state)
		current_pipeline_state->end(context);
}

void ui_renderer::resize_buffer(int width, int height)
{
	rendering_pass.render_targets.clear();
	rendering_pass.render_targets.emplace_back(render_data::instance()->get_swap_chain());
	rendering_pass.render_targets.back().clear_color = false;

	DXGI_SWAP_CHAIN_DESC desc;
	render_data::instance()->get_swap_chain()->GetDesc(&desc);

	//	ビューポートの設定
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	viewport.Width = static_cast<float>(desc.BufferDesc.Width);
	viewport.Height = static_cast<float>(desc.BufferDesc.Height);
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
}
