#pragma once

#include "Scenes/scene_system.h"
#include "Scenes/scene_resource.h"
#include "Scenes/scene_set_resource.h"
#include "Resource/resource_manager.h"
#include "Object/prefab_resource.h"

class locator_object;
class effect_object;
class game_object_serializer;

namespace minigame000
{
	class roomba_object;
	class roomba_player_controller_object;
	class roomba_camera_controller_object;
}	//	namespace minigame000

//	ミニゲームのシーンシステム
class minigame000_system : public scene_system
{
	SCENE_SYSTEM_DECRLEAR(minigame000_system, scene_system)

	friend class ::game_object_serializer;
	static constexpr std::uint32_t serialize_version = 3;	//	シリアライズのバージョン

public:
	//	遷移
	enum class states
	{
		load_wait,
		game,
	};

public:
	minigame000_system();
	~minigame000_system() override;

	void on_begin() override;
	void on_end() override;

	void pre_update(float elapsed_time) override;

#if defined(USE_IMGUI)
	void imgui_render() override;
#endif	//	defined(USE_IMGUI)

public:	//	シリアライズ
	void serialize(cereal::JSONOutputArchive& archive) const override;
	void deserialize(cereal::JSONInputArchive& archive) override;

private:	//	シリアライズ
	template<class Archive>
	void serialize_fields(Archive& archive, std::uint32_t version);

private:
	//	ステージ候補からランダムに1つ選ぶ
	int select_stage_index() const;

	//	読込の進行
	void update_stage_loading();
	void update_prefab_loading();

	//	ステージの全シーンが追加済みか
	bool is_stage_ready() const;

	//	開始位置収集
	std::vector<std::shared_ptr<locator_object>> collect_start_points() const;

	//	ルンバ生成
	void spawn_roombas();
	std::shared_ptr<minigame000::roomba_object> spawn_roomba(const prefab_resource& prefab, const std::shared_ptr<locator_object>& start_point);

	//	プレイヤー操作を割り当てる
	void possess_player(const std::shared_ptr<minigame000::roomba_object>& roomba);

	//	ステージのエフェクトの吸引/イベント座標をルンバへ追従させる
	void bind_stage_effects(const std::shared_ptr<minigame000::roomba_object>& roomba);

	//	ステージのエフェクトの吸引範囲をルンバの速度に合わせる
	void update_stage_effects();

private:	//	設定
	std::vector<resource::asset_reference<scene_set_resource>> stage_scenes;				//	ステージシーン候補(開始時にランダムで1つ選ぶ)
	resource::asset_reference<prefab_resource> roomba_prefab;								//	ルンバのプレハブ
	std::string start_point_tag;															//	使う開始位置のタグ

private:	//	実行時
	states state = states::load_wait;														//	ステート
	int selected_stage_index = -1;															//	選んだステージ候補の番号
	resource::load_operation_ptr<scene_set_resource> stage_scene_operation;					//	ステージシーンセットのロードオペレーター
	std::vector<resource::load_operation_ptr<scene_resource>> stage_scene_operations;		//	ステージ内の各シーンのロードオペレーター
	resource::load_operation_ptr<prefab_resource> roomba_prefab_operation;					//	プレハブのロードオペレーター
	std::vector<std::string> stage_scene_names;												//	追加したステージのシーン名
	prefab_resource_ptr roomba_prefab_resource;												//	読み込んだプレハブ

	std::vector<std::weak_ptr<minigame000::roomba_object>> roombas;							//	生成したルンバ
	std::weak_ptr<minigame000::roomba_object> attract_roomba;								//	エフェクトを吸い込むルンバ
	std::vector<std::weak_ptr<effect_object>> stage_effects;								//	ステージのエフェクト
	std::vector<std::shared_ptr<minigame000::roomba_player_controller_object>> controllers;	//	生成したコントローラー
	std::vector<std::shared_ptr<minigame000::roomba_camera_controller_object>> camera_controllers;
};