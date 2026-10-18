#include "Renderer/scene_render_features.h"

#include "sprite.h"

using namespace scene_rendering;

namespace
{
	static constexpr size_t vertex_max = 0xFFFF;
	static constexpr size_t index_max = 0x000FFFFF;

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

	struct sprite_buffer_views
	{
		buffer_view views[3];
	};

	void sprite_render(render_job_context& context, const ui_resources& ui, const std::vector<sprite_buffer_views>& vbvs, const std::vector<buffer_view>& ibvs)
	{
		static const semantic base_vertex_semantic("POSITION", 0);

		UINT buffer_index = 0;
		graphics_pipeline_state_ptr current_pipeline_state;
		for (auto& sprite_data : context.render_data->sprite_container.draw_items)
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
					context.recorder.end_pipeline();

				current_pipeline_state = pipeline_state;
				context.recorder.set_pipeline(*current_pipeline_state);
			}

			//	プリミティブ定数バッファ更新
			math::float4x4 transform_matrix = sprite->get_transform_matrix();
			context.recorder.update_subresource(ui.primitive_constant_buffer.Get(), 0, nullptr, &transform_matrix, 0, 0);

			mesh_draw_item draw_item{};
			draw_item.primitive_buffer = ui.primitive_constant_buffer;
			context.recorder.bind_draw(*current_pipeline_state, sprite->get_material(), &draw_item);

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
				context.recorder.set_vertex_buffers(0, stream_index, stream_buffers, stream_strides, stream_offsets);

			//	描画
			context.recorder.set_index_buffer(ibs.buffer.Get(), ibs.format, 0);
			context.recorder.set_primitive_topology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			context.recorder.draw_indexed(static_cast<UINT>(ibs.count()), static_cast<UINT>(ibs.offset()), base_vertex_location);
		}
		if (current_pipeline_state)
			context.recorder.end_pipeline();
	}
}

//	@tool UI の共有資源（頂点・インデックスの動的バッファと定数バッファ）
std::shared_ptr<const ui_resources> scene_rendering::create_ui_resources()
{
	auto next = std::make_shared<ui_resources>();
	std::unordered_map<semantic, buffer_view>& vertex_buffer_views = next->vertex_buffer_views;
	buffer_view& index_buffer_view = next->index_buffer_view;
	Microsoft::WRL::ComPtr<ID3D11Buffer>& primitive_constant_buffer = next->primitive_constant_buffer;
	HRESULT hr;
	auto device = render_data::instance()->get_device();

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
		buffer_desc.ByteWidth = sizeof(math::float4x4);
		buffer_desc.Usage = D3D11_USAGE_DEFAULT;
		buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		buffer_desc.CPUAccessFlags = 0;

		hr = device->CreateBuffer(&buffer_desc, nullptr, primitive_constant_buffer.ReleaseAndGetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	}
	return next;
}

//	UIパス
void scene_rendering::add_ui_pass(frame_graph& graph, const renderer_context& context, const scene_output& output_target, const scene_render_features& features)
{
	struct pass_data
	{
		std::shared_ptr<const ui_resources> resources;
		D3D11_VIEWPORT viewport{};
	};
	graph.add_pass<pass_data>("ui_rendering_pass", context, [&](frame_graph::builder& builder, pass_data& data)
	{
		//	@tool 出力先（バックバッファ）へ重ねて描く
		frame_graph::color_attachment output;
		output.external_view = output_target.view;
		output.clear = false;
		builder.set_render_targets({ output });
		data.viewport = output_target.viewport;
		data.resources = features.ui;
	},
	[](const pass_data& data, const frame_graph::resources&, render_job_context& context)
	{
		const ui_resources& ui = *data.resources;
		context.recorder.begin_event(L"ui_rendering");

		//	ビューポートの設定
		context.recorder.set_viewport(data.viewport);

		//	バッファ生成
		std::vector<sprite_buffer_views> vbvs;
		std::vector<buffer_view> ibvs;
		{
			HRESULT hr;
			D3D11_MAPPED_SUBRESOURCE mapped_vertices[3];
			D3D11_MAPPED_SUBRESOURCE mapped_indices;
			for (auto& it : ui.vertex_buffer_views)
			{
				if (it.first == format_datas[format_id::position].s)
					hr = context.recorder.map_write_discard(it.second.buffer.Get(), mapped_vertices[format_id::position]);
				else if (it.first == format_datas[format_id::color_0].s)
					hr = context.recorder.map_write_discard(it.second.buffer.Get(), mapped_vertices[format_id::color_0]);
				else if (it.first == format_datas[format_id::texcoord_0].s)
					hr = context.recorder.map_write_discard(it.second.buffer.Get(), mapped_vertices[format_id::texcoord_0]);
				_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
			}
			hr = context.recorder.map_write_discard(ui.index_buffer_view.buffer.Get(), mapped_indices);
			_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

			math::float3* positions = static_cast<math::float3*>(mapped_vertices[format_id::position].pData);
			math::float4* colors = static_cast<math::float4*>(mapped_vertices[format_id::color_0].pData);
			math::float2* texcoords = static_cast<math::float2*>(mapped_vertices[format_id::texcoord_0].pData);
			UINT* indices = static_cast<UINT*>(mapped_indices.pData);

			//	各セマンティックのバッファ/フォーマット/ストライドはフレーム内で共通
			buffer_view view_templates[3];
			for (auto& it : ui.vertex_buffer_views)
			{
				if (it.first == format_datas[format_id::position].s)
					view_templates[format_id::position] = it.second;
				else if (it.first == format_datas[format_id::color_0].s)
					view_templates[format_id::color_0] = it.second;
				else if (it.first == format_datas[format_id::texcoord_0].s)
					view_templates[format_id::texcoord_0] = it.second;
			}

			//	スプライト数分をまとめて確保
			size_t sprite_count = context.render_data->sprite_container.draw_items.size();
			vbvs.reserve(sprite_count);
			ibvs.reserve(sprite_count);

			//	各バッファに詰め込む
			sprite_buffer_views views;
			buffer_view ibv;
			UINT vertex_size = 0, index_size = 0;
			for (auto& sprite_data : context.render_data->sprite_container.draw_items)
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
					memcpy_s(indices + index_offset, sizeof(UINT) * (index_max - index_offset),
								sprite->get_indices().data(), sizeof(UINT) * (index_size - index_offset));
				}
				//	各view構築
				{
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

					ibv.format = ui.index_buffer_view.format;
					ibv.buffer = ui.index_buffer_view.buffer;
					ibv.stride_in_bytes = ui.index_buffer_view.stride_in_bytes;
					ibv.size_in_bytes = (index_size - index_offset) * ibv.stride_in_bytes;
					ibv.offset_in_bytes = index_offset * ibv.stride_in_bytes;
					ibvs.emplace_back(ibv);
				}
			}

			for (auto& it : ui.vertex_buffer_views)
				context.recorder.unmap(it.second.buffer.Get());
			context.recorder.unmap(ui.index_buffer_view.buffer.Get());
		}

		//	描画
		sprite_render(context, ui, vbvs, ibvs);

		context.recorder.end_event();
	});
}
