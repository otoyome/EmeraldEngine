#include "Object/Graphics/light_object.h"
#include "Object/game_object_registry.h"

#include <algorithm>

#include "Graphics/Core/color_space.h"
#include "Graphics/Lighting/light_utility.h"

#include "cereal/cereal.hpp"
#include "cereal/archives/json.hpp"
#include "cereal/types/common.hpp"
#include "cereal/types/vector.hpp"

template<class Archive>
void serialize(Archive& archive, light::shadow_map_bias_data& value)
{
	//	@tool	CEREAL_NVP を使うため、メンバーを同名の参照で受ける (キー名は従来と同じ)
	float& depth_bias = value.depth_bias;
	float& slope_depth_bias_scale = value.slope_depth_bias_scale;
	float& max_depth_bias = value.max_depth_bias;
	float& slope_depth_bias = value.slope_depth_bias;
	float& max_slope_depth_bias = value.max_slope_depth_bias;
	float& sigma_factor = value.sigma_factor;
	archive(CEREAL_NVP(depth_bias),
		CEREAL_NVP(slope_depth_bias_scale),
		CEREAL_NVP(max_depth_bias),
		CEREAL_NVP(slope_depth_bias),
		CEREAL_NVP(max_slope_depth_bias),
		CEREAL_NVP(sigma_factor));
}

light_ptr light_object::get_light() const
{
	return light;
}

void light_object::set_flag(light::flags flag, bool value)
{
	light->set_flag(flag, value);
}

bool light_object::is_flag(light::flags flag) const
{
	return light->is_flag(flag);
}

void light_object::set_intensity(float intensity)
{
	light->set_intensity(intensity);
}

float light_object::get_intensity() const
{
	return light->get_intensity();
}

void light_object::set_color(const math::float3& color)
{
	light->set_color(color);
}

const math::float3& light_object::get_color() const
{
	return light->get_color();
}

#if defined(USE_IMGUI)
void light_object::imgui_render_light_parameters()
{
	//	単位(平行光源は lx 固定)
	if (light->get_light_type() != light_type::directional)
	{
		auto unit_mode = light->get_unit_mode();
		if (ImGui::Combo("unit", unit_mode) && unit_mode != light::unit_modes::lux)
			light->set_unit_mode(unit_mode);
	}
	float intensity = light->get_intensity();
	if (ImGui::DragFloat("intensity", &intensity, 1.0f, 0.0f, FLT_MAX, "%.3f"))
		light->set_intensity((std::max)(intensity, 0.0f));
	ImGui::SameLine();
	ImGui::TextDisabled("[%s]", magic_enum::enum_name(light->get_unit_mode()).data());

	//	色
	auto color_mode = light->get_color_mode();
	if (ImGui::Combo("color_mode", color_mode))
		light->set_color_mode(color_mode);

	math::float3 color = light->get_color();
	const char* color_label = color_mode == light::color_modes::filter_and_temperature ? "filter" : "color";
	if (ImGui::ColorEditGamut3(color_label, &color.x, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR))
		light->set_color(color);

	if (color_mode == light::color_modes::filter_and_temperature)
	{
		int kelvin = light->get_kelvin();
		if (ImGui::SliderInt("kelvin", &kelvin, light_utility::kelvin_minimum, light_utility::kelvin_maximum))
			light->set_kelvin(kelvin);

		//	色温度の色見本
		//	Rec.2020 → Rec.709 → sRGB、最大成分を 1 にして色味だけを見せる
		math::vector3f swatch = color_space::convert_gamut(light_utility::get_temperature_color(kelvin), color_space::gamuts::rec2020, color_space::gamuts::rec709);
		swatch = math::vector3f((std::max)(swatch[0], 0.0f), (std::max)(swatch[1], 0.0f), (std::max)(swatch[2], 0.0f));
		const float maximum = (std::max)({ swatch[0], swatch[1], swatch[2], 1e-6f });
		swatch = color_space::convert_linear_to_srgb(swatch * (1.0f / maximum));
		ImGui::SameLine();
		ImGui::ColorButton("##kelvin_color", ImVec4(swatch[0], swatch[1], swatch[2], 1.0f), ImGuiColorEditFlags_NoTooltip);
	}
}
#endif	//	defined(USE_IMGUI)

light_object::light_object(std::string_view name)
: spatial_object(name)
{
}

light_object::~light_object()
{
}

void light_object::post_update(float elapsed_time)
{
	base::post_update(elapsed_time);
	sync_to_light();
}

