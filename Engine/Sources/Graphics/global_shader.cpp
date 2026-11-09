#include "Graphics/global_shader.h"

#include <atomic>
#include <thread>
#include <typeinfo>
#include <unordered_map>

std::vector<global_shader_base*>& global_shader_base::get_list()
{
	static std::vector<global_shader_base*> list;
	return list;
}

global_shader_base::global_shader_base(std::string_view file_path, std::string_view entry_point, std::vector<shader_define> defines, std::vector<std::string_view> include_directries, shader_model model)
{
	file.file_path = file_path;
	file.entry_point = entry_point;
	file.defines = std::move(defines);
	file.include_directries = std::move(include_directries);
	file.model = model;
	file.compile_flags = 0;
#if defined(_DEBUG)
	system_utility::set_bitflag(file.compile_flags, shader_compile_flags::disable_optimize, true);
	system_utility::set_bitflag(file.compile_flags, shader_compile_flags::debug, true);
#endif	//	defined(_DEBUG)
	get_list().push_back(this);
}

bool global_shader_base::compile_all(std::size_t thread_count, const std::function<void(std::size_t, std::size_t)>& progress)
{
	auto& list = get_list();

	//	ìØÇ∂éwíËÇÃï®ÇÕ1âÒÇæÇØì«Ç›çûÇﬁ
	std::unordered_map<std::string, std::size_t> first_index;
	std::vector<std::size_t> unique_indices;
	std::vector<std::size_t> source_index(list.size());
	for (std::size_t index = 0; index < list.size(); ++index)
	{
		const load_shader_file& file = list[index]->file;
		std::string key = std::string(typeid(*list[index]).name()) + "|" + std::string(file.file_path) + "|" + std::string(file.entry_point);
		for (const auto& define : file.defines)
			key += "|" + std::string(define.name) + "=" + std::string(define.value);
		for (const auto& include : file.include_directries)
			key += "|I" + std::string(include);

		auto [it, inserted] = first_index.try_emplace(key, index);
		if (inserted)
			unique_indices.push_back(index);
		source_index[index] = it->second;
	}

	std::atomic<std::size_t> next{ 0 };
	std::atomic<std::size_t> done{ 0 };
	std::atomic<bool> succeeded{ true };

	auto work = [&]
	{
		for (std::size_t order = next++; order < unique_indices.size(); order = next++)
		{
			global_shader_base* target = list[unique_indices[order]];
			target->shader = target->load();
			if (!target->shader)
				succeeded = false;
			if (progress)
				progress(++done, unique_indices.size());
		}
	};

	std::vector<std::thread> workers;
	for (std::size_t count = 0; count < std::max<std::size_t>(thread_count, 1); ++count)
		workers.emplace_back(work);
	for (auto& worker : workers)
		worker.join();

	for (std::size_t index = 0; index < list.size(); ++index)
		list[index]->shader = list[source_index[index]]->shader;

	return succeeded;
}

void global_shader_base::release_all()
{
	for (auto* target : get_list())
		target->shader.reset();
}

const shader_ptr& global_shader_base::acquire() const
{
	if (!shader)
	{
		DEBUG_LOG_CATEGORY(logger::categorys::shader, "global shader is not compiled yet[%s]", file.file_path.data());
		shader = load();
	}
	return shader;
}
