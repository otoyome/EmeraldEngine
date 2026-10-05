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
	using rotator = math::rotator<T>;

public:
	bool is_flag(flags flag) const;
	void set_flag(flags flag, bool value);

	const vector3& get_scale() const {return scale;}
	void set_scale(vector3& scale) {this->scale = scale;}
	const rotator& get_rotation() const {return rotation;}
	void set_rotation(rotator& rotation) {this->rotation = rotation;}
	const vector3& get_translation() const {return translation;}
	void set_translation(vector3& translation) {this->translation = translation;}

public:
	transform();

private:
	UINT system_flags;		//	フラグ類
	vector3 scale;			//	拡縮率
	rotator rotation;		//	回転情報
	vector3 translation;	//	座標情報
};
