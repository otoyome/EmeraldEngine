#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <typeindex>
#include <utility>
#include <vector>

#include "Resource/resource_operation_state.h"

class resource_manager;

namespace resource
{
	//	@tool リソース読込操作(要求者ごとに1つ生成される)
	//	@tool ・状態/リソースの取得、中断、コールバックの登録/解除はどのスレッドからでも行える
	//	@tool ・完了コールバックは必ずresource_manager::update()内(メインスレッド)で呼ばれる(登録時に完了済みでもその場では呼ばれない)
	//	@tool ・操作を破棄するとその要求は中断扱いになり、コールバックも呼ばれない
	//	@tool   コールバックで参照するオブジェクトと同じ寿命で保持すること
	class load_operation_base : public std::enable_shared_from_this<load_operation_base>
	{
		friend class ::resource_manager;

	public:
		using callback_id = size_t;
		static constexpr callback_id invalid_callback_id = 0;

	public:
		operation_state get_state() const { return state.load(std::memory_order_acquire); }
		bool is_done() const { return is_finished(get_state()); }
		bool is_succeeded() const { return get_state() == operation_state::success; }
		bool is_canceled() const { return get_state() == operation_state::canceled; }

		std::string_view get_file_path() const { return file_path; }
		std::type_index get_type() const { return type; }

		std::shared_ptr<void> get_raw_resource() const;
		std::string get_error_message() const;

		//	@tool この要求のみ中断する(同じリソースを待っている他の要求には影響しない)
		//	@tool 完了前であれば状態はcanceledになり、登録済みのコールバックは呼ばれない
		void cancel();

		void remove_completed_callback(callback_id id);
		void clear_completed_callbacks();

	public:
		load_operation_base(std::string_view file_path, std::type_index type);
		virtual ~load_operation_base() = default;

		load_operation_base(const load_operation_base&) = delete;
		load_operation_base& operator=(const load_operation_base&) = delete;

	protected:
		using raw_callback = std::function<void(load_operation_base&)>;

		callback_id add_raw_completed_callback(raw_callback callback);

	private:
		//	@tool 以下はresource_managerから呼ばれる
		void set_processing();
		void complete(std::shared_ptr<void> result, operation_state result_state, std::string message);
		void invoke_callbacks();
		void request_dispatch();

	private:
		const std::string file_path;
		const std::type_index type;

		std::atomic<operation_state> state{ operation_state::idle };

		mutable std::mutex mutex;
		std::shared_ptr<void> resource;
		std::string error_message;
		std::vector<std::pair<callback_id, raw_callback>> callbacks;
		callback_id next_callback_id = invalid_callback_id;
		bool dispatch_queued = false;

		//	@tool 待機中のリクエスト(resource_managerのミューテックスで保護)
		std::weak_ptr<void> request_link;
	};

	//	@tool 型付きのリソース読込操作
	template<class Type>
	class load_operation : public load_operation_base
	{
	public:
		using callback = std::function<void(load_operation<Type>&)>;

	public:
		std::shared_ptr<Type> get_resource() const
		{
			return std::static_pointer_cast<Type>(get_raw_resource());
		}

		//	@tool 完了時(成功/失敗)に1度だけ呼ばれるコールバックを登録する
		//	@tool 中断済み/中断された場合は呼ばれない
		callback_id add_completed_callback(callback function)
		{
			return add_raw_completed_callback(
				[function = std::move(function)](load_operation_base& operation)
				{
					function(static_cast<load_operation<Type>&>(operation));
				});
		}

	public:
		explicit load_operation(std::string_view file_path)
		: load_operation_base(file_path, std::type_index(typeid(Type)))
		{
		}
		~load_operation() override = default;
	};

	template<class Type>
	using load_operation_ptr = std::shared_ptr<load_operation<Type>>;
}	//	namespace resource
