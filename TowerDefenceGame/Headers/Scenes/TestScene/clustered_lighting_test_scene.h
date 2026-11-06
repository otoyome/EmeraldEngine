#pragma once

#include "scenes/scene_base.h"

#if defined(USE_IMGUI)
#include "Editor/editor_camera.h"
#include "light.h"

#include <vector>

class light_object;
class directional_light_object;
class model_object;

//	クラスタードライティング確認シーン
class clustered_lighting_test_scene : public scene_base
{
	SCENE_DECRLEAR(clustered_lighting_test_scene, scene_base);

public:
	//	ライト配置設定
	struct light_settings
	{
		int light_count = 256;			//	ライト数
		float spot_ratio = 0.25f;		//	スポットライトの割合
		float area_size = 60.0f;		//	配置範囲（一辺）
		float height = 2.0f;			//	配置する高さ
		float range = 6.0f;				//	影響範囲
		float intensity = 800.0f;			//	強さ
		float spot_inner_cone = 20.0f;	//	スポットライトの内側角
		float spot_outer_cone = 35.0f;	//	スポットライトの外側角
		float shadow_ratio = 0.0f;		//	影を落とすライトの割合
		bool stratified = true;			//	配置範囲を格子に分けて各マスに1つずつ置く
		int seed = 1;					//	乱数の種
	};

	//	ライトの動き設定
	struct animation_settings
	{
		bool enable = true;			//	動かすか
		float speed = 0.5f;			//	角速度（回転/秒）
		float radius = 2.0f;		//	円運動の半径（メートル）
	};

public:
	clustered_lighting_test_scene();
	clustered_lighting_test_scene(const clustered_lighting_test_scene&) = delete;
	clustered_lighting_test_scene(clustered_lighting_test_scene&&) = delete;
	~clustered_lighting_test_scene() override;

protected:
	bool initialize_core() override;
	bool uninitialize_core() override;
	void update_core(float elapsed_time) override;
	void imgui_render_core() override;

private:
	void clear_lights();
	void rebuild_lights();
	void update_light_positions();

private:
	//	生成したライト
	struct light_entry
	{
		std::shared_ptr<light_object> object;
		math::vector3d base_position;	//	円運動の中心
		float phase = 0.0f;				//	円運動の初期位相
	};

	light_settings settings;
	animation_settings animation;
	std::vector<light_entry> lights;
	float animation_time = 0.0f;

	std::shared_ptr<directional_light_object> directional_light;
	float directional_intensity = 10000.0f;	//	平行光源の強さ

	editor_camera_manager::scene_request_ptr editor_camera_request;
};
#endif	//	defined(USE_IMGUI)
