#pragma once

#include <atomic>
#include <chrono>
#include <thread>

#include "Process/framework_process.h"
#include "Framework/splash_window.h"

//	シェーダーコンパイルプロセス
class shader_compile_process : public framework_process
{
public:
	using framework_process::framework_process;
	~shader_compile_process() override;

	bool enter() override;
	void exit() override;
	std::unique_ptr<framework_process> next_process() override;

private:
	void build();

private:
	std::thread worker;
	std::atomic<bool> finished{ false };
	bool succeeded = false;
	std::chrono::steady_clock::time_point start_time;
	splash_window splash;	//	起動時のスプラッシュウィンドウ
};
