#include "Material/material_shader_map.h"

#include <fstream>
#include <regex>
#include <sstream>

#include <d3dcompiler.h>

#include "Graphics/custom_pipeline_state.h"

namespace
{
	//	接線・カラー・UV は常に読む(メッシュに無い要素はシェーダーで 0 になる)
	constexpr std::string_view fixed_vertex_defines =
		"#define HAS_TANGENT\n"
		"#define VERTEX_COLOR_MAX 1\n"
		"#define TEXCOORD_MAX 2\n";

	//	静的スイッチを書くためのマクロ(値はスイッチごとの define で渡す)
	constexpr std::string_view switch_macro_definitions =
		"#define MATERIAL_SWITCH(name) MATERIAL_SWITCH_##name\n";

	//	マテリアルシェーダーの置き場所
	constexpr std::string_view material_shader_directory = "Assets/Resources/Shaders";

	//	ドメインごとの既定のマテリアルシェーダー
	constexpr std::string_view default_particle_shader_path = "Assets/Resources/Shaders/Particle/particle_default.hlsl";
	constexpr std::string_view default_decal_shader_path = "Assets/Resources/Shaders/DeferredDecal/decal_default.hlsl";
	constexpr std::string_view default_user_interface_shader_path = "Assets/Resources/Shaders/UserInterface/sprite_default.hlsl";
	constexpr std::string_view default_surface_shader_path = "Assets/Resources/Shaders/gltf.hlsl";
	constexpr std::string_view default_post_process_shader_path = "Assets/Resources/Shaders/PostProcess/color_filter_after_tone_mapping.hlsl";

	//	ドメインのパスごとの設定
	struct pass_template
	{
		std::string_view suffix;			//	パイプラインステート名の接尾辞
		std::string_view pass_define;		//	パスを指定する define 名
		std::string_view vs_out;			//	GS / PS の入力型(空なら define しない。テンプレート側で VS_OUT 構造体を定義するドメイン)
		std::string_view pixel_define;		//	PS で有効にするマテリアル関数の define 名
		std::string_view option_define;		//	全ステージに追加する define 名(空なら無し)
		bool hook_geometry;					//	マテリアルのジオメトリフックを使えるか
		bool optional_pixel;				//	PS を省略できるか(マスク以外は深度だけ書けばよい)
	};

	//	サーフェイス・テンプレート
	constexpr pass_template surface_pass_templates[] =
	{
		{ "_depth",		"SURFACE_PASS_DEPTH_WRITE",		"VS_DEPTH_OUT",			"MATERIAL_SURFACE_OPACITY",	"",						true,	true },
		{ "_shadowmap",	"SURFACE_PASS_SHADOW_WRITE",	"VS_SHADOW_MAP_OUT",	"MATERIAL_SURFACE_OPACITY",	"",						true,	true },
		{ "_gbuffer",	"SURFACE_PASS_G_BUFFER",		"VS_GBUFFER_OUT",		"MATERIAL_SURFACE",			"",						true,	false },
		{ "_forward",	"SURFACE_PASS_FORWARD",			"VS_FORWARD_OUT",		"MATERIAL_SURFACE",			"FORWARD_RENDERING",	true,	false },
	};
	static_assert(std::size(surface_pass_templates) == magic_enum::enum_count<material_shader_map::surface_passes>());

	//	パーティクル・テンプレート
	constexpr pass_template particle_pass_templates[] =
	{
		{ "_depth",		"PARTICLE_PASS_DEPTH_WRITE",	"PS_IN",	"MATERIAL_SURFACE_OPACITY",	"",	false,	true },
		{ "_gbuffer",	"PARTICLE_PASS_G_BUFFER",		"PS_IN",	"MATERIAL_SURFACE",			"",	false,	false },
		{ "_forward",	"PARTICLE_PASS_FORWARD",		"PS_IN",	"MATERIAL_SURFACE",			"",	false,	false },
	};
	static_assert(std::size(particle_pass_templates) == magic_enum::enum_count<material_shader_map::particle_passes>());

