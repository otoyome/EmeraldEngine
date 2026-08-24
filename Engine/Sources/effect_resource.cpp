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

#include "animation_data_version.h"

// CEREALバージョン定義
CEREAL_CLASS_VERSION(effect_resource::model_data, 1)
CEREAL_CLASS_VERSION(effect_resource::decal_data, 1)
CEREAL_CLASS_VERSION(effect_resource, 1)

bool effect_resource::model_data::is_valid() const
{
	return resource != nullptr;
}

template<class Archive>
void effect_resource::model_data::serialize(Archive& archive, int version)
{
	archive(name, file_path, scale, rotation, position);
}

template<class Archive>
void effect_resource::decal_data::serialize(Archive& archive, int version)
{
	archive(name, material, scale, rotation, position);
}

effect_resource::effect_resource()
: file_path()
, models()
, particles()
, decals()
, animations()
{
}

effect_resource::effect_resource(std::string_view file_path)
: file_path(file_path)
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

size_t effect_resource::get_animation_index_from_name(std::string_view name) const
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

void effect_resource::build()
{
	//	モデル情報の構築
	for (auto& model_data : models)
	{
		model_resource_manager::instance()->load(model_data.resource, model_data.file_path);
	}

	//	パーティクル情報の構築
	for (auto& particle_data : particles)
	{
		if (!particle_data.is_valid())
			continue;

		//	パーティクル情報構築
		if (auto renderer = particle_data.get_item<particle::item::renderer>())
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
			archive(CEREAL_NVP(models),
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

bool effect_resource_manager::load(effect_resource_ptr& resource, std::string_view file_path)
{
	resource = std::make_shared<effect_resource>();
	return resource->deserialize(file_path);
}
