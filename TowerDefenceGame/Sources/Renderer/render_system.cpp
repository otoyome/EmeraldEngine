#include "Renderer/render_system.h"
#include "Renderer/world_renderer.h"
#include "Graphics/render_data.h"

#include <algorithm>

#include "System/engine_config_data.h"
#include "Editor/detail_profiler.h"

render_system::render_system(HWND hwnd)
: hwnd(hwnd)
{
	//	描画ジョブの実行者
	executor = std::make_shared<render_job_executor>(4);

	//	全描画コマンド処理後に呼び出される関数登録
	executor->register_post_rendering(
		[this]
		{
			//	オーバーレイしたい場合
			if (overlay)
				overlay();

			//	ウィンドウが隠れている間(起動中)は表示しない
			if (!IsWindowVisible(this->hwnd))
				return;

			//	プレゼント
			UINT sync_interval{ engine_config_data::instance()->windows.vertical_sync };
			render_data::instance()->present(sync_interval);
		});
}

render_system::~render_system()
{
}

void render_system::set_overlay(overlay_function overlay)
{
	this->overlay = std::move(overlay);
}

void render_system::begin_frame()
{
	//	レンダラー開始処理
	executor->begin();
}

void render_system::end_frame()
{
	//	描画コマンドを実行
	{
		DETAIL_PROFILE_CPU_SCOPE("render.execute_commands");
		executor->render();
	}

	//	描画コマンド実行終了処理
	{
		DETAIL_PROFILE_CPU_SCOPE("render.imgui_and_present");
		executor->end();
	}
}

void render_system::add_back_buffer_renderer(world_renderer* renderer)
{
	if (renderer && std::find(back_buffer_renderers.begin(), back_buffer_renderers.end(), renderer) == back_buffer_renderers.end())
		back_buffer_renderers.emplace_back(renderer);
}

void render_system::remove_back_buffer_renderer(world_renderer* renderer)
{
	back_buffer_renderers.erase(std::remove(back_buffer_renderers.begin(), back_buffer_renderers.end(), renderer), back_buffer_renderers.end());
}

bool render_system::resize_back_buffer(UINT width, UINT height)
{
	//	バックバッファの描画先のコピーを手放させる
	for (world_renderer* renderer : back_buffer_renderers)
		renderer->release_output();

	const bool succeeded = render_data::instance()->resize_back_buffer(width, height);

	//	失敗しても元の大きさで描画先は作り直されているので、どちらでも作り直させる
	for (world_renderer* renderer : back_buffer_renderers)
		renderer->rebuild_output();
	return succeeded;
}
