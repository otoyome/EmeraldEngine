#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <source_location>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "System/system_utility.h"

namespace signals
{
	//	宛先型
	using address = core::hash_key;

	namespace detail
	{
		using type_id = const void*;
		template<typename... Types>

		struct type_list {};
		template<typename Type>

		type_id type_of()
		{
			static const char tag = 0;
			return &tag;
		}

		//	エントリー
		struct entry
		{
			uint64_t key = 0;
			std::function<void(const void* argument, void* result)> invoke;
			std::source_location location;	//	購読した場所
			std::atomic<bool> alive{ true };
			std::recursive_mutex call_mutex;

			//	生きていれば呼ぶ。呼んだら true
			bool call(const void* argument, void* result)
			{
				std::lock_guard<std::recursive_mutex> lock(call_mutex);
				if (!alive.load())
					return false;
				invoke(argument, result);
				return true;
			}

			//	以後は呼ばれないようにして、実行中の呼び出しがあれば終わるのを待つ
			void retire()
			{
				alive.store(false);
				std::lock_guard<std::recursive_mutex> lock(call_mutex);
			}
		};

		//	スロット
		struct slot
		{
			std::string_view name;	//	文字列リテラルを指す
			type_id type = nullptr;
			std::vector<std::shared_ptr<entry>> entries;	//	登録した順に呼ぶ
		};

		class registry;
	}

	//	接続オブジェクト
	class connection
	{
	public:
		connection() = default;
		connection(std::weak_ptr<detail::registry> owner, address target, uint64_t key);
		connection(const connection&) = delete;
		connection& operator=(const connection&) = delete;
		connection(connection&& other) noexcept;
		connection& operator=(connection&& other) noexcept;
		~connection();

		void disconnect();
		bool is_connected() const;

	private:
		std::weak_ptr<detail::registry> owner;
		address target = 0;
		uint64_t key = 0;
	};

	//	接続オブジェクトをまとめて持つ
	class connection_list
	{
	public:
		connection_list& operator+=(connection&& value)
		{
			connections.push_back(std::move(value));
			return *this;
		}
		void disconnect_all() { connections.clear(); }

	private:
		std::vector<connection> connections;
	};

	namespace detail
	{
		//	宛先と受け手の表。broadcaster と contactor の共通の土台
		class registry
		{
		public:
			static constexpr uint32_t max_depth = 16;

			//	入れ子の深さを数える（スレッドごと）
			class depth_guard
			{
			public:
				depth_guard();
				~depth_guard();
				depth_guard(const depth_guard&) = delete;
				depth_guard& operator=(const depth_guard&) = delete;
				explicit operator bool() const { return entered; }

			private:
				bool entered;
			};

		public:
			registry();

			//	受け手の写しを取る。型の違う同名の宛先なら false
			bool snapshot(address target, std::string_view name, type_id type, std::vector<std::shared_ptr<entry>>& out);
			void remove(address target, uint64_t key);

			static connection connect(const std::shared_ptr<registry>& self, address target, std::string_view name, type_id type,
				std::function<void(const void*, void*)> invoke, std::source_location location, bool exclusive);

		private:
			//	mutex を持った状態で呼ぶこと
			slot* find_or_add_locked(address target, std::string_view name, type_id type);

		private:
			std::mutex mutex;
			std::unordered_map<address, slot> slots;
			uint64_t next_key = 1;
		};
	}

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//	通知の宛先指定
	template<typename Payload = void>
	struct topic
	{
		//	載せるのは構造体1つか、何も載せない（void）
		static_assert(std::is_void_v<Payload> || (std::is_class_v<Payload> && std::is_copy_constructible_v<Payload>),
			"topic に載せるのはコピーできる構造体 1 つ");

		constexpr explicit topic(std::string_view name) : name(name), target(core::generate_hash(name)) {}

		std::string_view name;
		address target;
	};

	//	配信クラス
	class broadcaster
	{
	public:
		//	dispatch_posted で、配っている最中に積まれた分を同じフレームで回す回数
		static constexpr uint32_t max_post_rounds = 8;

	public:
		broadcaster();
		~broadcaster();
		broadcaster(const broadcaster&) = delete;
		broadcaster& operator=(const broadcaster&) = delete;

		//	予約配信（どのスレッドからでも積める）
		template<typename Payload>
		void post(const topic<Payload>& target, const std::type_identity_t<Payload>& payload)
		{
			push_posted([this, target, payload]() { publish(target, payload); });
		}
		void post(const topic<void>& target)
		{
			push_posted([this, target]() { publish(target); });
		}

		//	即時配信
		template<typename Payload>
		void publish(const topic<Payload>& target, const std::type_identity_t<Payload>& payload)
		{
			dispatch(target.target, target.name, detail::type_of<detail::type_list<Payload>>(), &payload);
		}
		void publish(const topic<void>& target)
		{
			dispatch(target.target, target.name, detail::type_of<detail::type_list<void>>(), nullptr);
		}

		//	予約配信処理（呼んだスレッドの上で配る）
		void dispatch_posted();

		//	配信購読
		template<typename Payload, typename Function>
		[[nodiscard]] connection subscribe(const topic<Payload>& target, Function&& function,
			std::source_location location = std::source_location::current())
		{
			std::function<void(const void*, void*)> invoke;
			if constexpr (std::is_void_v<Payload>)
			{
				static_assert(std::is_invocable_v<Function&>, "受け手は引数なしで呼べること");
				invoke = [function = std::forward<Function>(function)](const void*, void*) mutable { function(); };
			}
			else
			{
				static_assert(std::is_invocable_v<Function&, const Payload&>, "受け手は const Payload& で呼べること");
				invoke = [function = std::forward<Function>(function)](const void* argument, void*) mutable
				{
					function(*static_cast<const Payload*>(argument));
				};
			}
			return detail::registry::connect(core, target.target, target.name,
				detail::type_of<detail::type_list<Payload>>(), std::move(invoke), location, false);
		}

