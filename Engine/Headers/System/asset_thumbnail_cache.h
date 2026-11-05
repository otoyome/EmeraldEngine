#pragma	once

#if defined(USE_IMGUI)
#include <cstddef>
#include <filesystem>
#include <memory>

#include "System/cache_string.h"
#include "System/universally_unique_identifier.h"

class texture;

namespace DirectX
{
	class ScratchImage;
}

//	アセットサムネイルキャッシュ
class asset_thumbnail_cache
{
public:
	//	サムネイルの長辺
	static constexpr std::size_t thumbnail_size = 128;

	//	キャッシュの置き場所
	static constexpr const char* cache_directory = "Intermediate/Thumbnails";

public:
	//	アセットのサムネイルを返す
	std::shared_ptr<texture> load(const cache_string& asset_path);

	//	サムネイルを作れる種類か?
	static bool is_supported(const cache_string& asset_path);

private:
	static universally_unique_identifier find_uuid(const cache_string& asset_path);
	static std::filesystem::path make_cache_path(const universally_unique_identifier& uuid);

	static bool read_source(const cache_string& asset_path, DirectX::ScratchImage& out_image);
	static bool make_thumbnail(const DirectX::ScratchImage& source, DirectX::ScratchImage& out_image);
};
#endif	//	defined(USE_IMGUI)
