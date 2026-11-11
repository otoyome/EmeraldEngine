#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>

//	型名からgame_objectを生成レジスター
class game_object_registry
{
	SINGLETON(game_object_registry)

public:
	using create_function = std::function<game_object_ptr()>;

public:
	template<typename Type>
	void add()
	{
		static_assert(std::is_base_of_v<game_object, Type>);
		add(Type::type_name(), [](){ return std::make_shared<Type>(std::string_view{}); });
	}
	void add(std::string_view type, create_function function);

	game_object_ptr create(std::string_view type) const;
	bool contains(std::string_view type) const;

private:
	game_object_registry();
	~game_object_registry();

private:
	std::unordered_map<std::string, create_function> functions;
};
