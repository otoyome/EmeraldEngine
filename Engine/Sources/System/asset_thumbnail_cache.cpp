#include "System/asset_thumbnail_cache.h"

#if defined(USE_IMGUI)
#include <algorithm>
#include <cmath>

#include "texture.h"
#include "texture_factory.h"
#include "texture_resource.h"
#include "Resource/asset_file_header.h"
#include "Resource/asset_registry.h"
#include "system/logger.h"
#include "DirectXTex.h"

namespace
{
	bool convert(DirectX::ScratchImage& image, DXGI_FORMAT format)
	{
		if (image.GetMetadata().format == format)
			return true;

		DirectX::ScratchImage result;
		if (FAILED(DirectX::Convert(*image.GetImage(0, 0, 0), format, DirectX::TEX_FILTER_DEFAULT, DirectX::TEX_THRESHOLD_DEFAULT, result)))
			return false;
		image = std::move(result);
		return true;
	}
}

std::shared_ptr<texture> asset_thumbnail_cache::load(const cache_string& asset_path)
{
	if (!is_supported(asset_path))
		return nullptr;

	const universally_unique_identifier uuid = find_uuid(asset_path);
	if (uuid.is_empty())
		return nullptr;

	const std::filesystem::path cache_path = make_cache_path(uuid);
	std::error_code error;
	const auto asset_time = std::filesystem::last_write_time(std::filesystem::path(asset_path.get<utf16_cache>()), error);
	if (error)
		return nullptr;

	const auto cache_time = std::filesystem::last_write_time(cache_path, error);
	if (!error && cache_time >= asset_time)
	{
		DirectX::ScratchImage cached;
		if (SUCCEEDED(DirectX::LoadFromDDSFile(cache_path.c_str(), DirectX::DDS_FLAGS_NONE, nullptr, cached)))
			return texture_factory::create_from_image(cached);
	}

	DirectX::ScratchImage source;
	DirectX::ScratchImage thumbnail;
	if (!read_source(asset_path, source) || !make_thumbnail(source, thumbnail))
	{
		DEBUG_WARNING_CATEGORY(logger::categorys::system, "asset_thumbnail_cache create failed[%s]", asset_path.c_str());
		return nullptr;
	}

	std::filesystem::create_directories(cache_path.parent_path(), error);
	if (FAILED(DirectX::SaveToDDSFile(*thumbnail.GetImage(0, 0, 0), DirectX::DDS_FLAGS_NONE, cache_path.c_str())))
		DEBUG_WARNING_CATEGORY(logger::categorys::system, "asset_thumbnail_cache save failed[%s]", asset_path.c_str());

	return texture_factory::create_from_image(thumbnail);
}

bool asset_thumbnail_cache::is_supported(const cache_string& asset_path)
{
	return resource::get_asset_type_from_extension(asset_path.get<extension_cache>()) == resource::asset_types::texture;
}

universally_unique_identifier asset_thumbnail_cache::find_uuid(const cache_string& asset_path)
{
	if (asset_registry::is_valid())
	{
		const universally_unique_identifier uuid = asset_registry::instance()->find_uuid(asset_path.view());
		if (!uuid.is_empty())
			return uuid;
	}

	resource::asset_file_header header;
	if (resource::read_asset_header(std::filesystem::path(asset_path.get<utf16_cache>()), header))
		return header.uuid;
	return {};
}

std::filesystem::path asset_thumbnail_cache::make_cache_path(const universally_unique_identifier& uuid)
{
	return std::filesystem::path(cache_directory) / (uuid.to_string() + ".dds");
}

bool asset_thumbnail_cache::read_source(const cache_string& asset_path, DirectX::ScratchImage& out_image)
{
	switch (resource::get_asset_type_from_extension(asset_path.get<extension_cache>()))
	{
		case resource::asset_types::texture:
			return texture_resource::read_image(asset_path.view(), out_image);
		default:
			return false;
	}
}

bool asset_thumbnail_cache::make_thumbnail(const DirectX::ScratchImage& source, DirectX::ScratchImage& out_image)
{
	const auto& meta_data = source.GetMetadata();
	std::size_t mip = 0;
	while (mip + 1 < meta_data.mipLevels &&
		(std::max)((std::max)(meta_data.width >> (mip + 1), std::size_t{ 1 }), (std::max)(meta_data.height >> (mip + 1), std::size_t{ 1 })) >= thumbnail_size)
		++mip;

	const DirectX::Image* image = source.GetImage(mip, 0, 0);
	if (!image)
		return false;

	DirectX::ScratchImage work;
	if (DirectX::IsCompressed(image->format))
	{
		if (FAILED(DirectX::Decompress(*image, DXGI_FORMAT_UNKNOWN, work)))
			return false;
	}
	else if (FAILED(work.InitializeFromImage(*image)))
	{
		return false;
	}

	const auto& work_data = work.GetMetadata();
	const std::size_t longest = (std::max)(work_data.width, work_data.height);
	if (longest > thumbnail_size)
	{
		const double scale = static_cast<double>(thumbnail_size) / static_cast<double>(longest);
		const std::size_t width = (std::max)(std::size_t{ 1 }, static_cast<std::size_t>(std::lround(work_data.width * scale)));
		const std::size_t height = (std::max)(std::size_t{ 1 }, static_cast<std::size_t>(std::lround(work_data.height * scale)));
		DirectX::ScratchImage resized;
		if (FAILED(DirectX::Resize(*work.GetImage(0, 0, 0), width, height, DirectX::TEX_FILTER_DEFAULT, resized)))
			return false;
		work = std::move(resized);
	}

	const DXGI_FORMAT format = work.GetMetadata().format;
	if (!DirectX::IsSRGB(format) && DirectX::FormatDataType(format) != DirectX::FORMAT_TYPE_FLOAT)
	{
		if (!convert(work, DXGI_FORMAT_R8G8B8A8_UNORM) || !work.OverrideFormat(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB))
			return false;
	}
	else if (!convert(work, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB))
	{
		return false;
	}

	out_image = std::move(work);
	return true;
}
#endif	//	defined(USE_IMGUI)
