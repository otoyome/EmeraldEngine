#pragma once

#include <memory>
#include <vector>

//	親子関係
class hierarchy : public std::enable_shared_from_this<hierarchy>
{
public:
	using children_type = std::vector<std::shared_ptr<hierarchy>>;

public:
	std::shared_ptr<hierarchy> get_parent() const;

	const children_type& get_children() const;
	std::size_t get_child_count() const;

	//	ルートからの深さ
	std::size_t get_depth() const;

	//	子孫判定
	bool is_descendant_of(const hierarchy* ancestor) const;

protected:
	hierarchy();
	virtual ~hierarchy();
	hierarchy(const hierarchy&) = delete;
	hierarchy& operator=(const hierarchy&) = delete;

	//	自身を指定の子として登録する
	void link(const std::shared_ptr<hierarchy>& parent);

	//	自身を親から外す
	void unlink();

	//	すべての子の親を解除して子のリストを返す
	children_type release_children();

protected:
	//	アタッチできるか判定
	static bool can_attach(const hierarchy* self, const hierarchy* parent);

private:
	std::weak_ptr<hierarchy> parent;	//	親
	children_type children;				//	子供達
};
