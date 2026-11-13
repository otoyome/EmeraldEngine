#include "Graphics/Core/gpu_upload_list.h"

namespace
{
	D3D11_BOX make_buffer_box(UINT offset_in_bytes, UINT size_in_bytes)
	{
		D3D11_BOX box{};
		box.left = offset_in_bytes;
		box.right = offset_in_bytes + size_in_bytes;
		box.top = 0;
		box.bottom = 1;
		box.front = 0;
		box.back = 1;
		return box;
	}
}

void gpu_upload_list::update_buffer(ID3D11Buffer* buffer, UINT offset_in_bytes, const void* data, UINT size_in_bytes)
{
	if (!buffer || !data || size_in_bytes == 0)
		return;

	command cmd;
	cmd.type = command_types::update_buffer;
	cmd.destination = buffer;
	cmd.destination_offset_in_bytes = offset_in_bytes;
	cmd.size_in_bytes = size_in_bytes;
	cmd.data.assign(static_cast<const uint8_t*>(data), static_cast<const uint8_t*>(data) + size_in_bytes);

	std::lock_guard<std::mutex> lock(mutex);
	commands.emplace_back(std::move(cmd));
}

void gpu_upload_list::update_whole(ID3D11Resource* resource, const void* data, UINT size_in_bytes)
{
	if (!resource || !data || size_in_bytes == 0)
		return;

	command cmd;
	cmd.type = command_types::update_whole;
	cmd.destination = resource;
	cmd.size_in_bytes = size_in_bytes;
	cmd.data.assign(static_cast<const uint8_t*>(data), static_cast<const uint8_t*>(data) + size_in_bytes);

	std::lock_guard<std::mutex> lock(mutex);
	commands.emplace_back(std::move(cmd));
}

void gpu_upload_list::copy_buffer_region(ID3D11Buffer* destination, UINT destination_offset_in_bytes, ID3D11Buffer* source, UINT source_offset_in_bytes, UINT size_in_bytes)
{
	if (!destination || !source || size_in_bytes == 0)
		return;

	command cmd;
	cmd.type = command_types::copy_buffer_region;
	cmd.destination = destination;
	cmd.source = source;
	cmd.destination_offset_in_bytes = destination_offset_in_bytes;
	cmd.source_offset_in_bytes = source_offset_in_bytes;
	cmd.size_in_bytes = size_in_bytes;

	std::lock_guard<std::mutex> lock(mutex);
	commands.emplace_back(std::move(cmd));
}

void gpu_upload_list::execute(ID3D11DeviceContext* immediate_context)
{
	//	é¿çsíÜÇ…êœÇ‹ÇÍÇΩï™ÇÕéü
	{
		std::lock_guard<std::mutex> lock(mutex);
		executing_commands.swap(commands);
	}

	for (auto& cmd : executing_commands)
	{
		switch (cmd.type)
		{
			case command_types::update_buffer:
			{
				const D3D11_BOX box = make_buffer_box(cmd.destination_offset_in_bytes, cmd.size_in_bytes);
				immediate_context->UpdateSubresource(cmd.destination.Get(), 0, &box, cmd.data.data(), 0, 0);
				break;
			}
			case command_types::update_whole:
			{
				immediate_context->UpdateSubresource(cmd.destination.Get(), 0, nullptr, cmd.data.data(), 0, 0);
				break;
			}
			case command_types::copy_buffer_region:
			{
				const D3D11_BOX box = make_buffer_box(cmd.source_offset_in_bytes, cmd.size_in_bytes);
				immediate_context->CopySubresourceRegion(cmd.destination.Get(), 0, cmd.destination_offset_in_bytes, 0, 0, cmd.source.Get(), 0, &box);
				break;
			}
		}
	}
	executing_commands.clear();
}
