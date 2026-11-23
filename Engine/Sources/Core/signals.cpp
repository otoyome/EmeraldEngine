#include "Core/signals.h"

#include <algorithm>

#include "Core/logger.h"

namespace
{
	//	入れ子の深さはスレッドごとに数える
	thread_local uint32_t t_depth = 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
core::signals::connection::connection(std::weak_ptr<detail::registry> owner, address target, uint64_t key)
: owner(std::move(owner))
, target(target)
, key(key)
{
}

core::signals::connection::connection(connection&& other) noexcept
: owner(std::move(other.owner))
, target(other.target)
, key(std::exchange(other.key, 0))
{
}

core::signals::connection& core::signals::connection::operator=(connection&& other) noexcept
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

core::signals::connection::~connection()
{
	disconnect();
}

void core::signals::connection::disconnect()
{
	if (key != 0)
	{
		if (auto locked = owner.lock())
			locked->remove(target, key);
	}
	owner.reset();
	key = 0;
}

bool core::signals::connection::is_connected() const
{
	return key != 0 && !owner.expired();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
core::signals::detail::registry::depth_guard::depth_guard()
: entered(t_depth < max_depth)
{
	_ASSERT_EXPR_A(entered, "signals: 通知の入れ子が深すぎます");
	if (entered)
		++t_depth;
}

core::signals::detail::registry::depth_guard::~depth_guard()
{
	if (entered)
		--t_depth;
}

core::signals::detail::registry::registry() = default;

core::signals::detail::slot* core::signals::detail::registry::find_or_add_locked(address target, std::string_view name, type_id type)
{
	auto [it, added] = slots.try_emplace(target);
	slot& found = it->second;
	if (added)
	{
		found.name = name;
		found.type = type;
		return &found;
	}

	_ASSERT_EXPR_A(found.name == name, "signals: 宛先のハッシュが衝突しています");
	_ASSERT_EXPR_A(found.type == type, "signals: 同じ名前で型の違う宛先があります");
	return found.type == type ? &found : nullptr;
}

bool core::signals::detail::registry::snapshot(address target, std::string_view name, type_id type, std::vector<std::shared_ptr<entry>>& out)
{
	std::lock_guard<std::mutex> lock(mutex);
	const slot* found = find_or_add_locked(target, name, type);
	if (!found)
		return false;
	out = found->entries;
	return true;
}

void core::signals::detail::registry::remove(address target, uint64_t key)
{
	std::shared_ptr<entry> removed;
	{
		std::lock_guard<std::mutex> lock(mutex);
		auto it = slots.find(target);
		if (it == slots.end())
			return;

		auto& entries = it->second.entries;
		auto found = std::find_if(entries.begin(), entries.end(), [key](const auto& value) { return value->key == key; });
		if (found == entries.end())
			return;

		removed = *found;
		entries.erase(found);
	}

	//	配っている最中の写しは alive を見て飛ばす。実行中の呼び出しがあれば終わるまで待つ
	//	（表のロックは手放してから待つので、受け手の中での購読・解除とは競合しない）
	removed->retire();
}

core::signals::connection core::signals::detail::registry::connect(const std::shared_ptr<registry>& self, address target, std::string_view name, type_id type,
	std::function<void(const void*, void*)> invoke, std::source_location location, bool exclusive)
{
	auto value = std::make_shared<entry>();
	value->invoke = std::move(invoke);
	value->location = location;

	{
		std::lock_guard<std::mutex> lock(self->mutex);
		slot* found = self->find_or_add_locked(target, name, type);
		if (!found)
			return connection();

		_ASSERT_EXPR_A(!exclusive || found->entries.empty(), "signals: single の問い合わせに答える相手が 2 つ目です");

		value->key = self->next_key++;
		found->entries.push_back(value);
	}
	return connection(self, target, value->key);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
core::signals::broadcaster::broadcaster()
: core(std::make_shared<detail::registry>())
{
}

core::signals::broadcaster::~broadcaster() = default;

void core::signals::broadcaster::push_posted(std::function<void()> item)
{
	std::lock_guard<std::mutex> lock(posted_mutex);
	posted.push_back(std::move(item));
}

void core::signals::broadcaster::dispatch_posted()
{
	//	予約配信
	for (uint32_t round = 0; round < max_post_rounds; ++round)
	{
		std::vector<std::function<void()>> current;
		{
			std::lock_guard<std::mutex> lock(posted_mutex);
			if (posted.empty())
				break;
			current.swap(posted);
		}
		for (auto& item : current)
		{
			item();
		}
	}

	bool remaining = false;
	{
		std::lock_guard<std::mutex> lock(posted_mutex);
		remaining = !posted.empty();
	}
	DEBUG_CHECK_WARNING(remaining, "signals: 後で配る通知が連鎖しすぎたため、残りは次のフレームに回します\n");
}

void core::signals::broadcaster::dispatch(address target, std::string_view name, detail::type_id type, const void* payload)
{
	const std::shared_ptr<detail::registry> keep_alive = core;	//	受け手が broadcaster を消しても中身を残す

	//	写しを回すので、受け手の中での購読・解除は安全
	std::vector<std::shared_ptr<detail::entry>> receivers;
	if (!keep_alive->snapshot(target, name, type, receivers))
		return;

	detail::registry::depth_guard guard;
	if (!guard)
		return;
	for (const auto& receiver : receivers)
	{
		receiver->call(payload, nullptr);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
core::signals::contactor::contactor()
: core(std::make_shared<detail::registry>())
{
}
