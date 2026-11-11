#pragma once

#include <iosfwd>
#include <vector>

//	game_objectのシリアライザー
class game_object_serializer
{
public:
	static constexpr std::uint32_t current_version = 1;

public:
	//	保存
	static bool save(std::ostream& stream, const std::vector<game_object_ptr>& objects);

	//	読込
	static bool load(std::istream& stream, std::vector<game_object_ptr>& out_objects);
};
