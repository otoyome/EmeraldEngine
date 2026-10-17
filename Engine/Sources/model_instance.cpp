#include "model_instance.h"
#include "texture.h"
#include "model_buffer_pool.h"

#if defined(USE_IMGUI)
#include "Graphics/debug_primitive_collector.h"
#endif	//	defined(USE_IMGUI)

model_instance::model_instance()
: system_flags(0)
, lod_index(-1)
, resource()
, override_materials()
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

size_t model_instance::get_material_count() const
{
	return resource ? resource->get_materials().size() : 0;
}

const material& model_instance::get_material(size_t index) const
{
	if (index < override_materials.size() && override_materials[index])
		return *override_materials[index];

	_ASSERT_EXPR(index < get_material_count(), L"model_instance::get_material index out of range");
	return resource->get_materials()[index];
}

material& model_instance::get_override_material(size_t index)
{
	_ASSERT_EXPR(index < get_material_count(), L"model_instance::get_override_material index out of range");
	if (override_materials.size() < get_material_count())
		override_materials.resize(get_material_count());

	auto& override_material = override_materials[index];
	if (!override_material)
	{
		override_material = std::make_unique<material>();
		resource->get_materials()[index].duplicate(*override_material);
	}
	return *override_material;
}

void model_instance::set_override_material(size_t index, const material& value)
{
	_ASSERT_EXPR(index < get_material_count(), L"model_instance::set_override_material index out of range");
	if (override_materials.size() < get_material_count())
		override_materials.resize(get_material_count());

	auto& override_material = override_materials[index];
	if (!override_material)
		override_material = std::make_unique<material>();
	value.duplicate(*override_material);
}

void model_instance::reset_override_material(size_t index)
{
	if (index < override_materials.size())
		override_materials[index].reset();
}

bool model_instance::is_material_overridden(size_t index) const
{
	return index < override_materials.size() && override_materials[index] != nullptr;
}

