#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iosfwd>
#include <string_view>
#include <vector>

#include "Core/uuid.h"

namespace resource
{
	class resource_base;

	//	アセットの種類
	enum class asset_types : std::uint8_t
	{
		unknown,
		model,		//	.emo
		material,	//	.emr
		effect,		//	.eme
		texture,	//	.etx
		scene,		//	.escene
		ik,			//	.eik
		scene_set,	//	.esceneset
		prefab,		//	.eprefab
	};

	//	ファイルヘッダー
	struct asset_file_header
	{
		static constexpr std::uint32_t signature = 0x54455341;	//	'ASET'
		static constexpr std::size_t size = 28;					//	ファイル上の大きさ(バイト)
		static constexpr std::uint32_t current_version = 2;

		asset_types type = asset_types::unknown;
		std::uint32_t version = current_version;
		core::uuid uuid;
	};

	//	ヘッダーの書き込み
	bool write_asset_header(std::ostream& stream, const asset_file_header& header);

	//	ヘッダーの読み込み
	bool read_asset_header(std::istream& stream, asset_file_header& out_header);
	bool read_asset_header(const char* data, std::size_t data_size, asset_file_header& out_header);

	//	ファイルの先頭だけを読む
	bool read_asset_header(const std::filesystem::path& path, asset_file_header& out_header);

	//	ファイル先頭のヘッダーの UUID だけを書き換える
	bool rewrite_asset_header_uuid(const std::filesystem::path& path, const core::uuid& uuid);

	//	拡張子からアセットの種類を求める(「.」付き、大文字小文字は問わない)
	asset_types get_asset_type_from_extension(std::string_view extension);

	//	アセットファイルの保存
	bool write_asset_file(resource_base& target, asset_types type, std::string_view file_path, std::string_view body);

	//	アセットファイルの読込
	bool read_asset_file(std::string_view file_path, asset_types type, asset_file_header& out_header, std::vector<char>& out_body);
}	//	namespace resource
