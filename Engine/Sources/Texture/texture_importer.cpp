#include "Texture/texture_importer.h"

#if defined(USE_IMGUI)

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <vector>

#include "Texture/texture_factory.h"
#include "Resource/asset_file_header.h"
#include "Resource/resource_manager.h"
#include "System/project_path.h"
#include "system/logger.h"
#include "DirectXTex.h"

namespace
{
	using gpu_formats = texture_import_settings::gpu_formats;

	DirectX::TEX_FILTER_FLAGS to_tex_filter(texture_import_settings::filters filter)
	{
		switch (filter)
		{
			case texture_import_settings::filters::linear:		return DirectX::TEX_FILTER_LINEAR;
			case texture_import_settings::filters::cubic:		return DirectX::TEX_FILTER_CUBIC;
			case texture_import_settings::filters::triangle:	return DirectX::TEX_FILTER_TRIANGLE;
			case texture_import_settings::filters::box:
			default:											return DirectX::TEX_FILTER_DEFAULT;
		}
	}

	bool is_block_compressed(gpu_formats format)
	{
		switch (format)
		{
			case gpu_formats::bc7:
			case gpu_formats::bc1:
			case gpu_formats::bc5:
			case gpu_formats::bc4:
			case gpu_formats::bc6h:
				return true;
			default:
				return false;
		}
	}

	bool is_float_format(DXGI_FORMAT format)
	{
		return DirectX::FormatDataType(format) == DirectX::FORMAT_TYPE_FLOAT;
	}

	//	全ミップマップをまとめて形式変換する
	bool convert(DirectX::ScratchImage& image, DXGI_FORMAT format)
	{
		if (image.GetMetadata().format == format)
			return true;

		DirectX::ScratchImage result;
		if (SUCCEEDED(DirectX::Convert(image.GetImages(), image.GetImageCount(), image.GetMetadata(), format, DirectX::TEX_FILTER_DEFAULT, DirectX::TEX_THRESHOLD_DEFAULT, result)))
		{
			image = std::move(result);
			return true;
		}

		//	WIC は 2GB を超える画像を変換できない
		if (FAILED(DirectX::Convert(image.GetImages(), image.GetImageCount(), image.GetMetadata(), format, DirectX::TEX_FILTER_FORCE_NON_WIC, DirectX::TEX_THRESHOLD_DEFAULT, result)))
			return false;
		image = std::move(result);
		return true;
	}

	//	先頭の 1 枚だけを残す
	bool keep_top_image(DirectX::ScratchImage& image)
	{
		if (image.GetImageCount() == 1)
			return true;

		DirectX::ScratchImage top;
		if (FAILED(top.InitializeFromImage(*image.GetImage(0, 0, 0))))
			return false;
		image = std::move(top);
		return true;
	}
}

texture_importer::texture_importer()
{
}

texture_importer::~texture_importer()
{
}

texture_resource_ptr texture_importer::import(std::string_view source_path, const texture_import_settings& settings)
{
	auto imported = std::make_shared<texture_resource>();
	imported->settings = settings;
	imported->settings.source_path = project_path::to_relative(source_path);

	std::vector<char> dds_data;
	const bool built = settings.format == texture_import_settings::gpu_formats::source
		? read_source_dds(source_path, dds_data)
		: convert_source(source_path, imported->settings, dds_data);
	if (!built)
		return nullptr;
	const std::string_view dds(dds_data.data(), dds_data.size());

	std::filesystem::path path{ std::string(source_path) };
	path.replace_extension(texture_resource::extension);
	const std::string output_path = project_path::normalize(path.string());

	//	取り込み直しでも参照が切れないよう、既存のテクスチャリソースのUUIDを引き継ぐ
	resource::asset_file_header header;
	if (resource::read_asset_header(std::filesystem::path(output_path), header) && header.type == resource::asset_types::texture && !header.uuid.is_empty())
		imported->inherit_uuid(header.uuid);
	imported->set_file_path(output_path);

	if (!resource::saver<texture_resource>().save(*imported, output_path, dds))
		return nullptr;

	DEBUG_LOG_CATEGORY(logger::categorys::system, "texture_importer imported[%s]->[%s]", std::string(source_path).c_str(), output_path.c_str());
	if (!resource_manager::is_valid())
		return imported;

	//	読み込み済みなら同じインスタンスの中身を差し替えて返す
	if (auto loaded = resource_manager::instance()->search<texture_resource>(output_path))
	{
		resource_manager::instance()->revert(*loaded);
		return loaded;
	}
	auto operation = resource_manager::instance()->load<texture_resource>(output_path);
	return operation && operation->is_succeeded() ? operation->get_resource() : nullptr;
}