void light_object::pre_render()
{
	base::pre_render();
	sync_to_light();
}

void light_object::sync_to_light()
{
	if (!light)
		return;

	const std::uint32_t spatial_version = get_spatial()->get_version();
	if (applied_spatial_version == spatial_version)
		return;

	applied_spatial_version = spatial_version;
	light->set_position(get_world_translation());
	light->set_rotation(math::rotatorf(get_world_euler()));
}

#if defined(USE_IMGUI)
void light_object::imgui_render_core()
{
	base::imgui_render_core();
	sync_to_light();

	imgui_tree_inner_render();
}
#endif	//	defined(USE_IMGUI)

void light_object::on_active()
{
	light->set_flag(light::flags::active, true);
}

void light_object::on_deactive()
{
	light->set_flag(light::flags::active, false);
}

////////////////////////////////////////////////////////////////////////////////////////////////
directional_light_object::directional_light_object(std::string_view name)
: light_object(name)
{
	light = std::make_shared<::light>(light_type::directional);
	math::rotatorf rotation;
	rotation = math::rotatorf::from_direction(math::vector3f(1, -1, -1));
	set_world_euler(math::rotatord(rotation));
	set_world_translation(math::vector3d::zero());
	sync_to_light();
}

directional_light_object::~directional_light_object()
{
}

#if defined(USE_IMGUI)
void directional_light_object::imgui_tree_inner_render()
{
	bool use_shadow = light->is_flag(light::flags::use_shadow);
	ImGui::Checkbox("use_shadow", &use_shadow);
	light->set_flag(light::flags::use_shadow, use_shadow);

	bool debug_csm_area_flag = light->is_flag(light::flags::debug_csm_area_flag);
	ImGui::Checkbox("debug_csm_area_flag", &debug_csm_area_flag);
	light->set_flag(light::flags::debug_csm_area_flag, debug_csm_area_flag);

	bool debug_visualize_ailiasing_error_flag = light->is_flag(light::flags::debug_visualize_ailiasing_error_flag);
	ImGui::Checkbox("debug_visualize_ailiasing_error_flag", &debug_visualize_ailiasing_error_flag);
	light->set_flag(light::flags::debug_visualize_ailiasing_error_flag, debug_visualize_ailiasing_error_flag);

	imgui_render_light_parameters();

	auto shadow_attenuation =  light->get_shadow_attenuation();
	ImGui::SliderFloat3("shadow_attenuation", &shadow_attenuation.x, 0.0f, 1.0f);
	light->set_shadow_attenuation(shadow_attenuation);

	auto pssm_split_weight = light->get_pssm_split_weight();
	ImGui::SliderFloat("pssm_split_weight", &pssm_split_weight, 0.0f, 1.0f, "%.6f");
	light->set_pssm_split_weight(pssm_split_weight);

	auto csm_distance = light->get_csm_distance();
	ImGui::DragFloat("csm_distance", &csm_distance, 1.0f, 0.0f, 100000.0f, "%.1f");
	light->set_csm_distance(csm_distance);

	ImGui::Separator();
	for (int index = 0; index < common_pipeline_states::shadow_map_max; ++index)
	{
		std::string text = "shadow_bias ";
		text += std::to_string(index);
		int id = 0xFF34FF00 + index;
		if (ImGui::TreeNode(text.data()))
		{
			ImGui::PushID(id);
			auto shadow_bias = light->get_shadow_bias(index);
			ImGui::SliderFloat("depth_bias", &shadow_bias.depth_bias, 0.0f, 1.0f, "%.6f");
			ImGui::SliderFloat("slope_depth_bias_scale", &shadow_bias.slope_depth_bias_scale, 0.0f, 1.0f, "%.6f");

			ImGui::SliderFloat("max_depth_bias", &shadow_bias.max_depth_bias, 0.0f, 1.0f, "%.6f");
			ImGui::SliderFloat("slope_depth_bias", &shadow_bias.slope_depth_bias, 0.0f, 5.0f, "%.6f");
			ImGui::SliderFloat("max_slope_depth_bias", &shadow_bias.max_slope_depth_bias, 0.0f, 5.0f, "%.6f");
			ImGui::SliderFloat("sigma_factor", &shadow_bias.sigma_factor, 0.0f, 20.0f, "%.6f");
			light->set_shadow_bias(index, shadow_bias);
			ImGui::PopID();
			ImGui::TreePop();
		}
	}
}
#endif	//	defined(USE_IMGUI)

////////////////////////////////////////////////////////////////////////////////////////////////
void point_light_object::set_range(float radius)
{
	light->set_range(radius);
}

