#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <vector>

//	型名からgame_objectを生成レジスター
class game_object_registry
{
	SINGLETON(game_object_registry)

public:
	using create_function = std::function<game_object_ptr()>;

	//	起動前に静的オブジェクトで型を集める
	template<typename Type>
	class registrar
	{
	public:
		registrar()
		{
			static_assert(std::is_base_of_v<game_object, Type>);
			get_static_list().emplace_back(std::string(Type::type_name()), [](){ return std::make_shared<Type>(std::string_view{}); });
		}
	};

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

	//	@tool registrar が集めた型の一覧
	static std::vector<std::pair<std::string, create_function>>& get_static_list();

private:
	std::unordered_map<std::string, create_function> functions;
};

//	保存・読込できる型として登録する
//	型のcppにて宣言
#define GAME_OBJECT_REGISTER(TYPE) static const game_object_registry::registrar<TYPE> TYPE##_registrar
