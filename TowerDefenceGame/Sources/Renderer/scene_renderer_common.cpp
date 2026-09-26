#include "Renderer/scene_renderer_common.h"
#include "model_buffer_pool.h"

float scene_renderer_commons::convert_linear_depth_to_projection_depth(float depth, float z_buffer_parameteres_y)
{
	float z_buffer_parameteres_x = 1.0f - z_buffer_parameteres_y;
	return (1.0f / depth - z_buffer_parameteres_y) / z_buffer_parameteres_x;
}

void scene_renderer_commons::draw(const graphics_context& context,
			graphics_pipeline_state_ptr pipeline_state,
			UINT vertex_count,
			D3D_PRIMITIVE_TOPOLOGY primitive_topology,
			UINT vertex_offset)
{
	context.set_index_buffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
	context.set_primitive_topology(primitive_topology);
	context.device_context->Draw(vertex_count, vertex_offset);
}

void scene_renderer_commons::draw_indexed(const graphics_context& context,
					float mesh_length,
					const buffer_datas& buffer_data,
					graphics_pipeline_state_ptr pipeline_state)
{
	const buffer_datas::sub_mesh* draw_mesh = nullptr;
	ID3D11ShaderResourceView* morph_positions[common_pipeline_states::morph_max] = {};
	ID3D11ShaderResourceView* morph_normals[common_pipeline_states::morph_max] = {};
	if (context.is_shadow_rendering)
	{
		//	シャドウメッシュ対応
		draw_mesh = &buffer_data.shadow_mesh;
	}
	else
	{
		//	LOD対応
		std::size_t lod_count = buffer_data.color_meshs.size();
		std::size_t lod_mesh_index = 0;
		if (lod_count > 1)
		{
			float lod_range_min = buffer_data.lod_range_rate.x * context.scene_constant.camera_clip_distance.x;
			float lod_range_max = buffer_data.lod_range_rate.y * context.scene_constant.camera_clip_distance.y;
			float lod_range = lod_range_max - lod_range_min;
			float lod_rate = (lod_range > 1e-6f) ? math::saturate((mesh_length - lod_range_min) / lod_range) : 0.0f;
			lod_mesh_index = static_cast<std::size_t>(lod_rate * static_cast<float>(lod_count - 1) + 0.5f);
			if (lod_mesh_index >= lod_count)
				lod_mesh_index = lod_count - 1;
		}
		draw_mesh = &buffer_data.color_meshs[lod_mesh_index];

		//	モーフィング対応
		if (!draw_mesh->morph_meshes.empty())
		{
			static const semantic position_semantic("POSITION", 0);
			static const semantic normal_semantic("NORMAL", 0);
			int position_count = 0, normal_count = 0;
			for (auto& morph_mesh : draw_mesh->morph_meshes)
			{
				auto& vbv = morph_mesh.vertex_buffer_views;

				auto it_positions = vbv->find(position_semantic);
				auto it_normals = vbv->find(normal_semantic);
				if (it_positions != vbv->end())
					morph_positions[position_count++] = it_positions->second.srv.Get();
				if (it_normals != vbv->end())
					morph_normals[normal_count++] = it_normals->second.srv.Get();
			}
		}
	}

	//	メッシュレットが無ければ描画しない
	if (!draw_mesh->meshlets || draw_mesh->meshlets->empty())
		return;

	//	モーフィングバッファ設定
	context.device_context->VSSetShaderResources(common_pipeline_states::morph_srv_position_index,
												 common_pipeline_states::morph_max, morph_positions);
	context.device_context->VSSetShaderResources(common_pipeline_states::morph_srv_normal_index,
												 common_pipeline_states::morph_max, morph_normals);

	//	メッシュレットの頂点はメッシュローカルな頂点番号なので、
	//	プール内でのメッシュの先頭頂点位置を算出
	UINT mesh_base_vertex = 0;
	{
		static const semantic position_semantic("POSITION", 0);
		auto it = draw_mesh->vertex_buffer_views->find(position_semantic);
		if (it != draw_mesh->vertex_buffer_views->end())
			mesh_base_vertex = static_cast<UINT>(it->second.offset());
	}

	//	頂点バッファ登録
	{
		static const semantic position_semantic("POSITION", 0);
		static const semantic normal_semantic("NORMAL", 0);
		static const semantic tangent_semantic("TANGENT", 0);
		static const semantic color0_semantic("COLOR", 0);
		static const semantic texcoord0_semantic("TEXCOORD", 0);
		static const semantic texcoord1_semantic("TEXCOORD", 1);
		static const semantic joints0_semantic("JOINTS", 0);
		static const semantic joints1_semantic("JOINTS", 1);
		static const semantic joints2_semantic("JOINTS", 2);
		static const semantic joints3_semantic("JOINTS", 3);
		static const semantic weights0_semantic("WEIGHTS", 0);
		static const semantic weights1_semantic("WEIGHTS", 1);
		static const semantic weights2_semantic("WEIGHTS", 2);
		static const semantic weights3_semantic("WEIGHTS", 3);

		auto pool = model_buffer_pool_manager::instance();
		ID3D11ShaderResourceView* attribute_srvs[16] =
		{
			pool->get_vertex_buffer_srv(position_semantic),
			pool->get_vertex_buffer_srv(normal_semantic),
			pool->get_vertex_buffer_srv(tangent_semantic),
			pool->get_vertex_buffer_srv(color0_semantic),
			pool->get_vertex_buffer_srv(texcoord0_semantic),
			pool->get_vertex_buffer_srv(texcoord1_semantic),
			pool->get_vertex_buffer_srv(joints0_semantic),
			pool->get_vertex_buffer_srv(joints1_semantic),
			pool->get_vertex_buffer_srv(joints2_semantic),
			pool->get_vertex_buffer_srv(joints3_semantic),
			pool->get_vertex_buffer_srv(weights0_semantic),
			pool->get_vertex_buffer_srv(weights1_semantic),
			pool->get_vertex_buffer_srv(weights2_semantic),
			pool->get_vertex_buffer_srv(weights3_semantic),
			pool->get_meshlet_vertex_buffer_srv(),
			pool->get_meshlet_triangle_buffer_srv(),
		};
		context.device_context->VSSetShaderResources(102, 16, attribute_srvs);
	}

	//	メッシュレット単位で描画するので通常の頂点バッファ等は使用しない
	context.set_vertex_buffers(0, 0, nullptr, nullptr, nullptr);
	context.set_index_buffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
	context.set_primitive_topology(draw_mesh->primitive_topology);

	auto meshlet_constant_buffer = model_buffer_pool_manager::instance()->get_meshlet_draw_constant_buffer();
	context.device_context->VSSetConstantBuffers(common_pipeline_states::meshlet_cbv_index, 1, &meshlet_constant_buffer);

	for (auto& meshlet : *draw_mesh->meshlets)
	{
		common_pipeline_states::meshlet_draw_constants constants{};
		constants.vertex_offset = meshlet.vertex_offset;
		constants.triangle_offset = meshlet.triangle_offset;
		constants.mesh_base_vertex = mesh_base_vertex;
		context.device_context->UpdateSubresource(meshlet_constant_buffer, 0, nullptr, &constants, 0, 0);

		//	@tool SV_VertexIDはStartVertexLocationを含まないため、オフセットは定数バッファ(triangle_offset)経由で渡す
		context.device_context->Draw(meshlet.triangle_count * 3, 0);
	}
}

