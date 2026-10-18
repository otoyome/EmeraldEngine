#pragma once

#include <d3d11.h>
#include <wrl.h>
#include <vector>
#include <cstdint>
#include <mutex>

#include "System/variable_position_allocator.h"

class render_command_recorder;
#include "Collision/cubic_sdf.h"

class cubic_sdf_buffer_pool
{
public:
	static constexpr uint32_t invalid_id = 0xFFFFFFFF;

	//	GPU側で読むヘッダー情報
	struct sdf_header
	{
		uint32_t voxel_offset = 0;
		uint32_t resolution = 0;
		math::float3 bounds_min = { 0, 0, 0 };
		math::float3 bounds_max = { 0, 0, 0 };
	};

public:
	cubic_sdf_buffer_pool(ID3D11Device* device, UINT voxel_pool_size, UINT header_capacity);
	~cubic_sdf_buffer_pool();

	//	@tool 登録・解除は CPU 側の管理だけを行い、GPU への書き込みは flush_uploads でまとめて行う（どのスレッドから呼んでもよい）
	uint32_t register_sdf(const collision::cubic_sdf& sdf);
	void unregister_sdf(uint32_t sdf_id);

	//	@tool 登録・解除で積んだ書き込みを GPU へ送る（パーティクルの更新の前に呼ぶ）
	void flush_uploads(const render_command_recorder& recorder);

	void set_space_origin(const render_command_recorder& recorder, const math::vector3d& origin);

	ID3D11ShaderResourceView* get_voxel_shader_resource_view() const { return voxel_shader_resource_view.Get(); }
	ID3D11ShaderResourceView* get_header_shader_resource_view() const { return header_shader_resource_view.Get(); }

private:
	Microsoft::WRL::ComPtr<ID3D11Buffer> voxel_buffer;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> voxel_shader_resource_view;
	variable_position_allocator voxel_allocator;

	Microsoft::WRL::ComPtr<ID3D11Buffer> header_buffer;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> header_shader_resource_view;

	std::vector<sdf_header> headers;
	std::vector<math::vector3d> world_bounds_min;
	std::vector<math::vector3d> world_bounds_max;
	math::vector3d space_origin;
	std::vector<uint32_t> free_header_ids;

	//	@tool GPU への書き込み待ち（ヘッダーは書き込む時点の内容を使うので番号だけ覚える）
	struct pending_voxel_write
	{
		D3D11_BOX box;
		std::vector<uint8_t> data;
	};
	std::mutex mutex;
	std::vector<pending_voxel_write> pending_voxel_writes;
	std::vector<uint32_t> dirty_header_ids;
};
