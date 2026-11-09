#include "Renderer/render_system.h"
#include "Graphics/render_data.h"

#include "System/engine_config_data.h"
#include "Editor/detail_profiler.h"

render_system::render_system()
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

			//	プレゼント
			UINT sync_interval{ engine_config_data::instance()->windows.vertical_sync };
			render_data::instance()->get_swap_chain()->Present(sync_interval, 0);
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