bool texture_importer::convert_source(std::string_view source_path, const texture_import_settings& settings, std::vector<char>& out_dds)
{
	DirectX::ScratchImage image;
	if (!texture_factory::decode_file(source_path, image))
	{
		DEBUG_ERROR_CATEGORY(logger::categorys::system, "texture_importer decode failed[%s]", std::string(source_path).c_str());
		return false;
	}

	if (!prepare_source(image, settings) || !resize(image, settings))
	{
		DEBUG_ERROR_CATEGORY(logger::categorys::system, "texture_importer prepare failed[%s]", std::string(source_path).c_str());
		return false;
	}

	const auto& meta_data = image.GetMetadata();
	if (settings.generate_mipmaps && (meta_data.width > 1 || meta_data.height > 1))
	{
		DirectX::ScratchImage mip_chain;
		if (FAILED(DirectX::GenerateMipMaps(*image.GetImage(0, 0, 0), to_tex_filter(settings.filter), 0, mip_chain)))
		{
			DEBUG_ERROR_CATEGORY(logger::categorys::system, "texture_importer mipmap failed[%s]", std::string(source_path).c_str());
			return false;
		}
		image = std::move(mip_chain);
	}

	if (!encode(image, settings))
	{
		DEBUG_ERROR_CATEGORY(logger::categorys::system, "texture_importer encode failed[%s]", std::string(source_path).c_str());
		return false;
	}

	DirectX::Blob blob;
	if (FAILED(DirectX::SaveToDDSMemory(image.GetImages(), image.GetImageCount(), image.GetMetadata(), DirectX::DDS_FLAGS_NONE, blob)))
	{
		DEBUG_ERROR_CATEGORY(logger::categorys::system, "texture_importer save dds failed[%s]", std::string(source_path).c_str());
		return false;
	}
	const char* blob_data = reinterpret_cast<const char*>(blob.GetBufferPointer());
	out_dds.assign(blob_data, blob_data + blob.GetBufferSize());
	return true;
}

bool texture_importer::read_source_dds(std::string_view source_path, std::vector<char>& out_dds)
{
	std::ifstream stream(std::filesystem::path(std::string(source_path)), std::ios::binary);
	if (!stream.is_open())
	{
		DEBUG_ERROR_CATEGORY(logger::categorys::system, "texture_importer open failed[%s]", std::string(source_path).c_str());
		return false;
	}
	out_dds.assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());

	DirectX::TexMetadata meta_data;
	if (FAILED(DirectX::GetMetadataFromDDSMemory(reinterpret_cast<const std::uint8_t*>(out_dds.data()), out_dds.size(), DirectX::DDS_FLAGS_NONE, meta_data)))
	{
		DEBUG_ERROR_CATEGORY(logger::categorys::system, "texture_importer source is not dds[%s]", std::string(source_path).c_str());
		return false;
	}
	return true;
}

bool texture_importer::update_sampler(texture_resource& target, const texture_sampler_desc& sampler)
{
	const texture_sampler_desc previous = target.settings.sampler;
	target.settings.sampler = sampler;
	if (!resource::saver<texture_resource>().save(target, target.get_file_path()))
	{
		target.settings.sampler = previous;
		return false;
	}
	return true;
}

bool texture_importer::reimport(std::string_view etx_path)
{
	texture_resource current;
	if (!current.load_file(etx_path))
		return false;

	std::error_code error;
	if (current.settings.source_path.empty() || !std::filesystem::exists(std::filesystem::path(current.settings.source_path), error))
	{
		DEBUG_WARNING_CATEGORY(logger::categorys::system, "texture_importer source not found[%s][%s]", std::string(etx_path).c_str(), current.settings.source_path.c_str());
		return false;
	}
	return import(current.settings.source_path, current.settings) != nullptr;
}