	private:
		void dispatch(address target, std::string_view name, detail::type_id type, const void* payload);
		void push_posted(std::function<void()> item);

	private:
		std::shared_ptr<detail::registry> core;	//	構築後は変わらない
		std::mutex posted_mutex;
		std::vector<std::function<void()>> posted;	//	posted_mutex で保護
	};

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//	複数の回答者
	struct single {};	//	回答者は一人のはず(複数ある場合はアサート)
	struct all_of {};	//	全員が true なら true
	struct any_of {};	//	誰かが true なら true
	struct collect {};	//	全員の答えを集める

	//	問い合わせの宛先指定
	template<typename Result, typename Request = void, typename Policy = single>
	struct inquiry
	{
		//	Result : 回答の型
		//	Request : 問い合わせ時に渡すデータ
		//	Policy : 複数の回答者をどう扱うか
		static_assert(!std::is_void_v<Result>, "答えの無いものは topic を使う");
		static_assert(std::is_void_v<Request> || std::is_class_v<Request>, "問い合わせに載せるのは構造体 1 つ");
		static_assert(!(std::is_same_v<Policy, all_of> || std::is_same_v<Policy, any_of>) || std::is_same_v<Result, bool>,
			"all_of / any_of は答えが bool");

		constexpr explicit inquiry(std::string_view name) : name(name), target(core::generate_hash(name)) {}

		std::string_view name;
		address target;
	};

	//	問い合わせと回答の取得
	class contactor
	{
	public:
		contactor();
		~contactor() = default;
		contactor(const contactor&) = delete;
		contactor& operator=(const contactor&) = delete;

		//	問い合わせ
		//	single = std::optional<Result>
		//	all_of / any_of = std::optional<bool>
		//	collect = std::vector<Result>
		template<typename Result, typename Request, typename Policy>
		auto ask(const inquiry<Result, Request, Policy>& target, const std::type_identity_t<Request>& request)
		{
			return ask_core(target, &request);
		}
		template<typename Result, typename Policy>
		auto ask(const inquiry<Result, void, Policy>& target)
		{
			return ask_core(target, nullptr);
		}

		//	答える側の登録
		template<typename Result, typename Request, typename Policy, typename Function>
		[[nodiscard]] connection answer(const inquiry<Result, Request, Policy>& target, Function&& function,
			std::source_location location = std::source_location::current())
		{
			std::function<void(const void*, void*)> invoke;
			if constexpr (std::is_void_v<Request>)
			{
				static_assert(std::is_invocable_r_v<Result, Function&>, "答える側は引数なしで Result を返すこと");
				invoke = [function = std::forward<Function>(function)](const void*, void* result) mutable
				{
					static_cast<std::optional<Result>*>(result)->emplace(function());
				};
			}
			else
			{
				static_assert(std::is_invocable_r_v<Result, Function&, const Request&>, "答える側は const Request& を受けて Result を返すこと");
				invoke = [function = std::forward<Function>(function)](const void* argument, void* result) mutable
				{
					static_cast<std::optional<Result>*>(result)->emplace(function(*static_cast<const Request*>(argument)));
				};
			}
			return detail::registry::connect(core, target.target, target.name, type_of_inquiry<Result, Request, Policy>(),
				std::move(invoke), location, std::is_same_v<Policy, single>);
		}

	private:
		template<typename Result, typename Request, typename Policy>
		static detail::type_id type_of_inquiry() { return detail::type_of<detail::type_list<Result, Request, Policy>>(); }

		template<typename Result, typename Request, typename Policy>
		auto ask_core(const inquiry<Result, Request, Policy>& target, const void* request)
		{
			const std::shared_ptr<detail::registry> keep_alive = core;
			std::vector<std::shared_ptr<detail::entry>> receivers;
			keep_alive->snapshot(target.target, target.name, type_of_inquiry<Result, Request, Policy>(), receivers);
			detail::registry::depth_guard guard;

			if constexpr (std::is_same_v<Policy, collect>)
			{
				std::vector<Result> results;
				if (!guard)
					return results;
				for (const auto& receiver : receivers)
				{
					std::optional<Result> answer;
					if (!receiver->call(request, &answer))
						continue;
					if (answer)
						results.push_back(std::move(*answer));
				}
				return results;
			}
			else if constexpr (std::is_same_v<Policy, single>)
			{
				std::optional<Result> answer;
				if (!guard)
					return answer;
				for (const auto& receiver : receivers)
				{
					if (receiver->call(request, &answer))
						break;
				}
				return answer;
			}
			else
			{
				//	all_of は false、any_of は true が出た時点で打ち切る
				constexpr bool stop_value = std::is_same_v<Policy, any_of>;
				std::optional<bool> result;
				if (!guard)
					return result;
				for (const auto& receiver : receivers)
				{
					std::optional<bool> answer;
					if (!receiver->call(request, &answer))
						continue;
					if (!answer)
						continue;
					result = *answer;
					if (*answer == stop_value)
						break;
				}
				return result;
			}
		}

	private:
		std::shared_ptr<detail::registry> core;	//	構築後は変わらない
	};
}