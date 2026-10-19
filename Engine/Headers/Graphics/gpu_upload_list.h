#pragma once

#include <d3d11.h>
#include <wrl.h>
#include <vector>
#include <mutex>
#include <cstdint>

//	•`‰æ‘¤‚ªŽg‚¤ GPU Ž‘Œ¹‚Ö‚Ì“]‘—ˆê——
class gpu_upload_list
{
public:
	void update_buffer(ID3D11Buffer* buffer, UINT offset_in_bytes, const void* data, UINT size_in_bytes);
	void update_whole(ID3D11Resource* resource, const void* data, UINT size_in_bytes);
	void copy_buffer_region(ID3D11Buffer* destination, UINT destination_offset_in_bytes, ID3D11Buffer* source, UINT source_offset_in_bytes, UINT size_in_bytes);
	void execute(ID3D11DeviceContext* immediate_context);

private:
	enum class command_types
	{
		update_buffer,
		update_whole,
		copy_buffer_region,
	};

	struct command
	{
		command_types type;
		Microsoft::WRL::ComPtr<ID3D11Resource> destination;
		Microsoft::WRL::ComPtr<ID3D11Resource> source;
		UINT destination_offset_in_bytes = 0;
		UINT source_offset_in_bytes = 0;
		UINT size_in_bytes = 0;
		std::vector<uint8_t> data;
	};

	std::mutex mutex;
	std::vector<command> commands;
	std::vector<command> executing_commands;
};
