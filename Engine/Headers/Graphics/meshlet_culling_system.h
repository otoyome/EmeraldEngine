#pragma once

#include <vector>
#include <memory>
#include <unordered_map>

#include <d3d11.h>
#include <wrl.h>
#include <directxmath.h>

#include "graphics_context.h"
#include "shader.h"

#include "hiz_buffer.h"

class material;

//	メッシュレット単位カリングシステム
//	可視サブメッシュをマテリアル単位でグループ化し、マテリアルごとに独立したGPUリソースでカリング結果を保持する。
//	occlusion_culling_pass(オクルージョンカリングと同じタイミング)で1フレームに1度build_and_cull()を呼び、
//	以降の各描画パス(深度プリパス/G-Bufferパス等)はfind_result()で結果を参照して間接描画するだけにする
class meshlet_culling_system
{
public:
	//	生存メッシュレットの描画に必要な情報
	struct cull_result
	{
		ID3D11ShaderResourceView* visible_meshlet_srv = nullptr;
		ID3D11Buffer* indirect_args_buffer = nullptr;
	};

	//	メッシュレット生成時のmax_trianglesと一致させる(model_resource_utility.cppのgenerate_meshlets_for_geometry参照)
	static constexpr UINT max_meshlet_vertices_per_draw = 124 * 3;

public:
	meshlet_culling_system();
	~meshlet_culling_system();

	//	可視サブメッシュ一覧(draw_datasへのインデックス)をマテリアル単位でグループ化し、
	//	メッシュレット単位のフラスタム+Hi-Zオクルージョンカリングを実行して結果を保持する
	//	lengths : buffer_datasと同じ並びの、カメラ近平面からの距離(LOD選択に使用)
	void build_and_cull(graphics_context& context, const std::vector<std::size_t>& visible_draw_indices, const std::vector<float>& lengths, hiz_buffer& hiz);

	//	build_and_cull()の結果からマテリアルに対応する結果を取得する(その回で生存メッシュレットが無ければnullptr)
	const cull_result* find_result(material* mat) const;

private:
	//	メッシュレットインスタンス1件分(HLSL側の構造体とレイアウトを一致させる)
	struct meshlet_instance_gpu
	{
		UINT vertex_offset = 0;
		UINT triangle_offset = 0;
		UINT triangle_count = 0;
		UINT primitive_index = 0;
		DirectX::XMFLOAT3 bounds_center = {};
		float bounds_radius = 0.0f;
		UINT transform_index = 0;
		UINT no_cull = 0;
		UINT pipeline_group = 0;
		UINT padding = 0;
	};

	struct cull_constants
	{
		DirectX::XMFLOAT4X4 view_projection;
		UINT meshlet_instance_count;
		UINT hiz_mip_count;
		DirectX::XMFLOAT2 hiz_size;
		float depth_bias;
		DirectX::XMFLOAT2 clip_near_far;
		float dummy;
	};

	//	マテリアル単位で保持するGPUリソース(フレームをまたいで使い回し、容量が足りなくなったら成長させる)
	struct material_resources
	{
		Microsoft::WRL::ComPtr<ID3D11Buffer> meshlet_instance_buffer;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> meshlet_instance_srv;
		UINT meshlet_instance_capacity = 0;

		Microsoft::WRL::ComPtr<ID3D11Buffer> visible_meshlet_buffer;
		Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> visible_meshlet_uav;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> visible_meshlet_srv;
		UINT visible_meshlet_capacity = 0;

		Microsoft::WRL::ComPtr<ID3D11Buffer> indirect_args_buffer;
	};

	material_resources& get_or_create_resources(material* mat);
	void generate_meshlet_instance_buffer(material_resources& res, UINT count);
	void generate_visible_meshlet_buffer(material_resources& res, UINT count);
	void generate_submesh_transform_buffer(UINT count);

	void cull_material_group(graphics_context& context, material* mat, const std::vector<meshlet_instance_gpu>& instances, hiz_buffer& hiz);

private:
	std::shared_ptr<compute_shader> meshlet_culling_cs;

	Microsoft::WRL::ComPtr<ID3D11Buffer> submesh_transform_buffer;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> submesh_transform_srv;
	UINT submesh_transform_capacity = 0;

	Microsoft::WRL::ComPtr<ID3D11Buffer> cull_constant_buffer;

	//	マテリアルごとに永続化するGPUリソース(フレームをまたいで保持)
	std::unordered_map<material*, material_resources> per_material_resources;

	//	直近のbuild_and_cull()の結果(毎回作り直す)
	std::unordered_map<material*, cull_result> current_results;

	//	毎フレーム使い回すスクラッチ領域
	std::vector<DirectX::XMFLOAT4X4> submesh_transform_scratch;
	std::unordered_map<material*, std::vector<meshlet_instance_gpu>> per_material_scratch;
};