	//	遅延デカール・テンプレート
	constexpr pass_template decal_pass_templates[] =
	{
		{ "_primitive",		"DECAL_PASS_PRIMITIVE",		"",	"MATERIAL_DECAL",	"",	false,	false },
		{ "_fullscreen",	"DECAL_PASS_FULLSCREEN",	"",	"MATERIAL_DECAL",	"",	false,	false },
	};
	static_assert(std::size(decal_pass_templates) == magic_enum::enum_count<material_shader_map::decal_passes>());

	//	ポストプロセス・テンプレート
	constexpr pass_template post_process_pass_templates[] =
	{
		{ "",	"POST_PROCESS_PASS",	"",	"MATERIAL_POST_PROCESS",	"",	false,	false },
	};

	//	UIテンプレート
	constexpr pass_template user_interface_pass_templates[] =
	{
		{ "",	"USER_INTERFACE_PASS",	"",	"MATERIAL_USER_INTERFACE",	"",	false,	false },
	};

	//	ドメインごとの生成テンプレート
	struct domain_template
	{
		material_shader_map::domains domain;
		std::string_view directory;				//	マテリアルシェーダーを置くフォルダ(material_shader_directory からの相対。surface は直下)
		std::string_view template_path;			//	パステンプレート(入口で include する名前)
		std::string_view template_file_path;	//	パステンプレートのファイル(マテリアルを含まないステージの更新時刻・キャッシュ名・監視に使う)
		std::string_view header_path;			//	マテリアルシェーダーより先に include するヘッダー(空なら無し)
		std::string_view stage_vertex;			//	頂点ステージを指定
		std::string_view stage_geometry;		//	ジオメトリステージを指定
		std::string_view stage_pixel;			//	ピクセルステージを指定
		bool uses_model_vertex;					//	model.hlsli の頂点の define(接線・カラー・UV)を渡すか
		const pass_template* passes;			//	パステンプレート
		UINT pass_count;						//	パス数
	};

	constexpr domain_template domain_templates[] =
	{
		{
			material_shader_map::domains::surface, "",
			"surface_pass.hlsli", "Shaders/Passes/surface_pass.hlsli", "model.hlsli",
			"SURFACE_STAGE_VERTEX", "SURFACE_STAGE_GEOMETRY", "SURFACE_STAGE_PIXEL",
			true, surface_pass_templates, static_cast<UINT>(std::size(surface_pass_templates)),
		},
		{
			material_shader_map::domains::particle, "Particle",
			"particle_pass.hlsli", "Shaders/Passes/particle_pass.hlsli", "compute_particle_renderer.hlsli",
			"PARTICLE_STAGE_VERTEX", "PARTICLE_STAGE_GEOMETRY", "PARTICLE_STAGE_PIXEL",
			false, particle_pass_templates, static_cast<UINT>(std::size(particle_pass_templates)),
		},
		{
			material_shader_map::domains::deferred_decal, "DeferredDecal",
			"deferred_decal_pass.hlsli", "Shaders/Passes/deferred_decal_pass.hlsli", "",
			"DECAL_STAGE_VERTEX", "DECAL_STAGE_GEOMETRY", "DECAL_STAGE_PIXEL",
			false, decal_pass_templates, static_cast<UINT>(std::size(decal_pass_templates)),
		},
		{
			material_shader_map::domains::post_process, "PostProcess",
			"post_process_pass.hlsli", "Shaders/Passes/post_process_pass.hlsli", "fullscreen_quad.hlsli",
			"POST_PROCESS_STAGE_VERTEX", "POST_PROCESS_STAGE_GEOMETRY", "POST_PROCESS_STAGE_PIXEL",
			false, post_process_pass_templates, static_cast<UINT>(std::size(post_process_pass_templates)),
		},
		{
			material_shader_map::domains::user_interface, "UserInterface",
			"user_interface_pass.hlsli", "Shaders/Passes/user_interface_pass.hlsli", "sprite.hlsli",
			"USER_INTERFACE_STAGE_VERTEX", "USER_INTERFACE_STAGE_GEOMETRY", "USER_INTERFACE_STAGE_PIXEL",
			false, user_interface_pass_templates, static_cast<UINT>(std::size(user_interface_pass_templates)),
		},
	};

