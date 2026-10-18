#include "Renderer/render_job_executor.h"
#include <thread>
#include <fstream>

#include "Editor/detail_profiler.h"
#include "Renderer/renderer_context.h"
#include "graphics/clustered_lighting_system.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////
UINT render_command_list::reserve(std::string_view name, std::string_view owner_name, bool is_recording)
{
	std::lock_guard<std::mutex> lock(mutex);
	if (is_recording)
		++pending_recordings;
	auto& target = items.emplace_back();
	target.name = name;
	target.owner_name = owner_name;
	return static_cast<UINT>(items.size() - 1);
}

void render_command_list::set_command_list(UINT index, const Microsoft::WRL::ComPtr<ID3D11CommandList>& command_list, std::vector<gpu_readback_request>&& readbacks)
{
	{
		std::lock_guard<std::mutex> lock(mutex);
		auto& target = items[index];
		target.command_list = command_list;
		target.readbacks = std::move(readbacks);
		target.ready = true;
		--pending_recordings;
	}
	condition.notify_all();
}

void render_command_list::set_immediate_task(UINT index, std::function<void()> task, bool wait_for_recordings)
{
	{
		std::lock_guard<std::mutex> lock(mutex);
		auto& target = items[index];
		target.immediate_task = std::move(task);
		target.wait_for_recordings = wait_for_recordings;
		target.ready = true;
	}
	condition.notify_all();
}

void render_command_list::close()
{
	{
		std::lock_guard<std::mutex> lock(mutex);
		closed = true;
	}
	condition.notify_all();
}

void render_command_list::execute(ID3D11DeviceContext* immediate_context)
{
	//	常駐GPUスコープ計測フレーム開始
	DETAIL_PROFILE_GPU_BEGIN_FRAME();

	//	@tool 前のフレームまでの読み戻しで、GPU の処理が終わったものを受け取る
	gpu_readback_queue::instance().poll(immediate_context);

	for (std::size_t index = 0; ; ++index)
	{
		item* target = nullptr;
		{
			std::unique_lock<std::mutex> lock(mutex);
			condition.wait(lock, [&]
			{
				if (index >= items.size())
					return closed;
				const item& next = items[index];
				if (!next.ready)
					return false;
				//	@tool immediate context のパスは、記録中のジョブと CPU 側の情報を取り合わないよう記録の完了を待つ
				if (next.immediate_task && next.wait_for_recordings)
					return closed && pending_recordings == 0;
				return true;
			});
			if (index >= items.size())
				break;
			target = &items[index];
		}

		DETAIL_PROFILE_GPU_SCOPE_BEGIN();
		if (target->command_list)
		{
			immediate_context->ExecuteCommandList(target->command_list.Get(), false);
			gpu_readback_queue::instance().submit(std::move(target->readbacks));
		}
		else if (target->immediate_task)
		{
			target->immediate_task();
		}
		DETAIL_PROFILE_GPU_SCOPE_END(target->owner_name, target->name);

		//	@tool 実行し終わったものは早めに解放する
		target->command_list.Reset();
		target->immediate_task = nullptr;
	}

	//	常駐GPUスコープ計測フレーム終了
	DETAIL_PROFILE_GPU_END_FRAME();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
render_job_executor::render_job_executor(UINT num_threads)
: running(true)
{
	HRESULT hr;
	assert(num_threads != -1);

	//	@tool immediate context 用の記録器
	immediate_recorder.initialize(render_data::instance()->get_immediate_context());

	//	スレッド生成
	worker_threads.reserve(num_threads);
	deferred_recorders.reserve(num_threads);
	for (UINT index = 0; index < num_threads; ++index)
	{
		Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
		hr = render_data::instance()->get_device()->CreateDeferredContext(0, context.GetAddressOf());
		//	@tool スレッド開始前に、そのスレッド用の記録器を作っておく
		auto& recorder = deferred_recorders.emplace_back(std::make_unique<render_command_recorder>());
		recorder->initialize(context);
		worker_threads.emplace_back(std::thread(&render_job_executor::run, this, recorder.get()));
	}
}

void render_job_executor::setup_job_context(render_job_context& job_context, const renderer_context& context)
{
	//	新しいコマンドリストの記録開始
	job_context.recorder.begin();

	job_context.render_data = context.render_data;
	job_context.scene_constant = context.scene_constant;
	job_context.scene_constant_buffer = context.scene_constant_buffer;
	job_context.clustered_lighting = context.current_view_state ? context.current_view_state->get_clustered_lighting() : nullptr;

	//	リソースをテーブルにバインド
	job_context.register_frame_bind_table();
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

void render_job_executor::begin_command_list()
{
	current_command_list = std::make_shared<render_command_list>();
}

void render_job_executor::execute(std::function<render_job_function> task, std::string_view name, renderer_context& context, bool use_immediate_context, bool wait_for_recordings, std::string_view owner_name)
{
	_ASSERT_EXPR(current_command_list, L"begin_command_list is not called.");

	std::string name_text = std::string(name);
	std::string owner_text = std::string(owner_name);
	const UINT index = current_command_list->reserve(name_text, owner_text, !use_immediate_context);

	if (use_immediate_context)
	{
		current_command_list->set_immediate_task(index,
			[task, name_text, owner_text, context, this, command_list = current_command_list.get(), index]()
			{
				//	描画コマンド積み込み
				render_command_recorder& recorder = this->immediate_recorder;
				render_job_context job_context(recorder);
				setup_job_context(job_context, context);
				recorder.clear_render_target_slots();
				recorder.clear_shader_resorce_views();
				recorder.clear_unordered_access_views();
				recorder.begin_event(system_utility::convert_utf16(name_text).data());
				{
					DETAIL_PROFILE_RENDER_JOB_SCOPE(owner_text, name_text);
					task(job_context);
				}
				recorder.end_event();
				recorder.end();
				gpu_readback_queue::instance().submit(recorder.take_readback_requests());
			}, wait_for_recordings);
		return;
	}

	{
		std::lock_guard<std::mutex> lock_guard(mutex);
		std::shared_ptr<render_command_list> render_command_list = current_command_list;
		tasks.push(
			[task, name_text, owner_text, context, render_command_list, index](render_command_recorder& recorder)
			{
				//	描画コマンド積み込み
				render_job_context job_context(recorder);
				setup_job_context(job_context, context);
				recorder.begin_event(system_utility::convert_utf16(name_text).data());
				{
					DETAIL_PROFILE_RENDER_JOB_SCOPE(owner_text, name_text);
					task(job_context);
				}
				recorder.end_event();
				recorder.end();

				//	コマンドリスト生成
				render_command_list->set_command_list(index, recorder.finish_command_list(), recorder.take_readback_requests());
			});
	}

	//	待機スレッドを起床
	condition.notify_one();
}

std::shared_ptr<render_command_list> render_job_executor::end_command_list()
{
	std::shared_ptr<render_command_list> command_list = std::move(current_command_list);
	command_list->close();
	return command_list;
}

void render_job_executor::run(render_command_recorder* recorder)
{
	while (true)
	{
		std::function<void(render_command_recorder&)> task_function;

		{
			std::unique_lock<std::mutex> unique_lock(mutex);
			condition.wait(unique_lock, [&]{return !tasks.empty() || !running;});
			if (!running && tasks.empty())
				break;

			task_function = std::move(tasks.front());
			tasks.pop();
		}

		task_function(*recorder);
	}
}

