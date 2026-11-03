#include <cmath>

#include "effect_resource.h"
#include "graphics_utility.h"
#include "render_data.h"
#include "texture.h"

#include "cereal/cereal.hpp"
#include "cereal/types/memory.hpp"
#include "cereal/archives/binary.hpp"
#include "cereal/archives/json.hpp"
#include "cereal/types/string.hpp"
#include "cereal/types/vector.hpp"
#include "cereal/types/unordered_map.hpp"
#include "cereal/types/variant.hpp"

#include "animation_data_version.h"

#include <filesystem>

#include "Resource/resource_manager.h"

#if defined(USE_IMGUI)
#include "data_converter.h"
#endif	//	defined(USE_IMGUI)

// CEREALバージョン定義
CEREAL_CLASS_VERSION(effect_resource::node, 1)
CEREAL_CLASS_VERSION(effect_resource::model, 1)
CEREAL_CLASS_VERSION(effect_resource::decal, 2)
CEREAL_CLASS_VERSION(effect_resource::particle, 1)
CEREAL_CLASS_VERSION(effect_resource, 1)

template<class Archive>
void effect_resource::node::serialize(Archive& archive, int version)
{
	archive(name, parent_index, scale, rotation, position);
}

bool effect_resource::model::is_valid() const
{
	return resource != nullptr;
}

template<class Archive>
void effect_resource::model::serialize(Archive& archive, int version)
{
	archive(node_index, file_path);
}

bool effect_resource::decal::is_valid() const
{
	return material != nullptr;
}

template<class Archive>
void effect_resource::decal::serialize(Archive& archive, int version)
{
	if (version < 2)
	{
		archive(node_index, material);
		return;
	}

	if constexpr (Archive::is_saving::value)
	{
		std::string path = material ? std::string(material->get_file_path()) : material_path;
		archive(node_index, path);
	}
	else
	{
		archive(node_index, material_path);
	}
}

bool effect_resource::particle::is_valid() const
{
	return particle.is_valid();
}

