#pragma once

#include <cstddef>

#include "System/misc.h"

//	固定長プールクラス
class fixed_size_pool
{
public:
	fixed_size_pool() = default;
	~fixed_size_pool();

	fixed_size_pool(const fixed_size_pool&) = delete;
	fixed_size_pool& operator=(const fixed_size_pool&) = delete;

	void initialize(size_t piece_size);

	void* allocate(size_t alignment);
	bool deallocate(void* payload);

	bool validate_placement(void* payload, size_t size) const;

	struct usage
	{
		size_t block_size_in_bytes;
		size_t used_piece_count;
		size_t pooled_piece_count;
	};
	usage get_usage() const;

private:
	struct arena;

	arena* create_arena();
	static void* allocate_from_arena(arena* target, size_t alignment);

private:
	mutable SRWLOCK lock = SRWLOCK_INIT;
	arena* first_arena = nullptr;
	size_t piece_size = 0;
};
