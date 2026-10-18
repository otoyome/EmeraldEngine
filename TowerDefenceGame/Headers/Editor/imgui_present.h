#pragma once

#if defined(USE_IMGUI)
#include <memory>
#include <vector>

#include "imgui.h"

//	@tool ImGui の描画を提出スレッドで行うための部品（描画データの複製・ウィンドウの表示先・描画と表示）
namespace imgui_present
{
	//	@tool ImGui の描画データの複製（提出スレッドで描くため。描画リストは複製が所有する）
	struct draw_snapshot
	{
		ImDrawData draw_data;
		//	@tool 描画リストが参照するテクスチャ（提出スレッドで描き終わるまで解放させない）
		std::vector<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> textures;
		~draw_snapshot();
	};
	using draw_snapshot_ptr = std::shared_ptr<draw_snapshot>;

	//	@tool ImGui の描画データを複製する（参照しているテクスチャも保持する）
	draw_snapshot_ptr copy_draw_data(const ImDrawData& source);

	//	@tool ウィンドウの表示先（スワップチェーンと描画先ビュー）
	//	@tool 生成・リサイズ・破棄はメインスレッド、描画と表示は提出スレッドで行う。リサイズと破棄は描画中のフレームを待ってから行う
	struct present_target
	{
		Microsoft::WRL::ComPtr<IDXGISwapChain> swap_chain;
		Microsoft::WRL::ComPtr<ID3D11RenderTargetView> render_target_view;
		UINT width = 0;
		UINT height = 0;

		//	@tool ウィンドウの表示先を作る（失敗した場合はスワップチェーンが無い物を返す）
		static std::shared_ptr<present_target> create(HWND window, UINT width, UINT height);
		//	@tool 大きさを変える（描画中のフレームを待ってから呼ぶ）
		void resize(UINT width, UINT height);
		//	@tool 解放する（描画中のフレームを待ってから呼ぶ）
		void release();
	};
	using present_target_ptr = std::shared_ptr<present_target>;

	//	@tool 1つのウィンドウに描く ImGui の描画データ
	struct window_draw
	{
		present_target_ptr target;
		draw_snapshot_ptr draw_data;
		bool clear = true;
	};

	//	@tool 提出スレッド：ウィンドウごとに描いて表示する（ImGui の描画バックエンドを持つコンテキストを現在にしてから呼ぶ）
	void draw_windows(ID3D11DeviceContext* immediate_context, const std::vector<window_draw>& windows);
}
#endif	//	defined(USE_IMGUI)
