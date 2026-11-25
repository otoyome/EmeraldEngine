#pragma once

#include "Scenes/scene_system.h"
#include "Scenes/scene_resource.h"
#include "Resource/resource_manager.h"
#include "Input/input_action.h"
#include "Core/signals.h"

class game_object_serializer;
class camera_object;
class locator_object;

//	ミニゲームのタイトルのシーンシステム
class minigame000_title_system : public scene_system
{
	SCENE_SYSTEM_DECRLEAR(minigame000_title_system, scene_system)

	friend class ::game_object_serializer;
	static constexpr std::uint32_t serialize_version = 2;	//	シリアライズのバージョン

public:
	//	遷移
	enum class states
	{
		input_wait,		//	決定入力待ち
		load_wait,		//	次のシーンの読込待ち
		changed,		//	切替済み
	};

public:
	minigame000_title_system();
	~minigame000_title_system() override;

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
	//	次のシーンの読込の進行
	void update_next_scene_loading();

	//	フォーカス点の周りをカメラで回る
	void update_camera(float elapsed_time);

private:	//	設定
	resource::asset_reference<scene_resource> next_scene;				//	決定後に切り替えるシーン
	float input_delay = 0.5f;											//	開始直後に入力を受け付けない時間(秒)

	//	カメラ
	std::string camera_focus_tag;										//	フォーカスするロケーターのタグ
	float camera_distance;												//	フォーカス点からの距離
	float camera_pitch;													//	見下ろす角度(ラジアン)
	float camera_rotate_speed;											//	回る速さ(ラジアン/秒)

private:	//	実行時
	states state = states::input_wait;									//	ステート
	float elapsed = 0.0f;												//	開始からの経過時間
	bool decided = false;												//	決定入力があったか
	resource::load_operation_ptr<scene_resource> next_scene_operation;	//	次のシーンのロードオペレーター
	scene_resource_ptr next_scene_resource;								//	読み込んだ次のシーン

	std::weak_ptr<camera_object> camera;								//	回すカメラ
	std::weak_ptr<locator_object> camera_focus;							//	フォーカスするロケーター
	double camera_yaw = 0.0;											//	今の回転角

	input::action decide_action;										//	決定の入力アクション
	core::signals::connection_list connections;							//	購読リスト
};
