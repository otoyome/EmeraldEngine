#include "Texture/texture_import_settings.h"

#include "cereal/archives/binary.hpp"
#include "cereal/types/array.hpp"
#include "cereal/types/string.hpp"

texture_import_settings texture_import_settings::make_preset(presets preset)
{
	texture_import_settings settings;
	switch (preset)
	{
		case presets::color:
			settings.transfer_function = transfer_functions::srgb;
			settings.gamut = color_gamuts::rec709;
			settings.format = gpu_formats::bc7;
			settings.generate_mipmaps = true;
			break;
		case presets::normal:
			settings.transfer_function = transfer_functions::linear;
			settings.gamut = color_gamuts::none;
			settings.format = gpu_formats::bc5;
			settings.generate_mipmaps = true;
			break;
		case presets::data:
			settings.transfer_function = transfer_functions::linear;
			settings.gamut = color_gamuts::none;
			settings.format = gpu_formats::uncompressed;
			settings.generate_mipmaps = false;
			break;
	}
	return settings;
}

template<class Archive>
void texture_import_settings::serialize(Archive& archive)
{
	archive(source_path, transfer_function, gamut, format, generate_mipmaps, filter, max_resolution);
	archive(sampler.filter, sampler.address_u, sampler.address_v, sampler.address_w, sampler.max_anisotropy, sampler.mip_lod_bias, sampler.min_lod, sampler.max_lod, sampler.border_color);
}

template void texture_import_settings::serialize<cereal::BinaryInputArchive>(cereal::BinaryInputArchive&);
template void texture_import_settings::serialize<cereal::BinaryOutputArchive>(cereal::BinaryOutputArchive&);
