#include "Resource/data_compression.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <thread>

#include <compressapi.h>
#pragma comment(lib, "Cabinet.lib")

namespace
{
	//	圧縮ファイルヘッダー
	struct compressed_header
	{
		char magic[4];
		uint32_t version;
		uint32_t algorithm;
		uint32_t chunk_size;
		uint64_t uncompressed_size;
		uint32_t chunk_count;
		uint32_t reserved;
	};
	static_assert(sizeof(compressed_header) == 32);

	//	識別子はモデルファイルの圧縮から使っているものを引き継ぐ
	constexpr char compressed_magic[4] = { 'E', 'M', 'O', 'Z' };
	constexpr uint32_t compressed_version = 1;
	constexpr DWORD compressed_algorithm = COMPRESS_ALGORITHM_XPRESS_HUFF;
	constexpr uint32_t compressed_chunk_size = 1024 * 1024;

	//	チャンク単位の処理を並列実行
	template<typename Function>
	bool parallel_chunks(size_t chunk_count, Function&& function)
	{
		std::atomic<size_t> next_chunk{ 0 };
		std::atomic<bool> succeeded{ true };
		const size_t thread_count = std::max<size_t>(1, std::min<size_t>(std::thread::hardware_concurrency(), chunk_count));
		std::vector<std::thread> workers;
		workers.reserve(thread_count);
		for (size_t thread_index = 0; thread_index < thread_count; ++thread_index)
		{
			workers.emplace_back([&]()
			{
				for (size_t chunk_index; (chunk_index = next_chunk++) < chunk_count; )
				{
					if (!function(chunk_index))
						succeeded = false;
				}
			});
		}
		for (auto& worker : workers)
			worker.join();
		return succeeded;
	}
}

bool resource::compress(const void* source, size_t source_size, std::vector<char>& out_data)
{
	const char* source_bytes = static_cast<const char*>(source);
	const size_t chunk_count = (source_size + compressed_chunk_size - 1) / compressed_chunk_size;
	std::vector<std::vector<char>> compressed_chunks(chunk_count);
	bool succeeded = parallel_chunks(chunk_count, [&](size_t chunk_index)
	{
		COMPRESSOR_HANDLE compressor = nullptr;
		if (!CreateCompressor(compressed_algorithm | COMPRESS_RAW, nullptr, &compressor))
			return false;

		const size_t offset = chunk_index * compressed_chunk_size;
		const size_t input_size = std::min<size_t>(compressed_chunk_size, source_size - offset);
		SIZE_T required_size = 0;
		Compress(compressor, source_bytes + offset, input_size, nullptr, 0, &required_size);

		auto& chunk = compressed_chunks[chunk_index];
		chunk.resize(required_size);
		SIZE_T compressed_size = 0;
		const bool result = Compress(compressor, source_bytes + offset, input_size, chunk.data(), chunk.size(), &compressed_size) != FALSE;
		chunk.resize(compressed_size);
		CloseCompressor(compressor);
		return result;
	});
	if (!succeeded)
		return false;

	compressed_header header{};
	memcpy(header.magic, compressed_magic, sizeof(header.magic));
	header.version = compressed_version;
	header.algorithm = compressed_algorithm;
	header.chunk_size = compressed_chunk_size;
	header.uncompressed_size = source_size;
	header.chunk_count = static_cast<uint32_t>(chunk_count);

	size_t total_size = sizeof(header) + sizeof(uint32_t) * chunk_count;
	for (auto& chunk : compressed_chunks)
		total_size += chunk.size();

	out_data.clear();
	out_data.reserve(total_size);
	out_data.insert(out_data.end(), reinterpret_cast<const char*>(&header), reinterpret_cast<const char*>(&header) + sizeof(header));
	for (auto& chunk : compressed_chunks)
	{
		const uint32_t chunk_size = static_cast<uint32_t>(chunk.size());
		out_data.insert(out_data.end(), reinterpret_cast<const char*>(&chunk_size), reinterpret_cast<const char*>(&chunk_size) + sizeof(chunk_size));
	}
	for (auto& chunk : compressed_chunks)
		out_data.insert(out_data.end(), chunk.begin(), chunk.end());
	return true;
}

bool resource::is_compressed(const void* data, size_t size)
{
	return size >= sizeof(compressed_header) && memcmp(data, compressed_magic, sizeof(compressed_magic)) == 0;
}

bool resource::decompress(const void* data, size_t size, std::vector<char>& out_data)
{
	if (!is_compressed(data, size))
		return false;

	const char* bytes = static_cast<const char*>(data);
	compressed_header header{};
	memcpy(&header, bytes, sizeof(header));
	if (header.version != compressed_version || header.algorithm != compressed_algorithm || header.chunk_size == 0)
		return false;

	const uint64_t expected_chunk_count = (header.uncompressed_size + header.chunk_size - 1) / header.chunk_size;
	const size_t table_offset = sizeof(header);
	if (header.chunk_count != expected_chunk_count || size < table_offset + sizeof(uint32_t) * header.chunk_count)
		return false;

	//	チャンク毎の開始位置を求める
	std::vector<size_t> chunk_offsets(header.chunk_count);
	std::vector<uint32_t> chunk_sizes(header.chunk_count);
	size_t data_offset = table_offset + sizeof(uint32_t) * header.chunk_count;
	for (uint32_t chunk_index = 0; chunk_index < header.chunk_count; ++chunk_index)
	{
		memcpy(&chunk_sizes[chunk_index], bytes + table_offset + sizeof(uint32_t) * chunk_index, sizeof(uint32_t));
		chunk_offsets[chunk_index] = data_offset;
		data_offset += chunk_sizes[chunk_index];
	}
	if (data_offset > size)
		return false;

	out_data.resize(static_cast<size_t>(header.uncompressed_size));
	return parallel_chunks(header.chunk_count, [&](size_t chunk_index)
	{
		DECOMPRESSOR_HANDLE decompressor = nullptr;
		if (!CreateDecompressor(header.algorithm | COMPRESS_RAW, nullptr, &decompressor))
			return false;

		const size_t output_offset = chunk_index * header.chunk_size;
		const size_t output_size = std::min<size_t>(header.chunk_size, out_data.size() - output_offset);
		SIZE_T decompressed_size = 0;
		const bool result = Decompress(decompressor, bytes + chunk_offsets[chunk_index], chunk_sizes[chunk_index],
									   out_data.data() + output_offset, output_size, &decompressed_size) != FALSE;
		CloseDecompressor(decompressor);
		return result && decompressed_size == output_size;
	});
}

resource::memory_streambuf::memory_streambuf(char* begin, size_t size)
{
	setg(begin, begin, begin + size);
}