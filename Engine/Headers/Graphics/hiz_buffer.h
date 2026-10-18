#pragma once

#include <vector>
#include <memory>

#include <d3d11.h>
#include <wrl.h>

#include "render_job_context.h"
#include "shader.h"

//	äKëwê[ìxÉoÉbÉtÉ@
class hiz_buffer
{
public:
	hiz_buffer();
	~hiz_buffer();

	void resize_buffer(int width, int height);

	void build(render_job_context& context);

	ID3D11ShaderResourceView* get_srv() const { return hiz_srv.Get(); }
	UINT get_mip_count() const { return hiz_mip_count; }
	math::uint2 get_size() const { return hiz_mip_sizes.empty() ? math::uint2{ 0, 0 } : hiz_mip_sizes[0]; }

private:
	std::shared_ptr<compute_shader> hiz_copy_cs;
	std::shared_ptr<compute_shader> hiz_downsample_cs;

	Microsoft::WRL::ComPtr<ID3D11Texture2D> hiz_texture;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> hiz_srv;
	std::vector<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> hiz_mip_srvs;
	std::vector<Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView>> hiz_mip_uavs;
	std::vector<math::uint2> hiz_mip_sizes;
	UINT hiz_mip_count = 0;

	Microsoft::WRL::ComPtr<ID3D11Buffer> hiz_constant_buffer;

	int last_width = 0;
	int last_height = 0;
};
