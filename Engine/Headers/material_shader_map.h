#pragma once

#include <memory>
#include <string>
#include <vector>

#include "system/singleton.h"
#include "pipeline_state.h"

//	マテリアルシェーダーマップ
class material_shader_map
{
	SINGLETON(material_shader_map)

public:
	using domains = pipeline_state_base::domains;

	//	surface ドメインのパス
	enum class surface_passes : std::uint8_t
	{
		depth_write,	//	深度書き込み
		shadow_write,	//	シャドウマップ生成
		g_buffer,		//	G-Buffer生成
		forward,		//	フォワードレンダリング
	};

	//	マテリアルシェーダーの登録情報
	struct material_shader_desc
	{
		std::string name;					//	生成するパイプラインステート名
		std::string file_path;				//	マテリアルシェーダーのファイルパス
		domains domain = domains::surface;	//	処理系統
		UINT geometry_passes = 0;			//	ジオメトリフックを使うパス
		bool hidden = false;				//	エディターの一覧に出さない
	};

	//	パスをビットに変換
	static constexpr UINT to_bit(surface_passes pass) { return 1u << static_cast<UINT>(pass); }

private:
	//	登録済みのマテリアルシェーダー
	struct entry
	{
		material_shader_desc desc;
		std::vector<graphics_pipeline_state_ptr> pipeline_states;
	};

private:
	material_shader_map();
	~material_shader_map();

public:
	//	パイプラインステートを生成して pipeline_state_manager に登録する
	bool generate(const material_shader_desc& desc);

private:
	bool generate_surface(entry& target);

private:
	std::vector<std::unique_ptr<entry>> entries;
};
