#pragma once

#include "System/signals.h"

class scene_manager;
class engine_context;

struct scene_generates
{
	template<typename Type, typename Manager>
	static void add_scene(Manager& scenes)
	{
		scenes.template add_scene<Type>();
	}
	template<typename Type, typename ... Lists, typename Manager>
	static void add_scenes(Manager& scenes)
	{
		add_scene<Type>(scenes);
		if constexpr (sizeof...(Lists) > 0)
			add_scenes<Lists...>(scenes);
	}
};

class scene_base
{
public:
	static constexpr std::string_view type_name(){ return NAMEOF_TYPE(scene_base); }
	virtual std::string_view get_type_name() const { return type_name(); }
	virtual bool check_type(std::string_view text) const { return type_name() == text; }

	engine_context& get_context() const;

public:
	scene_base();
	scene_base(const scene_base&) = delete;
	scene_base(scene_base&&) = delete;
	virtual ~scene_base();

	bool initialize(engine_context& context);
	bool uninitialize();

	void update(float elapsed_time);
#if defined(USE_IMGUI)
	void imgui_render();
#endif	//	defined(USE_IMGUI)

protected:
	virtual bool initialize_core() { return true; }
	virtual bool uninitialize_core() { return true; }
	virtual void update_core(float elapsed_time) {}
#if defined(USE_IMGUI)
	virtual void imgui_render_core() {}
#endif	//	defined(USE_IMGUI)

protected:
	signals::broadcaster& get_broadcaster();
	signals::contactor& get_contactor();

protected:
	void register_object(game_object_ptr object);
	void register_object(collision_object_ptr object);

private:
	friend class scene_manager;

	//	@tool scene_manager がシーンを作る間だけ入る（コンストラクタからワールドを使えるようにする）
	inline static engine_context* constructing_context = nullptr;

	engine_context* context = nullptr;
	std::vector<std::weak_ptr<game_object>> game_objects;
	std::vector<std::weak_ptr<collision_object>> collision_objects;
};

class scene_manager
{
public:
	explicit scene_manager(engine_context& context);
	~scene_manager();

public:
	void updates(float elapsed_time);

	void remove_all_scene();

	template<typename scene_type>
	void create_scene_function()
	{
		scene_base::constructing_context = &context;
		auto created = std::make_unique<scene_type>();
		scene_base::constructing_context = nullptr;

		scenes.insert_or_assign(std::string(scene_type::type_name()), std::move(created));
		auto& scene = scenes.find(std::string(scene_type::type_name()))->second;
		scene->initialize(context);
	}

	//	全てのシーンを破棄して一つのシーンを読み込む
	template<typename scene_type>
	bool change_scene()
	{
		for (auto& scene : scenes)
		{
			if (std::find(remove_scenes.begin(), remove_scenes.end(), scene.first) != remove_scenes.end())
				continue;

			remove_scenes.emplace_back(scene.first);
		}

		add_scene<scene_type>();

		return true;
	}

	template<typename scene_type>
	bool add_scene()
	{
		if (scenes.find(std::string(scene_type::type_name())) != scenes.end())
		{
			return false;
		}
		if (std::find_if(add_scenes.begin(),
			add_scenes.end(),
			[&](add_scene_data& data) {return data.name == scene_type::type_name(); }) != add_scenes.end())
		{
			return false;
		}

		add_scenes.emplace_back(scene_type::type_name(), scene_type::generate_order(), [this](){ create_scene_function<scene_type>(); });
		scene_type::on_scene_children(*this);
		return true;
	}

	template<typename scene_type>
	bool remove_scene()
	{
		return remove_scene(scene_type::type_name());
	}

	template<typename scene_type>
	bool valid_scene()
	{
		return valid_scene(scene_type::type_name());
	}

public:
	//	シーン全体で共有する通知と問い合わせ
	signals::broadcaster& get_broadcaster() { return broadcaster; }
	signals::contactor& get_contactor() { return contactor; }

public:
	bool remove_scene(std::string_view scene_name);
	bool valid_scene(std::string_view scene_name);
	std::vector<std::string> get_valid_scene_names() const;

public:
#if defined(USE_IMGUI)
	void imgui_render();

	//	全てのシーンを破棄して一つのシーンを読み込む
	template<typename scene_type>
	bool change_scene_for_imgui()
	{
		for (auto& scene : scenes)
		{
			if (std::find(remove_scenes.begin(), remove_scenes.end(), scene.first) != remove_scenes.end())
				continue;

			remove_scenes.emplace_back(scene.first);
		}
		updates(0);

		add_scene<scene_type>();

		return true;
	}
#endif	//	defined(USE_IMGUI)

private:
	struct add_scene_data
	{
		std::string name;
		int order;
		std::function<void()> func;
		add_scene_data(std::string_view name, int order, std::function<void()> func)
		: name(name), order(order), func(func)
		{
		}
	};

	engine_context& context;

	signals::broadcaster broadcaster;
	signals::contactor contactor;

	std::vector<add_scene_data> add_scenes;
	std::vector<std::string> remove_scenes;
	std::unordered_map<std::string, std::unique_ptr<scene_base>> scenes;
};

#define	__SCENE_DECRLEAR_INNER__(ABSTRACT_TYPE, BASE_TYPE)	\
public:	\
using base = BASE_TYPE;	\
static constexpr std::string_view type_name(){return NAMEOF_TYPE(ABSTRACT_TYPE);}	\
std::string_view get_type_name() const override {return type_name();}	\
bool check_type(std::string_view text) const override {return type_name() == text ? true : BASE_TYPE::check_type(text);}

#define	SCENE_DECRLEAR(ABSTRACT_TYPE, BASE_TYPE) \
__SCENE_DECRLEAR_INNER__(ABSTRACT_TYPE, BASE_TYPE) \
static constexpr int generate_order(){return 0;} \
static void on_scene_children(scene_manager&) {} \
private:

#define	SCENE_DECRLEAR_AND_SET_ORDER(ABSTRACT_TYPE, ORDER, BASE_TYPE)	\
__SCENE_DECRLEAR_INNER__(ABSTRACT_TYPE, BASE_TYPE) \
static constexpr int generate_order(){return ORDER;} \
static void on_scene_children(scene_manager&) {} \
private:

#define	SCENE_ADD_CHILDREN_DECRLEAR(ABSTRACT_TYPE, BASE_TYPE, ... )	\
__SCENE_DECRLEAR_INNER__(ABSTRACT_TYPE, BASE_TYPE) \
static constexpr int generate_order(){return 0;} \
static void on_scene_children(scene_manager& scenes) { scene_generates::add_scenes<__VA_ARGS__>(scenes); } \
private:

#define	SCENE_ADD_CHILDREN_DECRLEAR_AND_SET_ORDER(ABSTRACT_TYPE, ORDER, BASE_TYPE, ... )	\
__SCENE_DECRLEAR_INNER__(ABSTRACT_TYPE, BASE_TYPE) \
static constexpr int generate_order(){return ORDER;} \
static void on_scene_children(scene_manager& scenes) { scene_generates::add_scenes<__VA_ARGS__>(scenes); } \
private:
