#include "Object/SceneSystem/MiniGame/minigame000_system.h"

#include "cereal/cereal.hpp"
#include "cereal/archives/json.hpp"
#include "cereal/types/string.hpp"
#include "cereal/types/vector.hpp"

#include <random>

#include "Resource/asset_registry.h"
#include "Object/asset_reference_field.h"
#include "Scenes/scene_manager.h"
#include "Scenes/scene_instance.h"
#include "Input/mouse.h"

#include "Object/game_object_serializer.h"
#include "Object/Graphics/camera_object.h"
#include "Object/System/locator_object.h"
#include "Object/Graphics/effect_object.h"
#include "object/gameobjects/MiniGame/minigame000_roomba_object.h"
#include "object/gameobjects/MiniGame/minigame000_roomba_player_controller_object.h"
#include "object/gameobjects/MiniGame/minigame000_roomba_camera_controller_object.h"
#include "object/gameobjects/MiniGame/minigame000_stage_object.h"

namespace
{
	//	1回当たった時の濡れ量
	constexpr float trash_wet_value = 1.0f;
	constexpr float bubble_wet_value = 400.0f;

	//	マウス固定の反映
	void apply_mouse_lock(bool lock)
	{
		if (!mouse::is_valid())
			return;

		mouse::instance()->set_center_lock(lock);
		mouse::instance()->set_cursol_hide(lock);
	}

	//	シーンカメラ検索
	std::shared_ptr<camera_object> find_view_camera(const std::vector<game_object_ptr>& objects)
	{
		std::shared_ptr<camera_object> result;
		for (const game_object_ptr& object : objects)
		{
			if (!object->check_type(camera_object::type_name()) || !object->is_active())
				continue;

			std::shared_ptr<camera_object> camera = std::static_pointer_cast<camera_object>(object);
			if (!result || camera->get_view_order() > result->get_view_order())
				result = camera;
		}
		return result;
	}
}

minigame000_system::minigame000_system()
: stage_scenes()
, roomba_prefab()
, start_point_tag("roomba")
, stage_scene_operation()
, roomba_prefab_operation()
, stage_scene_operations()
, stage_scene_names()
, roomba_prefab_resource()
, roombas()
, controllers()
{
}

minigame000_system::~minigame000_system()
{
}

void minigame000_system::on_begin()
{
	//	ステージ候補からランダムに選ぶ
	selected_stage_index = select_stage_index();
	DEBUG_CHECK_ERROR_CATEGORY(selected_stage_index < 0, core::logger::categorys::system, "minigame000_system stage scene is empty");

	//	非同期読み込み開始
	if (selected_stage_index >= 0)
		stage_scene_operation = resource_manager::instance()->load_async(stage_scenes[selected_stage_index]);
	roomba_prefab_operation = resource_manager::instance()->load_async(roomba_prefab);
	DEBUG_CHECK_ERROR_CATEGORY(!stage_scene_operation || !roomba_prefab_operation, core::logger::categorys::system, "minigame000_system roomba prefab is empty");
}

void minigame000_system::on_end()
{
	//	読込中にシーンが終わったら中断
	if (stage_scene_operation)
	{
		stage_scene_operation->cancel();
		stage_scene_operation.reset();
	}
	if (roomba_prefab_operation)
	{
		roomba_prefab_operation->cancel();
		roomba_prefab_operation.reset();
	}
	roomba_prefab_resource.reset();

	for (const std::weak_ptr<minigame000::roomba_object>& weak_roomba : roombas)
	{
		if (const std::shared_ptr<minigame000::roomba_object> roomba = weak_roomba.lock())
			roomba->set_input_source(nullptr);
	}

	roombas.clear();
	controllers.clear();
	attract_roomba.reset();
	stage.reset();
	trash_count = 0;
	bubble_count = 0;
	state = states::load_wait;

	//	ステージ内シーンの読込中断と、追加したシーンの削除
	for (const resource::load_operation_ptr<scene_resource>& operation : stage_scene_operations)
	{
		operation->cancel();
	}
	stage_scene_operations.clear();
	for (const std::string& name : stage_scene_names)
	{
		get_context().get_scenes().remove_scene(name);
	}
	stage_scene_names.clear();
	selected_stage_index = -1;

	//	マウス固定を解除
	apply_mouse_lock(false);
}

void minigame000_system::pre_update(float elapsed_time)
{
	switch (state)
	{
		case states::load_wait:
		{
			update_stage_loading();
			update_prefab_loading();

			//	プレハブ読込済み、かつステージの全シーンが追加済みならステージの準備に移る
			if (!roomba_prefab_operation && is_stage_ready())
			{
				setup_stage();
				state = states::stage_wait;
			}
			break;
		}
		case states::stage_wait:
		{
			//	ステージ側の準備(エフェクトの読込/振り分け/ゴミの最大数)が整ったら生成に移る
			if (is_stage_completed())
			{
				spawn_roombas();

				//	ゲーム中はマウス固定
				apply_mouse_lock(true);
				state = states::game;
			}
			break;
		}
		case states::game:
		{
			process_stage_effect_events();
			update_stage_effects();
			break;
		}
	}
}

#if defined(USE_IMGUI)
void minigame000_system::imgui_render()
{
	ImGui::SeparatorText("stage_scenes");
	int remove_index = -1;
	for (int index = 0; index < static_cast<int>(stage_scenes.size()); ++index)
	{
		ImGui::PushID(index);
		if (ImGui::SmallButton("remove"))
			remove_index = index;
		ImGui::SameLine();
		asset_reference_field::imgui_combo("##stage_scene", stage_scenes[index], resource::asset_types::scene_set);
		ImGui::PopID();
	}
	if (remove_index >= 0)
		stage_scenes.erase(stage_scenes.begin() + remove_index);
	if (ImGui::Button("add stage scene"))
		stage_scenes.emplace_back();

	ImGui::Separator();
	asset_reference_field::imgui_combo("roomba_prefab", roomba_prefab, resource::asset_types::prefab);
	ImGui::InputTextUtf8("start_point_tag", start_point_tag);

	ImGui::SeparatorText("state");
	ImGui::Text("selected stage : %d", selected_stage_index);
	ImGui::Text("stage loading : %s", stage_scene_operation ? "true" : "false");
	ImGui::Text("stage scene loading : %zu", stage_scene_operations.size());
	ImGui::Text("stage scenes : %zu", stage_scene_names.size());
	ImGui::Text("prefab loading : %s", roomba_prefab_operation ? "true" : "false");
	ImGui::Text("roombas : %zu", roombas.size());
	const std::shared_ptr<minigame000::minigame_stage_object> current_stage = stage.lock();
	ImGui::Text("stage completed : %s", current_stage ? (current_stage->is_completed() ? "true" : "false") : "(none)");
	ImGui::Text("trash : %d / %d", trash_count, current_stage ? current_stage->get_trash_max() : 0);
	ImGui::Text("bubble count : %d", bubble_count);
	ImGui::Text("controllers : %zu", controllers.size());
}
#endif	//	defined(USE_IMGUI)

template<class Archive>
void minigame000_system::serialize_fields(Archive& archive, std::uint32_t version)
{
	switch (version)
	{
		case 1:
		{
			archive(CEREAL_NVP(roomba_prefab),
					CEREAL_NVP(start_point_tag));
			break;
		}
		case 2:
		{
			resource::asset_reference<scene_set_resource> stage_scene;
			archive(CEREAL_NVP(stage_scene),
					CEREAL_NVP(roomba_prefab),
					CEREAL_NVP(start_point_tag));
			stage_scenes = { stage_scene };
			break;
		}
		case 3:
		{
			archive(CEREAL_NVP(stage_scenes),
					CEREAL_NVP(roomba_prefab),
					CEREAL_NVP(start_point_tag));
			break;
		}
	}
}

void minigame000_system::serialize(cereal::JSONOutputArchive& archive) const
{
	game_object_serializer::save_fields(archive, "minigame000_system", *this);
}

void minigame000_system::deserialize(cereal::JSONInputArchive& archive)
{
	game_object_serializer::load_fields(archive, "minigame000_system", *this);
}

int minigame000_system::select_stage_index() const
{
	//	@remainder	通信ではホストが選んだ番号を配る
	std::vector<int> candidates;
	for (int index = 0; index < static_cast<int>(stage_scenes.size()); ++index)
	{
		if (!stage_scenes[index].is_empty())
			candidates.emplace_back(index);
	}
	if (candidates.empty())
		return -1;

	std::mt19937 random(std::random_device{}());
	std::uniform_int_distribution<std::size_t> distribution(0, candidates.size() - 1);
	return candidates[distribution(random)];
}

void minigame000_system::update_stage_loading()
{
	//	シーンセットの読込が終わったら、中の各シーンの非同期読込を始める
	if (stage_scene_operation && stage_scene_operation->is_done())
	{
		const resource::load_operation_ptr<scene_set_resource> operation = std::move(stage_scene_operation);
		stage_scene_operation.reset();

		if (!operation->is_succeeded() || !operation->get_resource())
		{
			DEBUG_ERROR_CATEGORY(core::logger::categorys::system, "minigame000_system stage scene load failed[%s] %s",
				std::string(operation->get_file_path()).c_str(), operation->get_error_message().c_str());
		}
		else
		{
			for (const std::string& path : operation->get_resource()->get_scene_paths())
			{
				stage_scene_operations.emplace_back(resource_manager::instance()->load_async<scene_resource>(path));
			}
		}
	}

	//	読み終わったシーンから追加をリクエスト
	for (auto it = stage_scene_operations.begin(); it != stage_scene_operations.end();)
	{
		const resource::load_operation_ptr<scene_resource>& operation = *it;
		if (!operation->is_done())
		{
			++it;
			continue;
		}

		if (!operation->is_succeeded() || !operation->get_resource())
		{
			DEBUG_ERROR_CATEGORY(core::logger::categorys::system, "minigame000_system stage scene load failed[%s] %s",
				std::string(operation->get_file_path()).c_str(), operation->get_error_message().c_str());
		}
		else
		{
			const scene_resource_ptr& resource = operation->get_resource();
			if (get_context().get_scenes().add_scene(resource))
				stage_scene_names.emplace_back(scene_manager::make_scene_name(resource));
			else
				DEBUG_ERROR_CATEGORY(core::logger::categorys::system, "minigame000_system stage scene add failed[%s]", std::string(operation->get_file_path()).c_str());
		}
		it = stage_scene_operations.erase(it);
	}
}

bool minigame000_system::is_stage_ready() const
{
	if (stage_scene_operation || !stage_scene_operations.empty())
		return false;

	for (const std::string& name : stage_scene_names)
	{
		if (!get_context().get_scenes().find_scene(name))
			return false;
	}
	return true;
}

void minigame000_system::update_prefab_loading()
{
	if (!roomba_prefab_operation || !roomba_prefab_operation->is_done())
		return;

	const resource::load_operation_ptr<prefab_resource> operation = std::move(roomba_prefab_operation);
	roomba_prefab_operation.reset();

	if (!operation->is_succeeded() || !operation->get_resource())
	{
		DEBUG_ERROR_CATEGORY(core::logger::categorys::system, "minigame000_system roomba prefab load failed[%s] %s",
			std::string(operation->get_file_path()).c_str(), operation->get_error_message().c_str());
		return;
	}

	roomba_prefab_resource = operation->get_resource();
}

std::vector<std::shared_ptr<locator_object>> minigame000_system::collect_start_points() const
{
	//	ステージの全シーンから集める
	std::vector<const scene_instance*> stages;
	for (const std::string& name : stage_scene_names)
	{
		if (const scene_instance* stage = get_context().get_scenes().find_scene(name))
			stages.emplace_back(stage);
	}

	//	一致する開始点を取得
	std::vector<std::shared_ptr<locator_object>> start_points;
	for (const scene_instance* stage : stages)
		locator_object::collect(*stage, start_point_tag, start_points);
	if (!start_points.empty())
		return start_points;

	//	一致する物が無ければ全てのロケーターを使う
	DEBUG_WARNING_CATEGORY(core::logger::categorys::system, "minigame000_system locator not found[%s]. use all locators\n", start_point_tag.c_str());
	for (const scene_instance* stage : stages)
		locator_object::collect(*stage, "", start_points);
	return start_points;
}

