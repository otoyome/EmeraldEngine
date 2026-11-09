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
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <vector>

#include "system/singleton.h"

#include "Resource/resource_operation_state.h"
#include "Resource/resource_operation.h"
#include "Resource/resource_loader.h"
#include "Resource/resource_saver.h"
#include "Resource/asset_reference.h"
#include "Resource/asset_registry.h"
#include "Resource/resource_base.h"

//	リソース管理クラス
class resource_manager
{
	SINGLETON(resource_manager)

	friend class resource::load_operation_base;

public:
	static constexpr unsigned worker_count = 2;	//	ワーカースレッド数
	static constexpr float default_build_time_budget = 4.0f;	//	1フレームあたりの構築処理の時間(ミリ秒)。0以下で無制限
	static constexpr unsigned expired_cache_cleanup_interval = 120;	//	解放済みキャッシュ情報を掃除する間隔(フレーム)

public:
	//	1フレームあたりの構築処理の時間(ミリ秒)
	void set_build_time_budget(float milliseconds) { build_time_budget = milliseconds; }
	float get_build_time_budget() const { return build_time_budget; }

	//	読込待ち/読込中/構築待ちのリクエスト数
	size_t get_processing_count() const { return processing_count.load(std::memory_order_acquire); }

	//	解放済みリソースのキャッシュ情報を削除
	void remove_expired_caches();

	//	呼び出したスレッドがメインスレッドか判定
	bool is_main_thread() const { return std::this_thread::get_id() == main_thread_id; }

private:
	resource_manager();
	~resource_manager();

public:	//	メインスレッド用関数
	//	
	void update();

	//	指定の操作が完了するまで待つ
	bool wait(resource::load_operation_base& operation);

	//	同期読込
	template<class Type, class Loader = resource::loader<Type>>
	[[nodiscard]] resource::load_operation_ptr<Type> load(std::string_view file_path, bool force_load = false)
	{
		auto operation = load_async<Type, Loader>(file_path, resource::load_priority::high, force_load);
		wait(*operation);
		return operation;
	}

	//	同期保存
	template<class Type, class Saver = resource::saver<Type>>
	bool save(const std::shared_ptr<Type>& value, std::string_view file_path)
	{
		static_assert(std::is_base_of_v<resource::resource_base, Type>, "Type must derive from resource::resource_base");
		if (!value || file_path.empty())
			return false;

		const std::string path(file_path);
		Saver saver;
		if (!saver.save(*value, std::string_view(path)))
			return false;

		register_resource<Type>(path, value);
		return true;
	}

	template<class Type, class Saver = resource::saver<Type>>
	bool save(const std::shared_ptr<Type>& value)
	{
		return value && save<Type, Saver>(value, value->get_file_path());
	}

public:
	//	非同期読込
	template<class Type, class Loader = resource::loader<Type>>
	[[nodiscard]] resource::load_operation_ptr<Type> load_async(std::string_view file_path, resource::load_priority priority = resource::load_priority::normal, bool force_load = false)
	{
		auto operation = std::make_shared<resource::load_operation<Type>>(file_path);
		request_load(operation, priority, force_load, &create_loader<Loader>, &assign_file_path<Type>);
		return operation;
	}

	template<class Type, class Loader = resource::loader<Type>>
	[[nodiscard]] resource::load_operation_ptr<Type> load_async(const resource::asset_reference<Type>& reference, resource::load_priority priority = resource::load_priority::normal, bool force_load = false)
	{
		if (reference.is_empty())
			return nullptr;

		const std::string file_path = asset_registry::is_valid() ? asset_registry::instance()->resolve_path(reference.uuid, reference.path) : reference.path;
		return load_async<Type, Loader>(std::string_view(file_path), priority, force_load);
	}

	//	読込結果取得
	template<class Type, class Loader = resource::loader<Type>, class... Args>
	[[nodiscard]] std::shared_ptr<Type> resolve(const resource::load_operation_ptr<Type>& operation, const resource::asset_reference<Type>& reference, Args&&... args)
	{
		if (operation && operation->is_succeeded())
		{
			if (auto value = operation->get_resource())
				return value;
		}
		return Loader::create_fallback(reference, std::forward<Args>(args)...);
	}

