#pragma once

#include <array>
#include <string>
#include <string_view>

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

//	テクスチャ
//	☆ﾃｷﾄｰ('ω')ｰﾄｷﾃ☆
class texture
{
public:
	enum class flags
	{
		srgb,			//	SRGB

		gamut_rec709,	//	Rec.709
		gamut_rec2020,	//	Rec.2020

		cube_map,	//	キューブマップ
	};

public:
	D3D11_RESOURCE_DIMENSION	get_dimension() const;
	const Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& get_shader_resource_view() const;
	const Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView>& get_unordered_access_view() const;

#if defined(USE_IMGUI)
	std::string_view get_debug_name() const { return debug_name; }
	void set_debug_name(std::string_view name) { debug_name = name; }
#endif	//	defined(USE_IMGUI)

	UINT get_system_flags() const { return system_flags; }
	void set_system_flags(UINT flags) { system_flags = flags; }
	bool is_flag(flags flag) const { return core::is_bitflag(system_flags, 1 << static_cast<UINT>(flag)); }
	void set_flag(flags flag, bool value) { core::set_bitflag(system_flags, 1 << static_cast<UINT>(flag), value); }

	//	サンプラー設定
	const texture_sampler_desc& get_sampler_desc() const { return sampler_desc; }
	void set_sampler_desc(const texture_sampler_desc& desc) { sampler_desc = desc; }

	virtual int get_width() const = 0;	//	テクスチャ幅取得
	virtual int get_height() const = 0;	//	テクスチャ高さ取得
	virtual int get_depth_or_array_size() const = 0;	//	テクスチャ深度/配列数取得

public:
	texture(D3D11_RESOURCE_DIMENSION dimension, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shader_resource_view, Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> unordered_access_view);
	virtual ~texture(){}

protected:
#if defined(USE_IMGUI)
	std::string debug_name;
#endif	//	defined(USE_IMGUI)
	UINT system_flags;
	D3D11_RESOURCE_DIMENSION dimension;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shader_resource_view;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> unordered_access_view;
	texture_sampler_desc sampler_desc;
};

using texture_ptr = std::shared_ptr<texture>;

class texture2d : public texture
{
public:
	//	テクスチャの情報取得
	inline const D3D11_TEXTURE2D_DESC& get_desc() const { return desc; }

	int get_width() const override { return desc.Width; }
	int get_height() const override { return desc.Height; }
	int get_depth_or_array_size() const override { return desc.ArraySize; }

	//	キューブマップ判定
	inline bool is_cube() const { return core::is_bitflag(desc.MiscFlags, D3D11_RESOURCE_MISC_TEXTURECUBE); }

public:
	texture2d(Microsoft::WRL::ComPtr<ID3D11Texture2D> texture, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shader_resource_view, Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> unordered_access_view = nullptr);
	~texture2d() override;

private:
	D3D11_TEXTURE2D_DESC	desc;
};

using texture2d_ptr = std::shared_ptr<texture2d>;