	const domain_template* find_domain_template(material_shader_map::domains domain)
	{
		for (auto& item : domain_templates)
		{
			if (item.domain == domain)
				return &item;
		}
		return nullptr;
	}

	//	コメントを取り除く(改行は残す)
	std::string remove_comments(const std::string& text)
	{
		std::string result;
		result.reserve(text.size());
		for (std::size_t index = 0; index < text.size(); ++index)
		{
			if (text.compare(index, 2, "//") == 0)
			{
				while (index < text.size() && text[index] != '\n')
					++index;
				if (index < text.size())
					result += '\n';
			}
			else if (text.compare(index, 2, "/*") == 0)
			{
				index += 2;
				while (index < text.size() && text.compare(index, 2, "*/") != 0)
				{
					if (text[index] == '\n')
						result += '\n';
					++index;
				}
				++index;
			}
			else
			{
				result += text[index];
			}
		}
		return result;
	}

	//	スイッチの数に応じたビットマスク
	std::uint64_t switch_mask(std::size_t count)
	{
		return count >= 64 ? ~0ull : ((1ull << count) - 1ull);
	}

	//	マテリアルシェーダーをパスとステージを指定してコンパイルし、パイプラインステートに設定する
	template<class Shader>
	bool load_pass_shader(graphics_custom_pipeline_state& pipeline_state, std::string_view file_path, std::string source_text, const domain_template& domain, const pass_template& pass_template, std::string_view stage_define, bool hook_geometry)
	{
		load_shader_file desc;
		desc.file_path = file_path;
		desc.compile_flags = 0;
		desc.source_text = std::move(source_text);
#if defined(_DEBUG)
		system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::disable_optimize, true);
		system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::debug, true);
#endif	//	defined(_DEBUG)
		desc.defines.emplace_back(pass_template.pass_define, "1");
		desc.defines.emplace_back(stage_define, "1");
		if (!pass_template.option_define.empty())
			desc.defines.emplace_back(pass_template.option_define, "1");

		if (stage_define == domain.stage_pixel)
		{
			if (!pass_template.pixel_define.empty())
				desc.defines.emplace_back(pass_template.pixel_define, "1");
			if (!pass_template.vs_out.empty())
				desc.defines.emplace_back("VS_OUT", pass_template.vs_out);
		}
		else if (stage_define == domain.stage_geometry && hook_geometry)
		{
			desc.defines.emplace_back("MATERIAL_GEOMETRY", "1");
			if (!pass_template.vs_out.empty())
				desc.defines.emplace_back("VS_OUT", pass_template.vs_out);
		}

		std::shared_ptr<Shader> shader;
		if (!shader_manager::instance()->load_shader<Shader>(shader, desc))
			return false;

		pipeline_state.set_shader(shader);
		return true;
	}

	//	インクルードを空にする
	class empty_include_handler : public ID3DInclude
	{
	public:
		HRESULT __stdcall Open(D3D_INCLUDE_TYPE, LPCSTR, LPCVOID, LPCVOID* data, UINT* bytes) override
		{
			static const char empty[] = "\n";
			*data = empty;
			*bytes = 1;
			return S_OK;
		}
		HRESULT __stdcall Close(LPCVOID) override
		{
			return S_OK;
		}
	};

	//	マテリアルシェーダーを GS のステージとしてプリプロセスする
	bool preprocess_geometry_stage(const std::string& shader_path, const std::string& source_text, const domain_template& domain, const pass_template& pass_template, std::string& out, std::string& error)
	{
		const std::string source = std::string(switch_macro_definitions) + source_text;
		const std::string pass_define(pass_template.pass_define);
		const std::string vs_out(pass_template.vs_out);
		const std::string option_define(pass_template.option_define);
		const std::string stage_define(domain.stage_geometry);

		std::vector<D3D_SHADER_MACRO> defines;
		defines.push_back({ pass_define.data(), "1" });
		defines.push_back({ stage_define.data(), "1" });
		defines.push_back({ "MATERIAL_GEOMETRY", "1" });
		if (!vs_out.empty())
			defines.push_back({ "VS_OUT", vs_out.data() });
		if (!option_define.empty())
			defines.push_back({ option_define.data(), "1" });
		defines.push_back({ nullptr, nullptr });

		empty_include_handler include_handler;
		Microsoft::WRL::ComPtr<ID3DBlob> preprocessed;
		Microsoft::WRL::ComPtr<ID3DBlob> errors;
		const HRESULT hr = D3DPreprocess(source.data(), source.size(), shader_path.data(), defines.data(), &include_handler, preprocessed.GetAddressOf(), errors.GetAddressOf());
		if (FAILED(hr) || !preprocessed)
		{
			if (errors)
				error.assign(static_cast<const char*>(errors->GetBufferPointer()), errors->GetBufferSize());
			return false;
		}

		out.assign(static_cast<const char*>(preprocessed->GetBufferPointer()), preprocessed->GetBufferSize());
		return true;
	}
}

