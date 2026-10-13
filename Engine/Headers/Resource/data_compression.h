#pragma	once

#include <cstddef>
#include <vector>

//	バイト列の圧縮/展開
namespace resource
{
	//	圧縮
	bool compress(const void* source, size_t source_size, std::vector<char>& out_data);

	//	圧縮データか判定
	bool is_compressed(const void* data, size_t size);

	//	解凍
	bool decompress(const void* data, size_t size, std::vector<char>& out_data);

	//	メモリ上のバイト列を istream として読むためのバッファクラス
	struct memory_streambuf : public std::streambuf
	{
		memory_streambuf(char* begin, size_t size);
	};

}	//	namespace resource
