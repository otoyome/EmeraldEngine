#pragma once

#include <cstddef>
#include <vector>

//	カスタムアロケーター使用フラグ
#define USE_CUSTOM_ALLOCATOR

//	固定長&可変長プールクラス
class memory_allocator
{
public:
	static void* allocate(size_t requested_size, size_t alignment = 16);
	static void deallocate(void* payload_ptr);

	static void* validate_placement(void* target, size_t size);

	//	プロセス終了時にスラブ/セグメントを全解放する(CRTのリーク検出誤検知対策。自動的にatexit登録される)
	static void shutdown();

	//	デバッグ表示用の使用量情報
	struct size_class_usage
	{
		size_t block_size_in_bytes;
		size_t used_piece_count;
		size_t pooled_piece_count;
	};
	static std::vector<size_class_usage> get_size_class_usages();

	//	デバッグ表示用の使用量情報
	struct large_pool_usage
	{
		size_t used_bytes;
		size_t segment_count;
		size_t segment_capacity_bytes;
	};
	static large_pool_usage get_large_pool_usage();
};
