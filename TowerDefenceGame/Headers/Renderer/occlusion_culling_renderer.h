#pragma once

#include <vector>
#include <unordered_map>

#include "renderer.h"
#include "scene_render_targets.h"
#include "shader.h"

//	オクルージョンカリング
class occlusion_culling_renderer : public renderer
{
public:
	renderer_domain get_domain() const override { return renderer_domain::occlusion_culling_pass; }

public:
	occlusion_culling_renderer(render_job_registrar job_executor, std::shared_ptr<scene_render_targets> render_targets);
	~occlusion_culling_renderer() override;

	void render(renderer_context& context) override;
	void resize_buffer(int width, int height) override;

	//	前フレームの判定結果を読み出す
	void read_last_frame(std::unordered_map<UINT, uint8_t>& out_visibility);

private:
	void generate_instance_buffers(UINT count);

private:
	std::shared_ptr<scene_render_targets> render_targets;

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

	std::vector<UINT> last_unique_ids;
	UINT last_instance_count = 0;
};
