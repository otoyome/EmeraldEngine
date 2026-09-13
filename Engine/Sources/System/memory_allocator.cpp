#include "System/memory_allocator.h"
#include "System/fixed_size_pool.h"
#include "System/misc.h"

#include <cstdlib>
#include <cstdint>
#include <new>

namespace
{
	constexpr size_t size_classes[] = { 16, 32, 64, 128, 256, 512, 1024 };
	constexpr size_t size_class_count = sizeof(size_classes) / sizeof(size_classes[0]);
	constexpr uint32_t large_pool_class_index = 0xFFFFFFFFu;

	constexpr size_t large_pool_default_segment_bytes = 1 * 1024 * 1024;
	constexpr size_t variable_node_min_split_payload = 32;

#if defined(_DEBUG)
	constexpr uint32_t canary_pattern = 0xCDCDCDCDu;
#endif

	size_t align_up(size_t value, size_t alignment)
	{
		return (value + alignment - 1) & ~(alignment - 1);
	}

	//	排他制御クラス
	//	@remainder	std::mutexが使えないので実装
	struct srw_exclusive_lock
	{
		explicit srw_exclusive_lock(SRWLOCK& target) : target(target) { AcquireSRWLockExclusive(&target); }
		~srw_exclusive_lock() { ReleaseSRWLockExclusive(&target); }
		srw_exclusive_lock(const srw_exclusive_lock&) = delete;
		srw_exclusive_lock& operator=(const srw_exclusive_lock&) = delete;
	private:
		SRWLOCK& target;
	};

	//	可変長ノード
	struct variable_node
	{
		variable_node* next;
		variable_node* prev;
		size_t size;		//	サイズ
		uint32_t used;		//	0 = 空き, 1 = 使用中
#if defined(_DEBUG)
		uint32_t canary;
#else	//	defined(_DEBUG)
		uint32_t reserved;
#endif	//	defined(_DEBUG)
	};
	constexpr size_t variable_node_header_size = sizeof(variable_node);

	struct variable_segment
	{
		variable_segment* next_segment;
		uint8_t* base;
		uint8_t* end;
	};

	struct large_pool_state
	{
		SRWLOCK lock = SRWLOCK_INIT;
		variable_segment* first_segment = nullptr;
		size_t used_bytes = 0;
		size_t segment_count = 0;
		size_t segment_capacity_bytes = 0;
	};

	bool variable_segment_owns(const variable_segment* segment, const void* payload)
	{
		const uint8_t* p = static_cast<const uint8_t*>(payload);
		return segment->base + variable_node_header_size <= p && p < segment->end;
	}

	variable_segment* create_variable_segment(size_t min_bytes)
	{
		size_t segment_bytes = min_bytes > large_pool_default_segment_bytes ? min_bytes : large_pool_default_segment_bytes;

		variable_segment* segment = static_cast<variable_segment*>(std::malloc(sizeof(variable_segment)));
		if (!segment)
			return nullptr;

		uint8_t* base = static_cast<uint8_t*>(std::malloc(segment_bytes));
		if (!base)
		{
			std::free(segment);
			return nullptr;
		}

		segment->next_segment = nullptr;
		segment->base = base;
		segment->end = base + segment_bytes;

		variable_node* node = reinterpret_cast<variable_node*>(base);
		node->next = nullptr;
		node->prev = nullptr;
		node->size = segment_bytes - variable_node_header_size;
		node->used = 0;
#if defined(_DEBUG)
		node->canary = canary_pattern;
#endif	//	defined(_DEBUG)
		return segment;
	}

	void split_variable_node_exact(variable_node* node, size_t exact_size)
	{
		variable_node* remainder = reinterpret_cast<variable_node*>(
			reinterpret_cast<uint8_t*>(node + 1) + exact_size);
		remainder->size = node->size - exact_size - variable_node_header_size;
		remainder->used = 0;
#if defined(_DEBUG)
		remainder->canary = canary_pattern;
#endif	//	defined(_DEBUG)
		remainder->next = node->next;
		remainder->prev = node;
		if (remainder->next)
			remainder->next->prev = remainder;

		node->size = exact_size;
		node->next = remainder;
	}

	void split_variable_node(variable_node* node, size_t requested_size)
	{
		size_t remaining = node->size - requested_size;
		if (remaining < variable_node_header_size + variable_node_min_split_payload)
			return;

		split_variable_node_exact(node, requested_size);
	}

