#include "System/asset_converter.h"

#if defined(USE_IMGUI)
#include <algorithm>
#include <filesystem>
#include <string>

#include "System/asset_browser.h"
#include "Model/model_resource.h"
#include "Model/model_resource_utility.h"
#include "Texture/texture_importer.h"
#include "Texture/texture_resource.h"
#include "Resource/resource_manager.h"
#include "system/logger.h"

namespace
{
	//	テクスチャ拡張子
	const std::vector<cache_string> texture_extensions = { ".png", ".dds", ".tga", ".bmp", ".jpg", ".jpeg", ".hdr" };
}

void asset_converter::register_handlers()
{
	auto converter = std::make_shared<asset_converter>();
	const std::string model_extension = std::string(".") + std::string(model_resource::extension);
	const std::string texture_extension = std::string(".") + std::string(texture_resource::extension);

	{
		asset_browser::convert_handler handler;
		handler.label = "Convert to " + model_extension;
		handler.extensions = { ".gltf", ".glb" };
		handler.output_path = [model_extension](const cache_string& file_path) { return replace_extension(file_path, model_extension); };
		handler.imgui_render_options = [converter](const cache_string&) { converter->imgui_render_gltf_options(); };
		handler.convert = [converter](const cache_string& file_path) { return converter->convert_gltf(file_path); };
		asset_browser::instance()->register_convert_handler(std::move(handler));
	}
	{
		asset_browser::convert_handler handler;
		handler.label = "Convert to " + model_extension;
		handler.extensions = { ".pmx" };
		handler.output_path = [model_extension](const cache_string& file_path) { return replace_extension(file_path, model_extension); };
		handler.imgui_render_options = [converter](const cache_string&) { converter->imgui_render_mmd_options(); };
		handler.convert = [converter](const cache_string& file_path) { return converter->convert_mmd(file_path); };
		asset_browser::instance()->register_convert_handler(std::move(handler));
	}
	{
		asset_browser::convert_handler handler;
		handler.label = "Convert to " + texture_extension;
		handler.extensions = texture_extensions;
		handler.output_path = [texture_extension](const cache_string& file_path) { return replace_extension(file_path, texture_extension); };
		handler.imgui_render_options = [converter](const cache_string&) { converter->imgui_render_texture_options(); };
		handler.convert = [converter](const cache_string& file_path) { return converter->convert_texture(file_path); };
		asset_browser::instance()->register_convert_handler(std::move(handler));
	}
}

void asset_converter::imgui_render_gltf_options()
{
	ImGui::Checkbox("compute_tangent", &gltf_option.is_compute_tangent);
	ImGui::Checkbox("populate_vertex_color", &gltf_option.is_populate_vertex_color);
	ImGui::Checkbox("normalize_weight", &gltf_option.is_normalize_weight);

	ImGui::Checkbox("change_coordinate_system", &gltf_option.is_change_coordinate_system);
	ImGui::Checkbox("change_coordinate_system_indices", &gltf_option.is_change_coordinate_system_indices);

	ImGui::Separator();
	ImGui::Checkbox("recomputed_normal", &gltf_option.is_recomputed_normal);
	ImGui::Checkbox("recomputed_normal_to_copy_tangent", &gltf_option.is_recomputed_normal_to_copy_tangent);
}

void asset_converter::imgui_render_mmd_options()
{
	ImGui::Checkbox("compute_tangent", &mmd_option.is_compute_tangent);
	ImGui::Checkbox("populate_vertex_color", &mmd_option.is_populate_vertex_color);
	ImGui::Checkbox("normalize_weight", &mmd_option.is_normalize_weight);

	ImGui::Checkbox("change_coordinate_system", &mmd_option.is_change_coordinate_system);
	ImGui::Checkbox("change_coordinate_system_indices", &mmd_option.is_change_coordinate_system_indices);

	ImGui::Separator();
	ImGui::Checkbox("recomputed_normal", &mmd_option.is_recomputed_normal);
	ImGui::Checkbox("recomputed_normal_to_copy_tangent", &mmd_option.is_recomputed_normal_to_copy_tangent);
}

bool asset_converter::convert_gltf(const cache_string& file_path)
{
	const std::string path(file_path.view());
	data_converter::gltf::convert_model_data data = gltf_option;
	data.file_path = path;

	auto resource = std::make_shared<model_resource>();
	if (!data_converter::gltf::convert(*resource, data))
		return false;
	return save_model(resource);
}

bool asset_converter::convert_mmd(const cache_string& file_path)
{
	const std::string path(file_path.view());
	data_converter::mmd::convert_model_data data = mmd_option;
	data.file_path = path;

	auto resource = std::make_shared<model_resource>();
	if (!data_converter::mmd::convert(*resource, data))
		return false;
	return save_model(resource);
}

bool asset_converter::save_model(const std::shared_ptr<model_resource>& resource)
{
	//	最適化とメッシュレット生成をしてから保存
	model_resource_utility::optimize_meshs(*resource);
	model_resource_utility::generate_meshlets(*resource);

	const std::string output_path(resource->get_file_path());
	if (!resource_manager::instance()->save_uncached(resource, output_path))
		return false;

	DEBUG_LOG_CATEGORY(logger::categorys::model, "asset_converter converted[%s]", output_path.c_str());
	return true;
}

void asset_converter::imgui_render_texture_options()
{
	ImGui::TextUnformatted("preset");
	constexpr std::pair<texture_import_settings::presets, const char*> presets[] =
	{
		{ texture_import_settings::presets::color, "color" },
		{ texture_import_settings::presets::normal, "normal" },
		{ texture_import_settings::presets::data, "data" },
	};
	for (const auto& [preset, name] : presets)
	{
		ImGui::SameLine();
		if (ImGui::RadioButton(name, texture_preset == preset))
			texture_preset = preset;
	}

	ImGui::DragInt("max_resolution (0 = source)", &texture_max_resolution, 16.0f, 0, 16384);
}

bool asset_converter::convert_texture(const cache_string& file_path)
{
	if (!texture_importer::is_valid())
		return false;

	texture_import_settings settings = texture_import_settings::make_preset(texture_preset);
	settings.max_resolution = static_cast<std::uint32_t>(std::max(texture_max_resolution, 0));
	return texture_importer::instance()->import(file_path.view(), settings) != nullptr;
}

cache_string asset_converter::replace_extension(const cache_string& file_path, std::string_view extension)
{
	const std::string_view path = file_path.view();
	const std::size_t slash = path.rfind('/');
	const std::size_t dot = path.rfind('.');
	if (dot == std::string_view::npos || (slash != std::string_view::npos && dot < slash))
		return std::string(path) + std::string(extension);
	return std::string(path.substr(0, dot)) + std::string(extension);
}
#endif	//	defined(USE_IMGUI)
