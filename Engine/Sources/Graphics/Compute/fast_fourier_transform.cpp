#include "Graphics/Pipeline/shader.h"
#include "Graphics/Pipeline/global_shader.h"
#include "Texture/texture_factory.h"
#include "Graphics/Compute/fast_fourier_transform.h"

namespace
{
	constexpr std::string_view shader_path = ENGINE_SHADER_PATH("Postprocess/System/fast_fourier_transform_cs.hlsl");
	constexpr std::string_view complex_texture = "Complex Texture ";
	constexpr int texture_sizes[] = 
	{
		512,
		1024,
	};
	int texture_counter = 0;

	//	グローバルシェーダー
	global_shader<compute_shader> global_fft_512_forward_row_cs(shader_path, "main", { shader_define("LENGTH", "512"), shader_define("ROW_PASS", "1") });
	global_shader<compute_shader> global_fft_512_forward_col_cs(shader_path, "main", { shader_define("LENGTH", "512"), shader_define("COL_PASS", "1") });
	global_shader<compute_shader> global_fft_512_inverse_row_cs(shader_path, "main", { shader_define("LENGTH", "512"), shader_define("TRANSFORM_INVERSE", "1"), shader_define("ROW_PASS", "1") });
	global_shader<compute_shader> global_fft_512_inverse_col_cs(shader_path, "main", { shader_define("LENGTH", "512"), shader_define("TRANSFORM_INVERSE", "1"), shader_define("COL_PASS", "1") });
	global_shader<compute_shader> global_fft_1024_forward_row_cs(shader_path, "main", { shader_define("LENGTH", "1024"), shader_define("ROW_PASS", "1") });
	global_shader<compute_shader> global_fft_1024_forward_col_cs(shader_path, "main", { shader_define("LENGTH", "1024"), shader_define("COL_PASS", "1") });
	global_shader<compute_shader> global_fft_1024_inverse_row_cs(shader_path, "main", { shader_define("LENGTH", "1024"), shader_define("TRANSFORM_INVERSE", "1"), shader_define("ROW_PASS", "1") });
	global_shader<compute_shader> global_fft_1024_inverse_col_cs(shader_path, "main", { shader_define("LENGTH", "1024"), shader_define("TRANSFORM_INVERSE", "1"), shader_define("COL_PASS", "1") });
}

fast_fourier_transform::fast_fourier_transform(sizes size)
: texture_size(size)
{
	//	コンピュートシェーダー生成関数
	{
		const bool large = size != sizes::_512;
		auto& forward = cs[static_cast<int>(modes::forward)];
		auto& inverse = cs[static_cast<int>(modes::inverse)];
		forward[static_cast<int>(directions::row)] = (large ? global_fft_1024_forward_row_cs : global_fft_512_forward_row_cs).get();
		forward[static_cast<int>(directions::col)] = (large ? global_fft_1024_forward_col_cs : global_fft_512_forward_col_cs).get();
		inverse[static_cast<int>(directions::row)] = (large ? global_fft_1024_inverse_row_cs : global_fft_512_inverse_row_cs).get();
		inverse[static_cast<int>(directions::col)] = (large ? global_fft_1024_inverse_col_cs : global_fft_512_inverse_col_cs).get();
	}

	// テクスチャ準備
	{
		D3D11_TEXTURE2D_DESC texture2d_desc = {};
		texture2d_desc.Width = texture_sizes[static_cast<int>(size)];
		texture2d_desc.Height = texture_sizes[static_cast<int>(size)];
		texture2d_desc.MipLevels = 1;
		texture2d_desc.ArraySize = 1;
		texture2d_desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		texture2d_desc.SampleDesc.Count = 1;
		texture2d_desc.SampleDesc.Quality = 0;
		texture2d_desc.Usage = D3D11_USAGE_DEFAULT;
		texture2d_desc.CPUAccessFlags = 0;
		texture2d_desc.MiscFlags = 0;
		texture2d_desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
		for (size_t index = 0; index < magic_enum::enum_count<textures>(); ++index)
		{
			std::string name = std::string(complex_texture) + std::to_string(texture_counter++);
			if (!texture_factory::create_texture2d(complex_number_textures[index],
				name, texture2d_desc))
			{
				_ASSERT_EXPR(!"Failed Generate Texture", "complex_texture");
				return;
			}
		}
	}
}

fast_fourier_transform::~fast_fourier_transform()
{
}

void fast_fourier_transform::clear_uavs(ID3D11DeviceContext* immediate_context,
										 math::float4 real_clear_value,
										 math::float4 imaginary_clear_value)
{
	auto* real_uav = complex_number_textures[static_cast<int>(textures::real0)]->get_unordered_access_view().Get();
	auto* imaginary_uav = complex_number_textures[static_cast<int>(textures::imaginary0)]->get_unordered_access_view().Get();
	immediate_context->ClearUnorderedAccessViewFloat(real_uav, &real_clear_value.x);
	immediate_context->ClearUnorderedAccessViewFloat(imaginary_uav, &imaginary_clear_value.x);
	current_texture_index	= 0;
	next_texture_index		= 1;
}

void fast_fourier_transform::solve(ID3D11DeviceContext* immediate_context, modes mode)
{
	solve_core(immediate_context, mode, directions::row);
	solve_core(immediate_context, mode, directions::col);
}

void fast_fourier_transform::solve_core(ID3D11DeviceContext* immediate_context, modes mode, directions direction)
{
	ID3D11ShaderResourceView* setSRV[]	=
	{
		complex_number_textures[static_cast<int>(textures::real0) + 2 * current_texture_index]->get_shader_resource_view().Get(),
		complex_number_textures[static_cast<int>(textures::imaginary0) + 2 * current_texture_index]->get_shader_resource_view().Get(),
	};
	ID3D11UnorderedAccessView* setUAV[]	=
	{
		complex_number_textures[static_cast<int>(textures::real0) + 2 * next_texture_index]->get_unordered_access_view().Get(),
		complex_number_textures[static_cast<int>(textures::imaginary0) + 2 * next_texture_index]->get_unordered_access_view().Get(),
	};
	immediate_context->CSSetShader(cs[static_cast<int>(mode)][static_cast<int>(direction)]->get_shader().Get(), nullptr, 0);
	immediate_context->CSSetShaderResources(0, sizeof(setSRV) / sizeof(setSRV[0]), setSRV);
	immediate_context->CSSetUnorderedAccessViews(0, sizeof(setUAV) / sizeof(setUAV[0]), setUAV, nullptr);

	immediate_context->Dispatch(1, texture_sizes[static_cast<int>(texture_size)], 1);

	ID3D11ShaderResourceView* clear_srv[] = {nullptr, nullptr};
	ID3D11UnorderedAccessView* clear_uav[] = {nullptr, nullptr};
	immediate_context->CSSetShader( nullptr, nullptr, 0 );
	immediate_context->CSSetShaderResources(0, 2, clear_srv);
	immediate_context->CSSetUnorderedAccessViews(0, 2, clear_uav, nullptr);

	int temp = current_texture_index;
	current_texture_index = next_texture_index;
	next_texture_index = temp;
}

const texture_ptr& fast_fourier_transform::get_complex_real() const
{
	return	complex_number_textures[static_cast<int>(textures::real0) + 2 * current_texture_index];
}

const texture_ptr& fast_fourier_transform::get_complex_imaginary() const
{
	return	complex_number_textures[static_cast<int>(textures::imaginary0) + 2 * current_texture_index];
}
