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

	//	1つのメッシュレットを描くビューの数(影はカスケードの数だけインスタンスを増やして描く)
	UINT get_view_count(const graphics_context& context)
	{
		if (!context.is_shadow_rendering)
			return 1;
		return std::max(context.shadow_map_constant.shadow_map_count, 1u);
	}

	//	影の VS がカスケードごとにカリングできるよう、ワールド空間の境界球をレコードに入れる
	void set_record_bounds(common_pipeline_states::meshlet_instance_record& record,
						   const math::float4x4& world_transform,
						   const math::float3& local_center,
						   float local_radius,
						   bool apply_culling)
	{
		if (!apply_culling)
		{
			record.no_cull = 1;
			return;
		}

		record.bounds_center = math::vector3f::transform_coord(local_center, world_transform).to_float3();

		float scale_x = sqrtf(world_transform._11 * world_transform._11 + world_transform._12 * world_transform._12 + world_transform._13 * world_transform._13);
		float scale_y = sqrtf(world_transform._21 * world_transform._21 + world_transform._22 * world_transform._22 + world_transform._23 * world_transform._23);
		float scale_z = sqrtf(world_transform._31 * world_transform._31 + world_transform._32 * world_transform._32 + world_transform._33 * world_transform._33);
		float scale = scale_x;
		if (scale_y > scale) scale = scale_y;
		if (scale_z > scale) scale = scale_z;
		record.bounds_radius = local_radius * scale;
	}

	//	ローカル空間のメッシュレット境界球をワールド変換し、視錐台と交差するか判定する
	bool is_meshlet_visible(const collision::frustum_culling& frustum_culling,
							 const math::float4x4& world_transform,
							 const math::float3& local_center,
							 float local_radius)
	{
		const math::float3 world_center = math::vector3f::transform_coord(local_center, world_transform).to_float3();

		float scale_x = sqrtf(world_transform._11 * world_transform._11 + world_transform._12 * world_transform._12 + world_transform._13 * world_transform._13);
		float scale_y = sqrtf(world_transform._21 * world_transform._21 + world_transform._22 * world_transform._22 + world_transform._23 * world_transform._23);
		float scale_z = sqrtf(world_transform._31 * world_transform._31 + world_transform._32 * world_transform._32 + world_transform._33 * world_transform._33);
		float scale = scale_x;
		if (scale_y > scale) scale = scale_y;
		if (scale_z > scale) scale = scale_z;

		return frustum_culling.to_sphere(world_center, local_radius * scale);
	}

	//	AABBと球が交差するかどうかを判定する
	bool aabb_intersects_sphere(const math::float3& aabb_min,
								 const math::float3& aabb_max,
								 const math::float3& sphere_center,
								 float sphere_radius)
	{
		float clamped_x = sphere_center.x;
		if (clamped_x < aabb_min.x) clamped_x = aabb_min.x;
		if (clamped_x > aabb_max.x) clamped_x = aabb_max.x;
		float clamped_y = sphere_center.y;
		if (clamped_y < aabb_min.y) clamped_y = aabb_min.y;
		if (clamped_y > aabb_max.y) clamped_y = aabb_max.y;
		float clamped_z = sphere_center.z;
		if (clamped_z < aabb_min.z) clamped_z = aabb_min.z;
		if (clamped_z > aabb_max.z) clamped_z = aabb_max.z;

		float diff_x = clamped_x - sphere_center.x;
		float diff_y = clamped_y - sphere_center.y;
		float diff_z = clamped_z - sphere_center.z;
		float distance_squared = diff_x * diff_x + diff_y * diff_y + diff_z * diff_z;

		return distance_squared <= sphere_radius * sphere_radius;
	}

	//	ローカル空間のメッシュレット境界をワールド変換し、球と交差するか判定する
	bool is_meshlet_visible_sphere(const math::float3& sphere_center,
									float sphere_radius,
									const math::float4x4& world_transform,
									const math::float3& local_center,
									float local_radius)
	{
		const math::float3 world_center = math::vector3f::transform_coord(local_center, world_transform).to_float3();

		float scale_x = sqrtf(world_transform._11 * world_transform._11 + world_transform._12 * world_transform._12 + world_transform._13 * world_transform._13);
		float scale_y = sqrtf(world_transform._21 * world_transform._21 + world_transform._22 * world_transform._22 + world_transform._23 * world_transform._23);
		float scale_z = sqrtf(world_transform._31 * world_transform._31 + world_transform._32 * world_transform._32 + world_transform._33 * world_transform._33);
		float scale = scale_x;
		if (scale_y > scale) scale = scale_y;
		if (scale_z > scale) scale = scale_z;

		float world_radius = local_radius * scale;
		float diff_x = world_center.x - sphere_center.x;
		float diff_y = world_center.y - sphere_center.y;
		float diff_z = world_center.z - sphere_center.z;
		float distance_squared = diff_x * diff_x + diff_y * diff_y + diff_z * diff_z;
		float radius_sum = sphere_radius + world_radius;

		return distance_squared <= radius_sum * radius_sum;
	}

}