void scene_renderer_commons::model_render(graphics_context& context,
					const std::vector<std::size_t>& mesh_indices,
					const std::vector<float>& mesh_lengths,
					material::surface_paths surface_path)
{
	auto draw_data_ptr = context.render_data->mesh_container.draw_datas.data();
	auto material_data_ptr = context.render_data->mesh_container.material_datas.data();
	auto buffer_data_ptr = context.render_data->mesh_container.buffer_datas.data();

	//	モデル類描画
	graphics_pipeline_state_ptr current_pipeline_state;
	for (auto& draw_index : mesh_indices)
	{
		auto draw_data = draw_data_ptr + draw_index;
		auto material_data = material_data_ptr + draw_data->material_index;

		if (auto pipeline_state = material_data->material->get_pipeline_state(surface_path))
		{
			if (pipeline_state != current_pipeline_state)
			{
				if (current_pipeline_state)
					current_pipeline_state->end(context);

				current_pipeline_state = pipeline_state;
				current_pipeline_state->begin(context);
			}

			auto buffer_data = buffer_data_ptr + draw_data->buffer_index;

			material_data->material->update_parameters(context);
			current_pipeline_state->update_parameters(context, *material_data->material, buffer_data);
			draw_indexed(context, mesh_lengths[draw_index], *buffer_data, current_pipeline_state);
		}
	}
	if (current_pipeline_state)
		current_pipeline_state->end(context);
}

