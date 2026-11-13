#include "Animation/ik_resource.h"

#include "cereal/cereal.hpp"
#include "cereal/archives/binary.hpp"
#include "cereal/archives/json.hpp"
#include "cereal/types/string.hpp"
#include "cereal/types/vector.hpp"
#include "cereal/types/unordered_map.hpp"
#include "cereal/types/variant.hpp"
#include "cereal/types/array.hpp"

#include <filesystem>
#include <sstream>

#include "Resource/asset_file_header.h"
#include "Resource/data_compression.h"

// CEREALバージョン定義
CEREAL_CLASS_VERSION(ik_resource::foot_data, 4)
CEREAL_CLASS_VERSION(ik_resource::hand_data, 1)
CEREAL_CLASS_VERSION(ik_resource::look_at_data, 1)
CEREAL_CLASS_VERSION(ik_resource::node, 2)
CEREAL_CLASS_VERSION(ik_resource::solve_data, 3)
CEREAL_CLASS_VERSION(ik_resource::behavior_data, 1)

template<class Archive>
void ik_resource::foot_data::serialize(Archive& archive, int version)
{
	switch (version)
	{
		case 1:
		{
			size_t pelvis_node_index = invalid_id; // 腰補正ボーン
			archive(CEREAL_NVP(ray_length),
					CEREAL_NVP(foot_offset),
					CEREAL_NVP(pelvis_node_index),
					CEREAL_NVP(align_to_normal));
			break;
		}
		case 2:
		{
			archive(CEREAL_NVP(ray_length),
					CEREAL_NVP(foot_offset),
					CEREAL_NVP(adjust_node_indices),
					CEREAL_NVP(align_to_normal));
			break;
		}
		case 3:
		{
			std::array<math::float3, 2> ray_origin_offsets;
			archive(CEREAL_NVP(ray_length),
					CEREAL_NVP(ray_origin_offsets),
					CEREAL_NVP(foot_offset),
					CEREAL_NVP(adjust_node_indices),
					CEREAL_NVP(align_to_normal));
			break;
		}
		case 4:
		{
			archive(CEREAL_NVP(ray_length),
					CEREAL_NVP(ray_origin_offsets),
					CEREAL_NVP(foot_offset),
					CEREAL_NVP(adjust_node_indices),
					CEREAL_NVP(align_to_normal));
			break;
		}
	}
}

template<class Archive>
void ik_resource::hand_data::serialize(Archive& archive, int version)
{
	archive(CEREAL_NVP(target_offset));
}

template<class Archive>
void ik_resource::look_at_data::serialize(Archive& archive, int version)
{
	archive(CEREAL_NVP(use_spine),
			CEREAL_NVP(use_eye));
}

template<class Archive>
void ik_resource::node::serialize(Archive& archive, int version)
{
	switch (version)
	{
		case 1:
		{
			float weight = 1.0f; // 影響度
			archive(CEREAL_NVP(source_node_name),
					CEREAL_NVP(mode),
					CEREAL_NVP(limit_angles),
					CEREAL_NVP(use_axis),
					CEREAL_NVP(weight));
			break;
		}
		case 2:
		{
			archive(CEREAL_NVP(source_node_name),
					CEREAL_NVP(mode),
					CEREAL_NVP(limit_angles),
					CEREAL_NVP(use_axis));
			break;
		}
	}
}

