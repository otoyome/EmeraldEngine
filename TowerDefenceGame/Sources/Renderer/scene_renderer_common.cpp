#include "Renderer/scene_renderer_common.h"
#include "model_buffer_pool.h"

namespace
{
	//	メッシュレット用の属性SRV設定
	void bind_meshlet_attribute_srvs(const graphics_context& context)
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

	//	ローカル空間のメッシュレット境界球をワールド変換し、視錐台と交差するか判定する
	bool is_meshlet_visible(const collision::frustum_culling& frustum_culling,
							 const DirectX::XMFLOAT4X4& world_transform,
							 const DirectX::XMFLOAT3& local_center,
							 float local_radius)
	{
		DirectX::XMVECTOR local_center_vector = DirectX::XMLoadFloat3(&local_center);
		DirectX::XMMATRIX world_matrix = DirectX::XMLoadFloat4x4(&world_transform);
		DirectX::XMFLOAT3 world_center;
		DirectX::XMStoreFloat3(&world_center, DirectX::XMVector3TransformCoord(local_center_vector, world_matrix));

		float scale_x = sqrtf(world_transform._11 * world_transform._11 + world_transform._12 * world_transform._12 + world_transform._13 * world_transform._13);
		float scale_y = sqrtf(world_transform._21 * world_transform._21 + world_transform._22 * world_transform._22 + world_transform._23 * world_transform._23);
		float scale_z = sqrtf(world_transform._31 * world_transform._31 + world_transform._32 * world_transform._32 + world_transform._33 * world_transform._33);
		float scale = scale_x;
		if (scale_y > scale) scale = scale_y;
		if (scale_z > scale) scale = scale_z;

		return frustum_culling.to_sphere(world_center, local_radius * scale);
	}
}

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
					UINT primitive_index,
					graphics_pipeline_state_ptr pipeline_state,
					const collision::frustum_culling* meshlet_frustum_culling)
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
		std::size_t lod_mesh_index = buffer_data.select_lod_mesh_index(mesh_length, context.scene_constant.camera_clip_distance);
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

	//	頂点バッファ登録
	bind_meshlet_attribute_srvs(context);

	//	メッシュレット単位で描画するので通常の頂点バッファ入力は使用しない
	context.set_vertex_buffers(0, 0, nullptr, nullptr, nullptr);
	context.set_index_buffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
	context.set_primitive_topology(draw_mesh->primitive_topology);

	auto pool = model_buffer_pool_manager::instance();
	auto single_instance_buffer = pool->get_single_meshlet_instance_buffer();
	auto single_instance_srv = pool->get_single_meshlet_instance_srv();
	context.device_context->VSSetShaderResources(118, 1, &single_instance_srv);

	bool apply_meshlet_frustum_culling = meshlet_frustum_culling != nullptr &&
		system_utility::is_bitflag(buffer_data.system_flags, buffer_datas::flags::culling);

	for (auto& meshlet : *draw_mesh->meshlets)
	{
		//	@tool カリング無効(no_cull)なオブジェクトはメッシュレット単位のフラスタムカリングも行わない
		if (apply_meshlet_frustum_culling && !is_meshlet_visible(*meshlet_frustum_culling, buffer_data.world_transform, meshlet.bounds_center, meshlet.bounds_radius))
			continue;

		common_pipeline_states::meshlet_instance_record record{};
		record.vertex_offset = meshlet.vertex_offset;
		record.triangle_offset = meshlet.triangle_offset;
		record.triangle_count = meshlet.triangle_count;
		record.primitive_index = primitive_index;
		context.device_context->UpdateSubresource(single_instance_buffer, 0, nullptr, &record, 0, 0);

		context.device_context->DrawInstanced(meshlet.triangle_count * 3, 1, 0, 0);
	}
}

