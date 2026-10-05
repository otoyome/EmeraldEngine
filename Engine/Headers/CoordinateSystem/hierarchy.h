#pragma once

#include "Math/math_all.h"

class hierarchy
{
	enum class flags
	{
		dirty,	//	再計算フラグ
	};

public:
	bool is_flag(flags flag) const;
	void set_flag(flags flag, bool value);

public:
	hierarchy();

private:
	UINT system_flags;		//	フラグ類
};
