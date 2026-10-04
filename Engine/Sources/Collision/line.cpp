#include	"collision/line.h"

using namespace collision;

line::line()
{
}

line::line(types type, const math::float3& start, const math::float3& vector)
{
	initialize( type, start, vector );
}

void line::initialize(types type, const math::float3& start, const math::float3& vector)
{
	this->type = type;
	point[0] = start;
	if (type == types::line)
	{
		point[1] = math::vector3f(vector).normalized().to_float3();
	}
	else
	{
		point[1] = vector;
	}
}

float line::calculate_distance(math::float3& horizon, float& delta, const math::float3& point) const
{
	return (type == types::line) ? calculate_line_distance(horizon, delta, point)
								 : calculate_segment_distance(horizon, delta, point);
}

float line::calculate_distance(math::float3& horizon1, float& delta1, math::float3& horizon2, float& delta2, const line& line) const
{
	return (type == types::line) ? calculate_line_distance(horizon1, delta1, horizon2, delta2, line)
								 : calculate_segment_distance(horizon1, delta1, horizon2, delta2, line);
}

bool line::is_parallel(const line& line) const
{
	math::vector3f PointA[2] = {math::vector3f(point[0]), math::vector3f(point[1])};
	math::vector3f PointB[2] = {math::vector3f(line.point[0]), math::vector3f(line.point[1])};
	//	直線は[1]が方向ベクトル
	math::vector3f LineA = (type == types::line) ? PointA[1] : PointA[1] - PointA[0];
	math::vector3f LineB = (line.type == types::line) ? PointB[1] : PointB[1] - PointB[0];
	LineA = LineA.normalized();
	LineB = LineB.normalized();
	math::vector3f Cross = math::vector3f::cross(LineA, LineB);
	return	Cross.square_length() < FLT_EPSILON;
}

float line::get_length() const
{
	math::vector3f PointA[2] = {math::vector3f(point[0]), math::vector3f(point[1])};
	math::vector3f LineA = PointA[1] - PointA[0];

	return (type == types::line) ? FLT_MAX : LineA.length();
}

line::types line::get_type() const
{
	return	type;
}

math::float3 line::get_point(float t) const
{
	math::vector3f PointA[2] = {math::vector3f(point[0]), math::vector3f(point[1])};
	math::vector3f LineA = PointA[1] - PointA[0];

	math::vector3f Position;
	if (type == types::line)
	{
		Position = PointA[1] * (math::vector3f(t, t, t)) + PointA[0];
	}
	else
	{
		Position = math::lerp(PointA[0], PointA[1], t);
	}
	math::float3 position;
	position = Position.to_float3();
	return position;
}

float line::calculate_line_distance(math::float3& horizon, float& delta, const math::float3& point) const
{
	math::vector3f PointA[2] = {math::vector3f(this->point[0]), math::vector3f(this->point[1])};
	math::vector3f PointB(point);
	float square_length = PointA[1].square_length();
	delta	= 0.0f;
	if (square_length > 0.0f)
	{
		delta = math::vector3f::dot(PointA[1], PointB - PointA[0]);
		delta /= square_length;
	}

	math::vector3f Horizon;
	Horizon = PointA[1] * (math::vector3f(delta, delta, delta)) + PointA[0];
	horizon = Horizon.to_float3();

	return (Horizon - PointB).length();
}

float line::calculate_segment_distance(math::float3& horizon, float& delta, const math::float3& point) const
{
	math::vector3f PointA[2] = {math::vector3f(this->point[0]), math::vector3f(this->point[1])};
	math::vector3f Point(point);
	math::vector3f LineA = PointA[1] - PointA[0];

	//	線分上の媒介変数(0～1)で最近点を算出
	float square_length = LineA.square_length();
	delta = 0.0f;
	if (square_length > FLT_EPSILON)
	{
		delta = math::vector3f::dot(LineA, Point - PointA[0]) / square_length;
		delta = math::saturate(delta);
	}

	math::vector3f Horizon = LineA * (math::vector3f(delta, delta, delta)) + PointA[0];
	horizon = Horizon.to_float3();
	return (Horizon - Point).length();
}

