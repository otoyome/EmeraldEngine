#pragma once

#include <d3d11.h>
#include <wrl.h>
#include <vector>
#include <mutex>
#include <functional>

//	GPU からの読み戻しキュー
class gpu_readback_queue
{
public:
	using receive_function = std::function<void(const void* data, UINT size_in_bytes)>;

private:
	struct staging_buffer
	{
		Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
		UINT capacity_in_bytes = 0;
	};

	struct request
	{
		staging_buffer staging;
		UINT size_in_bytes = 0;
		receive_function receive;
	};

public:
	//	ステージングバッファへのコピーコマンドを積む
	void read_back(ID3D11DeviceContext* device_context, ID3D11Buffer* source, UINT size_in_bytes, receive_function receive);

	//	フレームのコマンドを全てGPUに送った後に記録
	void mark_submitted();

	//	GPU側の処理結果を取得
	void poll(ID3D11DeviceContext* immediate_context);

private:
	staging_buffer acquire_staging(UINT size_in_bytes);

private:
	std::mutex mutex;
	std::vector<staging_buffer> free_stagings;
	std::vector<request> recorded_requests;	
	std::vector<request> submitted_requests;
};
