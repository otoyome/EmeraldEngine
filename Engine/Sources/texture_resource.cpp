#include "texture_resource.h"

#include <filesystem>
#include <sstream>

#include "texture_defaults.h"
#include "texture_factory.h"
#include "Resource/asset_file_header.h"
#include "Resource/data_compression.h"
#include "system/logger.h"
#include "DirectXTex.h"

#include "cereal/archives/binary.hpp"
#include "cereal/types/vector.hpp"

namespace
{
	bool read_etx(std::string_view file_path, resource::asset_file_header& out_header, texture_import_settings& out_settings, std::vector<char>& out_dds)
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
}

bool texture_resource::read_image(std::string_view file_path, DirectX::ScratchImage& out_image)
{
	resource::asset_file_header header;
	texture_import_settings settings;
	std::vector<char> dds;
	if (!read_etx(file_path, header, settings, dds))
		return false;
	return texture_factory::decode_memory(dds.data(), dds.size(), out_image);
}

bool texture_resource::load_file(std::string_view file_path)
{
	resource::asset_file_header header;
	std::vector<char> dds;
	if (!read_etx(file_path, header, settings, dds))
		return false;

	DirectX::ScratchImage image;
	if (!texture_factory::decode_memory(dds.data(), dds.size(), image))
	{
		DEBUG_ERROR_CATEGORY(logger::categorys::system, "texture_resource decode failed[%s]", std::string(file_path).c_str());
		return false;
	}

	auto created = texture_factory::create_from_image(image);
	if (!created)
	{
		DEBUG_ERROR_CATEGORY(logger::categorys::system, "texture_resource create failed[%s]", std::string(file_path).c_str());
		return false;
	}
	created->set_sampler_desc(settings.sampler);
	value = std::move(created);

	inherit_uuid(header.uuid);
	set_file_path(file_path);
	set_missing(false);
	return true;
}

bool texture_resource::save_file(std::string_view file_path, std::string_view dds)
{
	if (dds.empty())
	{
		DEBUG_ERROR_CATEGORY(logger::categorys::system, "texture_resource has no image data[%s]", std::string(file_path).c_str());
		return false;
	}

	std::ostringstream buffer_stream(std::ios::binary);
	try
	{
		cereal::BinaryOutputArchive archive(buffer_stream);
		archive(settings, cereal::make_size_tag(static_cast<cereal::size_type>(dds.size())), cereal::binary_data(dds.data(), dds.size()));
	}
	catch (...)
	{
		DEBUG_ERROR_CATEGORY(logger::categorys::system, "texture_resource serialize failed[%s]", std::string(file_path).c_str());
		return false;
	}

	if (!resource::write_asset_file(*this, resource::asset_types::texture, file_path, buffer_stream.str()))
		return false;

	if (value)
		value->set_sampler_desc(settings.sampler);
	set_missing(false);
	return true;
}

namespace resource
{
	operation_state loader<texture_resource>::load(std::string_view file_path)
	{
		std::error_code error;
		if (!std::filesystem::exists(std::filesystem::path(std::string(file_path)), error))
		{
			set_error_message("file not found");
			return operation_state::not_found;
		}

		resource = std::make_shared<texture_resource>();
		if (!resource->load_file(file_path))
			return operation_state::deserialize_failed;
		return operation_state::success;
	}

	texture_resource_ptr loader<texture_resource>::create_fallback(const asset_reference<texture_resource>& reference)
	{
		DEBUG_ERROR_CATEGORY(logger::categorys::system, "texture_resource missing[%s]", reference.path.c_str());

		auto fallback = std::make_shared<texture_resource>();
		if (texture_defaults::is_valid())
			fallback->value = texture_defaults::instance()->get_missing();
		fallback->set_file_path(reference.path);
		fallback->inherit_uuid(reference.uuid);
		fallback->set_missing(true);
		return fallback;
	}

	bool loader<texture_resource>::revert(texture_resource& target)
	{
		if (target.is_missing())
			return false;
		target.settings = resource->settings;
		target.value = resource->value;
		return true;
	}

	bool saver<texture_resource>::save(texture_resource& resource, std::string_view file_path)
	{
		const std::string source_path(resource.get_file_path().empty() ? file_path : resource.get_file_path());
		asset_file_header header;
		texture_import_settings unused_settings;
		std::vector<char> dds;
		if (!read_etx(source_path, header, unused_settings, dds))
			return false;
		return resource.save_file(file_path, std::string_view(dds.data(), dds.size()));
	}

	bool saver<texture_resource>::save(texture_resource& resource, std::string_view file_path, std::string_view dds)
	{
		return resource.save_file(file_path, dds);
	}
}	//	namespace resource
