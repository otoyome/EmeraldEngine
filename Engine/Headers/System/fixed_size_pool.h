#pragma once

#include <cstddef>
#include <cstdint>

#include "System/misc.h"

//	固定長プールクラス
class fixed_size_pool
{
public:
	fixed_size_pool() = default;
	~fixed_size_pool();

	fixed_size_pool(const fixed_size_pool&) = delete;
	fixed_size_pool& operator=(const fixed_size_pool&) = delete;

	void initialize(uint32_t class_index, size_t piece_size);

	void* allocate(size_t alignment);
	void deallocate(void* payload);

	bool validate_placement(void* payload, size_t size) const;

	//	プロセス終了直前(memory_allocatorのatexitフック)専用
	void release_all();

	struct usage
	{
		size_t block_size_in_bytes;
		size_t used_piece_count;
		size_t pooled_piece_count;
	};
	usage get_usage() const;

private:
	struct arena;
	struct piece_header;
	struct srw_exclusive_lock;
	enum piece_state : uint32_t { piece_state_free = 0, piece_state_used = 1 };

	static const size_t arena_slab_bytes;
	static const size_t piece_header_size;
#if defined(_DEBUG)
	static const uint32_t canary_pattern;
#endif

	static size_t align_up(size_t value, size_t alignment);
	arena* create_arena();
	static void* allocate_from_arena(arena* target, uint32_t class_index, size_t alignment);

private:
	mutable SRWLOCK lock = SRWLOCK_INIT;
	arena* first_arena = nullptr;
	size_t piece_size = 0;
	uint32_t class_index = 0;
};
