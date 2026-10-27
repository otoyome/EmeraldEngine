#include "Resource/resource_manager.h"

#include <algorithm>
#include <chrono>
#include <crtdbg.h>
#include <exception>
#include <filesystem>
#include <objbase.h>

#include "system/logger.h"

namespace
{
	//	@tool コールバック内での追加登録を同フレームで処理する最大回数
	constexpr int dispatch_pass_max = 8;
}

resource_manager::resource_manager()
: main_thread_id(std::this_thread::get_id())
{
	workers.reserve(worker_count);
	for (unsigned index = 0; index < worker_count; ++index)
	{
		workers.emplace_back([this] { worker_loop(); });
	}
}

resource_manager::~resource_manager()
{
	{
		std::lock_guard<std::mutex> lock(mutex);
		shutting_down = true;
	}
	work_condition.notify_all();
	for (auto& worker : workers)
	{
		if (worker.joinable())
			worker.join();
	}

	//	@tool 未完了のリクエストを回収(ローダーの破棄はこのスレッドで行う)
	std::vector<request_ptr> remaining;
	{
		std::lock_guard<std::mutex> lock(mutex);
		for (auto& queue : pending_queues)
		{
			remaining.insert(remaining.end(), queue.begin(), queue.end());
			queue.clear();
		}
		remaining.insert(remaining.end(), loaded_requests.begin(), loaded_requests.end());
		loaded_requests.clear();
		processing_requests.clear();
		dispatch_operations.clear();
	}

	//	@tool 終了処理中なのでコールバックは呼び出さずに中断扱いで完了させる
	for (auto& target : remaining)
	{
		for (auto& waiter : target->waiters)
		{
			if (auto operation = waiter.lock())
			{
				operation->clear_completed_callbacks();
				operation->complete(nullptr, resource::operation_state::canceled, "resource_manager shutdown");
			}
		}
	}
}

bool resource_manager::wait(resource::load_operation_base& operation)
{
	_ASSERT_EXPR(is_main_thread(), L"resource_manager::wait must be called from main thread");
	if (!is_main_thread())
		return false;

	if (operation.is_done())
		return operation.is_succeeded();

	request_ptr target;
	{
		std::lock_guard<std::mutex> lock(mutex);
		target = std::static_pointer_cast<request>(operation.request_link.lock());
	}
	if (target)
		wait_request(target);

	return operation.is_succeeded();
}

void resource_manager::update()
{
	_ASSERT_EXPR(is_main_thread(), L"resource_manager::update must be called from main thread");
	if (!is_main_thread())
		return;

	//	@tool 構築処理(時間上限付き。最低1件は処理する)
	{
		using clock = std::chrono::steady_clock;
		const clock::time_point start_time = clock::now();
		const float budget = build_time_budget;

		size_t built_count = 0;
		for (;;)
		{
			request_ptr target;
			{
				std::lock_guard<std::mutex> lock(mutex);
				if (built_count > 0 && budget > 0.0f)
				{
					const float elapsed = std::chrono::duration<float, std::milli>(clock::now() - start_time).count();
					if (elapsed >= budget)
						break;
				}

				//	@tool 依存リソースが揃った物から順に構築する
				for (auto it = loaded_requests.begin(); it != loaded_requests.end(); ++it)
				{
					if (is_buildable(**it))
					{
						target = std::move(*it);
						loaded_requests.erase(it);
						break;
					}
				}
			}
			if (!target)
				break;

			finalize(target);
			++built_count;
		}
	}

	//	@tool 完了コールバック呼び出し
	dispatch_callbacks();

	//	@tool 解放済みキャッシュの掃除
	if (++frame_count % expired_cache_cleanup_interval == 0)
	{
		remove_expired_caches();
	}
}

void resource_manager::remove_expired_caches()
{
	std::lock_guard<std::mutex> lock(mutex);
	for (auto it = caches.begin(); it != caches.end(); )
	{
		if (it->second.resource.expired())
			it = caches.erase(it);
		else
			++it;
	}
}

std::wstring resource_manager::make_key(std::string_view file_path)
{
	//	@tool 区切り文字/相対表記/大文字小文字の違いを吸収する
	//	@tool SJISの2バイト目に'\\'等が含まれるため、バイト単位ではなくワイド文字に変換してから正規化する
	std::wstring key;
	try
	{
		key = std::filesystem::path(std::string(file_path)).lexically_normal().generic_wstring();
	}
	catch (...)
	{
		key.reserve(file_path.size());
		for (char c : file_path)
		{
			key.push_back(static_cast<wchar_t>(static_cast<unsigned char>(c)));
		}
	}

	if (!key.empty())
		CharLowerBuffW(key.data(), static_cast<DWORD>(key.size()));
	return key;
}

