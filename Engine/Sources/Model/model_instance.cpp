#include "Model/model_instance.h"
#include "Texture/texture.h"
#include "Model/model_buffer_pool.h"

#if defined(USE_IMGUI)
#include "Graphics/debug_primitive_collector.h"
#endif	//	defined(USE_IMGUI)

model_instance::model_instance()
: system_flags(0)
, lod_index(-1)
, resource()
, materials()
, nodes()
{
	unique_id = ++id_counter;
	assert(unique_id != invalid_id);

	set_flag(flags::active, true);
	set_flag(flags::color_render, true);
	set_flag(flags::shadow_caster, true);
	set_flag(flags::culling, true);
}

model_instance::model_instance(std::shared_ptr<model_resource> resource)
: model_instance()
{
	this->resource = resource;
	build_model();
}

model_instance::~model_instance()
{
}

bool model_instance::compute_animation_nodes(std::vector<node_animation>& nodes, size_t animation_index, float time) const
{
	if (!resource)
		return false;

	return resource->compute_animation_nodes(nodes, animation_index, time);
}

bool model_instance::compute_animation_node(node_animation& node, size_t animation_index, size_t node_index, float time) const
{
	if (!resource)
		return false;

	return resource->compute_animation_node(node, animation_index, node_index, time);
}

void model_instance::set_node_poses(const std::vector<node_animation>& node_animations)
{
	auto instance_node_ptr = nodes.data();
	auto animation_node_ptr = node_animations.data();
	for (size_t index = 0; index < node_animations.size(); ++index)
	{
		auto& in = *(instance_node_ptr + index);
		auto& an = *(animation_node_ptr + index);
		in.local_transform = math::matrixf::affine_transformation(an.scale, math::quaternionf(an.rotation), an.translation).to_float4x4();
	}
}

void model_instance::set_node_pose(size_t node_index, const node_animation& node)
{
	if (node_index >= nodes.size())
	{
		assert(node_index < nodes.size());
		return;
	}

	auto& in = nodes[node_index];
	in.local_transform = math::matrixf::affine_transformation(node.scale, math::quaternionf(node.rotation), node.translation).to_float4x4();
}

bool model_instance::set_morph(std::string_view morph_name, const morph_animation& morph)
{
	for (auto& entry : morphes)
	{
		if (entry.morph && entry.morph->name == morph_name)
		{
			entry.weight = morph.weight;
			return true;
		}
	}
	return false;
}

void model_instance::update_transform(const math::transformd& world)
{
	this->world = world;
	update_node_transforms();
}

void model_instance::update_node_transforms()
{
	for (auto& node : nodes)
	{
		math::matrixf global_transform(node.local_transform);
		if (node.parent_index != invalid_id)
		{
			auto& parent_node = nodes[node.parent_index];
			global_transform *= math::matrixf(parent_node.global_transform);
		}
		node.global_transform = global_transform.to_float4x4();
	}
}

void model_instance::build_model()
{
	if(!resource)
		return;

	//	マテリアルインスタンス構築
	materials.reserve(resource->get_materials().size());
	for (auto& resource_material : resource->get_materials())
	{
		materials.emplace_back(resource_material);
	}

	//	ノード構築
	nodes.reserve(resource->get_nodes().size());
	for (auto& resource_node : resource->get_nodes())
	{
		node instance_node;
		instance_node.name = resource_node.name;
		instance_node.parent_index = resource_node.parent_index;
		const math::matrixf local_transform = math::matrixf::affine_transformation(resource_node.scale, math::quaternionf(resource_node.rotation), resource_node.translation);
		instance_node.local_transform = local_transform.to_float4x4();
		if (instance_node.parent_index != model_resource::invalid_id)
		{
			instance_node.global_transform = (local_transform * math::matrixf(nodes[instance_node.parent_index].global_transform)).to_float4x4();
		}
		else
		{
			instance_node.global_transform = instance_node.local_transform;
		}
		instance_node.previous_global_transform = instance_node.global_transform;
		nodes.emplace_back(instance_node);
	}

	//	メッシュ構築
	meshes.reserve(resource->get_meshes().size());
	for (UINT index = 0; index < static_cast<UINT>(resource->get_meshes().size()); ++index)
	{
		auto& mesh = meshes.emplace_back();
		mesh.mesh = &resource->get_meshes()[index];
		mesh.visible = true;
	}

	//	モーフ構築
	morphes.reserve(resource->get_morphes().size());
	for (UINT index = 0; index < static_cast<UINT>(resource->get_morphes().size()); ++index)
	{
		auto& morph = morphes.emplace_back();
		morph.morph = &resource->get_morphes()[index];
		morph.weight = 0;
	}
}

