#pragma once

#include <vector>
#include <unordered_map>
#include <memory>
#include <string>

#include "buffer_view.h"
#include "System/variable_position_allocator.h"

//	GPUバッファの実体とアップロードキューだけを持つクラス
class model_gpu_buffer
{
public:
	model_gpu_buffer(UINT bind_flag, UINT capacity_in_bytes, UINT stride_in_bytes = 0);
	~model_gpu_buffer();

	void queue_upload(UINT offset_in_bytes, const void* data, UINT size_in_bytes);
	void flush_uploads();

	void grow(UINT new_capacity_in_bytes);

	ID3D11Buffer* get_buffer() const { return buffer.Get(); }
	ID3D11ShaderResourceView* get_srv() const { return srv.Get(); }
	UINT get_capacity_in_bytes() const { return capacity_in_bytes; }
	UINT get_stride_in_bytes() const { return stride_in_bytes; }

private:
	void create_srv();

	//	アップロードバッファ
	struct pending_upload
	{
		UINT offset_in_bytes;
		std::vector<uint8_t> data;
	};

	Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
	std::vector<pending_upload> pending_uploads;
	UINT capacity_in_bytes = 0;
	UINT stride_in_bytes = 0;
	UINT bind_flag = 0;
};

//	モデルの頂点/インデックスバッファ管理クラス
class model_buffer_pool_manager
{
	SINGLETON(model_buffer_pool_manager)

	//	プロファイラー表示用の使用量情報
	struct pool_usage
	{
		std::string label;
		UINT used_in_bytes;
		UINT capacity_in_bytes;
	};

private:
	model_buffer_pool_manager();
	~model_buffer_pool_manager();

public:
	UINT allocate_vertex_slots(UINT vertex_count);
	void free_vertex_slots(UINT base_vertex);

	void queue_vertex_upload(const semantic& target_semantic, UINT byte_offset, const void* data, UINT size_in_bytes, UINT stride_in_bytes);
	ID3D11Buffer* get_vertex_buffer(const semantic& target_semantic);
	ID3D11ShaderResourceView* get_vertex_buffer_srv(const semantic& target_semantic);

	//	メッシュレット用頂点・三角形バッファ構築
	void build_meshlet_data(const std::vector<UINT>& meshlet_vertices, const std::vector<uint8_t>& meshlet_triangles, UINT& out_vertex_base, UINT& out_triangle_base);
	void free_meshlet_data(UINT vertex_base, UINT vertex_count, UINT triangle_base, UINT triangle_count);

	ID3D11ShaderResourceView* get_meshlet_vertex_buffer_srv();
	ID3D11ShaderResourceView* get_meshlet_triangle_buffer_srv();
	ID3D11Buffer* get_meshlet_draw_constant_buffer();

	void flush_uploads();

	std::vector<pool_usage> get_pool_usages() const;

	void build_vertex_buffers(std::unordered_map<semantic, buffer_view>& vertex_buffer_views);
	void free_vertex_buffers(const std::unordered_map<semantic, buffer_view>& vertex_buffer_views);

private:
	model_gpu_buffer& get_vertex_buffer_pool(const semantic& target_semantic, UINT stride_in_bytes);

	void grow_vertex_capacity(UINT additional_vertex_count);
	UINT allocate_meshlet_vertices(UINT count);
	void free_meshlet_vertices(UINT base);
	UINT allocate_meshlet_triangles(UINT count);
	void free_meshlet_triangles(UINT base);
	void grow_meshlet_vertex_capacity(UINT additional_count);
	void grow_meshlet_triangle_capacity(UINT additional_count);

	variable_position_allocator vertex_slot_allocator;
	std::unordered_map<semantic, std::unique_ptr<model_gpu_buffer>> vertex_buffers;
	UINT vertex_slot_capacity_current = 0;

	variable_position_allocator meshlet_vertex_allocator;
	std::unique_ptr<model_gpu_buffer> meshlet_vertex_buffer;
	UINT meshlet_vertex_capacity_current = 0;

	variable_position_allocator meshlet_triangle_allocator;
	std::unique_ptr<model_gpu_buffer> meshlet_triangle_buffer;
	UINT meshlet_triangle_capacity_current = 0;

	Microsoft::WRL::ComPtr<ID3D11Buffer> meshlet_draw_constant_buffer;
};
