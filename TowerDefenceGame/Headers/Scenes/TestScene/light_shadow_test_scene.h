#pragma once

#include "scenes/scene_base.h"

#if defined(USE_IMGUI)
#include "Editor/editor_camera.h"
#include "light.h"

#include <vector>

class light_object;
class directional_light_object;
class model_object;

//	シャドウマップ確認シーン
class light_shadow_test_scene : public scene_base
{
	SCENE_DECRLEAR(light_shadow_test_scene, scene_base);

public:
	//	ライト配置設定（長さはメートル、角度は度）
	struct light_settings
	{
		int light_count = 8;			//	ライト数
		float spot_ratio = 0.5f;		//	スポットライトの割合
		float ring_radius = 8.0f;		//	並べる円の半径
		float height = 4.0f;			//	高さ
		float range = 15.0f;			//	影響範囲
		float intensity = 800.0f;			//	強さ
		float spot_inner_cone = 20.0f;	//	スポットライトの内側角
		float spot_outer_cone = 40.0f;	//	スポットライトの外側角
		bool shadow = true;				//	影を落とすか
	};

	//	動き設定
	struct motion_settings
	{
		bool rotate_lights = false;		//	ライトを円周上で回す
		float light_speed = 0.1f;		//	ライトの回転速度
		bool move_character = false;	//	動かすキャラクターを円運動させる
		float character_speed = 0.2f;	//	キャラクターの回転速度
		float character_radius = 4.0f;	//	キャラクターの円運動の半径
		bool animate_character = true;	//	アニメーションするキャラクターを再生する
	};

public:
	light_shadow_test_scene();
	light_shadow_test_scene(const light_shadow_test_scene&) = delete;
	light_shadow_test_scene(light_shadow_test_scene&&) = delete;
	~light_shadow_test_scene() override;

protected:
	bool initialize_core() override;
	bool uninitialize_core() override;
	void update_core(float elapsed_time) override;
	void imgui_render_core() override;

private:
	//	ライトを作り直す
	void clear_lights();
	void rebuild_lights();

	//	ライトの位置と向きを更新
	void update_lights();

	//	動かすキャラクターの位置を更新
	void update_character();

private:
	light_settings settings;
	motion_settings motion;
	std::vector<std::shared_ptr<light_object>> lights;
	float light_time = 0.0f;
	float character_time = 0.0f;

	std::shared_ptr<directional_light_object> directional_light;
	float directional_intensity = 10000.0f;

	std::shared_ptr<model_object> animated_character;
	std::shared_ptr<model_object> moving_character;

	editor_camera_manager::scene_request_ptr editor_camera_request;
};
#endif	//	defined(USE_IMGUI)
