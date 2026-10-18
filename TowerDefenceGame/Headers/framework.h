#pragma once

#include <memory>

#include "Scenes/scene_base.h"
#include "System/engine_world.h"
#include "Framework/app_window.h"
#include "Framework/asset_watcher.h"
#include "Renderer/render_system.h"
#include "camera.h"

#if defined(USE_IMGUI)
#include "Editor/editor_host.h"
#endif	//	defined(USE_IMGUI)

//	@tool アプリケーション全体の流れ（管理クラスの生成・破棄の順番、毎フレームの更新と描画の呼び出し順）
//	@tool 描画は render_system、エディター機能は editor_host、ウィンドウは app_window、アセットの再読込は asset_watcher が持つ
class framework
{
public:
	static constexpr const TCHAR* APPLICATION_NAME = _T("Sample Engine");

public:
	framework(app_window& window);
	~framework();

	framework(const framework&) = delete;
	framework& operator=(const framework&) = delete;
	framework(framework&&) noexcept = delete;
	framework& operator=(framework&&) noexcept = delete;

	int run();

private:
	bool initialize();
	void update(float elapsed_time);
	void render(float elapsed_time);
	bool uninitialize();

	bool create_collision_filters();

	//	@tool ワールド1つ分の更新（処理中のワールドに対して行う）
	void update_world(float elapsed_time);
	//	@tool 全てのワールドで順に処理する（メイン → ImGui のウィンドウに映すワールド）
	template<class Function>
	void for_each_world(Function&& function)
	{
		function();
#if defined(USE_IMGUI)
		if (editor)
			editor->for_each_world(function);
#endif	//	defined(USE_IMGUI)
	}

private:
	app_window& window;
	CONST HWND hwnd;

	//	@tool 描画（抽出・描画スレッド・提出スレッド）
	std::unique_ptr<render_system> renderer;
	//	@tool アセットの再読込
	asset_watcher assets;
#if defined(USE_IMGUI)
	//	@tool エディター機能（ImGui・デバッグ用のウィンドウ・エディター用のワールド）
	std::unique_ptr<editor_host> editor;
#endif	//	defined(USE_IMGUI)

	//	@tool ゲームのワールド
	std::unique_ptr<engine_world> main_world;

	//	メイン視点情報
	view_parameters main_view_parameters;
	bool main_view_camera_cut = false;

private:	//	その他
	high_resolution_timer tictoc;
};
