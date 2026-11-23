#pragma	once

#if defined(USE_IMGUI)
#include <memory>

#include "Model/data_converter.h"
#include "Texture/texture_import_settings.h"
#include "Core/cache_string.h"

//	アセットコンバーター
class asset_converter
{
public:
	//	アセットブラウザに変換処理を登録
	static void register_handlers();

private:
	//	モデル変換
	void imgui_render_gltf_options();
	void imgui_render_mmd_options();
	bool convert_gltf(const core::cache_string& file_path);
	bool convert_mmd(const core::cache_string& file_path);
	static bool save_model(const std::shared_ptr<model_resource>& resource);

	//	テクスチャ変換
	void imgui_render_texture_options();
	bool convert_texture(const core::cache_string& file_path);

	//	拡張子を差し替えた変換先のパス
	static core::cache_string replace_extension(const core::cache_string& file_path, std::string_view extension);

private:	//	ポップアップで選んだ設定
	data_converter::gltf::convert_model_data gltf_option;
	data_converter::mmd::convert_model_data mmd_option;
	texture_import_settings::presets texture_preset = texture_import_settings::presets::color;
	int texture_max_resolution = 0;
};
#endif	//	defined(USE_IMGUI)
