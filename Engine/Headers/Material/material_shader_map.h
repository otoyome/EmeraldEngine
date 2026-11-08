#pragma once

#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

#include "system/singleton.h"
#include "Graphics/pipeline_state.h"

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

	//	particle ドメインのパス
	enum class particle_passes : std::uint8_t
	{
		depth_write,	//	深度書き込み
		g_buffer,		//	G-Buffer生成
		forward,		//	フォワードレンダリング
	};

	//	deferred_decal ドメインのパス
	enum class decal_passes : std::uint8_t
	{
		primitive,		//	プリミティブで書き込む
		fullscreen,		//	全画面で書き込む
	};

	//	静的スイッチの宣言
	//	マテリアルシェーダーに書かれた MATERIAL_SWITCH(名前) から自動で作る
	struct static_switch_declaration
	{
		std::string name;			//	シェーダーに渡す define 名
		bool default_value = false;	//	既定値
	};

	//	マテリアルシェーダーの機能宣言
	struct declaration
	{
		UINT geometry_passes = 0;											//	material_geometry() が定義されているパス
		std::vector<static_switch_declaration> static_switches;				//	静的スイッチ
		std::string error;													//	解析エラー

		bool is_valid() const { return error.empty(); }
	};

	//	パイプラインステートの要求
	struct request
	{
		std::string_view shader_path;				//	マテリアルシェーダーのパス
		domains domain = domains::surface;			//	マテリアルのドメイン
		UINT pass = 0;								//	ドメインのパスの番号(surface_passes / particle_passes / decal_passes、1パスのドメインは 0)
		std::uint64_t switch_bits = 0;				//	static_switches の並び順のビット
		bool pixel = true;							//	depth_write / shadow_write で PS を付けるか(マスク時のみ必要)
	};

	//	パスをビットに変換
	static constexpr UINT to_bit(UINT pass) { return 1u << pass; }
	static constexpr UINT to_bit(surface_passes pass) { return to_bit(static_cast<UINT>(pass)); }

	//	静的スイッチの最大数
	static constexpr std::size_t static_switch_max = 64;

private:
	//	読み込んだマテリアルシェーダー
	struct entry
	{
		std::string shader_path;
		domains domain = domains::surface;
		std::filesystem::file_time_type write_time;
		declaration shader_declaration;
		std::map<std::tuple<std::uint64_t, UINT, bool>, graphics_pipeline_state_ptr> pipeline_states;	//	キーごとのパイプラインステート
	};

private:
	material_shader_map();
	~material_shader_map();

public:
	//	機能宣言を取得
	const declaration* find_declaration(std::string_view shader_path, domains domain);

	//	パイプラインステートを取得
	graphics_pipeline_state_ptr get_pipeline_state(const request& request);

	//	エラー表示用のマテリアルシェーダーのパイプラインステートを取得
	graphics_pipeline_state_ptr get_failed_pipeline_state(domains domain, UINT pass);

	//	マテリアルのパラメーター構成に使うパイプラインステートを取得
	void get_parameter_pipeline_states(std::string_view shader_path, domains domain, std::vector<graphics_pipeline_state_ptr>& out);

	//	バージョン情報
	std::uint32_t get_version() const { return version; }

#if defined(USE_IMGUI)
	//	Shaders/Materials 以下のマテリアルシェーダーの一覧
	std::vector<std::string> collect_shader_paths(domains domain) const;

	//	元のマテリアルシェーダーを写してドメインのフォルダに新しく作る。作ったパスを返す(失敗したら空)
	std::string create_shader_file(domains domain, std::string_view name, std::string_view source_path) const;
#endif	//	defined(USE_IMGUI)

public:
	//	ドメインがマテリアルシェーダーで描けるか
	static bool is_supported_domain(domains domain);

	//	ドメインのパスの数
	static UINT get_pass_count(domains domain);

	//	ドメインの既定のマテリアルシェーダー
	static std::string_view get_default_shader_path(domains domain);

	//	エラーマテリアルシェーダーパス取得
	static std::string_view get_failed_shader_path(domains domain);

	//	旧置き場所(Shaders/Materials)のパスを今の置き場所のパスに読み替える
	static std::string convert_legacy_shader_path(std::string_view shader_path);

private:
	entry* find_entry(std::string_view shader_path, domains domain);
	static void parse_declaration(const std::string& shader_path, domains domain, declaration& out);
	static std::string generate_source(const entry& target, bool include_material, const std::uint64_t* switch_bits);

private:
	std::unordered_map<std::string, std::unique_ptr<entry>> entries;
	std::uint32_t version = 0;
};
