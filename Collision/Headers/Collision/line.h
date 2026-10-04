#pragma once

#include "collision.h"

namespace collision
{
	template<typename T>
	class basic_line
	{
	public:
		using element_type = math::basic_element3<T>;
		using vector_type = math::vector3<T>;

		enum class types
		{
			line,
			segment,
		};

	public:
		basic_line();
		basic_line(types type, const element_type& start, const element_type& vector);

		void initialize(types type, const element_type& start, const element_type& vector);

		T calculate_distance(element_type& horizon, T& delta, const element_type& point) const;
		T calculate_distance(element_type& horizon1, T& delta1, element_type& horizon2, T& delta2, const basic_line& line) const;

		bool is_parallel(const basic_line& line) const;

		T get_length() const;
		types get_type() const;
		element_type get_point(T t) const;

	public:
		inline const element_type& operator[](size_t index) const { return	point[index]; }

	private:
		T calculate_line_distance(element_type& horizon, T& delta, const element_type& point) const;
		T calculate_segment_distance(element_type& horizon, T& delta, const element_type& point) const;
		T calculate_line_distance(element_type& horizon1, T& delta1, element_type& horizon2, T& delta2, const basic_line& line) const;
		T calculate_segment_distance(element_type& horizon1, T& delta1, element_type& horizon2, T& delta2, const basic_line& line) const;

		static element_type to_element(const vector_type& v) { return element_type(v.x, v.y, v.z); }

	private:
		types type;
		element_type point[2]; //  [0]：始点, [1]：[直線]正規化済みの方向ベクトル,[線分]終点
	};

	using line = basic_line<double>;
	using linef = basic_line<float>;

	extern template class basic_line<float>;
	extern template class basic_line<double>;
}	//	namespace collision
