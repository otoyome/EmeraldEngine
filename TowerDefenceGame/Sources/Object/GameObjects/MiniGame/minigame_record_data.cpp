#include "object/gameobjects/minigame/minigame_record_data.h"

#include "cereal/cereal.hpp"
#include "cereal/archives/json.hpp"
#include "cereal/types/vector.hpp"

#include <filesystem>

namespace
{
	//	É~ÉjÉQÅ[ÉÄÇÃãLò^ï€ë∂êÊ
	static constexpr std::string_view file_path = "Saves/minigame_record.json";
}

template<class Archive>
void minigame_record_entry::serialize(Archive& archive, int version)
{
	archive(CEREAL_NVP(clear_time), CEREAL_NVP(max_speed_kmh), CEREAL_NVP(trash_count), CEREAL_NVP(trash_max));
}

const std::vector<minigame_record_entry>& minigame_record_data::get_records() const
{
	return records;
}

void minigame_record_data::add_record(const minigame_record_entry& entry)
{
	records.insert(records.begin(), entry);
	if (records.size() > record_max)
	{
		records.resize(record_max);
	}
	save();
}

minigame_record_data::minigame_record_data()
{
	std::ifstream istream(file_path.data());
	if (istream.is_open())
	{
		cereal::JSONInputArchive archive(istream);

		try
		{
			archive(CEREAL_NVP(records));
		}
		catch (...)
		{
			DEBUG_LOG_CATEGORY(logger::categorys::system, "Load Failed MinigameRecord[%s]", file_path.data());
			return;
		}
		DEBUG_LOG_CATEGORY(logger::categorys::system, "Load Succeeded MinigameRecord[%s]", file_path.data());
		return;
	}
	DEBUG_LOG_CATEGORY(logger::categorys::system, "Open Failed MinigameRecord[%s]", file_path.data());
}

minigame_record_data::~minigame_record_data()
{
}

void minigame_record_data::save()
{
	std::error_code error;
	std::filesystem::create_directories(std::filesystem::path(file_path).parent_path(), error);

	std::ofstream ostream(file_path.data());
	if (ostream.is_open())
	{
		cereal::JSONOutputArchive archive(ostream);

		try
		{
			archive(CEREAL_NVP(records));
		}
		catch (...)
		{
			DEBUG_LOG_CATEGORY(logger::categorys::system, "Save Failed MinigameRecord[%s]", file_path.data());
			return;
		}
		DEBUG_LOG_CATEGORY(logger::categorys::system, "Save Succeeded MinigameRecord[%s]", file_path.data());
		return;
	}
	DEBUG_LOG_CATEGORY(logger::categorys::system, "Open Failed MinigameRecord[%s]", file_path.data());
}
