#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "System/fixed_size_pool.h"

//	カスタムアロケーター使用フラグ
#define USE_CUSTOM_ALLOCATOR

//	固定長&可変長プールクラス
class memory_allocator
{
	//	サイズクラス一覧。すべて16の倍数にすることで、fixed_size_pool側のストライドが常に16バイト境界になる
	static constexpr size_t size_classes[7] = { 16, 32, 64, 128, 256, 512, 1024 };
	static constexpr size_t size_class_count = 7;

	//	ヘッダ末尾4バイトのタグに使う値。size_class_count未満ならfixed_size_pool側、これ以上なら可変長プール側
	static constexpr uint32_t large_pool_tag = 0xFFFFFFFFu;

	//	可変長プールが新しいセグメントを確保する際の最低バイト数
	static constexpr size_t large_pool_default_segment_bytes = 1 * 1024 * 1024;

	//	可変長プールの分割後、端数がこれ未満なら分割せずノード全体をそのまま使う
	static constexpr size_t variable_node_min_split_payload = 32;

#if defined(_DEBUG)
	static constexpr uint32_t canary_pattern = 0xCDCDCDCDu;
#endif	//	defined(_DEBUG)

public:
	struct variable_segment
	{
		variable_segment* next_segment;
		uint8_t* base;
		uint8_t* end;
	};

	struct variable_node
	{
		variable_node* next;
		variable_node* prev;
		variable_segment* owner_segment;
		size_t size;
		uint32_t used;
#if defined(_DEBUG)
		uint32_t canary;
		uint32_t reserved;
#endif	//	defined(_DEBUG)
		uint32_t large_pool_tag_value;
	};

	struct large_pool_state
	{
		SRWLOCK lock = SRWLOCK_INIT;
		variable_segment* first_segment = nullptr;
		size_t used_bytes = 0;
		size_t segment_count = 0;
		size_t segment_capacity_bytes = 0;
	};

	struct allocator_state
	{
		fixed_size_pool classes[7];
		large_pool_state large_pool;

		allocator_state();
	};

	struct srw_exclusive_lock
	{
		explicit srw_exclusive_lock(SRWLOCK& target);
		~srw_exclusive_lock();
		srw_exclusive_lock(const srw_exclusive_lock&) = delete;
		srw_exclusive_lock& operator=(const srw_exclusive_lock&) = delete;
	private:
		SRWLOCK& target;
	};

	struct size_class_usage
	{
		size_t block_size_in_bytes;
		size_t used_piece_count;
		size_t pooled_piece_count;
	};

	struct large_pool_usage
	{
		size_t used_bytes;
		size_t segment_count;
		size_t segment_capacity_bytes;
	};

public:
	static void* allocate(size_t requested_size, size_t alignment = 16);
	static void deallocate(void* payload_ptr);

	static void* validate_placement(void* target, size_t size);

	static void shutdown();

	static std::vector<size_class_usage> get_size_class_usages();

	static large_pool_usage get_large_pool_usage();

private:
	static allocator_state& get_state();
	static size_t align_up(size_t value, size_t alignment);
	static uint32_t find_size_class_index(size_t size);
	static uint32_t read_tag(void* payload);
	static void shutdown_at_exit();

	static void init_variable_node_tail(variable_node* node);
	static variable_segment* create_variable_segment(size_t min_bytes);
	static void split_variable_node_exact(variable_node* node, size_t exact_size);
	static void split_variable_node(variable_node* node, size_t requested_size);
	static void* try_allocate_from_node(variable_node** io_node, size_t requested_size, size_t alignment);
	static void* large_pool_allocate(large_pool_state& state, size_t size, size_t alignment);
	static void large_pool_deallocate(large_pool_state& state, void* payload);
	static bool large_pool_validate_placement(const void* payload, size_t size);
};
