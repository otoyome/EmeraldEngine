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
#include "Resource/resource_manager.h"
#include "Resource/asset_file_header.h"

#if defined(USE_IMGUI)
#include "data_converter.h"
#endif	//	defined(USE_IMGUI)

#include <filesystem>
#include <sstream>

// CEREALバージョン定義
CEREAL_CLASS_VERSION(model_resource::node, 1)
CEREAL_CLASS_VERSION(model_resource::bone, 1)
CEREAL_CLASS_VERSION(model_resource::mesh, 1)
CEREAL_CLASS_VERSION(model_resource::mesh::meshlet, 1)
CEREAL_CLASS_VERSION(model_resource::mesh::sub_mesh, 1)
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
	archive(CEREAL_NVP(vertex_buffer_views),
			CEREAL_NVP(index_buffer_view),
			CEREAL_NVP(meshlets),
			CEREAL_NVP(meshlet_vertices),
			CEREAL_NVP(meshlet_triangles));
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
: materials()
, meshes()
{
}

model_resource::model_resource(std::string_view file_path)
: materials()
, meshes()
{
	set_file_path(file_path);
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

bool model_resource::save_file(std::string_view file_path)
{
	std::vector<resource::asset_reference<material_resource>> material_references;
	material_references.reserve(materials.size());
	for (auto& material : materials)
		material_references.emplace_back(resource::asset_reference<material_resource>::from(material));

	std::ostringstream buffer_stream(std::ios::binary);
	{
		cereal::BinaryOutputArchive archive(buffer_stream);

		try
		{
			archive(CEREAL_NVP(material_references),
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

	if (!resource::write_asset_file(*this, resource::asset_types::model, file_path, buffer_stream.str()))
		return false;

	DEBUG_LOG_CATEGORY(logger::categorys::model, "model_resource serialize successed[%s]", file_path.data());
	return true;
}

bool model_resource::load_file(std::string_view file_path, std::vector<resource::asset_reference<material_resource>>& out_material_references)
{
	resource::asset_file_header header;
	std::vector<char> body;
	if (!resource::read_asset_file(file_path, resource::asset_types::model, header, body))
		return false;

	resource::memory_streambuf stream_buffer(body.data(), body.size());
	std::istream istream(&stream_buffer);
	cereal::BinaryInputArchive archive(istream);

	try
	{
		out_material_references.clear();
		archive(CEREAL_NVP(out_material_references),
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

	materials.clear();
	materials.resize(out_material_references.size());
	inherit_uuid(header.uuid);
	set_file_path(file_path);
	DEBUG_LOG_CATEGORY(logger::categorys::model, "model_resource deserialize successed[%s]", file_path.data());
	return true;
}
void model_resource::build_model()
{
	HRESULT hr;

	//	マテリアル
	for (auto& material : materials)
	{
		if (material && material->get_file_path().empty())
			material->build();
	}
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
	auto operation = resource_manager::instance()->load<model_resource>(file_path, force_load);
	if (!operation->is_succeeded())
		return false;

	resource = operation->get_resource();
	return true;
}

namespace resource
{
	operation_state loader<model_resource>::load(std::string_view file_path)
	{
		std::error_code error;
		if (!std::filesystem::exists(std::filesystem::path(std::string(file_path)), error))
		{
			set_error_message("file not found");
			return operation_state::not_found;
		}

		resource = std::make_shared<model_resource>();
		if (!resource->load_file(file_path, material_references))
			return operation_state::deserialize_failed;

		//	参照するマテリアルを依存リソースとして読み込む
		material_operations.resize(material_references.size());
		for (size_t index = 0; index < material_references.size(); ++index)
		{
			material_operations[index] = resource_manager::instance()->load_async(material_references[index], load_priority::high);
			add_dependency(material_operations[index]);
		}
		return operation_state::success;
	}

	operation_state loader<model_resource>::build()
	{
		for (size_t index = 0; index < material_references.size(); ++index)
		{
			if (!material_references[index].is_empty())
				resource->materials[index] = resource_manager::instance()->resolve(material_operations[index], material_references[index], material_resource::domains::surface);
		}
		material_operations.clear();

		resource->build_model();
		return operation_state::success;
	}

	bool saver<model_resource>::save(model_resource& resource, std::string_view file_path)
	{
#if defined(USE_IMGUI)
		data_converter::assign_material_paths(resource.materials, file_path);
#endif	//	defined(USE_IMGUI)
		for (auto& material : resource.materials)
		{
			if (material && !material->is_missing() && !resource_manager::instance()->save(material))
			{
				DEBUG_LOG_CATEGORY(logger::categorys::model, "model_resource material save failed[%s][%s]", file_path.data(), material->get_name().data());
				return false;
			}
		}
		return resource.save_file(file_path);
	}
}	//	namespace resource