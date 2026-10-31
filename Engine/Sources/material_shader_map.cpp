#include "material_shader_map.h"

#include "custom_pipeline_state.h"

namespace
{
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
		bool fixed_geometry;				//	常にジオメトリシェーダーを使うか
	};

	//	material_shader_map::surface_passes の順に並べる
	constexpr surface_pass_template surface_pass_templates[] =
	{
		{ "_depth",		"SURFACE_PASS_DEPTH_WRITE",		"VS_DEPTH_OUT",			"MATERIAL_SURFACE_OPACITY",	"",						true,	false },
		{ "_shadowmap",	"SURFACE_PASS_SHADOW_WRITE",	"VS_SHADOW_MAP_OUT",	"MATERIAL_SURFACE_OPACITY",	"",						false,	true },
		{ "_gbuffer",	"SURFACE_PASS_G_BUFFER",		"VS_GBUFFER_OUT",		"MATERIAL_SURFACE",			"",						true,	false },
		{ "_forward",	"SURFACE_PASS_FORWARD",			"VS_FORWARD_OUT",		"MATERIAL_SURFACE",			"FORWARD_RENDERING",	true,	false },
	};
	static_assert(std::size(surface_pass_templates) == magic_enum::enum_count<material_shader_map::surface_passes>());

	//	マテリアルシェーダーをパスとステージを指定してコンパイルし、パイプラインステートに設定する
	template<class Shader>
	bool load_pass_shader(graphics_custom_pipeline_state& pipeline_state, std::string_view material_path, const surface_pass_template& pass_template, std::string_view stage_define, bool hook_geometry)
	{
		load_shader_file desc;
		desc.file_path = material_path;
		desc.compile_flags = 0;
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
}

material_shader_map::material_shader_map()
{
}

material_shader_map::~material_shader_map()
{
}

bool material_shader_map::generate(const material_shader_desc& desc)
{
	//	同名は作らせない
	for (auto& registered : entries)
	{
		if (registered->desc.name == desc.name)
		{
			if (registered->desc.file_path != desc.file_path)
			{
				DEBUG_ERROR_CATEGORY(logger::categorys::shader, "material_shader_map name conflict [%s] %s / %s", desc.name.data(), registered->desc.file_path.data(), desc.file_path.data());
				return false;
			}
			return true;
		}
	}

	if (desc.domain != domains::surface)
	{
		DEBUG_ERROR_CATEGORY(logger::categorys::shader, "material_shader_map unsupported domain [%s]", desc.name.data());
		return false;
	}

	auto& target = entries.emplace_back(std::make_unique<entry>());
	target->desc = desc;
	return generate_surface(*target);
}

bool material_shader_map::generate_surface(entry& target)
{
	pipeline_state_manager* manager = pipeline_state_manager::instance();
	const std::string& material_path = target.desc.file_path;

	bool result = true;
	for (UINT index = 0; index < static_cast<UINT>(std::size(surface_pass_templates)); ++index)
	{
		const surface_pass_template& pass_template = surface_pass_templates[index];
		const std::string name = target.desc.name + std::string(pass_template.suffix);

		const bool use_hook_geometry = system_utility::is_bitflag(target.desc.geometry_passes, to_bit(static_cast<surface_passes>(index)));
		if (use_hook_geometry && !pass_template.hook_geometry)
		{
			DEBUG_ERROR_CATEGORY(logger::categorys::shader, "material_shader_map geometry hook is not supported [%s]", name.data());
			result = false;
			continue;
		}

		std::shared_ptr<graphics_custom_pipeline_state> pipeline_state;
		if (!manager->generate_pipeline_state<graphics_custom_pipeline_state>(name, &pipeline_state))
		{
			result = false;
			continue;
		}
		pipeline_state->set_domain(target.desc.domain);
		pipeline_state->set_flag(pipeline_state_base::flags::hidden, target.desc.hidden);

		if (!load_pass_shader<vertex_shader>(*pipeline_state, material_path, pass_template, surface_stage_vertex, false))
			result = false;

		if (use_hook_geometry || pass_template.fixed_geometry)
		{
			if (!load_pass_shader<geometry_shader>(*pipeline_state, material_path, pass_template, surface_stage_geometry, use_hook_geometry))
				result = false;
		}

		if (!load_pass_shader<pixel_shader>(*pipeline_state, material_path, pass_template, surface_stage_pixel, false))
			result = false;

		target.pipeline_states.emplace_back(pipeline_state);
	}
	return result;
}
