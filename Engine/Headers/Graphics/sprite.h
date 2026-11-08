#pragma once

#define	NOMINMAX
#include "Texture/texture.h"
#include "Material/material_instance.h"

class sprite
{
public:
	static constexpr std::string_view base_texture_name = "color_map";

public:
	const std::vector<math::float3>& get_positions() const { return positions; }
	const std::vector<math::float4>& get_colors() const { return colors; }
	const std::vector<math::float2>& get_texcoords() const { return texcoords; }
	const std::vector<UINT>& get_indices() const {return indices;}
	math::float4x4 get_transform_matrix() const { return transform_matrix; }
	material_instance& get_material() {return material;}
	const material_instance& get_material() const {return material;}

public:
	sprite();
	~sprite();

	struct vertex
	{
		math::float3 position;
		math::float4 color;
		math::float2 texcoord;
	};
	void update_vertices(const std::vector<vertex>& vertices, const std::vector<UINT>& indices);

	void update_transform(
		float dx, float dy, float dw, float dh,
		float r, float g, float b, float a,
		float angle, float pivot_x, float pivot_y,
		float sx, float sy, float sw, float sh);

private:
	std::vector<math::float3> positions;
	std::vector<math::float4> colors;
	std::vector<math::float2> texcoords;
	std::vector<UINT> indices;
	math::float4x4 transform_matrix;
	material_instance material;

private:
	static material_resource_ptr acquire_default_material();
	inline static std::weak_ptr<::material_resource> default_material;
};
