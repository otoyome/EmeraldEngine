#pragma once

#include "collision.h"

namespace collision
{
	class line
	{
	public:
		enum class types
		{
			line,
			segment,
		};

	public:
		line();
		line(types type, const math::float3& start, const math::float3& vector);

		void initialize(types type, const math::float3& start, const math::float3& vector);

		float calculate_distance(math::float3& horizon, float& delta, const math::float3& point) const;
		float calculate_distance(math::float3& horizon1, float& delta1, math::float3& horizon2, float& delta2, const line& line) const;

		bool is_parallel(const line& line) const;

		float get_length() const;
		types get_type() const;
		math::float3 get_point(float t) const;

	public:
		inline const math::float3& operator[](size_t index) const { return	point[index]; }

	private:
		float calculate_line_distance(math::float3& horizon, float& delta, const math::float3& point) const;
		float calculate_segment_distance(math::float3& horizon, float& delta, const math::float3& point) const;
		float calculate_line_distance(math::float3& horizon1, float& delta1, math::float3& horizon2, float& delta2, const line& line) const;
		float calculate_segment_distance(math::float3& horizon1, float& delta1, math::float3& horizon2, float& delta2, const line& line) const;

	private:
		types type;
		math::float3 point[2]; //  [0]：始点, [1]：[直線]正規化済みの方向ベクトル,[線分]終点
	};
}	//	namespace collision


