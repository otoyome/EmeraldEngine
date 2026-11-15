#include "Object/object_manager.h"
#include "Object/spatial_object.h"
#include "Object/engine_context.h"

#include <algorithm>

namespace
{
	static constexpr float fixed_per_second = system_units::convert_fps_to_sec(1.0f);
}

object_manager::object_manager(engine_context& context)
: context(context)
, game_objects()
, game_object_add()
, game_object_removes()
, messenger()
, world()

, total_time(0)
, remain_time(0)

#if defined(USE_IMGUI)
, pause(false)
, step(false)
, fixed_update_count(0)
, debug_stop_time(false)
#endif // defined(USE_IMGUI)
{
}

object_manager::~object_manager()
{
}

void object_manager::updates(float elapsed_time)
{
	//	追加
	add_objects();

#if defined(USE_IMGUI)
	if (pause)
	{
		if (step)
		{
			elapsed_time = fixed_per_second + 0.00001f;
			step = false;
		}
		else
		{
			debug_updates(elapsed_time);
			return;
		}
	}
#endif // defined(USE_IMGUI)

	//	更新処理
	{
#if defined(USE_IMGUI)
		//	デバッグ更新
		for (auto& object : game_objects)
		{
			if (!object->is_active())
				continue;

			object->debug_start_of_frame();
		}

		remove_objects();
		add_objects();
#endif	//	defined(USE_IMGUI)

		//	フレーム制限
		//	一度に60回以上更新する場合は破棄する
		total_time += elapsed_time;
		float temporary_elapsed_time = elapsed_time + remain_time;
		int loop_count = static_cast<int>(temporary_elapsed_time / fixed_per_second);
#if defined(USE_IMGUI)
		fixed_update_count = loop_count;
#endif // defined(USE_IMGUI)
		int limit_loop_count = std::min(loop_count, static_cast<int>(1.0f / fixed_per_second + 0.0001f));
		temporary_elapsed_time -= (loop_count - limit_loop_count) * fixed_per_second;
		if (loop_count > limit_loop_count)
			elapsed_time = temporary_elapsed_time;

		//	固定更新処理
		for (int count = 0; count < limit_loop_count; ++count)
		{
			{
				DETAIL_PROFILE_CPU_SCOPE("object.fixed_update");
				for_each_parallel([](game_object* object)
				{
					object->fixed_update(fixed_per_second);
				}, object_profile_function::fixed_update);
			}

			remove_objects();
			add_objects();

			//	当たり判定
			{
				DETAIL_PROFILE_CPU_SCOPE("object.collision_step");
		#if 01	//	defined(_DEBUG)
				world.step(fixed_per_second, 4, system_units::convert_fps_to_sec(0.25f));
		#else	//	defined(_DEBUG)
				world.step(fixed_per_second);
		#endif	//	defined(_DEBUG)
			}

			temporary_elapsed_time -= fixed_per_second;
		}
		remain_time = temporary_elapsed_time;

		//	更新処理
		{
			DETAIL_PROFILE_CPU_SCOPE("object.update");
			for_each_parallel([elapsed_time](game_object* object)
			{
				object->update(elapsed_time);
			}, object_profile_function::update);
		}

		remove_objects();
		add_objects();

		//	更新後処理
		{
			DETAIL_PROFILE_CPU_SCOPE("object.post_update");
			for_each_parallel([elapsed_time](game_object* object)
			{
				object->post_update(elapsed_time);
			}, object_profile_function::post_update);
		}

		remove_objects();
		add_objects();

#if defined(USE_IMGUI)
		//	デバッグ更新の計測
		DETAIL_PROFILE_CPU_SCOPE("object.debug_update");

		debug_updates(elapsed_time);
#endif	//	defined(USE_IMGUI)
	}
}

#if defined(USE_IMGUI)
void object_manager::debug_updates(float elapsed_time)
{
	//	デバッグ更新
	for (auto& object : game_objects)
	{
		if (!object->is_active())
			continue;
		object->debug_pre_update(elapsed_time);
	}
	for (auto& object : game_objects)
	{
		if (!object->is_active())
			continue;
		object->debug_update(elapsed_time);
	}
	for (auto& object : game_objects)
	{
		if (!object->is_active())
			continue;
		object->debug_post_update(elapsed_time);
	}
}
#endif	//	defined(USE_IMGUI)

