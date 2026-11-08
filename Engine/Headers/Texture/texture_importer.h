#pragma once

#if defined(USE_IMGUI)

#include <string_view>
#include <vector>

#include "Texture/texture_resource.h"

namespace DirectX
{
	class ScratchImage;
}

//	テクスチャインポータ―
class texture_importer
{
	SINGLETON(texture_importer)

public:
	texture_importer();
	~texture_importer();

	//	テクスチャリソース変換(既にあればUUIDは引き継ぐ)
	texture_resource_ptr import(std::string_view source_path, const texture_import_settings& settings);

	//	テクスチャリソースのリロード
	bool reimport(std::string_view etx_path);

	//	サンプラーだけを変えて保存
	bool update_sampler(texture_resource& target, const texture_sampler_desc& sampler);

private:
	//	元画像を設定どおりに変換して DDS にする
	bool convert_source(std::string_view source_path, const texture_import_settings& settings, std::vector<char>& out_dds);

	//	元のDDSをそのまま読む
	static bool read_source_dds(std::string_view source_path, std::vector<char>& out_dds);

	//	settingsの形式にする
	bool encode(DirectX::ScratchImage& image, const texture_import_settings& settings);

	//	BC6H/BC7のGPU圧縮用のデバイス取得
	ID3D11Device* get_compress_device();

private:
	//	解釈を確定させた作業用の画像にする(8bit か 32bit 浮動小数、mip 無し)
	static bool prepare_source(DirectX::ScratchImage& image, const texture_import_settings& settings);

	//	最大解像度と BC 形式の 4 の倍数に合わせて縮小・拡大する
	static bool resize(DirectX::ScratchImage& image, const texture_import_settings& settings);

private:
	//	描画側のデバイスコンテキストと競合させないため、圧縮専用に別のデバイスを持つ
	Microsoft::WRL::ComPtr<ID3D11Device> compress_device;
	bool compress_device_failed = false;
};

#endif	//	defined(USE_IMGUI)
