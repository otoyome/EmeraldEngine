#pragma once

#include "Math/math_all.h"

template<typename T>
class transform
{
	enum class flags
	{
		dirty,	//	再計算フラグ
	};

	using vector3 = math::vector3<T>;
	using quaternion = math::quaternion<T>;
	using matrix = math::matrix<T>;

public:
	bool is_flag(flags flag) const;
	void set_flag(flags flag, bool value);

	const vector3& get_scale() const;
	void set_scale(const vector3& scale);
	const quaternion& get_rotation() const;
	void set_rotation(const quaternion& rotation);
	const vector3& get_translation() const;
	void set_translation(const vector3& translation);

	const matrix& get_transform() const;

public:
	transform();

	inline vector3 transform_point(const vector3& point) const
	{
		update();
		return point * srt;
	}
	inline vector3 transform_vector(const vector3& vector) const
	{
		update();
		return vector * sr;
	}
	inline vector3 transform_direction(const vector3& direction) const
	{
		return direction * math::matrixd::rotation_quaternion(rotation);
	}

	inline vector3 inverse_transform_point(const vector3& point) const
	{
		update();
		return point * inverse_srt;
	}

private:
	void update();

private:
	mutable UINT system_flags;	//	フラグ類
	mutable matrix srt;			//	SRT行列
	mutable matrix inverse_srt;	//	逆SRT行列
	mutable matrix sr;			//	SR行列

	vector3 scale;				//	拡縮率
	quaternion rotation;		//	回転情報
	vector3 translation;		//	座標情報
};
