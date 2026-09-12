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

// CEREALバージョン定義
CEREAL_CLASS_VERSION(effect_resource::node, 1)
CEREAL_CLASS_VERSION(effect_resource::model, 1)
CEREAL_CLASS_VERSION(effect_resource::decal, 1)
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
	archive(node_index, material);
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
: file_path()
, nodes()
, models()
, particles()
, decals()
, animations()
{
}

effect_resource::effect_resource(std::string_view file_path)
: file_path(file_path)
, nodes()
, models()
, particles()
, decals()
, animations()
{
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
	//	モデル情報の構築
	for (auto& model_data : models)
	{
		model_resource_manager::instance()->load(model_data.resource, model_data.file_path);
	}

	//	デカール情報の構築
	for (auto& decal_data : decals)
	{
		//	マテリアル構築
		if (decal_data.material)
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
			if (renderer->material)
				renderer->material->build();
		}
	}
}

bool effect_resource::serialize()
{
	return serialize(file_path);
}

bool effect_resource::deserialize()
{
	return deserialize(file_path);
}

bool effect_resource::serialize(std::string_view file_path)
{
	std::ofstream ostream(file_path.data(), std::ios::binary);
	if(ostream.is_open())
	{
		cereal::BinaryOutputArchive archive(ostream);

		try
		{
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

bool effect_resource::deserialize(std::string_view file_path)
{
	std::ifstream istream(file_path.data(), std::ios::binary);
	if(istream.is_open())
	{
		cereal::BinaryInputArchive archive(istream);

		try
		{
			archive(CEREAL_NVP(nodes),
					CEREAL_NVP(models),
					CEREAL_NVP(particles),
					CEREAL_NVP(decals),
					CEREAL_NVP(animations));
		}
		catch (...)
		{
			DEBUG_ERROR_CATEGORY(logger::categorys::model, "effect_resource deserialize failed[%s]", file_path.data());
			return false;
		}

		//	構築
		this->file_path = file_path;
		build();
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
	std::string key(file_path);

	if (!force_load)
	{
		auto it = resources.find(key);
		if (it != resources.end())
		{
			if (effect_resource_ptr cached = it->second.lock())
			{
				resource = cached;
				return true;
			}

			resources.erase(it);
		}
	}

	auto new_resource = std::make_shared<effect_resource>();
	if (!new_resource->deserialize(file_path))
		return false;

	resource = new_resource;
	resources[key] = resource;
	return true;
}
