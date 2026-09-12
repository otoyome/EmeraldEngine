#include "timeline_resource.h"

#include <fstream>

#include "cereal/cereal.hpp"
#include "cereal/archives/binary.hpp"
#include "cereal/types/string.hpp"
#include "cereal/types/vector.hpp"
#include "cereal/types/variant.hpp"

//	CEREALバージョン定義
CEREAL_CLASS_VERSION(timeline_resource::animation_entry, 1)
CEREAL_CLASS_VERSION(timeline_resource::object_entry::actor_data::transform_entry::parameter, 1)
CEREAL_CLASS_VERSION(timeline_resource::object_entry::actor_data::transform_entry, 1)
CEREAL_CLASS_VERSION(timeline_resource::object_entry::actor_data, 1)
CEREAL_CLASS_VERSION(timeline_resource::object_entry::camera_data::parameter, 1)
CEREAL_CLASS_VERSION(timeline_resource::object_entry::camera_data, 1)
CEREAL_CLASS_VERSION(timeline_resource::object_entry::effect_data, 1)
CEREAL_CLASS_VERSION(timeline_resource::object_entry::event_data, 1)
CEREAL_CLASS_VERSION(timeline_resource::object_entry, 1)

template<class Archive>
void timeline_resource::animation_entry::serialize(Archive& archive, int version)
{
	archive(CEREAL_NVP(animation_name),
			CEREAL_NVP(start_time),
			CEREAL_NVP(end_time),
			CEREAL_NVP(loop));
}

template<class Archive>
void timeline_resource::object_entry::actor_data::transform_entry::parameter::serialize(Archive& archive, int version)
{
	archive(CEREAL_NVP(position),
			CEREAL_NVP(rotation));
}

template<class Archive>
void timeline_resource::object_entry::actor_data::transform_entry::serialize(Archive& archive, int version)
{
	archive(CEREAL_NVP(start_time),
			CEREAL_NVP(end_time),
			CEREAL_NVP(start),
			CEREAL_NVP(end));
}

template<class Archive>
void timeline_resource::object_entry::actor_data::serialize(Archive& archive, int version)
{
	archive(CEREAL_NVP(model_path),
			CEREAL_NVP(animations),
			CEREAL_NVP(transforms));
}

template<class Archive>
void timeline_resource::object_entry::camera_data::parameter::serialize(Archive& archive, int version)
{
	archive(CEREAL_NVP(position),
			CEREAL_NVP(rotation),
			CEREAL_NVP(fov_y));
}

template<class Archive>
void timeline_resource::object_entry::camera_data::serialize(Archive& archive, int version)
{
	archive(CEREAL_NVP(start_time),
			CEREAL_NVP(end_time),
			CEREAL_NVP(start),
			CEREAL_NVP(end));
}

template<class Archive>
void timeline_resource::object_entry::effect_data::serialize(Archive& archive, int version)
{
	archive(CEREAL_NVP(start_time),
			CEREAL_NVP(end_time),
			CEREAL_NVP(effect_path),
			CEREAL_NVP(position));
}

template<class Archive>
void timeline_resource::object_entry::event_data::serialize(Archive& archive, int version)
{
	archive(CEREAL_NVP(time),
			CEREAL_NVP(identifier));
}

template<class Archive>
void timeline_resource::object_entry::serialize(Archive& archive, int version)
{
	archive(CEREAL_NVP(name),
			CEREAL_NVP(data));
}

bool timeline_resource::serialize(std::string_view file_path)
{
	std::ofstream ostream(file_path.data(), std::ios::binary);
	if (ostream.is_open())
	{
		cereal::BinaryOutputArchive archive(ostream);

		try
		{
			archive(CEREAL_NVP(total_time),
					CEREAL_NVP(objects));
		}
		catch (...)
		{
			DEBUG_WARNING("timeline_resource[%s] serialize failed.", file_path.data());
			return false;
		}

		return true;
	}
	return false;
}

bool timeline_resource::deserialize(std::string_view file_path)
{
	std::ifstream istream(file_path.data(), std::ios::binary);
	if (istream.is_open())
	{
		cereal::BinaryInputArchive archive(istream);

		try
		{
			archive(CEREAL_NVP(total_time),
					CEREAL_NVP(objects));
		}
		catch (...)
		{
			DEBUG_WARNING("timeline_resource[%s] deserialize failed.", file_path.data());
			return false;
		}

		return true;
	}
	return false;
}