size_t resource_manager::to_index(resource::load_priority priority)
{
	const size_t index = static_cast<size_t>(priority);
	return index < magic_enum::enum_count<resource::load_priority>() ? index : static_cast<size_t>(resource::load_priority::normal);
}

void resource_manager::request_load(const std::shared_ptr<resource::load_operation_base>& operation, resource::load_priority priority, bool force_load, loader_factory factory)
{
	const std::wstring key = make_key(operation->get_file_path());

	std::shared_ptr<void> cached;
	resource::operation_state immediate_state = resource::operation_state::idle;
	{
		std::lock_guard<std::mutex> lock(mutex);

		if (shutting_down)
		{
			immediate_state = resource::operation_state::canceled;
		}
		else if (!force_load)
		{
			//	@tool 読込済み
			auto cache_it = caches.find(key);
			if (cache_it != caches.end())
			{
				cached = cache_it->second.resource.lock();
				if (!cached)
				{
					caches.erase(cache_it);
				}
				else if (cache_it->second.type != operation->get_type())
				{
					cached.reset();
					immediate_state = resource::operation_state::type_mismatch;
				}
				else
				{
					immediate_state = resource::operation_state::success;
				}
			}

			//	@tool 読込中のリクエストに相乗り
			if (immediate_state == resource::operation_state::idle)
			{
				auto it = processing_requests.find(key);
				if (it != processing_requests.end() && !it->second->skipped)
				{
					if (it->second->type != operation->get_type())
					{
						immediate_state = resource::operation_state::type_mismatch;
					}
					else
					{
						join_request(it->second, operation, priority);
						return;
					}
				}
			}
		}

		//	@tool 新規リクエスト
		if (immediate_state == resource::operation_state::idle)
		{
			auto new_request = std::make_shared<request>(operation->get_file_path(), key, operation->get_type(), factory(), priority, force_load);
			join_request(new_request, operation, priority);
			pending_queues[to_index(priority)].push_back(new_request);
			processing_requests.insert_or_assign(key, std::move(new_request));
			processing_count.fetch_add(1, std::memory_order_acq_rel);
		}
	}

	if (immediate_state == resource::operation_state::idle)
	{
		work_condition.notify_one();
		return;
	}

	if (immediate_state == resource::operation_state::type_mismatch)
	{
		DEBUG_WARNING_CATEGORY(logger::categorys::system, "resource_manager type mismatch[%s]", std::string(operation->get_file_path()).c_str());
	}
	operation->complete(std::move(cached), immediate_state, immediate_state == resource::operation_state::type_mismatch ? "type mismatch" : "");
}

std::shared_ptr<void> resource_manager::search_raw(std::string_view file_path, std::type_index type)
{
	const std::wstring key = make_key(file_path);

	std::lock_guard<std::mutex> lock(mutex);
	auto it = caches.find(key);
	if (it == caches.end() || it->second.type != type)
		return nullptr;
	return it->second.resource.lock();
}

void resource_manager::join_request(const request_ptr& target, const std::shared_ptr<resource::load_operation_base>& operation, resource::load_priority priority)
{
	//	@tool 解放済みの待機者を掃除してから追加
	auto& waiters = target->waiters;
	waiters.erase(std::remove_if(waiters.begin(), waiters.end(), [](const std::weak_ptr<resource::load_operation_base>& waiter) { return waiter.expired(); }), waiters.end());
	waiters.emplace_back(operation);

	operation->set_processing();
	operation->request_link = target;

	//	@tool 未着手であれば優先度を引き上げる
	if (target->phase == request_phases::pending && to_index(priority) > to_index(target->priority))
	{
		if (remove_pending(target))
		{
			target->priority = priority;
			pending_queues[to_index(priority)].push_back(target);
		}
	}
}

bool resource_manager::has_live_waiters(const request& target) const
{
	for (auto& waiter : target.waiters)
	{
		auto operation = waiter.lock();
		if (operation && !operation->is_canceled())
			return true;
	}
	return false;
}

bool resource_manager::is_buildable(const request& target) const
{
	//	@tool 失敗/中断していれば依存を待たずに完了させる
	if (target.load_state != resource::operation_state::success || !has_live_waiters(target))
		return true;

	for (auto& dependency : target.loader->get_dependencies())
	{
		if (!dependency->is_done())
			return false;
	}
	return true;
}

