#include "model_instance.h"
#include "texture.h"

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
		DirectX::XMMATRIX Scale = DirectX::XMMatrixScaling(an.scale.x, an.scale.y, an.scale.z);
		DirectX::XMMATRIX Rotation = DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&an.rotation));
		DirectX::XMMATRIX Matrix = Scale * Rotation;
		Matrix.r[3] = DirectX::XMVectorSet(an.translation.x, an.translation.y, an.translation.z, 1.0f);
		DirectX::XMStoreFloat4x4(&in.local_transform, Matrix);
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
	DirectX::XMMATRIX Scale = DirectX::XMMatrixScaling(node.scale.x, node.scale.y, node.scale.z);
	DirectX::XMMATRIX Rotation = DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&node.rotation));
	DirectX::XMMATRIX Matrix = Scale * Rotation;
	Matrix.r[3] = DirectX::XMVectorSet(node.translation.x, node.translation.y, node.translation.z, 1.0f);
	DirectX::XMStoreFloat4x4(&in.local_transform, Matrix);
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

void model_instance::update_transform(const DirectX::XMFLOAT4X4& world_transform)
{
	DirectX::XMMATRIX WorldTransform = DirectX::XMLoadFloat4x4(&world_transform);
	this->world_transform = world_transform;
	for (auto& node : nodes)
	{
		DirectX::XMMATRIX GlobalTransform = DirectX::XMLoadFloat4x4(&node.local_transform);
		if (node.parent_index != invalid_id)
		{
			auto& parent_node = nodes[node.parent_index];
			GlobalTransform *= DirectX::XMLoadFloat4x4(&parent_node.global_transform);
		}
		DirectX::XMStoreFloat4x4(&node.global_transform, GlobalTransform);
	}
}

void model_instance::update_transform(const DirectX::XMMATRIX& world_transform)
{
	DirectX::XMFLOAT4X4 matrix;
	DirectX::XMStoreFloat4x4(&matrix, world_transform);
	update_transform(matrix);
}

void model_instance::build_model()
{
	if(!resource)
		return;

	//	マテリアルをコピーしておく
	materials.reserve(resource->get_materials().size());
	for (auto& resource_material : resource->get_materials())
	{
		materials.emplace_back();
		resource_material.duplicate(materials.back());
	}

	//	ノード構築
	nodes.reserve(resource->get_nodes().size());
	for (auto& resource_node : resource->get_nodes())
	{
		node instance_node;
		instance_node.name = resource_node.name;
		instance_node.parent_index = resource_node.parent_index;
		DirectX::XMMATRIX LocalTransform = DirectX::XMMatrixScalingFromVector(DirectX::XMLoadFloat3(&resource_node.scale)) *
																			  DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&resource_node.rotation)) *
																			  DirectX::XMMatrixTranslationFromVector(DirectX::XMLoadFloat3(&resource_node.translation));
		DirectX::XMStoreFloat4x4(&instance_node.local_transform, LocalTransform);
		if (instance_node.parent_index != model_resource::invalid_id)
		{
			DirectX::XMMATRIX GlobalTransform = DirectX::XMLoadFloat4x4(&nodes[instance_node.parent_index].global_transform);
			DirectX::XMStoreFloat4x4(&instance_node.global_transform, LocalTransform * GlobalTransform);
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
model_instance_manger::model_instance_manger()
{
}

model_instance_manger::~model_instance_manger()
{
}

void model_instance_manger::registered(std::weak_ptr<model_instance> instance)
{
	//	登録済みであれば登録しない
	auto it  = std::find_if(instances.begin(),
							instances.end(),
							[instance](std::weak_ptr<model_instance> value)
							{
								return system_utility::equals(value, instance);
							});
	if (it != instances.end())
		return;

	instances.emplace_back(instance);
}

void model_instance_manger::get_container(mesh_container& container)
{
	container.clear();

	std::size_t primitive_buffer_count = 0;

	std::vector<DirectX::XMMATRIX> NodeWorldTransforms, PreviousNodeWorldTransforms;
	NodeWorldTransforms.resize(common_pipeline_states::mesh_bone_max);
	PreviousNodeWorldTransforms.resize(common_pipeline_states::mesh_bone_max);

	//	バッファ/マテリアルの配列生成
	std::size_t buffer_count = 0, material_count = 0;
	container.buffer_datas.resize(256);
	container.material_datas.resize(256);

	//	メッシュコンテナ生成
	auto it = instances.begin();
	while (it != instances.end())
	{
		if (it->expired())
		{
			it = instances.erase(it);
			continue;
		}

		auto instance = it->lock();
		++it;	//	次のインスタンスに移動させておく

		if (!instance->is_flag(model_instance::flags::active))
			continue;

		if (!instance->is_flag(model_instance::flags::color_render) &&
			!instance->is_flag(model_instance::flags::shadow_caster))
		{
			for (auto& node : instance->get_nodes())
				node.previous_global_transform = node.global_transform;
			instance->set_previous_world_transform(instance->get_world_transform());
			continue;
		}

		//	ワールド行列等生成
		DirectX::XMMATRIX WorldTransform = DirectX::XMLoadFloat4x4(&instance->get_world_transform());
		DirectX::XMMATRIX PreviousWorldTransform = DirectX::XMLoadFloat4x4(&instance->get_previous_world_transform());

		//	ノードサイズ調整
		if (NodeWorldTransforms.size() <= instance->get_nodes().size())
		{
			NodeWorldTransforms.resize(instance->get_nodes().size());
			PreviousNodeWorldTransforms.resize(instance->get_nodes().size());
		}

		//	描画情報構築
		for (auto& instance_mesh : instance->get_meshes())
		{
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
					auto gm = DirectX::XMLoadFloat4x4(&node.global_transform);
					auto pgm = DirectX::XMLoadFloat4x4(&node.previous_global_transform);
					NodeWorldTransforms[bone.node_index] = gm * WorldTransform;
					PreviousNodeWorldTransforms[bone.node_index] = pgm * PreviousWorldTransform;
					if (mesh.node_index == bone.node_index)
						inside = true;
				}
				if (!inside)
				{
					auto& node = instance->get_nodes()[mesh.node_index];
					auto gm = DirectX::XMLoadFloat4x4(&node.global_transform);
					auto pgm = DirectX::XMLoadFloat4x4(&node.previous_global_transform);
					NodeWorldTransforms[mesh.node_index] = gm * WorldTransform;
					PreviousNodeWorldTransforms[mesh.node_index] = pgm * PreviousWorldTransform;
				}
			}

			//	ワールド空間上のAABBを生成
			DirectX::XMFLOAT3 bounds_min, bounds_max;
			{
				bounds_min = { +FLT_MAX, +FLT_MAX, +FLT_MAX };
				bounds_max = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
				DirectX::XMFLOAT3 scale((mesh.bounds_max.x - mesh.bounds_min.x) * 0.5f, (mesh.bounds_max.y - mesh.bounds_min.y) * 0.5f, (mesh.bounds_max.z - mesh.bounds_min.z) * 0.5f);
				DirectX::XMFLOAT3 center(mesh.bounds_min.x + scale.x, mesh.bounds_min.y + scale.y, mesh.bounds_min.z + scale.z);

				collision::bounding_box box = collision::bounding_box(scale, {0.0f, 0.0f, 0.0f, 1.0f}, center);

				DirectX::XMFLOAT3 vertices[8];
				box.calculate_vertex(vertices, ARRAYSIZE(vertices));
				for (auto& vertex : vertices)
				{
					DirectX::XMStoreFloat3(&vertex,
						DirectX::XMVector3TransformCoord(DirectX::XMLoadFloat3(&vertex),
											NodeWorldTransforms[mesh.node_index]));
					bounds_min.x = std::min(vertex.x, bounds_min.x);
					bounds_min.y = std::min(vertex.y, bounds_min.y);
					bounds_min.z = std::min(vertex.z, bounds_min.z);
					bounds_max.x = std::max(vertex.x, bounds_max.x);
					bounds_max.y = std::max(vertex.y, bounds_max.y);
					bounds_max.z = std::max(vertex.z, bounds_max.z);
				}
			}

#if defined(USE_IMGUI)
			//	カリングオブジェクトの表示
			if (draw_mesh_aabb)
			{
				DirectX::XMFLOAT3 scale((bounds_max.x - bounds_min.x) * 0.5f, (bounds_max.y - bounds_min.y) * 0.5f, (bounds_max.z - bounds_min.z) * 0.5f);
				DirectX::XMFLOAT4 rotation = {0.0f, 0.0f, 0.0f, 1.0f};
				DirectX::XMFLOAT3 center(bounds_min.x + scale.x, bounds_min.y + scale.y, bounds_min.z + scale.z);
				DirectX::XMFLOAT4 color = {1.0f, 0.0f, 0.0f, 1.0f};
				QUEUE_DEBUG_PRIMITIVE_BOX(center, rotation, scale, color, true);
			}
#endif	//	defined(USE_IMGUI)


			//	バッファ情報構築
			if (buffer_count >= container.buffer_datas.size())
			{
				container.buffer_datas.resize(container.buffer_datas.size() * 2);
			}
			auto& buffer_data = container.buffer_datas[buffer_count];
			buffer_data.lod_range_rate = instance->get_lod_range_rate();
			buffer_data.bounds_min = bounds_min;
			buffer_data.bounds_max = bounds_max;
			buffer_data.system_flags = 0;
			system_utility::set_bitflag(buffer_data.system_flags, buffer_datas::flags::culling, instance->is_flag(model_instance::flags::culling));
			system_utility::set_bitflag(buffer_data.system_flags, buffer_datas::flags::static_mesh, mesh.bones.empty());
			buffer_data.unique_id = instance->get_unique_id();

			buffer_data.color_meshs.clear();
			buffer_data.shadow_mesh.vertex_buffer_views = nullptr;
			buffer_data.shadow_mesh.index_buffer_view = nullptr;
			buffer_data.shadow_mesh.morph_meshes.clear();

			//	頂点バッファ系列登録
			{
				auto& main_mesh = buffer_data.color_meshs.emplace_back();
				main_mesh.vertex_buffer_views = &mesh.vertex_buffer_views;
				main_mesh.index_buffer_view = &mesh.index_buffer_view;
				main_mesh.primitive_topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

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
					sub_mesh.index_buffer_view = &lod_mesh.index_buffer_view;
					if (lod_mesh.index_buffer_view.count() == 0)
					{
						sub_mesh.index_buffer_view = &mesh.index_buffer_view;
					}
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
				if (mesh.lod_meshs[index].index_buffer_view.count() != 0)
				{
					sub_mesh.index_buffer_view = &mesh.lod_meshs[index].index_buffer_view;
				}
			}

			//	変換行列・プリミティブヘッダを登録
			{
				const semantic tangent_semantic("TANGENT", 0);
				const semantic position_semantic("POSITION", 0);
				const semantic normal_semantic("NORMAL", 0);

				common_pipeline_states::primitive_constants& header = container.primitive_header_scratch.emplace_back();

				//	接線の有無
				if (!buffer_data.color_meshs.empty())
				{
					header.primitive_parameter.x = buffer_data.color_meshs[0].vertex_buffer_views->find(tangent_semantic) != buffer_data.color_meshs[0].vertex_buffer_views->end();

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
					DirectX::XMMATRIX NodeOffsetTransform = DirectX::XMLoadFloat4x4(&bone.offset_matrix);
					DirectX::XMStoreFloat4x4(&container.bone_matrix_scratch.emplace_back(), NodeOffsetTransform * NodeWorldTransforms[bone.node_index]);
				}
				header.previous_bone_offset = static_cast<UINT>(container.bone_matrix_scratch.size());
				for (UINT bone_index = 0; bone_index < bone_count; ++bone_index)
				{
					auto& bone = mesh.bones[bone_index];
					DirectX::XMMATRIX NodeOffsetTransform = DirectX::XMLoadFloat4x4(&bone.offset_matrix);
					DirectX::XMStoreFloat4x4(&container.bone_matrix_scratch.emplace_back(), NodeOffsetTransform * PreviousNodeWorldTransforms[bone.node_index]);
				}
				header.bone_count = bone_count;

				//	先頭頂点番号取得
				{
					auto it = mesh.vertex_buffer_views.find(position_semantic);
					if (it != mesh.vertex_buffer_views.end())
					{
						header.base_vertex = static_cast<UINT>(it->second.offset());
					}
				}

				//	プリミティブ用インデックス定数バッファ取得
				if (primitive_buffer_count + 1 >= constant_bufferes.size())
				{
					generate_constant_bufferes();
				}
				buffer_data.primitive_buffer = constant_bufferes[primitive_buffer_count++];

				common_pipeline_states::primitive_index_constants index_constant{};
				index_constant.primitive_index = static_cast<UINT>(buffer_count);
				render_data::instance()->get_immediate_context()->UpdateSubresource(buffer_data.primitive_buffer.Get(), 0, 0, &index_constant, 0, 0);
			}

			//	マテリアル情報構築
			if (material_count >= container.material_datas.size())
			{
				container.material_datas.resize(container.material_datas.size() * 2);
			}
			auto& material_data = container.material_datas[material_count];
			material_data.material = instance->get_materials().data() + mesh.material_index;

			//	描画情報構築
			container.draw_datas.emplace_back();
			auto& draw_data = container.draw_datas.back();
			draw_data.buffer_index = static_cast<UINT>(buffer_count);
			draw_data.material_index = static_cast<UINT>(material_count);
			++buffer_count;
			++material_count;
			if (instance->is_flag(model_instance::flags::color_render))
			{
				container.color_datas.emplace_back(container.draw_datas.size() - 1);
			}

			//	影描画するなら登録
			if (instance->is_flag(model_instance::flags::shadow_caster) &&
				material_data.material->is_flag(material::flags::shadow_caster))
			{
				//	影メッシュがあれば登録
				buffer_data.shadow_mesh.vertex_buffer_views = &mesh.shadow_mesh.vertex_buffer_views;
				if (mesh.shadow_mesh.vertex_buffer_views.empty())
				{
					buffer_data.shadow_mesh.vertex_buffer_views = &mesh.vertex_buffer_views;
				}
				buffer_data.shadow_mesh.index_buffer_view = &mesh.shadow_mesh.index_buffer_view;
				if (mesh.shadow_mesh.index_buffer_view.size_in_bytes == 0)
				{
					buffer_data.shadow_mesh.index_buffer_view = &mesh.index_buffer_view;
				}
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
				//		DirectX::XMFLOAT4X4 node_world_transform = instance->get_nodes()[capsule.node_index].world_transform;
				//		capsule.capsule.
				//		container.capsule_shadow_datas.emplace_back();
				//	}
				//}
			}
		}

		//	過去行列を保存
		{
			DirectX::XMMATRIX WorldTransform = DirectX::XMLoadFloat4x4(&instance->get_world_transform());
			for (auto& node : instance->get_nodes())
			{
				node.previous_global_transform = node.global_transform;
			}
			instance->set_previous_world_transform(instance->get_world_transform());
		}
	}

	//	バッファリサイズ
	container.buffer_datas.resize(buffer_count);
	container.material_datas.resize(material_count);

	//	Transform 行列をGPUへ転送
	container.build_transform_buffers();
}


void model_instance_manger::generate_constant_bufferes()
{
	auto render_data = render_data::instance();
	auto device = render_data->get_device();

	HRESULT hr;
	D3D11_BUFFER_DESC buffer_desc{};
	buffer_desc.Usage = D3D11_USAGE_DEFAULT;
	buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	buffer_desc.CPUAccessFlags = 0;
	buffer_desc.MiscFlags = 0;
	buffer_desc.StructureByteStride = 0;
	auto& buffer = constant_bufferes.emplace_back();
	buffer_desc.ByteWidth = sizeof(common_pipeline_states::primitive_index_constants);
	hr = device->CreateBuffer(&buffer_desc, nullptr, buffer.ReleaseAndGetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
}
