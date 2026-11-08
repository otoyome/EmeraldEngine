#pragma once

#include <vector>
#include <memory>
#include <unordered_map>

#include <d3d11.h>
#include <wrl.h>

#include "Graphics/graphics_context.h"
#include "Graphics/shader.h"

#include "hiz_buffer.h"

class material_instance;

//	メッシュレットカリングシステム
class meshlet_culling_system
{
	//	メッシュレットインスタンス
	struct meshlet_instance_gpu
	{
		UINT vertex_offset = 0;
		UINT triangle_offset = 0;
		UINT triangle_count = 0;
		UINT primitive_index = 0;
		math::float3 bounds_center = {};
		float bounds_radius = 0.0f;
		UINT transform_index = 0;
		UINT no_cull = 0;
		UINT pipeline_group = 0;
		UINT padding = 0;
	};

	struct cull_constants
	{
		math::float4x4 view_projection;
		UINT meshlet_instance_count;
		UINT hiz_mip_count;
		math::float2 hiz_size;
		float depth_bias;
		math::float2 clip_near_far;
		float dummy;
	};

	//	マテリアル単位のリソース
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

public:
	//	メッシュレット描画情報
	struct cull_result
	{
		ID3D11ShaderResourceView* visible_meshlet_srv = nullptr;
		ID3D11Buffer* indirect_args_buffer = nullptr;
	};

	//	メッシュレット生成時のmax_trianglesと一致させる
	//	model_resource_utility.cppのgenerate_meshlets_for_geometry参照
	static constexpr UINT max_meshlet_vertices_per_draw = 124 * 3;

public:
	meshlet_culling_system();
	~meshlet_culling_system();

	//	マテリアル単位でグループ化してカリング
	void build_and_cull(graphics_context& context, const std::vector<std::size_t>& visible_draw_indices, const std::vector<float>& lengths, hiz_buffer& hiz);

	//	build_and_cull()の結果からマテリアルに対応する結果を取得する
	const cull_result* find_result(material_instance* mat) const;

private:
	void generate_meshlet_instance_buffer(material_resources& resource, UINT count);
	void generate_visible_meshlet_buffer(material_resources& resource, UINT count);
	void generate_submesh_transform_buffer(UINT count);

	void cull_material_group(graphics_context& context, material_instance* material, const std::vector<meshlet_instance_gpu>& instances, hiz_buffer& hiz);

private:
	std::shared_ptr<compute_shader> meshlet_culling_cs;

	Microsoft::WRL::ComPtr<ID3D11Buffer> submesh_transform_buffer;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> submesh_transform_srv;
	UINT submesh_transform_capacity = 0;

	Microsoft::WRL::ComPtr<ID3D11Buffer> cull_constant_buffer;

	std::unordered_map<material_instance*, material_resources> material_resource_map;
	std::unordered_map<material_instance*, cull_result> current_results;

	//	スクラッチバッファ
	std::vector<math::float4x4> submesh_transform_scratch;
	std::unordered_map<material_instance*, std::vector<meshlet_instance_gpu>> material_scratch_buffer;
};
