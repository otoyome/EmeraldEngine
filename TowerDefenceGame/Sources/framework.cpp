#include "framework.h"

#include <algorithm>

#include "Framework/app_window.h"
#include "Renderer/render_system.h"

#include "Process/core_process.h"
#include "Process/graphics_process.h"
#include "Process/input_process.h"
#include "Process/audio_process.h"
#include "Process/asset_watch_process.h"
#include "Process/editor_process.h"
#include "Process/shader_compile_process.h"

#if defined(USE_IMGUI)
#include "Editor/editor_host.h"
#endif	//	defined(USE_IMGUI)
#include "Editor/detail_profiler.h"
#include "System/engine_config_data.h"

framework::framework(app_window& window)
: window(window)
, hwnd(window.get_handle())
{
	logger::create_instance();

	window.add_message_hook([this](HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
	{
#if defined(USE_IMGUI)
		if (editor_host::handle_message(hwnd, msg, wparam, lparam)) { return true; }
#endif	//	defined(USE_IMGUI)

		if (mouse::is_valid() && mouse::instance()->window_proccess(hwnd, msg, wparam, lparam)) { return true; }

		switch (msg)
		{
			case WM_ENTERSIZEMOVE:
			{
				tictoc.stop();
				break;
			}
			case WM_EXITSIZEMOVE:
			{
				tictoc.start();
				break;
			}
		}
		return false;
	});
}

framework::~framework()
{
	window.clear_message_hooks();
	logger::delete_instance();
}

int framework::run()
{
	MSG msg{};

	if (!initialize())
	{
		return uninitialize();
	}

	//	ウィンドウ名設定
	SetWindowTextW(hwnd, engine_config_data::instance()->application.name.get<utf16_cache>().c_str());

	while (WM_QUIT != msg.message)
	{
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			tictoc.tick();

			//	詳細プロファイラーのフレーム開始
			DETAIL_PROFILE_BEGIN_FRAME();

			update(tictoc.time_interval());
			render(tictoc.time_interval());

			//	今のプロセスが済んでいたら次のプロセスへ
			if (!advance_process())
				PostQuitMessage(0);

			//	詳細プロファイラーのフレーム終了
			DETAIL_PROFILE_END_FRAME();
		}
	}

	return uninitialize() ? static_cast<int>(msg.wParam) : 0;
}

bool framework::initialize()
{
	//	常駐プロセス
	if (!add_process(std::make_unique<core_process>(*this)))
		return false;
	if (!add_process(std::make_unique<graphics_process>(*this)))
		return false;
	if (!add_process(std::make_unique<input_process>(*this)))
		return false;
	if (!add_process(std::make_unique<audio_process>(*this)))
		return false;
	if (!add_process(std::make_unique<asset_watch_process>(*this)))
		return false;
#if defined(USE_IMGUI)
	if (!add_process(std::make_unique<editor_process>(*this)))
		return false;
#endif	//	defined(USE_IMGUI)

	//	起動プロセス
	if (!add_process(std::make_unique<shader_compile_process>(*this)))
		return false;

	return advance_process();
}

bool framework::add_process(std::unique_ptr<framework_process> process)
{
	const framework_process::orders order = process->get_order();
	auto position = std::upper_bound(processes.begin(), processes.end(), order,
		[](framework_process::orders value, const process_entry& entry) { return value < entry.process->get_order(); });
	position = processes.insert(position, process_entry{ std::move(process), ++entered_count });

	return position->process->enter();
}

bool framework::advance_process()
{
	for (process_entry& entry : processes)
	{
		while (std::unique_ptr<framework_process> next = entry.process->next_process())
		{
			//	同じ位置で置き換える
			_ASSERT_EXPR(next->get_order() == entry.process->get_order(), L"next_process must keep the same order");
			entry.process->exit();
			entry.process = std::move(next);
			entry.entered_index = ++entered_count;
			if (!entry.process->enter())
				return false;
		}
	}
	return true;
}

void framework::update(float elapsed_time/*Elapsed seconds from last frame*/)
{
	//	更新処理全体の計測
	DETAIL_PROFILE_CPU_SCOPE("update");

	//	プロセス更新
	for (process_entry& entry : processes)
		entry.process->update(elapsed_time);

#if defined(USE_IMGUI)
	//	各プロセスの窓
	if (editor && editor->is_frame_begun())
	{
		//	ImGuiウィンドウ構築の計測
		DETAIL_PROFILE_CPU_SCOPE("update.imgui");
		for (process_entry& entry : processes)
			entry.process->imgui_render();
	}
#endif // defined(USE_IMGUI)
}

void framework::render(float elapsed_time/*Elapsed seconds from last frame*/)
{
	//	描画処理全体の計測
	DETAIL_PROFILE_CPU_SCOPE("render");

	if (!renderer)
		return;

	//	描画準備
	for (process_entry& entry : processes)
		entry.process->render(elapsed_time);

	//	描画コマンド実行
	renderer->begin_frame();
	for (process_entry& entry : processes)
		entry.process->submit_render_jobs(elapsed_time);
	renderer->end_frame();

	//	描画後処理
	for (process_entry& entry : processes)
		entry.process->post_render();
}

bool framework::uninitialize()
{
	while (!processes.empty())
	{
		auto last = std::max_element(processes.begin(), processes.end(),
			[](const process_entry& l, const process_entry& r) { return l.entered_index < r.entered_index; });
		last->process->exit();
		processes.erase(last);
	}
	return true;
}
