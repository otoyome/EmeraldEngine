#pragma once

#include "Scenes/scene_system.h"
#include "Graphics/Lighting/light.h"

#include <vector>

class light_object;
class directional_light_object;
class model_object;

//	シャドウマップ確認シーン
class light_shadow_test_system : public scene_system
{
	SCENE_SYSTEM_DECRLEAR(light_shadow_test_system, scene_system)

public:
	//	ライト配置設定（長さはメートル、角度は度）
	struct light_settings
	{
		int light_count = 8;			//	ライト数
		float spot_ratio = 0.5f;		//	スポットライトの割合
		float ring_radius = 8.0f;		//	並べる円の半径
		float height = 4.0f;			//	高さ
		float range = 15.0f;			//	影響範囲
		float intensity = 20000.0f;		//	強さ
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
	light_shadow_test_system();
	~light_shadow_test_system() override;

	void on_begin() override;
	void on_end() override;
	void pre_update(float elapsed_time) override;

	void serialize(cereal::JSONOutputArchive& archive) const override;
	void deserialize(cereal::JSONInputArchive& archive) override;

#if defined(USE_IMGUI)
	void imgui_render() override;
#endif	//	defined(USE_IMGUI)

	//	シーンに置いたオブジェクト
	void set_directional_light(std::shared_ptr<directional_light_object> light) { directional_light = std::move(light); }
	void set_animated_character(std::shared_ptr<model_object> model) { animated_character = std::move(model); }
	void set_moving_character(std::shared_ptr<model_object> model) { moving_character = std::move(model); }

	//	円周上に並べたライトを作る
	static std::vector<std::shared_ptr<light_object>> create_ring_lights(const light_settings& settings);

private:
	//	動かすライト
	struct light_entry
	{
		std::shared_ptr<light_object> object;
		math::vector3d base_position;	//	回す前の位置
		bool active = true;				//	シーンに置いたときの有効フラグ
	};

	void collect_scene_lights();
	void restore_scene_lights();
	void set_scene_lights_active(bool active);

	void regenerate_lights();
	void clear_generated_lights();

	//	ライトの位置と向きを更新
	void update_lights();

	//	動かすキャラクターの位置を更新
	void update_character();

	//	今動かしているライト
	std::vector<light_entry>& get_current_lights();

private:
	light_settings settings;
	motion_settings motion;
	std::vector<light_entry> scene_lights;		//	シーンに置いたライト
	std::vector<light_entry> generated_lights;	//	作り直したライト
	float light_time = 0.0f;
	float character_time = 0.0f;

	std::shared_ptr<directional_light_object> directional_light;
	std::shared_ptr<model_object> animated_character;
	std::shared_ptr<model_object> moving_character;
	math::vector3d moving_character_position;	//	シーンに置いた位置
};
