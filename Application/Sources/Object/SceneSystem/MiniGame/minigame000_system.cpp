#include "Object/SceneSystem/MiniGame/minigame000_system.h"

#include "cereal/cereal.hpp"
#include "cereal/archives/json.hpp"
#include "cereal/types/string.hpp"

#include "Resource/asset_registry.h"
#include "Object/asset_reference_field.h"
#include "Scenes/scene_manager.h"
#include "Scenes/scene_instance.h"

#include "Object/game_object_serializer.h"
#include "object/camera_object.h"
#include "object/system/start_point_object.h"
#include "object/gameobjects/MiniGame/minigame000_roomba_object.h"
#include "object/gameobjects/MiniGame/minigame000_roomba_player_controller_object.h"
#include "object/gameobjects/MiniGame/minigame000_roomba_camera_controller_object.h"

namespace
{
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
: stage_scene()
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
	//	非同期読み込み開始
	stage_scene_operation = resource_manager::instance()->load_async(stage_scene);
	roomba_prefab_operation = resource_manager::instance()->load_async(roomba_prefab);
	DEBUG_CHECK_ERROR_CATEGORY(!stage_scene_operation || !roomba_prefab_operation, logger::categorys::system, "minigame000_system roomba prefab is empty");
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
}

void minigame000_system::pre_update(float elapsed_time)
{
	switch (state)
	{
		case states::load_wait:
		{
			update_stage_loading();
			update_prefab_loading();

			//	プレハブ読込済み、かつステージの全シーンが追加済みなら生成に移る
			if (!roomba_prefab_operation && is_stage_ready())
			{
				spawn_roombas();
				state = states::game;
			}
			break;
		}
	}
}

#if defined(USE_IMGUI)
void minigame000_system::imgui_render()
{
	asset_reference_field::imgui_combo("stage_scene", stage_scene, resource::asset_types::scene_set);
	asset_reference_field::imgui_combo("roomba_prefab", roomba_prefab, resource::asset_types::prefab);
	ImGui::InputTextUtf8("start_point_tag", start_point_tag);

	ImGui::SeparatorText("state");
	ImGui::Text("stage loading : %s", stage_scene_operation ? "true" : "false");
	ImGui::Text("stage scene loading : %zu", stage_scene_operations.size());
	ImGui::Text("stage scenes : %zu", stage_scene_names.size());
	ImGui::Text("prefab loading : %s", roomba_prefab_operation ? "true" : "false");
	ImGui::Text("roombas : %zu", roombas.size());
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
			archive(CEREAL_NVP(stage_scene),
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

void minigame000_system::update_stage_loading()
{
	//	シーンセットの読込が終わったら、中の各シーンの非同期読込を始める
	if (stage_scene_operation && stage_scene_operation->is_done())
	{
		const resource::load_operation_ptr<scene_set_resource> operation = std::move(stage_scene_operation);
		stage_scene_operation.reset();

		if (!operation->is_succeeded() || !operation->get_resource())
		{
			DEBUG_ERROR_CATEGORY(logger::categorys::system, "minigame000_system stage scene load failed[%s] %s",
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
			DEBUG_ERROR_CATEGORY(logger::categorys::system, "minigame000_system stage scene load failed[%s] %s",
				std::string(operation->get_file_path()).c_str(), operation->get_error_message().c_str());
		}
		else
		{
			const scene_resource_ptr& resource = operation->get_resource();
			if (get_context().get_scenes().add_scene(resource))
				stage_scene_names.emplace_back(scene_manager::make_scene_name(resource));
			else
				DEBUG_ERROR_CATEGORY(logger::categorys::system, "minigame000_system stage scene add failed[%s]", std::string(operation->get_file_path()).c_str());
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
		DEBUG_ERROR_CATEGORY(logger::categorys::system, "minigame000_system roomba prefab load failed[%s] %s",
			std::string(operation->get_file_path()).c_str(), operation->get_error_message().c_str());
		return;
	}

	roomba_prefab_resource = operation->get_resource();
}

std::vector<std::shared_ptr<start_point_object>> minigame000_system::collect_start_points() const
{
	//	ステージの全シーンから集める
	std::vector<game_object_ptr> objects;
	for (const std::string& name : stage_scene_names)
	{
		const scene_instance* stage = get_context().get_scenes().find_scene(name);
		if (!stage)
			continue;
		for (const std::weak_ptr<game_object>& weak_object : stage->get_objects())
		{
			if (game_object_ptr object = weak_object.lock())
				objects.emplace_back(std::move(object));
		}
	}

	//	一致する開始点を取得
	std::vector<std::shared_ptr<start_point_object>> tagged_points;
	std::vector<std::shared_ptr<start_point_object>> all_points;
	for (const game_object_ptr& object : objects)
	{
		if (!object->check_type(start_point_object::type_name()))
			continue;

		std::shared_ptr<start_point_object> start_point = std::static_pointer_cast<start_point_object>(object);
		if (start_point->get_tag() == start_point_tag)
			tagged_points.emplace_back(start_point);
		all_points.emplace_back(std::move(start_point));
	}

	return tagged_points.empty() ? all_points : tagged_points;
}

void minigame000_system::spawn_roombas()
{
	//	プレハブの読込に失敗していたら生成しない
	if (!roomba_prefab_resource)
	{
		DEBUG_ERROR_CATEGORY(logger::categorys::system, "minigame000_system roomba prefab not loaded");
		return;
	}

	const std::vector<std::shared_ptr<start_point_object>> start_points = collect_start_points();
	const std::shared_ptr<start_point_object> start_point = start_points.empty() ? nullptr : start_points.front();
	DEBUG_CHECK_ERROR_CATEGORY(!start_point, logger::categorys::system, "minigame000_system start_point_object not found[%s]", start_point_tag.c_str());

	//	生成して最初のルンバをプレイヤー操作に割当
	//	@remainder	AI/通信の割り当ては残りの開始位置を使ってここで分岐させる予定
	if (const std::shared_ptr<minigame000::roomba_object> roomba = spawn_roomba(*roomba_prefab_resource, start_point))
		possess_player(roomba);
}

std::shared_ptr<minigame000::roomba_object> minigame000_system::spawn_roomba(const prefab_resource& prefab, const std::shared_ptr<start_point_object>& start_point)
{
	prefab_resource::instance created;
	if (!prefab.instantiate(created) || created.objects.empty())
	{
		DEBUG_ERROR_CATEGORY(logger::categorys::system, "minigame000_system roomba prefab instantiate failed");
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
		DEBUG_ERROR_CATEGORY(logger::categorys::system, "minigame000_system roomba_object not found in prefab");
		return nullptr;
	}

	//	開始位置に置く
	if (start_point)
	{
		roomba->set_world_translation(start_point->get_world_translation());
		roomba->set_world_rotation(start_point->get_world_rotation());
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

SCENE_SYSTEM_REGISTER(minigame000_system);