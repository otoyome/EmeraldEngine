#include "Math/rigid_transform.h"
#include "Math/math_serialize.h"

#include "cereal/archives/binary.hpp"
#include "cereal/archives/json.hpp"

namespace math
{
	template<typename T>
	rigid_transform<T> rigid_transform<T>::identity()
	{
		return rigid_transform();
	}

	template<typename T>
	rigid_transform<T> rigid_transform<T>::from_matrix(const matrix& m)
	{
		vector3 out_scale;
		quaternion out_rotation;
		vector3 out_translation;
		if (!m.decompose(out_scale, out_rotation, out_translation))
		{
			return identity();
		}
		return rigid_transform(out_rotation, out_translation);
	}

	template<typename T>
	rigid_transform<T> rigid_transform<T>::blend(const rigid_transform& l, const rigid_transform& r, T value)
	{
		return rigid_transform(math::slerp(l.rotation, r.rotation, value),
							   math::lerp(l.translation, r.translation, value));
	}

	template<typename T>
	const typename rigid_transform<T>::quaternion& rigid_transform<T>::get_rotation() const
	{
		return rotation;
	}

	template<typename T>
	void rigid_transform<T>::set_rotation(const quaternion& rotation)
	{
		this->rotation = rotation;
	}

	template<typename T>
	const typename rigid_transform<T>::vector3& rigid_transform<T>::get_translation() const
	{
		return translation;
	}

	template<typename T>
	void rigid_transform<T>::set_translation(const vector3& translation)
	{
		this->translation = translation;
	}

	template<typename T>
	typename rigid_transform<T>::matrix rigid_transform<T>::get_transform() const
	{
		return matrix(rotation, translation);
	}

	template<typename T>
	typename rigid_transform<T>::matrix rigid_transform<T>::get_inverse_transform() const
	{
		return matrix::inverse_rigid_transformation(rotation, translation);
	}

	template<typename T>
	typename rigid_transform<T>::vector3 rigid_transform<T>::get_right() const
	{
		return rotation.right();
	}

	template<typename T>
	typename rigid_transform<T>::vector3 rigid_transform<T>::get_up() const
	{
		return rotation.up();
	}

	template<typename T>
	typename rigid_transform<T>::vector3 rigid_transform<T>::get_front() const
	{
		return rotation.front();
	}

	template<typename T>
	math::rotator<T> rigid_transform<T>::get_euler() const
	{
		return math::rotator<T>(rotation);
	}

	template<typename T>
	void rigid_transform<T>::set_euler(const math::rotator<T>& euler)
	{
		set_rotation(euler.to_quaternion());
	}

	template<typename T>
	rigid_transform<T>::rigid_transform()
	: rotation(quaternion::identity())
	, translation(vector3::zero())
	{
	}

	template<typename T>
	rigid_transform<T>::rigid_transform(const quaternion& rotation, const vector3& translation)
	: rotation(rotation)
	, translation(translation)
	{
	}

	template<typename T>
	rigid_transform<T>::rigid_transform(const rigid_transform<other_value_type>& v)
	: rotation(v.get_rotation())
	, translation(v.get_translation())
	{
	}

	template<typename T>
	typename rigid_transform<T>::vector3 rigid_transform<T>::transform_point(const vector3& point) const
	{
		return rotation.rotate(point) + translation;
	}

	template<typename T>
	typename rigid_transform<T>::vector3 rigid_transform<T>::transform_direction(const vector3& direction) const
	{
		return rotation.rotate(direction);
	}

	template<typename T>
	typename rigid_transform<T>::vector3 rigid_transform<T>::inverse_transform_point(const vector3& point) const
	{
		//	先に座標の差分を取ることで、原点から遠い位置での桁落ちを避ける
		return inverse_transform_direction(point - translation);
	}

	template<typename T>
	typename rigid_transform<T>::vector3 rigid_transform<T>::inverse_transform_direction(const vector3& direction) const
	{
		return rotation.inverted().rotate(direction);
	}

