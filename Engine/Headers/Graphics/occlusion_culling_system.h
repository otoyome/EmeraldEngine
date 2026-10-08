#pragma once

#include <vector>
#include <unordered_map>
#include <memory>

#include <d3d11.h>
#include <wrl.h>

#include "graphics_context.h"
#include "shader.h"

#include "hiz_buffer.h"

//	オクルージョンカリング
class occlusion_culling_system
{
public:
	occlusion_culling_system();
	~occlusion_culling_system();

	//	GPU上でカリング判定
	void update(graphics_context& context, hiz_buffer& hiz);

	//	前フレームの判定結果取得
	void read_last_frame(std::unordered_map<UINT, uint8_t>& out_visibility);

private:
	void generate_instance_buffers(UINT count);

private:
	std::shared_ptr<compute_shader> occlusion_culling_cs;

	Microsoft::WRL::ComPtr<ID3D11Buffer> cull_constant_buffer;

	Microsoft::WRL::ComPtr<ID3D11Buffer> instance_buffer;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> instance_srv;
	Microsoft::WRL::ComPtr<ID3D11Buffer> visibility_buffer;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> visibility_uav;
	Microsoft::WRL::ComPtr<ID3D11Buffer> visibility_staging;
	UINT instance_capacity = 0;

	std::vector<UINT> last_unique_ids;
	UINT last_instance_count = 0;
};
