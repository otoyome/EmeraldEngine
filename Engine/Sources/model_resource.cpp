#include <cmath>

#include "model_resource.h"
#include "model_buffer_pool.h"
#include "graphics_utility.h"
#include "render_data.h"
#include "texture.h"

#include "cereal/cereal.hpp"
#include "cereal/archives/binary.hpp"
#include "cereal/archives/json.hpp"
#include "cereal/types/string.hpp"
#include "cereal/types/vector.hpp"
#include "cereal/types/unordered_map.hpp"

#include "animation_data_version.h"

#include "Resource/data_compression.h"

#include <sstream>

// CEREALバージョン定義
CEREAL_CLASS_VERSION(model_resource::node, 1)
CEREAL_CLASS_VERSION(model_resource::bone, 1)
CEREAL_CLASS_VERSION(model_resource::mesh, 4)
CEREAL_CLASS_VERSION(model_resource::mesh::meshlet, 1)
CEREAL_CLASS_VERSION(model_resource::mesh::sub_mesh, 2)
CEREAL_CLASS_VERSION(model_resource::mesh::morph_mesh, 1)
CEREAL_CLASS_VERSION(model_resource::capsule_shadow, 1)
CEREAL_CLASS_VERSION(model_resource, 1)

namespace
{
	template<class Type>
	void free_meshlet_data(Type& holder)
	{
		if (holder.meshlet_draw_infos.empty())
			return;

		model_buffer_pool_manager::instance()->free_meshlet_data(
			holder.meshlet_vertex_base, static_cast<UINT>(holder.meshlet_vertices.size()),
			holder.meshlet_triangle_base, static_cast<UINT>(holder.meshlet_triangles.size()));

		holder.meshlet_draw_infos.clear();
	}

	template<class Type>
	void build_meshlet_data(Type& holder)
	{
		free_meshlet_data(holder);

		if (holder.meshlets.empty() || holder.meshlet_vertices.empty() || holder.meshlet_triangles.empty())
			return;

		model_buffer_pool_manager::instance()->build_meshlet_data(holder.meshlet_vertices, holder.meshlet_triangles,
			holder.meshlet_vertex_base, holder.meshlet_triangle_base);

		holder.meshlet_draw_infos.reserve(holder.meshlets.size());
		for (auto& meshlet : holder.meshlets)
		{
			meshlet_draw_data draw_info{};
			draw_info.vertex_offset = holder.meshlet_vertex_base + meshlet.vertex_offset;
			draw_info.triangle_offset = holder.meshlet_triangle_base + meshlet.triangle_offset;
			draw_info.triangle_count = meshlet.triangle_count;
			draw_info.bounds_center = meshlet.bounds_center;
			draw_info.bounds_radius = meshlet.bounds_radius;
			holder.meshlet_draw_infos.emplace_back(draw_info);
		}
	}
}

template<class Archive>

void model_resource::node::serialize(Archive& archive, int version)
{
	archive(CEREAL_NVP(name),
			CEREAL_NVP(parent_index),
			CEREAL_NVP(scale),
			CEREAL_NVP(rotation),
			CEREAL_NVP(translation));
}

template<class Archive>
void model_resource::bone::serialize(Archive& archive, int version)
{
	archive(CEREAL_NVP(node_index),
			CEREAL_NVP(offset_matrix));
}

template<class Archive>
void model_resource::mesh::sub_mesh::serialize(Archive& archive, int version)
{
	switch (version)
	{
		case 1:
		{
			archive(CEREAL_NVP(vertex_buffer_views),
					CEREAL_NVP(index_buffer_view));
			break;
		}
		case 2:
		{
			archive(CEREAL_NVP(vertex_buffer_views),
					CEREAL_NVP(index_buffer_view),
					CEREAL_NVP(meshlets),
					CEREAL_NVP(meshlet_vertices),
					CEREAL_NVP(meshlet_triangles));
			break;
		}
		default:
		{
			archive(CEREAL_NVP(vertex_buffer_views),
					CEREAL_NVP(index_buffer_view));
			break;
		}
	}
}

template<class Archive>
void model_resource::mesh::morph_mesh::serialize(Archive& archive, int version)
{
	archive(CEREAL_NVP(vertex_buffer_views));
}

template<class Archive>
void model_resource::mesh::meshlet::serialize(Archive& archive, int version)
{
	archive(CEREAL_NVP(vertex_offset),
			CEREAL_NVP(triangle_offset),
			CEREAL_NVP(vertex_count),
			CEREAL_NVP(triangle_count),
			CEREAL_NVP(bounds_center),
			CEREAL_NVP(bounds_radius),
			CEREAL_NVP(cone_apex),
			CEREAL_NVP(cone_axis),
			CEREAL_NVP(cone_cutoff));
}

