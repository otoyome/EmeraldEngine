#include "System/fixed_size_pool.h"
#include "System/misc.h"

#include <cstdlib>
#include <cstdint>

//	排他制御クラス。std::mutexはMSVC実装だと初回ロック時に内部でヒープ確保を行うことがあり、
//	operator new内で使うと無限再帰(スタックオーバーフロー)を起こすため、確保を伴わないSRWLOCKを直接使う
struct fixed_size_pool::srw_exclusive_lock
{
	explicit srw_exclusive_lock(SRWLOCK& target) : target(target) { AcquireSRWLockExclusive(&target); }
	~srw_exclusive_lock() { ReleaseSRWLockExclusive(&target); }
	srw_exclusive_lock(const srw_exclusive_lock&) = delete;
	srw_exclusive_lock& operator=(const srw_exclusive_lock&) = delete;
private:
	SRWLOCK& target;
};

//	1ピース分のヘッダ。owner_arenaを直接持つことで、deallocate時にアリーナを総当たりせずO(1)で
//	対象アリーナへ辿り着ける。class_indexは必ず構造体の末尾(payloadの直前)に置く。memory_allocator側が
//	payload-4バイトを読むだけで「どのサイズクラスのプールが払い出したポインタか」をO(1)で判定するために使う。
//	全サイズクラスでヘッダサイズを16バイト(デバッグ時32バイト)固定にし、ペイロードの16バイトアライメントを保証している
struct fixed_size_pool::piece_header
{
	arena* owner_arena;
	uint32_t state;		//	0 = 空き, 1 = 使用中
#if defined(_DEBUG)
	uint32_t canary;
	uint32_t reserved[3];
#endif	//	defined(_DEBUG)
	uint32_t class_index;	//	末尾固定。payloadの直前4バイト
};

//	固定長プールの1スラブ。満杯になったら(呼び出し側が)新しいスラブを連結する形にして、
//	起動時に指定した個数を超えると確保できなくなる、という制約を無くしている。
//	arena構造体自身もVirtualAllocした領域の先頭に埋め込むため、reservedでサイズを16バイト境界に揃えている
//	(こうしないと直後に続くピース列の先頭が16バイトアライメントからズレる)
struct fixed_size_pool::arena
{
	arena* next_arena;
	uint8_t* pool;
	uint8_t* pool_end;
	size_t stride;
	size_t used_count;
	size_t piece_count;
	size_t next_free_hint;		//	空きがあるかもしれない最小オフセット
	size_t reserved;
};

const size_t fixed_size_pool::arena_slab_bytes = 64 * 1024;
const size_t fixed_size_pool::piece_header_size = sizeof(piece_header);
#if defined(_DEBUG)
const uint32_t fixed_size_pool::canary_pattern = 0xCDCDCDCDu;
#endif

size_t fixed_size_pool::align_up(size_t value, size_t alignment)
{
	return (value + alignment - 1) & ~(alignment - 1);
}

fixed_size_pool::~fixed_size_pool()
{
	//	スラブはVirtualAllocでプロセスに直接確保しており、意図的に一切解放しない。
	//	以前はプロセス終了時にatexitで明示的に解放していたが、静的初期化順序次第では
	//	このプールより後に構築された(=先に破棄される)静的オブジェクトが、このプール由来の
	//	メモリを解放しようとする前にスラブを消してしまい、解放済み領域への書き込みでクラッシュする
	//	不具合があった。VirtualAllocで確保した領域はCRTのデバッグヒープに追跡されないため、
	//	そもそも「未解放」としてリーク検出に引っかからない。解放せず放置する方が安全
}

void fixed_size_pool::initialize(uint32_t new_class_index, size_t new_piece_size)
{
	class_index = new_class_index;
	piece_size = new_piece_size;
}

fixed_size_pool::arena* fixed_size_pool::create_arena()
{
	size_t stride = piece_header_size + piece_size;
	size_t piece_count = arena_slab_bytes / stride;
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
	new_arena->pool = pool;
	new_arena->pool_end = pool + stride * piece_count;
	new_arena->stride = stride;
	new_arena->used_count = 0;
	new_arena->piece_count = piece_count;
	new_arena->next_free_hint = 0;

	for (uint8_t* piece = pool; piece < new_arena->pool_end; piece += stride)
	{
		piece_header* header = reinterpret_cast<piece_header*>(piece);
		header->owner_arena = new_arena;
		header->state = piece_state_free;
		header->class_index = class_index;
#if defined(_DEBUG)
		header->canary = canary_pattern;
#endif
	}
	return new_arena;
}

//	ヒントの位置からスキャンし、空きかつアライメント条件を満たすピースを探す
void* fixed_size_pool::allocate_from_arena(arena* target, uint32_t class_index, size_t alignment)
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
				_ASSERT_EXPR(header->owner_arena == target && header->class_index == class_index,
					L"fixed_size_pool: header corrupted (owner/class mismatch)");

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
		if (void* payload = allocate_from_arena(target, class_index, alignment))
			return payload;
	}

	arena* new_arena = create_arena();
	if (!new_arena)
		return nullptr;
	new_arena->next_arena = first_arena;
	first_arena = new_arena;

	return allocate_from_arena(new_arena, class_index, alignment);
}

void fixed_size_pool::deallocate(void* payload)
{
	//	呼び出し元(memory_allocator)がヘッダ末尾のclass_indexタグを見て、既にこのプールが
	//	払い出したポインタだと確定した上で呼んでいる。owner_arenaにより対象アリーナを
	//	総当たりせずO(1)で特定できる
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

	//	使用中フラグを明示的にチェックすることで二重解放を検出する
	_ASSERT_EXPR(header->state == piece_state_used, L"fixed_size_pool: double free detected");
	header->state = piece_state_free;
	--target->used_count;

	if (piece_offset < target->next_free_hint)
		target->next_free_hint = piece_offset;
}

bool fixed_size_pool::validate_placement(void* payload, size_t size) const
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
