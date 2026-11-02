#pragma once

#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <unordered_map>
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

	//	静的スイッチの宣言
	//	@tool マテリアルシェーダーに書かれた HAS_TEXTURE(テクスチャ名) / MATERIAL_SWITCH(名前) から自動で作る
	//	@tool HAS_TEXTURE はテクスチャの有無で決まり、MATERIAL_SWITCH はマテリアルの設定で切り替える
	struct static_switch_declaration
	{
		std::string name;			//	シェーダーに渡す define 名
		bool default_value = false;	//	既定値
		std::string texture;		//	空でなければ、このテクスチャが設定されているかで値が決まる
	};

	//	マテリアルシェーダーの機能宣言
	//	@tool ファイルの中身から自動で読み取る(宣言の行は書かない)
	struct declaration
	{
		UINT geometry_passes = 0;											//	@tool material_geometry() が定義されているパス(パスごとにプリプロセスして調べる)
		std::vector<static_switch_declaration> static_switches;				//	@tool 静的スイッチ(書かれている順)
		std::string error;													//	解析エラー(空なら正常)

		bool is_valid() const { return error.empty(); }
	};

	//	@tool メッシュから決まる頂点の構成(VS のスキニングとモーフだけが変わる)
	struct vertex_format
	{
		UINT bone_max = 16;		//	@tool 1頂点に影響するボーンの最大数(4 の倍数、0 ならスキニングしない)
		bool morph = true;		//	@tool モーフを適用するか

		bool operator==(const vertex_format& other) const { return bone_max == other.bone_max && morph == other.morph; }
		bool operator!=(const vertex_format& other) const { return !(*this == other); }
	};

	//	パイプラインステートの要求
	struct request
	{
		std::string_view shader_path;				//	マテリアルシェーダーのパス
		surface_passes pass = surface_passes::g_buffer;
		std::uint64_t switch_bits = 0;				//	static_switches の並び順のビット
		vertex_format vertex;						//	@tool 描くメッシュの頂点の構成
		bool pixel = true;							//	depth_write / shadow_write で PS を付けるか(マスク時のみ必要)
	};

	//	パスをビットに変換
	static constexpr UINT to_bit(surface_passes pass) { return 1u << static_cast<UINT>(pass); }

	//	静的スイッチの最大数
	static constexpr std::size_t static_switch_max = 64;

private:
	//	読み込んだマテリアルシェーダー
	struct entry
	{
		std::string shader_path;
		std::filesystem::file_time_type write_time;
		declaration shader_declaration;
		std::map<std::tuple<std::uint64_t, UINT, bool, UINT, bool>, graphics_pipeline_state_ptr> pipeline_states;	//	キーごとのパイプラインステート
	};

private:
	material_shader_map();
	~material_shader_map();

public:
	//	機能宣言を取得
	const declaration* find_declaration(std::string_view shader_path);

	//	パイプラインステートを取得
	graphics_pipeline_state_ptr get_pipeline_state(const request& request);

	//	マテリアルのパラメーター構成に使うパイプラインステートを取得
	void get_parameter_pipeline_states(std::string_view shader_path, std::vector<graphics_pipeline_state_ptr>& out);

#if defined(USE_IMGUI)
	//	編集窓用:Shaders/Materials 以下のマテリアルシェーダーの一覧
	std::vector<std::string> collect_shader_paths() const;
#endif	//	defined(USE_IMGUI)

private:
	entry* find_entry(std::string_view shader_path);
	static void parse_declaration(const std::string& shader_path, declaration& out);
	static std::string generate_source(const entry& target, const vertex_format& vertex, bool include_material, const std::uint64_t* switch_bits);

private:
	std::unordered_map<std::string, std::unique_ptr<entry>> entries;
};
