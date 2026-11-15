#include "System/engine_config_data.h"

#include "cereal/cereal.hpp"
#include "cereal/archives/binary.hpp"
#include "cereal/archives/json.hpp"
#include "cereal/types/string.hpp"
#include "cereal/types/vector.hpp"
#include "cereal/types/unordered_map.hpp"

CEREAL_CLASS_VERSION(engine_config_data::application_data, 2)
CEREAL_CLASS_VERSION(engine_config_data::windows_data, 1)
CEREAL_CLASS_VERSION(engine_config_data::antialias_data, 1)
CEREAL_CLASS_VERSION(engine_config_data::rendering_data, 1)
CEREAL_CLASS_VERSION(engine_config_data::collision_data, 1)
CEREAL_CLASS_VERSION(engine_config_data::collision_data::channel_data, 1)
CEREAL_CLASS_VERSION(engine_config_data::collision_data::response_data, 1)
CEREAL_CLASS_VERSION(engine_config_data::collision_data::profile_data, 1)

namespace
{
	//	エンジン設定情報
	static constexpr std::string_view file_path = "engine_config.ini";

	//	ファイル全体のバージョン
	static constexpr std::uint32_t file_version = 2;
}

template<class Archive>
void serialize(Archive& archive, RECT& v)
{
	archive(
		cereal::make_nvp("left", v.left),
		cereal::make_nvp("right", v.right),
		cereal::make_nvp("top", v.top),
		cereal::make_nvp("bottom", v.bottom)
	);
}

template<class Archive>
void engine_config_data::application_data::serialize(Archive& archive, int version)
{
	switch (version)
	{
		case 0:
		case 1:
		{
			archive(CEREAL_NVP(name), CEREAL_NVP(splash_image_path));
			break;
		}
		case 2:
		{
			archive(CEREAL_NVP(name), CEREAL_NVP(splash_image_path), CEREAL_NVP(icon_path));
			break;
		}
	}
}

template<class Archive>
void engine_config_data::windows_data::serialize(Archive& archive, int version)
{
	switch (version)
	{
		case 0:
		{
			archive(CEREAL_NVP(window_left), CEREAL_NVP(window_top), CEREAL_NVP(vertical_sync));

			break;
		}
		case 1:
		{
			archive(CEREAL_NVP(window_left), CEREAL_NVP(window_top), CEREAL_NVP(vertical_sync),
					CEREAL_NVP(window_mode), CEREAL_NVP(screen_width), CEREAL_NVP(screen_height));
			break;
		}
	}
}

template<class Archive>
void engine_config_data::antialias_data::serialize(Archive& archive, int version)
{
	switch (version)
	{
		case 0:
		case 1:
		{
			archive(CEREAL_NVP(mode));
			break;
		}
	}
}

template<class Archive>
void engine_config_data::rendering_data::serialize(Archive& archive, int version)
{
	switch (version)
	{
		case 0:
		{
			archive(CEREAL_NVP(camera_relative_rendering));
			break;
		}
		case 1:
		{
			archive(CEREAL_NVP(camera_relative_rendering), CEREAL_NVP(rendering_gamut));
			break;
		}
	}
}

template<class Archive>
void engine_config_data::collision_data::channel_data::serialize(Archive& archive, int version)
{
	switch (version)
	{
		case 0:
		case 1:
		{
			archive(CEREAL_NVP(name));
			break;
		}
	}
}

template<class Archive>
void engine_config_data::collision_data::response_data::serialize(Archive& archive, int version)
{
	switch (version)
	{
		case 0:
		case 1:
		{
			archive(CEREAL_NVP(channel), CEREAL_NVP(response));
			break;
		}
	}
}

template<class Archive>
void engine_config_data::collision_data::profile_data::serialize(Archive& archive, int version)
{
	switch (version)
	{
		case 0:
		case 1:
		{
			archive(CEREAL_NVP(name), CEREAL_NVP(object_type), CEREAL_NVP(channel_responses));
			break;
		}
	}
}

template<class Archive>
void engine_config_data::collision_data::serialize(Archive& archive, int version)
{
	switch (version)
	{
		case 0:
		case 1:
		{
			archive(CEREAL_NVP(channels), CEREAL_NVP(profiles));
			break;
		}
	}
}

engine_config_data::engine_config_data()
{
	std::ifstream istream(file_path.data());
	if (istream.is_open())
	{
		cereal::JSONInputArchive archive(istream);

		try
		{
			//	"version" が無いファイルは旧形式
			std::uint32_t version = 0;
			try
			{
				archive(CEREAL_NVP(version));
			}
			catch (...)
			{
				version = 0;
			}

			switch (version)
			{
				case 0:
				{
					archive(CEREAL_NVP(windows), CEREAL_NVP(antialias), CEREAL_NVP(rendering));
					break;
				}
				case 1:
				{
					archive(CEREAL_NVP(application), CEREAL_NVP(windows), CEREAL_NVP(antialias), CEREAL_NVP(rendering));
					break;
				}
				case 2:
				{
					archive(CEREAL_NVP(application), CEREAL_NVP(windows), CEREAL_NVP(antialias), CEREAL_NVP(rendering), CEREAL_NVP(collision));
					break;
				}
				default:
				{
					DEBUG_LOG_CATEGORY(logger::categorys::system, "Unknown Version EngineConfig[%s] version[%u]", file_path.data(), version);
					active_rendering_gamut = rendering.rendering_gamut;
					return;
				}
			}
		}
		catch (...)
		{
			DEBUG_LOG_CATEGORY(logger::categorys::system, "Load Failed EngineConfig[%s]", file_path.data());
			active_rendering_gamut = rendering.rendering_gamut;
			return;
		}
		DEBUG_LOG_CATEGORY(logger::categorys::system, "Load Succeeded EngineConfig[%s]", file_path.data());
		active_rendering_gamut = rendering.rendering_gamut;
		return;
	}
	DEBUG_LOG_CATEGORY(logger::categorys::system, "Open Failed EngineConfig[%s]", file_path.data());
	active_rendering_gamut = rendering.rendering_gamut;
}

engine_config_data::~engine_config_data()
{
	std::ofstream ostream(file_path.data());
	if (ostream.is_open())
	{
		cereal::JSONOutputArchive archive(ostream);

		try
		{
			std::uint32_t version = file_version;
			archive(CEREAL_NVP(version), CEREAL_NVP(application), CEREAL_NVP(windows), CEREAL_NVP(antialias), CEREAL_NVP(rendering), CEREAL_NVP(collision));
		}
		catch (...)
		{
			DEBUG_LOG_CATEGORY(logger::categorys::system, "Save Failed EngineConfig[%s]", file_path.data());
			return;
		}
		DEBUG_LOG_CATEGORY(logger::categorys::system, "Save Succeeded EngineConfig[%s]", file_path.data());
		return;
	}
	DEBUG_LOG_CATEGORY(logger::categorys::system, "Open Failed EngineConfig[%s]", file_path.data());
}
