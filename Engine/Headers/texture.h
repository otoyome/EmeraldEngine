#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include "system/parallel_for_executor.h"

enum class texture_data_types
{
	color,	//	カラーマップ
	normal,	//	法線マップ
};

//	サンプラー設定
struct texture_sampler_desc
{
	//	フィルタ
	enum class filters
	{
		point,			//	ポイント
		linear,			//	リニア
		anisotropic,	//	異方性

		max,
	};

	//	アドレスモード
	enum class address_modes
	{
		wrap,			//	繰り返し
		mirror,			//	反転して繰り返し
		clamp,			//	端の色で引き伸ばし
		border,			//	境界色
		mirror_once,	//	一度だけ反転して以降は端の色

		max,
	};

	filters filter = filters::linear;
	address_modes address_u = address_modes::wrap;
	address_modes address_v = address_modes::wrap;
	address_modes address_w = address_modes::wrap;
	UINT max_anisotropy = 16;
	float mip_lod_bias = 0.0f;
	float min_lod = 0.0f;
	float max_lod = D3D11_FLOAT32_MAX;
	std::array<float, 4> border_color{ 0.0f, 0.0f, 0.0f, 0.0f };

	D3D11_SAMPLER_DESC to_d3d11_desc() const;
};

//	簡易的なテクスチャのメタデータ
struct texture_meta_data
{
	static constexpr uint32_t current_version = 1;

	texture_data_types type;
	UINT system_flags;
	texture_sampler_desc sampler;

	bool serialize(std::string_view file_path);
	bool deserialize(std::string_view file_path);
};

//	テクスチャ
//	☆ﾃｷﾄｰ('ω')ｰﾄｷﾃ☆
class texture
{
	using file_path_type = std::filesystem::path;
	using file_time_type = std::filesystem::file_time_type;

public:
	enum class flags
	{
		srgb,		//	SRGB対応済み

		cube_map,	//	キューブマップ
	};

public:
	D3D11_RESOURCE_DIMENSION	get_dimension() const;
	//	@tool 取得時点のビューを返す（再読み込みで差し替わっても、受け取った側が持っている間は有効）
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> get_shader_resource_view() const;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> get_unordered_access_view() const;

	texture_data_types get_type() const;
	void set_type(texture_data_types type);

	//	ファイルパスであればtrue, キーであればfalse
	bool is_file_path_or_key() const;
	//	ファイルパスかキーを取得
	std::string_view get_file_path_or_key() const;
	void set_file_path(std::string_view file_path);
	void set_data_key(std::string_view key);

	UINT get_system_flags() const { return system_flags; }
	void set_system_flags(UINT flags) { system_flags = flags; ++revision; }
	bool is_flag(flags flag) const { return system_utility::is_bitflag(system_flags, 1 << static_cast<UINT>(flag)); }
	void set_flag(flags flag, bool value) { system_utility::set_bitflag(system_flags, 1 << static_cast<UINT>(flag), value); ++revision; }

	//	サンプラー設定
	const texture_sampler_desc& get_sampler_desc() const { return sampler_desc; }
	void set_sampler_desc(const texture_sampler_desc& desc) { sampler_desc = desc; ++revision; }

	//	@tool 描画に関わる内容（ビュー・フラグ・サンプラー設定）を変えた回数
	//	@tool 描画側の複製（マテリアルのプロキシなど）が作り直しの判定に使う。メインスレッドでのみ読み書きする
	UINT get_revision() const { return revision; }

	virtual int get_width() const = 0;	//	テクスチャ幅取得
	virtual int get_height() const = 0;	//	テクスチャ高さ取得
	virtual int get_depth_or_array_size() const = 0;	//	テクスチャ深度/配列数取得

public:
	texture(D3D11_RESOURCE_DIMENSION dimension, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shader_resource_view, Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> unordered_access_view);
	virtual ~texture(){}

	void update_last_write_time();

#if defined(USE_IMGUI)
	bool is_reload();

	void reload();

protected:
	virtual void on_reload() {}
#endif	//	defined(USE_IMGUI)

protected:
	bool is_file;
	std::string file_path_or_key;
	texture_data_types type;
	UINT system_flags;
	D3D11_RESOURCE_DIMENSION dimension;
	//	@tool ビュー一式（再読み込みでは丸ごと差し替える。描画スレッドからも読むので atomic に読み書きする）
	struct view_set
	{
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shader_resource_view;
		Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> unordered_access_view;
	};
	std::atomic<std::shared_ptr<const view_set>> views;
	texture_sampler_desc sampler_desc;
	UINT revision = 0;

#if defined(USE_IMGUI)
	file_time_type last_write_time;
#endif	//	defined(USE_IMGUI)
};

using texture_ptr = std::shared_ptr<texture>;

//	テクスチャ管理クラス
//	☆ﾃｷﾄｰ('ω')ｰﾄｷﾃ☆
class texture_manager
{
	SINGLETON(texture_manager)

