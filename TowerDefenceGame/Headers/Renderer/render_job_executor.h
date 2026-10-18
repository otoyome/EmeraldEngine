#pragma once

#include <queue>
#include <deque>

#include "renderer_context.h"
#include "frame_graph.h"

using render_job_function = void(render_job_context&);

//	描画コマンドリスト
class render_command_list
{
public:
	struct item
	{
		std::string name;										//	名前
		std::string owner_name;									//	オーナー名
		Microsoft::WRL::ComPtr<ID3D11CommandList> command_list;	//	deferred context で記録したもの
		std::function<void()> immediate_task;					//	immediate context で実行するもの
		bool wait_for_recordings = true;						//	immediate_task の実行前に、このフレームの記録が全て終わるのを待つか
		bool ready = false;
		std::vector<gpu_readback_request> readbacks;			//	@tool command_list の実行後に登録する読み戻し要求
	};

public:
	UINT reserve(std::string_view name, std::string_view owner_name, bool is_recording);

	void set_command_list(UINT index, const Microsoft::WRL::ComPtr<ID3D11CommandList>& command_list, std::vector<gpu_readback_request>&& readbacks);
	void set_immediate_task(UINT index, std::function<void()> task, bool wait_for_recordings);
	void close();

	void execute(ID3D11DeviceContext* immediate_context);

private:
	std::mutex mutex;
	std::condition_variable condition;
	std::deque<item> items;
	UINT pending_recordings = 0;
	bool closed = false;
};

//	描画用ジョブ実行者
//	☆ﾃｷﾄｰ('ω')ｰﾄｷﾃ☆
class render_job_executor
{
public:
	render_job_executor(UINT num_threads);
	~render_job_executor();

	//	コマンドリスト生成開始
	void begin_command_list();

	//	パスをコマンドリストに積む
	void execute(std::function<render_job_function> task, std::string_view name, renderer_context& context, bool use_immediate_context = false, bool wait_for_recordings = true, std::string_view owner_name = "framework");

	//	コマンドリスト生成終了
	std::shared_ptr<render_command_list> end_command_list();

private:
	//	@tool ジョブ開始時の記録開始と render_job_context の設定（immediate / deferred で共通）
	static void setup_job_context(render_job_context& job_context, const renderer_context& context);

	void run(render_command_recorder* recorder);

private:
	std::mutex mutex;
	std::condition_variable condition;
	std::vector<std::thread> worker_threads;
	std::vector<std::unique_ptr<render_command_recorder>> deferred_recorders;	//	@tool ワーカースレッドごとの deferred context の記録器
	render_command_recorder immediate_recorder;	//	@tool 提出スレッドでのみ使う

	std::queue<std::function<void(render_command_recorder&)>> tasks;
	bool running;

	std::shared_ptr<render_command_list> current_command_list;
};

