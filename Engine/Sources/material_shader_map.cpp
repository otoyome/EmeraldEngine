#include "material_shader_map.h"

#include <fstream>
#include <regex>
#include <sstream>

#include <d3dcompiler.h>

#include "custom_pipeline_state.h"

namespace
{
	//	surface ドメインのパステンプレート
	constexpr std::string_view surface_pass_template_path = "surface_pass.hlsli";

	//	VS はマテリアルシェーダーを含まないので、更新時刻・キャッシュ名・監視はテンプレートのファイルで行う
	constexpr std::string_view surface_pass_template_file_path = "Shaders/Passes/surface_pass.hlsli";

	//	頂点要件の define を受け取るヘッダー
	constexpr std::string_view model_header_path = "model.hlsli";

	//	@tool 接線・カラー・UV は常に読む(メッシュに無い要素はシェーダーで 0 になる)
	constexpr std::string_view fixed_vertex_defines =
		"#define HAS_TANGENT\n"
		"#define VERTEX_COLOR_MAX 1\n"
		"#define TEXCOORD_MAX 2\n";

	//	@tool 静的スイッチを書くためのマクロ(値はスイッチごとの define で渡す)
	constexpr std::string_view switch_macro_definitions =
		"#define HAS_TEXTURE(name) HAS_TEXTURE_##name\n"
		"#define MATERIAL_SWITCH(name) MATERIAL_SWITCH_##name\n";

	//	マテリアルシェーダーの置き場所
	constexpr std::string_view material_shader_directory = "Shaders/Materials";

	//	ステージを指定する define 名
	constexpr std::string_view surface_stage_vertex = "SURFACE_STAGE_VERTEX";
	constexpr std::string_view surface_stage_geometry = "SURFACE_STAGE_GEOMETRY";
	constexpr std::string_view surface_stage_pixel = "SURFACE_STAGE_PIXEL";

	//	surface ドメインのパスごとの設定
	struct surface_pass_template
	{
		std::string_view suffix;			//	パイプラインステート名の接尾辞
		std::string_view pass_define;		//	パスを指定する define 名
		std::string_view vs_out;			//	GS / PS の入力型(VS は model_*_vs.hlsli 側で決まる)
		std::string_view pixel_define;		//	PS で有効にするマテリアル関数の define 名
		std::string_view option_define;		//	全ステージに追加する define 名(空なら無し)
		bool hook_geometry;					//	マテリアルのジオメトリフックを使えるか
		bool optional_pixel;				//	PS を省略できるか(マスク以外は深度だけ書けばよい)
	};

	//	material_shader_map::surface_passes の順に並べる
	constexpr surface_pass_template surface_pass_templates[] =
	{
		{ "_depth",		"SURFACE_PASS_DEPTH_WRITE",		"VS_DEPTH_OUT",			"MATERIAL_SURFACE_OPACITY",	"",						true,	true },
		{ "_shadowmap",	"SURFACE_PASS_SHADOW_WRITE",	"VS_SHADOW_MAP_OUT",	"MATERIAL_SURFACE_OPACITY",	"",						true,	true },
		{ "_gbuffer",	"SURFACE_PASS_G_BUFFER",		"VS_GBUFFER_OUT",		"MATERIAL_SURFACE",			"",						true,	false },
		{ "_forward",	"SURFACE_PASS_FORWARD",			"VS_FORWARD_OUT",		"MATERIAL_SURFACE",			"FORWARD_RENDERING",	true,	false },
	};
	static_assert(std::size(surface_pass_templates) == magic_enum::enum_count<material_shader_map::surface_passes>());

	//	@tool コメントを取り除く(改行は残す)
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
	bool load_pass_shader(graphics_custom_pipeline_state& pipeline_state, std::string_view file_path, std::string source_text, const surface_pass_template& pass_template, std::string_view stage_define, bool hook_geometry)
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

		if (stage_define == surface_stage_pixel)
		{
			desc.defines.emplace_back(pass_template.pixel_define, "1");
			desc.defines.emplace_back("VS_OUT", pass_template.vs_out);
		}
		else if (stage_define == surface_stage_geometry && hook_geometry)
		{
			desc.defines.emplace_back("MATERIAL_GEOMETRY", "1");
			desc.defines.emplace_back("VS_OUT", pass_template.vs_out);
		}

		std::shared_ptr<Shader> shader;
		if (!shader_manager::instance()->load_shader<Shader>(shader, desc))
			return false;

