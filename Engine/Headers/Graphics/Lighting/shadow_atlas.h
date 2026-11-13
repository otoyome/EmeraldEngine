#pragma once

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

#include <d3d11.h>
#include <wrl.h>

#include "Texture/texture.h"

//	シャドウマップアトラス
class shadow_atlas
{
public:
	//	統計
	struct statistics
	{
		UINT allocated_lights = 0;	//	今回アトラスを使ったライト数
		UINT rendered_lights = 0;	//	今回影を描き直したライト数
		UINT cached_lights = 0;		//	スライスを持ち続けているライト数
		UINT used_slices = 0;		//	使用中のスライス数
		UINT slice_count = 0;		//	スライス数
	};

public:
	//	アトラスを使うか
	static bool is_enable();

	//	アトラスを割り当てるライトの最大数
	static UINT get_max_lights();

	//	直近の統計（デバッグ表示用）
	static const statistics& get_last_statistics();

public:
	//	フレームの最初に呼ぶ
	void begin_frame();

	//	ライトのスライスを確保
	bool acquire(UINT light_id, UINT slice_count, std::uint64_t signature, bool force_render, UINT& out_first_slice, bool& out_needs_render);

	//	フレームの最後に呼ぶ
	void end_frame();

	//	シャドウマップテクスチャ
	const texture_ptr& get_texture() const { return texture; }

	//	スライス範囲への書き込み先
	ID3D11DepthStencilView* get_depth_stencil_view(UINT first_slice, UINT slice_count);

	//	1スライス分のビューポート
	const D3D11_VIEWPORT& get_viewport() const { return viewport; }

	//	今回アトラスを使ったライト数
	UINT get_allocated_lights() const { return frame_statistics.allocated_lights; }

private:
	//	ライトごとのスライス
	struct entry
	{
		UINT first_slice = 0;
		UINT slice_count = 0;
		std::uint64_t signature = 0;
		std::uint64_t last_used_frame = 0;
	};

	bool find_free_slices(UINT count, UINT& out_first_slice) const;
	void release(UINT light_id);

private:
	texture_ptr texture;
	std::unordered_map<UINT, Microsoft::WRL::ComPtr<ID3D11DepthStencilView>> depth_stencil_views;
	D3D11_VIEWPORT viewport{};
	UINT resolution = 0;
	UINT slice_count = 0;

	std::unordered_map<UINT, entry> entries;
	std::vector<UINT> slice_owners;
	std::uint64_t frame = 0;
	statistics frame_statistics;
};