template<class Archive>
void model_resource::morph::morph_part_data::serialize(Archive& archive, int version)
{
	archive(CEREAL_NVP(mesh_name));
}

template<class Archive>
void model_resource::morph::serialize(Archive& archive, int version)
{
	archive(CEREAL_NVP(name), CEREAL_NVP(parts));
}

template<class Archive>
void model_resource::mesh::serialize(Archive& archive, int version)
{
	switch (version)
	{
		case 2:
		{
			archive(CEREAL_NVP(bones),
					CEREAL_NVP(material_index),
					CEREAL_NVP(node_index),
					CEREAL_NVP(vertex_buffer_views),
					CEREAL_NVP(index_buffer_view),
					CEREAL_NVP(bounds_min),
					CEREAL_NVP(bounds_max),
					CEREAL_NVP(lod_meshs));
			break;
		}
		case 3:
		{
			archive(CEREAL_NVP(bones),
					CEREAL_NVP(material_index),
					CEREAL_NVP(node_index),
					CEREAL_NVP(vertex_buffer_views),
					CEREAL_NVP(index_buffer_view),
					CEREAL_NVP(morph_meshes),
					CEREAL_NVP(morph_indices),
					CEREAL_NVP(bounds_min),
					CEREAL_NVP(bounds_max),
					CEREAL_NVP(lod_meshs));
			break;
		}
		case 4:
		{
			archive(CEREAL_NVP(bones),
					CEREAL_NVP(material_index),
					CEREAL_NVP(node_index),
					CEREAL_NVP(vertex_buffer_views),
					CEREAL_NVP(index_buffer_view),
					CEREAL_NVP(morph_meshes),
					CEREAL_NVP(morph_indices),
					CEREAL_NVP(bounds_min),
					CEREAL_NVP(bounds_max),
					CEREAL_NVP(lod_meshs),
					CEREAL_NVP(meshlets),
					CEREAL_NVP(meshlet_vertices),
					CEREAL_NVP(meshlet_triangles));
			break;
		}
		default:
		{
			archive(CEREAL_NVP(bones),
					CEREAL_NVP(material_index),
					CEREAL_NVP(node_index),
					CEREAL_NVP(vertex_buffer_views),
					CEREAL_NVP(index_buffer_view),
					CEREAL_NVP(bounds_min),
					CEREAL_NVP(bounds_max));
			break;
		}
	}
}

template<class Archive>
void model_resource::capsule_shadow::serialize(Archive& archive, int version)
{
	archive(CEREAL_NVP(node_index),
			CEREAL_NVP(rotation),
			CEREAL_NVP(center),
			CEREAL_NVP(radius),
			CEREAL_NVP(length));
}

model_resource::model_resource()
: file_path()
, materials()
, meshes()
{
}

model_resource::model_resource(std::string_view file_path)
: file_path(file_path)
, materials()
, meshes()
{
}

model_resource::~model_resource()
{
	for (auto& mesh : meshes)
	{
		model_buffer_pool_manager::instance()->free_vertex_buffers(mesh.vertex_buffer_views);
		free_meshlet_data(mesh);

		model_buffer_pool_manager::instance()->free_vertex_buffers(mesh.shadow_mesh.vertex_buffer_views);
		free_meshlet_data(mesh.shadow_mesh);

		for (auto& lod_mesh : mesh.lod_meshs)
		{
			model_buffer_pool_manager::instance()->free_vertex_buffers(lod_mesh.vertex_buffer_views);
			free_meshlet_data(lod_mesh);
		}
	}
}

bool model_resource::compute_animation_nodes(std::vector<animation::node>& nodes, size_t animation_index, float time) const
{
	nodes.clear();

	size_t node_index = 0;
	if (animation_index >= animations.size())
	{
		nodes.resize(animations[0].nodes.size());
		for (auto& node : animations[0].nodes)
		{
			nodes[node_index++] = node.nodes[0];
		}
		return false;
	}

	auto& animation = animations[animation_index];
	float calculate_time = math::clamp(time, 0.0f, animation.end_time);
	nodes.resize(animation.nodes.size());
	return animation.compute_animation(nodes, calculate_time);
}

bool model_resource::compute_animation_node(animation::node& node, size_t animation_index, size_t node_index, float time) const
{
	if (animation_index >= animations.size())
	{
		return false;
	}
	auto& animation = animations[animation_index];
	float calculate_time = math::clamp(time, 0.0f, animation.end_time);
	return animation.compute_animation(node, node_index, calculate_time);
}

