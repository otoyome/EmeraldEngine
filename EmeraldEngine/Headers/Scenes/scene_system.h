#pragma once

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <vector>

class engine_context;
class scene_instance;

namespace signals
{
	class broadcaster;
	class contactor;
}

namespace cereal
{
	class JSONOutputArchive;
	class JSONInputArchive;
}

//	シーンシステム
class scene_system
{
public:
	static constexpr std::string_view type_name() { return NAMEOF_SHORT_TYPE(scene_system); }
	virtual std::string_view get_type_name() const { return type_name(); }

public:
	scene_system() = default;
	scene_system(const scene_system&) = delete;
	scene_system& operator=(const scene_system&) = delete;
	virtual ~scene_system() = default;

	//	シーン開始時に呼び出される
	virtual void on_begin() {}
	//	シーン終了時に呼び出される
	virtual void on_end() {}

	//	オブジェクトの更新の前
	virtual void pre_update(float elapsed_time) {}
	//	オブジェクトの更新の後
	virtual void post_update(float elapsed_time) {}

#if defined(USE_IMGUI)
	//	ImGui描画
	virtual void imgui_render() {}

	//	パネル領域
	enum class panel_regions
	{
		menu_bar,	//	メニューバー
		left,		//	左
		right,		//	右
		bottom,		//	下
	};

	//	パネルの指定の領域に描画するか
	virtual bool has_panel(panel_regions region) const { return false; }

	//	パネルの指定の領域へImGui描画
	virtual void imgui_render_panel(panel_regions region) {}

	//	ビューポートの画像の上に重ねて描く
	virtual bool imgui_render_viewport_overlay(const ImVec2& image_position, const ImVec2& image_size) { return false; }

	//	窓を閉じてよいか
	virtual bool request_close() { return true; }

	//	シーンエディターで編集中だけ呼ばれる関数群
	virtual void editor_update(float elapsed_time) {}

public:
	//	シーンエディターで編集中か
	bool is_editing() const { return owner == nullptr; }

	//	シーンシステムウィンドウからImGui描画を行う
	void imgui_render_window(std::string_view scene_path);

	void open_window() { window_opened = true; }
#endif	//	defined(USE_IMGUI)

public:	//	シリアライズ
	virtual void serialize(cereal::JSONOutputArchive& archive) const {}
	virtual void deserialize(cereal::JSONInputArchive& archive) {}

public:
	scene_instance& get_scene() const;
	engine_context& get_context() const;

protected:
	//	システムで生成したオブジェクトを取得
	std::vector<game_object_ptr> get_scene_objects() const;

	//	実行時だけのオブジェクトとしてシーンに登録する
	void register_object(game_object_ptr object);
	void register_object(collision_object_ptr object);
	void unregister_object(const game_object_ptr& object);

	signals::broadcaster& get_broadcaster() const;
	signals::contactor& get_contactor() const;

private:
	friend class scene_instance;
	friend class scene_editor;
	scene_instance* owner = nullptr;
	engine_context* context = nullptr;
#if defined(USE_IMGUI)
	bool window_opened = false;
#endif	//	defined(USE_IMGUI)
};

#define	SCENE_SYSTEM_DECRLEAR(ABSTRACT_TYPE, BASE_TYPE)	\
public:	\
	using base = BASE_TYPE;	\
	static constexpr std::string_view type_name() { return NAMEOF_SHORT_TYPE(ABSTRACT_TYPE); }	\
	std::string_view get_type_name() const override { return type_name(); }	\
private:

//	型名からシーンシステムを生成する
class scene_system_registry
{
	SINGLETON(scene_system_registry)

public:
	using create_function = std::function<std::unique_ptr<scene_system>()>;

	//	静的オブジェクトで型を集めるクラス
	template<typename Type>
	class registrar
	{
	public:
		registrar()
		{
			static_assert(std::is_base_of_v<scene_system, Type>);
			get_static_list().emplace_back(std::string(Type::type_name()), [](){ return std::make_unique<Type>(); });
		}
	};

public:
	//	起動時に明示的に登録する(静的ライブラリ側の型用)
	template<typename Type>
	void add()
	{
		static_assert(std::is_base_of_v<scene_system, Type>);
		add(Type::type_name(), [](){ return std::make_unique<Type>(); });
	}
	void add(std::string_view type, create_function function);

	std::unique_ptr<scene_system> create(std::string_view type) const;
	bool contains(std::string_view type) const;
	std::vector<std::string> get_type_names() const;

private:
	scene_system_registry();
	~scene_system_registry();

	static std::vector<std::pair<std::string, create_function>>& get_static_list();

private:
	std::unordered_map<std::string, create_function> functions;
};

//	シーンシステムとしてcppに置いて登録
#define SCENE_SYSTEM_REGISTER(TYPE) static const scene_system_registry::registrar<TYPE> TYPE##_registrar