material_shader_map::material_shader_map()
{
}

material_shader_map::~material_shader_map()
{
}

const material_shader_map::declaration* material_shader_map::find_declaration(std::string_view shader_path, domains domain)
{
	entry* target = find_entry(shader_path, domain);
	return target ? &target->shader_declaration : nullptr;
}

bool material_shader_map::is_supported_domain(domains domain)
{
	return find_domain_template(domain) != nullptr;
}

UINT material_shader_map::get_pass_count(domains domain)
{
	const domain_template* target = find_domain_template(domain);
	return target ? target->pass_count : 0;
}

std::string_view material_shader_map::get_default_shader_path(domains domain)
{
	switch (domain)
	{
		case domains::surface: return default_surface_shader_path;
		case domains::particle: return default_particle_shader_path;
		case domains::deferred_decal: return default_decal_shader_path;
		case domains::post_process: return default_post_process_shader_path;
		case domains::user_interface: return default_user_interface_shader_path;
		default: return "";
	}
}

std::string_view material_shader_map::get_failed_shader_path(domains domain)
{
	switch (domain)
	{
		case domains::surface: return "Assets/Resources/Shaders/3d_failed.hlsl";
		case domains::particle: return default_particle_shader_path;
		case domains::deferred_decal: return "Assets/Resources/Shaders/DeferredDecal/decal_failed.hlsl";
		case domains::post_process: return "Assets/Resources/Shaders/PostProcess/post_process_failed.hlsl";
		case domains::user_interface: return default_user_interface_shader_path;
		default: return "";
	}
}

std::string material_shader_map::convert_legacy_shader_path(std::string_view shader_path)
{
	constexpr std::string_view legacy_directory = "Shaders/Materials/";

	std::string path(shader_path);
	std::replace(path.begin(), path.end(), '\\', '/');
	if (!path.starts_with(legacy_directory))
		return std::string(shader_path);

	return std::string(material_shader_directory) + "/" + path.substr(legacy_directory.size());
}