size_t model_resource::get_animation_index_from_name(std::string_view name) const
{
	size_t index = 0;
	for (auto& animation : animations)
	{
		if (animation.name == name)
		{
			return index;
		}
		++index;
	}
	return invalid_id;
}

size_t model_resource::get_node_index_from_name(std::string_view name) const
{
	size_t index = 0;
	for (auto& node : nodes)
	{
		if (node.name == name)
		{
			return index;
		}
		++index;
	}
	return invalid_id;
}

size_t model_resource::get_mesh_index_from_name(std::string_view name) const
{
	size_t index = get_node_index_from_name(name);
	if (index == invalid_id)
		return invalid_id;

	size_t mesh_index = 0;
	for (auto& mesh : meshes)
	{
		if (mesh.node_index == index)
			return mesh_index;
		++mesh_index;
	}
	return invalid_id;
}

bool model_resource::serialize(std::string_view file_path)
{
	std::ostringstream buffer_stream(std::ios::binary);
	{
		cereal::BinaryOutputArchive archive(buffer_stream);

		try
		{
			archive(CEREAL_NVP(materials),
					CEREAL_NVP(nodes),
					CEREAL_NVP(meshes),
					CEREAL_NVP(morphes),
					CEREAL_NVP(animations),
					CEREAL_NVP(capsule_shadows));
		}
		catch (...)
		{
			DEBUG_LOG_CATEGORY(logger::categorys::model, "model_resource serialize failed[%s]", file_path.data());
			return false;
		}
	}

	const std::string serialized_data = buffer_stream.str();
	std::vector<char> file_data;
	if (!resource::compress(serialized_data.data(), serialized_data.size(), file_data))
	{
		DEBUG_LOG_CATEGORY(logger::categorys::model, "model_resource compress failed[%s]", file_path.data());
		return false;
	}

	std::ofstream ostream(file_path.data(), std::ios::binary);
	if(ostream.is_open())
	{
		ostream.write(file_data.data(), static_cast<std::streamsize>(file_data.size()));
		if (!ostream.good())
		{
			DEBUG_LOG_CATEGORY(logger::categorys::model, "model_resource write failed[%s]", file_path.data());
			return false;
		}

		DEBUG_LOG_CATEGORY(logger::categorys::model, "model_resource serialize successed[%s]", file_path.data());
		return true;
	}

	DEBUG_LOG_CATEGORY(logger::categorys::model, "model_resource open failed[%s]", file_path.data());
	return false;
}

bool model_resource::deserialize(std::string_view file_path)
{
	std::ifstream file_stream(file_path.data(), std::ios::binary);
	if(file_stream.is_open())
	{
		file_stream.seekg(0, std::ios::end);
		std::vector<char> file_data(static_cast<size_t>(file_stream.tellg()));
		file_stream.seekg(0, std::ios::beg);
		file_stream.read(file_data.data(), static_cast<std::streamsize>(file_data.size()));
		file_stream.close();

		std::vector<char> decompressed_data;
		std::vector<char>* source_data = &file_data;
		if (resource::is_compressed(file_data.data(), file_data.size()))
		{
			if (!resource::decompress(file_data.data(), file_data.size(), decompressed_data))
			{
				DEBUG_ERROR_CATEGORY(logger::categorys::model, "model_resource decompress failed[%s]", file_path.data());
				return false;
			}
			std::vector<char>().swap(file_data);
			source_data = &decompressed_data;
		}

		resource::memory_streambuf stream_buffer(source_data->data(), source_data->size());
		std::istream istream(&stream_buffer);
		cereal::BinaryInputArchive archive(istream);

		try
		{
			archive(CEREAL_NVP(materials),
					CEREAL_NVP(nodes),
					CEREAL_NVP(meshes),
					CEREAL_NVP(morphes),
					CEREAL_NVP(animations),
					CEREAL_NVP(capsule_shadows));
		}
		catch (...)
		{
			DEBUG_ERROR_CATEGORY(logger::categorys::model, "model_resource deserialize failed[%s]", file_path.data());
			return false;
		}

		//	モデル構築
		this->file_path = file_path;
		build_model();
		DEBUG_LOG_CATEGORY(logger::categorys::model, "model_resource deserialize successed[%s]", file_path.data());
		return true;
	}

	DEBUG_ERROR_CATEGORY(logger::categorys::model, "model_resource open failed[%s]", file_path.data());
	return false;
}

void model_resource::paste_materials(const std::vector<material>& materials)
{
	for (size_t index = 0; index < materials.size(); ++index)
	{
		materials[index].duplicate(this->materials[index]);
	}
}

