#pragma once

#include <d3d11.h>
#include <wrl.h>
#include <DirectXMath.h>
#include <vector>
#include <cstdint>

#include "System/variable_position_allocator.h"
#include "Collision/cubic_sdf.h"

namespace collision
{
	//	@tool 複数のcubic_sdfのボクセルデータを1つの巨大なStructuredBufferにまとめて保持し、
	//	@tool コンピュートシェーダー側から共通のバッファ+IDでサンプリングできるようにする管理クラス
	//	@tool (compute_particle_systemのような単一プールの管理クラスに専用バッファを持たせるのではなく、
	//	@tool  複数のSDFを同時に扱えるよう独立したクラスにしている)
	class cubic_sdf_gpu_resource
	{
	public:
		static constexpr uint32_t invalid_id = 0xFFFFFFFF;

		//	GPU側で読むヘッダー情報(HLSL側のsdf_header構造体と1:1で対応させる)
		struct sdf_header
		{
			uint32_t voxel_offset = 0;		//	voxelバッファ内の要素オフセット
			uint32_t resolution = 0;		//	0のとき未使用スロット
			DirectX::XMFLOAT3 bounds_min = { 0, 0, 0 };
			DirectX::XMFLOAT3 bounds_max = { 0, 0, 0 };
		};

	public:
		//	voxel_pool_size : voxelバッファ全体の要素数(float単位)
		//	header_capacity : 同時に登録できるcubic_sdfの最大数
		bool initialize(ID3D11Device* device, UINT voxel_pool_size, UINT header_capacity);
		void uninitialize();

		//	cubic_sdfを登録してGPUバッファへアップロードする。戻り値はsdf_id(失敗時はinvalid_id)
		uint32_t register_sdf(ID3D11DeviceContext* device_context, const cubic_sdf& sdf);
		//	登録済みのsdf_idを解放する(voxel領域とheaderスロットを再利用可能にする)
		void unregister_sdf(ID3D11DeviceContext* device_context, uint32_t sdf_id);

		ID3D11ShaderResourceView* get_voxel_shader_resource_view() const { return voxel_shader_resource_view.Get(); }
		ID3D11ShaderResourceView* get_header_shader_resource_view() const { return header_shader_resource_view.Get(); }

	private:
		Microsoft::WRL::ComPtr<ID3D11Buffer> voxel_buffer;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> voxel_shader_resource_view;
		variable_position_allocator voxel_allocator;

		Microsoft::WRL::ComPtr<ID3D11Buffer> header_buffer;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> header_shader_resource_view;
		std::vector<sdf_header> headers;			//	CPU側ミラー(headers[i].resolution == 0 のとき未使用)
		std::vector<uint32_t> free_header_ids;		//	再利用可能なheaderインデックス
	};

}	//	namespace collision
