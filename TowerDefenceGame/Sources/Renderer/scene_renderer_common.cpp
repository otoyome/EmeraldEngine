#include "Renderer/scene_renderer_common.h"
#include "model_buffer_pool.h"

namespace
{
	//	メッシュレット用の属性SRV設定
	//	@tool 抽出時に写したモデル用プールの SRV を使う（描画中にプールのバッファが大きくなっても、このフレームの物を使い続ける）
	void bind_meshlet_attribute_srvs(const render_job_context& context)
	{
		const auto& srvs = context.render_data->mesh_container.meshlet_attribute_srvs;
		ID3D11ShaderResourceView* attribute_srvs[16] = {};
		for (UINT index = 0; index < 16; ++index)
			attribute_srvs[index] = srvs[index].Get();
		context.recorder.set_shader_resource_views(shader_stage::vertex, 102, 16, attribute_srvs);
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

void scene_renderer_commons::draw(const render_job_context& context,
			graphics_pipeline_state_ptr pipeline_state,
			UINT vertex_count,
			D3D_PRIMITIVE_TOPOLOGY primitive_topology,
			UINT vertex_offset)
{
	context.recorder.set_index_buffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
	context.recorder.set_primitive_topology(primitive_topology);
	context.recorder.draw(vertex_count, vertex_offset);
}

void scene_renderer_commons::draw_indexed(const render_job_context& context,
					float mesh_length,
					const mesh_draw_item& draw_item,
					UINT primitive_index,
					graphics_pipeline_state_ptr pipeline_state,
					const collision::frustum_culling* meshlet_frustum_culling)
{
	const mesh_draw_item::sub_mesh* draw_mesh = nullptr;
	ID3D11ShaderResourceView* morph_positions[common_pipeline_states::morph_max] = {};
	ID3D11ShaderResourceView* morph_normals[common_pipeline_states::morph_max] = {};
	if (context.recorder.get_render_state_overrides().is_shadow_rendering)
	{
		//	シャドウメッシュ対応
		draw_mesh = &draw_item.shadow_mesh;
	}
	else
	{
		//	LOD対応
		std::size_t lod_mesh_index = draw_item.select_lod_mesh_index(mesh_length);
		draw_mesh = &draw_item.color_meshs[lod_mesh_index];

		//	モーフィング対応
		//	@tool モーフは先頭のメッシュ（LOD 0）にだけ付く
		if (lod_mesh_index == 0 && !draw_item.morph_meshes.empty())
		{
			static const semantic position_semantic("POSITION", 0);
			static const semantic normal_semantic("NORMAL", 0);
			int position_count = 0, normal_count = 0;
			for (auto& morph_mesh : draw_item.morph_meshes)
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
	context.recorder.set_shader_resource_views(shader_stage::vertex, common_pipeline_states::morph_srv_position_index,
												 common_pipeline_states::morph_max, morph_positions);
	context.recorder.set_shader_resource_views(shader_stage::vertex, common_pipeline_states::morph_srv_normal_index,
												 common_pipeline_states::morph_max, morph_normals);

	//	頂点バッファ登録
	bind_meshlet_attribute_srvs(context);

	//	メッシュレット単位で描画するので通常の頂点バッファ入力は使用しない
	context.recorder.set_vertex_buffers(0, 0, nullptr, nullptr, nullptr);
	context.recorder.set_index_buffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
	context.recorder.set_primitive_topology(draw_mesh->primitive_topology);

	auto pool = model_buffer_pool_manager::instance();

	bool apply_meshlet_frustum_culling = meshlet_frustum_culling != nullptr &&
		system_utility::is_bitflag(draw_item.system_flags, mesh_draw_item::flags::culling);

	std::vector<common_pipeline_states::meshlet_instance_record> instance_records;
	instance_records.reserve(draw_mesh->meshlets->size());
	for (auto& meshlet : *draw_mesh->meshlets)
	{
		if (apply_meshlet_frustum_culling && !is_meshlet_visible(*meshlet_frustum_culling, draw_item.world_transform, meshlet.bounds_center, meshlet.bounds_radius))
			continue;

		auto& record = instance_records.emplace_back();
		record.vertex_offset = meshlet.vertex_offset;
		record.triangle_offset = meshlet.triangle_offset;
		record.triangle_count = meshlet.triangle_count;
		record.primitive_index = primitive_index;
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
	context.recorder.update_subresource(batch_buffer, 0, &box, instance_records.data(), 0, 0);

	context.recorder.set_shader_resource_views(shader_stage::vertex, 118, 1, &batch_srv);
	context.recorder.draw_instanced(meshlet_culling_system::max_meshlet_vertices_per_draw, static_cast<UINT>(instance_records.size()), 0, 0);
}

void scene_renderer_commons::draw_geometric_primitive(const render_job_context& context, const geometric_primitive& primitive)
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

	context.recorder.set_vertex_buffers(0, 2, buffers, strides, offsets);
	context.recorder.set_index_buffer(index_buffer_view.buffer.Get(), index_buffer_view.format, 0);
	context.recorder.set_primitive_topology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	context.recorder.draw_indexed(static_cast<UINT>(index_buffer_view.count()), 0, 0);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
scene_renderer_commons::mesh_draw_command_list_ptr scene_renderer_commons::build_model_draw_commands(const view_render_data& render_data,
					const std::vector<std::size_t>& mesh_indices,
					const std::vector<float>& mesh_lengths,
					material::surface_paths surface_path)
{
	auto commands = std::make_shared<mesh_draw_command_list>();
	commands->reserve(mesh_indices.size());

	const auto& mesh_container = render_data.mesh_container;
	for (auto& draw_index : mesh_indices)
	{
		const material_render_proxy* material = mesh_container.draw_items[draw_index].material.get();

		//	@tool パイプラインの無いものは描画しない
		auto pipeline_state = material->get_pipeline_state(surface_path);
		if (!pipeline_state)
			continue;

		auto& command = commands->emplace_back();
		command.pipeline = pipeline_state.get();
		command.material = material;
		command.data_index = static_cast<UINT>(draw_index);
		command.mesh_length = mesh_lengths[draw_index];
	}
	return commands;
}

scene_renderer_commons::mesh_draw_command_list_ptr scene_renderer_commons::build_model_indirect_draw_commands(const view_render_data& render_data,
					const std::vector<std::size_t>& mesh_indices,
					const std::vector<float>& mesh_lengths,
					material::surface_paths surface_path)
{
	auto commands = std::make_shared<mesh_draw_command_list>();
	commands->reserve(mesh_indices.size());

	const auto& mesh_container = render_data.mesh_container;
	for (auto& draw_index : mesh_indices)
	{
		const auto& draw_item = mesh_container.draw_items[draw_index];
		const material_render_proxy* material = draw_item.material.get();

		//	パイプラインの無いものは描画しない
		auto pipeline_state = material->get_pipeline_state(surface_path);
		if (!pipeline_state)
			continue;

		auto& command = commands->emplace_back();
		command.pipeline = pipeline_state.get();
		command.material = material;
		command.data_index = static_cast<UINT>(draw_index);
		command.mesh_length = mesh_lengths[draw_index];

		//	モーフ持ちは別パス
		if (!draw_item.color_meshs.empty())
		{
			std::size_t lod_mesh_index = draw_item.select_lod_mesh_index(mesh_lengths[draw_index]);
			command.fallback = lod_mesh_index == 0 && !draw_item.morph_meshes.empty();
		}
	}
	return commands;
}

void scene_renderer_commons::execute_model_draw_commands(render_job_context& context,
					const mesh_draw_command_list& commands,
					const collision::frustum_culling* meshlet_frustum_culling)
{
	auto draw_item_ptr = context.render_data->mesh_container.draw_items.data();

	//	モデル類描画
	const graphics_pipeline_state* current_pipeline_state = nullptr;
	for (const auto& command : commands)
	{
		if (command.pipeline != current_pipeline_state)
		{
			if (current_pipeline_state)
				context.recorder.end_pipeline();

			current_pipeline_state = command.pipeline;
			context.recorder.set_pipeline(*current_pipeline_state);
		}

		auto draw_item = draw_item_ptr + command.data_index;

		context.recorder.bind_draw(*current_pipeline_state, *command.material, draw_item);
		draw_indexed(context, command.mesh_length, *draw_item, command.data_index, nullptr, meshlet_frustum_culling);
	}
	if (current_pipeline_state)
		context.recorder.end_pipeline();
}

void scene_renderer_commons::execute_model_indirect_draw_commands(render_job_context& context,
					const mesh_draw_command_list& commands,
					meshlet_culling_system& meshlet_culling)
{
	auto draw_item_ptr = context.render_data->mesh_container.draw_items.data();

	//	メッシュレットの描画の場合は使わない
	context.recorder.set_vertex_buffers(0, 0, nullptr, nullptr, nullptr);
	context.recorder.set_index_buffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
	context.recorder.set_primitive_topology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	std::unordered_set<const material_render_proxy*> drawn;
	mesh_draw_command_list fallback_commands;
	for (const auto& command : commands)
	{
		//	モーフ持ちは別パス
		if (command.fallback)
		{
			fallback_commands.emplace_back(command);
			continue;
		}

		//	メッシュレットカリングした結果、失敗していた場合は別パス
		auto result = meshlet_culling.find_result(command.material);
		if (!result)
		{
			fallback_commands.emplace_back(command);
			continue;
		}

		if (!drawn.insert(command.material).second)
			continue;	//	このマテリアルは既に描画済み

		context.recorder.set_pipeline(*command.pipeline);
		context.recorder.bind_draw(*command.pipeline, *command.material, draw_item_ptr + command.data_index);

		//	メッシュレットバインド
		bind_meshlet_attribute_srvs(context);
		context.recorder.set_shader_resource_views(shader_stage::vertex, 118, 1, &result->visible_meshlet_srv);

		context.recorder.draw_instanced_indirect(result->indirect_args_buffer, 0);

		context.recorder.end_pipeline();
	}

	if (!fallback_commands.empty())
	{
		execute_model_draw_commands(context, fallback_commands);
	}
}

namespace
{
	//	影の描画をマテリアル単位でまとめる
	template<typename MeshTest, typename MeshletTest>
	scene_renderer_commons::shadow_draw_batch_list_ptr build_shadow_draw_batches_core(const view_render_data& render_data,
						const std::vector<std::size_t>& mesh_indices,
						material::surface_paths surface_path,
						MeshTest is_visible_mesh,
						MeshletTest is_visible_meshlet)
	{
		const auto& mesh_container = render_data.mesh_container;

		//	マテリアル単位でメッシュレットをまとめて描画
		std::unordered_map<const material_render_proxy*, scene_renderer_commons::shadow_draw_batch> batches;
		for (auto& draw_index : mesh_indices)
		{
			const auto& draw_item = mesh_container.draw_items[draw_index];
			if (!is_visible_mesh(draw_item))
				continue;

			const material_render_proxy* mat = draw_item.material.get();
			auto pipeline_state = mat->get_pipeline_state(surface_path);
			if (!pipeline_state)
				continue;

			//	影メッシュが無ければ対象外
			const mesh_draw_item::sub_mesh& draw_mesh = draw_item.shadow_mesh;
			if (!draw_mesh.meshlets || draw_mesh.meshlets->empty())
				continue;

			//	メッシュレット単位のカリングと収集
			bool apply_meshlet_culling = system_utility::is_bitflag(draw_item.system_flags, mesh_draw_item::flags::culling);
			auto& batch = batches[mat];
			batch.pipeline = pipeline_state.get();
			batch.material = mat;
			for (auto& meshlet : *draw_mesh.meshlets)
			{
				if (apply_meshlet_culling && !is_visible_meshlet(draw_item, meshlet))
					continue;

				auto& record = batch.instance_records.emplace_back();
				record.vertex_offset = meshlet.vertex_offset;
				record.triangle_offset = meshlet.triangle_offset;
				record.triangle_count = meshlet.triangle_count;
				record.primitive_index = static_cast<UINT>(draw_index);
			}
		}

		//	描画する順番はまとめた時の順番のまま
		auto list = std::make_shared<scene_renderer_commons::shadow_draw_batch_list>();
		list->reserve(batches.size());
		for (auto& pair : batches)
		{
			list->emplace_back(std::move(pair.second));
		}
		return list;
	}
}

scene_renderer_commons::shadow_draw_batch_list_ptr scene_renderer_commons::build_shadow_draw_batches(const view_render_data& render_data,
					const std::vector<std::size_t>& mesh_indices,
					const collision::frustum_culling& frustum_culling,
					material::surface_paths surface_path)
{
	return build_shadow_draw_batches_core(render_data, mesh_indices, surface_path,
		//	AABB判定
		[&](const mesh_draw_item& draw_item) { return frustum_culling.to_aabb(draw_item.bounds_min, draw_item.bounds_max); },
		//	メッシュレット単位のフラスタムカリング
		[&](const mesh_draw_item& draw_item, const pooled_meshlet& meshlet) { return is_meshlet_visible(frustum_culling, draw_item.world_transform, meshlet.bounds_center, meshlet.bounds_radius); });
}

scene_renderer_commons::shadow_draw_batch_list_ptr scene_renderer_commons::build_shadow_draw_batches(const view_render_data& render_data,
					const std::vector<std::size_t>& mesh_indices,
					const math::float3& sphere_center,
					float sphere_radius,
					material::surface_paths surface_path)
{
	return build_shadow_draw_batches_core(render_data, mesh_indices, surface_path,
		//	球との交差判定
		[&](const mesh_draw_item& draw_item) { return aabb_intersects_sphere(draw_item.bounds_min, draw_item.bounds_max, sphere_center, sphere_radius); },
		//	メッシュレット単位の球カリング
		[&](const mesh_draw_item& draw_item, const pooled_meshlet& meshlet) { return is_meshlet_visible_sphere(sphere_center, sphere_radius, draw_item.world_transform, meshlet.bounds_center, meshlet.bounds_radius); });
}

void scene_renderer_commons::execute_shadow_draw_batches(render_job_context& context, const shadow_draw_batch_list& batches)
{
	//	マテリアル単位でまとめて描画
	if (batches.empty())
		return;

	auto pool = model_buffer_pool_manager::instance();

	context.recorder.set_vertex_buffers(0, 0, nullptr, nullptr, nullptr);
	context.recorder.set_index_buffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
	context.recorder.set_primitive_topology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	for (const auto& batch : batches)
	{
		//	メッシュレット無し
		if (batch.instance_records.empty())
			continue;

		context.recorder.set_pipeline(*batch.pipeline);
		context.recorder.bind_draw(*batch.pipeline, *batch.material, nullptr);

		bind_meshlet_attribute_srvs(context);

		ID3D11Buffer* batch_buffer = nullptr;
		ID3D11ShaderResourceView* batch_srv = nullptr;
		pool->get_meshlet_instance_batch(static_cast<UINT>(batch.instance_records.size()), &batch_buffer, &batch_srv);

		D3D11_BOX box{};
		box.right = static_cast<UINT>(sizeof(common_pipeline_states::meshlet_instance_record) * batch.instance_records.size());
		box.bottom = 1;
		box.back = 1;
		context.recorder.update_subresource(batch_buffer, 0, &box, batch.instance_records.data(), 0, 0);

		context.recorder.set_shader_resource_views(shader_stage::vertex, 118, 1, &batch_srv);
		context.recorder.draw_instanced(meshlet_culling_system::max_meshlet_vertices_per_draw, static_cast<UINT>(batch.instance_records.size()), 0, 0);

		context.recorder.end_pipeline();
	}
}

scene_renderer_commons::mesh_draw_command_list_ptr scene_renderer_commons::build_particle_draw_commands(const view_render_data& render_data,
					const std::vector<std::size_t>& draw_indices,
					material::surface_paths surface_path)
{
	auto commands = std::make_shared<mesh_draw_command_list>();
	commands->reserve(draw_indices.size());

	//	スプライト/トレイルパーティクル
	const auto& draw_items = render_data.effect_container.particle_draw_items;
	for (auto& draw_index : draw_indices)
	{
		const material_render_proxy* material = draw_items[draw_index].material.get();
		auto pipeline_state = material->get_pipeline_state(surface_path);
		if (!pipeline_state)
			continue;

		auto& command = commands->emplace_back();
		command.pipeline = pipeline_state.get();
		command.material = material;
		command.data_index = static_cast<UINT>(draw_index);
	}
	return commands;
}

void scene_renderer_commons::execute_particle_draw_commands(render_job_context& context,
					const mesh_draw_command_list& commands)
{
	//	スプライト/トレイルパーティクル
	auto draw_item_ptr = context.render_data->effect_container.particle_draw_items.data();

	//	パーティクル類描画
	const graphics_pipeline_state* current_pipeline_state = nullptr;
	for (const auto& command : commands)
	{
		auto draw_item = draw_item_ptr + command.data_index;

		if (command.pipeline != current_pipeline_state)
		{
			if (current_pipeline_state)
				context.recorder.end_pipeline();

			current_pipeline_state = command.pipeline;
			context.recorder.set_pipeline(*current_pipeline_state);
		}

		context.recorder.bind_draw(*current_pipeline_state, *command.material, nullptr);

		auto vertex_buffer = draw_item->vertex_buffer_view.vertex_buffer;
		auto strides = draw_item->vertex_buffer_view.strides_for_vertex.data();
		auto offsets = draw_item->vertex_buffer_view.offsets_for_vertex.data();
		auto indirect_buffer = draw_item->vertex_buffer_view.indirect_buffer;
		auto offset_for_args = draw_item->vertex_buffer_view.offset_for_args;

		context.recorder.set_vertex_buffers(0, 1, vertex_buffer.GetAddressOf(), strides, offsets);
		context.recorder.set_primitive_topology(draw_item->primitive_topology);
		context.recorder.draw_instanced_indirect(indirect_buffer.Get(), offset_for_args);
	}
	if (current_pipeline_state)
		context.recorder.end_pipeline();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void scene_renderer_commons::scene_texture_handles::append_g_buffers(std::vector<frame_graph::texture_handle>& handles) const
{
	for (const auto& handle : g_buffers)
		handles.emplace_back(handle);
}

D3D11_VIEWPORT scene_renderer_commons::scene_textures_config::get_viewport() const
{
	D3D11_VIEWPORT viewport{};
	viewport.Width = static_cast<float>(width);
	viewport.Height = static_cast<float>(height);
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	return viewport;
}

scene_renderer_commons::scene_texture_handles scene_renderer_commons::create_scene_textures(frame_graph& graph, const scene_textures_config& config, const texture_ptr& shadow_map, const texture_ptr& output)
{
	scene_texture_handles handles;

	D3D11_TEXTURE2D_DESC desc{};
	desc.Width = config.width;
	desc.Height = config.height;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.CPUAccessFlags = 0;
	desc.MiscFlags = 0;

	//	@tool シーン色
	desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
	handles.scene_color = graph.create_texture("scene_color", desc);

	//	@tool 深度・ステンシル（型の無い形式で作り、シェーダーからは深度として読む）
	desc.Format = common_pipeline_states::scene_depth_texture_format;
	desc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
	handles.scene_depth = graph.create_texture("scene_depth", desc, common_pipeline_states::scene_depth_shader_resource_view_format);

	//	@tool G-Buffer
	static constexpr DXGI_FORMAT g_buffer_formats[] =
	{
		DXGI_FORMAT_R8G8B8A8_UNORM,			//	BaseColor
		DXGI_FORMAT_R32G32B32A32_FLOAT,		//	Normal_Depth
		DXGI_FORMAT_R8G8B8A8_UNORM,			//	Parameters
		DXGI_FORMAT_R32G32B32A32_FLOAT,		//	Velocity
		DXGI_FORMAT_R32G32B32A32_UINT,		//	UniqueData

		DXGI_FORMAT_R8G8B8A8_UNORM,			//	[エンジン改造:G-Buffer]
		DXGI_FORMAT_R8G8B8A8_UNORM,			//	[エンジン改造:G-Buffer]
	};
	static_assert(ARRAYSIZE(g_buffer_formats) == static_cast<UINT>(common_pipeline_states::gbuffer_id::max));
	desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
	for (UINT index = 0; index < static_cast<UINT>(common_pipeline_states::gbuffer_id::max); ++index)
	{
		desc.Format = g_buffer_formats[index];
		handles.g_buffers[index] = graph.create_texture(common_pipeline_states::gbuffer_names[index], desc);
	}

	//	@tool シャドウマップ（全視点で共有する）と出力先（null はバックバッファ）は取り込む
	handles.shadow_map = graph.import_texture("shadow_map", shadow_map);
	if (output)
		handles.output = graph.import_texture("output", output);
	return handles;
}

D3D11_TEXTURE2D_DESC scene_renderer_commons::make_work_texture_desc(const scene_textures_config& config, DXGI_FORMAT format, UINT divisor)
{
	//	@tool 以前のレンダーターゲット集の作業用テクスチャと同じ形式にする
	D3D11_TEXTURE2D_DESC desc{};
	desc.Width = config.width / divisor;
	desc.Height = config.height / divisor;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = format;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
	desc.CPUAccessFlags = 0;
	desc.MiscFlags = 0;
	return desc;
}

void scene_renderer_commons::capture_meshlet_attribute_srvs(mesh_container& container)
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
	for (UINT index = 0; index < 16; ++index)
		container.meshlet_attribute_srvs[index] = attribute_srvs[index];
}