void object_manager::for_each_parallel(const std::function<void(game_object*)>& body, object_profile_function function)
{
	const std::size_t count = game_objects.size();
	if (count == 0)
		return;

	constexpr std::size_t grain = 8;

	object_update_executor.parallel_for(count, grain,
		[this, &body, function](std::size_t, std::size_t begin, std::size_t end)
		{
			for (std::size_t index = begin; index < end; ++index)
			{
				game_object* object = game_objects[index].get();
				if (!object->is_active())
					continue;
				DETAIL_PROFILE_OBJECT_SCOPE(function, object);
				body(object);
			}
		});
}

void object_manager::pre_render()
{
	remove_objects();
	add_objects();

	//	描画前処理
	{
		DETAIL_PROFILE_CPU_SCOPE("object.pre_render");
		for_each_parallel([](game_object* object)
		{
			object->pre_render();
		}, object_profile_function::pre_render);
	}

	remove_objects();
	add_objects();
}

void object_manager::post_render()
{
	remove_objects();
	add_objects();

	//	描画前処理
	{
		DETAIL_PROFILE_CPU_SCOPE("object.post_render");
		for (auto& object : game_objects)
		{
			if (!object->is_active())
				continue;

			DETAIL_PROFILE_OBJECT_SCOPE(object_profile_function::post_render, object.get());
			object->post_render();
		}
	}

	remove_objects();
	add_objects();
}

void object_manager::finalize()
{
	for (auto& object : game_objects)
	{
		object->on_finalize();
	}
	system_utility::std_vector_swap_clear(game_objects);
	for (auto& object : game_object_add)
	{
		object->on_finalize();
	}
	system_utility::std_vector_swap_clear(game_object_add);
	for (auto& object : game_object_removes)
	{
		object->on_finalize();
	}
	system_utility::std_vector_swap_clear(game_object_removes);
}

#if defined(USE_IMGUI)
void object_manager::imgui_render()
{
	//	アウトライナー
	if (ImGui::Begin("outliner"))
	{
		//	ドラッグ＆ドロップで決まった親子の付け替え（parent が空なら親から外す）
		game_object* pending_child = nullptr;
		game_object* pending_parent = nullptr;
		bool pending = false;

		//	一時オブジェクトでない spatial_object だけ、ドラッグして親子を付け替えられる
		auto make_drop = [&](game_object* object) -> std::function<void(game_object&)>
		{
			if (object->is_transient() || !object->check_type(spatial_object::type_name()))
				return nullptr;
			return [&pending_child, &pending_parent, &pending, object](game_object& child)
			{
				pending_child = &child;
				pending_parent = object;
				pending = true;
			};
		};

		std::unordered_map<const hierarchy*, game_object*> owners;
		std::unordered_map<const game_object*, const hierarchy*> nodes;
		for (auto& object : game_objects)
		{
			if (!object->check_type(spatial_object::type_name()))
				continue;
			const hierarchy* node = static_cast<spatial_object*>(object.get())->get_spatial().get();
			owners.emplace(node, object.get());
			nodes.emplace(object.get(), node);
		}

		//	子孫にオブジェクトを持つか
		std::function<bool(const hierarchy*)> has_object_children = [&](const hierarchy* node)
		{
			for (auto& child : node->get_children())
			{
				if (owners.find(child.get()) != owners.end() || has_object_children(child.get()))
					return true;
			}
			return false;
		};

		//	子孫のうち最も近いオブジェクトを描画
		std::function<void(const hierarchy*)> render_children;
		auto render_object = [&](game_object* object)
		{
			auto it = nodes.find(object);
			if (it == nodes.end())
			{
				object->imgui_render_outliner(nullptr, false, make_drop(object));
				return;
			}
			const hierarchy* node = it->second;
			object->imgui_render_outliner([&render_children, node]() { render_children(node); }, has_object_children(node), make_drop(object));
		};
		render_children = [&](const hierarchy* node)
		{
			for (auto& child : node->get_children())
			{
				auto it = owners.find(child.get());
				if (it != owners.end())
					render_object(it->second);
				else
					render_children(child.get());
			}
		};

		//	祖先にオブジェクトを持たないものをルートとして描画
		for (auto& object : game_objects)
		{
			bool is_root = true;
			auto it = nodes.find(object.get());
			if (it != nodes.end())
			{
				for (auto parent = it->second->get_parent(); parent; parent = parent->get_parent())
				{
					if (owners.find(parent.get()) != owners.end())
					{
						is_root = false;
						break;
					}
				}
			}
			if (is_root)
				render_object(object.get());
		}

		//	一覧の下の空いている所に落とすと、親から外す
		const ImVec2 rest_size(std::max(ImGui::GetContentRegionAvail().x, 1.0f), std::max(ImGui::GetContentRegionAvail().y, ImGui::GetFrameHeight()));
		ImGui::InvisibleButton("##outliner_root", rest_size);
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(game_object::outliner_payload_type))
			{
				pending_child = *static_cast<game_object* const*>(payload->Data);
				pending_parent = nullptr;
				pending = true;
			}
			ImGui::EndDragDropTarget();
		}

		//	木を並べ終えてから付け替える
		if (pending)
		{
			auto find_object = [&](const game_object* target) -> game_object_ptr
			{
				auto it = std::find_if(game_objects.begin(), game_objects.end(), [target](const game_object_ptr& object) { return object.get() == target; });
				return it != game_objects.end() ? *it : nullptr;
			};
			const game_object_ptr child = find_object(pending_child);
			const game_object_ptr parent = pending_parent ? find_object(pending_parent) : nullptr;
			if (child && (parent || !pending_parent))
			{
				if (const engine_context::attach_handler& handler = context.get_attach_handler())
					handler(child, parent);
				else
					spatial_object::attach_object(child, parent);
			}
		}
	}
	ImGui::End();

	//	インスペクター：選んでいるオブジェクトの設定（このワールドのオブジェクトだけ）
	if (ImGui::Begin("inspector"))
	{
		const std::shared_ptr<game_object> selected = context.get_selected_object();
		const bool owned = selected && std::find(game_objects.begin(), game_objects.end(), selected) != game_objects.end();
		if (owned)
			selected->imgui_render_inspector();
		else
			ImGui::TextDisabled("(no selection)");
	}
	ImGui::End();
}

