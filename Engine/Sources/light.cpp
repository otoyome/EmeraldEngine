#include "light.h"
#include "texture.h"
#include "graphics_context.h"

namespace
{
	//	シェーダー用のライト情報に変換
	light_data convert_light_data(const light_render_data& light)
	{
		light_data data{};
		data.color = { light.color.x * light.color.w, light.color.y * light.color.w, light.color.z * light.color.w };
		data.attribute = light_data_utility::make_light_attribute(light.attribute, light.type);
		switch(light.type)
		{
			case light_type::directional:
			{
				data.direction = light.direction;
				break;
			}
			case light_type::point:
			{
				data.position = light.position;
				data.inv_range = light_data_utility::convert_inv_range(light.range);
				break;
			}
			case light_type::spot:
			{
				data.position = light.position;
				data.inv_range = light_data_utility::convert_inv_range(light.range);
				data.direction = math::vector3f(light.direction).normalized().to_float3();
				data.cone = light_data_utility::encode_spot_cone(light.inner_cone, light.outer_cone);
				break;
			}
		}
		return data;
	}

	//	ライトの描画情報を描画原点からの相対座標で作る
	light_render_data make_light_render_data(const light& source, const math::vector3d& render_origin)
	{
		light_render_data ret;
		ret.type = source.get_light_type();
		ret.attribute = source.get_attribute();
		ret.position = (source.get_position() - render_origin).to_float3();
		ret.rotation = source.get_rotation();
		ret.direction = source.get_rotation().front().to_float3();
		ret.color = source.get_color();
		ret.range = source.get_range();
		ret.inner_cone = source.get_inner_cone();
		ret.outer_cone = source.get_outer_cone();
		ret.shadow_attenuation = source.get_shadow_attenuation();
		ret.csm_distance = source.get_csm_distance();
		ret.pssm_split_weight = source.get_pssm_split_weight();
		for (int index = 0; index < static_cast<int>(common_pipeline_states::shadow_map_max); ++index)
			ret.bias_data[index] = source.get_shadow_bias(index);
		ret.data = convert_light_data(ret);
		return ret;
	}

	//	ライトプローブの描画情報を描画原点からの相対座標で作る
	light_probe_render_data make_light_probe_render_data(const light_probe& source, const math::vector3d& render_origin)
	{
		light_probe_render_data ret;
		ret.attribute = source.get_attribute();

		const math::float3 position = (source.get_position() - render_origin).to_float3();
		const math::float3 inner_range = source.get_inner_range();
		const math::float3 outer_range = source.get_outer_range();
		ret.data.position.x = position.x;
		ret.data.position.y = position.y;
		ret.data.position.z = position.z;
		ret.data.inner_area.x = inner_range.x;
		ret.data.inner_area.y = inner_range.y;
		ret.data.inner_area.z = inner_range.z;
		ret.data.outer_area.x = outer_range.x;
		ret.data.outer_area.y = outer_range.y;
		ret.data.outer_area.z = outer_range.z;
		ret.data.color = source.get_color();

		ret.diffuse_ibl_texture = source.get_diffuse_ibl_texture();
		ret.specular_ibl_texture = source.get_specular_ibl_texture();
		ret.lut_ggx_texture = source.get_lut_ggx_texture();
		return ret;
	}
}

light::light(light_type type)
: system_flags(0)
, type(type)
, position(math::vector3d::zero())
, rotation(0, 0, 0)
, color(1, 1, 1, 1)
, parameter(10.0f, math::angle::to_radian(30.0f), math::angle::to_radian(45.0f))
{
	set_flag(flags::active, true);
	set_flag(flags::use_shadow, true);

	csm_distance = 5000.0f;
	pssm_split_weight = 0.8f;
	shadow_attenuation = {0.2f, 0.2f, 0.2f};

	switch(type)
	{
		case light_type::directional:
		{
			bias_data[0].max_depth_bias = 0.01f;
			bias_data[0].sigma_factor = 0.5f;
			bias_data[1].max_depth_bias = 0.015f;
			bias_data[1].sigma_factor = 1.25f;
			bias_data[2].max_depth_bias = 0.018f;
			bias_data[2].sigma_factor = 20.0f;
			bias_data[3].max_depth_bias = 0.1f;
			bias_data[3].sigma_factor = 20.0f;
			bias_data[4].max_depth_bias = 0.12f;
			bias_data[4].sigma_factor = 20.0f;
			bias_data[5].max_depth_bias = 0.15f;
			bias_data[5].sigma_factor = 20.0f;
			break;
		}
		case light_type::point:
		{
			bias_data[0].max_depth_bias = 0.01f;
			bias_data[1].max_depth_bias = 0.01f;
			bias_data[2].max_depth_bias = 0.01f;
			bias_data[3].max_depth_bias = 0.01f;
			bias_data[4].max_depth_bias = 0.01f;
			bias_data[5].max_depth_bias = 0.01f;
			break;
		}
		case light_type::spot:
		{
			bias_data[0].max_depth_bias = 0.01f;
			bias_data[1].max_depth_bias = 0.01f;
			bias_data[2].max_depth_bias = 0.01f;
			bias_data[3].max_depth_bias = 0.01f;
			bias_data[4].max_depth_bias = 0.01f;
			bias_data[5].max_depth_bias = 0.01f;
			break;
		}
	}
}