float line::calculate_line_distance(math::float3& horizon1, float& delta1, math::float3& horizon2, float& delta2, const line& line) const
{
	//	2直線が平行？
	if (is_parallel(line))
	{
		//	点P11と直線L2の最短距離の問題に帰着
		float length = line.calculate_line_distance(horizon2, delta2, point[0]);
		horizon1	= point[0];
		delta1	= 0.0f;	//delta2	= 0.0f;

		return	length;
	}

	//	2直線はねじれ関係
	math::vector3f PointA[2] = {math::vector3f(point[0]), math::vector3f(point[1])};
	math::vector3f PointB[2] = {math::vector3f(line.point[0]), math::vector3f(line.point[1])};
	float dotV1_V2 = math::vector3f::dot(PointA[1], PointB[1]);
	float dotV1_V1 = PointA[1].square_length();
	float dotV2_V2 = PointB[1].square_length();
	math::vector3f VectorP21_P11 = PointA[0] - PointB[0];

	float dotB2_P21P11 = math::vector3f::dot(PointB[1], VectorP21_P11);
	float dotB1_P21P11 = math::vector3f::dot(PointA[1], VectorP21_P11);
	delta1 = ( dotV1_V2 * dotB2_P21P11 - dotV2_V2 * dotB1_P21P11 ) / ( dotV1_V1 * dotV2_V2 - dotV1_V2 * dotV1_V2 );
	horizon1 = get_point(delta1);
	math::vector3f Horizon1(horizon1);
	delta2 = math::vector3f::dot(PointB[1], Horizon1 - PointB[0]);
	delta2 /= dotV2_V2;
	horizon2 = line.get_point(delta2);
	math::vector3f Horizon2(horizon2);
	return (Horizon2 - Horizon1).length();
}

float line::calculate_segment_distance(math::float3& horizon1, float& delta1, math::float3& horizon2, float& delta2, const line& line) const
{
	math::vector3f PointA0(point[0]);
	math::vector3f PointA1(point[1]);
	math::vector3f PointB0(line.point[0]);
	math::vector3f PointB1(line.point[1]);
	math::vector3f LineA2_A1 = PointA1 - PointA0;
	math::vector3f LineB2_B1 = PointB1 - PointB0;
	math::vector3f LineA1_B1 = PointA0 - PointB0;

	float square_length1 = LineA2_A1.square_length();
	float square_length2 = LineB2_B1.square_length();

	//	S1が縮退している？
	if (square_length1 <= FLT_EPSILON)
	{
		//	S2も縮退？
		if (square_length2 < FLT_EPSILON)
		{
			//	点と点の距離の問題に帰着
			delta1 = 0.0f;
			delta2 = 0.0f;
			horizon1 = point[0];
			horizon2 = line.point[0];
			math::vector3f Horizon1(horizon1);
			math::vector3f Horizon2(horizon2);
			return (Horizon2 - Horizon1).length();
		}
		else
		{
			//	S1の始点とS2の最短問題に帰着
			float dotLB2B1_LA1B1 = math::vector3f::dot(LineB2_B1, LineA1_B1);
			delta1 = 0.0f;
			delta2 = math::saturate(dotLB2B1_LA1B1 / square_length2);
		}
	}
	else
	{
		float dotLA2A1_LA1B1 = math::vector3f::dot(LineA2_A1, LineA1_B1);
		if (square_length2 <= FLT_EPSILON)
		{
			//	S2が縮退している場合は
			//	S2の始点とS1の最短問題に帰着
			delta2 = 0.0f;
			delta1 = math::saturate(-dotLA2A1_LA1B1 / square_length1);
		}
		else
		{
			float dotLB2B1_LA1B1 = math::vector3f::dot(LineB2_B1, LineA1_B1);
			float dotLA2A1_LB2B1 = math::vector3f::dot(LineA2_A1, LineB2_B1);
			float de_normal = square_length1 * square_length2 - dotLA2A1_LB2B1 * dotLA2A1_LB2B1;
			if (de_normal != 0.0f)
			{
				delta1 = math::saturate((dotLA2A1_LB2B1 * dotLB2B1_LA1B1 - dotLA2A1_LA1B1 * square_length2) / de_normal);
			}
			else
			{
				delta1 = 0.0f;
			}

			delta2 = (dotLA2A1_LB2B1 * delta1 + dotLB2B1_LA1B1) / square_length2;
			
			//	S2側が外の場合
			if (delta2 < 0.0f)
			{
				delta2 = 0.0f;
				delta1 = math::saturate(-dotLA2A1_LA1B1 / square_length1);
			}
			else if (delta2 > 1.0f)
			{
				delta2 = 1.0f;
				delta1 = math::saturate((dotLA2A1_LB2B1 - dotLA2A1_LA1B1) / square_length1);
			}
		}
	}

	math::vector3f Horizon1 = LineA2_A1 * (math::vector3f(delta1, delta1, delta1)) + PointA0;
	math::vector3f Horizon2 = LineB2_B1 * (math::vector3f(delta2, delta2, delta2)) + PointB0;
	horizon1 = Horizon1.to_float3();
	horizon2 = Horizon2.to_float3();
	return (Horizon2 - Horizon1).length();
}

