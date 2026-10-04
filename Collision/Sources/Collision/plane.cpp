#include	"collision/plane.h"

math::float3 collision::plane::get_plane_position() const
{
	//	n・p + w = 0 を満たす点
	return math::float3(-parameter.x * parameter.w, -parameter.y * parameter.w, -parameter.z * parameter.w);
}

math::float3 collision::plane::get_normal() const
{
	return math::float3(parameter.x, parameter.y, parameter.z);
}

float collision::plane::get_distance() const
{
	return parameter.w;
}

const math::float4& collision::plane::get_parameter() const
{
	return parameter;
}

void collision::plane::set_parameter(const math::float4& parameter)
{
	this->parameter = parameter;
}

collision::plane::plane(const math::float4& parameter)
{
	initialize(parameter);
}

collision::plane::plane(const math::float3& normal, float distance)
{
	initialize(normal, distance);
}

void collision::plane::initialize(const math::float4& parameter)
{
	this->parameter = parameter;
}

void collision::plane::initialize(const math::float3& normal, float distance)
{
	this->parameter.x = normal.x;
	this->parameter.y = normal.y;
	this->parameter.z = normal.z;
	this->parameter.w = distance;
}

void collision::plane::normlize()
{
	math::vector4f Parameter(parameter);
	float Length = Parameter.length();
	//	法線が無い平面は常に内側扱い
	if (Length <= FLT_EPSILON)
	{
		parameter = math::float4(0.0f, 0.0f, 0.0f, parameter.w >= 0.0f ? 1.0f : -1.0f);
		return;
	}
	parameter = (Parameter / Length).to_float4();
}

float collision::plane::calculate_distance(const math::float3& point) const
{
	return	parameter.x * point.x + parameter.y * point.y + parameter.z * point.z + parameter.w;
}

bool collision::plane::to_line(const line& line, float* distance, math::float3* insert_point) const
{
	math::float3 plane_position = get_plane_position();
	math::vector3f PlanePosition(plane_position);
	math::float3 line_0_position = line[0];
	math::float3 line_1_position = line[1];
	math::vector3f Line0Position(line_0_position);
	math::vector3f Line1Position(line_1_position);
	math::float3 normal = get_normal();
	math::vector3f Normal(normal);
	if (line.get_type() == line::types::segment)
	{
		math::vector3f V1 = Line0Position - PlanePosition;
		math::vector3f V2 = Line1Position - PlanePosition;
		float dotV1_Normal = math::vector3f::dot(V1, Normal);
		float dotV2_Normal = math::vector3f::dot(V2, Normal);
		if (fabsf(dotV1_Normal - dotV2_Normal) <= FLT_EPSILON)
		{
			//	平行時は平面上にある場合のみ交差
			if (fabsf(dotV1_Normal) > FLT_EPSILON)
				return false;
			if (distance)
				*distance = 0.0f;
			if (insert_point)
				*insert_point = Line0Position.to_float3();
			return true;
		}
		else if ((dotV1_Normal >= 0.0 && dotV2_Normal <= 0.0) || (dotV1_Normal <= 0.0 && dotV2_Normal >= 0.0))
		{
			float length = fabsf(dotV1_Normal) / (fabsf(dotV1_Normal) + fabsf(dotV2_Normal));
			math::vector3f Segment = Line1Position - Line0Position;
			if (distance)
				*distance = length * Segment.length();
			if (insert_point)
				*insert_point = (Line0Position + (Segment * length)).to_float3();
			return true;
		}
		//	交差していない
		return false;
	}
	else
	{
		math::vector3f V1 = PlanePosition - Line0Position;
		math::vector3f V2 = Line1Position - PlanePosition;
		float dotV1_Normal = math::vector3f::dot(V1, Normal);
		if (fabsf(dotV1_Normal) < FLT_EPSILON)
		{
			if (distance)
				*distance	= 0.0f;
			if (insert_point)
				*insert_point = Line0Position.to_float3();
			return true;
		}
		float dotLine2_Normal = math::vector3f::dot(Line1Position, Normal);
		if (dotLine2_Normal != 0.0f)
		{
			float length = dotV1_Normal / dotLine2_Normal;
			if (distance)
				*distance = length;
			if (insert_point)
				*insert_point = (Line0Position + (Line1Position * length)).to_float3();
			return true;
		}
	}
	return false;
}


