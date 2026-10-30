#include "Resource/resource_operation.h"
#include "Resource/resource_manager.h"

namespace resource
{
	load_operation_base::load_operation_base(std::string_view file_path, std::type_index type)
	: file_path(file_path)
	, type(type)
	{
	}

	std::shared_ptr<void> load_operation_base::get_raw_resource() const
	{
		std::lock_guard<std::mutex> lock(mutex);
		return resource;
	}

	std::string load_operation_base::get_error_message() const
	{
		std::lock_guard<std::mutex> lock(mutex);
		return error_message;
	}

	void load_operation_base::cancel()
	{
		std::lock_guard<std::mutex> lock(mutex);
		if (is_done())
			return;

		state.store(operation_state::canceled, std::memory_order_release);
		callbacks.clear();
	}

	void load_operation_base::remove_completed_callback(callback_id id)
	{
		std::lock_guard<std::mutex> lock(mutex);
		for (auto it = callbacks.begin(); it != callbacks.end(); ++it)
		{
			if (it->first == id)
			{
				callbacks.erase(it);
				return;
			}
		}
	}

	void load_operation_base::clear_completed_callbacks()
	{
		std::lock_guard<std::mutex> lock(mutex);
		callbacks.clear();
	}

	load_operation_base::callback_id load_operation_base::add_raw_completed_callback(raw_callback callback)
	{
		callback_id id = invalid_callback_id;
		bool need_dispatch = false;
		{
			std::lock_guard<std::mutex> lock(mutex);
			if (is_canceled())
				return invalid_callback_id;

			id = ++next_callback_id;
			callbacks.emplace_back(id, std::move(callback));

			//	完了済みの場合は次のupdate()で呼び出す
			if (is_done() && !dispatch_queued)
			{
				dispatch_queued = true;
				need_dispatch = true;
			}
		}

		if (need_dispatch)
			request_dispatch();
		return id;
	}

	void load_operation_base::set_processing()
	{
		operation_state expected = operation_state::idle;
		state.compare_exchange_strong(expected, operation_state::processing, std::memory_order_acq_rel);
	}

	void load_operation_base::complete(std::shared_ptr<void> result, operation_state result_state, std::string message)
	{
		bool need_dispatch = false;
		{
			std::lock_guard<std::mutex> lock(mutex);

			//	中断/完了済みの場合は結果を受け取らない
			if (is_done())
				return;

			resource = std::move(result);
			error_message = std::move(message);
			state.store(result_state, std::memory_order_release);

			if (!callbacks.empty() && !dispatch_queued)
			{
				dispatch_queued = true;
				need_dispatch = true;
			}
		}

		if (need_dispatch)
			request_dispatch();
	}

	void load_operation_base::invoke_callbacks()
	{
		std::vector<std::pair<callback_id, raw_callback>> invoke_list;
		{
			std::lock_guard<std::mutex> lock(mutex);
			dispatch_queued = false;
			if (!is_done() || is_canceled())
				return;
			invoke_list.swap(callbacks);
		}

		//	コールバック内で読込要求やコールバック登録を行えるようにロック外で呼び出す
		for (auto& callback : invoke_list)
		{
			callback.second(*this);
		}
	}

	void load_operation_base::request_dispatch()
	{
		if (resource_manager::is_valid())
			resource_manager::instance()->enqueue_dispatch(weak_from_this());
	}
}	//	namespace resource
