#include "System/engine_config_data.h"

#include "cereal/cereal.hpp"
#include "cereal/archives/binary.hpp"
#include "cereal/archives/json.hpp"
#include "cereal/types/string.hpp"
#include "cereal/types/vector.hpp"
#include "cereal/types/unordered_map.hpp"
#include "cereal/types/unordered_map.hpp"

namespace
{
	//	ÉGÉìÉWÉìê›íËèÓïÒ
	static constexpr std::string_view file_path = "engine_config.ini";
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
	archive(CEREAL_NVP(name), CEREAL_NVP(splash_image_path));
}

template<class Archive>
void engine_config_data::windows_data::serialize(Archive& archive, int version)
{
	archive(CEREAL_NVP(window_left), CEREAL_NVP(window_top), CEREAL_NVP(vertical_sync));
}

template<class Archive>
void engine_config_data::antialias_data::serialize(Archive& archive, int version)
{
	archive(CEREAL_NVP(mode));
}

template<class Archive>
void engine_config_data::rendering_data::serialize(Archive& archive, int version)
{
	archive(CEREAL_NVP(camera_relative_rendering));

	try
	{
		archive(CEREAL_NVP(rendering_gamut));
	}
	catch (...)
	{
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
			archive(CEREAL_NVP(windows), CEREAL_NVP(antialias), CEREAL_NVP(rendering));

			try
			{
				archive(CEREAL_NVP(application));
			}
			catch (...)
			{
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
			archive(CEREAL_NVP(application), CEREAL_NVP(windows), CEREAL_NVP(antialias), CEREAL_NVP(rendering));
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
