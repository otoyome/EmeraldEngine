#pragma once

#include <atomic>
#include <chrono>
#include <thread>

#include "Process/framework_process.h"
#include "Framework/splash_window.h"

//	起動プロセス
class boot_process : public framework_process
{
public:
	using framework_process::framework_process;
	~boot_process() override;

	bool enter() override;
	void exit() override;
	std::unique_ptr<framework_process> next_process() override;

private:
	void build();

	//	アセットスキャン
	void scan_assets();

	//	グローバルシェーダーコンパイル
	bool compile_global_shaders();

	//	マテリアルpipeline準備
	void prepare_material_pipelines();

	//	スプラッシュの進捗
	void set_progress_after_scan(float rate);

private:
	std::thread worker;
	std::atomic<bool> finished{ false };
	bool succeeded = false;
	std::chrono::steady_clock::time_point start_time;
	splash_window splash;	//	起動時のスプラッシュウィンドウ
};