template<class Archive>
void effect_resource::particle::serialize(Archive& archive, int version)
{
	archive(node_index, particle);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
effect_resource::effect_resource()
: nodes()
, models()
, particles()
, decals()
, animations()
{
}

effect_resource::effect_resource(std::string_view file_path)
: nodes()
, models()
, particles()
, decals()
, animations()
{
	set_file_path(file_path);
}

effect_resource::~effect_resource()
{
}

bool effect_resource::compute_animation_nodes(std::vector<animation::node>& nodes, size_t animation_index, float time) const
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

bool effect_resource::compute_animation_node(animation::node& node, size_t animation_index, size_t node_index, float time) const
{
	if (animation_index >= animations.size())
	{
		return false;
	}
	auto& animation = animations[animation_index];
	float calculate_time = math::clamp(time, 0.0f, animation.end_time);
	return animation.compute_animation(node, node_index, calculate_time);
}

void effect_resource::build()
{
	//	デカール情報の構築
	for (auto& decal_data : decals)
	{
		//	マテリアル構築
		if (decal_data.material && decal_data.material->get_file_path().empty())
			decal_data.material->build();
	}

	//	パーティクル情報の構築
	for (auto& particle_data : particles)
	{
		if (!particle_data.is_valid())
			continue;

		//	パーティクル情報構築
		if (auto renderer = particle_data.particle.get_item<::particle::item::renderer>())
		{
			//	マテリアル構築
			if (renderer->material && renderer->material->get_file_path().empty())
				renderer->material->build();
		}
	}
}

bool effect_resource::serialize()
{
	return serialize(std::string(get_file_path()));
}

bool effect_resource::serialize(std::string_view file_path)
{
	std::vector<material_resource_ptr> save_materials;
	for (auto& decal_data : decals)
	{
		if (decal_data.material)
			save_materials.emplace_back(decal_data.material);
	}
	for (auto& particle_data : particles)
	{
		auto renderer = particle_data.particle.get_item<::particle::item::renderer>();
		if (renderer && renderer->material)
			save_materials.emplace_back(renderer->material);
	}
#if defined(USE_IMGUI)
	data_converter::assign_material_paths(save_materials, file_path);
#endif	//	defined(USE_IMGUI)
	for (auto& material : save_materials)
	{
		if (!material_resource::save_asset(material))
		{
			DEBUG_LOG_CATEGORY(logger::categorys::model, "effect_resource material save failed[%s][%s]", file_path.data(), material->get_name().data());
			return false;
		}
	}

	std::ofstream ostream(file_path.data(), std::ios::binary);
	if(ostream.is_open())
	{
		cereal::BinaryOutputArchive archive(ostream);

		try
		{
			archive(file_signature, file_format_version, get_uuid_for_serialize());

			archive(CEREAL_NVP(nodes),
					CEREAL_NVP(models),
					CEREAL_NVP(particles),
					CEREAL_NVP(decals),
					CEREAL_NVP(animations));
		}
		catch (...)
		{
			DEBUG_LOG_CATEGORY(logger::categorys::model, "effect_resource serialize failed[%s]", file_path.data());
			return false;
		}

		DEBUG_LOG_CATEGORY(logger::categorys::model, "effect_resource serialize successed[%s]", file_path.data());
		return true;
	}

	DEBUG_LOG_CATEGORY(logger::categorys::model, "effect_resource open failed[%s]", file_path.data());
	return false;
}

bool effect_resource::load_file(std::string_view file_path)
{
	std::ifstream istream(file_path.data(), std::ios::binary);
	if(istream.is_open())
	{
		cereal::BinaryInputArchive archive(istream);

		try
		{
			std::uint64_t head = 0;
			archive(head);
			if (head == file_signature)
			{
				std::uint32_t format_version = 0;
				archive(format_version, get_uuid_for_serialize());
				archive(CEREAL_NVP(nodes));
			}
			else
			{
				nodes.resize(static_cast<size_t>(head));
				for (auto& node_data : nodes)
				{
					archive(node_data);
				}
			}

			archive(CEREAL_NVP(models),
					CEREAL_NVP(particles),
					CEREAL_NVP(decals),
					CEREAL_NVP(animations));
		}
		catch (...)
		{
			DEBUG_ERROR_CATEGORY(logger::categorys::model, "effect_resource deserialize failed[%s]", file_path.data());
			return false;
		}

		set_file_path(file_path);
		DEBUG_LOG_CATEGORY(logger::categorys::model, "effect_resource deserialize successed[%s]", file_path.data());
		return true;
	}

	DEBUG_ERROR_CATEGORY(logger::categorys::model, "effect_resource open failed[%s]", file_path.data());
	return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
effect_resource_manager::effect_resource_manager()
{
}

effect_resource_manager::~effect_resource_manager()
{
}

bool effect_resource_manager::load(effect_resource_ptr& resource, std::string_view file_path, bool force_load)
{
	auto operation = resource_manager::instance()->load<effect_resource>(file_path, force_load);
	if (!operation->is_succeeded())
		return false;

	resource = operation->get_resource();
	return true;
}

namespace resource
{
	operation_state loader<effect_resource>::load(std::string_view file_path)
	{
		std::error_code error;
		if (!std::filesystem::exists(std::filesystem::path(std::string(file_path)), error))
		{
			set_error_message("file not found");
			return operation_state::not_found;
		}

		resource = std::make_shared<effect_resource>();
		if (!resource->load_file(file_path))
			return operation_state::deserialize_failed;

		for (auto& model_data : resource->models)
		{
			auto& operation = model_operations.emplace_back();
			if (!model_data.file_path.empty())
				operation = resource_manager::instance()->load_async<model_resource>(model_data.file_path, load_priority::high);
			add_dependency(operation);
		}
		for (auto& decal_data : resource->decals)
		{
			auto& operation = decal_material_operations.emplace_back(material_resource::request_load(decal_data.material_path));
			add_dependency(operation);
		}
		for (auto& particle_data : resource->particles)
		{
			auto renderer = particle_data.particle.get_item<::particle::item::renderer>();
			auto& operation = particle_material_operations.emplace_back(renderer ? material_resource::request_load(renderer->material_path) : nullptr);
			add_dependency(operation);
		}
		return operation_state::success;
	}

	operation_state loader<effect_resource>::build()
	{
		for (size_t index = 0; index < resource->models.size(); ++index)
		{
			auto& operation = model_operations[index];
			resource->models[index].resource = operation && operation->is_succeeded() ? operation->get_resource() : nullptr;
		}

		for (size_t index = 0; index < resource->decals.size(); ++index)
		{
			auto& decal_data = resource->decals[index];
			if (!decal_data.material_path.empty())
				decal_data.material = material_resource::resolve(decal_material_operations[index], decal_data.material_path, material_resource::domains::deferred_decal);
		}
		for (size_t index = 0; index < resource->particles.size(); ++index)
		{
			auto renderer = resource->particles[index].particle.get_item<::particle::item::renderer>();
			if (renderer && !renderer->material_path.empty())
				renderer->material = material_resource::resolve(particle_material_operations[index], renderer->material_path, material_resource::domains::particle);
		}
		model_operations.clear();
		decal_material_operations.clear();
		particle_material_operations.clear();

		resource->build();
		return operation_state::success;
	}
}	//	namespace resource