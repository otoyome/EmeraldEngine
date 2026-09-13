#pragma once

#include <wrl.h>
#include <list>

//	可変長アロケーション座標だけ管理クラス
class variable_position_allocator
{
public:
	static const UINT allocate_failed_code = 0xFFFFFFFF;

public:
	//	pool_size = 2,147,483,647バイトまで
	bool initialize(UINT pool_size);

	//	割り当て
	UINT allocate(UINT size);

	//	解除
	bool deallocate(UINT deallocate_position);

	//	プール全体のサイズ
	UINT get_capacity() const { return capacity; }

	//	現在確保されている合計サイズ(allocate/deallocateで自動更新される)
	UINT get_used_size() const { return used_size; }

private:
	//	座標ノード
	std::list<UINT> node_list;
	UINT capacity = 0;
	UINT used_size = 0;
};
