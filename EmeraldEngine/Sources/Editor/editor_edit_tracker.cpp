#include "Editor/editor_edit_tracker.h"

#if defined(USE_IMGUI)
#include <sstream>

#include "ImGuizmo/ImGuizmo.h"
#include "Object/engine_context.h"
#include "Object/object_manager.h"
#include "Object/game_object_serializer.h"
#include "Scenes/scene_system.h"
#include "Material/material_resource.h"

namespace
{
	//	ワールドのオブジェクトを UUID から引く関数
	game_object_serializer::find_function make_find_function(engine_context& context)
	{
		return [&context](const core::uuid& uuid) -> game_object_ptr
		{
			if (uuid.is_empty())
				return nullptr;
			for (const game_object_ptr& object : context.get_objects().search_game_objects<game_object>())
			{
				if (object->get_uuid() == uuid)
					return object;
			}
			return nullptr;
		};
	}
}

void editor_edit_tracker::begin(std::vector<target> targets)
{
	//	操作中は編集前の中身を持ったままにする
	if (interacting)
		return;

	tracked_targets.clear();
	for (target& source : targets)
	{
		if (!source.capture || !source.apply)
			continue;
		std::string snapshot = source.capture();
		if (snapshot.empty())
			continue;
		tracked_targets.push_back({ std::move(source), std::move(snapshot) });
	}
}

void editor_edit_tracker::end(editor_transaction_history& history)
{
	//	操作が終わったフレームか、1フレームで終わる操作があったフレームで比べる
	const bool now_interacting = ImGui::IsAnyItemActive() || ImGuizmo::IsUsing();
	const bool finished = !now_interacting && (interacting
		|| ImGui::IsMouseReleased(ImGuiMouseButton_Left)
		|| ImGui::IsKeyReleased(ImGuiKey_Enter)
		|| ImGui::IsKeyReleased(ImGuiKey_KeypadEnter)
		|| ImGui::IsKeyReleased(ImGuiKey_Space));
	interacting = now_interacting;
	if (!finished)
		return;

	for (tracked_target& tracked : tracked_targets)
	{
		std::string after = tracked.source.capture();
		if (after.empty() || after == tracked.snapshot)
			continue;

		std::string before = std::move(tracked.snapshot);
		tracked.snapshot = after;
		const std::function<bool(const std::string&)> apply = tracked.source.apply;
		history.push(tracked.source.label,
			[apply, before]() { return apply(before); },
			[apply, after]() { return apply(after); });
	}
}

void editor_edit_tracker::resync()
{
	for (tracked_target& tracked : tracked_targets)
		tracked.snapshot = tracked.source.capture();
}

void editor_edit_tracker::reset()
{
	tracked_targets.clear();
	interacting = false;
}

editor_edit_tracker::target editor_edit_tracker::make_object_target(engine_context& context, const std::shared_ptr<game_object>& object)
{
	target result;
	if (!object)
		return result;

	const std::weak_ptr<game_object> weak_object = object;
	result.label = make_label("Edit", object);
	result.capture = [weak_object]() -> std::string
	{
		const game_object_ptr locked = weak_object.lock();
		if (!locked)
			return {};
		std::ostringstream stream;
		if (!game_object_serializer::save_snapshot(stream, *locked))
			return {};
		return stream.str();
	};
	result.apply = [weak_object, &context](const std::string& snapshot)
	{
		const game_object_ptr locked = weak_object.lock();
		if (!locked || snapshot.empty())
			return false;
		std::istringstream stream(snapshot);
		return game_object_serializer::load_snapshot(stream, *locked, make_find_function(context));
	};
	return result;
}

std::string editor_edit_tracker::make_system_snapshot(const scene_system& system)
{
	std::ostringstream stream;
	if (!game_object_serializer::save_snapshot(stream, system))
		return {};
	return stream.str();
}

bool editor_edit_tracker::apply_system_snapshot(engine_context& context, scene_system& system, const std::string& snapshot)
{
	if (snapshot.empty())
		return false;
	std::istringstream stream(snapshot);
	return game_object_serializer::load_snapshot(stream, system, make_find_function(context));
}

std::string editor_edit_tracker::make_label(std::string_view action, const std::shared_ptr<game_object>& object)
{
	if (!object)
		return std::string(action);
	const core::cache_string name(object->get_name());
	return std::string(action) + " " + name.get<core::utf8_cache>().data();
}

editor_edit_tracker::shortcuts editor_edit_tracker::check_shortcuts(ImGuiInputFlags route)
{
	//	受け先の登録のため、毎フレーム必ず Shortcut を呼ぶ
	const ImGuiInputFlags flags = route | ImGuiInputFlags_Repeat;
	const bool undo = ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Z, flags);
	const bool redo = ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Y, flags) || ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_Z, flags);

	//	文字入力中は入力欄の元に戻すを優先し、スライダーやギズモの操作中は積む途中なので受け付けない
	if (ImGui::GetIO().WantTextInput || ImGui::IsAnyItemActive() || ImGuizmo::IsUsing())
		return shortcuts::none;
	if (redo)
		return shortcuts::redo;
	if (undo)
		return shortcuts::undo;
	return shortcuts::none;
}

////////////////////////////////////////////////////////////////////////////////////////////////
void editor_change_tracker::set_target(std::string label, editor_transaction_target target)
{
	if (!target.capture)
	{
		reset();
		return;
	}

	this->label = std::move(label);
	if (target.key && target.key == this->target.key)
		return;

	//	別の物になったら、変更前を取り直す
	this->target = std::move(target);
	checkpoint = this->target.capture();
	changed = false;
}

void editor_change_tracker::end(editor_transaction_history& history)
{
	//	操作中(ドラッグ中など)は続きの変更があるので、操作が終わってから積む
	if (!changed || ImGui::IsAnyItemActive() || ImGuizmo::IsUsing())
		return;
	changed = false;
	if (!target.capture || !checkpoint)
		return;

	//	先に取っておいた変更前を使って、明示的なトランザクションとして積む
	history.begin_transaction(label);
	history.record(target, checkpoint);
	history.commit_transaction();

	//	次の変更の変更前は今の中身
	checkpoint = target.capture();
}

void editor_change_tracker::resync()
{
	changed = false;
	checkpoint = target.capture ? target.capture() : editor_restore_function();
}

void editor_change_tracker::reset()
{
	label.clear();
	target = {};
	checkpoint = {};
	changed = false;
}

editor_transaction_target editor_change_tracker::make_material_target(const std::shared_ptr<material_resource>& material, std::function<void()> on_restored)
{
	editor_transaction_target result;
	if (!material)
		return result;

	result.key = material.get();
	const std::weak_ptr<material_resource> weak_material = material;
	result.capture = [weak_material, on_restored]() -> editor_restore_function
	{
		const std::shared_ptr<material_resource> locked = weak_material.lock();
		if (!locked)
			return {};

		//	今の中身を複製しておき、戻す時はその複製から書き戻す
		auto copy = std::make_shared<material_resource>();
		locked->duplicate(*copy);
		return [weak_material, copy, on_restored]()
		{
			const std::shared_ptr<material_resource> target = weak_material.lock();
			if (!target)
				return false;
			copy->duplicate(*target);
			if (on_restored)
				on_restored();
			return true;
		};
	};
	return result;
}
#endif	//	defined(USE_IMGUI)