material_shader_map::entry* material_shader_map::find_entry(std::string_view shader_path, domains domain)
{
	if (shader_path.empty() || !is_supported_domain(domain))
		return nullptr;

	std::string key = std::string(shader_path);
	std::replace(key.begin(), key.end(), '\\', '/');

	std::error_code error;
	const auto write_time = std::filesystem::last_write_time(key, error);

	auto it = entries.find(key);
	if (it != entries.end())
	{
		//	1つのマテリアルシェーダーは1つのドメインでしか使えない
		if (it->second->domain != domain)
		{
			DEBUG_ERROR_CATEGORY(logger::categorys::shader, "material_shader_map domain mismatch [%s]", key.data());
			return nullptr;
		}

		//	リロード
		if (!error && it->second->write_time != write_time)
		{
			declaration latest;
			parse_declaration(it->second->shader_path, domain, latest);
			const auto& current = it->second->shader_declaration;
			const bool changed = latest.geometry_passes != current.geometry_passes ||
								 latest.static_switches.size() != current.static_switches.size() ||
								 !std::equal(latest.static_switches.begin(), latest.static_switches.end(), current.static_switches.begin(),
									[](const static_switch_declaration& a, const static_switch_declaration& b)
									{
										return a.name == b.name && a.default_value == b.default_value;
									});
			if (changed)
			{
				it->second->pipeline_states.clear();
				++version;
			}
			it->second->shader_declaration = std::move(latest);
			it->second->write_time = write_time;
		}
		return it->second.get();
	}

	if (error)
	{
		DEBUG_ERROR_CATEGORY(logger::categorys::shader, "material_shader_map file not found [%s]", key.data());
		return nullptr;
	}

	auto target = std::make_unique<entry>();
	target->shader_path = key;
	target->domain = domain;
	target->write_time = write_time;
	parse_declaration(target->shader_path, domain, target->shader_declaration);
	return entries.emplace(key, std::move(target)).first->second.get();
}

