#pragma once

#include "render_pass.h"
#include "render_data.h"
#include "graphics/shadow_atlas.h"

//	レンダーターゲット集
//	☆ﾃｷﾄｰ('ω')ｰﾄｷﾃ☆
//	@tool 視点ごとに持つ資源（レンダーターゲットと、前のフレームから引き継ぐ情報）
//	@tool 描画中のフレームは shared_ptr で持ち続けるので、作り直す時は新しく作って差し替える
struct scene_render_targets
{
	texture_ptr scene_color_texture;
	texture_ptr scene_depth_stencil_texture;
	std::vector<texture_ptr> g_buffer_textures;
	texture_ptr shadowmap_depth_texture;

	//	@tool 作業用テクスチャは各パスがフレーム毎にフレームグラフの一時テクスチャとして要求する

	//	SSRデノイズ用
	texture_ptr ssr_accumlate_textures[2];
	texture_ptr ssr_counter_textures[2];

	//	@tool 最終的な出力先（null はバックバッファ。エディターの画面などはテクスチャへ出力して ImGui で表示する）
	texture_ptr output_texture;
	//	@tool 出力先の描画先とビューポート（作り置き）
	render_target_view output_view;
	D3D11_VIEWPORT output_viewport{};

	//	@tool 前のフレームから引き継ぐ情報（描画スレッドがパスを準備する時に読み書きする）
	struct view_history
	{
		//	@tool SSR の蓄積バッファの切り替え
		bool ssr_accumlate_flip = false;

		//	@tool 平均輝度（前のフレームの値から順応させる）
		texture_ptr average_luminance_textures[2];
		int average_luminance_index = 0;

		//	@tool 点光源・スポットライトのシャドウマップアトラス
		shadow_atlas atlas;

		//	@tool シャドウマップの描画先（配列の深度ビュー。初回に作る）
		render_pass shadow_map_pass;
		D3D11_VIEWPORT shadow_map_viewport{};

		//	@tool 被写界深度の範囲総和テーブル（大きさは視点の大きさで決まる）
		UINT dof_filter_buffer_width = 0;
		UINT dof_filter_buffer_height = 0;
		Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> dof_filter_buffer_uav;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> dof_filter_buffer_srv;
		Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> dof_filter_buffer_transpose_uav;
	};
	view_history history;

	//	@tool シーンの大きさ
	UINT get_width() const { return scene_color_texture ? static_cast<UINT>(scene_color_texture->get_width()) : 0; }
	UINT get_height() const { return scene_color_texture ? static_cast<UINT>(scene_color_texture->get_height()) : 0; }

	//	@tool シーン全体のビューポート
	D3D11_VIEWPORT get_scene_viewport() const
	{
		D3D11_VIEWPORT viewport{};
		viewport.Width = static_cast<float>(get_width());
		viewport.Height = static_cast<float>(get_height());
		viewport.MinDepth = 0.0f;
		viewport.MaxDepth = 1.0f;
		return viewport;
	}

	//	@tool 出力先の描画先とビューポートを作る（作成時に一度呼ぶ）
	void create_output_view()
	{
		if (output_texture)
		{
			texture_ptr texture = output_texture;
			output_view = render_target_view(texture);
		}
		else
		{
			output_view = render_target_view(render_data::instance()->get_swap_chain());
		}

		output_viewport = {};
		if (auto texture = std::dynamic_pointer_cast<texture2d>(output_texture))
		{
			output_viewport.Width = static_cast<float>(texture->get_desc().Width);
			output_viewport.Height = static_cast<float>(texture->get_desc().Height);
		}
		else
		{
			DXGI_SWAP_CHAIN_DESC desc;
			render_data::instance()->get_swap_chain()->GetDesc(&desc);
			output_viewport.Width = static_cast<float>(desc.BufferDesc.Width);
			output_viewport.Height = static_cast<float>(desc.BufferDesc.Height);
		}
		output_viewport.MinDepth = 0.0f;
		output_viewport.MaxDepth = 1.0f;
	}
};
