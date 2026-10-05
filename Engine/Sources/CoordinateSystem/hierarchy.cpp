#include "CoordinateSystem/hierarchy.h"

hierarchy::hierarchy()
{
}

hierarchy::~hierarchy()
{
	unlink();
}

std::shared_ptr<hierarchy> hierarchy::get_parent() const
{
	return parent.lock();
}

const hierarchy::children_type& hierarchy::get_children() const
{
	return children;
}

std::size_t hierarchy::get_child_count() const
{
	return children.size();
}

std::size_t hierarchy::get_depth() const
{
	std::size_t depth = 0;
	for (std::shared_ptr<hierarchy> node = parent.lock(); node; node = node->parent.lock())
	{
		++depth;
	}
	return depth;
}

bool hierarchy::is_descendant_of(const hierarchy* ancestor) const
{
	if (!ancestor)
	{
		return false;
	}
	for (std::shared_ptr<hierarchy> node = parent.lock(); node; node = node->parent.lock())
	{
		if (node.get() == ancestor)
		{
			return true;
		}
	}
	return false;
}

bool hierarchy::can_attach(const hierarchy* self, const hierarchy* parent)
{
	if (!parent)
	{
		return true;
	}
	return parent != self && !parent->is_descendant_of(self);
}

void hierarchy::link(const std::shared_ptr<hierarchy>& parent)
{
	const std::shared_ptr<hierarchy> self = shared_from_this();
	unlink();
	if (!parent)
	{
		return;
	}

	this->parent = parent;
	parent->children.push_back(self);
}

void hierarchy::unlink()
{
	if (std::shared_ptr<hierarchy> current = parent.lock())
	{
		children_type& siblings = current->children;
		auto it = std::find_if(siblings.begin(), siblings.end(), [this](const std::shared_ptr<hierarchy>& child) { return child.get() == this; });
		if (it != siblings.end())
		{
			siblings.erase(it);
		}
	}
	parent.reset();
}

hierarchy::children_type hierarchy::release_children()
{
	children_type released = std::move(children);
	children.clear();
	for (const std::shared_ptr<hierarchy>& child : released)
	{
		child->parent.reset();
	}
	return released;
}
