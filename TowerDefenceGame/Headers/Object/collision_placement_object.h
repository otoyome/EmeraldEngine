#pragma once

#include <string>
#include <vector>

#include "Object/spatial_object.h"
#include "Object/Collision/collision_object.h"

//	基底当たり判定オブジェクト
class collision_placement_object : public spatial_object
{
	GAME_OBJECT_DECRLEAR(collision_placement_object, spatial_object)

	static constexpr std::uint32_t serialize_version = 2;

public:
	size_t get_collision_count() const { return collisions.size(); }

	const std::string& get_filter_name() const { return filter_name; }
	void set_filter_name(std::string_view name);

public:
	collision_placement_object(std::string_view name);
	~collision_placement_object() override;

	void on_initialize() override;
	void on_finalize() override;
	void fixed_update(float elapsed_time) override;
	void pre_render() override;

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
	//	当たり判定生成
	virtual void create_collisions(std::vector<collision_object_ptr>& out_collisions) {}
	//	当たり判定破棄
	virtual void release_resources() {}

	//	リビルド
	void rebuild_collisions();

private:
	void build_collisions();
	void release_collisions();
	void sync_transform();

private:
	std::vector<collision_object_ptr> collisions;
	std::string filter_name = "default";
};
