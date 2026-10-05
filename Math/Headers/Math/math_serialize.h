#pragma once

#include "cereal/cereal.hpp"

#include "Math/math_all.h"

namespace math
{
	template<class Archive, typename T>
	void serialize(Archive& archive, element2<T>& v)
	{
		archive(
			cereal::make_nvp("x", v.x),
			cereal::make_nvp("y", v.y)
		);
	}

	template<class Archive, typename T>
	void serialize(Archive& archive, element3<T>& v)
	{
		archive(
			cereal::make_nvp("x", v.x),
			cereal::make_nvp("y", v.y),
			cereal::make_nvp("z", v.z)
		);
	}

	template<class Archive, typename T>
	void serialize(Archive& archive, element4<T>& v)
	{
		archive(
			cereal::make_nvp("x", v.x),
			cereal::make_nvp("y", v.y),
			cereal::make_nvp("z", v.z),
			cereal::make_nvp("w", v.w)
		);
	}

	template<class Archive, typename T>
	void serialize(Archive& archive, element_matrix<T, 4, 4>& m)
	{
		auto& e = m.elements;
		archive(
			cereal::make_nvp("_11", e[0]), cereal::make_nvp("_12", e[1]), cereal::make_nvp("_13", e[2]), cereal::make_nvp("_14", e[3]),
			cereal::make_nvp("_21", e[4]), cereal::make_nvp("_22", e[5]), cereal::make_nvp("_23", e[6]), cereal::make_nvp("_24", e[7]),
			cereal::make_nvp("_31", e[8]), cereal::make_nvp("_32", e[9]), cereal::make_nvp("_33", e[10]), cereal::make_nvp("_34", e[11]),
			cereal::make_nvp("_41", e[12]), cereal::make_nvp("_42", e[13]), cereal::make_nvp("_43", e[14]), cereal::make_nvp("_44", e[15])
		);
	}

	template<class Archive, typename T>
	void serialize(Archive& archive, vector2<T>& v)
	{
		archive(
			cereal::make_nvp("x", v.x),
			cereal::make_nvp("y", v.y)
		);
	}

	template<class Archive, typename T>
	void serialize(Archive& archive, vector3<T>& v)
	{
		archive(
			cereal::make_nvp("x", v.x),
			cereal::make_nvp("y", v.y),
			cereal::make_nvp("z", v.z)
		);
	}

	template<class Archive, typename T>
	void serialize(Archive& archive, vector4<T>& v)
	{
		archive(
			cereal::make_nvp("x", v.x),
			cereal::make_nvp("y", v.y),
			cereal::make_nvp("z", v.z),
			cereal::make_nvp("w", v.w)
		);
	}

	template<class Archive, typename T>
	void serialize(Archive& archive, quaternion<T>& q)
	{
		archive(
			cereal::make_nvp("x", q.x),
			cereal::make_nvp("y", q.y),
			cereal::make_nvp("z", q.z),
			cereal::make_nvp("w", q.w)
		);
	}

	template<class Archive, typename T>
	void serialize(Archive& archive, matrix<T>& m)
	{
		archive(
			cereal::make_nvp("_11", m.s11), cereal::make_nvp("_12", m.s12), cereal::make_nvp("_13", m.s13), cereal::make_nvp("_14", m.s14),
			cereal::make_nvp("_21", m.s21), cereal::make_nvp("_22", m.s22), cereal::make_nvp("_23", m.s23), cereal::make_nvp("_24", m.s24),
			cereal::make_nvp("_31", m.s31), cereal::make_nvp("_32", m.s32), cereal::make_nvp("_33", m.s33), cereal::make_nvp("_34", m.s34),
			cereal::make_nvp("_41", m.s41), cereal::make_nvp("_42", m.s42), cereal::make_nvp("_43", m.s43), cereal::make_nvp("_44", m.s44)
		);
	}

	template<class Archive>
	void serialize(Archive& archive, rotatorf& v)
	{
		archive(
			cereal::make_nvp("x", v.x),
			cereal::make_nvp("y", v.y),
			cereal::make_nvp("z", v.z)
		);
	}
}
