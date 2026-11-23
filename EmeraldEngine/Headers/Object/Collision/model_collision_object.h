#pragma once

#include "Object/collision_placement_object.h"

//	事前計算済み当たり判定オブジェクト
class model_collision_object : public collision_placement_object
{
	GAME_OBJECT_DECRLEAR(model_collision_object, collision_placement_object)

	static constexpr std::uint32_t serialize_version = 1;

public:
	model_collision_object(std::string_view name);
	~model_collision_object() override;

	//	当たり判定を作るモデル
	const core::uuid& get_model() const { return model; }
	void set_model(const core::uuid& value);

	//	sdfがあれば使用する
	void set_register_sdf(bool value);

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
	void release_resources() override;

private:
	core::uuid model;
	bool register_sdf = true;
	std::vector<uint32_t> sdf_ids;
};
