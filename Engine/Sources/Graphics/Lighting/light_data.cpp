#include "Graphics/Lighting/light_data.h"

#include "Graphics/Core/render_data.h"

#include <limits>

light_type light_data_utility::get_light_type(const light_data& data)
{
	return get_light_type(data.attribute);
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
