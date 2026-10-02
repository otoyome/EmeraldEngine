#include "Object/object_manager.h"

namespace
{
	static constexpr float fixed_per_second = system_units::convert_fps_to_sec(1.0f);
}

object_manager::object_manager()
: game_objects()
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
			for_each_parallel([](game_object* object)
			{
				object->fixed_update(fixed_per_second);
			});

			remove_objects();
			add_objects();

			//	当たり判定
		#if 01	//	defined(_DEBUG)
			world.step(fixed_per_second, 4, system_units::convert_fps_to_sec(0.25f));
		#else	//	defined(_DEBUG)
			world.step(fixed_per_second);
		#endif	//	defined(_DEBUG)

			temporary_elapsed_time -= fixed_per_second;
		}
		remain_time = temporary_elapsed_time;

		//	更新処理
		for_each_parallel([elapsed_time](game_object* object)
		{
			object->update(elapsed_time);
		});

		remove_objects();
		add_objects();

		//	更新後処理
		for_each_parallel([elapsed_time](game_object* object)
		{
			object->post_update(elapsed_time);
		});

		remove_objects();
		add_objects();

#if defined(USE_IMGUI)
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
#endif	//	defined(USE_IMGUI)
	}
}

void object_manager::for_each_parallel(const std::function<void(game_object*)>& body)
{
	const std::size_t count = game_objects.size();
	if (count == 0)
		return;

	constexpr std::size_t grain = 8;

	object_update_executor.parallel_for(count, grain,
		[this, &body](std::size_t, std::size_t begin, std::size_t end)
		{
			for (std::size_t index = begin; index < end; ++index)
			{
				game_object* object = game_objects[index].get();
				if (!object->is_active())
					continue;
				body(object);
			}
		});
}

void object_manager::pre_render()
{
	remove_objects();
	add_objects();

	//	描画前処理
	for_each_parallel([](game_object* object)
	{
		object->pre_render();
	});

	remove_objects();
	add_objects();
}

void object_manager::post_render()
{
	remove_objects();
	add_objects();

	//	描画前処理
	for (auto& object : game_objects)
	{
		if (!object->is_active())
			continue;

		object->post_render();
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
	if (ImGui::Begin("objects"))
	{
		for (auto& object : game_objects)
		{
			object->imgui_render();
		}
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
