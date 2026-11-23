#include "Audio/sound_bank.h"

#include <fstream>

#include "Core/logger.h"

#include "cereal/cereal.hpp"
#include "cereal/archives/json.hpp"
#include "cereal/types/string.hpp"
#include "cereal/types/vector.hpp"

CEREAL_CLASS_VERSION(audio::sound_cue_desc, 1)

namespace audio
{
	template<class Archive>
	void sound_cue_desc::save(Archive& archive, std::uint32_t const version) const
	{
		const std::string bus_name(magic_enum::enum_name(target_bus));
		archive(CEREAL_NVP(name),
			CEREAL_NVP(files),
			cereal::make_nvp("bus", bus_name),
			CEREAL_NVP(volume),
			CEREAL_NVP(pitch),
			CEREAL_NVP(volume_random),
			CEREAL_NVP(pitch_random),
			CEREAL_NVP(loop),
			CEREAL_NVP(fade_in_time),
			CEREAL_NVP(fade_out_time));
	}

	template<class Archive>
	void sound_cue_desc::load(Archive& archive, std::uint32_t const version)
	{
		std::string bus_name;
		archive(CEREAL_NVP(name),
			CEREAL_NVP(files),
			cereal::make_nvp("bus", bus_name),
			CEREAL_NVP(volume),
			CEREAL_NVP(pitch),
			CEREAL_NVP(volume_random),
			CEREAL_NVP(pitch_random),
			CEREAL_NVP(loop),
			CEREAL_NVP(fade_in_time),
			CEREAL_NVP(fade_out_time));

		const auto parsed = magic_enum::enum_cast<bus>(bus_name);
		DEBUG_CHECK_WARNING(!parsed.has_value(), "Sound Cue Unknown Bus[%s][%s]", name.c_str(), bus_name.c_str());
		target_bus = parsed.value_or(bus::se);
	}

	bool sound_bank_desc::load_file(std::string_view file_path)
	{
		std::ifstream istream{ std::string(file_path) };
		if (!istream.is_open())
		{
			DEBUG_WARNING_CATEGORY(core::logger::categorys::system, "Open Failed SoundBank[%s]", std::string(file_path).c_str());
			return false;
		}

		try
		{
			cereal::JSONInputArchive archive(istream);
			archive(CEREAL_NVP(cues));
		}
		catch (...)
		{
			DEBUG_WARNING_CATEGORY(core::logger::categorys::system, "Load Failed SoundBank[%s]", std::string(file_path).c_str());
			return false;
		}
		return true;
	}

	bool sound_bank_desc::save_file(std::string_view file_path) const
	{
		std::ofstream ostream{ std::string(file_path) };
		if (!ostream.is_open())
		{
			DEBUG_WARNING_CATEGORY(core::logger::categorys::system, "Open Failed SoundBank[%s]", std::string(file_path).c_str());
			return false;
		}

		try
		{
			cereal::JSONOutputArchive archive(ostream);
			archive(CEREAL_NVP(cues));
		}
		catch (...)
		{
			DEBUG_WARNING_CATEGORY(core::logger::categorys::system, "Save Failed SoundBank[%s]", std::string(file_path).c_str());
			return false;
		}
		return true;
	}
}
