#include "System/fixed_size_pool.h"
#include "System/misc.h"

#include <cstdlib>
#include <cstdint>

namespace
{
	constexpr size_t arena_slab_bytes = 64 * 1024;

#if defined(_DEBUG)
	constexpr uint32_t canary_pattern = 0xCDCDCDCDu;
#endif	//	defined(_DEBUG)

	size_t align_up(size_t value, size_t alignment)
	{
		return (value + alignment - 1) & ~(alignment - 1);
	}

	struct piece_header
	{
		uint32_t state;
#if defined(_DEBUG)
		uint32_t canary;
		uint32_t reserved[2];
#else	//	defined(_DEBUG)
		uint32_t reserved[3];
#endif	//	defined(_DEBUG)
	};
	constexpr size_t piece_header_size = sizeof(piece_header);
	enum piece_state : uint32_t { piece_state_free = 0, piece_state_used = 1 };

	//	îrëºêßå‰ÉNÉâÉX
	struct srw_exclusive_lock
	{
		explicit srw_exclusive_lock(SRWLOCK& target) : target(target) { AcquireSRWLockExclusive(&target); }
		~srw_exclusive_lock() { ReleaseSRWLockExclusive(&target); }
		srw_exclusive_lock(const srw_exclusive_lock&) = delete;
		srw_exclusive_lock& operator=(const srw_exclusive_lock&) = delete;
	private:
		SRWLOCK& target;
	};
}

struct fixed_size_pool::arena
{
	arena* next_arena;
	uint8_t* pool;
	uint8_t* pool_end;
	size_t stride;
	size_t used_count;
	size_t piece_count;
	size_t next_free_hint;
};

fixed_size_pool::~fixed_size_pool()
{
}

void fixed_size_pool::initialize(size_t new_piece_size)
{
	piece_size = new_piece_size;
}

fixed_size_pool::arena* fixed_size_pool::create_arena()
{
	size_t stride = piece_header_size + piece_size;
	size_t piece_count = arena_slab_bytes / stride;
	if (piece_count == 0)
		piece_count = 1;

	arena* new_arena = static_cast<arena*>(std::malloc(sizeof(arena)));
	if (!new_arena)
		return nullptr;

	uint8_t* pool = static_cast<uint8_t*>(std::malloc(stride * piece_count));
	if (!pool)
	{
		std::free(new_arena);
		return nullptr;
	}

	new_arena->next_arena = nullptr;
	new_arena->pool = pool;
	new_arena->pool_end = pool + stride * piece_count;
	new_arena->stride = stride;
	new_arena->used_count = 0;
	new_arena->piece_count = piece_count;
	new_arena->next_free_hint = 0;

	for (uint8_t* piece = pool; piece < new_arena->pool_end; piece += stride)
	{
		piece_header* header = reinterpret_cast<piece_header*>(piece);
		header->state = piece_state_free;
#if defined(_DEBUG)
		header->canary = canary_pattern;
#endif
	}
	return new_arena;
}

void* fixed_size_pool::allocate_from_arena(arena* target, size_t alignment)
{
	uint8_t* piece = target->pool + target->next_free_hint;
	while (piece < target->pool_end)
	{
		piece_header* header = reinterpret_cast<piece_header*>(piece);
		if (header->state == piece_state_free)
		{
			uint8_t* payload = piece + piece_header_size;
			if (align_up(reinterpret_cast<size_t>(payload), alignment) == reinterpret_cast<size_t>(payload))
			{
#if defined(_DEBUG)
				_ASSERT_EXPR(header->canary == canary_pattern,
					L"fixed_size_pool: header canary corrupted (buffer underrun?)");
#endif	//	defined(_DEBUG)
				header->state = piece_state_used;
				++target->used_count;

				size_t freed_to = static_cast<size_t>(piece + target->stride - target->pool);
				if (freed_to > target->next_free_hint)
					target->next_free_hint = freed_to;

				return payload;
			}
		}
		piece += target->stride;
	}
	return nullptr;
}

void* fixed_size_pool::allocate(size_t alignment)
{
	srw_exclusive_lock scoped_lock(lock);

	for (arena* target = first_arena; target; target = target->next_arena)
	{
		if (void* payload = allocate_from_arena(target, alignment))
			return payload;
	}

	arena* new_arena = create_arena();
	if (!new_arena)
		return nullptr;
	new_arena->next_arena = first_arena;
	first_arena = new_arena;

	return allocate_from_arena(new_arena, alignment);
}

bool fixed_size_pool::deallocate(void* payload)
{
	srw_exclusive_lock scoped_lock(lock);

	const uint8_t* p = static_cast<const uint8_t*>(payload);
	for (arena* target = first_arena; target; target = target->next_arena)
	{
		if (!(target->pool + piece_header_size <= p && p < target->pool_end))
			continue;

		uint8_t* piece = static_cast<uint8_t*>(payload) - piece_header_size;
		size_t piece_offset = static_cast<size_t>(piece - target->pool);
		if (piece_offset % target->stride != 0)
			continue;

		piece_header* header = reinterpret_cast<piece_header*>(piece);
#if defined(_DEBUG)
		_ASSERT_EXPR(header->canary == canary_pattern,
			L"fixed_size_pool: header canary corrupted (buffer overrun from neighbor?)");
#endif	//	defined(_DEBUG)

		_ASSERT_EXPR(header->state == piece_state_used, L"fixed_size_pool: double free detected");
		header->state = piece_state_free;
		--target->used_count;

		if (piece_offset < target->next_free_hint)
			target->next_free_hint = piece_offset;
		return true;
	}
	return false;
}

bool fixed_size_pool::validate_placement(void* payload, size_t size) const
{
	srw_exclusive_lock scoped_lock(lock);

	const uint8_t* p = static_cast<const uint8_t*>(payload);
	for (const arena* target = first_arena; target; target = target->next_arena)
	{
		if (!(target->pool + piece_header_size <= p && p < target->pool_end))
			continue;

		const uint8_t* piece = p - piece_header_size;
		size_t piece_offset = static_cast<size_t>(piece - target->pool);
		if (piece_offset % target->stride != 0)
			continue;

		const piece_header* header = reinterpret_cast<const piece_header*>(piece);
#if defined(_DEBUG)
		if (header->canary != canary_pattern)
			return false;
#endif	//	defined(_DEBUG)
		if (header->state != piece_state_used)
			return false;

		return target->stride - piece_header_size >= size;
	}
	return false;
}

fixed_size_pool::usage fixed_size_pool::get_usage() const
{
	srw_exclusive_lock scoped_lock(lock);

	usage result;
	result.block_size_in_bytes = piece_size;
	result.used_piece_count = 0;
	result.pooled_piece_count = 0;
	for (const arena* target = first_arena; target; target = target->next_arena)
	{
		result.used_piece_count += target->used_count;
		result.pooled_piece_count += target->piece_count;
	}
	return result;
}
