#include "CoordinateSystem/spatial.h"

#include "cereal/archives/binary.hpp"
#include "cereal/archives/json.hpp"

spatial::spatial()
: spatial(transformd::identity())
{
}

spatial::spatial(const transformd& local)
: system_flags(0)
, version(0)
, local(local)
, world(local)
{
	set_flag(flags::dirty, true);
}

spatial::~spatial()
{
	//	子は現在のワールド姿勢を維持したままルートにする
	const transformd& current_world = get_world();
	for (const std::shared_ptr<hierarchy>& child : release_children())
	{
		spatial* child_spatial = static_cast<spatial*>(child.get());
		child_spatial->local = child_spatial->local * current_world;
		child_spatial->mark_dirty();
	}
}

bool spatial::is_flag(flags flag) const
{
	return system_utility::is_bitflag(system_flags, 1 << static_cast<int>(flag));
}

void spatial::set_flag(flags flag, bool value)
{
	system_utility::set_bitflag(system_flags, 1 << static_cast<int>(flag), value);
}

const transformd& spatial::get_local() const
{
	return local;
}

void spatial::set_local(const transformd& local)
{
	this->local = local;
	mark_dirty();
}

void spatial::set_local_scale(const math::vector3d& scale)
{
	local.set_scale(scale);
	mark_dirty();
}

void spatial::set_local_rotation(const math::quaterniond& rotation)
{
	local.set_rotation(rotation);
	mark_dirty();
}

void spatial::set_local_translation(const math::vector3d& translation)
{
	local.set_translation(translation);
	mark_dirty();
}

const transformd& spatial::get_world() const
{
	update_world();
	return world;
}

void spatial::set_world(const transformd& world)
{
	const std::shared_ptr<spatial> parent = get_parent();
	local = parent ? world.relative_to(parent->get_world()) : world;
	mark_dirty();
}

std::uint32_t spatial::get_version() const
{
	return version;
}

std::shared_ptr<spatial> spatial::get_parent() const
{
	return std::static_pointer_cast<spatial>(hierarchy::get_parent());
}

std::shared_ptr<spatial> spatial::get_child(std::size_t index) const
{
	return std::static_pointer_cast<spatial>(get_children()[index]);
}

std::shared_ptr<spatial> spatial::get_root()
{
	std::shared_ptr<spatial> node = std::static_pointer_cast<spatial>(shared_from_this());
	while (std::shared_ptr<spatial> parent = node->get_parent())
	{
		node = parent;
	}
	return node;
}

bool spatial::attach(const std::shared_ptr<spatial>& parent, attach_rule rule)
{
	if (parent == get_parent())
	{
		return true;
	}

	//	循環参照する場合は外す
	if (!can_attach(this, parent.get()))
	{
		return false;
	}

	const std::shared_ptr<hierarchy> self = shared_from_this();
	if (rule == attach_rule::keep_world)
	{
		const transformd current_world = get_world();
		link(parent);
		local = parent ? current_world.relative_to(parent->get_world()) : current_world;
	}
	else
	{
		link(parent);
	}
	mark_dirty();
	return true;
}

void spatial::detach(attach_rule rule)
{
	attach(nullptr, rule);
}

void spatial::update_descendants() const
{
	update_world();
	for (const std::shared_ptr<hierarchy>& child : get_children())
	{
		static_cast<const spatial*>(child.get())->update_descendants();
	}
}

void spatial::mark_dirty()
{
	++version;

	if (is_flag(flags::dirty))
	{
		return;
	}
	set_flag(flags::dirty, true);

	for (const std::shared_ptr<hierarchy>& child : get_children())
	{
		static_cast<spatial*>(child.get())->mark_dirty();
	}
}

void spatial::update_world() const
{
	if (!is_flag(flags::dirty))
		return;

	const std::shared_ptr<spatial> parent = get_parent();
	world = parent ? local * parent->get_world() : local;

	system_utility::set_bitflag(system_flags, 1 << static_cast<int>(flags::dirty), false);
}

template<class Archive>
void spatial::serialize(Archive& archive, int version)
{
	archive(cereal::make_nvp("local", local));

	if constexpr (Archive::is_loading::value)
	{
		mark_dirty();
	}
}

//	serialize は cpp で定義しているため、使用するアーカイブごとに明示的に実体化
template void spatial::serialize<cereal::BinaryInputArchive>(cereal::BinaryInputArchive&, int);
template void spatial::serialize<cereal::BinaryOutputArchive>(cereal::BinaryOutputArchive&, int);
template void spatial::serialize<cereal::JSONInputArchive>(cereal::JSONInputArchive&, int);
template void spatial::serialize<cereal::JSONOutputArchive>(cereal::JSONOutputArchive&, int);
