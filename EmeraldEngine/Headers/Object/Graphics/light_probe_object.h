#pragma once

#include "collision/collision.h"
#include "object/spatial_object.h"
#include "Resource/asset_handle.h"
#include "Texture/texture_resource.h"

#if defined(USE_IMGUI)
#include "Editor/editor_items.h"
#endif	//	defined(USE_IMGUI)

//	ライトプローブオブジェクト
class light_probe_object : public spatial_object
{
	GAME_OBJECT_DECRLEAR(light_probe_object, spatial_object)

public:
	static constexpr std::uint32_t serialize_version = 1;

	enum class source_types : std::uint8_t
	{
		captured,			//	キャプチャしたもの
		specified_texture,	//	指定した IBL テクスチャ
	};

public:
	light_probe_ptr get_light_probe() const;

	void set_flag(light_probe::flags flag, bool value);
	bool is_flag(light_probe::flags flag) const;

	void set_intensity(float intensity);
	float get_intensity() const;
	void set_color(const math::float3& color);
	const math::float3& get_color() const;

	void set_source_type(source_types type);
	source_types get_source_type() const { return source_type; }

	void set_diffuse_ibl_texture(texture_resource_ptr texture);
	const texture_resource_ptr& get_diffuse_ibl_texture() const;

	void set_specular_ibl_texture(texture_resource_ptr texture);
	const texture_resource_ptr& get_specular_ibl_texture() const;

	void set_lut_ggx_texture(texture_resource_ptr texture);
	const texture_resource_ptr& get_lut_ggx_texture() const;

public:
	light_probe_object(std::string_view name);
	~light_probe_object() override;

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

	void sync_to_light_probe();

	void apply_ibl_textures();

protected:
	light_probe_ptr light_probe;

private:
	friend class game_object_serializer;
	template<class Archive>
	void serialize_fields(Archive& archive, std::uint32_t version);

	std::uint32_t applied_spatial_version = UINT32_MAX;

	resource::asset_handle<texture_resource> diffuse_ibl_texture;
	resource::asset_handle<texture_resource> specular_ibl_texture;
	resource::asset_handle<texture_resource> lut_ggx_texture;
	source_types source_type = source_types::specified_texture;

#if defined(USE_IMGUI)
	std::shared_ptr<editor_items::context> items_context;
#endif	//	defined(USE_IMGUI)
};

//	スカイライトオブジェクト
class sky_light_object : public light_probe_object
{
	GAME_OBJECT_DECRLEAR(sky_light_object, light_probe_object)

public:
	sky_light_object(std::string_view name);
	~sky_light_object() override;

#if defined(USE_IMGUI)
	void imgui_tree_inner_render() override;
#endif	//	defined(USE_IMGUI)
};

//	範囲制限有りライトプローブオブジェクト
class area_light_probe_object : public light_probe_object
{
	GAME_OBJECT_DECRLEAR(area_light_probe_object, light_probe_object)

public:
	void set_inner_range(math::float3 range);
	math::float3 get_inner_range() const;

	void set_outer_range(math::float3 range);
	math::float3 get_outer_range() const;

public:
	area_light_probe_object(std::string_view name);
	~area_light_probe_object() override;

	void serialize(cereal::JSONOutputArchive& archive) const override;
	void deserialize(cereal::JSONInputArchive& archive) override;

#if defined(USE_IMGUI)
	void imgui_tree_inner_render() override;
#endif	//	defined(USE_IMGUI)
};
