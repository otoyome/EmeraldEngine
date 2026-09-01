#include "Object/Collision/collision_octree.h"

#include <algorithm>

namespace
{
	//	ルート領域が退化していても最低限の大きさを確保する
	static constexpr float min_root_half = 0.5f;
}

collision_octree::collision_octree()
: nodes()
, max_depth(6)
, entry_count(0)
, ready(false)
{
}

collision_octree::~collision_octree()
{
}

void collision_octree::reset(const aabb& world_bounds, int depth_limit)
{
	nodes.clear();
	entry_count = 0;
	max_depth = (std::max)(depth_limit, 0);

	DirectX::XMFLOAT3 center
	{
		(world_bounds.min.x + world_bounds.max.x) * 0.5f,
		(world_bounds.min.y + world_bounds.max.y) * 0.5f,
		(world_bounds.min.z + world_bounds.max.z) * 0.5f,
	};

	float half = min_root_half;
	half = (std::max)(half, (world_bounds.max.x - world_bounds.min.x) * 0.5f);
	half = (std::max)(half, (world_bounds.max.y - world_bounds.min.y) * 0.5f);
	half = (std::max)(half, (world_bounds.max.z - world_bounds.min.z) * 0.5f);

	nodes.reserve(64);
	allocate_node(center, half);
	ready = true;
}

void collision_octree::clear()
{
	nodes.clear();
	entry_count = 0;
	ready = false;
}

size_t collision_octree::allocate_node(const DirectX::XMFLOAT3& center, float half_size)
{
	node created{};
	created.center = center;
	created.half_size = half_size;
	for (auto& child : created.children)
		child = invalid_node;

	nodes.push_back(std::move(created));
	return nodes.size() - 1;
}

void collision_octree::insert(collision_object* object, const aabb& bounds)
{
	if (!ready || nodes.empty())
		return;

	entry item{};
	item.object = object;
	item.bounds = bounds;
	item.order = static_cast<UINT>(entry_count++);

	insert_recursive(0, 0, item);
}

void collision_octree::insert_recursive(std::size_t node_index, int depth, const entry& item)
{
	//	子の空間へ行けるか
	if (depth < max_depth)
	{
		int octant = child_octant_containing(nodes[node_index], item.bounds);
		if (octant >= 0)
		{
			if (nodes[node_index].children[octant] == invalid_node)
			{
				DirectX::XMFLOAT3 child_center;
				float child_half;
				child_bounds(nodes[node_index], octant, child_center, child_half);

				std::size_t created = allocate_node(child_center, child_half);
				nodes[node_index].children[octant] = created;
			}
			insert_recursive(nodes[node_index].children[octant], depth + 1, item);
			return;
		}
	}

	//	これ以上潜れないのでこのノードに配置
	nodes[node_index].entries.push_back(item);
}

int collision_octree::child_octant_containing(const node& parent, const aabb& bounds) const
{
	int octant = 0;

	if (bounds.min.x >= parent.center.x)      octant |= 1;
	else if (bounds.max.x > parent.center.x)  return -1;

	if (bounds.min.y >= parent.center.y)      octant |= 2;
	else if (bounds.max.y > parent.center.y)  return -1;

	if (bounds.min.z >= parent.center.z)      octant |= 4;
	else if (bounds.max.z > parent.center.z)  return -1;

	return octant;
}

void collision_octree::child_bounds(const node& parent, int octant, DirectX::XMFLOAT3& out_center, float& out_half) const
{
	out_half = parent.half_size * 0.5f;
	out_center.x = parent.center.x + ((octant & 1) ? out_half : -out_half);
	out_center.y = parent.center.y + ((octant & 2) ? out_half : -out_half);
	out_center.z = parent.center.z + ((octant & 4) ? out_half : -out_half);
}

collision_octree::aabb collision_octree::node_bounds(const node& n)
{
	aabb box{};
	box.min = { n.center.x - n.half_size, n.center.y - n.half_size, n.center.z - n.half_size };
	box.max = { n.center.x + n.half_size, n.center.y + n.half_size, n.center.z + n.half_size };
	return box;
}

collision_octree::object_pair collision_octree::make_ordered_pair(const entry& lhs, const entry& rhs)
{
	if (lhs.order <= rhs.order)
		return { lhs.object, rhs.object };
	return { rhs.object, lhs.object };
}

void collision_octree::collect_pairs(std::vector<object_pair>& out_pairs) const
{
	if (!ready || nodes.empty())
		return;

	std::vector<const entry*> ancestors;
	ancestors.reserve(32);
	collect_recursive(0, ancestors, out_pairs);
}

void collision_octree::collect_recursive(std::size_t node_index, std::vector<const entry*>& ancestors, std::vector<object_pair>& out_pairs) const
{
	const node& n = nodes[node_index];

	//	同一ノード内の総当たり
	for (std::size_t i = 0; i < n.entries.size(); ++i)
	{
		for (std::size_t j = i + 1; j < n.entries.size(); ++j)
		{
			if (aabb::intersects(n.entries[i].bounds, n.entries[j].bounds))
				out_pairs.emplace_back(make_ordered_pair(n.entries[i], n.entries[j]));
		}
	}

	//	大きい物が親に留まっている可能性があるので、親ノードのオブジェクトとの組
	for (const entry& current : n.entries)
	{
		for (const entry* ancestor : ancestors)
		{
			if (aabb::intersects(current.bounds, ancestor->bounds))
				out_pairs.emplace_back(make_ordered_pair(*ancestor, current));
		}
	}

	//	子が無ければここで終わり
	bool has_child = false;
	for (std::size_t child : n.children)
	{
		if (child != invalid_node)
		{
			has_child = true;
			break;
		}
	}
	if (!has_child)
		return;

	//	このノードのオブジェクトを祖先リストへ積んで子へ潜る
	for (const entry& current : n.entries)
		ancestors.push_back(&current);

	for (std::size_t child : n.children)
	{
		if (child != invalid_node)
			collect_recursive(child, ancestors, out_pairs);
	}

	ancestors.resize(ancestors.size() - n.entries.size());
}

void collision_octree::query(const aabb& bounds, std::vector<collision_object*>& out_objects) const
{
	if (!ready || nodes.empty())
		return;
	if (!aabb::intersects(node_bounds(nodes[0]), bounds))
		return;

	query_recursive(0, bounds, out_objects);
}

void collision_octree::query_recursive(std::size_t node_index, const aabb& bounds, std::vector<collision_object*>& out_objects) const
{
	const node& n = nodes[node_index];

	for (const entry& current : n.entries)
	{
		if (aabb::intersects(current.bounds, bounds))
			out_objects.push_back(current.object);
	}

	for (std::size_t child : n.children)
	{
		if (child == invalid_node)
			continue;
		if (aabb::intersects(node_bounds(nodes[child]), bounds))
			query_recursive(child, bounds, out_objects);
	}
}
