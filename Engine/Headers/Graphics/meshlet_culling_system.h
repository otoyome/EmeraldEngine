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

	//	マテリアル単位のリソース
	struct material_resources
	{
		Microsoft::WRL::ComPtr<ID3D11Buffer> meshlet_instance_buffer;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> meshlet_instance_srv;
		UINT meshlet_instance_capacity = 0;

		Microsoft::WRL::ComPtr<ID3D11Buffer> visible_meshlet_buffer;
		Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> visible_meshlet_uav;
		Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> visible_meshlet_remap_uav;	//	@tool 付け替え用(Append無し)
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> visible_meshlet_srv;
		UINT visible_meshlet_capacity = 0;

		Microsoft::WRL::ComPtr<ID3D11Buffer> indirect_args_buffer;
	};

	struct remap_constants
	{
		UINT meshlet_capacity;
		UINT remap_count;
		UINT dummy[2];
	};

public:
	//	メッシュレット描画情報
	//	@tool 記録スレッドと再生側でバッファが差し替わっても生存するよう参照を保持する
	struct cull_result
	{
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> visible_meshlet_srv;
		Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> visible_meshlet_remap_uav;
		Microsoft::WRL::ComPtr<ID3D11Buffer> indirect_args_buffer;
		UINT visible_meshlet_capacity = 0;
	};
	using cull_result_map = std::unordered_map<material*, cull_result>;

	//	@tool 前フレームの可視リストを今フレームのプリミティブ番号へ付け替えるための情報
	struct remap_snapshot
	{
		cull_result_map results;									//	@tool 前フレームのカリング結果
		std::vector<UINT> remap_table;								//	@tool 前フレームのプリミティブ番号 → 今フレームの番号
		Microsoft::WRL::ComPtr<ID3D11Buffer> remap_buffer;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> remap_srv;
	};

	//	@tool 付け替え先が無いことを示す番号
	static constexpr UINT invalid_primitive_index = 0xFFFFFFFF;

	//	メッシュレット生成時のmax_trianglesと一致させる
	//	model_resource_utility.cppのgenerate_meshlets_for_geometry参照
	static constexpr UINT max_meshlet_vertices_per_draw = 124 * 3;

public:
	meshlet_culling_system();
	~meshlet_culling_system();

	//	@tool 可視サブメッシュをマテリアル単位でグループ化し、GPUバッファを確保する(記録時・メインスレッドで呼ぶ)
	void prepare(const render_data_context& render_data, const std::vector<std::size_t>& visible_draw_indices, const std::vector<float>& lengths, const DirectX::XMFLOAT4& camera_clip_distance);

	//	@tool prepare()で準備した内容でカリング処理を行う(コマンド再生時に呼ぶ)
	void cull(graphics_context& context, hiz_buffer& hiz);

	//	@tool prepare()の結果(マテリアル毎のカリング結果)
	const cull_result_map& get_results() const { return current_results; }

	//	@tool 前フレームの可視リストを付け替える情報を生成する(記録時・メインスレッドで呼ぶ)
	std::shared_ptr<remap_snapshot> create_remap_snapshot(const render_data_context& render_data, const std::vector<std::size_t>& visible_draw_indices, const std::vector<float>& lengths, const DirectX::XMFLOAT4& camera_clip_distance);

	//	@tool create_remap_snapshot()の内容で可視リストのプリミティブ番号を付け替える
	void apply_remap(graphics_context& context, const remap_snapshot& snapshot);

private:
	void generate_meshlet_instance_buffer(material_resources& resource, UINT count);
	void generate_visible_meshlet_buffer(material_resources& resource, UINT count);
	void generate_submesh_transform_buffer(UINT count);

	void cull_material_group(graphics_context& context, material* material, const std::vector<meshlet_instance_gpu>& instances, hiz_buffer& hiz);

	//	@tool カリング対象のサブメッシュを列挙する(prepareと付け替えで同じ条件を使う)
	template<typename Function>
	void for_each_cull_target(const render_data_context& render_data, const std::vector<std::size_t>& visible_draw_indices, const std::vector<float>& lengths, const DirectX::XMFLOAT4& camera_clip_distance, Function function) const;

private:
	std::shared_ptr<compute_shader> meshlet_culling_cs;
	std::shared_ptr<compute_shader> meshlet_remap_cs;
	Microsoft::WRL::ComPtr<ID3D11Buffer> remap_constant_buffer;
	Microsoft::WRL::ComPtr<ID3D11Buffer> remap_buffer;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> remap_srv;
	UINT remap_capacity = 0;

	//	@tool 可視リストが参照しているプリミティブ番号毎のキー(unique_idとLOD番号)
	std::vector<UINT64> result_primitive_keys;

	Microsoft::WRL::ComPtr<ID3D11Buffer> submesh_transform_buffer;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> submesh_transform_srv;
	UINT submesh_transform_capacity = 0;

	Microsoft::WRL::ComPtr<ID3D11Buffer> cull_constant_buffer;

	std::unordered_map<material*, material_resources> material_resource_map;
	std::unordered_map<material*, cull_result> current_results;

	//	スクラッチバッファ
	std::vector<DirectX::XMFLOAT4X4> submesh_transform_scratch;
	std::unordered_map<material*, std::vector<meshlet_instance_gpu>> material_scratch_buffer;
};