light::~light()
{
}

light_type light::get_light_type() const
{
	return type;
}

const math::vector3d& light::get_position() const
{
	return position;
}

void light::set_position(const math::vector3d& position)
{
	this->position = position;
}

const math::rotatorf& light::get_rotation() const
{
	return rotation;
}

void light::set_rotation(const math::rotatorf& rotation)
{
	this->rotation = rotation;
}

const math::float4& light::get_color() const
{
	return color;
}

void light::set_color(const math::float4& color)
{
	this->color = color;
}

float light::get_range() const
{
	return parameter.x;
}

void light::set_range(float range)
{
	this->parameter.x = range;
}

float light::get_inner_cone() const
{
	return parameter.y;
}

void light::set_inner_cone(float inner_cone)
{
	this->parameter.y = inner_cone;
}

float light::get_outer_cone() const
{
	return parameter.z;
}

void light::set_outer_cone(float outer_cone)
{
	this->parameter.z = outer_cone;
}

const math::float3& light::get_shadow_attenuation() const
{
	return shadow_attenuation;
}

void light::set_shadow_attenuation(const math::float3& attenuation)
{
	this->shadow_attenuation = attenuation;
}

float light::get_csm_distance() const
{
	return csm_distance;
}

void light::set_csm_distance(float distance)
{
	this->csm_distance = math::max(distance, 0.0001f);
}

float light::get_pssm_split_weight() const
{
	return pssm_split_weight;
}

void light::set_pssm_split_weight(float weight)
{
	this->pssm_split_weight = weight;
}

const light::shadow_map_bias_data& light::get_shadow_bias(int index) const
{
	return bias_data[math::clamp(index, 0, static_cast<int>(common_pipeline_states::shadow_map_max) - 1)];
}

void light::set_shadow_bias(int index, const shadow_map_bias_data& bias_data)
{
	if (index < 0 || index >= static_cast<int>(common_pipeline_states::shadow_map_max))
		return;
	this->bias_data[index] = bias_data;
}

////////////////////////////////////////////////////////////////////////////////////////////////
light_probe::light_probe()
: system_flags(0)
, diffuse_texture()
, specular_texture()
, lut_ggx_texture()
, position(math::vector3d::zero())
, inner_range(50.0f, 50.0f, 50.0f)
, outer_range(100.0f, 100.0f, 100.0f)
, color(1, 1, 1, 1)
{
	set_flag(flags::active, true);
}

light_probe::~light_probe()
{
}

texture_ptr light_probe::get_diffuse_ibl_texture() const
{
	return diffuse_texture;
}

texture_ptr light_probe::get_specular_ibl_texture() const
{
	return specular_texture;
}

texture_ptr light_probe::get_lut_ggx_texture() const
{
	return lut_ggx_texture;
}

void light_probe::set_diffuse_ibl_texture(texture_ptr texture)
{
	diffuse_texture = texture;
}

void light_probe::set_specular_ibl_texture(texture_ptr texture)
{
	specular_texture = texture;
}

void light_probe::set_lut_ggx_texture(texture_ptr texture)
{
	lut_ggx_texture = texture;
}

const math::vector3d& light_probe::get_position() const
{
	return position;
}

void light_probe::set_position(const math::vector3d& position)
{
	this->position = position;
}

math::float3 light_probe::get_inner_range() const
{
	return inner_range;
}

void light_probe::set_inner_range(const math::float3& inner_range)
{
	this->inner_range = inner_range;
}

math::float3 light_probe::get_outer_range() const
{
	return outer_range;
}

void light_probe::set_outer_range(const math::float3& outer_range)
{
	this->outer_range = outer_range;
}

const math::float4& light_probe::get_color() const
{
	return color;
}

void light_probe::set_color(const math::float4& color)
{
	this->color = color;
}

////////////////////////////////////////////////////////////////////////////////////////////////
light_manager::light_manager()
{
}

light_manager::~light_manager()
{
}

bool light_manager::generate_light(light_ptr& out_light, light_type type)
{
	out_light = std::make_shared<light>(type);
	lights.emplace_back(out_light);
	return true;
}

bool light_manager::generate_light_probe(light_probe_ptr& out_light_probe)
{
	out_light_probe = std::make_shared<light_probe>();
	light_probes.emplace_back(out_light_probe);
	return true;
}

void light_manager::get_container(light_container& container, const math::vector3d& render_origin)
{
	//	ライト
	{
		container.lights.clear();
		auto it = lights.begin();
		for (; it != lights.end();)
		{
			if (it->expired())
			{
				it = lights.erase(it);
				continue;
			}
			container.lights.emplace_back(make_light_render_data(*it->lock(), render_origin));
			++it;
		}
	}
	//	プローブ
	{
		container.light_probes.clear();
		auto it = light_probes.begin();
		for (; it != light_probes.end();)
		{
			if (it->expired())
			{
				it = light_probes.erase(it);
				continue;
			}
			container.light_probes.emplace_back(make_light_probe_render_data(*it->lock(), render_origin));
			++it;
		}
	}
}
