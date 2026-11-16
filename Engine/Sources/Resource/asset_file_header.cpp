#include "Resource/asset_file_header.h"

#include <array>
#include <cstring>
#include <fstream>
#include <string>

#include "Resource/asset_registry.h"
#include "Resource/data_compression.h"
#include "Resource/resource_base.h"
#include "system/logger.h"
#include "System/system_utility.h"

namespace
{
	//	並び：signature(4) type(1) 予備(3) version(4) uuid 上位(8) uuid 下位(8)
	constexpr std::size_t type_offset = 4;
	constexpr std::size_t version_offset = 8;
	constexpr std::size_t uuid_offset = 12;

	using header_bytes = std::array<char, resource::asset_file_header::size>;

	template<class Type>
	void write_value(header_bytes& bytes, std::size_t offset, const Type& value)
	{
		std::memcpy(bytes.data() + offset, &value, sizeof(Type));
	}

	template<class Type>
	Type read_value(const char* data, std::size_t offset)
	{
		Type value{};
		std::memcpy(&value, data + offset, sizeof(Type));
		return value;
	}

	header_bytes encode(const resource::asset_file_header& header)
	{
		header_bytes bytes{};
		write_value(bytes, 0, resource::asset_file_header::signature);
		write_value(bytes, type_offset, header.type);
		write_value(bytes, version_offset, header.version);
		write_value(bytes, uuid_offset, header.uuid.get_high());
		write_value(bytes, uuid_offset + sizeof(std::uint64_t), header.uuid.get_low());
		return bytes;
	}
}

namespace resource
{
	bool write_asset_header(std::ostream& stream, const asset_file_header& header)
	{
		const header_bytes bytes = encode(header);
		stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
		return stream.good();
	}

	bool read_asset_header(const char* data, std::size_t data_size, asset_file_header& out_header)
	{
		if (!data || data_size < asset_file_header::size)
			return false;
		if (read_value<std::uint32_t>(data, 0) != asset_file_header::signature)
			return false;

		out_header.type = read_value<asset_types>(data, type_offset);
		out_header.version = read_value<std::uint32_t>(data, version_offset);
		//	上位・下位の順は GUID のバイト列と同じ並び
		out_header.uuid = universally_unique_identifier(read_value<GUID>(data, uuid_offset));
		return true;
	}

	bool read_asset_header(std::istream& stream, asset_file_header& out_header)
	{
		header_bytes bytes{};
		stream.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
		if (stream.gcount() != static_cast<std::streamsize>(bytes.size()))
			return false;
		return read_asset_header(bytes.data(), bytes.size(), out_header);
	}

	bool read_asset_header(const std::filesystem::path& path, asset_file_header& out_header)
	{
		std::ifstream stream(path, std::ios::binary);
		return stream.is_open() && read_asset_header(stream, out_header);
	}

	bool rewrite_asset_header_uuid(const std::filesystem::path& path, const universally_unique_identifier& uuid)
	{
		asset_file_header header;
		if (!read_asset_header(path, header))
			return false;

		header.uuid = uuid;
		const header_bytes bytes = encode(header);

		std::fstream stream(path, std::ios::binary | std::ios::in | std::ios::out);
		if (!stream.is_open())
			return false;
		stream.seekp(0, std::ios::beg);
		stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
		return stream.good();
	}

	asset_types get_asset_type_from_extension(std::string_view extension)
	{
		const std::string lower = system_utility::to_lower(extension);

		if (lower == ".emo")
			return asset_types::model;
		if (lower == ".emr")
			return asset_types::material;
		if (lower == ".eme")
			return asset_types::effect;
		if (lower == ".etx")
			return asset_types::texture;
		if (lower == ".escene")
			return asset_types::scene;
		if (lower == ".eik")
			return asset_types::ik;
		if (lower == ".esceneset")
			return asset_types::scene_set;
		return asset_types::unknown;
	}

	bool write_asset_file(resource_base& target, asset_types type, std::string_view file_path, std::string_view body)
	{
		if (asset_registry::is_valid())
			asset_registry::instance()->prepare_save(target, file_path);

		std::vector<char> compressed_body;
		if (!compress(body.data(), body.size(), compressed_body))
		{
			DEBUG_ERROR_CATEGORY(logger::categorys::system, "asset file compress failed[%s]", std::string(file_path).c_str());
			return false;
		}

		const std::filesystem::path path{ std::string(file_path) };
		if (path.has_parent_path())
		{
			std::error_code error;
			std::filesystem::create_directories(path.parent_path(), error);
		}

		std::ofstream stream(path, std::ios::binary);
		if (!stream.is_open())
		{
			DEBUG_ERROR_CATEGORY(logger::categorys::system, "asset file open failed[%s]", std::string(file_path).c_str());
			return false;
		}

		asset_file_header header;
		header.type = type;
		header.uuid = target.get_uuid();
		write_asset_header(stream, header);
		stream.write(compressed_body.data(), static_cast<std::streamsize>(compressed_body.size()));
		if (!stream.good())
		{
			DEBUG_ERROR_CATEGORY(logger::categorys::system, "asset file write failed[%s]", std::string(file_path).c_str());
			return false;
		}

		target.set_file_path(file_path);
		if (asset_registry::is_valid())
			asset_registry::instance()->register_asset(header.uuid, type, file_path);
		return true;
	}

	bool read_asset_file(std::string_view file_path, asset_types type, asset_file_header& out_header, std::vector<char>& out_body)
	{
		std::ifstream stream(std::filesystem::path(std::string(file_path)), std::ios::binary);
		if (!stream.is_open())
		{
			DEBUG_ERROR_CATEGORY(logger::categorys::system, "asset file open failed[%s]", std::string(file_path).c_str());
			return false;
		}
		const std::vector<char> file_data((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());

		if (!read_asset_header(file_data.data(), file_data.size(), out_header) || out_header.type != type)
		{
			DEBUG_ERROR_CATEGORY(logger::categorys::system, "asset file invalid header[%s]", std::string(file_path).c_str());
			return false;
		}

		if (!decompress(file_data.data() + asset_file_header::size, file_data.size() - asset_file_header::size, out_body))
		{
			DEBUG_ERROR_CATEGORY(logger::categorys::system, "asset file decompress failed[%s]", std::string(file_path).c_str());
			return false;
		}
		return true;
	}
}	//	namespace resource