float point_light_object::get_range() const
{
	return light->get_range();
}

point_light_object::point_light_object(std::string_view name)
: light_object(name)
{
	light = std::make_shared<::light>(light_type::point);
	set_world_translation(math::vector3d::zero());
	sync_to_light();
	set_range(10.0f);

	//	バイアスなどの指定
	{
		int index = 0;
		auto bias = light->get_shadow_bias(index);
		bias.depth_bias = 0;
		bias.sigma_factor = 1.0f;
		light->set_shadow_bias(index, bias);
	}
	{
		int index = 1;
		auto bias = light->get_shadow_bias(index);
		bias.depth_bias = 0;
		bias.sigma_factor = 1.0f;
		light->set_shadow_bias(index, bias);
	}
	{
		int index = 2;
		auto bias = light->get_shadow_bias(index);
		bias.depth_bias = 0;
		bias.sigma_factor = 1.0f;
		light->set_shadow_bias(index, bias);
	}
	{
		int index = 3;
		auto bias = light->get_shadow_bias(index);
		bias.depth_bias = 0.0f;
		bias.sigma_factor = 1.0f;
		light->set_shadow_bias(index, bias);
	}
	{
		int index = 4;
		auto bias = light->get_shadow_bias(index);
		bias.depth_bias = 0.0f;
		bias.sigma_factor = 1.0f;
		light->set_shadow_bias(index, bias);
	}
	{
		int index = 5;
		auto bias = light->get_shadow_bias(index);
		bias.depth_bias = 0.0f;
		bias.sigma_factor = 1.0f;
		light->set_shadow_bias(index, bias);
	}
}

point_light_object::~point_light_object()
{
}

#if defined(USE_IMGUI)
void point_light_object::imgui_tree_inner_render()
{
	bool use_shadow = light->is_flag(light::flags::use_shadow);
	ImGui::Checkbox("use_shadow", &use_shadow);
	light->set_flag(light::flags::use_shadow, use_shadow);

	auto range = get_range();
	ImGui::DragFloat("range", &range, 0.01f);
	set_range(range);

	imgui_render_light_parameters();
}
#endif	//	defined(USE_IMGUI)

////////////////////////////////////////////////////////////////////////////////////////////////
void spot_light_object::set_range(float radius)
{
	light->set_range(radius);
}

float spot_light_object::get_range() const
{
	return light->get_range();
}

void spot_light_object::set_inner_cone(float degree)
{
	light->set_inner_cone(math::angle::to_radian(degree));
}

float spot_light_object::get_inner_cone() const
{
	return math::angle::to_degree(light->get_inner_cone());
}

void spot_light_object::set_outer_cone(float degree)
{
	light->set_outer_cone(math::angle::to_radian(degree));
}

float spot_light_object::get_outer_cone() const
{
	return math::angle::to_degree(light->get_outer_cone());
}

spot_light_object::spot_light_object(std::string_view name)
: light_object(name)
{
	light = std::make_shared<::light>(light_type::spot);
	set_world_translation(math::vector3d::zero());
	sync_to_light();
	set_range(10.0f);
	set_inner_cone(45.0f);
	set_outer_cone(60.0f);
}

spot_light_object::~spot_light_object()
{
}

#if defined(USE_IMGUI)
void spot_light_object::imgui_tree_inner_render()
{
	bool use_shadow = is_flag(light::flags::use_shadow);
	ImGui::Checkbox("use_shadow", &use_shadow);
	set_flag(light::flags::use_shadow, use_shadow);

	auto range = get_range();
	ImGui::DragFloat("range", &range, 0.01f);
	set_range(range);

	auto inner_cone = get_inner_cone();
	ImGui::DragFloat("inner_cone", &inner_cone, 0.01f);
	set_inner_cone(inner_cone);

	auto outer_cone = get_outer_cone();
	ImGui::DragFloat("outer_cone", &outer_cone, 0.01f);
	set_outer_cone(outer_cone);

	imgui_render_light_parameters();
}
#endif	//	defined(USE_IMGUI)

