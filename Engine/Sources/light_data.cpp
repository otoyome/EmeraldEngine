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
	light.direction = data.work_data[0];
	light.color = data.work_data[1];
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

light_probes light_data_utility::convert_light_probes(const light_data& data)
{
	light_probes light_probe;
	light_probe.position = data.work_data[0];
	light_probe.color = data.work_data[1];
	light_probe.inner_area = data.work_data[2];
	light_probe.outer_area = data.work_data[3];
	return light_probe;
}

light_data light_data_utility::convert_light_data(const directional_lights& light)
{
	light_data data;
	data.work_data[0] = light.direction;
	data.work_data[1] = light.color;
	data.work_data[3].w = static_cast<int>(light_type::directional);
	return data;
}

light_data light_data_utility::convert_light_data(const point_lights& light)
{
	light_data data{};
	data.work_data[0] = { light.position.x, light.position.y, light.position.z, light.inv_range };
	data.work_data[1] = { light.color.x, light.color.y, light.color.z, as_float(light.attribute) };
	data.work_data[3].w = static_cast<int>(light_type::point);
	return data;
}

light_data light_data_utility::convert_light_data(const spot_lights& light)
{
	light_data data{};
	data.work_data[0] = { light.position.x, light.position.y, light.position.z, light.inv_range };
	data.work_data[1] = { light.color.x, light.color.y, light.color.z, as_float(light.attribute) };
	data.work_data[2] = { light.direction.x, light.direction.y, light.direction.z, as_float(light.cone) };
	data.work_data[3].w = static_cast<int>(light_type::spot);
	return data;
}

light_data light_data_utility::convert_light_data(const light_probes& light_probe)
{
	light_data data;
	data.work_data[0] = light_probe.position;
	data.work_data[1] = light_probe.color;
	data.work_data[2] = light_probe.inner_area;
	data.work_data[3] = light_probe.outer_area;
	data.work_data[3].w = static_cast<int>(light_type::light_probe);
	return data;
}

light_type light_data_utility::get_light_type(const light_data& data)
{
	return static_cast<light_type>(data.work_data[3].w + 0.5f);
}

math::float3 light_data_utility::get_position(const light_data& data)
{
	switch (get_light_type(data))
	{
	case light_type::directional: return  math::float3(data.work_data[2].x, data.work_data[2].y, data.work_data[2].z);
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
	case light_type::directional: return  data.work_data[1];
	case light_type::point: return  math::float4(data.work_data[1].x, data.work_data[1].y, data.work_data[1].z, 1.0f);
	case light_type::spot: return  math::float4(data.work_data[1].x, data.work_data[1].y, data.work_data[1].z, 1.0f);
	}
	return math::float4(0, 0, 0, 0);
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
