#include "Renderer/render_job_executor.h"
#include <thread>

#include "Editor/detail_profiler.h"
#include "Renderer/renderer_context.h"
#include "Graphics/Lighting/clustered_lighting_system.h"

#if defined(USE_IMGUI)
#include "imgui.h"
#include "imgui/imgui_impl_dx11.h"
#include "imgui/imgui_impl_win32.h"
#endif // defined(USE_IMGUI)

namespace
{
	std::uint64_t make_sort_key(UINT process_order, UINT sort_order, std::uint64_t sequence)
	{
		_ASSERT_EXPR(process_order < (1u << 8), L"process_order overflow");
		_ASSERT_EXPR(sort_order < (1u << 24), L"sort_order overflow");
		return (static_cast<std::uint64_t>(process_order) << 56) | (static_cast<std::uint64_t>(sort_order) << 32) | (sequence & 0xFFFFFFFFull);
	}
}

render_job_executor::render_job_executor(UINT num_threads)
: running(true)
{
	HRESULT hr;
	assert(num_threads != -1);

	//	スレッド生成
	worker_threads.reserve(num_threads);
	deferred_contexts.reserve(num_threads);
	for (UINT index = 0; index < num_threads; ++index)
	{
		Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
		hr = render_data::instance()->get_device()->CreateDeferredContext(0, context.GetAddressOf());
		deferred_contexts.emplace_back(context);
		worker_threads.emplace_back(std::thread(&render_job_executor::run, this, index));
	}
}

render_job_executor::~render_job_executor()
{
	//	スレッドを終了させる
	{
		std::lock_guard<std::mutex>	cLockGuard(mutex);
		if (!running)
			return;

		running = false;
	}

	//	積み込みスレッド破棄
	condition.notify_all();
	for (auto& it : worker_threads)
	{
		it.join();
	}
}

void render_job_executor::execute(std::function<render_job_function> task, std::string_view name, renderer_context& context, UINT sort_order, bool use_immediate_context, std::string_view owner_name)
{
	std::string name_text = std::string(name);
	std::string owner_text = std::string(owner_name);
	if (use_immediate_context)
	{
		render_command_data command_data;
		command_data.name = name_text;
		command_data.owner_name = owner_text;
		command_data.sort_key = make_sort_key(process_order, sort_order, sequence_counter++);
		command_data.task = std::function<void()>(
			[task, name_text, owner_text, context]()
			{
				//	描画コマンド積み込み
				graphics_context g_context;
				g_context.device_context = render_data::instance()->get_immediate_context().Get();
				g_context.render_data = context.render_data;
				g_context.scene_constant = context.scene_constant;
				g_context.scene_constant_buffer = context.scene_constant_buffer;
				g_context.clustered_lighting = context.current_view_state ? context.current_view_state->get_clustered_lighting() : nullptr;
				g_context.debug_begin_event(system_utility::convert_utf16(name_text).data());
				{
					DETAIL_PROFILE_RENDER_JOB_SCOPE(owner_text, name_text);
					task(g_context);
				}
				g_context.debug_end_event();
			} );

		register_command(command_data);
	}
	else
	{
		std::lock_guard<std::mutex> lock_guard(mutex);
		const std::uint64_t command_sort_key = make_sort_key(process_order, sort_order, sequence_counter++);
		auto task_function = std::make_shared<std::function<void(ID3D11DeviceContext*)>>(
			[task, name_text, owner_text, context, command_sort_key, this](ID3D11DeviceContext* deferred_context)
			{
				//	描画コマンド積み込み
				graphics_context g_context;
				g_context.device_context = deferred_context;
				g_context.render_data = context.render_data;
				g_context.scene_constant = context.scene_constant;
				g_context.scene_constant_buffer = context.scene_constant_buffer;
				g_context.clustered_lighting = context.current_view_state ? context.current_view_state->get_clustered_lighting() : nullptr;
				g_context.debug_begin_event(system_utility::convert_utf16(name_text).data());
				{
					DETAIL_PROFILE_RENDER_JOB_SCOPE(owner_text, name_text);
					task(g_context);
				}
				g_context.debug_end_event();

				//	コマンドリスト生成
				render_command_data command_data;
				command_data.name = name_text;
				command_data.owner_name = owner_text;
				command_data.sort_key = command_sort_key;
				deferred_context->FinishCommandList(false, command_data.command_list.GetAddressOf());
				this->register_command(command_data);
			} );

		tasks.push(std::function<void(ID3D11DeviceContext*)>(
			[task_function](ID3D11DeviceContext* context)
			{
				(*task_function)(context);
			}));
	}

	//	待機スレッドを起床
	condition.notify_one();
}

