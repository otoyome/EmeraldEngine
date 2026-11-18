#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <vector>

//	game_object‚ÌŒ^ƒŒƒWƒXƒgƒŠ
class game_object_registry
{
	SINGLETON(game_object_registry)

public:
	using create_function = std::function<game_object_ptr()>;

	//	Œ^î•ñ
	struct type_entry
	{
		std::string name;
		std::string category;
	};

	//	Œ^‚Æ¶¬ŠÖ”î•ñ
	struct static_entry
	{
		type_entry type;
		create_function function;
	};

	//	“o˜^Œ^î•ñ
	struct registered_type
	{
		std::string category;
		create_function function;
	};

	//	‹N“®‘O‚ÉÃ“IƒIƒuƒWƒFƒNƒg‚ÅŒ^‚ğW‚ß‚é
	template<typename Type>
	class registrar
	{
	public:
		explicit registrar(std::string_view category)
		{
			static_assert(std::is_base_of_v<game_object, Type>);
			get_static_list().push_back({ { std::string(Type::type_name()), std::string(category) }, [](){ return std::make_shared<Type>(std::string_view{}); } });
		}
	};

public:
	std::vector<std::string> get_type_names() const;
	std::vector<type_entry> get_placeable_types() const;

public:
	template<typename Type>
	void add(std::string_view category)
	{
		static_assert(std::is_base_of_v<game_object, Type>);
		add(Type::type_name(), category, [](){ return std::make_shared<Type>(std::string_view{}); });
	}
	void add(std::string_view type, std::string_view category, create_function function);

	game_object_ptr create(std::string_view type) const;
	bool contains(std::string_view type) const;

private:
	game_object_registry();
	~game_object_registry();

private:
	static std::vector<static_entry>& get_static_list();

private:
	std::unordered_map<std::string, registered_type> functions;
};

//	•Û‘¶E“Ç‚Å‚«‚éŒ^‚Æ‚µ‚ÄŒ^‚Ìcpp‚É‚ÄéŒ¾‚µ‚Ä“o˜^
#define GAME_OBJECT_REGISTER(TYPE, CATEGORY) static const game_object_registry::registrar<TYPE> TYPE##_registrar(CATEGORY)
