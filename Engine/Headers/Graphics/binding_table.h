#pragma once

#include "texture.h"
#include "Graphics/immediate_constant_allocator.h"

//	割り当てレイヤー
enum class resource_view_bind_layer : uint8_t
{
	frame_view,	//	フレーム・視点
	pass,		//	描画パス

	max,
};

//	テクスチャ・SRV を割り当てる際に必要な情報
struct texture_bind
{
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shader_resource_view;	//	リソース本体
	Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler_state;				//	サンプラー設定
	UINT shader_resource_view_flags = 0;									//	テクスチャフラグ
	bool is_texture = false;												//	テクスチャから作ったか

	static texture_bind from_texture(const texture& value);
};

//	リソースバインドレコード
struct resource_view_bind_entry
{
	enum class kinds : uint8_t
	{
		texture,				//	テクスチャ（フラグ・サンプラー設定も参照する）
		shader_resource_view,	//	テクスチャ以外のSRV
		constant_buffer,		//	定数バッファ全体
		constant_data,			//	割り当て時に揮発性定数バッファへ書き込む
	};

	system_utility::hash_key key = 0;
	kinds kind = kinds::texture;

	texture_ptr texture;
	texture_bind view;

	Microsoft::WRL::ComPtr<ID3D11Buffer> constant_buffer;
	std::vector<std::byte> constant_data;
	mutable constant_range uploaded_range;	//	定数バッファの転送先

	bool requires_draw_data = false;	//	描画バッファ単位の情報がある描画にだけ割り当てる
};

//	リソースバインドテーブル
class resource_view_bind_table
{
public:
	void set_texture(resource_view_bind_layer layer, system_utility::hash_key key, const texture_ptr& value);
	void set_shader_resource_view(resource_view_bind_layer layer, system_utility::hash_key key, ID3D11ShaderResourceView* value);
	void set_constant_buffer(resource_view_bind_layer layer, system_utility::hash_key key, const Microsoft::WRL::ComPtr<ID3D11Buffer>& value);
	void set_constants(resource_view_bind_layer layer, system_utility::hash_key key, const void* data, UINT size, bool requires_draw_data = false);
	template<typename T>
	void set_constants(resource_view_bind_layer layer, system_utility::hash_key key, const T& value, bool requires_draw_data = false)
	{
		set_constants(layer, key, &value, static_cast<UINT>(sizeof(T)), requires_draw_data);
	}

	UINT get_revision() const { return revision; }
	texture_ptr get_texture(system_utility::hash_key key) const;
	template<typename T>
	const T* get_constants(system_utility::hash_key key) const
	{
		const resource_view_bind_entry* entry = find(key);
		if (!entry || entry->kind != resource_view_bind_entry::kinds::constant_data || entry->constant_data.size() != sizeof(T))
			return nullptr;
		return reinterpret_cast<const T*>(entry->constant_data.data());
	}

public:
	void remove(resource_view_bind_layer layer, system_utility::hash_key key);
	void clear(resource_view_bind_layer layer);

	const resource_view_bind_entry* find(system_utility::hash_key key) const;


private:
	resource_view_bind_entry& acquire(resource_view_bind_layer layer, system_utility::hash_key key);

private:
	std::vector<resource_view_bind_entry> layers[static_cast<int>(resource_view_bind_layer::max)];
	UINT revision = 0;
};