	void* try_allocate_from_node(variable_node** io_node, size_t requested_size, size_t alignment)
	{
		variable_node* node = *io_node;
		uint8_t* payload = reinterpret_cast<uint8_t*>(node + 1);
		size_t padding = static_cast<size_t>(
			align_up(reinterpret_cast<size_t>(payload), alignment) - reinterpret_cast<size_t>(payload));

		if (padding != 0)
		{
			if (padding < variable_node_header_size)
				padding += align_up(variable_node_header_size - padding, alignment);

			if (node->size < padding + requested_size)
				return nullptr;

			split_variable_node_exact(node, padding - variable_node_header_size);
			node = node->next;
			*io_node = node;
		}

		if (node->size < requested_size)
			return nullptr;

		split_variable_node(node, requested_size);
		node->used = 1;
#if defined(_DEBUG)
		node->canary = canary_pattern;
#endif	//	defined(_DEBUG)
		return node + 1;
	}

	void* large_pool_allocate(large_pool_state& state, size_t size, size_t alignment)
	{
		size_t requested = align_up(size, 16);

		for (variable_segment* segment = state.first_segment; segment; segment = segment->next_segment)
		{
			for (variable_node* node = reinterpret_cast<variable_node*>(segment->base); node; node = node->next)
			{
				if (node->used)
					continue;

				variable_node* candidate = node;
				if (void* payload = try_allocate_from_node(&candidate, requested, alignment))
				{
					state.used_bytes += requested;
					return payload;
				}
			}
		}

		size_t min_bytes = variable_node_header_size * 2 + requested + alignment;
		variable_segment* segment = create_variable_segment(min_bytes);
		if (!segment)
			return nullptr;

		segment->next_segment = state.first_segment;
		state.first_segment = segment;
		state.segment_count += 1;
		state.segment_capacity_bytes += static_cast<size_t>(segment->end - segment->base);

		variable_node* node = reinterpret_cast<variable_node*>(segment->base);
		void* payload = try_allocate_from_node(&node, requested, alignment);
		_ASSERT_EXPR(payload != nullptr, L"memory_allocator: newly created segment could not satisfy the request that required it");
		if (payload)
			state.used_bytes += requested;
		return payload;
	}

	bool large_pool_deallocate(large_pool_state& state, void* payload)
	{
		for (variable_segment* segment = state.first_segment; segment; segment = segment->next_segment)
		{
			if (!variable_segment_owns(segment, payload))
				continue;

			variable_node* node = reinterpret_cast<variable_node*>(payload) - 1;
#if defined(_DEBUG)
			_ASSERT_EXPR(node->canary == canary_pattern,
				L"memory_allocator: large pool header canary corrupted (buffer overrun?)");
#endif	//	defined(_DEBUG)
			_ASSERT_EXPR(node->used == 1, L"memory_allocator: double free detected (large allocation)");

			state.used_bytes -= node->size;
			node->used = 0;

			//	後ろの空きノードと結合
			if (node->next && !node->next->used)
			{
				node->size += node->next->size + variable_node_header_size;
				node->next = node->next->next;
				if (node->next)
					node->next->prev = node;
			}
			//	前の空きノードと結合
			if (node->prev && !node->prev->used)
			{
				node->prev->size += node->size + variable_node_header_size;
				node->prev->next = node->next;
				if (node->next)
					node->next->prev = node->prev;
			}
			return true;
		}
		return false;
	}

	bool large_pool_validate_placement(const large_pool_state& state, const void* payload, size_t size)
	{
		for (variable_segment* segment = state.first_segment; segment; segment = segment->next_segment)
		{
			if (!variable_segment_owns(segment, payload))
				continue;

			const variable_node* node = reinterpret_cast<const variable_node*>(payload) - 1;
#if defined(_DEBUG)
			if (node->canary != canary_pattern)
				return false;
#endif	//	defined(_DEBUG)
			if (!node->used)
				return false;

			return node->size >= size;
		}
		return false;
	}

	////////////////////////////////////////////////////////////////////////////////////////////////////
	struct allocator_state
	{
		fixed_size_pool classes[size_class_count];
		large_pool_state large_pool;

		allocator_state()
		{
			for (uint32_t index = 0; index < size_class_count; ++index)
				classes[index].initialize(size_classes[index]);
		}
	};

	allocator_state& get_state()
	{
		static allocator_state state;
		return state;
	}