bool texture_importer::prepare_source(DirectX::ScratchImage& image, const texture_import_settings& settings)
{
	const auto& meta_data = image.GetMetadata();
	if (meta_data.dimension != DirectX::TEX_DIMENSION_TEXTURE2D || meta_data.arraySize != 1 || meta_data.IsCubemap())
		return false;

	if (!keep_top_image(image))
		return false;

	if (DirectX::IsCompressed(image.GetMetadata().format))
	{
		DirectX::ScratchImage decompressed;
		if (FAILED(DirectX::Decompress(*image.GetImage(0, 0, 0), DXGI_FORMAT_UNKNOWN, decompressed)))
			return false;
		image = std::move(decompressed);
	}

	const DXGI_FORMAT format = image.GetMetadata().format;
	if (is_float_format(format))
		return convert(image, DXGI_FORMAT_R32G32B32A32_FLOAT);

	const DXGI_FORMAT linear_format = DirectX::MakeLinear(format);
	if (linear_format != format && !image.OverrideFormat(linear_format))
		return false;
	if (!convert(image, DXGI_FORMAT_R8G8B8A8_UNORM))
		return false;
	if (settings.transfer_function == texture_import_settings::transfer_functions::srgb)
		return image.OverrideFormat(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
	return true;
}

bool texture_importer::resize(DirectX::ScratchImage& image, const texture_import_settings& settings)
{
	const auto& meta_data = image.GetMetadata();
	std::size_t width = meta_data.width;
	std::size_t height = meta_data.height;

	const std::size_t longest = (std::max)(width, height);
	if (settings.max_resolution > 0 && longest > settings.max_resolution)
	{
		const double scale = static_cast<double>(settings.max_resolution) / static_cast<double>(longest);
		width = (std::max)(std::size_t{ 1 }, static_cast<std::size_t>(std::lround(width * scale)));
		height = (std::max)(std::size_t{ 1 }, static_cast<std::size_t>(std::lround(height * scale)));
	}

	//	BC 形式は最上位の幅と高さが 4 の倍数でないといけない
	if (is_block_compressed(settings.format))
	{
		width = (width + 3) & ~static_cast<std::size_t>(3);
		height = (height + 3) & ~static_cast<std::size_t>(3);
	}

	if (width == meta_data.width && height == meta_data.height)
		return true;

	DirectX::ScratchImage resized;
	if (FAILED(DirectX::Resize(*image.GetImage(0, 0, 0), width, height, to_tex_filter(settings.filter), resized)))
	{
		//	WIC は 2GB を超える画像を扱えない
		if (FAILED(DirectX::Resize(*image.GetImage(0, 0, 0), width, height, to_tex_filter(settings.filter) | DirectX::TEX_FILTER_FORCE_NON_WIC, resized)))
			return false;
	}
	image = std::move(resized);
	return true;
}

bool texture_importer::encode(DirectX::ScratchImage& image, const texture_import_settings& settings)
{
	const bool is_float = is_float_format(image.GetMetadata().format);
	const bool srgb = settings.transfer_function == texture_import_settings::transfer_functions::srgb;
	const DXGI_FORMAT color_format = srgb ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM;

	auto compress = [&](DXGI_FORMAT format, bool use_gpu)
	{
		DirectX::ScratchImage result;
		HRESULT hr = E_FAIL;
		if (use_gpu)
		{
			if (auto* device = get_compress_device())
				hr = DirectX::Compress(device, image.GetImages(), image.GetImageCount(), image.GetMetadata(), format, DirectX::TEX_COMPRESS_DEFAULT, DirectX::TEX_ALPHA_WEIGHT_DEFAULT, result);
		}
		//	GPU 圧縮が使えなければ CPU で圧縮する
		if (FAILED(hr))
			hr = DirectX::Compress(image.GetImages(), image.GetImageCount(), image.GetMetadata(), format, DirectX::TEX_COMPRESS_PARALLEL, DirectX::TEX_THRESHOLD_DEFAULT, result);
		if (FAILED(hr))
			return false;
		image = std::move(result);
		return true;
	};

	//	1・2 チャンネルの BC 形式は値のまま詰める
	auto to_raw_unorm = [&]()
	{
		if (is_float)
			return convert(image, DXGI_FORMAT_R8G8B8A8_UNORM);
		return image.OverrideFormat(DXGI_FORMAT_R8G8B8A8_UNORM);
	};

	switch (settings.format)
	{
		case gpu_formats::uncompressed:
			return is_float ? convert(image, color_format) : true;
		case gpu_formats::float16:
			return convert(image, DXGI_FORMAT_R16G16B16A16_FLOAT);
		case gpu_formats::bc7:
			if (is_float && !convert(image, color_format))
				return false;
			return compress(DirectX::IsSRGB(image.GetMetadata().format) ? DXGI_FORMAT_BC7_UNORM_SRGB : DXGI_FORMAT_BC7_UNORM, true);
		case gpu_formats::bc1:
			if (is_float && !convert(image, color_format))
				return false;
			return compress(DirectX::IsSRGB(image.GetMetadata().format) ? DXGI_FORMAT_BC1_UNORM_SRGB : DXGI_FORMAT_BC1_UNORM, false);
		case gpu_formats::bc5:
			return to_raw_unorm() && compress(DXGI_FORMAT_BC5_UNORM, false);
		case gpu_formats::bc4:
			return to_raw_unorm() && compress(DXGI_FORMAT_BC4_UNORM, false);
		case gpu_formats::bc6h:
			if (!is_float && !convert(image, DXGI_FORMAT_R16G16B16A16_FLOAT))
				return false;
			return compress(DXGI_FORMAT_BC6H_UF16, true);
	}
	return false;
}

ID3D11Device* texture_importer::get_compress_device()
{
	if (!compress_device && !compress_device_failed)
	{
		HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, compress_device.GetAddressOf(), nullptr, nullptr);
		if (FAILED(hr))
		{
			compress_device_failed = true;
			DEBUG_WARNING_CATEGORY(logger::categorys::system, "texture_importer compress device create failed");
		}
	}
	return compress_device.Get();
}

#endif	//	defined(USE_IMGUI)
