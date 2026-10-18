#pragma once

#include <vector>
#include <mutex>

struct gpu_upload_list;

//	@tool マテリアルの定数バッファ管理クラス
//	@tool GPU バッファの実体と転送待ちは描画側のこのクラスが持ち、マテリアルは番号（ハンドル）だけを持つ
//	@tool 変更分は flush_uploads でフレームの転送一覧へ積み、描画の最初のパスでまとめて転送する
class material_constant_buffer_pool
{
	SINGLETON(material_constant_buffer_pool)

public:
	using handle = UINT;
	static constexpr handle invalid_handle = UINT_MAX;

private:
	material_constant_buffer_pool();
	~material_constant_buffer_pool();

public:
	//	@tool 定数バッファの確保・解放
	handle allocate(UINT size_in_bytes);
	void release(handle value);

	//	@tool 定数の内容を転送待ちに積む（同じフレームで何度積んでも、最後に積んだ内容だけを転送する）
	void queue_upload(handle value, const void* data, UINT size_in_bytes);

	//	@tool 描画プロキシーが参照するバッファの取得
	Microsoft::WRL::ComPtr<ID3D11Buffer> get_buffer(handle value) const;

	//	@tool 転送待ちをフレームの転送一覧へ移す（実際の転送は描画の最初のパスで行う）
	void flush_uploads(gpu_upload_list& out_uploads);

private:
	struct slot
	{
		Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;	//	@tool 定数バッファ
		UINT size_in_bytes = 0;							//	@tool バッファの大きさ
		std::vector<uint8_t> pending_data;				//	@tool 転送待ちの内容
		bool used = false;								//	@tool 確保済みか？
		bool upload_queued = false;						//	@tool 転送待ちに積まれているか？
	};

	mutable std::mutex mutex;
	std::vector<slot> slots;
	std::vector<handle> free_handles;
	std::vector<handle> queued_handles;
};
