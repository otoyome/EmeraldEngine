#pragma once

#include "Object/spatial_object.h"

//	グループオブジェクト
class group_object : public spatial_object
{
	GAME_OBJECT_DECRLEAR(group_object, spatial_object)

public:
	group_object(std::string_view name);
	~group_object() override;
};
