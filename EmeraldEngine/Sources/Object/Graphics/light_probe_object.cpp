#include "Object/Graphics/light_probe_object.h"
#include "Object/game_object_registry.h"

#include <algorithm>

#include "Texture/texture_resource_utility.h"
#include "Object/game_object_serializer.h"

#include "cereal/cereal.hpp"
#include "cereal/archives/json.hpp"
#include "cereal/types/common.hpp"

light_probe_ptr light_probe_object::get_light_probe() const
{
	return light_probe;
}

void light_probe_object::set_flag(light_probe::flags flag, bool value)
{
	light_probe->set_flag(flag, value);
}

bool light_probe_object::is_flag(light_probe::flags flag) const
{
	return light_probe->is_flag(flag);
}

void light_probe_object::set_intensity(float intensity)
{
	light_probe->set_intensity(intensity);
}

float light_probe_object::get_intensity() const
{
	return light_probe->get_intensity();
}

void light_probe_object::set_color(const math::float3& color)
{
	light_probe->set_color(color);
}

const math::float3& light_probe_object::get_color() const
{
	return light_probe->get_color();
}

void light_probe_object::set_diffuse_ibl_texture(texture_resource_ptr texture)
{
	diffuse_ibl_texture = texture;
	apply_ibl_textures();
}

const texture_resource_ptr& light_probe_object::get_diffuse_ibl_texture() const
{
	return diffuse_ibl_texture.get();
}

void light_probe_object::set_specular_ibl_texture(texture_resource_ptr texture)
{
	specular_ibl_texture = texture;
	apply_ibl_textures();
}

const texture_resource_ptr& light_probe_object::get_specular_ibl_texture() const
{
	return specular_ibl_texture.get();
}

void light_probe_object::set_lut_ggx_texture(texture_resource_ptr texture)
{
	lut_ggx_texture = texture;
	apply_ibl_textures();
}

const texture_resource_ptr& light_probe_object::get_lut_ggx_texture() const
{
	return lut_ggx_texture.get();
}

void light_probe_object::set_source_type(source_types type)
{
	source_type = type;
	apply_ibl_textures();
}

void light_probe_object::apply_ibl_textures()
{
	if (!light_probe)
		return;

	const bool use_texture = source_type == source_types::specified_texture;
	auto get_texture = [use_texture](const resource::asset_handle<texture_resource>& handle) -> texture_ptr
	{
		return use_texture && handle.get() ? handle.get()->get_texture() : nullptr;
	};
	light_probe->set_diffuse_ibl_texture(get_texture(diffuse_ibl_texture));
	light_probe->set_specular_ibl_texture(get_texture(specular_ibl_texture));
	light_probe->set_lut_ggx_texture(get_texture(lut_ggx_texture));
}

light_probe_object::light_probe_object(std::string_view name)
: spatial_object(name)
{
#if defined(USE_IMGUI)
	items_context = editor_items::generate_context();
#endif	//	defined(USE_IMGUI)
}

light_probe_object::~light_probe_object()
{
}

void light_probe_object::post_update(float elapsed_time)
{
	base::post_update(elapsed_time);
	sync_to_light_probe();
}

void light_probe_object::pre_render()
{
	base::pre_render();
	sync_to_light_probe();
}

void light_probe_object::sync_to_light_probe()
{
	if (!light_probe)
		return;

	const std::uint32_t spatial_version = get_spatial()->get_version();
	if (applied_spatial_version == spatial_version)
		return;

	applied_spatial_version = spatial_version;
	light_probe->set_position(get_world_translation());
}

#if defined(USE_IMGUI)
void light_probe_object::imgui_render_core()
{
	base::imgui_render_core();
	sync_to_light_probe();

	//	強さ
	float intensity = get_intensity();
	if (ImGui::DragFloat("intensity (nit)", &intensity, 10.0f, 0.0f, FLT_MAX, "%.1f"))
		set_intensity((std::max)(intensity, 0.0f));
	math::float3 color = get_color();
	if (ImGui::ColorEditGamut3("color", &color.x, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR))
		set_color(color);

	//	IBLテクスチャ
	auto edit_ibl_texture = [&](const char* label, const resource::asset_handle<texture_resource>& current, void (light_probe_object::*setter)(texture_resource_ptr))
	{
		resource::asset_handle<texture_resource> handle = current;
		ImGui::PushID(label);
		ImGui::Text(label);
		ImGui::SameLine();
		if (editor_items::edit_texture_item(items_context, handle))
			(this->*setter)(handle.get());
		ImGui::PopID();
	};

	//	IBL の取得元
	static constexpr const char* source_type_names[] = { "captured", "specified_texture" };
	int source_type_index = static_cast<int>(source_type);
	if (ImGui::Combo("source", &source_type_index, source_type_names, IM_ARRAYSIZE(source_type_names)))
		set_source_type(static_cast<source_types>(source_type_index));

	if (source_type == source_types::specified_texture)
	{
		edit_ibl_texture("diffuse_ibl", diffuse_ibl_texture, &light_probe_object::set_diffuse_ibl_texture);
		edit_ibl_texture("specular_ibl", specular_ibl_texture, &light_probe_object::set_specular_ibl_texture);
		edit_ibl_texture("lut_ggx", lut_ggx_texture, &light_probe_object::set_lut_ggx_texture);
	}
	else
	{
		ImGui::TextDisabled("capture is not implemented");
	}

	imgui_tree_inner_render();
}
#endif	//	defined(USE_IMGUI)

