#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <source_location>
#include <string_view>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "System/system_utility.h"

//	@tool シグナル：通知（topic）と問い合わせ（inquiry）で、送る側と受ける側が互いを知らずにやり取りする
namespace signals
{
	//	@tool 宛先のハッシュ（名前の文字列から作る）
	using address = system_utility::hash_key;

	namespace detail
	{
		//	@tool 型ごとの印（同じプロセスの中でだけ一意）
		using type_id = const void*;
		template<typename... Types>
		struct type_list {};
		template<typename Type>
		type_id type_of()
		{
			static const char tag = 0;
			return &tag;
		}

		//	@tool 受け手 1 つ分
		struct entry
		{
			uint64_t key = 0;
			std::function<void(const void* argument, void* result)> invoke;	//	@tool 引数と答えは型を消して渡す
			std::source_location location;										//	@tool 購読した場所
			bool alive = true;
		};

		//	@tool 宛先 1 つ分
		struct slot
		{
			std::string_view name;							//	@tool 文字列リテラルを指す
			type_id type = nullptr;
			std::vector<std::shared_ptr<entry>> entries;	//	@tool 登録した順に呼ぶ
		};

		class registry;
	}

	//	@tool 購読の解除役。破棄されると購読が外れる
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
		std::weak_ptr<detail::registry> owner;	//	@tool 送る側が先に消えても安全
		address target = 0;
		uint64_t key = 0;
	};

	//	@tool connection をまとめて持つ。持ち主のメンバーの最後に置く（最初に外れる）
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
		//	@tool 宛先と受け手の表。broadcaster と inquiry_desk の共通の土台
		class registry
		{
		public:
			static constexpr uint32_t max_depth = 16;

			//	@tool 入れ子の深さを数える
			class depth_guard
			{
			public:
				explicit depth_guard(registry& owner);
				~depth_guard();
				depth_guard(const depth_guard&) = delete;
				depth_guard& operator=(const depth_guard&) = delete;
				explicit operator bool() const { return entered; }

			private:
				registry& owner;
				bool entered;
			};

		public:
			registry();

			//	@tool 型の違う同名の宛先なら nullptr
			slot* find_or_add(address target, std::string_view name, type_id type);
			std::vector<std::shared_ptr<entry>> snapshot(const slot& target) const;
			void remove(address target, uint64_t key);
			void check_thread() const;

			static connection connect(const std::shared_ptr<registry>& self, address target, std::string_view name, type_id type,
				std::function<void(const void*, void*)> invoke, std::source_location location, bool exclusive);

		private:
			std::unordered_map<address, slot> slots;
			uint64_t next_key = 1;
			uint32_t depth = 0;
			std::thread::id owner_thread;
		};
	}

	//////////////////////////////////////////////////////////////////////////
	//	@tool 通知
	//////////////////////////////////////////////////////////////////////////

	//	@tool 通知の宛先。載せるのは構造体 1 つか、何も載せない（void）
	template<typename Payload = void>
	struct topic
	{
		static_assert(std::is_void_v<Payload> || (std::is_class_v<Payload> && std::is_copy_constructible_v<Payload>),
			"topic に載せるのはコピーできる構造体 1 つ");

		constexpr explicit topic(std::string_view name) : name(name), target(system_utility::generate_hash(name)) {}

		std::string_view name;
		address target;
	};

	class broadcaster
	{
	public:
		//	@tool dispatch_posted で、配っている最中に積まれた分を同じフレームで回す回数
		static constexpr uint32_t max_post_rounds = 8;

	public:
		broadcaster();
		~broadcaster();
		broadcaster(const broadcaster&) = delete;
		broadcaster& operator=(const broadcaster&) = delete;

		//	@tool 後で配る：dispatch_posted でまとめて配る（既定はこちら）
		template<typename Payload>
		void post(const topic<Payload>& target, const std::type_identity_t<Payload>& payload)
		{
			posted.push_back([this, target, payload]() { publish(target, payload); });
		}
		void post(const topic<void>& target)
		{
			posted.push_back([this, target]() { publish(target); });
		}

		//	@tool すぐに配る：その宛先の受け手を登録順に全て呼ぶ
		template<typename Payload>
		void publish(const topic<Payload>& target, const std::type_identity_t<Payload>& payload)
		{
			dispatch(target.target, target.name, detail::type_of<detail::type_list<Payload>>(), &payload);
		}
		void publish(const topic<void>& target)
		{
			dispatch(target.target, target.name, detail::type_of<detail::type_list<void>>(), nullptr);
		}

		//	@tool post で積んだ分を配る。持ち主が毎フレーム呼ぶ
		void dispatch_posted();

		//	@tool 購読
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

	private:
		std::shared_ptr<detail::registry> core;
		std::vector<std::function<void()>> posted;
	};

	//////////////////////////////////////////////////////////////////////////
	//	@tool 問い合わせ
	//////////////////////////////////////////////////////////////////////////

	//	@tool 答える相手が複数いる時のまとめ方
	struct single {};	//	@tool 答えるのは 1 つだけ（2 つ目は assert）
	struct all_of {};	//	@tool 全員が true なら true
	struct any_of {};	//	@tool 誰かが true なら true
	struct collect {};	//	@tool 全員の答えを集める

	//	@tool 問い合わせの宛先
	template<typename Result, typename Request = void, typename Policy = single>
	struct inquiry
	{
		static_assert(!std::is_void_v<Result>, "答えの無いものは topic を使う");
		static_assert(std::is_void_v<Request> || std::is_class_v<Request>, "問い合わせに載せるのは構造体 1 つ");
		static_assert(!(std::is_same_v<Policy, all_of> || std::is_same_v<Policy, any_of>) || std::is_same_v<Result, bool>,
			"all_of / any_of は答えが bool");

		constexpr explicit inquiry(std::string_view name) : name(name), target(system_utility::generate_hash(name)) {}

		std::string_view name;
		address target;
	};

	class inquiry_desk
	{
	public:
		inquiry_desk();
		~inquiry_desk();
		inquiry_desk(const inquiry_desk&) = delete;
		inquiry_desk& operator=(const inquiry_desk&) = delete;

		//	@tool 問い合わせる。single は std::optional<Result>、all_of / any_of は std::optional<bool>、collect は std::vector<Result>
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

		//	@tool 答える側の登録
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
			core->check_thread();
			const std::shared_ptr<detail::registry> keep_alive = core;	//	@tool 答える側が inquiry_desk を消しても中身を残す
			const detail::slot* found = keep_alive->find_or_add(target.target, target.name, type_of_inquiry<Result, Request, Policy>());
			std::vector<std::shared_ptr<detail::entry>> receivers;
			if (found)
				receivers = keep_alive->snapshot(*found);
			detail::registry::depth_guard guard(*keep_alive);

			if constexpr (std::is_same_v<Policy, collect>)
			{
				std::vector<Result> results;
				for (const auto& receiver : receivers)
				{
					if (!guard || !receiver->alive)
						continue;
					std::optional<Result> answer;
					receiver->invoke(request, &answer);
					if (answer)
						results.push_back(std::move(*answer));
				}
				return results;
			}
			else if constexpr (std::is_same_v<Policy, single>)
			{
				std::optional<Result> answer;
				for (const auto& receiver : receivers)
				{
					if (!guard || !receiver->alive)
						continue;
					receiver->invoke(request, &answer);
					break;
				}
				return answer;
			}
			else
			{
				//	@tool all_of は false、any_of は true が出た時点で打ち切る
				constexpr bool stop_value = std::is_same_v<Policy, any_of>;
				std::optional<bool> result;
				for (const auto& receiver : receivers)
				{
					if (!guard || !receiver->alive)
						continue;
					std::optional<bool> answer;
					receiver->invoke(request, &answer);
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
		std::shared_ptr<detail::registry> core;
	};
}