void scene_renderer_commons::draw_geometric_primitive(const graphics_context& context, const geometric_primitive& primitive)
{
	static const semantic position_semantic("POSITION", 0);
	static const semantic texcoord_semantic("TEXCOORD", 0);

	auto& vertex_buffer_views = primitive.get_vertex_buffer_views();
	auto& index_buffer_view = primitive.get_index_buffer_view();

	auto position_it = vertex_buffer_views.find(position_semantic);
	auto texcoord_it = vertex_buffer_views.find(texcoord_semantic);

	ID3D11Buffer* buffers[2] =
	{
		position_it != vertex_buffer_views.end() ? position_it->second.buffer.Get() : nullptr,
		texcoord_it != vertex_buffer_views.end() ? texcoord_it->second.buffer.Get() : nullptr,
	};
	UINT strides[2] =
	{
		position_it != vertex_buffer_views.end() ? static_cast<UINT>(position_it->second.stride_in_bytes) : 0,
		texcoord_it != vertex_buffer_views.end() ? static_cast<UINT>(texcoord_it->second.stride_in_bytes) : 0,
	};
	UINT offsets[2] = { 0, 0 };

	context.set_vertex_buffers(0, 2, buffers, strides, offsets);
	context.set_index_buffer(index_buffer_view.buffer.Get(), index_buffer_view.format, 0);
	context.set_primitive_topology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	context.device_context->DrawIndexed(static_cast<UINT>(index_buffer_view.count()), 0, 0);
}

void scene_renderer_commons::model_render(graphics_context& context,
					const std::vector<std::size_t>& mesh_indices,
					const std::vector<float>& mesh_lengths,
					material::surface_paths surface_path,
					const collision::frustum_culling* meshlet_frustum_culling)
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
			draw_indexed(context, mesh_lengths[draw_index], *buffer_data, draw_data->buffer_index, current_pipeline_state, meshlet_frustum_culling);
		}
	}
	if (current_pipeline_state)
		current_pipeline_state->end(context);
}

void scene_renderer_commons::model_render_indirect(graphics_context& context,
					const std::vector<std::size_t>& mesh_indices,
					const std::vector<float>& mesh_lengths,
					material::surface_paths surface_path,
					meshlet_culling_system& meshlet_culling)
{
	auto draw_data_ptr = context.render_data->mesh_container.draw_datas.data();
	auto material_data_ptr = context.render_data->mesh_container.material_datas.data();

	std::unordered_set<material*> drawn;
	std::vector<std::size_t> fallback_indices;
	for (auto& draw_index : mesh_indices)
	{
		auto draw_data = draw_data_ptr + draw_index;
		auto material_data = material_data_ptr + draw_data->material_index;
		material* mat = material_data->material;

		auto pipeline_state = mat->get_pipeline_state(surface_path);
		auto result = pipeline_state ? meshlet_culling.find_result(mat) : nullptr;

		if (!pipeline_state || !result)
		{
			fallback_indices.emplace_back(draw_index);
			continue;
		}

		if (!drawn.insert(mat).second)
			continue;	//	このマテリアルは既に描画済み

		pipeline_state->begin(context);
		mat->update_parameters(context);
		pipeline_state->update_parameters(context, *mat, nullptr);

		bind_meshlet_attribute_srvs(context);
		context.device_context->VSSetShaderResources(118, 1, &result->visible_meshlet_srv);

		context.set_vertex_buffers(0, 0, nullptr, nullptr, nullptr);
		context.set_index_buffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
		context.set_primitive_topology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		context.device_context->DrawInstancedIndirect(result->indirect_args_buffer, 0);

		pipeline_state->end(context);
	}

	if (!fallback_indices.empty())
	{
		model_render(context, fallback_indices, mesh_lengths, surface_path);
	}
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
			draw_indexed(context, mesh_length, *buffer_data, draw_data->buffer_index, current_pipeline_state, &frustum_culling);
		}
	}
	if (current_pipeline_state)
		current_pipeline_state->end(context);
}

void scene_renderer_commons::particle_render(graphics_context& context,
					const std::vector<std::size_t>& draw_indices,
					material::surface_paths surface_path)
{
	//	スプライト/トレイルパーティクル
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
