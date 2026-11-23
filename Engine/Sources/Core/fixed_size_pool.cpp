#include "Core/fixed_size_pool.h"
#include "System/misc.h"

#include <cstdlib>
#include <cstdint>

//	排他制御クラス
//	std::mutexはMSVC環境だと初回ロック時に内部でヒープ確保が行われることがあるため自作
struct core::fixed_size_pool::srw_exclusive_lock
{
	explicit srw_exclusive_lock(SRWLOCK& target) : target(target) { AcquireSRWLockExclusive(&target); }
	~srw_exclusive_lock() { ReleaseSRWLockExclusive(&target); }
	srw_exclusive_lock(const srw_exclusive_lock&) = delete;
	srw_exclusive_lock& operator=(const srw_exclusive_lock&) = delete;
private:
	SRWLOCK& target;
};

//	1ピース分のヘッダー
struct core::fixed_size_pool::piece_header
{
	arena* owner_arena;
	uint32_t state;		//	0 = 空き, 1 = 使用中
#if defined(_DEBUG)
	uint32_t canary;
	uint32_t reserved[3];
#endif	//	defined(_DEBUG)
	uint32_t class_index;
};

//	固定長プールの1スラブ
struct core::fixed_size_pool::arena
{
	arena* next_arena;
	arena* prev_arena;
	uint8_t* pool;
	uint8_t* pool_end;
	size_t stride;
	size_t used_count;
	size_t piece_count;
	void* free_list_head;
	arena* next_available;	//	空きピースを持つアリーナだけをつなぐリスト
	arena* prev_available;
};

const size_t core::fixed_size_pool::arena_slab_bytes = 64 * 1024;
const size_t core::fixed_size_pool::piece_header_size = sizeof(piece_header);
#if defined(_DEBUG)
const uint32_t core::fixed_size_pool::canary_pattern = 0xCDCDCDCDu;
#endif // defined(_DEBUG)

core::fixed_size_pool::~fixed_size_pool()
{
}

void core::fixed_size_pool::initialize(uint32_t new_class_index, size_t new_piece_size)
{
	class_index = new_class_index;
	piece_size = new_piece_size;
}

