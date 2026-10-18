#pragma once

#include <vector>
#include <unordered_map>
#include <memory>
#include <mutex>

#include <d3d11.h>
#include <wrl.h>

#include "render_job_context.h"
#include "Graphics/gpu_readback.h"
#include "shader.h"

#include "hiz_buffer.h"

//	オクルージョンカリング
class occlusion_culling_system
{
public:
	occlusion_culling_system();
	~occlusion_culling_system();

	//	GPU上でカリング判定
	void update(render_job_context& context, hiz_buffer& hiz);

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
	//	@tool 判定結果の読み戻し（受け取り関数は提出スレッドで呼ばれる）
	struct visibility_result
	{
		std::mutex mutex;
		std::vector<std::pair<UINT, uint8_t>> visibility;	//	@tool インスタンス番号と可視判定
	};
	gpu_readback_buffer visibility_readback;
	std::shared_ptr<visibility_result> received_visibility = std::make_shared<visibility_result>();
	UINT instance_capacity = 0;

	std::vector<UINT> last_unique_ids;
	UINT last_instance_count = 0;
};