bool resource_manager::remove_pending(const request_ptr& target)
{
	auto& queue = pending_queues[to_index(target->priority)];
	auto it = std::find(queue.begin(), queue.end(), target);
	if (it == queue.end())
		return false;
	queue.erase(it);
	return true;
}

void resource_manager::load_request(request& target)
{
	try
	{
		target.load_state = target.loader->load(target.file_path);
	}
	catch (const std::exception& exception)
	{
		target.load_state = resource::operation_state::deserialize_failed;
		target.error_message = exception.what();
	}
	catch (...)
	{
		target.load_state = resource::operation_state::deserialize_failed;
		target.error_message = "unknown exception";
	}

	if (target.error_message.empty())
		target.error_message = target.loader->get_error_message();
}

bool resource_manager::wait_request(const request_ptr& target)
{
	//	@tool 循環依存の検出
	if (std::find(waiting_requests.begin(), waiting_requests.end(), target.get()) != waiting_requests.end())
		return false;

	bool load_here = false;
	{
		std::unique_lock<std::mutex> lock(mutex);
		if (target->phase == request_phases::pending)
		{
			//	@tool ワーカーが未着手ならこのスレッドで読み込む
			remove_pending(target);
			target->phase = request_phases::loading;
			load_here = true;
		}
		else
		{
			if (target->phase == request_phases::loading)
			{
				loaded_condition.wait(lock, [&target] { return target->phase == request_phases::loaded; });
			}

			//	@tool 構築待ちから取り出す。見つからない場合は既に構築処理中
			auto it = std::find(loaded_requests.begin(), loaded_requests.end(), target);
			if (it == loaded_requests.end())
				return false;
			loaded_requests.erase(it);
		}
	}

	waiting_requests.push_back(target.get());

	if (load_here)
	{
		load_request(*target);
		std::lock_guard<std::mutex> lock(mutex);
		target->phase = request_phases::loaded;
	}

	//	@tool 依存リソースの完了待ち
	if (target->load_state == resource::operation_state::success)
	{
		for (auto& dependency : target->loader->get_dependencies())
		{
			if (!dependency->is_done())
				wait(*dependency);
		}
	}

	finalize(target);

	waiting_requests.pop_back();
	return true;
}

void resource_manager::finalize(const request_ptr& target)
{
	request& current = *target;

	resource::operation_state result = current.load_state;
	std::string message = current.error_message;
	std::shared_ptr<void> resource;

	bool wanted = false;
	{
		std::lock_guard<std::mutex> lock(mutex);
		wanted = has_live_waiters(current);

		//	@tool 読込中に同じリソースがキャッシュされた場合はそちらを優先する
		if (result == resource::operation_state::success && wanted && !current.force_load)
		{
			auto it = caches.find(current.key);
			if (it != caches.end())
			{
				std::shared_ptr<void> cached = it->second.resource.lock();
				if (cached)
				{
					if (it->second.type == current.type)
						resource = std::move(cached);
					else
						result = resource::operation_state::type_mismatch;
				}
			}
		}
	}

	if (result == resource::operation_state::success && !wanted)
		result = resource::operation_state::canceled;

	//	@tool 構築
	if (result == resource::operation_state::success && !resource)
	{
		for (auto& dependency : current.loader->get_dependencies())
		{
			if (!dependency->is_done())
			{
				result = resource::operation_state::dependency_failed;
				message = "dependency not completed";
				break;
			}
		}

		if (result == resource::operation_state::success)
		{
			try
			{
				result = current.loader->build();
				if (result == resource::operation_state::success)
				{
					resource = current.loader->get_resource();
					if (!resource)
						result = resource::operation_state::build_failed;
				}
			}
			catch (const std::exception& exception)
			{
				result = resource::operation_state::build_failed;
				message = exception.what();
			}
			catch (...)
			{
				result = resource::operation_state::build_failed;
				message = "unknown exception";
			}
		}

		if (result != resource::operation_state::success && message.empty())
			message = current.loader->get_error_message();
	}

	//	@tool キャッシュ登録と待機者の回収
	//	@tool 構築中に相乗りした待機者も含めるため、構築後にまとめて行う
	std::vector<std::shared_ptr<resource::load_operation_base>> waiters;
	{
		std::lock_guard<std::mutex> lock(mutex);
		if (result == resource::operation_state::success)
		{
			caches.insert_or_assign(current.key, cache_entry{ current.file_path, current.type, resource });
		}

		auto it = processing_requests.find(current.key);
		if (it != processing_requests.end() && it->second == target)
			processing_requests.erase(it);

		for (auto& waiter : current.waiters)
		{
			if (auto operation = waiter.lock())
				waiters.emplace_back(std::move(operation));
		}
		current.waiters.clear();
	}
	processing_count.fetch_sub(1, std::memory_order_acq_rel);

	if (result != resource::operation_state::success && result != resource::operation_state::canceled)
	{
		DEBUG_ERROR_CATEGORY(logger::categorys::system, "resource_manager load failed[%s][%s] %s", current.file_path.c_str(), resource::to_string(result), message.c_str());
	}

	for (auto& operation : waiters)
	{
		operation->complete(result == resource::operation_state::success ? resource : nullptr, result, message);
	}
}