core::fixed_size_pool::arena* core::fixed_size_pool::create_arena()
{
	//	ピース列の先頭を16バイト境界に揃える
	static_assert(sizeof(arena) % 16 == 0, "fixed_size_pool::arena must be a multiple of 16 bytes");

	size_t stride = piece_header_size + piece_size;
	//	arena構造体ぶんを差し引き、全体がVirtualAllocの確保粒度に収まるようにする
	size_t piece_count = (arena_slab_bytes - sizeof(arena)) / stride;
	if (piece_count == 0)
		piece_count = 1;

	//	arena構造体自身とピース列を1回のVirtualAllocでまとめて確保する
	size_t total_bytes = sizeof(arena) + stride * piece_count;
	void* block = VirtualAlloc(nullptr, total_bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if (!block)
		return nullptr;

	arena* new_arena = static_cast<arena*>(block);
	uint8_t* pool = static_cast<uint8_t*>(block) + sizeof(arena);

	new_arena->next_arena = nullptr;
	new_arena->prev_arena = nullptr;
	new_arena->pool = pool;
	new_arena->pool_end = pool + stride * piece_count;
	new_arena->stride = stride;
	new_arena->used_count = 0;
	new_arena->piece_count = piece_count;
	new_arena->free_list_head = nullptr;
	new_arena->next_available = nullptr;
	new_arena->prev_available = nullptr;

	for (size_t index = piece_count; index-- > 0; )
	{
		uint8_t* piece = pool + index * stride;
		piece_header* header = reinterpret_cast<piece_header*>(piece);
		header->owner_arena = new_arena;
		header->state = piece_state_free;
		header->class_index = class_index;
#if defined(_DEBUG)
		header->canary = canary_pattern;
#endif	//	defined(_DEBUG)

		uint8_t* payload = piece + piece_header_size;
		*reinterpret_cast<void**>(payload) = new_arena->free_list_head;
		new_arena->free_list_head = payload;
	}
	return new_arena;
}

void* core::fixed_size_pool::allocate_from_arena(arena* target)
{
	if (!target->free_list_head)
		return nullptr;

	uint8_t* payload = static_cast<uint8_t*>(target->free_list_head);
	piece_header* header = reinterpret_cast<piece_header*>(payload - piece_header_size);

#if defined(_DEBUG)
	_ASSERT_EXPR(header->canary == canary_pattern,
		L"fixed_size_pool: header canary corrupted (buffer underrun?)");
#endif	//	defined(_DEBUG)
	_ASSERT_EXPR(header->owner_arena == target && header->class_index == class_index,
		L"fixed_size_pool: header corrupted (owner/class mismatch)");
	_ASSERT_EXPR(header->state == piece_state_free, L"fixed_size_pool: free list corrupted (piece already in use)");

	target->free_list_head = *reinterpret_cast<void**>(payload);
	header->state = piece_state_used;
	++target->used_count;

	return payload;
}

void* core::fixed_size_pool::allocate(size_t alignment)
{
	_ASSERT_EXPR(alignment <= 16 && (16 % alignment) == 0,
		L"fixed_size_pool: this pool only guarantees 16-byte alignment");

	srw_exclusive_lock scoped_lock(lock);

	//	空きのあるアリーナだけをつないだリストの先頭から取る
	arena* target = available_head;
	if (!target)
	{
		target = create_arena();
		if (!target)
			return nullptr;

		target->next_arena = first_arena;
		if (first_arena)
			first_arena->prev_arena = target;
		first_arena = target;

		link_available(target);
		++empty_arena_count;
	}

	if (target->used_count == 0)
		--empty_arena_count;

	void* payload = allocate_from_arena(target);

	//	満杯になったアリーナは空きありリストから外す
	if (!target->free_list_head)
		unlink_available(target);

	return payload;
}

void core::fixed_size_pool::link_available(arena* target)
{
	target->prev_available = nullptr;
	target->next_available = available_head;
	if (available_head)
		available_head->prev_available = target;
	available_head = target;
}

void core::fixed_size_pool::unlink_available(arena* target)
{
	if (target->prev_available)
		target->prev_available->next_available = target->next_available;
	else
		available_head = target->next_available;
	if (target->next_available)
		target->next_available->prev_available = target->prev_available;

	target->next_available = nullptr;
	target->prev_available = nullptr;
}

void core::fixed_size_pool::release_arena_if_redundant(arena* target)
{
	if (target->used_count != 0)
		return;

	//	空になったアリーナを1つだけ予備として残す
	++empty_arena_count;
	if (empty_arena_count <= 1)
		return;

	//	空のアリーナは必ず空きありリストに入っている
	unlink_available(target);

	if (target->prev_arena)
		target->prev_arena->next_arena = target->next_arena;
	else
		first_arena = target->next_arena;
	if (target->next_arena)
		target->next_arena->prev_arena = target->prev_arena;

	--empty_arena_count;
	VirtualFree(target, 0, MEM_RELEASE);
}

void core::fixed_size_pool::deallocate(void* payload)
{
	uint8_t* piece = static_cast<uint8_t*>(payload) - piece_header_size;
	piece_header* header = reinterpret_cast<piece_header*>(piece);

#if defined(_DEBUG)
	_ASSERT_EXPR(header->canary == canary_pattern,
		L"fixed_size_pool: header canary corrupted (buffer overrun from neighbor?)");
#endif	//	defined(_DEBUG)
	_ASSERT_EXPR(header->class_index == class_index, L"fixed_size_pool: pointer belongs to a different size class");

	arena* target = header->owner_arena;
	size_t piece_offset = static_cast<size_t>(piece - target->pool);
	_ASSERT_EXPR(target->pool + piece_header_size <= static_cast<uint8_t*>(payload) && piece < target->pool_end
		&& piece_offset % target->stride == 0,
		L"fixed_size_pool: header corrupted (owner_arena out of range)");

	srw_exclusive_lock scoped_lock(lock);

	//	二重解放チェック
	_ASSERT_EXPR(header->state == piece_state_used, L"fixed_size_pool: double free detected");
	header->state = piece_state_free;

	//	返却前に満杯だったアリーナは、空きありリストへ戻す
	bool was_full = (target->free_list_head == nullptr);

	*reinterpret_cast<void**>(payload) = target->free_list_head;
	target->free_list_head = payload;
	--target->used_count;

	if (was_full)
		link_available(target);

	release_arena_if_redundant(target);
}

bool core::fixed_size_pool::validate_placement(void* payload, size_t size) const
{
	const uint8_t* piece = static_cast<const uint8_t*>(payload) - piece_header_size;
	const piece_header* header = reinterpret_cast<const piece_header*>(piece);

	if (header->class_index != class_index)
		return false;

	srw_exclusive_lock scoped_lock(lock);

#if defined(_DEBUG)
	if (header->canary != canary_pattern)
		return false;
#endif	//	defined(_DEBUG)
	if (header->state != piece_state_used)
		return false;

	const arena* target = header->owner_arena;
	return target->stride - piece_header_size >= size;
}

core::fixed_size_pool::usage core::fixed_size_pool::get_usage() const
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
