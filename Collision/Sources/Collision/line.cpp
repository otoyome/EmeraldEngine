#include	"collision/line.h"

using namespace collision;

template<typename T>
line<T>::line()
{
}

template<typename T>
line<T>::line(types type, const element_type& start, const element_type& vector)
{
	initialize( type, start, vector );
}

template<typename T>
void line<T>::initialize(types type, const element_type& start, const element_type& vector)
{
	this->type = type;
	point[0] = start;
	if (type == types::line)
	{
		point[1] = to_element(vector_type(vector).normalized());
	}
	else
	{
		point[1] = vector;
	}
}

template<typename T>
T line<T>::calculate_distance(element_type& horizon, T& delta, const element_type& point) const
{
	return (type == types::line) ? calculate_line_distance(horizon, delta, point)
								 : calculate_segment_distance(horizon, delta, point);
}

template<typename T>
T line<T>::calculate_distance(element_type& horizon1, T& delta1, element_type& horizon2, T& delta2, const line& line) const
{
	return (type == types::line) ? calculate_line_distance(horizon1, delta1, horizon2, delta2, line)
								 : calculate_segment_distance(horizon1, delta1, horizon2, delta2, line);
}

template<typename T>
bool line<T>::is_parallel(const line& line) const
{
	vector_type PointA[2] = {vector_type(point[0]), vector_type(point[1])};
	vector_type PointB[2] = {vector_type(line.point[0]), vector_type(line.point[1])};
	//	直線は[1]が方向ベクトル
	vector_type LineA = (type == types::line) ? PointA[1] : PointA[1] - PointA[0];
	vector_type LineB = (line.type == types::line) ? PointB[1] : PointB[1] - PointB[0];
	LineA = LineA.normalized();
	LineB = LineB.normalized();
	vector_type Cross = vector_type::cross(LineA, LineB);
	return	Cross.square_length() < FLT_EPSILON;
}

template<typename T>
T line<T>::get_length() const
{
	vector_type PointA[2] = {vector_type(point[0]), vector_type(point[1])};
	vector_type LineA = PointA[1] - PointA[0];

	return (type == types::line) ? FLT_MAX : LineA.length();
}

template<typename T>
typename line<T>::types line<T>::get_type() const
{
	return	type;
}

template<typename T>
typename line<T>::element_type line<T>::get_point(T t) const
{
	vector_type PointA[2] = {vector_type(point[0]), vector_type(point[1])};
	vector_type LineA = PointA[1] - PointA[0];

	vector_type Position;
	if (type == types::line)
	{
		Position = PointA[1] * (vector_type(t, t, t)) + PointA[0];
	}
	else
	{
		Position = math::lerp(PointA[0], PointA[1], t);
	}
	element_type position;
	position = to_element(Position);
	return position;
}

template<typename T>
T line<T>::calculate_line_distance(element_type& horizon, T& delta, const element_type& point) const
{
	vector_type PointA[2] = {vector_type(this->point[0]), vector_type(this->point[1])};
	vector_type PointB(point);
	T square_length = PointA[1].square_length();
	delta	= 0.0f;
	if (square_length > 0.0f)
	{
		delta = vector_type::dot(PointA[1], PointB - PointA[0]);
		delta /= square_length;
	}

	vector_type Horizon;
	Horizon = PointA[1] * (vector_type(delta, delta, delta)) + PointA[0];
	horizon = to_element(Horizon);

	return (Horizon - PointB).length();
}

template<typename T>
T line<T>::calculate_segment_distance(element_type& horizon, T& delta, const element_type& point) const
{
	vector_type PointA[2] = {vector_type(this->point[0]), vector_type(this->point[1])};
	vector_type Point(point);
	vector_type LineA = PointA[1] - PointA[0];

	//	線分上の媒介変数(0～1)で最近点を算出
	T square_length = LineA.square_length();
	delta = 0.0f;
	if (square_length > FLT_EPSILON)
	{
		delta = vector_type::dot(LineA, Point - PointA[0]) / square_length;
		delta = math::saturate(delta);
	}

	vector_type Horizon = LineA * (vector_type(delta, delta, delta)) + PointA[0];
	horizon = to_element(Horizon);
	return (Horizon - Point).length();
}

