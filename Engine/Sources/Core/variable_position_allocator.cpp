#include "Core/variable_position_allocator.h"

namespace
{
	//	sizeの上位1bitをalive(確保済み)フラグとして使用する
	constexpr UINT alive_flag = 1u << 31;

	bool is_alive(UINT size_with_flag)
	{
		return (size_with_flag & alive_flag) != 0;
	}

	UINT pack_alive(UINT size)
	{
		return size | alive_flag;
	}

	UINT unpack_size(UINT size_with_flag)
	{
		return size_with_flag & ~alive_flag;
	}
}

//	pool_size = 2,147,483,647バイトまで
bool core::variable_position_allocator::initialize(UINT pool_size)
{
	if (pool_size >= alive_flag)
		return false;

	nodes.clear();
	nodes.push_back({ 0, pool_size });
	capacity = pool_size;
	used_size = 0;
	return true;
}

UINT core::variable_position_allocator::allocate(UINT size)
{
	for (size_t index = 0; index < nodes.size(); ++index)
	{
		if (is_alive(nodes[index].size))
			continue;

		UINT node_size = unpack_size(nodes[index].size);
		if (node_size < size)
			continue;

		UINT offset = nodes[index].offset;

		if (node_size > size)
		{
			node remaining{ nodes[index].offset + size, node_size - size };
			nodes.insert(nodes.begin() + index + 1, remaining);
		}
		nodes[index].size = pack_alive(size);

		used_size += size;
		return offset;
	}

	return allocate_failed_code;
}

bool core::variable_position_allocator::deallocate(UINT deallocate_position)
{
	if (deallocate_position == allocate_failed_code)
		return false;

	for (size_t index = 0; index < nodes.size(); ++index)
	{
		if (nodes[index].offset != deallocate_position || !is_alive(nodes[index].size))
			continue;

		UINT freed_size = unpack_size(nodes[index].size);
		nodes[index].size = freed_size;

		if (index + 1 < nodes.size() && !is_alive(nodes[index + 1].size))
		{
			nodes[index].size += nodes[index + 1].size;
			nodes.erase(nodes.begin() + index + 1);
		}
		if (index > 0 && !is_alive(nodes[index - 1].size))
		{
			nodes[index - 1].size += nodes[index].size;
			nodes.erase(nodes.begin() + index);
		}

		used_size -= freed_size;
		return true;
	}

	return false;
}

bool core::variable_position_allocator::grow(UINT additional_size)
{
	if (additional_size == 0)
		return true;

	UINT new_capacity = capacity + additional_size;
	if (new_capacity < capacity || new_capacity >= alive_flag)
		return false;

	//	末尾のノードが空きならそのまま伸ばす。使用中(alive)なら新しい空きノードを追加する
	if (!nodes.empty() && !is_alive(nodes.back().size))
		nodes.back().size = unpack_size(nodes.back().size) + additional_size;
	else
		nodes.push_back({ capacity, additional_size });

	capacity = new_capacity;
	return true;
}