void resource_manager::enqueue_dispatch(std::weak_ptr<resource::load_operation_base> operation)
{
	std::lock_guard<std::mutex> lock(mutex);
	dispatch_operations.emplace_back(std::move(operation));
}

void resource_manager::dispatch_callbacks()
{
	for (int pass = 0; pass < dispatch_pass_max; ++pass)
	{
		std::vector<std::weak_ptr<resource::load_operation_base>> operations;
		{
			std::lock_guard<std::mutex> lock(mutex);
			operations.swap(dispatch_operations);
		}
		if (operations.empty())
			break;

		for (auto& weak_operation : operations)
		{
			if (auto operation = weak_operation.lock())
				operation->invoke_callbacks();
		}
	}
}

void resource_manager::worker_loop()
{
	//	@tool WIC等を使う読込処理のためにCOMを初期化
	const HRESULT com_result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);

	for (;;)
	{
		request_ptr current;
		bool wanted = false;
		{
			std::unique_lock<std::mutex> lock(mutex);
			work_condition.wait(lock, [this]
			{
				if (shutting_down)
					return true;
				for (auto& queue : pending_queues)
				{
					if (!queue.empty())
						return true;
				}
				return false;
			});
			if (shutting_down)
				break;

			//	@tool 優先度の高い物から取り出す
			for (size_t index = magic_enum::enum_count<resource::load_priority>(); index-- > 0; )
			{
				auto& queue = pending_queues[index];
				if (!queue.empty())
				{
					current = std::move(queue.front());
					queue.pop_front();
					break;
				}
			}

			current->phase = request_phases::loading;
			wanted = has_live_waiters(*current);
			if (!wanted)
				current->skipped = true;
		}

		//	@tool 待機者が居なければ読込自体を省略する
		if (wanted)
			load_request(*current);
		else
			current->load_state = resource::operation_state::canceled;

		//	@tool 構築とローダーの破棄はメインスレッドで行うため、所有権ごと受け渡す
		{
			std::lock_guard<std::mutex> lock(mutex);
			current->phase = request_phases::loaded;
			loaded_requests.emplace_back(std::move(current));
		}
		loaded_condition.notify_all();
	}

	if (SUCCEEDED(com_result))
		CoUninitialize();
}

#if defined(USE_IMGUI)
void resource_manager::imgui_render()
{
	ImGui::Text("processing : %zu", get_processing_count());
	ImGui::DragFloat("build_time_budget(ms)", &build_time_budget, 0.1f, 0.0f, 100.0f, "%.1f");

	std::lock_guard<std::mutex> lock(mutex);

	if (ImGui::TreeNode("caches"))
	{
		for (auto& cache : caches)
		{
			const long use_count = cache.second.resource.use_count();
			if (use_count <= 0)
				continue;
			ImGui::Text("%s [%s] (use:%ld)", system_utility::convert_utf8(cache.second.file_path).data(), cache.second.type.name(), use_count);
		}
		ImGui::TreePop();
	}

	if (ImGui::TreeNode("requests"))
	{
		for (auto& processing : processing_requests)
		{
			auto& target = *processing.second;
			const char* phase_name = target.phase == request_phases::pending ? "pending" : target.phase == request_phases::loading ? "loading" : "loaded";
			ImGui::Text("%s [%s][%s] waiters:%zu", system_utility::convert_utf8(target.file_path).data(), phase_name, resource::to_string(target.priority), target.waiters.size());
		}
		ImGui::TreePop();
	}
}
#endif	//	defined(USE_IMGUI)
