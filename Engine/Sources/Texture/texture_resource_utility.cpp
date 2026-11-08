#include "Texture/texture_resource_utility.h"

#include <sstream>

#include "Texture/texture_factory.h"
#include "Texture/texture_resource.h"
#include "Resource/resource_manager.h"
#include "Resource/data_compression.h"
#include "system/logger.h"
#include "DirectXTex.h"

#include "cereal/archives/binary.hpp"
#include "cereal/types/vector.hpp"

bool texture_resource_utility::read_file(std::string_view file_path, resource::asset_file_header& out_header, texture_import_settings& out_settings, std::vector<char>& out_dds)
{
	std::vector<char> body;
	if (!resource::read_asset_file(file_path, resource::asset_types::texture, out_header, body))
		return false;

	resource::memory_streambuf stream_buffer(body.data(), body.size());
	std::istream istream(&stream_buffer);
	try
	{
		cereal::BinaryInputArchive archive(istream);
		archive(out_settings, out_dds);
	}
	catch (...)
	{
		DEBUG_ERROR_CATEGORY(logger::categorys::system, "texture_resource deserialize failed[%s]", std::string(file_path).c_str());
		return false;
	}
	return true;
}

bool texture_resource_utility::read_image(std::string_view file_path, DirectX::ScratchImage& out_image)
{
	resource::asset_file_header header;
	texture_import_settings settings;
	std::vector<char> dds;
	if (!read_file(file_path, header, settings, dds))
		return false;
	return texture_factory::decode_memory(dds.data(), dds.size(), out_image);
}

texture_ptr texture_resource_utility::load_texture(std::string_view file_path)
{
	if (!resource_manager::is_valid())
		return nullptr;

	auto operation = resource_manager::instance()->load<texture_resource>(file_path);
	const resource::asset_reference<texture_resource> reference{ {}, std::string(file_path) };
	auto loaded = resource_manager::instance()->resolve(operation, reference);
	return loaded ? loaded->get_texture() : nullptr;
}
