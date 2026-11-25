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
}

void editor_transaction_history::discard_after(std::size_t index)
{
	transactions.erase(transactions.begin() + index, transactions.end());
	if (saved_position > index)
		saved_position = invalid_position;
}
#endif	//	defined(USE_IMGUI)
