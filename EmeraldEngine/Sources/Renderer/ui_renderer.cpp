#include "Renderer/ui_renderer.h"

#include "Graphics/UI/sprite.h"

namespace
{
	//	頂点フォーマット
	enum format_id
	{
		position,
		color_0,
		texcoord_0,
		format_max,
	};
	constexpr UINT format_strides[format_max] =
	{
		sizeof(math::float3),
		sizeof(math::float4),
		sizeof(math::float2),
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
		for (int format_index = 0; format_index < format_max; ++format_index)
		{
			auto& view = vertex_buffer_views[format_index];
			view.stride_in_bytes = format_strides[format_index];
			view.size_in_bytes = view.stride_in_bytes * vertex_max;

			D3D11_BUFFER_DESC buffer_desc{};
			buffer_desc.ByteWidth = static_cast<UINT>(view.size_in_bytes);
			buffer_desc.Usage = D3D11_USAGE_DYNAMIC;
			buffer_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
			buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
			buffer_desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;

			hr = device->CreateBuffer(&buffer_desc, nullptr, view.buffer.ReleaseAndGetAddressOf());
			_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

			D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc{};
			srv_desc.Format = DXGI_FORMAT_R32_TYPELESS;
			srv_desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
			srv_desc.BufferEx.NumElements = static_cast<UINT>(view.size_in_bytes / sizeof(UINT));
			srv_desc.BufferEx.Flags = D3D11_BUFFEREX_SRV_FLAG_RAW;
			hr = device->CreateShaderResourceView(view.buffer.Get(), &srv_desc, view.srv.ReleaseAndGetAddressOf());
			_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
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
		buffer_desc.ByteWidth = sizeof(math::float4x4);
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
		render_targets->ui_output.activate(context.device_context);

		//	ビューポートの設定
		context.device_context->RSSetViewports(1, &viewport);

		//	バッファ生成
		std::vector<buffer_view> ibvs;
		{
			HRESULT hr;
			D3D11_MAPPED_SUBRESOURCE mapped_vertices[format_max];
			D3D11_MAPPED_SUBRESOURCE mapped_indices;
			for (int format_index = 0; format_index < format_max; ++format_index)
			{
				hr = context.device_context->Map(vertex_buffer_views[format_index].buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_vertices[format_index]);
				_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
			}
			hr = context.device_context->Map(index_buffer_view.buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_indices);
			_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

			math::float3* positions = static_cast<math::float3*>(mapped_vertices[format_id::position].pData);
			math::float4* colors = static_cast<math::float4*>(mapped_vertices[format_id::color_0].pData);
			math::float2* texcoords = static_cast<math::float2*>(mapped_vertices[format_id::texcoord_0].pData);
			UINT* indices = static_cast<UINT*>(mapped_indices.pData);

			//	スプライト数分をまとめて確保
			size_t sprite_count = context.render_data->sprite_container.sprite_datas.size();
			ibvs.reserve(sprite_count);

			//	各バッファに詰め込む
			buffer_view ibv;
			UINT vertex_size = 0, index_size = 0;
			for (auto& sprite_data : context.render_data->sprite_container.sprite_datas)
			{
				auto& sprite = sprite_data.sprite;

				//	頂点情報書き込み
				size_t vertex_offset = vertex_size;
				vertex_size += static_cast<UINT>(sprite->get_positions().size());
				assert(vertex_size < vertex_max);
				{
					memcpy_s(positions + vertex_offset, sizeof(math::float3) * (vertex_max - vertex_offset),
								sprite->get_positions().data(), sizeof(math::float3) * (vertex_size - vertex_offset));
					memcpy_s(colors + vertex_offset, sizeof(math::float4) * (vertex_max - vertex_offset),
								sprite->get_colors().data(), sizeof(math::float4) * (vertex_size - vertex_offset));
					memcpy_s(texcoords + vertex_offset, sizeof(math::float2) * (vertex_max - vertex_offset),
								sprite->get_texcoords().data(), sizeof(math::float2) * (vertex_size - vertex_offset));
				}
				//	インデックス情報書き込み
				size_t index_offset = index_size;
				index_size += static_cast<UINT>(sprite->get_indices().size());
				assert(index_size < index_max);
				{
					const auto& sprite_indices = sprite->get_indices();
					for (size_t index = 0; index < sprite_indices.size(); ++index)
						indices[index_offset + index] = sprite_indices[index] + static_cast<UINT>(vertex_offset);
				}
				//	各view構築
				{
					ibv.format = index_buffer_view.format;
					ibv.buffer = index_buffer_view.buffer;
					ibv.stride_in_bytes = index_buffer_view.stride_in_bytes;
					ibv.size_in_bytes = (index_size - index_offset) * ibv.stride_in_bytes;
					ibv.offset_in_bytes = index_offset * ibv.stride_in_bytes;
					ibvs.emplace_back(ibv);
				}
			}

			for (auto& view : vertex_buffer_views)
				context.device_context->Unmap(view.buffer.Get(), 0);
			context.device_context->Unmap(index_buffer_view.buffer.Get(), 0);
		}

		//	描画
		sprite_render(context, ibvs);

		render_targets->ui_output.deactivate(context.device_context);

		context.debug_end_event();
	}, "ui_rendering_pass", context, get_render_basis_sort_order());
}

void ui_renderer::sprite_render(graphics_context& context, const std::vector<buffer_view>& ibvs)
{
	//	頂点ストリーム
	ID3D11ShaderResourceView* vertex_streams[format_max] = {};
	for (int format_index = 0; format_index < format_max; ++format_index)
		vertex_streams[format_index] = vertex_buffer_views[format_index].srv.Get();

	UINT buffer_index = 0;
	graphics_pipeline_state_ptr current_pipeline_state;
	for (auto& sprite_data : context.render_data->sprite_container.sprite_datas)
	{
		auto& ibs = ibvs[buffer_index];
		++buffer_index;

		auto& sprite = sprite_data.sprite;

		//	ドメイン&ブレンドモードチェック
		if (sprite->get_material().get_domain() != material_resource::domains::user_interface ||
			sprite->get_material().get_blend_mode() != material_resource::blend_modes::translucent)
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
		math::float4x4 transform_matrix = sprite->get_transform_matrix();
		context.device_context->UpdateSubresource(primitive_constant_buffer.Get(), 0, nullptr, &transform_matrix, 0, 0);

		mesh_draw_item buffer_data{};
		buffer_data.primitive_buffer = primitive_constant_buffer;
		current_pipeline_state->update_parameters(context, sprite->get_material(), &buffer_data);

		//	頂点ストリーム設定
		context.set_vertex_streams(format_max, vertex_streams);

		//	描画
		context.set_index_buffer(ibs.buffer.Get(), ibs.format, 0);
		context.set_primitive_topology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		context.device_context->DrawIndexed(static_cast<UINT>(ibs.count()), static_cast<UINT>(ibs.offset()), 0);
	}
	if (current_pipeline_state)
		current_pipeline_state->end(context);
}

void ui_renderer::resize_buffer(int width, int height)
{
	//	ビューポートの設定
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	viewport.Width = static_cast<float>(render_targets->ui_output.get_size().x);
	viewport.Height = static_cast<float>(render_targets->ui_output.get_size().y);
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
}
