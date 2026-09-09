#pragma once

#include <vector>
#include <unordered_map>
#include <memory>

#include <d3d11.h>
#include <wrl.h>
#include <directxmath.h>

#include "graphics_context.h"
#include "shader.h"

//	オクルージョンカリング
class occlusion_culling_system
{
public:
	occlusion_culling_system();
	~occlusion_culling_system();

	void resize_buffer(int width, int height);

	//	GPU側でカリング処理
	void update(graphics_context& context, float animation_test_distance);

	//	前フレームの判定結果を読み出す
	void read_last_frame(std::unordered_map<UINT, uint8_t>& out_visibility);

private:
	void generate_instance_buffers(UINT count);

private:
	std::shared_ptr<compute_shader> hiz_copy_cs;
	std::shared_ptr<compute_shader> hiz_downsample_cs;
	std::shared_ptr<compute_shader> occlusion_culling_cs;

	Microsoft::WRL::ComPtr<ID3D11Texture2D> hiz_texture;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> hiz_srv;
	std::vector<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> hiz_mip_srvs;
	std::vector<Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView>> hiz_mip_uavs;
	std::vector<DirectX::XMUINT2> hiz_mip_sizes;
	UINT hiz_mip_count = 0;

	Microsoft::WRL::ComPtr<ID3D11Buffer> hiz_constant_buffer;
	Microsoft::WRL::ComPtr<ID3D11Buffer> cull_constant_buffer;

	Microsoft::WRL::ComPtr<ID3D11Buffer> instance_buffer;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> instance_srv;
	Microsoft::WRL::ComPtr<ID3D11Buffer> visibility_buffer;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> visibility_uav;
	Microsoft::WRL::ComPtr<ID3D11Buffer> visibility_staging;
	UINT instance_capacity = 0;

	int last_width = 0;
	int last_height = 0;

	std::vector<UINT> last_unique_ids;
	UINT last_instance_count = 0;
};
