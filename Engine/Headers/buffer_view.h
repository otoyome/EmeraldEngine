#pragma once

#include <span>

//	バッファビュー
struct buffer_view
{
	DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
	Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
	std::vector<BYTE> row_data;
	size_t stride_in_bytes = 0;
	size_t size_in_bytes = 0;
	size_t offset_in_bytes = 0;
	size_t count() const
	{
		return size_in_bytes / stride_in_bytes;
	}
	size_t offset() const
	{
		return offset_in_bytes / stride_in_bytes;
	}
	buffer_view() = default;
	~buffer_view() {}

	template<class Archive>
	void serialize(Archive& archive, int version)
	{
		archive(CEREAL_NVP(format),
				CEREAL_NVP(row_data),
				CEREAL_NVP(stride_in_bytes),
				CEREAL_NVP(size_in_bytes),
				CEREAL_NVP(offset_in_bytes));
	}
};

//	識別子
class semantic
{
public:
	semantic() {}
	semantic(std::string_view name, UINT index)
	: name(name), index(index)
	{
		build();
	}
	bool operator==(const semantic& l) const
	{
		assert(hash != -1);
		assert(l.hash != -1);
		return hash == l.hash;
	}

	template<class Archive>
	void serialize(Archive& archive, int version)
	{
		archive(CEREAL_NVP(name), CEREAL_NVP(index));
		dirty = true;
		build();	//	とりあえずここで
	}

	void build()
	{
		if (!dirty)
			return;

		dirty = false;
		hash = system_utility::generate_hash(name) + index;
	}

	system_utility::hash_key get_hash_key() const
	{
		assert(hash != -1);
		return hash;
	}
	std::string_view get_name() const {return name;}
	UINT get_index() const {return index;}

private:
	bool dirty = true;
	std::string name;
	UINT index;
	system_utility::hash_key hash = -1;
};

//	メッシュレット情報
struct pooled_meshlet
{
	UINT vertex_offset = 0;
	UINT triangle_offset = 0;
	UINT triangle_count = 0;
	math::float3 bounds_center = { 0, 0, 0 };
	float bounds_radius = 0.0f;
};

class material_render_proxy;

//	描画バッファ情報
struct mesh_draw_item
{
	enum class flags
	{
		culling = 1 << 0,		//	カリングを行うかどうか
		static_mesh = 1 << 1,	//	アニメーションしないモデルかどうか
	};
	struct morph_mesh
	{
		const std::unordered_map<semantic, buffer_view>* vertex_buffer_views;
		float weight = 0;
	};
	struct sub_mesh
	{
		const std::unordered_map<semantic, buffer_view>* vertex_buffer_views;
		D3D_PRIMITIVE_TOPOLOGY primitive_topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		const std::vector<pooled_meshlet>* meshlets = nullptr;
	};

	UINT vertex_count;										//	Drawで使用する際の頂点数
	UINT system_flags = 0;									//	モデルフラグ類
	std::span<const sub_mesh> color_meshs;					//	色描画メッシュ
	std::vector<morph_mesh> morph_meshes;					//	モーフ用メッシュ
	sub_mesh shadow_mesh;									//	影用描画メッシュ
	math::float2 lod_distance_range = {0.0f, 10000.0f};		//	LOD距離範囲
	UINT unique_id = 0xFFFFFFFF;							//	ユニークID
	math::float3 bounds_min, bounds_max;					//	AABB範囲
	math::float4x4 world_transform = {};					//	ワールド変換行列
	Microsoft::WRL::ComPtr<ID3D11Buffer> primitive_buffer;	//	プリミティブバッファ
	std::shared_ptr<const material_render_proxy> material;		//	@tool マテリアル

	//	LOD選択
	std::size_t select_lod_mesh_index(float mesh_length) const
	{
		std::size_t lod_count = color_meshs.size();
		std::size_t lod_mesh_index = 0;
		if (lod_count > 1)
		{
			float lod_range_min = lod_distance_range.x;
			float lod_range_max = lod_distance_range.y;
			float lod_range = lod_range_max - lod_range_min;
			float lod_rate = 0.0f;
			if (lod_range > 1e-6f)
			{
				lod_rate = (mesh_length - lod_range_min) / lod_range;
				if (lod_rate < 0.0f) lod_rate = 0.0f;
				if (lod_rate > 1.0f) lod_rate = 1.0f;
			}
			lod_mesh_index = static_cast<std::size_t>(lod_rate * static_cast<float>(lod_count - 1) + 0.5f);
			if (lod_mesh_index >= lod_count)
				lod_mesh_index = lod_count - 1;
		}
		return lod_mesh_index;
	}
};


namespace std
{
    template <>
    struct hash<semantic>
	{
		std::size_t operator()(const semantic& item) const
		{
			return item.get_hash_key();
		}
	};
}	//	namespace std