float scene_renderer_commons::convert_view_depth_to_projection_depth(float view_depth, float near_clip, float far_clip)
{
	const float reciprocal_far_clip = 1.0f / far_clip;
	const float near_far_ratio = near_clip * reciprocal_far_clip;
	if constexpr (math::is_reverse_depth())
	{
		return near_clip * (1.0f - view_depth * reciprocal_far_clip) / (view_depth * (1.0f - near_far_ratio));
	}
	else
	{
		return (view_depth - near_clip) / (view_depth * (1.0f - near_far_ratio));
	}
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
		std::size_t lod_mesh_index = buffer_data.select_lod_mesh_index(mesh_length);
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

	bool apply_meshlet_frustum_culling = meshlet_frustum_culling != nullptr &&
		system_utility::is_bitflag(buffer_data.system_flags, buffer_datas::flags::culling);

	std::vector<common_pipeline_states::meshlet_instance_record> instance_records;
	instance_records.reserve(draw_mesh->meshlets->size());
	for (auto& meshlet : *draw_mesh->meshlets)
	{
		if (apply_meshlet_frustum_culling && !is_meshlet_visible(*meshlet_frustum_culling, buffer_data.world_transform, meshlet.bounds_center, meshlet.bounds_radius))
			continue;

		auto& record = instance_records.emplace_back();
		record.vertex_offset = meshlet.vertex_offset;
		record.triangle_offset = meshlet.triangle_offset;
		record.triangle_count = meshlet.triangle_count;
		record.primitive_index = primitive_index;
		set_record_bounds(record, buffer_data.world_transform, meshlet.bounds_center, meshlet.bounds_radius, apply_meshlet_frustum_culling);
	}

	if (instance_records.empty())
		return;

	ID3D11Buffer* batch_buffer = nullptr;
	ID3D11ShaderResourceView* batch_srv = nullptr;
	pool->get_meshlet_instance_batch(static_cast<UINT>(instance_records.size()), &batch_buffer, &batch_srv);

	D3D11_BOX box{};
	box.right = static_cast<UINT>(sizeof(common_pipeline_states::meshlet_instance_record) * instance_records.size());
	box.bottom = 1;
	box.back = 1;
	context.device_context->UpdateSubresource(batch_buffer, 0, &box, instance_records.data(), 0, 0);

	context.device_context->VSSetShaderResources(118, 1, &batch_srv);
	context.device_context->DrawInstanced(meshlet_culling_system::max_meshlet_vertices_per_draw, static_cast<UINT>(instance_records.size()) * get_view_count(context), 0, 0);
}

