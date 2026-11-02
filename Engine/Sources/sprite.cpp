#include "sprite.h"

#include <sstream>

sprite::sprite()
{
	HRESULT hr{ S_OK };

	positions.push_back({ 0.0f, 0.0f, 0.0f });
	positions.push_back({ 1.0f, 0.0f, 0.0f });
	positions.push_back({ 0.0f, 1.0f, 0.0f });
	positions.push_back({ 1.0f, 1.0f, 0.0f });
	colors.push_back({ 1.0f, 1.0f, 1.0f, 1.0f });
	colors.push_back({ 1.0f, 1.0f, 1.0f, 1.0f });
	colors.push_back({ 1.0f, 1.0f, 1.0f, 1.0f });
	colors.push_back({ 1.0f, 1.0f, 1.0f, 1.0f });
	texcoords.push_back({ 0.0f, 0.0f });
	texcoords.push_back({ 1.0f, 0.0f });
	texcoords.push_back({ 0.0f, 1.0f });
	texcoords.push_back({ 1.0f, 1.0f });
	indices.emplace_back(0);
	indices.emplace_back(1);
	indices.emplace_back(2);
	indices.emplace_back(1);
	indices.emplace_back(3);
	indices.emplace_back(2);
	transform_matrix = math::float4x4();

	material.set_domain(material::domains::user_interface);
	material.set_blend_mode(material::blend_modes::translucent);
	material.set_shader_path(::material::default_user_interface_shader_path);

	material.set_texture(base_texture_name, texture_manager::instance()->search(texture_manager::white_texture2d_name));
}

sprite::~sprite()
{
}

void sprite::update_vertices(const std::vector<vertex>& vertices, const std::vector<UINT>& indices)
{
	this->positions.clear();
	this->colors.clear();
	this->texcoords.clear();
	for (auto& vertex : vertices)
	{
		this->positions.push_back(vertex.position);
		this->colors.push_back(vertex.color);
		this->texcoords.push_back(vertex.texcoord);
	}
	this->indices = indices;
}

void sprite::update_transform(float dx, float dy, float dw, float dh,
							  float r, float g, float b, float a,
							  float angle, float pivot_x, float pivot_y,
							  float sx, float sy, float sw, float sh)
{
	float sin_angle, cos_angle;
	math::angle::sin_cos(angle, sin_angle, cos_angle);

	transform_matrix._11 = dx;
	transform_matrix._12 = dy;
	transform_matrix._13 = dw;
	transform_matrix._14 = dh;

	transform_matrix._21 = cos_angle;
	transform_matrix._22 = sin_angle;
	transform_matrix._23 = pivot_x;
	transform_matrix._24 = pivot_y;

	transform_matrix._31 = sx;
	transform_matrix._32 = sy;
	transform_matrix._33 = sw;
	transform_matrix._34 = sh;

	transform_matrix._41 = r;
	transform_matrix._42 = g;
	transform_matrix._43 = b;
	transform_matrix._44 = a;
}