template<typename T>
T line<T>::calculate_line_distance(element_type& horizon1, T& delta1, element_type& horizon2, T& delta2, const line& line) const
{
	//	2直線が平行？
	if (is_parallel(line))
	{
		//	点P11と直線L2の最短距離の問題に帰着
		T length = line.calculate_line_distance(horizon2, delta2, point[0]);
		horizon1	= point[0];
		delta1	= 0.0f;	//delta2	= 0.0f;

		return	length;
	}

	//	2直線はねじれ関係
	vector_type PointA[2] = {vector_type(point[0]), vector_type(point[1])};
	vector_type PointB[2] = {vector_type(line.point[0]), vector_type(line.point[1])};
	T dotV1_V2 = vector_type::dot(PointA[1], PointB[1]);
	T dotV1_V1 = PointA[1].square_length();
	T dotV2_V2 = PointB[1].square_length();
	vector_type VectorP21_P11 = PointA[0] - PointB[0];

	T dotB2_P21P11 = vector_type::dot(PointB[1], VectorP21_P11);
	T dotB1_P21P11 = vector_type::dot(PointA[1], VectorP21_P11);
	delta1 = ( dotV1_V2 * dotB2_P21P11 - dotV2_V2 * dotB1_P21P11 ) / ( dotV1_V1 * dotV2_V2 - dotV1_V2 * dotV1_V2 );
	horizon1 = get_point(delta1);
	vector_type Horizon1(horizon1);
	delta2 = vector_type::dot(PointB[1], Horizon1 - PointB[0]);
	delta2 /= dotV2_V2;
	horizon2 = line.get_point(delta2);
	vector_type Horizon2(horizon2);
	return (Horizon2 - Horizon1).length();
}

template<typename T>
T line<T>::calculate_segment_distance(element_type& horizon1, T& delta1, element_type& horizon2, T& delta2, const line& line) const
{
	vector_type PointA0(point[0]);
	vector_type PointA1(point[1]);
	vector_type PointB0(line.point[0]);
	vector_type PointB1(line.point[1]);
	vector_type LineA2_A1 = PointA1 - PointA0;
	vector_type LineB2_B1 = PointB1 - PointB0;
	vector_type LineA1_B1 = PointA0 - PointB0;

	T square_length1 = LineA2_A1.square_length();
	T square_length2 = LineB2_B1.square_length();

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
			vector_type Horizon1(horizon1);
			vector_type Horizon2(horizon2);
			return (Horizon2 - Horizon1).length();
		}
		else
		{
			//	S1の始点とS2の最短問題に帰着
			T dotLB2B1_LA1B1 = vector_type::dot(LineB2_B1, LineA1_B1);
			delta1 = 0.0f;
			delta2 = math::saturate(dotLB2B1_LA1B1 / square_length2);
		}
	}
	else
	{
		T dotLA2A1_LA1B1 = vector_type::dot(LineA2_A1, LineA1_B1);
		if (square_length2 <= FLT_EPSILON)
		{
			//	S2が縮退している場合は
			//	S2の始点とS1の最短問題に帰着
			delta2 = 0.0f;
			delta1 = math::saturate(-dotLA2A1_LA1B1 / square_length1);
		}
		else
		{
			T dotLB2B1_LA1B1 = vector_type::dot(LineB2_B1, LineA1_B1);
			T dotLA2A1_LB2B1 = vector_type::dot(LineA2_A1, LineB2_B1);
			T de_normal = square_length1 * square_length2 - dotLA2A1_LB2B1 * dotLA2A1_LB2B1;
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

	vector_type Horizon1 = LineA2_A1 * (vector_type(delta1, delta1, delta1)) + PointA0;
	vector_type Horizon2 = LineB2_B1 * (vector_type(delta2, delta2, delta2)) + PointB0;
	horizon1 = to_element(Horizon1);
	horizon2 = to_element(Horizon2);
	return (Horizon2 - Horizon1).length();
}

template class collision::line<float>;
template class collision::line<double>;
