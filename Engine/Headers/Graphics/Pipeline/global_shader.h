#pragma once

#include <functional>
#include <vector>

#include "Graphics/Pipeline/shader.h"

//	エンジン固有のシェーダー
//	起動時にまとめてコンパイル
class global_shader_base
{
public:
	//	登録された全グローバルシェーダー
	static std::vector<global_shader_base*>& get_list();

	//	全コンパイル
	static bool compile_all(std::size_t thread_count, const std::function<void(std::size_t, std::size_t)>& progress);

	//	全解放
	static void release_all();

public:
	std::string_view get_file_path() const { return file.file_path; }

protected:
	global_shader_base(std::string_view file_path, std::string_view entry_point, std::vector<shader_define> defines, std::vector<std::string_view> include_directries, shader_model model);
	virtual ~global_shader_base() = default;

	global_shader_base(const global_shader_base&) = delete;
	global_shader_base& operator=(const global_shader_base&) = delete;

	virtual shader_ptr load() const = 0;
	const shader_ptr& acquire() const;

protected:
	load_shader_file file;
	mutable shader_ptr shader;
};

//	グローバルシェーダー
template<class Type>
class global_shader : public global_shader_base
{
public:
	global_shader(std::string_view file_path, std::string_view entry_point = "main",
				  std::vector<shader_define> defines = {}, std::vector<std::string_view> include_directries = {},
				  shader_model model = shader_model::_5_0)
	: global_shader_base(file_path, entry_point, std::move(defines), std::move(include_directries), model)
	{
	}

	//	シェーダー取得
	std::shared_ptr<Type> get() const { return std::static_pointer_cast<Type>(acquire()); }

private:
	shader_ptr load() const override
	{
		std::shared_ptr<Type> result;
		if (!shader_manager::instance()->load_shader(result, file))
			return nullptr;
		return result;
	}
};