size_t model_instance::get_node_index_by_name(std::string_view node_name) const
{
	for (size_t index = 0; index < nodes.size(); ++index)
	{
		auto& node = nodes[index];
		if (node.name == node_name)
			return index;
	}

	return model_resource::invalid_id;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////

void mesh_collector::collect(mesh_container& container, const std::vector<model_instance_ptr>& instances, const math::vector3d& render_origin)
{
	const semantic position_semantic("POSITION", 0);
	const semantic normal_semantic("NORMAL", 0);

	container.clear();

	std::vector<math::matrixf> NodeWorldTransforms, PreviousNodeWorldTransforms;
	NodeWorldTransforms.resize(common_pipeline_states::mesh_bone_max);
	PreviousNodeWorldTransforms.resize(common_pipeline_states::mesh_bone_max);

	//	バッファ/マテリアルの配列生成
	container.draw_items.clear();
	container.draw_items.reserve(256);
	container.material_datas.clear();
	container.material_datas.reserve(256);

	//	メッシュコンテナ生成
	for (const auto& instance : instances)
	{
		if (!instance)
			continue;

		if (!instance->is_flag(model_instance::flags::active))
			continue;

		if (!instance->is_flag(model_instance::flags::color_render) &&
			!instance->is_flag(model_instance::flags::shadow_caster))
		{
			for (auto& node : instance->get_nodes())
				node.previous_global_transform = node.global_transform;
			instance->set_previous_world(instance->get_world());
			continue;
		}

		//	ワールド行列等生成
		//	描画原点からの相対行列にする
		//	前フレームの姿勢も、現在の描画原点から見た相対行列にする
		const math::matrixf WorldTransform = instance->get_world().to_relative_matrix(render_origin);
		const math::matrixf PreviousWorldTransform = instance->get_previous_world().to_relative_matrix(render_origin);

		//	ノードサイズ調整
		if (NodeWorldTransforms.size() <= instance->get_nodes().size())
		{
			NodeWorldTransforms.resize(instance->get_nodes().size());
			PreviousNodeWorldTransforms.resize(instance->get_nodes().size());
		}

		//	描画情報構築
		UINT submesh_index = 0;
		for (auto& instance_mesh : instance->get_meshes())
		{
			UINT current_submesh_index = submesh_index++;
			if (!instance_mesh.mesh || !instance_mesh.visible)
				continue;
			auto& mesh = *instance_mesh.mesh;
			if (mesh.material_index == model_resource::invalid_id)
				continue;

			//	必要な行列生成
			{
				bool inside = false;
				for (auto& bone : mesh.bones)
				{
					auto& node = instance->get_nodes()[bone.node_index];
					const math::matrixf gm(node.global_transform);
					const math::matrixf pgm(node.previous_global_transform);
					NodeWorldTransforms[bone.node_index] = gm * WorldTransform;
					PreviousNodeWorldTransforms[bone.node_index] = pgm * PreviousWorldTransform;
					if (mesh.node_index == bone.node_index)
						inside = true;
				}
				if (!inside)
				{
					auto& node = instance->get_nodes()[mesh.node_index];
					const math::matrixf gm(node.global_transform);
					const math::matrixf pgm(node.previous_global_transform);
					NodeWorldTransforms[mesh.node_index] = gm * WorldTransform;
					PreviousNodeWorldTransforms[mesh.node_index] = pgm * PreviousWorldTransform;
				}
			}

			//	ワールド空間上のAABBを生成
			math::float3 bounds_min, bounds_max;
			{
				bounds_min = { +FLT_MAX, +FLT_MAX, +FLT_MAX };
				bounds_max = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
				math::float3 scale((mesh.bounds_max.x - mesh.bounds_min.x) * 0.5f, (mesh.bounds_max.y - mesh.bounds_min.y) * 0.5f, (mesh.bounds_max.z - mesh.bounds_min.z) * 0.5f);
				math::float3 center(mesh.bounds_min.x + scale.x, mesh.bounds_min.y + scale.y, mesh.bounds_min.z + scale.z);

				collision::bounding_box box = collision::bounding_box(math::double3(scale), {0.0, 0.0, 0.0, 1.0}, math::double3(center));

				math::double3 vertices[8];
				box.calculate_vertex(vertices, ARRAYSIZE(vertices));
				for (auto& box_vertex : vertices)
				{
					math::float3 vertex(box_vertex);
					vertex = math::vector3f::transform_coord(vertex, NodeWorldTransforms[mesh.node_index]).to_float3();
					bounds_min.x = std::min(vertex.x, bounds_min.x);
					bounds_min.y = std::min(vertex.y, bounds_min.y);
					bounds_min.z = std::min(vertex.z, bounds_min.z);
					bounds_max.x = std::max(vertex.x, bounds_max.x);
					bounds_max.y = std::max(vertex.y, bounds_max.y);
					bounds_max.z = std::max(vertex.z, bounds_max.z);
				}
			}

			//	バッファ情報構築
			auto& buffer_data = container.draw_items.emplace_back();
			UINT buffer_index = static_cast<UINT>(container.draw_items.size() - 1);
			buffer_data.lod_distance_range = instance->get_lod_distance_range();
			buffer_data.bounds_min = bounds_min;
			buffer_data.bounds_max = bounds_max;
			buffer_data.world_transform = (NodeWorldTransforms[mesh.node_index]).to_float4x4();
			buffer_data.system_flags = 0;
			system_utility::set_bitflag(buffer_data.system_flags, mesh_draw_item::flags::culling, instance->is_flag(model_instance::flags::culling));
			system_utility::set_bitflag(buffer_data.system_flags, mesh_draw_item::flags::static_mesh, mesh.bones.empty());
			{
				UINT combined = instance->get_unique_id();
				combined ^= current_submesh_index + 0x9e3779b9u + (combined << 6) + (combined >> 2);
				buffer_data.unique_id = combined;
			}
			buffer_data.color_meshs.clear();
			buffer_data.shadow_mesh.vertex_buffer_views = nullptr;
			buffer_data.shadow_mesh.morph_meshes.clear();

			//	頂点バッファ系列登録
			{
				auto& main_mesh = buffer_data.color_meshs.emplace_back();
				main_mesh.vertex_buffer_views = &mesh.vertex_buffer_views;
				main_mesh.primitive_topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
				main_mesh.meshlets = &mesh.pooled_meshlets;

				//	モーフバッファ登録
				if (!instance->get_morphes().empty() && !mesh.morph_indices.empty())
				{
					UINT morph_count = static_cast<UINT>(instance->get_morphes().size());
					for (UINT index = 0; index < morph_count; ++index)
					{
						auto& morph = instance->get_morphes()[index];
						if (morph.weight < 0.001f)
							continue;

						UINT part_count = static_cast<UINT>(morph.morph->parts.size());
						for (UINT part_index = 0; part_index < part_count; ++part_index)
						{
							auto& morph_part = morph.morph->parts[part_index];
							auto morph_it = mesh.morph_indices.find(morph_part.mesh_name);
							if (morph_it == mesh.morph_indices.end())
								continue;

							auto& morph_mesh = main_mesh.morph_meshes.emplace_back();
							morph_mesh.vertex_buffer_views = &mesh.morph_meshes[morph_it->second].vertex_buffer_views;
							morph_mesh.weight = morph.weight;
							break;
						}
					}
				}
			}

			if (instance->get_lod_index() == -1)
			{
				//	LODメッシュがあれば登録
				for (auto& lod_mesh : mesh.lod_meshs)
				{
					auto& sub_mesh = buffer_data.color_meshs.emplace_back();
					sub_mesh.vertex_buffer_views = &lod_mesh.vertex_buffer_views;
					if (lod_mesh.vertex_buffer_views.empty())
					{
						sub_mesh.vertex_buffer_views = &mesh.vertex_buffer_views;
					}
					sub_mesh.meshlets = lod_mesh.pooled_meshlets.empty() ? &mesh.pooled_meshlets : &lod_mesh.pooled_meshlets;
					sub_mesh.primitive_topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
				}
			}
			else if (!mesh.lod_meshs.empty() && instance->get_lod_index() > 0)
			{
				//	LODメッシュを強制適用
				int index = std::min(instance->get_lod_index() - 1, static_cast<int>(mesh.lod_meshs.size() - 1));
				auto& sub_mesh = buffer_data.color_meshs.back();
				if (!mesh.lod_meshs[index].vertex_buffer_views.empty())
				{
					sub_mesh.vertex_buffer_views = &mesh.lod_meshs[index].vertex_buffer_views;
				}
				sub_mesh.meshlets = mesh.lod_meshs[index].pooled_meshlets.empty() ? &mesh.pooled_meshlets : &mesh.lod_meshs[index].pooled_meshlets;
			}

			//	変換行列・プリミティブヘッダを登録
			{
				common_pipeline_states::primitive_header& header = container.primitive_header_scratch.emplace_back();

				//	頂点属性の所持判定、属性毎の先頭頂点番号記録
				for (auto& view : mesh.vertex_buffer_views)
				{
					const auto element = model_buffer_pool_manager::find_vertex_element(view.first);
					if (element == common_pipeline_states::vertex_elements::max || view.second.stride_in_bytes == 0 || view.second.size_in_bytes == 0)
						continue;

					const UINT element_index = static_cast<UINT>(element);
					header.has_element |= common_pipeline_states::vertex_element_bit(element);
					(&header.element_bases[element_index / 4].x)[element_index % 4] = static_cast<UINT>(view.second.offset());
				}

				if (!buffer_data.color_meshs.empty())
				{
					//	モーフ重み
					for (auto& morph_mesh : buffer_data.color_meshs[0].morph_meshes)
					{
						if (morph_mesh.vertex_buffer_views->find(position_semantic) != morph_mesh.vertex_buffer_views->end())
						{
							float* weight = &header.position_morph[header.primitive_parameter.y / 4].x;
							weight[header.primitive_parameter.y % 4] = morph_mesh.weight;
							header.primitive_parameter.y++;
						}
						if (morph_mesh.vertex_buffer_views->find(normal_semantic) != morph_mesh.vertex_buffer_views->end())
						{
							float* weight = &header.normal_morph[header.primitive_parameter.z / 4].x;
							weight[header.primitive_parameter.z % 4] = morph_mesh.weight;
							header.primitive_parameter.z++;
						}
					}
				}

				//	ボーン行列を mesh_container.bone_matrix_scratch へ連結
				UINT bone_count = static_cast<UINT>(mesh.bones.size());
				if (bone_count > common_pipeline_states::mesh_bone_max)
					bone_count = common_pipeline_states::mesh_bone_max;

				header.bone_offset = static_cast<UINT>(container.bone_matrix_scratch.size());
				for (UINT bone_index = 0; bone_index < bone_count; ++bone_index)
				{
					auto& bone = mesh.bones[bone_index];
					const math::matrixf NodeOffsetTransform(bone.offset_matrix);
					container.bone_matrix_scratch.push_back((NodeOffsetTransform * NodeWorldTransforms[bone.node_index]).to_float4x4());
				}
				header.previous_bone_offset = static_cast<UINT>(container.bone_matrix_scratch.size());
				for (UINT bone_index = 0; bone_index < bone_count; ++bone_index)
				{
					auto& bone = mesh.bones[bone_index];
					const math::matrixf NodeOffsetTransform(bone.offset_matrix);
					container.bone_matrix_scratch.push_back((NodeOffsetTransform * PreviousNodeWorldTransforms[bone.node_index]).to_float4x4());
				}
				header.bone_count = bone_count;
			}

			//	マテリアル情報構築
			auto& material_data = container.material_datas.emplace_back();
			UINT material_index = static_cast<UINT>(container.material_datas.size() - 1);
			material_data.material = instance->get_materials().data() + mesh.material_index;

			//	描画情報構築
			container.draw_datas.emplace_back();
			auto& draw_data = container.draw_datas.back();
			draw_data.buffer_index = buffer_index;
			draw_data.material_index = material_index;
			if (instance->is_flag(model_instance::flags::color_render))
			{
				container.color_datas.emplace_back(container.draw_datas.size() - 1);
			}

			//	影描画するなら登録
			if (instance->is_flag(model_instance::flags::shadow_caster) &&
				material_data.material->is_flag(material_resource::flags::shadow_caster))
			{
				//	影メッシュがあれば登録
				buffer_data.shadow_mesh.vertex_buffer_views = &mesh.shadow_mesh.vertex_buffer_views;
				if (mesh.shadow_mesh.vertex_buffer_views.empty())
				{
					buffer_data.shadow_mesh.vertex_buffer_views = &mesh.vertex_buffer_views;
				}
				buffer_data.shadow_mesh.meshlets = mesh.shadow_mesh.pooled_meshlets.empty() ? &mesh.pooled_meshlets : &mesh.shadow_mesh.pooled_meshlets;
				buffer_data.shadow_mesh.primitive_topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

				container.shadow_datas.emplace_back(container.draw_datas.size() - 1);

				//if (instance->get_resource()->get_capsule_shadows().empty())
				//{
				//	//	カプセルシャドウオブジェクトがなければ通常シャドウ
				//	container.shadow_datas.emplace_back(container.shadow_datas.size() - 1);
				//}
				//else if (!in_sight)	//	カプセルシャドウは、メッシュが見えている場合のみ登録
				//{
				//	for (auto& capsule : instance->get_resource()->get_capsule_shadows())
				//	{
				//		math::float4x4 node_world_transform = instance->get_nodes()[capsule.node_index].world_transform;
				//		capsule.capsule.
				//		container.capsule_shadow_datas.emplace_back();
				//	}
				//}
			}
		}

		//	過去行列を保存
		{
			for (auto& node : instance->get_nodes())
			{
				node.previous_global_transform = node.global_transform;
			}
			instance->set_previous_world(instance->get_world());
		}
	}

	//	Transform 行列をGPUへ転送
	container.build_transform_buffers();
}
