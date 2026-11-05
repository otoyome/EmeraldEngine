#pragma once

#if defined(USE_IMGUI)

#include <string_view>

#include "texture_resource.h"

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

	//	元画像を取り込んでテクスチャリソースを書き出す。既にがあれば UUID を引き継ぐ
	bool import(std::string_view source_path, std::string_view output_path, const texture_import_settings& settings);

	//	テクスチャリソースに控えた元画像と設定で取り込み直す
	bool reimport(std::string_view etx_path);

private:
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
