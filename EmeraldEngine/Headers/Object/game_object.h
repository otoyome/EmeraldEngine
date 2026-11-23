#pragma once

#include <mutex>
#include <functional>

#include "Core/uuid.h"

class engine_context;

namespace cereal
{
	class JSONOutputArchive;
	class JSONInputArchive;
}


//	ゲーム用オブジェクト
class game_object : public std::enable_shared_from_this<game_object>
{
public:
	static constexpr std::string_view type_name() {return NAMEOF_SHORT_TYPE(game_object);}
	virtual std::string_view get_type_name() const {return type_name();}
	virtual bool check_type(std::string_view text) const {return type_name() == text;}

public:
	//	名前取得
	std::string_view get_name() const;
	void set_name(std::string_view name);

	//	アクティブフラグ取得
	bool is_active() const;
	void set_active(bool active);

	//	一時オブジェクトフラグ取得
	bool is_transient() const { return transient; }
	void set_transient(bool value) { transient = value; }

	//	UUID取得
	const core::uuid& get_uuid() const { return uuid; }
	void set_uuid(const core::uuid& value) { uuid = value; }

	//	エンジンコンテキスト取得
	bool has_context() const { return context != nullptr; }
	engine_context& get_context() const;
	void set_context(engine_context& context) { this->context = &context; }

public:
	game_object(std::string_view name);
	virtual ~game_object();
	virtual void fixed_update(float elapsed_time);
	virtual void update(float elapsed_time);
	virtual void post_update(float elapsed_time);

	virtual void pre_render();
	virtual void post_render();

#if defined(USE_IMGUI)
	virtual void debug_start_of_frame() {}
	virtual void debug_pre_update(float elapsed_time) {}
	virtual void debug_update(float elapsed_time) {}
	virtual void debug_post_update(float elapsed_time) {}

	bool is_display_inspector() { return display_inspector; }
	void set_display_inspector(bool flag) { display_inspector = flag; }

	virtual void imgui_render_core();

	//	インスペクター
	void imgui_render_inspector();

#endif	//	defined(USE_IMGUI)

	virtual void on_initialize();
	virtual void on_finalize();

public:
	//	保存・読込
	virtual void serialize(cereal::JSONOutputArchive& archive) const;
	virtual void deserialize(cereal::JSONInputArchive& archive);

protected:
	virtual void on_active();
	virtual void on_deactive();

private:
	cache_string name;
	core::uuid uuid;
	bool active;
	bool transient = false;
	engine_context* context = nullptr;
#if defined(USE_IMGUI)
	bool display_inspector = true;
#endif	//	defined(USE_IMGUI)
};

#define	GAME_OBJECT_DECRLEAR(ABSTRACT_TYPE, BASE_TYPE)	\
public:	\
using base = BASE_TYPE;	\
	static constexpr std::string_view type_name() {return NAMEOF_SHORT_TYPE(ABSTRACT_TYPE);}	\
	std::string_view get_type_name() const override {return type_name();}	\
	bool check_type(std::string_view text) const override {return type_name() == text ? true : BASE_TYPE::check_type(text);}	\
private:
