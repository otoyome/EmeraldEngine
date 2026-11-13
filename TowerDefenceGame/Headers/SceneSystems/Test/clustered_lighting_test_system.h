#pragma once

#include "Scenes/scene_system.h"
#include "Graphics/Lighting/light.h"

#include <vector>

class light_object;
class directional_light_object;
class model_object;

//	クラスタードライティング確認シーン
class clustered_lighting_test_system : public scene_system
{
	SCENE_SYSTEM_DECRLEAR(clustered_lighting_test_system, scene_system)

public:
	//	ライト配置設定
	struct light_settings
	{
		int light_count = 256;			//	ライト数
		float spot_ratio = 0.25f;		//	スポットライトの割合
		float area_size = 60.0f;		//	配置範囲（一辺）
		float height = 2.0f;			//	配置する高さ
		float range = 6.0f;				//	影響範囲
		float intensity = 5000.0f;		//	強さ
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
	clustered_lighting_test_system();
	~clustered_lighting_test_system() override;

	void on_begin() override;
	void on_end() override;
	void pre_update(float elapsed_time) override;

	void serialize(cereal::JSONOutputArchive& archive) const override;
	void deserialize(cereal::JSONInputArchive& archive) override;

#if defined(USE_IMGUI)
	void imgui_render() override;
#endif	//	defined(USE_IMGUI)

	//	平行光源（.escene に置いたもの）
	void set_directional_light(std::shared_ptr<directional_light_object> light) { directional_light = std::move(light); }

private:
	//	動かすライト
	struct light_entry
	{
		std::shared_ptr<light_object> object;
		math::vector3d base_position;	//	円運動の中心
		float phase = 0.0f;				//	円運動の初期位相
		bool active = true;				//	シーンに置いたときの有効フラグ
	};

	void collect_scene_lights();
	void restore_scene_lights();
	void set_scene_lights_active(bool active);

	void regenerate_lights();
	void clear_generated_lights();

	void update_light_positions();

	std::vector<light_entry>& get_current_lights();

private:
	light_settings settings;
	animation_settings animation;
	std::vector<light_entry> scene_lights;
	std::vector<light_entry> generated_lights;
	float animation_time = 0.0f;

	std::shared_ptr<directional_light_object> directional_light;
};
