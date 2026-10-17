#pragma once

//	定数バッファの一部を指す範囲
struct constant_range
{
	ID3D11Buffer* buffer = nullptr;	//	バッファ
	UINT first_constant = 0;		//	16バイト単位の開始位置
	UINT num_constants = 0;			//	16バイト単位の大きさ
	bool reusable = false;			//	同じ記録の中で使い回せるか？

	bool is_valid() const { return buffer != nullptr; }
};

//	揮発性定数バッファアロケーター
class immediate_constant_allocator
{
public:
	static constexpr UINT alignment = 256;	//	DirectX12準拠

public:
	immediate_constant_allocator() = default;
	immediate_constant_allocator(const immediate_constant_allocator&) = delete;
	immediate_constant_allocator& operator=(const immediate_constant_allocator&) = delete;

	bool initialize(ID3D11Device* device, bool use_offsetting, UINT chunk_size = 1024 * 1024);

	//	記録開始
	void begin();

	//	定数書き込み
	constant_range allocate(ID3D11DeviceContext* context, const void* data, UINT size);

private:
	struct chunk
	{
		Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
		UINT offset = 0;
		bool discarded = false;
	};

	bool create_buffer(UINT size, Microsoft::WRL::ComPtr<ID3D11Buffer>& out_buffer) const;
	constant_range allocate_offset(ID3D11DeviceContext* context, const void* data, UINT size);
	constant_range allocate_whole(ID3D11DeviceContext* context, const void* data, UINT size);

private:
	Microsoft::WRL::ComPtr<ID3D11Device> device;
	bool use_offsetting = false;
	UINT chunk_size = 0;
	std::vector<chunk> chunks;
	std::size_t current_chunk = 0;
	std::vector<std::pair<UINT, Microsoft::WRL::ComPtr<ID3D11Buffer>>> buffers;
};
