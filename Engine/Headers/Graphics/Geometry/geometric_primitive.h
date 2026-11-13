#pragma once

#include <d3d11.h>
#include <wrl.h>


#include "Graphics/Core/buffer_view.h"

class geometric_primitive
{
protected:
	struct vertex
	{
		math::float3 position;
		math::float2 texcoord;
	};

public:
	const std::unordered_map<semantic, buffer_view>& get_vertex_buffer_views() const {return vertex_buffer_views;}
	const buffer_view& get_index_buffer_view() const {return index_buffer_view;}

public:
	virtual ~geometric_primitive() = default;

protected:
	geometric_primitive(ID3D11Device* device);
	void create_com_buffers(ID3D11Device* device, math::float3* positions, math::float2* texcoords, size_t vertex_count, uint32_t* indices, size_t index_count);

private:
	std::unordered_map<semantic, buffer_view> vertex_buffer_views;
	buffer_view index_buffer_view;
	std::vector<math::float4x4> transform_matrices;
};

class geometric_plane : public geometric_primitive
{
public:
	geometric_plane(ID3D11Device* device, bool x_flip = false);
};

class geometric_cube : public geometric_primitive
{
public:
	geometric_cube(ID3D11Device* device);
};

class geometric_sphere : public geometric_primitive
{
public:
	geometric_sphere(ID3D11Device* device, uint32_t slices, uint32_t stacks);
};