	template<typename T>
	void rigid_transform<T>::translate(const vector3& offset)
	{
		set_translation(translation + offset);
	}

	template<typename T>
	void rigid_transform<T>::rotate(const quaternion& rotation)
	{
		set_rotation(quaternion::concatenate(this->rotation, rotation));
	}

	template<typename T>
	rigid_transform<T> rigid_transform<T>::multiply(const rigid_transform& parent) const
	{
		return rigid_transform(quaternion::concatenate(rotation, parent.rotation),
							   parent.rotation.rotate(translation) + parent.translation);
	}

	template<typename T>
	rigid_transform<T> rigid_transform<T>::inverse() const
	{
		const quaternion inverse_rotation = rotation.inverted();
		return rigid_transform(inverse_rotation, inverse_rotation.rotate(-translation));
	}

	template<typename T>
	rigid_transform<T> rigid_transform<T>::relative_to(const rigid_transform& parent) const
	{
		//	this->multiply(parent.inverse()) と同じ結果だが、座標の差分を先に取って精度を保つ
		const quaternion inverse_parent_rotation = parent.rotation.inverted();
		return rigid_transform(quaternion::concatenate(rotation, inverse_parent_rotation),
							   inverse_parent_rotation.rotate(translation - parent.translation));
	}

	template<typename T>
	math::matrixf rigid_transform<T>::to_relative_matrix(const vector3& origin) const
	{
		return math::matrixf(math::quaternionf(rotation), math::vector3f(translation - origin));
	}

	template<typename T>
	bool rigid_transform<T>::equal(const rigid_transform& v, T tolerance) const
	{
		return math::nearly_equal(translation, v.translation, tolerance)
			&& std::abs(rotation.normalized().dot(v.rotation.normalized())) >= static_cast<T>(1) - tolerance;
	}

	template<typename T>
	rigid_transform<T> rigid_transform<T>::operator*(const rigid_transform& parent) const
	{
		return multiply(parent);
	}

	template<typename T>
	rigid_transform<T>& rigid_transform<T>::operator*=(const rigid_transform& parent)
	{
		*this = multiply(parent);
		return *this;
	}

	template<typename T>
	bool rigid_transform<T>::operator==(const rigid_transform& v) const
	{
		return rotation == v.rotation && translation == v.translation;
	}

	template<typename T>
	bool rigid_transform<T>::operator!=(const rigid_transform& v) const
	{
		return !(*this == v);
	}

	template<typename T>
	template<class Archive>
	void rigid_transform<T>::serialize(Archive& archive, int version)
	{
		archive(
			cereal::make_nvp("rotation", rotation),
			cereal::make_nvp("translation", translation)
		);
	}

	template class rigid_transform<float>;
	template class rigid_transform<double>;

	//	serialize は cpp で定義しているため、使用するアーカイブごとに明示的に実体化する
	template void rigid_transform<float>::serialize<cereal::BinaryInputArchive>(cereal::BinaryInputArchive&, int);
	template void rigid_transform<float>::serialize<cereal::BinaryOutputArchive>(cereal::BinaryOutputArchive&, int);
	template void rigid_transform<float>::serialize<cereal::JSONInputArchive>(cereal::JSONInputArchive&, int);
	template void rigid_transform<float>::serialize<cereal::JSONOutputArchive>(cereal::JSONOutputArchive&, int);
	template void rigid_transform<double>::serialize<cereal::BinaryInputArchive>(cereal::BinaryInputArchive&, int);
	template void rigid_transform<double>::serialize<cereal::BinaryOutputArchive>(cereal::BinaryOutputArchive&, int);
	template void rigid_transform<double>::serialize<cereal::JSONInputArchive>(cereal::JSONInputArchive&, int);
	template void rigid_transform<double>::serialize<cereal::JSONOutputArchive>(cereal::JSONOutputArchive&, int);

}	//	namespace math