		pipeline_state.set_shader(shader);
		return true;
	}

	//	@tool インクルードを空にする(マテリアルシェーダー本体の条件分岐だけを見たい)
	class empty_include_handler : public ID3DInclude
	{
	public:
		HRESULT __stdcall Open(D3D_INCLUDE_TYPE, LPCSTR, LPCVOID, LPCVOID* data, UINT* bytes) override
		{
			//	@tool 長さ 0 だと D3DPreprocess が失敗するので改行だけを返す
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

	//	@tool マテリアルシェーダーを GS のステージとしてプリプロセスする(インクルード先は展開しない)
	bool preprocess_geometry_stage(const std::string& shader_path, const std::string& source_text, const surface_pass_template& pass_template, std::string& out, std::string& error)
	{
		const std::string source = std::string(switch_macro_definitions) + source_text;
		const std::string pass_define(pass_template.pass_define);
		const std::string vs_out(pass_template.vs_out);
		const std::string option_define(pass_template.option_define);
		const std::string stage_define(surface_stage_geometry);

		std::vector<D3D_SHADER_MACRO> defines;
		defines.push_back({ pass_define.data(), "1" });
		defines.push_back({ stage_define.data(), "1" });
		defines.push_back({ "MATERIAL_GEOMETRY", "1" });
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

const material_shader_map::declaration* material_shader_map::find_declaration(std::string_view shader_path)
{
	entry* target = find_entry(shader_path);
	return target ? &target->shader_declaration : nullptr;
}

material_shader_map::entry* material_shader_map::find_entry(std::string_view shader_path)
{
	if (shader_path.empty())
		return nullptr;

	std::string key = std::string(shader_path);
	std::replace(key.begin(), key.end(), '\\', '/');

	std::error_code error;
	const auto write_time = std::filesystem::last_write_time(key, error);

	auto it = entries.find(key);
	if (it != entries.end())
	{
		//	ファイルが更新されていれば宣言を読み直す(生成済みのパイプラインステートは宣言が変わると使えないので破棄する)
		if (!error && it->second->write_time != write_time)
		{
			declaration latest;
			parse_declaration(it->second->shader_path, latest);
			const auto& current = it->second->shader_declaration;
			const bool changed = latest.geometry_passes != current.geometry_passes ||
								 latest.static_switches.size() != current.static_switches.size() ||
								 !std::equal(latest.static_switches.begin(), latest.static_switches.end(), current.static_switches.begin(),
									[](const static_switch_declaration& a, const static_switch_declaration& b)
									{
										return a.name == b.name && a.default_value == b.default_value && a.texture == b.texture;
									});
			if (changed)
				it->second->pipeline_states.clear();
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
	target->write_time = write_time;
	parse_declaration(target->shader_path, target->shader_declaration);
	return entries.emplace(key, std::move(target)).first->second.get();
}

void material_shader_map::parse_declaration(const std::string& shader_path, declaration& out)
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

	//	@tool 静的スイッチ:HAS_TEXTURE(テクスチャ名) / MATERIAL_SWITCH(名前) を書かれている順に集める
	static const std::regex switch_pattern(R"(\b(HAS_TEXTURE|MATERIAL_SWITCH)\s*\(\s*(\w+)\s*\))");
	const std::string code = remove_comments(source_text);
	for (auto it = std::sregex_iterator(code.begin(), code.end(), switch_pattern); it != std::sregex_iterator(); ++it)
	{
		const std::string macro = (*it)[1].str();
		const std::string argument = (*it)[2].str();

		static_switch_declaration item;
		item.name = macro + "_" + argument;
		if (macro == "HAS_TEXTURE")
			item.texture = argument;

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

	//	@tool ジオメトリ:パスごとにプリプロセスして、material_geometry() が残るパスで GS を使う
	static const std::regex geometry_pattern(R"(\bmaterial_geometry\s*\()");
	for (auto pass : magic_enum::enum_values<surface_passes>())
	{
		const surface_pass_template& pass_template = surface_pass_templates[static_cast<UINT>(pass)];
		if (!pass_template.hook_geometry)
			continue;

		std::string preprocessed;
		std::string error;
		if (!preprocess_geometry_stage(shader_path, source_text, pass_template, preprocessed, error))
		{
			out.error += "preprocess failed (" + std::string(magic_enum::enum_name(pass)) + ") : " + error + "\n";
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

std::string material_shader_map::generate_source(const entry& target, const vertex_format& vertex, bool include_material, const std::uint64_t* switch_bits)
{
	//	頂点要件 → 静的スイッチ → model.hlsli → マテリアルシェーダー → パステンプレートの順に並べる
	std::string source(fixed_vertex_defines);
	if (vertex.bone_max > 0)
		source += "#define BONE_MAX " + std::to_string(vertex.bone_max) + "\n";
	if (vertex.morph)
		source += "#define HAS_MORPH\n";
	source += switch_macro_definitions;
	if (switch_bits)
	{
		const auto& switches = target.shader_declaration.static_switches;
		for (std::size_t index = 0; index < switches.size(); ++index)
		{
			source += "#define " + switches[index].name + (((*switch_bits >> index) & 1ull) ? " 1\n" : " 0\n");
		}
	}
	source += "#include \"" + std::string(model_header_path) + "\"\n";
	if (include_material)
		source += "#include \"" + target.shader_path + "\"\n";
	source += "#include \"" + std::string(surface_pass_template_path) + "\"\n";
	return source;
}

graphics_pipeline_state_ptr material_shader_map::get_pipeline_state(const request& request)
{
	entry* target = find_entry(request.shader_path);
	if (!target || !target->shader_declaration.is_valid())
		return nullptr;

	const declaration& shader_declaration = target->shader_declaration;

	const UINT pass_index = static_cast<UINT>(request.pass);
	const surface_pass_template& pass_template = surface_pass_templates[pass_index];
	const bool use_pixel = !pass_template.optional_pixel || request.pixel;
	const std::uint64_t switch_bits = request.switch_bits & switch_mask(shader_declaration.static_switches.size());

	//	@tool ボーン数は 4 の倍数で 0～16 にそろえる
	vertex_format vertex = request.vertex;
	vertex.bone_max = std::min((vertex.bone_max + 3u) / 4u * 4u, 16u);

	const auto key = std::make_tuple(switch_bits, pass_index, use_pixel, vertex.bone_max, vertex.morph);
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
	name += "_b" + std::to_string(vertex.bone_max);
	if (vertex.morph)
		name += "_m";

	std::shared_ptr<graphics_custom_pipeline_state> pipeline_state;
	if (!pipeline_state_manager::instance()->generate_pipeline_state<graphics_custom_pipeline_state>(name, &pipeline_state))
		return nullptr;
	pipeline_state->set_domain(domains::surface);
	pipeline_state->set_flag(pipeline_state_base::flags::hidden, true);

	const bool use_hook_geometry = system_utility::is_bitflag(shader_declaration.geometry_passes, to_bit(request.pass));

	bool result = true;

	//	VS は頂点要件だけで決まるので、マテリアルシェーダーを含めずに同じ頂点要件のマテリアルと共有する
	if (!load_pass_shader<vertex_shader>(*pipeline_state, surface_pass_template_file_path, generate_source(*target, vertex, false, nullptr), pass_template, surface_stage_vertex, false))
		result = false;

	if (use_hook_geometry)
	{
		if (!load_pass_shader<geometry_shader>(*pipeline_state, target->shader_path, generate_source(*target, vertex, true, &switch_bits), pass_template, surface_stage_geometry, true))
			result = false;
	}

	if (use_pixel)
	{
		if (!load_pass_shader<pixel_shader>(*pipeline_state, target->shader_path, generate_source(*target, vertex, true, &switch_bits), pass_template, surface_stage_pixel, false))
			result = false;
	}

	if (!result)
	{
		DEBUG_ERROR_CATEGORY(logger::categorys::shader, "material_shader_map compile failed [%s]", name.data());
	}

	target->pipeline_states.emplace(key, pipeline_state);
	return pipeline_state;
}

void material_shader_map::get_parameter_pipeline_states(std::string_view shader_path, std::vector<graphics_pipeline_state_ptr>& out)
{
	entry* target = find_entry(shader_path);
	if (!target || !target->shader_declaration.is_valid())
		return;

	//	スイッチで使われなくなるテクスチャや定数も編集できるよう、全スイッチ ON の構成から取る
	request parameter_request;
	parameter_request.shader_path = target->shader_path;
	parameter_request.switch_bits = switch_mask(target->shader_declaration.static_switches.size());
	parameter_request.pixel = true;
	for (auto pass : { surface_passes::g_buffer, surface_passes::depth_write })
	{
		parameter_request.pass = pass;
		if (auto pipeline_state = get_pipeline_state(parameter_request))
			out.emplace_back(pipeline_state);
	}
}

#if defined(USE_IMGUI)
std::vector<std::string> material_shader_map::collect_shader_paths() const
{
	std::vector<std::string> paths;
	std::error_code error;
	for (auto& file : std::filesystem::recursive_directory_iterator(material_shader_directory, error))
	{
		if (!file.is_regular_file() || file.path().extension() != ".hlsl")
			continue;

		std::string path = file.path().string();
		std::replace(path.begin(), path.end(), '\\', '/');
		paths.emplace_back(path);
	}
	std::sort(paths.begin(), paths.end());
	return paths;
}
#endif	//	defined(USE_IMGUI)