template<class Archive>
void ik_resource::solve_data::serialize(Archive& archive, int version)
{
	switch (version)
	{
		case 1:
		{
			archive(CEREAL_NVP(name), CEREAL_NVP(mode), CEREAL_NVP(node_list),

					CEREAL_NVP(target_node_index), CEREAL_NVP(iteration), CEREAL_NVP(constraint_angle),

					CEREAL_NVP(use_pole_node_offset), CEREAL_NVP(pole_offset));
			break;
		}
		case 2:
		{
			bool aim_axis_local = false;
			archive(CEREAL_NVP(name), CEREAL_NVP(mode), CEREAL_NVP(node_list),

					CEREAL_NVP(target_node_index), CEREAL_NVP(iteration), CEREAL_NVP(constraint_angle),

					CEREAL_NVP(use_pole_node_offset), CEREAL_NVP(pole_offset),

					CEREAL_NVP(aim_axis_local), CEREAL_NVP(aim_axis));
			break;
		}
		case 3:
		{
			archive(CEREAL_NVP(name), CEREAL_NVP(mode), CEREAL_NVP(node_list),

					CEREAL_NVP(target_node_index), CEREAL_NVP(iteration), CEREAL_NVP(constraint_angle),

					CEREAL_NVP(use_pole_node_offset), CEREAL_NVP(pole_offset),

					CEREAL_NVP(aim_axis), CEREAL_NVP(aim_offset));
			break;
		}
	}
}

template<class Archive>
void ik_resource::behavior_data::serialize(Archive& archive, int version)
{
	archive(CEREAL_NVP(name),
			CEREAL_NVP(mode),
			CEREAL_NVP(solve_ids),
			CEREAL_NVP(data));
}

ik_resource::ik_resource()
{
}

ik_resource::~ik_resource()
{
}

std::size_t ik_resource::search_index_by_name(std::string_view data_name)
{
	std::string name = std::string(data_name);
	auto it = std::find_if(datas.begin(), datas.end(),
						[&](const solve_data& r) { return name == r.name; });
	if (it == datas.end())
		return invalid_id;

	return std::distance(datas.begin(), it);
}

bool ik_resource::save_file(std::string_view file_path)
{
	std::ostringstream buffer_stream(std::ios::binary);
	{
		cereal::BinaryOutputArchive archive(buffer_stream);

		try
		{
			archive(CEREAL_NVP(nodes),
					CEREAL_NVP(datas),
					CEREAL_NVP(behavior_datas));
		}
		catch (...)
		{
			DEBUG_LOG_CATEGORY(logger::categorys::model, "ik_resource serialize failed[%s]", file_path.data());
			return false;
		}
	}

	if (!resource::write_asset_file(*this, resource::asset_types::ik, file_path, buffer_stream.str()))
		return false;

	DEBUG_LOG_CATEGORY(logger::categorys::model, "ik_resource serialize successed[%s]", file_path.data());
	return true;
}

bool ik_resource::load_file(std::string_view file_path)
{
	resource::asset_file_header header;
	std::vector<char> body;
	if (!resource::read_asset_file(file_path, resource::asset_types::ik, header, body))
		return false;

	resource::memory_streambuf stream_buffer(body.data(), body.size());
	std::istream istream(&stream_buffer);
	cereal::BinaryInputArchive archive(istream);

	try
	{
		archive(CEREAL_NVP(nodes),
				CEREAL_NVP(datas),
				CEREAL_NVP(behavior_datas));
	}
	catch (...)
	{
		DEBUG_ERROR_CATEGORY(logger::categorys::model, "ik_resource deserialize failed[%s]", file_path.data());
		return false;
	}

	inherit_uuid(header.uuid);
	set_file_path(file_path);
	DEBUG_LOG_CATEGORY(logger::categorys::model, "ik_resource deserialize successed[%s]", file_path.data());
	return true;
}
namespace resource
{
	operation_state loader<ik_resource>::load(std::string_view file_path)
	{
		std::error_code error;
		if (!std::filesystem::exists(std::filesystem::path(std::string(file_path)), error))
		{
			set_error_message("file not found");
			return operation_state::not_found;
		}

		resource = std::make_shared<ik_resource>();
		if (!resource->load_file(file_path))
			return operation_state::deserialize_failed;

		return operation_state::success;
	}

	bool saver<ik_resource>::save(ik_resource& resource, std::string_view file_path)
	{
		return resource.save_file(file_path);
	}
}	//	namespace resource
