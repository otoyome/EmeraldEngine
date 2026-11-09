#pragma once

#include <atomic>
#include <chrono>
#include <thread>

#include "Process/framework_process.h"
#include "Renderer/world_renderer.h"

class ui_image;
class render_output;

//	シェーダーコンパイルプロセス
class shader_compile_process : public framework_process
{
public:
	using framework_process::framework_process;
	~shader_compile_process() override;

	bool enter() override;
	void exit() override;
	void update(float elapsed_time) override;
	void render(float elapsed_time) override;
	void submit_render_jobs(float elapsed_time) override;
	void post_render() override;
#if defined(USE_IMGUI)
	void imgui_render() override;
#endif	//	defined(USE_IMGUI)
	std::unique_ptr<framework_process> next_process() override;

private:
	void build(std::shared_ptr<render_output> output);
	float get_progress() const;

private:
	std::thread worker;	//	作成するスレッド
	std::atomic<int> finished_steps{ 0 };
	std::atomic<bool> finished{ false };
	bool succeeded = false;

	std::chrono::steady_clock::time_point start_time;

	std::unique_ptr<world_renderer> created_renderer;

private:	//	表示UI
	std::shared_ptr<ui_image> progress_bar;
};
