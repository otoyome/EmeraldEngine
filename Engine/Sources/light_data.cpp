#include "light_data.h"

#include "render_data.h"

#include <cstring>
#include <limits>

namespace
{
	UINT as_uint(float value)
	{
		UINT ret;
		std::memcpy(&ret, &value, sizeof(ret));
		return ret;
	}
	float as_float(UINT value)
	{
		float ret;
		std::memcpy(&ret, &value, sizeof(ret));
		return ret;
	}
}

directional_lights light_data_utility::convert_directional_lights(const light_data& data)
{
	directional_lights light;
	light.direction = { data.work_data[0].x, data.work_data[0].y, data.work_data[0].z };
	light.color = { data.work_data[1].x, data.work_data[1].y, data.work_data[1].z };
	light.attribute = as_uint(data.work_data[1].w);
	return light;
}

point_lights light_data_utility::convert_point_lights(const light_data& data)
{
	point_lights light;
	light.position = { data.work_data[0].x, data.work_data[0].y, data.work_data[0].z };
	light.inv_range = data.work_data[0].w;
	light.color = { data.work_data[1].x, data.work_data[1].y, data.work_data[1].z };
	light.attribute = as_uint(data.work_data[1].w);
	return light;
}

spot_lights light_data_utility::convert_spot_lights(const light_data& data)
{
	spot_lights light;
	light.position = { data.work_data[0].x, data.work_data[0].y, data.work_data[0].z };
	light.inv_range = data.work_data[0].w;
	light.color = { data.work_data[1].x, data.work_data[1].y, data.work_data[1].z };
	light.attribute = as_uint(data.work_data[1].w);
	light.direction = { data.work_data[2].x, data.work_data[2].y, data.work_data[2].z };
	light.cone = as_uint(data.work_data[2].w);
	return light;
}

light_data light_data_utility::convert_light_data(const directional_lights& light)
{
	light_data data{};
	data.work_data[0] = { light.direction.x, light.direction.y, light.direction.z, 0.0f };
	data.work_data[1] = { light.color.x, light.color.y, light.color.z, as_float(light.attribute) };
	return data;
}

light_data light_data_utility::convert_light_data(const point_lights& light)
{
	light_data data{};
	data.work_data[0] = { light.position.x, light.position.y, light.position.z, light.inv_range };
	data.work_data[1] = { light.color.x, light.color.y, light.color.z, as_float(light.attribute) };
	return data;
}

light_data light_data_utility::convert_light_data(const spot_lights& light)
{
	light_data data{};
	data.work_data[0] = { light.position.x, light.position.y, light.position.z, light.inv_range };
	data.work_data[1] = { light.color.x, light.color.y, light.color.z, as_float(light.attribute) };
	data.work_data[2] = { light.direction.x, light.direction.y, light.direction.z, as_float(light.cone) };
	return data;
}

light_type light_data_utility::get_light_type(const light_data& data)
{
	return get_light_type(as_uint(data.work_data[1].w));
}

math::float3 light_data_utility::get_position(const light_data& data)
{
	switch (get_light_type(data))
	{
	case light_type::directional: return  math::float3(0.0f, 0.0f, 0.0f);
	case light_type::point: return  math::float3(data.work_data[0].x, data.work_data[0].y, data.work_data[0].z);
	case light_type::spot: return  math::float3(data.work_data[0].x, data.work_data[0].y, data.work_data[0].z);
	}
	return math::float3(0, 0, 0);
}

math::float3 light_data_utility::get_direction(const light_data& data)
{
	switch (get_light_type(data))
	{
	case light_type::directional: return  math::float3(data.work_data[0].x, data.work_data[0].y, data.work_data[0].z);
	case light_type::point: return  math::float3(0.0f, 0.0f, 0.0f);
	case light_type::spot: return  math::float3(data.work_data[2].x, data.work_data[2].y, data.work_data[2].z);
	}
	return math::float3(0, 0, 0);
}

math::float4 light_data_utility::get_color(const light_data& data)
{
	switch (get_light_type(data))
	{
	case light_type::directional: return  math::float4(data.work_data[1].x, data.work_data[1].y, data.work_data[1].z, 1.0f);
	case light_type::point: return  math::float4(data.work_data[1].x, data.work_data[1].y, data.work_data[1].z, 1.0f);
	case light_type::spot: return  math::float4(data.work_data[1].x, data.work_data[1].y, data.work_data[1].z, 1.0f);
	}
	return math::float4(0, 0, 0, 0);
}

UINT light_data_utility::make_light_attribute(UINT flags, light_type type, UINT shadow_index)
{
	return (flags & light_attribute_flags_mask)
		| ((shadow_index & light_attribute_shadow_index_mask) << light_attribute_shadow_index_shift)
		| ((static_cast<UINT>(type) & light_attribute_type_mask) << light_attribute_type_shift);
}

light_type light_data_utility::get_light_type(UINT attribute)
{
	return static_cast<light_type>((attribute >> light_attribute_type_shift) & light_attribute_type_mask);
}

UINT light_data_utility::get_shadow_index(UINT attribute)
{
	return (attribute >> light_attribute_shadow_index_shift) & light_attribute_shadow_index_mask;
}

float light_data_utility::convert_inv_range(float range)
{
	return range > 0.0f ? 1.0f / range : std::numeric_limits<float>::infinity();
}

UINT light_data_utility::encode_spot_cone(float inner_cone, float outer_cone)
{
	auto encode_snorm16 = [](float value)
	{
		const float clamped = math::clamp(value, -1.0f, 1.0f);
		const int quantized = static_cast<int>(std::round(clamped * 32767.0f));
		return static_cast<UINT>(quantized) & 0xffffu;
	};
	return encode_snorm16(std::cos(inner_cone)) | (encode_snorm16(std::cos(outer_cone)) << 16);
}

math::float2 light_data_utility::decode_spot_cone(UINT cone)
{
	auto decode_snorm16 = [](UINT value)
	{
		const int signed_value = static_cast<int>(static_cast<int16_t>(value & 0xffffu));
		return math::max(static_cast<float>(signed_value) / 32767.0f, -1.0f);
	};
	return math::float2(decode_snorm16(cone), decode_snorm16(cone >> 16));
}
