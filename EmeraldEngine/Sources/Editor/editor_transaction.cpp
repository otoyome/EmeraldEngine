#include "Editor/editor_transaction.h"

#if defined(USE_IMGUI)

void editor_transaction_history::push(std::unique_ptr<editor_transaction> transaction)
{
	if (!transaction || applying)
		return;

	transactions.erase(transactions.begin() + position, transactions.end());
	if (saved_position > position)
		saved_position = invalid_position;
	transactions.emplace_back(std::move(transaction));
	++position;

	if (transactions.size() > limit)
	{
		const std::size_t over = transactions.size() - limit;
		transactions.erase(transactions.begin(), transactions.begin() + over);
		position -= over;
		saved_position = saved_position >= over && saved_position != invalid_position ? saved_position - over : invalid_position;
	}
}

void editor_transaction_history::push(std::string label, editor_function_transaction::function undo_function, editor_function_transaction::function redo_function)
{
	push(std::make_unique<editor_function_transaction>(std::move(label), std::move(undo_function), std::move(redo_function)));
}

bool editor_transaction_history::undo()
{
	if (!can_undo())
		return false;

	applying = true;
	const bool result = transactions[position - 1]->undo();
	applying = false;
	--position;
	if (!result)
		discard_after(position);
	return result;
}

bool editor_transaction_history::redo()
{
	if (!can_redo())
		return false;

	applying = true;
	const bool result = transactions[position]->redo();
	applying = false;
	if (!result)
	{
		discard_after(position);
		return false;
	}
	++position;
	return true;
}

void editor_transaction_history::clear()
{
	transactions.clear();
	position = 0;
	saved_position = 0;

	transaction_label.clear();
	recorded_targets.clear();
	transaction_depth = 0;
}

void editor_transaction_history::begin_transaction(std::string label)
{
	if (transaction_depth++ == 0)
	{
		transaction_label = std::move(label);
		recorded_targets.clear();
	}
}

void editor_transaction_history::record(const editor_transaction_target& target, editor_restore_function before)
{
	if (transaction_depth <= 0 || !target.capture || applying)
		return;

	//	同じ物は最初に記録した変更前を使う
	if (target.key)
	{
		for (const recorded_target& recorded : recorded_targets)
		{
			if (recorded.key == target.key)
				return;
		}
	}

	if (!before)
		before = target.capture();
	if (!before)
		return;
	recorded_targets.push_back({ target.key, target.capture, std::move(before) });
}

void editor_transaction_history::commit_transaction()
{
	if (transaction_depth <= 0)
		return;
	if (--transaction_depth > 0)
		return;

	std::vector<recorded_target> records = std::move(recorded_targets);
	recorded_targets.clear();
	if (records.empty())
		return;

	//	変更後を取る
	std::vector<editor_restore_function> befores;
	std::vector<editor_restore_function> afters;
	for (recorded_target& recorded : records)
	{
		editor_restore_function after = recorded.capture();
		if (!after)
			continue;
		befores.emplace_back(std::move(recorded.before));
		afters.emplace_back(std::move(after));
	}
	if (befores.empty())
		return;

	//	元に戻す時は記録と逆の順、やり直す時は記録した順に戻す
	push(std::move(transaction_label),
		[befores]()
		{
			bool result = true;
			for (auto it = befores.rbegin(); it != befores.rend(); ++it)
				result = (*it)() && result;
			return result;
		},
		[afters]()
		{
			bool result = true;
			for (const editor_restore_function& after : afters)
				result = after() && result;
			return result;
		});
	transaction_label.clear();
}

void editor_transaction_history::cancel_transaction()
{
	for (auto it = recorded_targets.rbegin(); it != recorded_targets.rend(); ++it)
		it->before();
	recorded_targets.clear();
	transaction_label.clear();
	transaction_depth = 0;
}

void editor_transaction_history::discard_after(std::size_t index)
{
	transactions.erase(transactions.begin() + index, transactions.end());
	if (saved_position > index)
		saved_position = invalid_position;
}
#endif	//	defined(USE_IMGUI)