	//	Loader::revert()があるリソースのみ、読み込み済みのリソースをファイルの内容に戻す
	template<class Type, class Loader = resource::loader<Type>>
	bool revert(Type& target)
	{
		static_assert(std::is_base_of_v<resource::resource_base, Type>, "Type must derive from resource::resource_base");
		const std::string file_path(target.get_file_path());
		if (!is_main_thread() || file_path.empty())
			return false;

		Loader loader;
		if (loader.load(file_path) != resource::operation_state::success)
			return false;
		for (auto& dependency : loader.get_dependencies())
			wait(*dependency);
		return loader.revert(target);
	}

	//	キャッシュに載せずに読み込む
	template<class Type, class Loader = resource::loader<Type>>
	std::shared_ptr<Type> load_uncached(std::string_view file_path)
	{
		static_assert(std::is_base_of_v<resource::resource_base, Type>, "Type must derive from resource::resource_base");
		if (!is_main_thread() || file_path.empty())
			return nullptr;

		Loader loader;
		if (loader.load(file_path) != resource::operation_state::success)
			return nullptr;
		for (auto& dependency : loader.get_dependencies())
			wait(*dependency);
		if (loader.build() != resource::operation_state::success)
			return nullptr;

		auto value = std::static_pointer_cast<Type>(loader.get_resource());
		if (value)
			assign_file_path<Type>(value.get(), file_path);
		return value;
	}

public:
	//	読込済みリソースの検索
	template<class Type>
	[[nodiscard]] std::shared_ptr<Type> search(std::string_view file_path)
	{
		return std::static_pointer_cast<Type>(search_raw(file_path, std::type_index(typeid(Type))));
	}

	//	読込以外で作ったリソースを登録
	template<class Type>
	void register_resource(std::string_view file_path, const std::shared_ptr<Type>& value)
	{
		register_raw(file_path, std::type_index(typeid(Type)), value, &assign_file_path<Type>);
	}

public:
	//	ファイルの移動
	//	対応表(asset_registry)は先に更新しておくこと
	void move_path(std::string_view old_path, std::string_view new_path);

public:
	//	path が base_path と同じか、その下にあれば、base_path の部分を new_base_path に置き換える
	static bool replace_base_path(std::string_view path, std::string_view base_path, std::string_view new_base_path, std::string& out_path);

	static std::wstring make_path_key(std::string_view file_path);

#if defined(USE_IMGUI)
	void imgui_render();
#endif	//	defined(USE_IMGUI)

private:
	using loader_factory = std::unique_ptr<resource::loader_base>(*)();
	using path_assigner = void(*)(void*, std::string_view);

	template<class Loader>
	static std::unique_ptr<resource::loader_base> create_loader()
	{
		return std::make_unique<Loader>();
	}

	//	型を消したリソースへパスを設定
	template<class Type>
	static void assign_file_path(void* value, std::string_view file_path)
	{
		if constexpr (std::is_base_of_v<resource::resource_base, Type>)
			static_cast<Type*>(value)->set_file_path(file_path);
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
		path_assigner assign_path;

		//	以下はミューテックスで保護
		request_phases phase = request_phases::pending;
		bool skipped = false;

		//	以下は読込を行ったスレッドが書き込み、phaseがloadedになった後に参照する
		resource::operation_state load_state = resource::operation_state::processing;
		std::string error_message;

		request(std::string_view file_path, std::wstring key, std::type_index type, std::unique_ptr<resource::loader_base> loader, resource::load_priority priority, bool force_load, path_assigner assign_path)
		: file_path(file_path)
		, key(std::move(key))
		, type(type)
		, loader(std::move(loader))
		, priority(priority)
		, force_load(force_load)
		, assign_path(assign_path)
		{
		}
	};
	using request_ptr = std::shared_ptr<request>;

	struct cache_entry
	{
		std::string file_path;
		std::type_index type;
		std::weak_ptr<void> resource;
		path_assigner assign_path = nullptr;
	};

private:
	static std::wstring make_key(std::string_view file_path);
	static size_t to_index(resource::load_priority priority);

	void request_load(const std::shared_ptr<resource::load_operation_base>& operation, resource::load_priority priority, bool force_load, loader_factory factory, path_assigner assign_path);
	std::shared_ptr<void> search_raw(std::string_view file_path, std::type_index type);
	void register_raw(std::string_view file_path, std::type_index type, std::shared_ptr<void> value, path_assigner assign_path);

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
