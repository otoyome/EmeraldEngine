#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <vector>

//	ワールド
class engine_world
{
	struct descriptor
	{
		std::function<void*()> create;
		std::function<void(void*)> bind;
		std::function<void(void*)> destroy;
	};

public:
	//	変数が生存している間、ワールドを切り替える
	class scope
	{
	public:
		explicit scope(engine_world& world) : previous(current) { world.make_current(); }
		~scope() { if (previous) previous->make_current(); }

		scope(const scope&) = delete;
		scope& operator=(const scope&) = delete;

	private:
		engine_world* previous;
	};

public:
	//	ワールドごとに持つ管理クラスを登録
	template<class T>
	static void register_scoped_singleton(std::function<void(T*)> before_destroy = nullptr)
	{
		descriptor target;
		target.create = [] { return static_cast<void*>(T::create_detached_instance()); };
		target.bind = [](void* instance) { T::set_current_instance(static_cast<T*>(instance)); };
		target.destroy = [before_destroy](void* instance)
		{
			if (before_destroy)
				before_destroy(static_cast<T*>(instance));
			T::destroy_detached_instance(static_cast<T*>(instance));
			T::set_current_instance(nullptr);
		};

		descriptors.emplace_back(std::move(target));
	}

	//	処理中のワールド
	static engine_world* get_current() { return current; }

public:
	std::string_view get_name() const { return name; }

public:
	explicit engine_world(std::string_view name);
	~engine_world();
	engine_world(const engine_world&) = delete;
	engine_world& operator=(const engine_world&) = delete;

	//	このワールドを処理中にする
	void make_current();

private:
	static inline std::vector<descriptor> descriptors;	//	管理クラス群
	static inline engine_world* current = nullptr;		//	現在ワールド

private:
	std::vector<void*> instances;	//	ワールドインスタンス
	std::string name;				//	ワールド名
};
