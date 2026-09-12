#include "timeline_resource.h"

#include <fstream>

#include "cereal/cereal.hpp"
#include "cereal/archives/binary.hpp"
#include "cereal/types/string.hpp"
#include "cereal/types/vector.hpp"

//	CEREALバージョン定義
CEREAL_CLASS_VERSION(timeline_resource::animation_entry, 1)
CEREAL_CLASS_VERSION(timeline_resource::actor_entry, 1)
CEREAL_CLASS_VERSION(timeline_resource::effect_entry, 1)
CEREAL_CLASS_VERSION(timeline_resource::camera_entry, 1)

template<class Archive>
void timeline_resource::animation_entry::serialize(Archive& archive, int version)
{
	archive(CEREAL_NVP(animation_name),
			CEREAL_NVP(start_frame),
			CEREAL_NVP(end_frame),
			CEREAL_NVP(loop));
}

template<class Archive>
void timeline_resource::actor_entry::serialize(Archive& archive, int version)
{
	archive(CEREAL_NVP(name),
			CEREAL_NVP(model_path),
			CEREAL_NVP(animations));
}

template<class Archive>
void timeline_resource::effect_entry::serialize(Archive& archive, int version)
{
	archive(CEREAL_NVP(effect_path),
			CEREAL_NVP(start_frame),
			CEREAL_NVP(end_frame),
			CEREAL_NVP(actor_index),
			CEREAL_NVP(offset_position));
}

template<class Archive>
void timeline_resource::camera_entry::parameter::serialize(Archive& archive, int version)
{
	archive(CEREAL_NVP(position),
			CEREAL_NVP(rotation),
			CEREAL_NVP(fov_y));
}

template<class Archive>
void timeline_resource::camera_entry::serialize(Archive& archive, int version)
{
	archive(CEREAL_NVP(start_frame),
			CEREAL_NVP(end_frame),
			CEREAL_NVP(start),
			CEREAL_NVP(end));
}

bool timeline_resource::serialize(std::string_view file_path)
{
	std::ofstream ostream(file_path.data(), std::ios::binary);
	if (ostream.is_open())
	{
		cereal::BinaryOutputArchive archive(ostream);

		try
		{
			archive(CEREAL_NVP(fps),
					CEREAL_NVP(total_frame),
					CEREAL_NVP(actors),
					CEREAL_NVP(effects),
					CEREAL_NVP(cameras));
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
			archive(CEREAL_NVP(fps),
					CEREAL_NVP(total_frame),
					CEREAL_NVP(actors),
					CEREAL_NVP(effects),
					CEREAL_NVP(cameras));
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