void minigame000_system::spawn_roombas()
{
	//	プレハブの読込に失敗していたら生成しない
	if (!roomba_prefab_resource)
	{
		DEBUG_ERROR_CATEGORY(core::logger::categorys::system, "minigame000_system roomba prefab not loaded");
		return;
	}

	const std::vector<std::shared_ptr<locator_object>> start_points = collect_start_points();
	const std::shared_ptr<locator_object> start_point = start_points.empty() ? nullptr : start_points.front();
	DEBUG_CHECK_ERROR_CATEGORY(!start_point, core::logger::categorys::system, "minigame000_system locator_object not found[%s]", start_point_tag.c_str());

	//	生成して最初のルンバをプレイヤー操作に割当
	//	@remainder	AI/通信の割り当ては残りの開始位置を使ってここで分岐させる予定
	if (const std::shared_ptr<minigame000::roomba_object> roomba = spawn_roomba(*roomba_prefab_resource, start_point))
	{
		possess_player(roomba);
		bind_stage_effects(roomba);
	}
}

std::shared_ptr<minigame000::roomba_object> minigame000_system::spawn_roomba(const prefab_resource& prefab, const std::shared_ptr<locator_object>& start_point)
{
	prefab_resource::instance created;
	if (!prefab.instantiate(created) || created.objects.empty())
	{
		DEBUG_ERROR_CATEGORY(core::logger::categorys::system, "minigame000_system roomba prefab instantiate failed");
		return nullptr;
	}

	//	ルンバ本体を探す
	std::shared_ptr<minigame000::roomba_object> roomba;
	for (const game_object_ptr& object : created.objects)
	{
		if (object->check_type(minigame000::roomba_object::type_name()))
		{
			roomba = std::static_pointer_cast<minigame000::roomba_object>(object);
			break;
		}
	}
	if (!roomba)
	{
		DEBUG_ERROR_CATEGORY(core::logger::categorys::system, "minigame000_system roomba_object not found in prefab");
		return nullptr;
	}

	//	開始位置に置く
	if (start_point)
	{
		roomba->set_world_translation(start_point->get_world_translation());
		roomba->set_world_rotation(start_point->get_world_rotation());
	}

	//	ステージの環境情報を渡す
	if (const std::shared_ptr<minigame000::minigame_stage_object> current = stage.lock())
	{
		const minigame000::stage_settings& settings = current->get_setting();
		minigame000::roomba_movement_environment environment;
		environment.world_gravity = settings.world_gravity;
		environment.world_air_density = settings.world_air_density;
		roomba->set_movement_environment(environment);
	}

	//	オブジェクト登録
	for (const game_object_ptr& object : created.objects)
	{
		register_object(object);
	}

	roombas.emplace_back(roomba);
	return roomba;
}

void minigame000_system::possess_player(const std::shared_ptr<minigame000::roomba_object>& roomba)
{
	const std::shared_ptr<camera_object> view_camera = find_view_camera(get_scene_objects());

	auto controller = std::make_shared<minigame000::roomba_player_controller_object>("roomba_player_controller");
	controller->set_camera(view_camera);
	register_object(controller);
	roomba->set_input_source(controller);
	controllers.emplace_back(std::move(controller));

	auto camera_controller = std::make_shared<minigame000::roomba_camera_controller_object>("roomba_camera_controller");
	camera_controller->set_camera(view_camera);
	camera_controller->set_target(roomba);
	register_object(camera_controller);
	camera_controllers.emplace_back(std::move(camera_controller));
}

