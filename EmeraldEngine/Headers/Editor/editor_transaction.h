#pragma once

#if defined(USE_IMGUI)
#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

//	基底トランザクション
class editor_transaction
{
public:
	virtual std::string_view get_label() const = 0;

public:
	virtual ~editor_transaction() = default;

	virtual bool redo() = 0;
	virtual bool undo() = 0;
};

//	関数型トランザクション
class editor_function_transaction : public editor_transaction
{
public:
	using function = std::function<bool()>;

public:
	std::string_view get_label() const override { return label; }

public:
	editor_function_transaction(std::string label, function undo_function, function redo_function)
	: label(std::move(label))
	, undo_function(std::move(undo_function))
	, redo_function(std::move(redo_function))
	{
	}

	bool redo() override { return redo_function ? redo_function() : false; }
	bool undo() override { return undo_function ? undo_function() : false; }

private:
	std::string label;
	function undo_function;
	function redo_function;
};

//	取った時点の中身に戻す関数
using editor_restore_function = std::function<bool()>;

//	明示的なトランザクションで記録する物
struct editor_transaction_target
{
	const void* key = nullptr;								//	同じ物を二重に記録しないための判定用
	std::function<editor_restore_function()> capture;		//	今の中身を取って、その中身に戻す関数を返す(取れなければ空)
};

//	トランザクション履歴
class editor_transaction_history
{
	static constexpr std::size_t invalid_position = static_cast<std::size_t>(-1);

	using transaction_list = std::vector<std::unique_ptr<editor_transaction>>;

	//	明示的なトランザクションで記録した物
	struct recorded_target
	{
		const void* key = nullptr;
		std::function<editor_restore_function()> capture;
		editor_restore_function before;		//	変更前に戻す関数
	};

public:
	static constexpr std::size_t default_limit = 256;

public:
	//	リドゥ・アンドゥ可能か？
	bool can_redo() const { return position < transactions.size(); }
	bool can_undo() const { return position > 0; }

	//	直近のリドゥ・アンドゥラベル
	std::string_view get_redo_label() const { return can_redo() ? transactions[position]->get_label() : std::string_view(); }
	std::string_view get_undo_label() const { return can_undo() ? transactions[position - 1]->get_label() : std::string_view(); }

	//	履歴を辿っている最中か?
	bool is_applying() const { return applying; }

	//	保存した位置から変わっているか
	bool is_dirty() const { return saved_position != position; }

	//	履歴最大値変更
	void set_limit(std::size_t value) { limit = value > 0 ? value : 1; }

	//	明示的なトランザクションの途中か
	bool is_in_transaction() const { return transaction_depth > 0; }

public:
	//	積む
	void push(std::unique_ptr<editor_transaction> transaction);

	//	関数型トランザクション用
	void push(std::string label, editor_function_transaction::function undo_function, editor_function_transaction::function redo_function);

	//	リドゥ
	bool redo();

	//	アンドゥ
	bool undo();

	//	全部捨てる
	void clear();

	//	保存した位置を覚える
	void mark_saved() { saved_position = position; }

public:	//	明示的なトランザクション
	//	開始
	void begin_transaction(std::string label);

	//	変更前を記録
	void record(const editor_transaction_target& target, editor_restore_function before = {});

	//	記録した物の変更後を取って1件積む
	void commit_transaction();

	//	記録した物を変更前に戻す
	void cancel_transaction();

private:
	//	index 以降を捨てる
	void discard_after(std::size_t index);

private:
	transaction_list transactions;		//	操作履歴
	std::size_t position = 0;			//	次に積む位置
	std::size_t saved_position = 0;		//	保存した時の位置
	std::size_t limit = default_limit;	//	積める数
	bool applying = false;				//	履歴を辿っている最中か

	std::string transaction_label;					//	明示的なトランザクションの名前
	std::vector<recorded_target> recorded_targets;	//	明示的なトランザクションで記録した物
	int transaction_depth = 0;						//	明示的なトランザクションの入れ子の深さ
};

//	スコープを抜けると確定するトランザクション
class editor_scoped_transaction
{
public:
	editor_scoped_transaction(editor_transaction_history& history, std::string label)
	: history(history)
	{
		history.begin_transaction(std::move(label));
	}
	~editor_scoped_transaction()
	{
		if (active)
			history.commit_transaction();
	}

	editor_scoped_transaction(const editor_scoped_transaction&) = delete;
	editor_scoped_transaction& operator=(const editor_scoped_transaction&) = delete;

	//	変更前を記録
	void record(const editor_transaction_target& target, editor_restore_function before = {}) { history.record(target, std::move(before)); }

	//	取り消す
	void cancel()
	{
		if (!active)
			return;
		history.cancel_transaction();
		active = false;
	}

private:
	editor_transaction_history& history;
	bool active = true;
};
#endif	//	defined(USE_IMGUI)
