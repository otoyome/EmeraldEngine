#pragma once

#include <vector>
#include <unordered_map>
#include <memory>
#include <string>
#include <mutex>

#include "Graphics/Core/buffer_view.h"
#include "Graphics/Pipeline/common_pipeline_states.h"
#include "Core/variable_position_allocator.h"

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

	//	属性毎の頂点プール
	struct vertex_pool
	{
		std::unique_ptr<model_gpu_buffer> buffer;
		core::variable_position_allocator allocator;
		UINT capacity = 0;
	};

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
	//	メッシュレットで読む頂点属性の番号を取得
	static common_pipeline_states::vertex_elements find_vertex_element(const semantic& target_semantic);

	//	頂点バッファ
	void build_vertex_buffers(std::unordered_map<semantic, buffer_view>& vertex_buffer_views);
	void free_vertex_buffers(const std::unordered_map<semantic, buffer_view>& vertex_buffer_views);

	ID3D11Buffer* get_vertex_buffer(const semantic& target_semantic);
	ID3D11ShaderResourceView* get_vertex_buffer_srv(const semantic& target_semantic);

public:
	//	メッシュレット用頂点・三角形バッファ構築
	void build_meshlet_data(const std::vector<UINT>& meshlet_vertices, const std::vector<uint8_t>& meshlet_triangles, UINT& out_vertex_base, UINT& out_triangle_base);
	void free_meshlet_data(UINT vertex_base, UINT vertex_count, UINT triangle_base, UINT triangle_count);

	ID3D11ShaderResourceView* get_meshlet_vertex_buffer_srv();
	ID3D11ShaderResourceView* get_meshlet_triangle_buffer_srv();
	void get_meshlet_instance_batch(UINT min_capacity, ID3D11Buffer** out_buffer, ID3D11ShaderResourceView** out_srv);

public:
	void flush_uploads();

	std::vector<pool_usage> get_pool_usages() const;

private:
	vertex_pool& get_vertex_pool(const semantic& target_semantic, UINT stride_in_bytes);
	UINT allocate_vertices(vertex_pool& pool, UINT vertex_count);

	UINT allocate_meshlet_vertices(UINT count);
	void free_meshlet_vertices(UINT base);
	UINT allocate_meshlet_triangles(UINT count);
	void free_meshlet_triangles(UINT base);
	void grow_meshlet_vertex_capacity(UINT additional_count);
	void grow_meshlet_triangle_capacity(UINT additional_count);

	//	頂点バッファ
	std::unordered_map<semantic, vertex_pool> vertex_buffers;

	//	メッシュレットバッファ
	core::variable_position_allocator meshlet_vertex_allocator;
	std::unique_ptr<model_gpu_buffer> meshlet_vertex_buffer;
	UINT meshlet_vertex_capacity_current = 0;

	//	三角形バッファ
	core::variable_position_allocator meshlet_triangle_allocator;
	std::unique_ptr<model_gpu_buffer> meshlet_triangle_buffer;
	UINT meshlet_triangle_capacity_current = 0;

	//	シングルオブジェクト・メッシュレット用バッファ
	std::mutex meshlet_instance_batch_mutex;
	Microsoft::WRL::ComPtr<ID3D11Buffer> meshlet_instance_batch_buffer;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> meshlet_instance_batch_srv;
	UINT meshlet_instance_batch_capacity = 0;
};
