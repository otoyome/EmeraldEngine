#pragma once

#include <vector>
#include <unordered_map>
#include <memory>
#include <string>

#include "buffer_view.h"
#include "System/variable_position_allocator.h"

//	頂点/インデックスバッファ構造体
class model_buffer_pool
{
public:
	model_buffer_pool(UINT bind_flag, UINT capacity_in_bytes);
	~model_buffer_pool();

	UINT allocate(UINT size_in_bytes);
	void free(UINT offset_in_bytes, UINT size_in_bytes);
	void queue_upload(UINT offset_in_bytes, const void* data, UINT size_in_bytes);
	void flush_uploads();

	ID3D11Buffer* get_buffer() const { return buffer.Get(); }
	UINT get_used_in_bytes() const { return used_in_bytes; }
	UINT get_capacity_in_bytes() const { return capacity_in_bytes; }

private:
	struct pending_upload
	{
		UINT offset_in_bytes;
		std::vector<BYTE> data;
	};

	variable_position_allocator allocator;
	Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
	std::vector<pending_upload> pending_uploads;
	UINT capacity_in_bytes = 0;
	UINT used_in_bytes = 0;
};

class model_buffer_pool_manager
{
	SINGLETON(model_buffer_pool_manager)

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
	UINT allocate_vertex(const semantic& target_semantic, UINT size_in_bytes);
	void free_vertex(const semantic& target_semantic, UINT offset_in_bytes, UINT size_in_bytes);
	void queue_vertex_upload(const semantic& target_semantic, UINT offset_in_bytes, const void* data, UINT size_in_bytes);
	ID3D11Buffer* get_vertex_buffer(const semantic& target_semantic);

	UINT allocate_index(UINT size_in_bytes);
	void free_index(UINT offset_in_bytes, UINT size_in_bytes);
	void queue_index_upload(UINT offset_in_bytes, const void* data, UINT size_in_bytes);
	ID3D11Buffer* get_index_buffer();

	void flush_uploads();

	std::vector<pool_usage> get_pool_usages() const;

private:
	model_buffer_pool& get_vertex_pool(const semantic& target_semantic);

	std::unordered_map<semantic, std::unique_ptr<model_buffer_pool>> vertex_pools;
	std::unique_ptr<model_buffer_pool> index_pool;
};
