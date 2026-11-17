#pragma once

#include <functional>
#include <memory>

#include "Renderer/render_job_executor.h"
#include "Renderer/display_ui_compositor.h"

//	描画システム
class render_system
{
public:
	using overlay_function = std::function<void()>;

public:
	explicit render_system(HWND hwnd);
	~render_system();

	render_system(const render_system&) = delete;
	render_system& operator=(const render_system&) = delete;
	render_system(render_system&&) noexcept = delete;
	render_system& operator=(render_system&&) noexcept = delete;

	const std::shared_ptr<render_job_executor>& get_executor() const { return executor; }

	//	描画最後に上書きしたいものがあれば
	void set_overlay(overlay_function overlay);

	//	プロセスの位置設定
	void set_process_order(UINT order) { executor->set_process_order(order); }

	void begin_frame();
	void end_frame();

private:
	CONST HWND hwnd;
	std::shared_ptr<render_job_executor> executor;
	overlay_function overlay;
	std::unique_ptr<display_ui_compositor> ui_compositor;
};
