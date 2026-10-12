#pragma once

#include <filesystem>

namespace collision
{
	//	統合した当たり判定メッシュのリスト情報
	struct combined_mesh
	{
		static constexpr std::string_view extension = "ebmc";
		static constexpr std::uint32_t invalid_material = 0xFFFFFFFF;

		std::vector<std::uint32_t> material_indices;

		//	統合メッシュ本体のファイルパス
		static std::filesystem::path make_mesh_path(std::string_view model_path, size_t combined_index);
		static std::filesystem::path make_info_path(std::string_view model_path);

		//	シリアライズ
		bool serialize(std::string_view file_path) const;
		bool deserialize(std::string_view file_path);
	};
}	//	namespace collision
