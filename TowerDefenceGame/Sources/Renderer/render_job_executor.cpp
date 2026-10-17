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

void render_command_list::set_command_list(UINT index, const Microsoft::WRL::ComPtr<ID3D11CommandList>& command_list)
{
	{
		std::lock_guard<std::mutex> lock(mutex);
		auto& target = items[index];
		target.command_list = command_list;
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

	//	@tool immediate context 用の記録資源
	create_recording_resources(render_data::instance()->get_immediate_context().Get(), immediate_recording_resources);

	//	スレッド生成
	worker_threads.reserve(num_threads);
	deferred_contexts.reserve(num_threads);
	deferred_recording_resources.resize(num_threads);
	for (UINT index = 0; index < num_threads; ++index)
	{
		Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
		hr = render_data::instance()->get_device()->CreateDeferredContext(0, context.GetAddressOf());
		deferred_contexts.emplace_back(context);
		//	@tool スレッド開始前に、そのスレッド用の記録資源を作っておく
		create_recording_resources(context.Get(), deferred_recording_resources[index]);
		worker_threads.emplace_back(std::thread(&render_job_executor::run, this, index));
	}
}

void render_job_executor::create_recording_resources(ID3D11DeviceContext* device_context, recording_resources& out_resources)
{
	HRESULT hr = device_context->QueryInterface(IID_PPV_ARGS(out_resources.device_context1.ReleaseAndGetAddressOf()));
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	auto render_data = render_data::instance();
	out_resources.constant_allocator = std::make_unique<immediate_constant_allocator>();
	out_resources.constant_allocator->initialize(render_data->get_device().Get(), render_data->get_feature_supports().constant_buffer_offsetting);
}

void render_job_executor::setup_graphics_context(graphics_context& g_context, ID3D11DeviceContext* device_context, recording_resources& resources, const renderer_context& context)
{
	//	新しいコマンドリストの記録開始
	resources.constant_allocator->begin();
	g_context.recorder.begin(device_context, resources.device_context1.Get(), resources.constant_allocator.get());

	g_context.render_data = context.render_data;
	g_context.scene_constant = context.scene_constant;
	g_context.scene_constant_buffer = context.scene_constant_buffer;
	g_context.clustered_lighting = context.current_view_state ? context.current_view_state->get_clustered_lighting() : nullptr;

	//	リソースをテーブルにバインド
	g_context.register_frame_bind_table();
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
				graphics_context g_context;
				setup_graphics_context(g_context, render_data::instance()->get_immediate_context().Get(), this->immediate_recording_resources, context);
				g_context.clear_render_target_slots();
				g_context.clear_shader_resorce_views();
				g_context.clear_unordered_access_views();
				g_context.debug_begin_event(system_utility::convert_utf16(name_text).data());
				{
					DETAIL_PROFILE_RENDER_JOB_SCOPE(owner_text, name_text);
					task(g_context);
				}
				g_context.debug_end_event();
			}, wait_for_recordings);
		return;
	}

	{
		std::lock_guard<std::mutex> lock_guard(mutex);
		std::shared_ptr<render_command_list> render_command_list = current_command_list;
		tasks.push(
			[task, name_text, owner_text, context, render_command_list, index](ID3D11DeviceContext* deferred_context, recording_resources& resources)
			{
				//	描画コマンド積み込み
				graphics_context g_context;
				setup_graphics_context(g_context, deferred_context, resources, context);
				g_context.debug_begin_event(system_utility::convert_utf16(name_text).data());
				{
					DETAIL_PROFILE_RENDER_JOB_SCOPE(owner_text, name_text);
					task(g_context);
				}
				g_context.debug_end_event();

				//	コマンドリスト生成
				Microsoft::WRL::ComPtr<ID3D11CommandList> command_list;
				deferred_context->FinishCommandList(false, command_list.GetAddressOf());
				render_command_list->set_command_list(index, command_list);
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

void render_job_executor::run(UINT context_index)
{
	auto deferred_context = deferred_contexts[context_index];
	auto& recording = deferred_recording_resources[context_index];
	while (true)
	{
		std::function<void(ID3D11DeviceContext*, recording_resources&)> task_function;

		{
			std::unique_lock<std::mutex> unique_lock(mutex);
			condition.wait(unique_lock, [&]{return !tasks.empty() || !running;});
			if (!running && tasks.empty())
				break;

			task_function = std::move(tasks.front());
			tasks.pop();
		}

		task_function(deferred_context.Get(), recording);
	}
}