	uint32_t find_size_class_index(size_t size)
	{
		for (uint32_t index = 0; index < size_class_count; ++index)
		{
			if (size_classes[index] >= size)
				return index;
		}
		return large_pool_class_index;
	}
}

void* memory_allocator::allocate(size_t requested_size, size_t alignment)
{
	size_t actual_size = requested_size == 0 ? 1 : requested_size;
	allocator_state& state = get_state();

	uint32_t class_index = alignment <= 16 ? find_size_class_index(actual_size) : large_pool_class_index;

	void* payload = nullptr;
	if (class_index != large_pool_class_index)
	{
		payload = state.classes[class_index].allocate(alignment);
	}
	else
	{
		srw_exclusive_lock lock(state.large_pool.lock);
		payload = large_pool_allocate(state.large_pool, actual_size, alignment);
	}

#if defined(_DEBUG)
	_ASSERT_EXPR(!payload || reinterpret_cast<size_t>(payload) % alignment == 0,
		L"memory_allocator: returned pointer does not satisfy the requested alignment");
#endif	//	defined(_DEBUG)
	return payload;
}

void memory_allocator::deallocate(void* payload_ptr)
{
	if (!payload_ptr)
		return;

	allocator_state& state = get_state();

	for (uint32_t class_index = 0; class_index < size_class_count; ++class_index)
	{
		if (state.classes[class_index].deallocate(payload_ptr))
			return;
	}

	srw_exclusive_lock lock(state.large_pool.lock);
	bool freed = large_pool_deallocate(state.large_pool, payload_ptr);
	_ASSERT_EXPR(freed, L"memory_allocator: deallocate() received a pointer that was not allocated by this allocator");
}

void* memory_allocator::validate_placement(void* target, size_t size)
{
	if (!target)
		return nullptr;

	allocator_state& state = get_state();

	for (uint32_t class_index = 0; class_index < size_class_count; ++class_index)
	{
		if (state.classes[class_index].validate_placement(target, size))
			return target;
	}

	srw_exclusive_lock lock(state.large_pool.lock);
	return large_pool_validate_placement(state.large_pool, target, size) ? target : nullptr;
}

std::vector<memory_allocator::size_class_usage> memory_allocator::get_size_class_usages()
{
	allocator_state& state = get_state();

	std::vector<size_class_usage> usages;
	usages.reserve(size_class_count);
	for (uint32_t class_index = 0; class_index < size_class_count; ++class_index)
	{
		fixed_size_pool::usage pool_usage = state.classes[class_index].get_usage();

		size_class_usage usage;
		usage.block_size_in_bytes = pool_usage.block_size_in_bytes;
		usage.used_piece_count = pool_usage.used_piece_count;
		usage.pooled_piece_count = pool_usage.pooled_piece_count;
		usages.emplace_back(usage);
	}
	return usages;
}

memory_allocator::large_pool_usage memory_allocator::get_large_pool_usage()
{
	allocator_state& state = get_state();
	srw_exclusive_lock lock(state.large_pool.lock);

	large_pool_usage usage;
	usage.used_bytes = state.large_pool.used_bytes;
	usage.segment_count = state.large_pool.segment_count;
	usage.segment_capacity_bytes = state.large_pool.segment_capacity_bytes;
	return usage;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void* operator new(size_t size)
{
#if defined(USE_CUSTOM_ALLOCATOR)
	void* ptr = memory_allocator::allocate(size);
#else	//	defined(USE_CUSTOM_ALLOCATOR)
	void* ptr = std::malloc(size);
#endif	//	defined(USE_CUSTOM_ALLOCATOR)
	if (!ptr)
		throw std::bad_alloc();
	return ptr;
}

void* operator new[](size_t size)
{
	return operator new(size);
}

void operator delete(void* ptr) noexcept
{
	if (!ptr)
		return;
#if defined(USE_CUSTOM_ALLOCATOR)
	memory_allocator::deallocate(ptr);
#else	//	defined(USE_CUSTOM_ALLOCATOR)
	std::free(ptr);
#endif	//	defined(USE_CUSTOM_ALLOCATOR)
}

void operator delete[](void* ptr) noexcept
{
	operator delete(ptr);
}

void operator delete(void* ptr, size_t) noexcept
{
	operator delete(ptr);
}

void operator delete[](void* ptr, size_t) noexcept
{
	operator delete(ptr);
}
