#pragma once

#include <d3d11.h>
#include <wrl.h>

#include <deque>
#include <functional>
#include <initializer_list>
#include <memory>
#include <mutex>
#include <vector>

//	@tool GPU から CPU への読み戻し
//	@tool 記録：render_command_recorder::read_back がステージングバッファへのコピーを積むだけ（即時・遅延のどちらのジョブでも同じ）
//	@tool 受け取り：提出スレッドがコマンドの実行後に要求を登録し、GPU の処理が終わったものから読み出して受け取り関数を呼ぶ
//	@tool 結果が届くのは記録から 1～2 フレーム後（D3D12 のフェンス待ち後の読み出しと同じ形）

//	@tool 読み出したデータ（受け取り関数の中でだけ有効）
struct gpu_readback_data
{
	const void* const* parts = nullptr;	//	@tool バッファごとの先頭
	const UINT* part_sizes = nullptr;	//	@tool バッファごとの大きさ（バイト）
	UINT part_count = 0;

	template<class T>
	const T* get(UINT part) const { return static_cast<const T*>(parts[part]); }
	UINT size(UINT part) const { return part_sizes[part]; }
};

//	@tool 受け取り関数（提出スレッドで呼ばれる。短く、スレッド安全に書くこと）
using gpu_readback_receiver = std::function<void(const gpu_readback_data&)>;

//	@tool 読み戻し元
struct gpu_readback_source
{
	ID3D11Resource* resource = nullptr;						//	@tool この資源の内容をコピーする
	ID3D11UnorderedAccessView* structure_count = nullptr;	//	@tool 追加・消費バッファの個数をコピーする（resource の代わり）
};

//	@tool 読み戻し用ステージングバッファの組の置き場（読み戻し先と要求で共有する）
struct gpu_readback_pool
{
	struct slot
	{
		std::vector<Microsoft::WRL::ComPtr<ID3D11Buffer>> buffers;
		bool in_use = false;
	};

	std::mutex mutex;
	Microsoft::WRL::ComPtr<ID3D11Device> device;
	std::vector<UINT> part_sizes;
	std::vector<std::unique_ptr<slot>> slots;
	UINT max_slot_count = 0;

	//	@tool 空いている組を取る（無ければ作る。上限を超えたら nullptr）
	slot* acquire();
	//	@tool 組を返す
	void release(slot* target);

	//	@tool 組のバッファを作る
	bool create_slot(slot& out_slot) const;
};

//	@tool 読み戻し先
class gpu_readback_buffer
{
public:
	//	@tool part_sizes：1回の読み戻しで使うバッファごとの大きさ（バイト。コピー元と同じ大きさにする）
	bool initialize(ID3D11Device* device, const std::vector<UINT>& part_sizes, UINT initial_slot_count = 3, UINT max_slot_count = 32);
	void reset() { pool.reset(); }

	bool is_valid() const { return pool != nullptr; }
	const std::shared_ptr<gpu_readback_pool>& get_pool() const { return pool; }

private:
	std::shared_ptr<gpu_readback_pool> pool;
};

//	@tool 読み戻し要求（記録したジョブから提出スレッドへ渡す）
struct gpu_readback_request
{
	std::shared_ptr<gpu_readback_pool> pool;
	gpu_readback_pool::slot* slot = nullptr;
	gpu_readback_receiver receiver;
};

//	@tool 提出スレッドで要求を受け付け、読み出せたものから受け取り関数を呼ぶ
class gpu_readback_queue
{
public:
	static gpu_readback_queue& instance();

	//	@tool 要求を登録する（コピーを含むコマンドを immediate context で実行した後に呼ぶ）
	void submit(std::vector<gpu_readback_request>&& requests);

	//	@tool GPU の処理が終わったものを読み出して受け取り関数を呼ぶ（提出スレッド、immediate context）
	//	@tool 同じ読み戻し先の要求は登録順に受け取る
	void poll(ID3D11DeviceContext* immediate_context);

private:
	std::mutex mutex;
	std::deque<gpu_readback_request> pending;
};
