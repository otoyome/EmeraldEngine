#pragma once

#include <cstddef>
#include <cstdint>

#include "System/misc.h"

//	@tool 固定長ブロックのプールクラス(EME::System::CFixedMemoryManager の設計を踏襲)
//	@tool ヒントスキャン方式で空きピースを探し、満杯になったら新しいスラブを連結して動的に成長する
//	@tool (EME版は起動時に指定した個数が上限だったが、こちらはその制約を無くしている)
//	@tool 各ピースのヘッダに所属アリーナへの直接ポインタを持たせており、deallocate/validate_placementは
//	@tool アリーナを総当たりせずO(1)で対象アリーナへ辿り着ける
//	@tool memory_allocatorの内部状態として使われる想定のため、std::malloc/std::free以外の動的確保
//	@tool (std::vector・newなど)は内部で一切行わない(operator newの中から呼ばれるため、自分自身への
//	@tool 再入を避ける必要がある)
class fixed_size_pool
{
public:
	fixed_size_pool() = default;
	~fixed_size_pool();

	fixed_size_pool(const fixed_size_pool&) = delete;
	fixed_size_pool& operator=(const fixed_size_pool&) = delete;

	//	@tool class_index はヘッダに埋め込む識別子(memory_allocator側のサイズクラス配列の添字)。
	//	@tool piece_size は1ピースあたりのバイト数。スラブは実際に確保が必要になるまで作らない
	void initialize(uint32_t class_index, size_t piece_size);

	void* allocate(size_t alignment);

	//	@tool payloadはこのプール(このclass_index)が払い出したポインタである前提で呼ぶ
	//	@tool (呼び出し元はヘッダのタグを見て既にディスパッチ済みのため、ここでは所有アリーナかの
	//	@tool 検証だけを行う)
	void deallocate(void* payload);

	//	@tool payloadが「現在使用中でsizeバイト以上のピース」であればtrueを返す(placement newの検証用)
	bool validate_placement(void* payload, size_t size) const;

	struct usage
	{
		size_t block_size_in_bytes;
		size_t used_piece_count;
		size_t pooled_piece_count;
	};
	usage get_usage() const;

	//	@tool アリーナを全て解放する。プロセス終了直前(memory_allocatorのatexitフック)専用
	void release_all();

	//	@tool 名前空間内のヘッダ構造体からowner_arenaを指せるよう、前方宣言だけpublicにしている
	//	@tool (実体はfixed_size_pool.cppでしか定義していないので、実質的にはopaqueなまま)
	struct arena;

private:
	arena* create_arena();
	static void* allocate_from_arena(arena* target, uint32_t class_index, size_t alignment);

private:
	mutable SRWLOCK lock = SRWLOCK_INIT;	//	@tool validate_placement/get_usageがconstのままロックできるようにmutableにしている
	arena* first_arena = nullptr;
	size_t piece_size = 0;
	uint32_t class_index = 0;
};