void render_job_executor::begin()
{
	//	積んだ順はフレームごとに数え直す
	sequence_counter = 0;

	//	常駐GPUスコープ計測フレーム開始
	DETAIL_PROFILE_GPU_BEGIN_FRAME();

	//	1F前のGPU情報を受け取る
	render_data::instance()->polling_gpu_readback();
}

void render_job_executor::end()
{
	//	常駐GPUスコープ計測フレーム終了
	DETAIL_PROFILE_GPU_END_FRAME();

	render_command_list.clear();
	post_render_function();
}

void render_job_executor::render()
{
	std::vector<render_command_data> local_command_list;
	{
		std::unique_lock<std::mutex> unique_lock(mutex);
		completion_condition.wait(unique_lock, [&] { return tasks.empty() && running_threads <= 0; });
		local_command_list = std::move(render_command_list);   // ロック保持中に取り出す
		render_command_list.clear();
	}

	std::sort(local_command_list.begin(), local_command_list.end(),
		[](const render_command_data& l, const render_command_data& r)
		{
			//	プロセスごとにまとめ、同じ順番の物は積んだ順にする
			return l.sort_key < r.sort_key;
		}
	);

	ID3D11DeviceContext* immediate_context = render_data::instance()->get_immediate_context().Get();

	//	GPU転送
	{
		DETAIL_PROFILE_GPU_SCOPE_BEGIN();
		render_data::instance()->upload_gpu_data();
		DETAIL_PROFILE_GPU_SCOPE_END("framework", "gpu_upload_list");
	}

	//	コマンドリスト実行
	for (auto& command : local_command_list)
	{
		DETAIL_PROFILE_GPU_SCOPE_BEGIN();
		if (command.command_list)
		{
			immediate_context->ExecuteCommandList(command.command_list.Get(), false);
		}
		else
		{
			command.task();
		}
		DETAIL_PROFILE_GPU_SCOPE_END(command.owner_name, command.name);
	}

	//	GPUの処理を纏めて受け取る
	auto& readback_queue = render_data::instance()->get_readback_queue();
	readback_queue.mark_submitted();
	readback_queue.poll(immediate_context);
}

void render_job_executor::register_command(render_command_data& command_data)
{
	std::unique_lock<std::mutex> unique_lock(mutex);

	render_command_list.emplace_back(command_data);
}

void render_job_executor::run(UINT context_index)
{
	auto deferred_context = deferred_contexts[context_index];
	while (true)
	{
		std::function<void(ID3D11DeviceContext*)> task_function;

		{
			std::unique_lock<std::mutex> unique_lock(mutex);
			condition.wait(unique_lock, [&]{return !tasks.empty() || !running;});
			++running_threads;
			if (!running && tasks.empty())
			{
				--running_threads;
				completion_condition.notify_all();
				break;
			}
			if (tasks.empty())
			{
				--running_threads;
				completion_condition.notify_all();
				continue;;
			}

			task_function = std::move(tasks.front());
			tasks.pop();
		}

		task_function(deferred_context.Get());

		{
			std::lock_guard<std::mutex> lock_guard(mutex);
			--running_threads;
			completion_condition.notify_all(); 
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
render_job_registrar::render_job_registrar(std::shared_ptr<render_job_executor> executor)
: executor(executor)
{
}

void render_job_registrar::execute(std::function<render_job_function> task, std::string_view name, renderer_context& context, UINT sort_order, bool use_immediate_context)
{
	executor->execute(task, name, context, sort_order, use_immediate_context, owner_name);
}