void model_resource::build_model()
{
	HRESULT hr;

	//	テクスチャの枚数が多いと重いので並列読込するように
	std::vector<texture_ptr> preloaded_textures;
	{
		std::vector<std::string> texture_path_storage;
		for (auto& material : materials)
		{
			for (auto& shader_resource : material.get_shader_resources())
			{
				for (auto& resource : shader_resource.second.resources)
				{
					if (resource.is_file_path_or_key && !resource.path.empty())
						texture_path_storage.emplace_back(resource.path);
				}
			}
		}

		if (!texture_path_storage.empty())
		{
			std::vector<std::string_view> texture_path_views(texture_path_storage.begin(), texture_path_storage.end());
			texture_manager::instance()->load_textures_from_files(preloaded_textures, texture_path_views);
		}
	}

	//	@tool マテリアルを使うメッシュの頂点からスキニングとモーフの構成を決める(複数のメッシュで使う場合は合わせた構成)
	{
		std::vector<material_shader_map::vertex_format> vertex_formats(materials.size(), material_shader_map::vertex_format{ 0, false });
		std::vector<bool> used(materials.size(), false);
		for (auto& mesh : meshes)
		{
			if (mesh.material_index >= materials.size())
				continue;

			UINT joint_count = 0;
			for (UINT index = 0; index < 4; ++index)
			{
				if (mesh.vertex_buffer_views.find(semantic("JOINTS", index)) != mesh.vertex_buffer_views.end())
					joint_count = index + 1;
			}

			auto& vertex_format = vertex_formats[mesh.material_index];
			vertex_format.bone_max = std::max(vertex_format.bone_max, joint_count * 4);
			vertex_format.morph = vertex_format.morph || !mesh.morph_meshes.empty();
			used[mesh.material_index] = true;
		}
		for (size_t index = 0; index < materials.size(); ++index)
		{
			//	@tool どのメッシュにも使われていなければ既定(全部入り)のまま
			if (used[index])
				materials[index].set_vertex_format(vertex_formats[index]);
		}
	}

	//	マテリアル
	for (auto& material : materials)
	{
		material.build();
	}
	preloaded_textures.clear();
	//	頂点バッファ
	for (auto& mesh : meshes)
	{
		model_buffer_pool_manager::instance()->build_vertex_buffers(mesh.vertex_buffer_views);
		model_buffer_pool_manager::instance()->build_vertex_buffers(mesh.shadow_mesh.vertex_buffer_views);
		for (auto& lod_mesh : mesh.lod_meshs)
		{
			model_buffer_pool_manager::instance()->build_vertex_buffers(lod_mesh.vertex_buffer_views);
		}

		//	モーフバッファビュー生成
		for (auto& morph_mesh : mesh.morph_meshes)
		{
			for (auto& view : morph_mesh.vertex_buffer_views)
			{
				D3D11_BUFFER_DESC buffer_desc{};
				buffer_desc.Usage = D3D11_USAGE_DEFAULT;
				buffer_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
				buffer_desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
				buffer_desc.ByteWidth = static_cast<UINT>(view.second.size_in_bytes);
				buffer_desc.StructureByteStride = static_cast<UINT>(view.second.stride_in_bytes);

				D3D11_SUBRESOURCE_DATA subresource_data{};
				subresource_data.pSysMem = view.second.row_data.data();
				hr = render_data::instance()->get_device()->CreateBuffer(&buffer_desc, &subresource_data, view.second.buffer.ReleaseAndGetAddressOf());
				_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
				hr = render_data::instance()->get_device()->CreateShaderResourceView(view.second.buffer.Get(), nullptr, view.second.srv.GetAddressOf());
				_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
			}
		}
	}

	//	メッシュレット情報構築
	rebuild_meshlet_data();
}

void model_resource::rebuild_meshlet_data()
{
	for (auto& mesh : meshes)
	{
		build_meshlet_data(mesh);
		build_meshlet_data(mesh.shadow_mesh);
		for (auto& lod_mesh : mesh.lod_meshs)
			build_meshlet_data(lod_mesh);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
model_resource_manager::model_resource_manager()
{
}

model_resource_manager::~model_resource_manager()
{
}

bool model_resource_manager::load(model_resource_ptr& resource, std::string_view file_path, bool force_load)
{
	std::string key(file_path);

	if (!force_load)
	{
		auto it = resources.find(key);
		if (it != resources.end())
		{
			if (model_resource_ptr cached = it->second.lock())
			{
				resource = cached;
				return true;
			}
			resources.erase(it);
		}
	}

	auto new_resource = std::make_shared<model_resource>();
	if (!new_resource->deserialize(file_path))
		return false;

	resource = new_resource;
	resources[key] = resource;
	return true;
}