////////////////////////////////////////////////////////////////////////////////////////////////
void light_object::serialize(cereal::JSONOutputArchive& archive) const
{
	base::serialize(archive);

	bool use_shadow = light->is_flag(light::flags::use_shadow);
	bool contact_shadow = light->is_flag(light::flags::contact_shadow);
	light::unit_modes unit_mode = light->get_unit_mode();
	float intensity = light->get_intensity();
	light::color_modes color_mode = light->get_color_mode();
	math::float3 color = light->get_color();
	int kelvin = light->get_kelvin();
	float max_draw_distance = light->get_max_draw_distance();
	std::vector<light::shadow_map_bias_data> shadow_biases;
	for (int index = 0; index < static_cast<int>(common_pipeline_states::shadow_map_max); ++index)
		shadow_biases.emplace_back(light->get_shadow_bias(index));

	archive(CEREAL_NVP(use_shadow),
		CEREAL_NVP(contact_shadow),
		CEREAL_NVP(unit_mode),
		CEREAL_NVP(intensity),
		CEREAL_NVP(color_mode),
		CEREAL_NVP(color),
		CEREAL_NVP(kelvin),
		CEREAL_NVP(max_draw_distance),
		CEREAL_NVP(shadow_biases));
}

void light_object::deserialize(cereal::JSONInputArchive& archive)
{
	base::deserialize(archive);

	bool use_shadow = false;
	bool contact_shadow = false;
	light::unit_modes unit_mode = light->get_unit_mode();
	float intensity = 0.0f;
	light::color_modes color_mode = light->get_color_mode();
	math::float3 color;
	int kelvin = 0;
	float max_draw_distance = 0.0f;
	std::vector<light::shadow_map_bias_data> shadow_biases;

	archive(CEREAL_NVP(use_shadow),
		CEREAL_NVP(contact_shadow),
		CEREAL_NVP(unit_mode),
		CEREAL_NVP(intensity),
		CEREAL_NVP(color_mode),
		CEREAL_NVP(color),
		CEREAL_NVP(kelvin),
		CEREAL_NVP(max_draw_distance),
		CEREAL_NVP(shadow_biases));

	light->set_flag(light::flags::use_shadow, use_shadow);
	light->set_flag(light::flags::contact_shadow, contact_shadow);
	light->set_unit_mode(unit_mode);
	light->set_intensity(intensity);
	light->set_color_mode(color_mode);
	light->set_color(color);
	light->set_kelvin(kelvin);
	light->set_max_draw_distance(max_draw_distance);
	const size_t bias_count = (std::min)(shadow_biases.size(), static_cast<size_t>(common_pipeline_states::shadow_map_max));
	for (size_t index = 0; index < bias_count; ++index)
		light->set_shadow_bias(static_cast<int>(index), shadow_biases[index]);
}

void directional_light_object::serialize(cereal::JSONOutputArchive& archive) const
{
	base::serialize(archive);

	float csm_distance = light->get_csm_distance();
	float pssm_split_weight = light->get_pssm_split_weight();
	math::float3 shadow_attenuation = light->get_shadow_attenuation();
	archive(CEREAL_NVP(csm_distance), CEREAL_NVP(pssm_split_weight), CEREAL_NVP(shadow_attenuation));
}

void directional_light_object::deserialize(cereal::JSONInputArchive& archive)
{
	base::deserialize(archive);

	float csm_distance = 0.0f;
	float pssm_split_weight = 0.0f;
	math::float3 shadow_attenuation;
	archive(CEREAL_NVP(csm_distance), CEREAL_NVP(pssm_split_weight), CEREAL_NVP(shadow_attenuation));

	light->set_csm_distance(csm_distance);
	light->set_pssm_split_weight(pssm_split_weight);
	light->set_shadow_attenuation(shadow_attenuation);
}

void point_light_object::serialize(cereal::JSONOutputArchive& archive) const
{
	base::serialize(archive);

	float range = get_range();
	archive(CEREAL_NVP(range));
}

void point_light_object::deserialize(cereal::JSONInputArchive& archive)
{
	base::deserialize(archive);

	float range = 0.0f;
	archive(CEREAL_NVP(range));
	set_range(range);
}

void spot_light_object::serialize(cereal::JSONOutputArchive& archive) const
{
	base::serialize(archive);

	float range = get_range();
	float inner_cone = get_inner_cone();
	float outer_cone = get_outer_cone();
	archive(CEREAL_NVP(range), CEREAL_NVP(inner_cone), CEREAL_NVP(outer_cone));
}

void spot_light_object::deserialize(cereal::JSONInputArchive& archive)
{
	base::deserialize(archive);

	float range = 0.0f;
	float inner_cone = 0.0f;
	float outer_cone = 0.0f;
	archive(CEREAL_NVP(range), CEREAL_NVP(inner_cone), CEREAL_NVP(outer_cone));
	set_range(range);
	set_inner_cone(inner_cone);
	set_outer_cone(outer_cone);
}