void scene_renderer_commons::draw_geometric_primitive(const graphics_context& context, const geometric_primitive& primitive)
{
	static const semantic position_semantic("POSITION", 0);
	static const semantic texcoord_semantic("TEXCOORD", 0);

	auto& vertex_buffer_views = primitive.get_vertex_buffer_views();
	auto& index_buffer_view = primitive.get_index_buffer_view();

	auto position_it = vertex_buffer_views.find(position_semantic);
	auto texcoord_it = vertex_buffer_views.find(texcoord_semantic);

	ID3D11ShaderResourceView* streams[2] =
	{
		position_it != vertex_buffer_views.end() ? position_it->second.srv.Get() : nullptr,
		texcoord_it != vertex_buffer_views.end() ? texcoord_it->second.srv.Get() : nullptr,
	};
	context.set_vertex_streams(2, streams);
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
	auto buffer_data_ptr = context.render_data->mesh_container.buffer_datas.data();

	//	メッシュレットの描画の場合は使わない
	context.set_vertex_buffers(0, 0, nullptr, nullptr, nullptr);
	context.set_index_buffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
	context.set_primitive_topology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	std::unordered_set<material*> drawn;
	std::vector<std::size_t> fallback_indices;
	for (auto& draw_index : mesh_indices)
	{
		auto draw_data = draw_data_ptr + draw_index;
		auto material_data = material_data_ptr + draw_data->material_index;
		material* material = material_data->material;

		//	モーフ持ちは別パス
		auto buffer_data = buffer_data_ptr + draw_data->buffer_index;
		if (!buffer_data->color_meshs.empty())
		{
			std::size_t lod_mesh_index = buffer_data->select_lod_mesh_index(mesh_lengths[draw_index]);
			if (!buffer_data->color_meshs[lod_mesh_index].morph_meshes.empty())
			{
				fallback_indices.emplace_back(draw_index);
				continue;
			}
		}

		//	メッシュレットカリングした結果、失敗していた場合は別パス
		auto pipeline_state = material->get_pipeline_state(surface_path);
		auto result = pipeline_state ? meshlet_culling.find_result(material) : nullptr;
		if (!pipeline_state || !result)
		{
			fallback_indices.emplace_back(draw_index);
			continue;
		}

		if (!drawn.insert(material).second)
			continue;	//	このマテリアルは既に描画済み

		pipeline_state->begin(context);
		pipeline_state->update_parameters(context, *material, buffer_data);

		//	メッシュレットバインド
		bind_meshlet_attribute_srvs(context);
		context.device_context->VSSetShaderResources(118, 1, &result->visible_meshlet_srv);

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

void scene_renderer_commons::model_render_shadow_batched(graphics_context& context,
					const std::vector<std::size_t>& mesh_indices,
					const collision::frustum_culling& frustum_culling,
					material::surface_paths surface_path)
{
	auto draw_data_ptr = context.render_data->mesh_container.draw_datas.data();
	auto material_data_ptr = context.render_data->mesh_container.material_datas.data();
	auto buffer_data_ptr = context.render_data->mesh_container.buffer_datas.data();

	struct material_batch
	{
		graphics_pipeline_state_ptr pipeline_state;
		std::vector<common_pipeline_states::meshlet_instance_record> instance_records;
	};
	std::unordered_map<material*, material_batch> batches;
	std::vector<std::size_t> fallback_indices;

	//	マテリアル単位でメッシュレットをまとめて描画
	for (auto& draw_index : mesh_indices)
	{
		auto draw_data = draw_data_ptr + draw_index;
		auto material_data = material_data_ptr + draw_data->material_index;

		//	AABB判定
		auto buffer_data = buffer_data_ptr + draw_data->buffer_index;
		if (!frustum_culling.to_aabb(buffer_data->bounds_min, buffer_data->bounds_max))
			continue;

		material* mat = material_data->material;
		auto pipeline_state = mat->get_pipeline_state(surface_path);
		if (!pipeline_state)
			continue;

		//	影メッシュが無ければ対象外
		const buffer_datas::sub_mesh& draw_mesh = buffer_data->shadow_mesh;
		if (!draw_mesh.meshlets || draw_mesh.meshlets->empty())
			continue;

		//	モーフィング持ちは別パス
		if (!draw_mesh.morph_meshes.empty())
		{
			fallback_indices.emplace_back(draw_index);
			continue;
		}

		//	メッシュレット単位のフラスタムカリングと収集
		bool apply_meshlet_frustum_culling = system_utility::is_bitflag(buffer_data->system_flags, buffer_datas::flags::culling);
		auto& batch = batches[mat];
		batch.pipeline_state = pipeline_state;
		for (auto& meshlet : *draw_mesh.meshlets)
		{
			if (apply_meshlet_frustum_culling && !is_meshlet_visible(frustum_culling, buffer_data->world_transform, meshlet.bounds_center, meshlet.bounds_radius))
				continue;

			auto& record = batch.instance_records.emplace_back();
			record.vertex_offset = meshlet.vertex_offset;
			record.triangle_offset = meshlet.triangle_offset;
			record.triangle_count = meshlet.triangle_count;
			record.primitive_index = draw_data->buffer_index;
			set_record_bounds(record, buffer_data->world_transform, meshlet.bounds_center, meshlet.bounds_radius, apply_meshlet_frustum_culling);
		}
	}

	//	マテリアル単位でまとめて描画
	if (!batches.empty())
	{
		auto pool = model_buffer_pool_manager::instance();

		context.set_vertex_buffers(0, 0, nullptr, nullptr, nullptr);
		context.set_index_buffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
		context.set_primitive_topology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		for (auto& pair : batches)
		{
			material* mat = pair.first;
			auto& batch = pair.second;
			//	メッシュレット無し
			if (batch.instance_records.empty())
				continue;

			batch.pipeline_state->begin(context);
			batch.pipeline_state->update_parameters(context, *mat, nullptr);

			bind_meshlet_attribute_srvs(context);

			ID3D11Buffer* batch_buffer = nullptr;
			ID3D11ShaderResourceView* batch_srv = nullptr;
			pool->get_meshlet_instance_batch(static_cast<UINT>(batch.instance_records.size()), &batch_buffer, &batch_srv);

			D3D11_BOX box{};
			box.right = static_cast<UINT>(sizeof(common_pipeline_states::meshlet_instance_record) * batch.instance_records.size());
			box.bottom = 1;
			box.back = 1;
			context.device_context->UpdateSubresource(batch_buffer, 0, &box, batch.instance_records.data(), 0, 0);

			context.device_context->VSSetShaderResources(118, 1, &batch_srv);
			context.device_context->DrawInstanced(meshlet_culling_system::max_meshlet_vertices_per_draw, static_cast<UINT>(batch.instance_records.size()) * get_view_count(context), 0, 0);

			batch.pipeline_state->end(context);
		}
	}

	//	モーフィング等でまとめられなかった分は通常経路で描画
	if (!fallback_indices.empty())
	{
		model_render(context, fallback_indices, frustum_culling, surface_path);
	}
}

void scene_renderer_commons::model_render_shadow_batched(graphics_context& context,
					const std::vector<std::size_t>& mesh_indices,
					const math::float3& sphere_center,
					float sphere_radius,
					material::surface_paths surface_path)
{
	auto draw_data_ptr = context.render_data->mesh_container.draw_datas.data();
	auto material_data_ptr = context.render_data->mesh_container.material_datas.data();
	auto buffer_data_ptr = context.render_data->mesh_container.buffer_datas.data();

	struct material_batch
	{
		graphics_pipeline_state_ptr pipeline_state;
		std::vector<common_pipeline_states::meshlet_instance_record> instance_records;
	};
	std::unordered_map<material*, material_batch> batches;
	std::vector<std::size_t> fallback_indices;

	//	マテリアル単位でメッシュレットをまとめて描画
	for (auto& draw_index : mesh_indices)
	{
		auto draw_data = draw_data_ptr + draw_index;
		auto material_data = material_data_ptr + draw_data->material_index;

		//	球との交差判定
		auto buffer_data = buffer_data_ptr + draw_data->buffer_index;
		if (!aabb_intersects_sphere(buffer_data->bounds_min, buffer_data->bounds_max, sphere_center, sphere_radius))
			continue;

		material* mat = material_data->material;
		auto pipeline_state = mat->get_pipeline_state(surface_path);
		if (!pipeline_state)
			continue;

		//	影メッシュが無ければ対象外
		const buffer_datas::sub_mesh& draw_mesh = buffer_data->shadow_mesh;
		if (!draw_mesh.meshlets || draw_mesh.meshlets->empty())
			continue;

		//	モーフィング持ちは別パス
		if (!draw_mesh.morph_meshes.empty())
		{
			fallback_indices.emplace_back(draw_index);
			continue;
		}

		//	メッシュレット単位の球カリングと収集
		bool apply_meshlet_culling = system_utility::is_bitflag(buffer_data->system_flags, buffer_datas::flags::culling);
		auto& batch = batches[mat];
		batch.pipeline_state = pipeline_state;
		for (auto& meshlet : *draw_mesh.meshlets)
		{
			if (apply_meshlet_culling && !is_meshlet_visible_sphere(sphere_center, sphere_radius, buffer_data->world_transform, meshlet.bounds_center, meshlet.bounds_radius))
				continue;

			auto& record = batch.instance_records.emplace_back();
			record.vertex_offset = meshlet.vertex_offset;
			record.triangle_offset = meshlet.triangle_offset;
			record.triangle_count = meshlet.triangle_count;
			record.primitive_index = draw_data->buffer_index;
			set_record_bounds(record, buffer_data->world_transform, meshlet.bounds_center, meshlet.bounds_radius, apply_meshlet_culling);
		}
	}

	//	マテリアル単位でまとめて描画
	if (!batches.empty())
	{
		auto pool = model_buffer_pool_manager::instance();

		context.set_vertex_buffers(0, 0, nullptr, nullptr, nullptr);
		context.set_index_buffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
		context.set_primitive_topology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		for (auto& pair : batches)
		{
			material* mat = pair.first;
			auto& batch = pair.second;
			//	このマテリアルは生存メッシュレット無し
			if (batch.instance_records.empty())
				continue;

			batch.pipeline_state->begin(context);
			batch.pipeline_state->update_parameters(context, *mat, nullptr);

			bind_meshlet_attribute_srvs(context);

			ID3D11Buffer* batch_buffer = nullptr;
			ID3D11ShaderResourceView* batch_srv = nullptr;
			pool->get_meshlet_instance_batch(static_cast<UINT>(batch.instance_records.size()), &batch_buffer, &batch_srv);

			D3D11_BOX box{};
			box.right = static_cast<UINT>(sizeof(common_pipeline_states::meshlet_instance_record) * batch.instance_records.size());
			box.bottom = 1;
			box.back = 1;
			context.device_context->UpdateSubresource(batch_buffer, 0, &box, batch.instance_records.data(), 0, 0);

			context.device_context->VSSetShaderResources(118, 1, &batch_srv);
			context.device_context->DrawInstanced(meshlet_culling_system::max_meshlet_vertices_per_draw, static_cast<UINT>(batch.instance_records.size()) * get_view_count(context), 0, 0);

			batch.pipeline_state->end(context);
		}
	}

	//	モーフィング等でまとめられなかった分は通常経路で描画
	if (!fallback_indices.empty())
	{
		graphics_pipeline_state_ptr current_pipeline_state;
		for (auto& draw_index : fallback_indices)
		{
			auto draw_data = draw_data_ptr + draw_index;
			auto material_data = material_data_ptr + draw_data->material_index;
			auto buffer_data = buffer_data_ptr + draw_data->buffer_index;

			if (auto pipeline_state = material_data->material->get_pipeline_state(surface_path))
			{
				if (pipeline_state != current_pipeline_state)
				{
					if (current_pipeline_state)
						current_pipeline_state->end(context);

					current_pipeline_state = pipeline_state;
					current_pipeline_state->begin(context);
				}

				current_pipeline_state->update_parameters(context, *material_data->material, buffer_data);
				draw_indexed(context, 0.0f, *buffer_data, draw_data->buffer_index, current_pipeline_state);
			}
		}
		if (current_pipeline_state)
			current_pipeline_state->end(context);
	}
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

			current_pipeline_state->update_parameters(context, *draw_data->material_data.material, nullptr);

			auto& vertex_stream = draw_data->vertex_buffer_view.vertex_stream;
			auto indirect_buffer = draw_data->vertex_buffer_view.indirect_buffer;
			auto offset_for_args = draw_data->vertex_buffer_view.offset_for_args;

			context.set_vertex_streams(1, vertex_stream.GetAddressOf());
			context.device_context->IASetPrimitiveTopology(draw_data->primitive_topology);
			context.device_context->DrawInstancedIndirect(indirect_buffer.Get(), offset_for_args);
		}
	}
	if (current_pipeline_state)
		current_pipeline_state->end(context);
}