void minigame000_system::setup_stage()
{
	std::shared_ptr<minigame000::minigame_stage_object> found_stage;
	for (const std::string& name : stage_scene_names)
	{
		const scene_instance* scene = get_context().get_scenes().find_scene(name);
		if (!scene)
			continue;

		for (const std::weak_ptr<game_object>& weak_object : scene->get_objects())
		{
			game_object_ptr object = weak_object.lock();
			if (object && object->check_type(minigame000::minigame_stage_object::type_name()))
			{
				found_stage = std::static_pointer_cast<minigame000::minigame_stage_object>(object);
				break;
			}
		}
		if (found_stage)
			break;
	}

	DEBUG_CHECK_ERROR_CATEGORY(!found_stage, core::logger::categorys::system, "minigame000_system stage_object not found");

	if (found_stage)
		found_stage->begin_setup();
	stage = found_stage;
}

bool minigame000_system::is_stage_completed() const
{
	const std::shared_ptr<minigame000::minigame_stage_object> current = stage.lock();
	return !current || current->is_completed();
}

void minigame000_system::bind_stage_effects(const std::shared_ptr<minigame000::roomba_object>& roomba)
{
	//	@remainder	エフェクト1つにつき座標は1つなので、複数台で吸い込む場合は別の仕組みが必要
	attract_roomba = roomba;

	const std::shared_ptr<minigame000::minigame_stage_object> current = stage.lock();
	if (!current)
		return;

	//	追従先はルンバの当たり判定
	std::shared_ptr<spatial_object> target = roomba->get_collision_object();
	if (!target)
		target = roomba;

	//	ゴミ/泡のエフェクトの追従先を設定
	for (auto weak_effect : current->get_trash_effects())
	{
		if (const std::shared_ptr<effect_object> effect = weak_effect.lock())
		{
			effect->set_attract_target(target);
			effect->set_event_target(target);
		}
	}
	for (auto weak_effect : current->get_bubble_effects())
	{
		if (const std::shared_ptr<effect_object> effect = weak_effect.lock())
		{
			effect->set_attract_target(target);
			effect->set_event_target(target);
		}
	}
}

void minigame000_system::update_stage_effects()
{
	const std::shared_ptr<minigame000::roomba_object> roomba = attract_roomba.lock();
	const std::shared_ptr<minigame000::minigame_stage_object> current = stage.lock();
	if (!roomba || !current)
		return;

	//	吸引アイテムを持つパーティクルの範囲を書き換える
	const float radius = roomba->get_attract_range();
	for (const std::vector<std::weak_ptr<effect_object>>* list : { &current->get_trash_effects(), &current->get_bubble_effects() })
	{
		for (const std::weak_ptr<effect_object>& weak_effect : *list)
		{
			const std::shared_ptr<effect_object> effect = weak_effect.lock();
			if (effect && effect->get_instance())
				effect->get_instance()->set_attract_radius(radius);
		}
	}
}

void minigame000_system::process_stage_effect_events()
{
	const std::shared_ptr<minigame000::roomba_object> roomba = attract_roomba.lock();
	const std::shared_ptr<minigame000::minigame_stage_object> current = stage.lock();
	if (!roomba || !current)
		return;

	//	ステージが振り分けたエフェクトに届いたイベント数を数える
	const auto count_events = [](const std::vector<std::weak_ptr<effect_object>>& list)
	{
		int count = 0;
		for (const std::weak_ptr<effect_object>& weak_effect : list)
		{
			if (const std::shared_ptr<effect_object> effect = weak_effect.lock())
				count += static_cast<int>(effect->get_particle_events().size());
		}
		return count;
	};

	const int trash_hits = count_events(current->get_trash_effects());
	const int bubble_hits = count_events(current->get_bubble_effects());
	trash_count += trash_hits;
	bubble_count += bubble_hits;

	const float wet_value = trash_wet_value * static_cast<float>(trash_hits) + bubble_wet_value * static_cast<float>(bubble_hits);
	if (wet_value > 0.0f)
		roomba->add_wet(wet_value);
}

SCENE_SYSTEM_REGISTER(minigame000_system);