	using texture_map = std::unordered_map<std::string, std::weak_ptr<texture>>;

public:
	static constexpr std::string_view white_texture2d_name = "white_texture";
	static constexpr std::string_view black_texture2d_name = "black_texture";

public:
	texture_manager();
	~texture_manager();

	void check_update_and_reload();

	bool generate_texture2d(texture_ptr& out_texture, std::string_view key, D3D11_TEXTURE2D_DESC desc, D3D11_SHADER_RESOURCE_VIEW_DESC* srv_desc = nullptr, D3D11_UNORDERED_ACCESS_VIEW_DESC* uav_desc = nullptr);
	bool generate_texture2d(texture_ptr& out_texture, std::string_view key, D3D11_TEXTURE2D_DESC desc, DWORD init_value, D3D11_SHADER_RESOURCE_VIEW_DESC* srv_desc = nullptr, D3D11_UNORDERED_ACCESS_VIEW_DESC* uav_desc = nullptr);
	bool generate_texture2d(texture_ptr& out_texture, std::string_view key, D3D11_TEXTURE2D_DESC desc, D3D11_SUBRESOURCE_DATA subresource_data, D3D11_SHADER_RESOURCE_VIEW_DESC* srv_desc = nullptr, D3D11_UNORDERED_ACCESS_VIEW_DESC* uav_desc = nullptr);
	//	@tool 名前で登録しないテクスチャを作る（持ち主が破棄を管理する作業用・描画先用）
	bool generate_unregistered_texture2d(texture_ptr& out_texture, D3D11_TEXTURE2D_DESC desc, D3D11_SHADER_RESOURCE_VIEW_DESC* srv_desc = nullptr, D3D11_UNORDERED_ACCESS_VIEW_DESC* uav_desc = nullptr);
	bool load_texture_from_file(texture_ptr& out_texture, std::string_view file_path, bool force_load = false);
	void load_textures_from_files(std::vector<texture_ptr>& out_textures, const std::vector<std::string_view>& file_pathes, bool force_load = false);
	bool load_texture_from_memory(texture_ptr& out_texture, std::string_view key, const void* data, size_t size);

	struct save_texture_desc
	{
		enum class data_formats
		{
			unknown,
			dds,
			jpg,
			png,
			tga,
			hdr,
		};

		enum class dimensions
		{
			_1d,
			_2d,
			_3d,
		};

		enum class miscs
		{
			_cube,
		};

		struct image
		{
			int width = -1;
			int height = -1;
			const UINT8* pixels = nullptr;
		};

		std::string_view file_path;
		DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
		data_formats data_format = data_formats::dds;
		dimensions dimension;
		miscs misc;
		std::vector<image> images;
	};
	bool save_texture(const save_texture_desc& desc);
	bool save_texture(const texture_ptr& texture, bool save_row_texture = false);
	//	@tool GPU のリソースを読み戻して画像ファイルに保存する（immediate context を使うのでメインスレッド専用）
	bool save_resource(ID3D11Resource* resource, std::string_view file_path, save_texture_desc::data_formats data_format);

	texture_ptr	search(std::string_view name);
	texture_map	get_textures() const;

#if defined(USE_IMGUI)
	bool decode_texture_from_file(texture_ptr& out_texture, std::string_view file_path);

	bool convert_texture_to_dds(std::string_view source_file_path, std::string_view dds_file_path, bool generate_mipmaps, UINT max_resolution = 0);

	bool limit_dds_resolution(std::string_view dds_file_path, UINT max_size, bool* out_changed = nullptr);
#endif	//	defined(USE_IMGUI)

private:
	bool load_texture_from_file_inner(texture_ptr& out_texture, std::string_view file_path);

private:
	texture_ptr	white_texture2d;
	texture_ptr	black_texture2d;
	texture_map	textures;
	parallel_for_executor load_parallel_executor;

#if defined(USE_IMGUI)
	std::chrono::steady_clock::time_point last_check_time{};
#endif	//	defined(USE_IMGUI)
};

class texture2d : public texture
{
public:
	//	テクスチャの情報取得
	inline const D3D11_TEXTURE2D_DESC& get_desc() const { return desc; }

	int get_width() const override { return desc.Width; }
	int get_height() const override { return desc.Height; }
	int get_depth_or_array_size() const override { return desc.ArraySize; }

	//	キューブマップ判定
	inline bool is_cube() const { return system_utility::is_bitflag(desc.MiscFlags, D3D11_RESOURCE_MISC_TEXTURECUBE); }

public:
	texture2d(Microsoft::WRL::ComPtr<ID3D11Texture2D> texture, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shader_resource_view, Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> unordered_access_view = nullptr);
	~texture2d() override;

#if defined(USE_IMGUI)
private:
	void on_reload() override;
#endif	//	defined(USE_IMGUI)

private:
	D3D11_TEXTURE2D_DESC	desc;
};

using texture2d_ptr = std::shared_ptr<texture2d>;
