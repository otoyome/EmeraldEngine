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

		//	@tool 自分で保存したファイルを読み直さないように、閉じてから更新時刻を覚えておく
		ostream.close();
		if (file_path == this->file_path)
			update_file_write_time();

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
		update_file_write_time();
		build();
		DEBUG_LOG_CATEGORY(logger::categorys::model, "effect_resource deserialize successed[%s]", file_path.data());
		return true;
	}

	DEBUG_ERROR_CATEGORY(logger::categorys::model, "effect_resource open failed[%s]", file_path.data());
	return false;
}

void effect_resource::update_file_write_time()
{
	std::error_code error;
	const auto write_time = std::filesystem::last_write_time(std::filesystem::path(file_path), error);
	file_write_time = error ? std::filesystem::file_time_type{} : write_time;
}

bool effect_resource::is_file_changed() const
{
	std::error_code error;
	const auto write_time = std::filesystem::last_write_time(std::filesystem::path(file_path), error);
	return !error && write_time != file_write_time;
}

std::shared_ptr<effect_resource> effect_resource::reload()
{
	if (file_path.empty() || !is_file_changed())
		return nullptr;

	//	@tool 別のリソースへ読み込んでから中身を入れ替える（読み込みに失敗したら今の中身のまま）
	//	@tool インスタンスは中身を複製して持つので、描画の完了を待つ必要はない
	auto loaded = std::make_shared<effect_resource>();
	if (!loaded->deserialize(file_path))
		return nullptr;

	std::swap(nodes, loaded->nodes);
	std::swap(models, loaded->models);
	std::swap(particles, loaded->particles);
	std::swap(decals, loaded->decals);
	std::swap(animations, loaded->animations);
	file_write_time = loaded->file_write_time;
	notify_content_changed();

	DEBUG_LOG_CATEGORY(logger::categorys::model, "effect_resource reload successed[%s]", file_path.data());
	return loaded;
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

bool effect_resource_manager::reload_file(const std::filesystem::path& file_path)
{
	//	@tool 登録名は読み込み時の書き方のままなので、絶対パスにそろえて比べる
	std::error_code error;
	const auto target = std::filesystem::absolute(file_path, error).lexically_normal();
	if (error)
		return false;

	bool reloaded = false;
	for (auto& [key, weak_resource] : resources)
	{
		const auto path = std::filesystem::absolute(std::filesystem::path(key), error).lexically_normal();
		if (error || _wcsicmp(path.c_str(), target.c_str()) != 0)
			continue;

		auto resource = weak_resource.lock();
		if (!resource)
			continue;

		if (auto retired = resource->reload())
		{
			retired_resources.emplace_back(std::move(retired));
			reloaded = true;
		}
	}
	return reloaded;
}
