#pragma once

#include "collision/collision.h"
#include "object/spatial_object.h"

#if defined(USE_IMGUI)
#include "Editor/editor_items.h"
#endif	//	defined(USE_IMGUI)

//	ライトオブジェクト
class light_object : public spatial_object
{
	GAME_OBJECT_DECRLEAR(light_object, spatial_object)

public:
	light_ptr get_light() const;

	void set_flag(light::flags flag, bool value);
	bool is_flag(light::flags flag) const;

	void set_intensity(float intensity);
	float get_intensity() const;
	void set_color(const math::float3& color);
	const math::float3& get_color() const;

public:
	light_object(std::string_view name);
	~light_object() override;

	void post_update(float elapsed_time) override;
	void pre_render() override;

	void serialize(cereal::JSONOutputArchive& archive) const override;
	void deserialize(cereal::JSONInputArchive& archive) override;

#if defined(USE_IMGUI)
	void imgui_render_core() override;
	virtual void imgui_tree_inner_render() = 0;
#endif	//	defined(USE_IMGUI)

protected:
	void on_active() override;
	void on_deactive() override;

	void sync_to_light();

#if defined(USE_IMGUI)
	void imgui_render_light_parameters();
#endif	//	defined(USE_IMGUI)

protected:
	light_ptr light;

private:
	std::uint32_t applied_spatial_version = UINT32_MAX;
};

//	平行光源オブジェクト
class directional_light_object : public light_object
{
	GAME_OBJECT_DECRLEAR(directional_light_object, light_object)

public:
	directional_light_object(std::string_view name);
	~directional_light_object() override;

	void serialize(cereal::JSONOutputArchive& archive) const override;
	void deserialize(cereal::JSONInputArchive& archive) override;

#if defined(USE_IMGUI)
	void imgui_tree_inner_render() override;
#endif	//	defined(USE_IMGUI)
};

//	点光源オブジェクト
class point_light_object : public light_object
{
	GAME_OBJECT_DECRLEAR(point_light_object, light_object)

public:
	void set_range(float radius);
	float get_range() const;

public:
	point_light_object(std::string_view name);
	~point_light_object() override;

	void serialize(cereal::JSONOutputArchive& archive) const override;
	void deserialize(cereal::JSONInputArchive& archive) override;

#if defined(USE_IMGUI)
	void imgui_tree_inner_render() override;
#endif	//	defined(USE_IMGUI)
};

//	スポットライトオブジェクト
class spot_light_object : public light_object
{
	GAME_OBJECT_DECRLEAR(spot_light_object, light_object)

public:
	void set_range(float radius);
	float get_range() const;

	void set_inner_cone(float degree);
	float get_inner_cone() const;

	void set_outer_cone(float degree);
	float get_outer_cone() const;

public:
	spot_light_object(std::string_view name);
	~spot_light_object() override;

	void serialize(cereal::JSONOutputArchive& archive) const override;
	void deserialize(cereal::JSONInputArchive& archive) override;

#if defined(USE_IMGUI)
	void imgui_tree_inner_render() override;
#endif	//	defined(USE_IMGUI)
};