void model_instance::build_model()
{
	if(!resource)
		return;

	//	@tool マテリアルはリソースのものを共有する（上書きする時だけ複製する）
	override_materials.clear();
	override_materials.resize(resource->get_materials().size());

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

/////////////////////////////////////////////////////////////////////////////////////////////////////////
bool model_instance::is_render_proxy_valid(const model_render_proxy& proxy) const
{
	//	@tool 構成（リソース・LOD・フラグ・表示状態・マテリアル）が作成時と同じか
	if (proxy.resource != resource || proxy.lod_index != lod_index || proxy.system_flags != system_flags)
		return false;
	if (resource && proxy.resource_mesh_revision != resource->get_mesh_revision())
		return false;

	if (proxy.mesh_visibles.size() != meshes.size())
		return false;
	for (size_t index = 0; index < meshes.size(); ++index)
	{
		if (proxy.mesh_visibles[index] != (meshes[index].mesh && meshes[index].visible))
			return false;
	}

	const size_t material_count = get_material_count();
	if (proxy.materials.size() != material_count)
		return false;
	for (size_t index = 0; index < material_count; ++index)
	{
		if (proxy.materials[index] != get_material(index).get_render_proxy())
			return false;
	}
	return true;
}

std::shared_ptr<const model_render_proxy> model_instance::get_render_proxy() const
{
	if (render_proxy && is_render_proxy_valid(*render_proxy))
		return render_proxy;

	auto proxy = std::make_shared<model_render_proxy>();
	proxy->resource = resource;
	proxy->resource_mesh_revision = resource ? resource->get_mesh_revision() : 0;
	proxy->lod_index = lod_index;
	proxy->system_flags = system_flags;

	proxy->mesh_visibles.reserve(meshes.size());
	for (auto& instance_mesh : meshes)
	{
		proxy->mesh_visibles.emplace_back(instance_mesh.mesh && instance_mesh.visible);
	}

	const size_t material_count = get_material_count();
	proxy->materials.reserve(material_count);
	for (size_t index = 0; index < material_count; ++index)
	{
		proxy->materials.emplace_back(get_material(index).get_render_proxy());
	}

	//	描画情報構築
	UINT submesh_index = 0;
	for (auto& instance_mesh : meshes)
	{
		UINT current_submesh_index = submesh_index++;
		if (!instance_mesh.mesh || !instance_mesh.visible)
			continue;
		auto& mesh = *instance_mesh.mesh;
		if (mesh.material_index == model_resource::invalid_id)
			continue;

		auto& entry = proxy->meshes.emplace_back();
		entry.mesh = &mesh;
		entry.material = proxy->materials[mesh.material_index];

		system_utility::set_bitflag(entry.system_flags, buffer_datas::flags::culling, is_flag(model_instance::flags::culling));
		system_utility::set_bitflag(entry.system_flags, buffer_datas::flags::static_mesh, mesh.bones.empty());
		{
			UINT combined = get_unique_id();
			combined ^= current_submesh_index + 0x9e3779b9u + (combined << 6) + (combined >> 2);
			entry.unique_id = combined;
		}

		//	頂点バッファ系列登録
		{
			auto& main_mesh = entry.color_meshs.emplace_back();
			main_mesh.vertex_buffer_views = &mesh.vertex_buffer_views;
			main_mesh.primitive_topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
			main_mesh.meshlets = &mesh.meshlet_draw_infos;
		}

		if (lod_index == -1)
		{
			//	LODメッシュがあれば登録
			for (auto& lod_mesh : mesh.lod_meshs)
			{
				auto& sub_mesh = entry.color_meshs.emplace_back();
				sub_mesh.vertex_buffer_views = &lod_mesh.vertex_buffer_views;
				if (lod_mesh.vertex_buffer_views.empty())
				{
					sub_mesh.vertex_buffer_views = &mesh.vertex_buffer_views;
				}
				sub_mesh.meshlets = lod_mesh.meshlet_draw_infos.empty() ? &mesh.meshlet_draw_infos : &lod_mesh.meshlet_draw_infos;
				sub_mesh.primitive_topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
			}
		}
		else if (!mesh.lod_meshs.empty() && lod_index > 0)
		{
			//	LODメッシュを強制適用
			int index = std::min(lod_index - 1, static_cast<int>(mesh.lod_meshs.size() - 1));
			auto& sub_mesh = entry.color_meshs.back();
			if (!mesh.lod_meshs[index].vertex_buffer_views.empty())
			{
				sub_mesh.vertex_buffer_views = &mesh.lod_meshs[index].vertex_buffer_views;
			}
			sub_mesh.meshlets = mesh.lod_meshs[index].meshlet_draw_infos.empty() ? &mesh.meshlet_draw_infos : &mesh.lod_meshs[index].meshlet_draw_infos;
		}

		//	頂点属性の所持判定、属性毎の先頭頂点番号記録
		for (auto& view : mesh.vertex_buffer_views)
		{
			const auto element = model_buffer_pool_manager::find_vertex_element(view.first);
			if (element == common_pipeline_states::vertex_elements::max || view.second.stride_in_bytes == 0 || view.second.size_in_bytes == 0)
				continue;

			const UINT element_index = static_cast<UINT>(element);
			entry.header.has_element |= common_pipeline_states::vertex_element_bit(element);
			(&entry.header.element_bases[element_index / 4].x)[element_index % 4] = static_cast<UINT>(view.second.offset());
		}

		//	影描画するなら登録
		entry.shadow_caster = is_flag(model_instance::flags::shadow_caster) && entry.material->is_flag(material::flags::shadow_caster);
		if (entry.shadow_caster)
		{
			//	影メッシュがあれば登録
			entry.shadow_mesh.vertex_buffer_views = &mesh.shadow_mesh.vertex_buffer_views;
			if (mesh.shadow_mesh.vertex_buffer_views.empty())
			{
				entry.shadow_mesh.vertex_buffer_views = &mesh.vertex_buffer_views;
			}
			entry.shadow_mesh.meshlets = mesh.shadow_mesh.meshlet_draw_infos.empty() ? &mesh.meshlet_draw_infos : &mesh.shadow_mesh.meshlet_draw_infos;
			entry.shadow_mesh.primitive_topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		}
	}

	render_proxy = proxy;
	return render_proxy;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
void model_instance_manger::get_container(mesh_container& container, const math::vector3d& render_origin)
{
	const semantic position_semantic("POSITION", 0);
	const semantic normal_semantic("NORMAL", 0);

	container.clear();

	std::vector<math::matrixf> NodeWorldTransforms, PreviousNodeWorldTransforms;
	NodeWorldTransforms.resize(common_pipeline_states::mesh_bone_max);
	PreviousNodeWorldTransforms.resize(common_pipeline_states::mesh_bone_max);

	//	バッファ/マテリアルの配列生成
	container.buffer_datas.clear();
	container.buffer_datas.reserve(256);
	container.material_datas.clear();
	container.keep_alive_objects.clear();
	container.material_datas.reserve(256);

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
			instance->set_previous_world(instance->get_world());
			continue;
		}

		//	@tool 構成が変わっていなければ前回のプロキシをそのまま使う（変わる物は行列・モーフ重み・ボーンだけ）
		//	@tool 描画側はインスタンスではなくプロキシ（とリソース）を参照するので、こちらを破棄させないようにする
		const std::shared_ptr<const model_render_proxy> proxy = instance->get_render_proxy();
		container.keep_alive_objects.emplace_back(proxy);

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
		for (auto& entry : proxy->meshes)
		{
			auto& mesh = *entry.mesh;

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
			//	@tool LOD・影メッシュ・フラグはプロキシのものを写す
			auto& buffer_data = container.buffer_datas.emplace_back();
			UINT buffer_index = static_cast<UINT>(container.buffer_datas.size() - 1);
			buffer_data.lod_distance_range = instance->get_lod_distance_range();
			buffer_data.bounds_min = bounds_min;
			buffer_data.bounds_max = bounds_max;
			buffer_data.world_transform = (NodeWorldTransforms[mesh.node_index]).to_float4x4();
			buffer_data.system_flags = entry.system_flags;
			buffer_data.unique_id = entry.unique_id;
			buffer_data.color_meshs = entry.color_meshs;	//	@tool プロキシのものを参照する（プロキシは keep_alive_objects で保持）
			buffer_data.shadow_mesh = entry.shadow_mesh;

			//	モーフバッファ登録
			//	@tool 重みで対象が変わるので毎回求める
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

						auto& morph_mesh = buffer_data.morph_meshes.emplace_back();
						morph_mesh.vertex_buffer_views = &mesh.morph_meshes[morph_it->second].vertex_buffer_views;
						morph_mesh.weight = morph.weight;
						break;
					}
				}
			}

			//	変換行列・プリミティブヘッダを登録
			{
				//	@tool 頂点属性の情報はプロキシで求めてある
				common_pipeline_states::primitive_header& header = container.primitive_header_scratch.emplace_back(entry.header);

				if (!buffer_data.color_meshs.empty())
				{
					//	モーフ重み
					for (auto& morph_mesh : buffer_data.morph_meshes)
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
			//	@tool 描画側にはマテリアルの複製を渡す
			material_data.material = entry.material;

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
			if (entry.shadow_caster)
			{
				container.shadow_datas.emplace_back(container.draw_datas.size() - 1);
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
	//	@tool バッファは使い回す（描画中のフレームと重ならないよう、複数を順番に使う。大きさが足りない時だけ作り直す）
	transform_buffer_set& buffer_set = transform_buffer_sets[transform_buffer_set_index];
	transform_buffer_set_index = (transform_buffer_set_index + 1) % transform_buffer_set_count;
	container.primitive_header_buffer = buffer_set.primitive_header_buffer;
	container.primitive_header_srv = buffer_set.primitive_header_srv;
	container.primitive_header_capacity = buffer_set.primitive_header_capacity;
	container.bone_matrix_buffer = buffer_set.bone_matrix_buffer;
	container.bone_matrix_srv = buffer_set.bone_matrix_srv;
	container.bone_matrix_capacity = buffer_set.bone_matrix_capacity;
	container.build_transform_buffers();
	buffer_set.primitive_header_buffer = container.primitive_header_buffer;
	buffer_set.primitive_header_srv = container.primitive_header_srv;
	buffer_set.primitive_header_capacity = container.primitive_header_capacity;
	buffer_set.bone_matrix_buffer = container.bone_matrix_buffer;
	buffer_set.bone_matrix_srv = container.bone_matrix_srv;
	buffer_set.bone_matrix_capacity = container.bone_matrix_capacity;
}
