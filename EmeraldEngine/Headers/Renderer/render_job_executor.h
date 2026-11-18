#pragma once

#include <queue>

#include "renderer_domain.h"
#include "renderer_context.h"

using render_job_function = void(graphics_context&);

//	描画用ジョブ実行者
//	☆ﾃｷﾄｰ('ω')ｰﾄｷﾃ☆
class render_job_executor
{
	struct render_command_data
	{
		std::string name;
		std::string owner_name;
		std::uint64_t sort_key = 0;
		Microsoft::WRL::ComPtr<ID3D11CommandList> command_list;
		std::function<void()> task;
	};

public:
	render_job_executor(UINT num_threads);
	~render_job_executor();

	void execute(std::function<render_job_function> task, std::string_view name, renderer_context& context, UINT sort_order, bool use_immediate_context = false, std::string_view owner_name = "framework");

	void set_process_order(UINT order) { process_order = order; }

	//	全描画コマンド実行後に実行する関数
	void register_post_rendering(std::function<void()> function)
	{
		post_render_function = function;
	}

public:
	void begin();
	void render();
	void end();

private:
	void run(UINT context_index);
	void register_command(render_command_data& command_data);

private:
	std::mutex mutex;
	std::condition_variable condition;
	std::condition_variable completion_condition;
	std::vector<std::thread> worker_threads;
	std::vector<Microsoft::WRL::ComPtr<ID3D11DeviceContext>> deferred_contexts;

	std::queue<std::function<void(ID3D11DeviceContext*)>> tasks;
	std::atomic_int running_threads;
	bool running;
	std::function<void()> post_render_function;

	std::vector<render_command_data> render_command_list;

	UINT process_order = 0;
	std::uint64_t sequence_counter = 0;
};

class render_job_registrar
{
public:
	render_job_registrar(std::shared_ptr<render_job_executor> executor);

	void execute(std::function<render_job_function> task, std::string_view name, renderer_context& context, UINT sort_order, bool use_immediate_context = false);

	//	発行する描画ジョブの所有者名
	std::string_view get_owner_name() const { return owner_name; }
	void set_owner_name(std::string_view name) { owner_name = name; }

private:
	std::shared_ptr<render_job_executor> executor;
	std::string owner_name = "renderer";
};
