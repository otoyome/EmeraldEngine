#pragma once

#include "scenes/scene_base.h"
#include "collision/collision.h"

//	当たり判定確認シーン
class collision_test_scene : public scene_base
{
	SCENE_DECRLEAR(collision_test_scene, scene_base);

public:
	//	形状の種類
	enum class shape_types
	{
		sphere,
		capsule,
		box,
		mesh,	//	床メッシュ
	};

	//	キャストの種類
	enum class cast_types
	{
		segment,
		sphere,
		capsule,
	};

	//	形状設定
	struct shape_desc
	{
		shape_types type = shape_types::sphere;
		math::float3 position = { 0.0f, 0.0f, 0.0f };
		math::float3 rotation = { 0.0f, 0.0f, 0.0f };	//	度数
		float radius = 0.5f;
		float length = 2.0f;
		math::float3 extent = { 1.0f, 1.0f, 1.0f };	//	箱の半径 / 床の半径(xz)
	};

	//	キャスト設定
	struct cast_desc
	{
		cast_types type = cast_types::sphere;
		math::float3 start = { 0.0f, 5.0f, 0.0f };
		math::float3 end = { 0.0f, -5.0f, 0.0f };
		math::float3 rotation = { 0.0f, 0.0f, 0.0f };	//	度数
		float radius = 0.5f;
		float length = 2.0f;
	};

	//	自動テスト結果
	struct test_result
	{
		std::string name;
		bool passed = false;
		std::string detail;
	};

public:
	collision_test_scene();
	collision_test_scene(const collision_test_scene&) = delete;
	collision_test_scene(collision_test_scene&&) = delete;
	~collision_test_scene() override;

protected:
	bool initialize_core() override;
	void update_core(float elapsed_time) override;
#if defined(USE_IMGUI)
	void imgui_render_core() override;
#endif	//	defined(USE_IMGUI)

public:
	//	形状生成
	static std::shared_ptr<collision::bounding_object> create_shape(const shape_desc& desc);
	//	設定をメートルから内部単位へ変換
	static shape_desc to_unit(const shape_desc& desc);
	static cast_desc to_unit(const cast_desc& desc);
	//	キャスト実行
	static bool execute_cast(const cast_desc& desc, const collision::bounding_object& target, collision::intersect_data* intersect);

private:
	void update_pair_test();
	void update_cast_test();
	void run_self_tests();

private:
	int mode = 0;	//	0:交差 1:キャスト
	shape_desc shape_descs[2];
	cast_desc cast;
	bool draw_ghost = true;

	//	表示用の判定結果
	bool hit_ab = false;
	bool hit_ab_without_data = false;
	bool hit_ba = false;
	bool hit_ba_without_data = false;
	collision::intersect_data intersect_ab;
	collision::intersect_data intersect_ba;
	bool cast_hit = false;
	collision::intersect_data cast_intersect;

	std::vector<test_result> test_results;
	size_t test_passed_count = 0;
};
