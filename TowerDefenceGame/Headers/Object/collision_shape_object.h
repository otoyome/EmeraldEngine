#pragma once

#include "Object/collision_placement_object.h"

//	形状当たり判定オブジェクト
class collision_shape_object : public collision_placement_object
{
	GAME_OBJECT_DECRLEAR(collision_shape_object, collision_placement_object)

	static constexpr std::uint32_t serialize_version = 1;

public:
	enum class shapes : std::uint8_t
	{
		sphere,
		capsule,
		box,
	};

public:
	collision_shape_object(std::string_view name);
	~collision_shape_object() override;

	shapes get_shape() const { return shape; }
	void set_shape(shapes value);

	//	形状の大きさ
	//	球 {半径}
	//	カプセル {半径, 長さ}
	//	箱 {半分の大きさ xyz}
	const math::float3& get_shape_scale() const { return shape_scale; }
	void set_shape_scale(const math::float3& value);

	void serialize(cereal::JSONOutputArchive& archive) const override;
	void deserialize(cereal::JSONInputArchive& archive) override;

private:
	friend class game_object_serializer;
	template<class Archive>
	void serialize_fields(Archive& archive, std::uint32_t version);

public:

#if defined(USE_IMGUI)
	void imgui_render_core() override;
#endif	//	defined(USE_IMGUI)

protected:
	void create_collisions(std::vector<collision_object_ptr>& out_collisions) override;

private:
	shapes shape = shapes::box;
	math::float3 shape_scale;
};