void material_shader_map::parse_declaration(const std::string& shader_path, domains domain, declaration& out)
{
	out = declaration{};

	std::ifstream stream(shader_path, std::ios::binary);
	if (!stream)
	{
		out.error = "file not found";
		DEBUG_ERROR_CATEGORY(logger::categorys::shader, "material_shader_map file not found [%s]", shader_path.data());
		return;
	}
	const std::string source_text((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());

	//	静的スイッチ:MATERIAL_SWITCH(名前) を書かれている順に集める
	static const std::regex switch_pattern(R"(\b(MATERIAL_SWITCH)\s*\(\s*(\w+)\s*\))");
	const std::string code = remove_comments(source_text);
	for (auto it = std::sregex_iterator(code.begin(), code.end(), switch_pattern); it != std::sregex_iterator(); ++it)
	{
		const std::string macro = (*it)[1].str();
		const std::string argument = (*it)[2].str();

		static_switch_declaration item;
		item.name = macro + "_" + argument;

		auto found = std::find_if(out.static_switches.begin(), out.static_switches.end(),
			[&](const static_switch_declaration& current) { return current.name == item.name; });
		if (found != out.static_switches.end())
			continue;

		if (out.static_switches.size() >= static_switch_max)
		{
			out.error += "too many switches : " + item.name + "\n";
			continue;
		}
		out.static_switches.emplace_back(std::move(item));
	}

	//	ジオメトリ:パスごとにプリプロセスして、material_geometry() が残るパスで GS を使う
	static const std::regex geometry_pattern(R"(\bmaterial_geometry\s*\()");
	const domain_template& domain_setting = *find_domain_template(domain);
	for (UINT pass = 0; pass < domain_setting.pass_count; ++pass)
	{
		const pass_template& pass_template = domain_setting.passes[pass];
		if (!pass_template.hook_geometry)
			continue;

		std::string preprocessed;
		std::string error;
		if (!preprocess_geometry_stage(shader_path, source_text, domain_setting, pass_template, preprocessed, error))
		{
			out.error += "preprocess failed (" + std::string(pass_template.pass_define) + ") : " + error + "\n";
			continue;
		}
		if (std::regex_search(preprocessed, geometry_pattern))
			out.geometry_passes |= to_bit(pass);
	}

	if (!out.error.empty())
	{
		DEBUG_ERROR_CATEGORY(logger::categorys::shader, "material_shader_map declaration error [%s]\n%s", shader_path.data(), out.error.data());
	}
}

std::string material_shader_map::generate_source(const entry& target, bool include_material, const std::uint64_t* switch_bits)
{
	const domain_template& domain_setting = *find_domain_template(target.domain);

	//	頂点要件 → 静的スイッチ → model.hlsli → マテリアルシェーダー → パステンプレートの順に並べる
	std::string source;
	if (domain_setting.uses_model_vertex)
		source += fixed_vertex_defines;
	source += switch_macro_definitions;
	if (switch_bits)
	{
		const auto& switches = target.shader_declaration.static_switches;
		for (std::size_t index = 0; index < switches.size(); ++index)
		{
			source += "#define " + switches[index].name + (((*switch_bits >> index) & 1ull) ? " 1\n" : " 0\n");
		}
	}
	if (!domain_setting.header_path.empty())
		source += "#include \"" + std::string(domain_setting.header_path) + "\"\n";
	if (include_material)
		source += "#include \"" + target.shader_path + "\"\n";
	source += "#include \"" + std::string(domain_setting.template_path) + "\"\n";
	return source;
}

graphics_pipeline_state_ptr material_shader_map::get_pipeline_state(const request& request)
{
	entry* target = find_entry(request.shader_path, request.domain);
	if (!target || !target->shader_declaration.is_valid())
		return nullptr;

	const declaration& shader_declaration = target->shader_declaration;
	const domain_template& domain_setting = *find_domain_template(request.domain);
	if (request.pass >= domain_setting.pass_count)
		return nullptr;

	const UINT pass_index = request.pass;
	const pass_template& pass_template = domain_setting.passes[pass_index];
	const bool use_pixel = !pass_template.optional_pixel || request.pixel;
	const std::uint64_t switch_bits = request.switch_bits & switch_mask(shader_declaration.static_switches.size());

	const auto key = std::make_tuple(switch_bits, pass_index, use_pixel);
	auto it = target->pipeline_states.find(key);
	if (it != target->pipeline_states.end())
		return it->second;

	//	名前はパス・PS の有無・スイッチから決める(同じキーなら同じ名前)
	std::string name = target->shader_path + std::string(pass_template.suffix);
	if (!use_pixel)
		name += "_no_pixel";
	if (switch_bits != 0)
	{
		std::ostringstream hex;
		hex << std::hex << switch_bits;
		name += "_" + hex.str();
	}

	std::shared_ptr<graphics_custom_pipeline_state> pipeline_state;
	if (!pipeline_state_manager::instance()->generate_pipeline_state<graphics_custom_pipeline_state>(name, &pipeline_state))
		return nullptr;
	pipeline_state->set_domain(request.domain);
	pipeline_state->set_flag(pipeline_state_base::flags::hidden, true);

	const bool use_hook_geometry = system_utility::is_bitflag(shader_declaration.geometry_passes, to_bit(pass_index));

	bool result = true;

	//	VS は頂点要件だけで決まるので、マテリアルシェーダーを含めずに同じ頂点要件のマテリアルと共有する
	if (!load_pass_shader<vertex_shader>(*pipeline_state, domain_setting.template_file_path, generate_source(*target, false, nullptr), domain_setting, pass_template, domain_setting.stage_vertex, false))
		result = false;

	if (use_hook_geometry)
	{
		if (!load_pass_shader<geometry_shader>(*pipeline_state, target->shader_path, generate_source(*target, true, &switch_bits), domain_setting, pass_template, domain_setting.stage_geometry, true))
			result = false;
	}

	if (use_pixel)
	{
		if (!load_pass_shader<pixel_shader>(*pipeline_state, target->shader_path, generate_source(*target, true, &switch_bits), domain_setting, pass_template, domain_setting.stage_pixel, false))
			result = false;
	}

	if (!result)
	{
		DEBUG_ERROR_CATEGORY(logger::categorys::shader, "material_shader_map compile failed [%s]", name.data());
	}

	target->pipeline_states.emplace(key, pipeline_state);
	return pipeline_state;
}

graphics_pipeline_state_ptr material_shader_map::get_failed_pipeline_state(domains domain, UINT pass)
{
	request failed_request;
	failed_request.shader_path = get_failed_shader_path(domain);
	failed_request.domain = domain;
	failed_request.pass = pass;
	failed_request.pixel = domain == domains::particle;
	return get_pipeline_state(failed_request);
}

void material_shader_map::get_parameter_pipeline_states(std::string_view shader_path, domains domain, std::vector<graphics_pipeline_state_ptr>& out)
{
	entry* target = find_entry(shader_path, domain);
	if (!target || !target->shader_declaration.is_valid())
		return;

	//	スイッチで使われなくなるテクスチャや定数も編集できるよう、全スイッチ ON の構成から取る
	request parameter_request;
	parameter_request.shader_path = target->shader_path;
	parameter_request.domain = domain;
	parameter_request.switch_bits = switch_mask(target->shader_declaration.static_switches.size());
	parameter_request.pixel = true;

	//	surface / particle は G-Buffer と深度、それ以外は先頭のパスから取る
	std::vector<UINT> passes;
	if (domain == domains::surface)
		passes = { static_cast<UINT>(surface_passes::g_buffer), static_cast<UINT>(surface_passes::depth_write) };
	else if (domain == domains::particle)
		passes = { static_cast<UINT>(particle_passes::g_buffer), static_cast<UINT>(particle_passes::depth_write) };
	else
		passes = { 0 };

	for (auto pass : passes)
	{
		parameter_request.pass = pass;
		if (auto pipeline_state = get_pipeline_state(parameter_request))
			out.emplace_back(pipeline_state);
	}
}

#if defined(USE_IMGUI)
std::vector<std::string> material_shader_map::collect_shader_paths(domains domain) const
{
	std::vector<std::string> paths;
	const domain_template* domain_setting = find_domain_template(domain);
	if (!domain_setting)
		return paths;

	auto add_path = [&](const std::filesystem::directory_entry& file)
	{
		if (!file.is_regular_file() || file.path().extension() != ".hlsl")
			return;

		std::string path = file.path().string();
		std::replace(path.begin(), path.end(), '\\', '/');
		paths.emplace_back(path);
	};

	//	surface は直下のファイルだけ、それ以外はドメインのフォルダ以下
	std::error_code error;
	if (domain_setting->directory.empty())
	{
		for (auto& file : std::filesystem::directory_iterator(material_shader_directory, error))
			add_path(file);
	}
	else
	{
		const std::filesystem::path directory = std::filesystem::path(material_shader_directory) / domain_setting->directory;
		for (auto& file : std::filesystem::recursive_directory_iterator(directory, error))
			add_path(file);
	}
	std::sort(paths.begin(), paths.end());
	return paths;
}

std::string material_shader_map::create_shader_file(domains domain, std::string_view name, std::string_view source_path) const
{
	const domain_template* domain_setting = find_domain_template(domain);
	if (!domain_setting || name.empty() || source_path.empty())
		return {};

	//	collect_shader_paths の候補に出るフォルダに置く
	std::filesystem::path directory(material_shader_directory);
	if (!domain_setting->directory.empty())
		directory /= domain_setting->directory;

	std::filesystem::path destination = directory / std::filesystem::path(std::string(name) + ".hlsl");
	std::error_code error;
	if (std::filesystem::exists(destination, error))
	{
		DEBUG_WARNING_CATEGORY(logger::categorys::shader, "material_shader_map create shader already exists[%s]", destination.string().c_str());
		return {};
	}

	std::filesystem::copy_file(std::filesystem::path(std::string(source_path)), destination, error);
	if (error)
	{
		DEBUG_ERROR_CATEGORY(logger::categorys::shader, "material_shader_map create shader failed[%s] -> [%s] %s", std::string(source_path).c_str(), destination.string().c_str(), error.message().c_str());
		return {};
	}

	std::string path = destination.string();
	std::replace(path.begin(), path.end(), '\\', '/');
	DEBUG_LOG_CATEGORY(logger::categorys::shader, "material_shader_map created shader[%s]", path.c_str());
	return path;
}
#endif	//	defined(USE_IMGUI)
