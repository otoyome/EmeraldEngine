#pragma once

#include <cstdint>
#include <string>

#include "texture.h"

//	テクスチャの取り込み設定
struct texture_import_settings
{
	//	伝達関数
	enum class transfer_functions : std::uint8_t
	{
		srgb,	//	sRGB
		linear,	//	リニア
	};

	//	色域
	enum class color_gamuts : std::uint8_t
	{
		none,		//	データ(色空間の変換をしない)
		rec709,		//	Rec.709
		rec2020,	//	Rec.2020
	};

	//	GPU 形式
	enum class gpu_formats : std::uint8_t
	{
		uncompressed,	//	非圧縮(R8G8B8A8)
		bc7,			//	BC7(カラー)
		bc1,			//	BC1(アルファ無しカラー)
		bc5,			//	BC5(2 チャンネル、法線)
		bc4,			//	BC4(1 チャンネル)
		bc6h,			//	BC6H(HDR)
		float16,		//	非圧縮の浮動小数(R16G16B16A16_FLOAT)
		source,			//	元の DDS をそのまま使う(キューブマップや作成済みの mip。DDS 以外は失敗する)
	};

	//	ミップマップ生成時の縮小フィルター
	enum class filters : std::uint8_t
	{
		box,
		linear,
		cubic,
		triangle,
	};

	//	テクスチャプリセット
	enum class presets : std::uint8_t
	{
		color,
		normal,
		data,
	};

	std::string source_path;
	transfer_functions transfer_function = transfer_functions::srgb;
	color_gamuts gamut = color_gamuts::rec709;
	gpu_formats format = gpu_formats::bc7;
	bool generate_mipmaps = true;
	filters filter = filters::box;
	std::uint32_t max_resolution = 0;
	texture_sampler_desc sampler;

	static texture_import_settings make_preset(presets preset);

	template<class Archive>
	void serialize(Archive& archive);
};