bool object_manager::get_fixed_update_count() const
{
	return fixed_update_count;
}

bool object_manager::get_pause() const
{
	return pause;
}

void object_manager::set_pause(bool pause)
{
	this->pause = pause;
}

void object_manager::called_step()
{
	step = true;
}

#endif // defined(USE_IMGUI)

bool object_manager::register_object(game_object_ptr object)
{
	if (!object)
		return false;

	object->set_context(context);

	std::lock_guard<std::mutex> lock(mutex);

	auto it = std::find(game_object_removes.begin(), game_object_removes.end(), object);
	if (it != game_object_removes.end())
	{
		game_object_removes.erase(it);

		if (std::find(game_objects.begin(), game_objects.end(), object) == game_objects.end())
			game_object_add.emplace_back(object);
		return true;
	}

	if (std::find(game_object_add.begin(), game_object_add.end(), object) != game_object_add.end())
		return false;
	if (std::find(game_objects.begin(), game_objects.end(), object) != game_objects.end())
		return false;

	game_object_add.emplace_back(object);
	return true;
}

bool object_manager::unregister_object(game_object_ptr object)
{
	if (!object)
		return false;

	std::lock_guard<std::mutex> lock(mutex);

	if (std::find(game_object_removes.begin(), game_object_removes.end(), object) != game_object_removes.end())
		return false;

	auto it = std::find(game_object_add.begin(), game_object_add.end(), object);
	if (it != game_object_add.end())
	{
		game_object_add.erase(it);
		game_object_removes.emplace_back(object);
		return true;
	}

	if (std::find(game_objects.begin(), game_objects.end(), object) == game_objects.end())
		return false;

	game_object_removes.emplace_back(object);
	return true;
}

bool object_manager::register_object(collision_object_ptr object)
{
	return world.register_object(object);
}

bool object_manager::unregister_object(collision_object_ptr object)
{
	return world.unregister_object(object);
}

void object_manager::add_objects()
{
	std::vector<game_object_ptr> adds;
	while (true)
	{
		{
			std::lock_guard<std::mutex> lock(mutex);
			if (game_object_add.empty())
				break;

			adds.swap(game_object_add);
			game_objects.insert(game_objects.end(), adds.begin(), adds.end());
		}

		for (auto& object : adds)
		{
			object->on_initialize();
		}
		adds.clear();
	}
}

void object_manager::remove_objects()
{
	std::vector<game_object_ptr> removes;
	while (true)
	{
		{
			std::lock_guard<std::mutex> lock(mutex);
			if (game_object_removes.empty())
				break;

			removes.swap(game_object_removes);

			for (auto& object : removes)
			{
				auto it = std::find(game_objects.begin(), game_objects.end(), object);
				if (it != game_objects.end())
					game_objects.erase(it);
			}
		}

		for (auto& object : removes)
		{
			object->on_finalize();
		}
		removes.clear();
	}
}
