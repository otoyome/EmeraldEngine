#pragma once

#include <d3d11.h>
#include <wrl.h>
#include <vector>
#include <cstdint>

#include "System/variable_position_allocator.h"
#include "Collision/cubic_sdf.h"

class gpu_upload_list;

class cubic_sdf_buffer_pool
{
public:
	static constexpr uint32_t invalid_id = 0xFFFFFFFF;

	//	GPUë§Ç≈ì«ÇﬁÉwÉbÉ_Å[èÓïÒ
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

	uint32_t register_sdf(gpu_upload_list& upload_list, const collision::cubic_sdf& sdf);
	void unregister_sdf(gpu_upload_list& upload_list, uint32_t sdf_id);

	void set_space_origin(ID3D11DeviceContext* device_context, const math::vector3d& origin);

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
};
