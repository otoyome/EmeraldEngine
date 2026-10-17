#pragma once

#include <queue>
#include <deque>

#include "renderer_context.h"
#include "frame_graph.h"

using render_job_function = void(graphics_context&);

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
	};

public:
	UINT reserve(std::string_view name, std::string_view owner_name, bool is_recording);

	void set_command_list(UINT index, const Microsoft::WRL::ComPtr<ID3D11CommandList>& command_list);
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
	//	@tool 記録に使うコンテキストごとの資源
	struct recording_resources
	{
		Microsoft::WRL::ComPtr<ID3D11DeviceContext1> device_context1;
		std::unique_ptr<immediate_constant_allocator> constant_allocator;
	};
	static void create_recording_resources(ID3D11DeviceContext* device_context, recording_resources& out_resources);

	//	@tool ジョブ開始時の graphics_context の設定（immediate / deferred で共通）
	static void setup_graphics_context(graphics_context& g_context, ID3D11DeviceContext* device_context, recording_resources& resources, const renderer_context& context);

	void run(UINT context_index);

private:
	std::mutex mutex;
	std::condition_variable condition;
	std::vector<std::thread> worker_threads;
	std::vector<Microsoft::WRL::ComPtr<ID3D11DeviceContext>> deferred_contexts;
	std::vector<recording_resources> deferred_recording_resources;	//	@tool deferred_contexts と同じ並び
	recording_resources immediate_recording_resources;				//	@tool 提出スレッドでのみ使う

	std::queue<std::function<void(ID3D11DeviceContext*, recording_resources&)>> tasks;
	bool running;

	std::shared_ptr<render_command_list> current_command_list;
};

