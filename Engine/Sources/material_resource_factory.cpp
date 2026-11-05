#include "material_resource_factory.h"

#include <filesystem>

#include "material_shader_map.h"

material_resource_ptr material_resource_factory::create_new(std::string_view name, material_resource::domains domain)
{
	auto material = std::make_shared<material_resource>(name);
	material->set_domain(domain);
	switch (domain)
	{
		case material_resource::domains::surface:
			material->set_shading_model(material_resource::shading_models::default_lit);
			break;
		case material_resource::domains::particle:
			material->set_shading_model(material_resource::shading_models::unlit);
			material->set_blend_mode(material_resource::blend_modes::translucent);
			material->set_alpha_clip_threshold(0.01f);
			material->set_flag(material_resource::flags::two_sided, true);
			material->set_flag(material_resource::flags::indirect_lighting_off, true);
			break;
		case material_resource::domains::deferred_decal:
			material->set_shading_model(material_resource::shading_models::default_lit);
			material->set_blend_mode(material_resource::blend_modes::mask);
			material->set_flag(material_resource::flags::indirect_lighting_off, true);
			break;
		case material_resource::domains::post_process:
			material->set_blendable_location(material_resource::blendable_locations::after_tone_mapping);
			break;
		default:
			break;
	}
	material->set_shader_path(material_shader_map::get_default_shader_path(domain));
	material->build();
	return material;
}

material_resource_ptr material_resource_factory::create_missing(const resource::asset_reference<material_resource>& reference, material_resource::domains domain)
{
	DEBUG_ERROR_CATEGORY(logger::categorys::model, "material_resource missing[%s][%s]", reference.path.c_str(), reference.uuid.to_string().c_str());

	auto material = std::make_shared<material_resource>(std::filesystem::path(reference.path).stem().string());
	material->set_domain(domain);
	material->set_shader_path(material_shader_map::get_failed_shader_path(domain));
	material->set_file_path(reference.path);
	material->inherit_uuid(reference.uuid);
	material->set_missing(true);
	material->build();
	return material;
}