void scene_renderer_commons::model_render(graphics_context& context,
					const std::vector<std::size_t>& mesh_indices,
					const collision::frustum_culling& frustum_culling,
					material::surface_paths surface_path)
{
	auto draw_data_ptr = context.render_data->mesh_container.draw_datas.data();
	auto material_data_ptr = context.render_data->mesh_container.material_datas.data();
	auto buffer_data_ptr = context.render_data->mesh_container.buffer_datas.data();

	//	モデル類描画
	graphics_pipeline_state_ptr current_pipeline_state;
	for (auto& draw_index : mesh_indices)
	{
		auto draw_data = draw_data_ptr + draw_index;
		auto material_data = material_data_ptr + draw_data->material_index;

		//	AABB判定
		auto buffer_data = buffer_data_ptr + draw_data->buffer_index;
		if (!frustum_culling.to_aabb(buffer_data->bounds_min, buffer_data->bounds_max))
			continue;

		if (auto pipeline_state = material_data->material->get_pipeline_state(surface_path))
		{
			if (pipeline_state != current_pipeline_state)
			{
				if (current_pipeline_state)
					current_pipeline_state->end(context);

				current_pipeline_state = pipeline_state;
				current_pipeline_state->begin(context);
			}

			material_data->material->update_parameters(context);
			current_pipeline_state->update_parameters(context, *material_data->material, buffer_data);
			float lod_center_x = (buffer_data->bounds_min.x + buffer_data->bounds_max.x) * 0.5f;
			float lod_center_y = (buffer_data->bounds_min.y + buffer_data->bounds_max.y) * 0.5f;
			float lod_center_z = (buffer_data->bounds_min.z + buffer_data->bounds_max.z) * 0.5f;
			float lod_diff_x = lod_center_x - context.scene_constant.camera_position.x;
			float lod_diff_y = lod_center_y - context.scene_constant.camera_position.y;
			float lod_diff_z = lod_center_z - context.scene_constant.camera_position.z;
			float mesh_length = sqrtf(lod_diff_x * lod_diff_x + lod_diff_y * lod_diff_y + lod_diff_z * lod_diff_z);
			draw_indexed(context, mesh_length, *buffer_data, current_pipeline_state);
		}
	}
	if (current_pipeline_state)
		current_pipeline_state->end(context);
}

void scene_renderer_commons::particle_render(graphics_context& context,
					const std::vector<std::size_t>& draw_indices,
					material::surface_paths surface_path)
{
	//	スプライト/軌跡パーティクル
	auto draw_data_ptr = context.render_data->effect_container.sprite_trail_draw_datas.data();

	//	パーティクル類描画
	graphics_pipeline_state_ptr current_pipeline_state;
	for (auto& draw_index : draw_indices)
	{
		auto draw_data = draw_data_ptr + draw_index;

		if (auto pipeline_state = draw_data->material_data.material->get_pipeline_state(surface_path))
		{
			if (pipeline_state != current_pipeline_state)
			{
				if (current_pipeline_state)
					current_pipeline_state->end(context);

				current_pipeline_state = pipeline_state;
				current_pipeline_state->begin(context);
			}

			draw_data->material_data.material->update_parameters(context);
			current_pipeline_state->update_parameters(context, *draw_data->material_data.material, nullptr);

			auto vertex_buffer = draw_data->vertex_buffer_view.vertex_buffer;
			auto strides = draw_data->vertex_buffer_view.strides_for_vertex.data();
			auto offsets = draw_data->vertex_buffer_view.offsets_for_vertex.data();
			auto indirect_buffer = draw_data->vertex_buffer_view.indirect_buffer;
			auto offset_for_args = draw_data->vertex_buffer_view.offset_for_args;

			context.device_context->IASetVertexBuffers(0, 1, vertex_buffer.GetAddressOf(), strides, offsets);
			context.device_context->IASetPrimitiveTopology(draw_data->primitive_topology);
			context.device_context->DrawInstancedIndirect(indirect_buffer.Get(), offset_for_args);
		}
	}
	if (current_pipeline_state)
		current_pipeline_state->end(context);
}
