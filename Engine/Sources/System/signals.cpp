#include "System/signals.h"

#include "System/logger.h"

namespace signals
{
	//////////////////////////////////////////////////////////////////////////
	//	@tool connection
	//////////////////////////////////////////////////////////////////////////

	connection::connection(std::weak_ptr<detail::registry> owner, address target, uint64_t key)
	: owner(std::move(owner))
	, target(target)
	, key(key)
	{
	}

	connection::connection(connection&& other) noexcept
	: owner(std::move(other.owner))
	, target(other.target)
	, key(std::exchange(other.key, 0))
	{
	}

	connection& connection::operator=(connection&& other) noexcept
	{
		if (this != &other)
		{
			disconnect();
			owner = std::move(other.owner);
			target = other.target;
			key = std::exchange(other.key, 0);
		}
		return *this;
	}

	connection::~connection()
	{
		disconnect();
	}

	void connection::disconnect()
	{
		if (key != 0)
		{
			if (auto locked = owner.lock())
				locked->remove(target, key);
		}
		owner.reset();
		key = 0;
	}

	bool connection::is_connected() const
	{
		return key != 0 && !owner.expired();
	}

	//////////////////////////////////////////////////////////////////////////
	//	@tool registry
	//////////////////////////////////////////////////////////////////////////

	namespace detail
	{
		registry::depth_guard::depth_guard(registry& owner)
		: owner(owner)
		, entered(owner.depth < max_depth)
		{
			_ASSERT_EXPR_A(entered, "signals: 通知の入れ子が深すぎます");
			if (entered)
				++owner.depth;
		}

		registry::depth_guard::~depth_guard()
		{
			if (entered)
				--owner.depth;
		}

		registry::registry()
		: slots()
		, owner_thread(std::this_thread::get_id())
		{
		}

		slot* registry::find_or_add(address target, std::string_view name, type_id type)
		{
			auto [it, added] = slots.try_emplace(target);
			slot& found = it->second;
			if (added)
			{
				found.name = name;
				found.type = type;
				return &found;
			}

			//	@tool 名前が違えばハッシュの衝突、型が違えば同名の宛先の重複
			_ASSERT_EXPR_A(found.name == name, "signals: 宛先のハッシュが衝突しています");
			_ASSERT_EXPR_A(found.type == type, "signals: 同じ名前で型の違う宛先があります");
			return found.type == type ? &found : nullptr;
		}

		std::vector<std::shared_ptr<entry>> registry::snapshot(const slot& target) const
		{
			return target.entries;
		}

		void registry::remove(address target, uint64_t key)
		{
			check_thread();
			auto it = slots.find(target);
			if (it == slots.end())
				return;

			auto& entries = it->second.entries;
			auto found = std::find_if(entries.begin(), entries.end(), [key](const auto& value) { return value->key == key; });
			if (found == entries.end())
				return;

			//	@tool 配っている最中の写しは alive を見て飛ばす
			(*found)->alive = false;
			entries.erase(found);
		}

		void registry::check_thread() const
		{
			_ASSERT_EXPR_A(std::this_thread::get_id() == owner_thread, "signals: 作ったスレッド以外から使われました");
		}

		connection registry::connect(const std::shared_ptr<registry>& self, address target, std::string_view name, type_id type,
			std::function<void(const void*, void*)> invoke, std::source_location location, bool exclusive)
		{
			self->check_thread();
			slot* found = self->find_or_add(target, name, type);
			if (!found)
				return connection();

			_ASSERT_EXPR_A(!exclusive || found->entries.empty(), "signals: single の問い合わせに答える相手が 2 つ目です");

			auto value = std::make_shared<entry>();
			value->key = self->next_key++;
			value->invoke = std::move(invoke);
			value->location = location;
			found->entries.push_back(value);
			return connection(self, target, value->key);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	//	@tool broadcaster
	//////////////////////////////////////////////////////////////////////////

	broadcaster::broadcaster()
	: core(std::make_shared<detail::registry>())
	, posted()
	{
	}

	broadcaster::~broadcaster() = default;

	void broadcaster::dispatch_posted()
	{
		core->check_thread();

		//	@tool 配っている最中に積まれた分も、回数の上限まで同じフレームで配る
		for (uint32_t round = 0; round < max_post_rounds && !posted.empty(); ++round)
		{
			std::vector<std::function<void()>> current;
			current.swap(posted);
			for (auto& item : current)
			{
				item();
			}
		}
		DEBUG_CHECK_WARNING(!posted.empty(), "signals: 後で配る通知が連鎖しすぎたため、残りは次のフレームに回します\n");
	}

	void broadcaster::dispatch(address target, std::string_view name, detail::type_id type, const void* payload)
	{
		core->check_thread();
		const std::shared_ptr<detail::registry> keep_alive = core;	//	@tool 受け手が broadcaster を消しても中身を残す
		const detail::slot* found = keep_alive->find_or_add(target, name, type);
		if (!found)
			return;

		//	@tool 写しを回すので、受け手の中での購読・解除は安全（追加された受け手は次の通知から）
		const auto receivers = keep_alive->snapshot(*found);
		detail::registry::depth_guard guard(*keep_alive);
		if (!guard)
			return;
		for (const auto& receiver : receivers)
		{
			if (receiver->alive)
				receiver->invoke(payload, nullptr);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	//	@tool inquiry_desk
	//////////////////////////////////////////////////////////////////////////

	inquiry_desk::inquiry_desk()
	: core(std::make_shared<detail::registry>())
	{
	}

	inquiry_desk::~inquiry_desk() = default;
}
