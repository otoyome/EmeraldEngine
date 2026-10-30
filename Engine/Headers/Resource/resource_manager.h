#pragma once

#include <array>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <typeindex>
#include <unordered_map>
#include <vector>

#include "system/singleton.h"

#include "Resource/resource_operation_state.h"
#include "Resource/resource_operation.h"
#include "Resource/resource_loader.h"

//	リソース管理クラス
class resource_manager
{
	SINGLETON(resource_manager)

	friend class resource::load_operation_base;

public:
	static constexpr unsigned worker_count = 2;	//	ワーカースレッド数
	static constexpr float default_build_time_budget = 4.0f;	//	1フレームあたりの構築処理の時間(ミリ秒)。0以下で無制限
	static constexpr unsigned expired_cache_cleanup_interval = 120;	//	解放済みキャッシュ情報を掃除する間隔(フレーム)

private:
	resource_manager();
	~resource_manager();

public:	//	メインスレッド用関数
	//	
	void update();

	//	同期読込(メインスレッド専用)
	template<class Type, class Loader = resource::loader<Type>>
	[[nodiscard]] resource::load_operation_ptr<Type> load(std::string_view file_path, bool force_load = false)
	{
		auto operation = load_async<Type, Loader>(file_path, resource::load_priority::high, force_load);
		wait(*operation);
		return operation;
	}

	//	指定の操作が完了するまで待つ(メインスレッド専用)
	bool wait(resource::load_operation_base& operation);

public:
	//	非同期読込
	template<class Type, class Loader = resource::loader<Type>>
	[[nodiscard]] resource::load_operation_ptr<Type> load_async(std::string_view file_path, resource::load_priority priority = resource::load_priority::normal, bool force_load = false)
	{
		auto operation = std::make_shared<resource::load_operation<Type>>(file_path);
		request_load(operation, priority, force_load, &create_loader<Loader>);
		return operation;
	}

	//	読込済みリソースの検索
	template<class Type>
	[[nodiscard]] std::shared_ptr<Type> search(std::string_view file_path)
	{
		return std::static_pointer_cast<Type>(search_raw(file_path, std::type_index(typeid(Type))));
	}

	//	1フレームあたりの構築処理の時間(ミリ秒)。
	//	0以下で無制限。最低1件は必ず構築する
	void set_build_time_budget(float milliseconds) { build_time_budget = milliseconds; }
	float get_build_time_budget() const { return build_time_budget; }

	//	読込待ち/読込中/構築待ちのリクエスト数
	size_t get_processing_count() const { return processing_count.load(std::memory_order_acquire); }

	//	解放済みリソースのキャッシュ情報を削除
	void remove_expired_caches();

	bool is_main_thread() const { return std::this_thread::get_id() == main_thread_id; }

#if defined(USE_IMGUI)
	void imgui_render();
#endif	//	defined(USE_IMGUI)

private:
	using loader_factory = std::unique_ptr<resource::loader_base>(*)();

	template<class Loader>
	static std::unique_ptr<resource::loader_base> create_loader()
	{
		return std::make_unique<Loader>();
	}

	enum class request_phases
	{
		pending,	//	ワーカーの読込待ち
		loading,	//	読込中
		loaded,		//	読込完了(構築待ち)
	};

	//	同じパス/型の要求をまとめた読込リクエスト
	struct request
	{
		std::string file_path;
		std::wstring key;
		std::type_index type;
		std::unique_ptr<resource::loader_base> loader;
		std::vector<std::weak_ptr<resource::load_operation_base>> waiters;
		resource::load_priority priority;
		bool force_load;

		//	以下はミューテックスで保護
		request_phases phase = request_phases::pending;
		bool skipped = false;

		//	以下は読込を行ったスレッドが書き込み、phaseがloadedになった後に参照する
		resource::operation_state load_state = resource::operation_state::processing;
		std::string error_message;

		request(std::string_view file_path, std::wstring key, std::type_index type, std::unique_ptr<resource::loader_base> loader, resource::load_priority priority, bool force_load)
		: file_path(file_path)
		, key(std::move(key))
		, type(type)
		, loader(std::move(loader))
		, priority(priority)
		, force_load(force_load)
		{
		}
	};
	using request_ptr = std::shared_ptr<request>;

	struct cache_entry
	{
		std::string file_path;
		std::type_index type;
		std::weak_ptr<void> resource;
	};

private:
	static std::wstring make_key(std::string_view file_path);
	static size_t to_index(resource::load_priority priority);

	void request_load(const std::shared_ptr<resource::load_operation_base>& operation, resource::load_priority priority, bool force_load, loader_factory factory);
	std::shared_ptr<void> search_raw(std::string_view file_path, std::type_index type);

	void join_request(const request_ptr& target, const std::shared_ptr<resource::load_operation_base>& operation, resource::load_priority priority);
	bool has_live_waiters(const request& target) const;
	bool is_buildable(const request& target) const;
	bool remove_pending(const request_ptr& target);

	void load_request(request& target);
	bool wait_request(const request_ptr& target);
	void finalize(const request_ptr& target);

	void enqueue_dispatch(std::weak_ptr<resource::load_operation_base> operation);
	void dispatch_callbacks();

	void worker_loop();

private:
	std::thread::id main_thread_id;
	mutable std::mutex mutex;
	std::condition_variable work_condition;		//	ワーカーへの通知
	std::condition_variable loaded_condition;	//	読込完了の通知(wait用)

	std::unordered_map<std::wstring, cache_entry> caches;
	std::unordered_map<std::wstring, request_ptr> processing_requests;	//	同一のリクエスト
	std::array<std::deque<request_ptr>, magic_enum::enum_count<resource::load_priority>()> pending_queues;
	std::vector<request_ptr> loaded_requests;
	std::vector<std::weak_ptr<resource::load_operation_base>> dispatch_operations;
	bool shutting_down = false;

	//	以下はメインスレッドのみで触る
	std::vector<const request*> waiting_requests;	//	wait()中のリクエスト
	float build_time_budget = default_build_time_budget;
	unsigned frame_count = 0;

	std::atomic<size_t> processing_count{ 0 };
	std::vector<std::thread> workers;
};
