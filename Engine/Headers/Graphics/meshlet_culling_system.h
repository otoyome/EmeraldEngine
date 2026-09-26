#pragma once

#include <vector>
#include <memory>

#include <d3d11.h>
#include <wrl.h>
#include <directxmath.h>

#include "graphics_context.h"
#include "shader.h"

#include "hiz_buffer.h"

//	メッシュレットカリングシステム
class meshlet_culling_system
{
public:
	meshlet_culling_system();
	~meshlet_culling_system();

	//	サブメッシュからメッシュレットインスタンスを構築
	void build(graphics_context& context, const std::vector<std::size_t>& visible_draw_indices, const std::vector<float>& lengths);

	//	メッシュレット単位でカリングして描画コール数を計算
	void cull(graphics_context& context, hiz_buffer& hiz);

	//	判定結果をコピー
	void copy_visible_count_to(graphics_context& context, ID3D11Buffer* dest_buffer, UINT dest_offset);

	UINT get_meshlet_instance_count() const { return meshlet_instance_count; }
	ID3D11ShaderResourceView* get_visible_meshlet_srv() const { return visible_meshlet_srv.Get(); }

private:
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

	void generate_meshlet_instance_buffer(UINT count);
	void generate_submesh_transform_buffer(UINT count);
	void generate_visible_meshlet_buffer(UINT count);

private:
	std::shared_ptr<compute_shader> meshlet_culling_cs;

	Microsoft::WRL::ComPtr<ID3D11Buffer> meshlet_instance_buffer;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> meshlet_instance_srv;
	UINT meshlet_instance_capacity = 0;
	UINT meshlet_instance_count = 0;

	Microsoft::WRL::ComPtr<ID3D11Buffer> submesh_transform_buffer;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> submesh_transform_srv;
	UINT submesh_transform_capacity = 0;

	Microsoft::WRL::ComPtr<ID3D11Buffer> visible_meshlet_buffer;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> visible_meshlet_uav;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> visible_meshlet_srv;
	UINT visible_meshlet_capacity = 0;

	Microsoft::WRL::ComPtr<ID3D11Buffer> cull_constant_buffer;

	std::vector<meshlet_instance_gpu> meshlet_instance_scratch;
	std::vector<DirectX::XMFLOAT4X4> submesh_transform_scratch;
};