void light_probe_object::on_active()
{
	light_probe->set_flag(light_probe::flags::active, true);
}

void light_probe_object::on_deactive()
{
	light_probe->set_flag(light_probe::flags::active, false);
}

////////////////////////////////////////////////////////////////////////////////////////////////
sky_light_object::sky_light_object(std::string_view name)
: light_probe_object(name)
{
	light_probe = std::make_shared<::light_probe>();
	light_probe->set_flag(light_probe::flags::unbind, true);
	set_world_translation(math::vector3d(90.0, 90.0, 90.0));
	sync_to_light_probe();
}

sky_light_object::~sky_light_object()
{
}

#if defined(USE_IMGUI)
void sky_light_object::imgui_tree_inner_render()
{
}
#endif	//	defined(USE_IMGUI)

////////////////////////////////////////////////////////////////////////////////////////////////
void area_light_probe_object::set_inner_range(math::float3 range)
{
	light_probe->set_inner_range(range);
}

math::float3 area_light_probe_object::get_inner_range() const
{
	return light_probe->get_inner_range();
}

void area_light_probe_object::set_outer_range(math::float3 range)
{
	light_probe->set_outer_range(range);
}

math::float3 area_light_probe_object::get_outer_range() const
{
	return light_probe->get_outer_range();
}

area_light_probe_object::area_light_probe_object(std::string_view name)
: light_probe_object(name)
{
	light_probe = std::make_shared<::light_probe>();
	light_probe->set_flag(light_probe::flags::unbind, false);
	set_world_translation(math::vector3d(90.0, 90.0, 90.0));
	sync_to_light_probe();
	set_outer_range({100.0f, 100.0f, 100.0f});
}

area_light_probe_object::~area_light_probe_object()
{
}

#if defined(USE_IMGUI)
void area_light_probe_object::imgui_tree_inner_render()
{
	auto inner_range = get_inner_range();
	ImGui::DragFloat3("inner_range", &inner_range.x, 0.01f);
	set_inner_range(inner_range);

	auto outer_range = get_outer_range();
	ImGui::DragFloat3("outer_range", &outer_range.x, 0.01f);
	set_outer_range(outer_range);
}
#endif	//	defined(USE_IMGUI)

template<class Archive>
void light_probe_object::serialize_fields(Archive& archive, std::uint32_t version)
{
	float intensity = get_intensity();
	math::float3 color = get_color();
	archive(CEREAL_NVP(intensity), CEREAL_NVP(color));
	switch (version)
	{
		case 0:
		{
			source_type = source_types::specified_texture;
			break;
		}
		case 1:
		{
			archive(CEREAL_NVP(source_type));
			break;
		}
	}

	core::uuid diffuse_ibl_texture_uuid = diffuse_ibl_texture.get_reference().uuid;
	core::uuid specular_ibl_texture_uuid = specular_ibl_texture.get_reference().uuid;
	core::uuid lut_ggx_texture_uuid = lut_ggx_texture.get_reference().uuid;
	game_object_serializer::serialize_asset_reference(archive, "diffuse_ibl_texture", diffuse_ibl_texture_uuid);
	game_object_serializer::serialize_asset_reference(archive, "specular_ibl_texture", specular_ibl_texture_uuid);
	game_object_serializer::serialize_asset_reference(archive, "lut_ggx_texture", lut_ggx_texture_uuid);

	if constexpr (Archive::is_loading::value)
	{
		auto load_texture = [](const core::uuid& uuid) -> texture_resource_ptr
		{
			if (uuid.is_empty())
				return nullptr;
			return texture_resource_utility::load_texture_resource(game_object_serializer::resolve_asset_path(uuid));
		};
		set_intensity(intensity);
		set_color(color);
		set_diffuse_ibl_texture(load_texture(diffuse_ibl_texture_uuid));
		set_specular_ibl_texture(load_texture(specular_ibl_texture_uuid));
		set_lut_ggx_texture(load_texture(lut_ggx_texture_uuid));
	}
}

void light_probe_object::serialize(cereal::JSONOutputArchive& archive) const
{
	base::serialize(archive);
	game_object_serializer::save_fields(archive, "light_probe_object", *this);
}

void light_probe_object::deserialize(cereal::JSONInputArchive& archive)
{
	base::deserialize(archive);
	game_object_serializer::load_fields(archive, "light_probe_object", *this);
}

void area_light_probe_object::serialize(cereal::JSONOutputArchive& archive) const
{
	base::serialize(archive);

	math::float3 inner_range = get_inner_range();
	math::float3 outer_range = get_outer_range();
	archive(CEREAL_NVP(inner_range), CEREAL_NVP(outer_range));
}

void area_light_probe_object::deserialize(cereal::JSONInputArchive& archive)
{
	base::deserialize(archive);

	math::float3 inner_range;
	math::float3 outer_range;
	archive(CEREAL_NVP(inner_range), CEREAL_NVP(outer_range));
	set_inner_range(inner_range);
	set_outer_range(outer_range);
}
