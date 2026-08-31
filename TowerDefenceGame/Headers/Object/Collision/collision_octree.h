#pragma once

#include <cstddef>
#include <utility>
#include <vector>
#include <DirectXMath.h>

class collision_object;

//	八分木空間分割
class collision_octree
{
public:
	struct aabb
	{
		DirectX::XMFLOAT3 min;
		DirectX::XMFLOAT3 max;

		//	2つのAABBが交差しているか
		static bool intersects(const aabb& lhs, const aabb& rhs)
		{
			return lhs.min.x <= rhs.max.x && lhs.max.x >= rhs.min.x
				&& lhs.min.y <= rhs.max.y && lhs.max.y >= rhs.min.y
				&& lhs.min.z <= rhs.max.z && lhs.max.z >= rhs.min.z;
		}
	};

	using object_pair = std::pair<collision_object*, collision_object*>;

public:
	collision_octree();
	~collision_octree();

	//	ルート領域と最大分割深度を指定して木を初期化する。
	void reset(const aabb& world_bounds, int depth_limit = 6);

	//	クリア
	void clear();

	//	オブジェクトを登録する。
	void insert(collision_object* object, const aabb& bounds);

	//	交差するオブジェクトの組を取得
	void collect_pairs(std::vector<object_pair>& out_pairs) const;

	//	指定AABBに交差するオブジェクトを取得
	void query(const aabb& bounds, std::vector<collision_object*>& out_objects) const;

	std::size_t size() const { return entry_count; }

private:
	//	@tool children[] は size_t（符号なし）なので、未生成スロットの番兵は -1 ではなくこれを使う
	static constexpr std::size_t invalid_node = static_cast<std::size_t>(-1);

	//	木に登録された1オブジェクト
	struct entry
	{
		collision_object* object;
		aabb bounds;
		UINT order;
	};

	//	八分木ノード
	struct node
	{
		DirectX::XMFLOAT3 center;
		float half_size;
		std::vector<entry> entries;
		size_t children[8];
	};

private:
	size_t allocate_node(const DirectX::XMFLOAT3& center, float half_size);
	void insert_recursive(std::size_t node_index, int depth, const entry& item);
	int  child_octant_containing(const node& parent, const aabb& bounds) const;
	void child_bounds(const node& parent, int octant, DirectX::XMFLOAT3& out_center, float& out_half) const;
	void collect_recursive(std::size_t node_index, std::vector<const entry*>& ancestors, std::vector<object_pair>& out_pairs) const;
	void query_recursive(std::size_t node_index, const aabb& bounds, std::vector<collision_object*>& out_objects) const;

	static aabb node_bounds(const node& n);
	static object_pair make_ordered_pair(const entry& lhs, const entry& rhs);

private:
	std::vector<node> nodes;
	int max_depth;
	std::size_t entry_count;
	bool ready;
};
