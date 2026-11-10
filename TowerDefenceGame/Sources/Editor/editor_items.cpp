#include "Editor/editor_items.h"
#include "Texture/texture_defaults.h"
#include "Texture/texture_resource_utility.h"

#if defined(USE_IMGUI)
#include <commdlg.h>
#include <random>

#include "Material/material_shader_map.h"
#include "Resource/resource_manager.h"
#include "Resource/asset_file_header.h"
#include "System/asset_browser.h"
#include "System/imgui_texture_keeper.h"

namespace
{
	constexpr std::string_view domain_texts[] =
	{
		"surface",			//	モデル
		"deferred_decal",	//	遅延デカール
		"postprocess",		//	ポストプロセス
		"user_interface",	//	UI
		"particle",			//	パーティクル
	};

	constexpr std::string_view blend_mode_texts[] =
	{
		"opaque",		//	不透明描画
		"mask",			//	不透明描画+アルファマスク
		"translucent",	//	半透明描画
	//	"addative"		//	加算合成
	};

	bool bool_item(std::string_view name, material_resource::constant_buffer::subset subset, bool* pbool)
	{
		if (subset.columns == 1)
		{
			return ImGui::Checkbox(name.data(), pbool);
		}
		const char element_text[][2] =
		{
			"x",
			"y",
			"z",
			"w",
		};
		bool change_flag = false;
		if (ImGui::TreeNode(name.data()))
		{
			for (UINT col = 0; col < subset.columns; ++col)
			{
				ImGui::PushID(static_cast<int>(reinterpret_cast<std::size_t>(pbool + col)));
				if (ImGui::Checkbox(element_text[col], &pbool[col]))
					change_flag = true;
				if (col + 1 == subset.columns)
					ImGui::SameLine();
				ImGui::PopID();
			}

			ImGui::TreePop();
		}
		return change_flag;
	}
	bool int_item(std::string_view name, material_resource::constant_buffer::subset subset, int* pint)
	{
		int limit_min = subset.editor_info.int_range.min;
		int limit_max = subset.editor_info.int_range.max;
		bool change_flag = false;
		switch (subset.columns)
		{
			case 4:
			{
				if (ImGui::DragInt4(name.data(), pint, 0.1f, limit_min, limit_max))
					change_flag = true;
				break;
			}
			case 3:
			{
				if (ImGui::DragInt3(name.data(), pint, 0.1f, limit_min, limit_max))
					change_flag = true;
				break;
			}
			case 2:
			{
				if (ImGui::DragInt2(name.data(), pint, 0.1f, limit_min, limit_max))
					change_flag = true;
				break;
			}
			default:
			{
				if (ImGui::DragInt(name.data(), pint, 0.1f, limit_min, limit_max))
					change_flag = true;
				break;
			}
		}
		return change_flag;
	}
	bool uint_item(std::string_view name, material_resource::constant_buffer::subset subset, UINT* puint)
	{
		int limit_min = std::max(subset.editor_info.uint_range.min, 0);
		int limit_max = subset.editor_info.uint_range.max;
		bool change_flag = false;
		switch (subset.columns)
		{
			case 4:
			{
				int pint[] =
				{
					static_cast<int>(puint[0]),
					static_cast<int>(puint[1]),
					static_cast<int>(puint[2]),
					static_cast<int>(puint[3])
				};
				if (ImGui::DragInt4(name.data(), pint, 0.1f, limit_min, limit_max))
				{
					puint[0] = pint[0];
					puint[1] = pint[1];
					puint[2] = pint[2];
					puint[3] = pint[3];
					change_flag = true;
				}
				break;
			}
			case 3:
			{
				int pint[] =
				{
					static_cast<int>(puint[0]),
					static_cast<int>(puint[1]),
					static_cast<int>(puint[2])
				};
				if (ImGui::DragInt3(name.data(), pint, 0.1f, limit_min, limit_max))
				{
					puint[0] = pint[0];
					puint[1] = pint[1];
					puint[2] = pint[2];
					change_flag = true;
				}
				break;
			}
			case 2:
			{
				int pint[] =
				{
					static_cast<int>(puint[0]),
					static_cast<int>(puint[1])
				};
				if (ImGui::DragInt2(name.data(), pint, 0.1f, limit_min, limit_max))
				{
					puint[0] = pint[0];
					puint[1] = pint[1];
					change_flag = true;
				}
				break;
			}
			default:
			{
				int pint[] =
				{
					static_cast<int>(puint[0])
				};
				if (ImGui::DragInt(name.data(), pint, 0.1f, limit_min, limit_max))
				{
					puint[0] = pint[0];
					change_flag = true;
				}
				break;
			}
		}
		return change_flag;
	}
	bool float_item(std::string_view name, material_resource::constant_buffer::subset subset, float* pfloat)
	{
		float limit_min = subset.editor_info.float_range.min;
		float limit_max = subset.editor_info.float_range.max;
		bool change_flag = false;
		switch (subset.columns)
		{
			case 4:
			{
				if (ImGui::DragFloat4(name.data(), pfloat, 0.1f, limit_min, limit_max))
					change_flag = true;
				break;
			}
			case 3:
			{
				if (ImGui::DragFloat3(name.data(), pfloat, 0.1f, limit_min, limit_max))
					change_flag = true;
				break;
			}
			case 2:
			{
				if (ImGui::DragFloat2(name.data(), pfloat, 0.1f, limit_min, limit_max))
					change_flag = true;
				break;
			}
			default:
			{
				if (ImGui::DragFloat(name.data(), pfloat, 0.1f, limit_min, limit_max))
					change_flag = true;
				break;
			}
		}
		return change_flag;
	}
	bool add_item_function(material_resource::constant_buffer& constant_buffer, std::string_view name, material_resource::constant_buffer::subset subset)
	{
		unsigned char* buffer_top = constant_buffer.data_buffer.data() + subset.start_offset;
		bool edit_flag = false;
		switch (subset.kind)
		{
			case material_resource::variable_kind::MatrixRows:
			case material_resource::variable_kind::MatrixColumns:
			{
				UINT array_max = subset.size / subset.stride;
				bool is_array = array_max > 1;
				bool tree_node_open = true;
				if (is_array)
					tree_node_open = ImGui::TreeNode(name.data());

				for (UINT array_count = 0; array_count < array_max && tree_node_open; ++array_count)
				{
					for (UINT row = 0; row < subset.rows; ++row)
					{
						std::string key = is_array ? ("[" + std::to_string(array_count) + "]")
										: std::string(name);

						unsigned char* top = buffer_top +
											 pipeline_state_base::get_variable_type_size(subset.kind, subset.type, 1, subset.columns) * row +
											 array_count * subset.stride;
						ImGui::PushID(static_cast<int>(reinterpret_cast<std::size_t>(top)));
						switch (subset.type)
						{
							case material_resource::variable_type::Bool:
							{
								if (bool_item(name, subset, reinterpret_cast<bool*>(top)))
								{
									constant_buffer.dirty = true;
									edit_flag = true;
								}
								break;
							}
							case material_resource::variable_type::Int:
							{
								if (int_item(name, subset, reinterpret_cast<int*>(top)))
								{
									constant_buffer.dirty = true;
									edit_flag = true;
								}
								break;
							}
							case material_resource::variable_type::UInt:
							{
								if (uint_item(name, subset, reinterpret_cast<UINT*>(top)))
								{
									constant_buffer.dirty = true;
									edit_flag = true;
								}
								break;
							}
							case material_resource::variable_type::Float:
							{
								if (float_item(name, subset, reinterpret_cast<float*>(top)))
								{
									constant_buffer.dirty = true;
									edit_flag = true;
								}
								break;
							}
						}
						ImGui::PopID();
					}
				}

				if (is_array && tree_node_open)
					ImGui::TreePop();

				break;
			}
			case material_resource::variable_kind::Vector:
			case material_resource::variable_kind::Scalar:
			{
				UINT array_max = subset.size / subset.stride;
				bool is_array = array_max > 1;
				bool tree_node_open = true;
				if (is_array)
					tree_node_open = ImGui::TreeNode(name.data());

				for (UINT array_count = 0; array_count < array_max && tree_node_open; ++array_count)
				{
					std::string key = is_array ? ("[" + std::to_string(array_count) + "]")
									: std::string(name);

					unsigned char* top = buffer_top + array_count * subset.stride;
					switch (subset.type)
					{
						case material_resource::variable_type::Bool:
						{
							if (bool_item(key, subset, reinterpret_cast<bool*>(top)))
							{
								constant_buffer.dirty = true;
								edit_flag = true;
							}
							break;
						}
						case material_resource::variable_type::Int:
						{
							if (int_item(key, subset, reinterpret_cast<int*>(top)))
							{
								constant_buffer.dirty = true;
								edit_flag = true;
							}
							break;
						}
						case material_resource::variable_type::UInt:
						{
							if (uint_item(key, subset, reinterpret_cast<UINT*>(top)))
							{
								constant_buffer.dirty = true;
								edit_flag = true;
							}
							break;
						}
						case material_resource::variable_type::Float:
						{
							if (float_item(key, subset, reinterpret_cast<float*>(top)))
							{
								constant_buffer.dirty = true;
								edit_flag = true;
							}
							break;
						}
					}
				}

				if (is_array && tree_node_open)
					ImGui::TreePop();

				break;
			}
		}
		return edit_flag;
	};
}

void editor_items::initialize()
{
	instance = std::make_unique<editor_items>();
}

void editor_items::terminate()
{
	instance.reset();
}

bool editor_items::begin_window(std::shared_ptr<context>& context, std::string_view name, bool* open, int flags)
{
	const std::string window_name = context ? std::string(name) + context->window_id : std::string(name);
	return ImGui::Begin(window_name.c_str(), open, flags);
}

std::shared_ptr<editor_items::context> editor_items::generate_context()
{
	std::random_device rd{};
	auto context_ptr = std::make_shared<context>();
	context_ptr->reset_item_id = rd();
	context_ptr->inner_item_id = context_ptr->reset_item_id;
	context_ptr->blank_texture = texture_defaults::instance()->get_black();
	context_ptr->cross_texture = texture_resource_utility::load_texture("Assets/Resources/Textures/editor/cross.etx");
	instance->context_ptrs.emplace_back(context_ptr);
	return context_ptr;
}

void editor_items::begin_contexts()
{
	for (auto& context : instance->context_ptrs)
	{
		auto context_ptr = context.lock();
		if (context_ptr)
			context_ptr->inner_item_id = context_ptr->reset_item_id;
	}
}

bool editor_items::edit_material(std::shared_ptr<context>& context, material_resource& material)
{
	bool edit_flag = false;
	float alpha_clip_threshold = material.get_alpha_clip_threshold();
	ImGui::PushID(context->get_item_id());

	//	ドメイン
	{
		auto get_domain_text = [](material_resource::domains value) -> std::string_view
		{
			const size_t index = static_cast<size_t>(value);
			return index < std::size(domain_texts) ? domain_texts[index] : magic_enum::enum_name(value);
		};
		const material_resource::domains current_domain = material.get_domain();
		if (ImGui::BeginCombo("domain", std::string(get_domain_text(current_domain)).c_str()))
		{
			for (auto value : magic_enum::enum_values<material_resource::domains>())
			{
				if (value == material_resource::domains::unknown)
					continue;
				if (ImGui::Selectable(std::string(get_domain_text(value)).c_str(), value == current_domain) && value != current_domain)
				{
					material.set_domain(value);
					edit_flag = true;
				}
			}
			ImGui::EndCombo();
		}
	}

	//	マテリアルシェーダーの選択(パスごとのパイプラインステートはシェーダーと静的パラメーターから決まる)
	//	候補はドメインのフォルダにあるマテリアルシェーダー
	if (material_shader_map::is_supported_domain(material.get_domain()))
	{
		auto shader_paths = material_shader_map::instance()->collect_shader_paths(material.get_domain());
		std::vector<const char*> combo_texts;
		combo_texts.reserve(shader_paths.size() + 1);
		combo_texts.emplace_back("[BLANK]");
		int selected_index = 0;
		for (std::size_t index = 0; index < shader_paths.size(); ++index)
		{
			combo_texts.emplace_back(shader_paths[index].data());
			if (shader_paths[index] == material.get_shader_path())
				selected_index = static_cast<int>(index) + 1;
		}
		if (ImGui::Combo("shader", &selected_index, combo_texts.data(), static_cast<int>(combo_texts.size())))
		{
			material.set_shader_path(selected_index == 0 ? std::string_view() : std::string_view(shader_paths[selected_index - 1]));
			edit_flag = true;
		}

		//	今のシェーダー(未設定ならドメインの既定)を写して、新しいマテリアルシェーダーを作る
		ImGui::SameLine();
		if (ImGui::Button("new..."))
		{
			context->new_shader_name[0] = '\0';
			ImGui::OpenPopup("##new_material_shader");
		}
		if (ImGui::BeginPopup("##new_material_shader"))
		{
			if (ImGui::IsWindowAppearing())
				ImGui::SetKeyboardFocusHere();
			bool accepted = ImGui::InputText("name", context->new_shader_name, IM_ARRAYSIZE(context->new_shader_name), ImGuiInputTextFlags_EnterReturnsTrue);

			//	フォルダの区切りなどファイル名に使えない文字は受け付けない
			const bool valid = context->new_shader_name[0] != '\0' && std::strpbrk(context->new_shader_name, "\\/:*?\"<>|.") == nullptr;
			ImGui::BeginDisabled(!valid);
			accepted |= ImGui::Button("create");
			ImGui::EndDisabled();

			if (accepted && valid)
			{
				const std::string_view source_path = material.get_shader_path().empty()
					? material_shader_map::get_default_shader_path(material.get_domain())
					: material.get_shader_path();
				const std::string name = system_utility::convert_utf8_to_shift_jis(context->new_shader_name);
				const std::string path = material_shader_map::instance()->create_shader_file(material.get_domain(), name, source_path);
				if (!path.empty())
				{
					material.set_shader_path(path);
					edit_flag = true;
				}
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}

		//	静的スイッチ
		if (auto shader_declaration = material_shader_map::instance()->find_declaration(material.get_shader_path(), material.get_domain()))
		{
			if (!shader_declaration->is_valid())
				ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%s", shader_declaration->error.data());

			for (auto& item : shader_declaration->static_switches)
			{
				bool value = material.get_static_switch(item.name);
				if (ImGui::Checkbox(item.name.data(), &value))
				{
					material.set_static_switch(item.name, value);
					edit_flag = true;
				}
			}
		}
	}

	//	ドメインごとの編集項目
	switch (material.get_domain())
	{
		case material_resource::domains::surface:
		case material_resource::domains::particle:
		{
			ImGui::Separator();

			//	シェーディングモデル設定
			{
				std::vector<std::string> row_texts;
				std::vector<char*> combo_texts;
				for (auto& name : magic_enum::enum_names<material_resource::shading_models>())
				{
					row_texts.emplace_back(name);
				}
				for (auto& name : row_texts)
				{
					combo_texts.emplace_back(name.data());
				}
				int blend_mode = static_cast<int>(material.get_shading_model());
				if (ImGui::Combo("shading_model", &blend_mode, combo_texts.data(), static_cast<int>(combo_texts.size())))
				{
					material.set_shading_model(static_cast<material_resource::shading_models>(blend_mode));
					edit_flag = true;
				}
			}

			//	ブレンドモード設定
			{
				std::vector<std::string> row_texts;
				std::vector<char*> combo_texts;
				for (auto& name : magic_enum::enum_names<material_resource::blend_modes>())
				{
					row_texts.emplace_back(name);
				}
				for (auto& name : row_texts)
				{
					combo_texts.emplace_back(name.data());
				}
				int value = static_cast<int>(material.get_blend_mode());
				if (ImGui::Combo("blend_mode", &value, combo_texts.data(), static_cast<int>(combo_texts.size())))
				{
					material.set_blend_mode(static_cast<material_resource::blend_modes>(value));
					edit_flag = true;
				}
			}

			//	ブレンドモードごとの対応
			if (material.get_blend_mode() == material_resource::blend_modes::translucent ||
				material.get_blend_mode() == material_resource::blend_modes::addative)
			{
				std::vector<std::string> row_texts;
				std::vector<char*> combo_texts;
				for (auto& name : magic_enum::enum_names<material_resource::translucency_paths>())
				{
					row_texts.emplace_back(name);
				}
				for (auto& name : row_texts)
				{
					combo_texts.emplace_back(name.data());
				}
				int value = static_cast<int>(material.get_translucency_path());
				if (ImGui::Combo("translucency_path", &value, combo_texts.data(), static_cast<int>(combo_texts.size())))
				{
					material.set_translucency_path(static_cast<material_resource::translucency_paths>(value));
					edit_flag = true;
				}
			}

			//	αクリップ値
			if (ImGui::SliderFloat("alpha_clip_threshold", &alpha_clip_threshold, 0.0f, 1.0f))
			{
				material.set_alpha_clip_threshold(alpha_clip_threshold);
				edit_flag = true;
			}

			ImGui::Separator();

			//	物理マテリアル
			if (ImGui::TreeNode("physical material"))
			{
				material_resource::physical_datas physical_data = material.get_physical_data();

				if (ImGui::DragFloat("friction", &physical_data.friction, 1.0f, 0.0f, 1000.0f))
				{
					edit_flag = true;
				}

				material.set_physical_data(physical_data);
				ImGui::TreePop();
			}

			ImGui::Separator();

			//	マテリアルフラグ
			if (ImGui::TreeNode("flags"))
			{
				for (auto data : magic_enum::enum_values<material_resource::flags>())
				{
					bool flag = material.is_flag(data);
					if (ImGui::Checkbox(magic_enum::enum_name(data).data(), &flag))
					{
						material.set_flag(data, flag);
						edit_flag = true;
					}
				}
				ImGui::TreePop();
			}
			break;
		}
		case material_resource::domains::post_process:
		{
			//	ブレンドモード設定
			{
				std::vector<std::string> row_texts;
				std::vector<char*> combo_texts;
				for (auto& name : magic_enum::enum_names<material_resource::blendable_locations>())
				{
					row_texts.emplace_back(name);
				}
				for (auto& name : row_texts)
				{
					combo_texts.emplace_back(name.data());
				}
				int blend_mode = static_cast<int>(material.get_blendable_location());
				if (ImGui::Combo("blendable_locations", &blend_mode, combo_texts.data(), static_cast<int>(combo_texts.size())))
				{
					material.set_blendable_location(static_cast<material_resource::blendable_locations>(blend_mode));
					edit_flag = true;
				}
			}
			break;
		}
		case material_resource::domains::deferred_decal:
		case material_resource::domains::user_interface:
		{
			break;
		}
		case material_resource::domains::system:
		{
			ImGui::Separator();
			for (auto path : magic_enum::enum_values<material_resource::system_paths>())
			{
				auto pipeline_state = material.get_pipeline_state(path);
				std::string pipeline_state_name = pipeline_state ? std::string(pipeline_state->get_name()) : std::string();
				if (ImGui::InputText(magic_enum::enum_name(path).data(), pipeline_state_name, ImGuiInputTextFlags_EnterReturnsTrue))
				{
					if (material.set_pipeline_state(path, pipeline_state_name))
						edit_flag = true;
				}
			}
			break;
		}
		default:
			break;
	}


	//	パラメーター
	for (auto& it : material.get_constant_buffers_for_editor())
	{
		material_resource::constant_buffer& constant_buffer = it.second;
		if (ImGui::TreeNode(it.first.data()))
		{
			for (auto& identifier : constant_buffer.identifieres)
			{
				auto& subset = constant_buffer.subsets[identifier.second];

				if (add_item_function(constant_buffer, identifier.first, subset))
				{
					edit_flag = true;
				}
			}
			ImGui::TreePop();
		}
	}

	//	リソース
	for (auto& it : material.get_shader_resources_for_editor())
	{
		material_resource::shader_resource& shader_resource = it.second;
		{
			ImGui::Text(it.first.data());
			UINT index = 0;
			for (auto& resource : shader_resource.resources)
			{
				//	テクスチャ
				{
					ImGui::PushID(context->get_item_id());
					ImGui::Text((std::string("[") + std::to_string(index) + std::string("]")).data());
					ImGui::SameLine();
					if (edit_material_texture_slot(context, resource))
						edit_flag = true;
					ImGui::PopID();
				}
				++index;
			}
		}
	}

	ImGui::PopID();
	return edit_flag;
}

bool editor_items::edit_shared_material(std::shared_ptr<context>& context, const material_resource_ptr& material)
{
	if (!material)
	{
		ImGui::TextDisabled("no material");
		return false;
	}

	bool edit_flag = false;
	ImGui::PushID(context->get_item_id());

	const std::string file_path = system_utility::convert_utf8(material->get_file_path());
	if (material->is_missing())
	{
		ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "missing : %s", file_path.c_str());
	}
	else if (file_path.empty())
	{
		ImGui::TextDisabled("not saved (saved as .emr together with its model / effect)");
	}
	else
	{
		ImGui::TextDisabled("%s", file_path.c_str());
		ImGui::TextDisabled("shared : edits affect every user of this material");
		if (ImGui::Button("save .emr"))
			resource_manager::instance()->save(material);
		ImGui::SameLine();
		if (ImGui::Button("revert"))
		{
			resource_manager::instance()->revert(*material);
			edit_flag = true;
		}
	}

	ImGui::TextDisabled("uuid : %s", material->get_uuid().to_string().c_str());

	ImGui::BeginDisabled(material->is_missing());
	if (edit_material(context, *material))
		edit_flag = true;
	ImGui::EndDisabled();

	ImGui::PopID();
	return edit_flag;
}

bool editor_items::edit_material_slot(std::shared_ptr<context>& context, material_resource_ptr& material, material_resource::domains domain)
{
	bool edit_flag = false;
	ImGui::PushID(context->get_item_id());

	//	今のマテリアルのファイル名(保存前は名前)
	std::string label = "(none)";
	if (material)
	{
		label = material->get_file_path().empty()
			? "(not saved) " + system_utility::convert_utf8(material->get_name())
			: system_utility::convert_utf8(material->get_file_path());
	}
	ImGui::Button(label.c_str(), ImVec2(-FLT_MIN, 0.0f));
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
		ImGui::SetTooltip("drop a .emr (%s) from the Asset Browser", magic_enum::enum_name(domain).data());

	if (ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(asset_browser::drag_payload_type))
		{
			const std::string file_path(static_cast<const char*>(payload->Data));
			const std::string extension = std::filesystem::path(file_path).extension().string();
			if (resource::get_asset_type_from_extension(extension) == resource::asset_types::material)
			{
				auto operation = resource_manager::instance()->load<material_resource>(file_path);
				material_resource_ptr loaded = operation && operation->is_succeeded() ? operation->get_resource() : nullptr;
				if (!loaded)
				{
					DEBUG_ERROR_CATEGORY(logger::categorys::model, "material slot load failed[%s]", file_path.c_str());
				}
				else if (loaded->get_domain() != domain)
				{
					//	ドメインが違うマテリアルはその場所で描けないので受け付けない
					DEBUG_WARNING_CATEGORY(logger::categorys::model, "material slot domain mismatch[%s] %s != %s", file_path.c_str(),
						magic_enum::enum_name(loaded->get_domain()).data(), magic_enum::enum_name(domain).data());
				}
				else if (loaded != material)
				{
					material = loaded;
					edit_flag = true;
				}
			}
		}
		ImGui::EndDragDropTarget();
	}

	ImGui::PopID();
	return edit_flag;
}
bool editor_items::edit_material_texture_slot(std::shared_ptr<context>& context, material_resource::shader_resource::resource& slot)
{
	bool edit_flag = false;

	const texture_ptr& texture = slot.get_texture();
	ImGui::Image(imgui_texture_keeper::instance()->keep(texture ? texture : context->blank_texture), icon_size_srv);
	ImGui::SameLine();

	//	テクスチャリソースパス
	std::string label = "(none)";
	if (slot.asset && slot.asset->get_file_path().empty())
		label = "(runtime, not saved)";
	else if (!slot.asset.is_empty())
		label = system_utility::convert_utf8(slot.asset.get_reference().path);
	if (slot.asset && slot.asset->is_missing())
		label = "missing : " + label;
	ImGui::Button(label.c_str(), ImVec2(-icon_size_srv.x - ImGui::GetStyle().ItemSpacing.x, 0.0f));
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
		ImGui::SetTooltip("drop a .etx from the Asset Browser");

	if (ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(asset_browser::drag_payload_type))
		{
			const std::string file_path(static_cast<const char*>(payload->Data));
			const std::string extension = std::filesystem::path(file_path).extension().string();
			if (resource::get_asset_type_from_extension(extension) == resource::asset_types::texture)
			{
				auto operation = resource_manager::instance()->load<texture_resource>(file_path);
				texture_resource_ptr loaded = operation && operation->is_succeeded() ? operation->get_resource() : nullptr;
				if (!loaded)
				{
					DEBUG_ERROR_CATEGORY(logger::categorys::model, "material texture slot load failed[%s]", file_path.c_str());
				}
				else if (loaded != slot.asset.get())
				{
					slot.asset = loaded;
					edit_flag = true;
				}
			}
		}
		ImGui::EndDragDropTarget();
	}

	//	空にする
	if (!slot.asset.is_empty())
	{
		ImGui::SameLine();
		ImGui::PushID("clear");
		if (ImGui::ImageButton("", imgui_texture_keeper::instance()->keep(context->cross_texture), icon_size_srv))
		{
			slot.asset = nullptr;
			edit_flag = true;
		}
		ImGui::PopID();
	}
	return edit_flag;
}

bool editor_items::edit_texture_item(std::shared_ptr<context>& context, texture_ptr& resource, bool* selected_file)
{
	bool edit_flag = false;
	if (selected_file)
		*selected_file = false;

	//	通常
	ImGui::Image(imgui_texture_keeper::instance()->keep(resource ? resource : context->blank_texture), icon_size_srv);

	//	アセットブラウザからテクスチャリソースをドロップして選ぶ
	if (ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(asset_browser::drag_payload_type))
		{
			const std::string file_path(static_cast<const char*>(payload->Data));
			const std::string extension = std::filesystem::path(file_path).extension().string();
			if (resource::get_asset_type_from_extension(extension) == resource::asset_types::texture)
			{
				if (auto selected_texture = texture_resource_utility::load_texture(file_path))
				{
					resource = selected_texture;
					if (selected_file)
						*selected_file = true;
					edit_flag = true;
				}
			}
		}
		ImGui::EndDragDropTarget();
	}
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
		ImGui::SetTooltip("drop a .etx from the Asset Browser");

	ImGui::SameLine();
	//	ファイル名
	{
		ImGui::Text((resource ? resource : context->blank_texture)->get_debug_name().data());
	}
	//	空選択
	if (resource)
	{
		ImGui::PushID(context->get_item_id());
		ImGui::SameLine();
		if (ImGui::ImageButton("", imgui_texture_keeper::instance()->keep(context->cross_texture), icon_size_srv))
		{
			resource = nullptr;	//	context->blank_texture;
			edit_flag = true;
		}
		ImGui::PopID();
	}
	return edit_flag;
}

bool editor_items::edit_model_resource_item(std::shared_ptr<context>& context, model_resource_ptr& resource, std::string* selected_file_path)
{
	bool edit_flag = false;
	if (ImGui::Button(resource ? resource->get_file_path().data() : "[BLANK]"))
	{
		//	ファイルのオープンダイアログシステム
		bool is_selected = false;
		std::string file_path;
		model_resource_ptr selected_model;
		if (open_file_dialog(model_file_dialog_filter, file_path) &&
			model_resource_manager::instance()->load(selected_model, file_path))
		{
			context->reserve_model_resource = resource;
			resource = selected_model;
			edit_flag = true;
		}
	}

#if 0
	ImGui::SameLine();
	//	ファイル名
	{
		if (resource)
		{
			ImGui::Text(resource->get_file_path_or_key().data());
		}
		else
		{
			ImGui::Text(context->blank_texture->get_file_path_or_key().data());
		}
	}
#endif

	//	空選択
	if (resource)
	{
		ImGui::PushID(context->get_item_id());
		ImGui::SameLine();
		if (ImGui::ImageButton("", imgui_texture_keeper::instance()->keep(context->cross_texture), icon_size_srv))
		{
			context->reserve_model_resource = resource;
			resource = nullptr;
			edit_flag = true;
		}
		ImGui::PopID();
	}

	return edit_flag;
}

bool editor_items::open_file_dialog(const TCHAR* text, std::string& out_selected_file_path)
{
	OPENFILENAME open_file_name; // 構造体
	TCHAR selected_file_path[MAX_PATH] = { 0 };
		
	ZeroMemory(&open_file_name, sizeof(open_file_name));
	open_file_name.lStructSize = sizeof(open_file_name);
	open_file_name.hwndOwner = GetForegroundWindow();
	open_file_name.lpstrFilter = text;
	open_file_name.lpstrFile = selected_file_path;
	open_file_name.nMaxFile = MAX_PATH;
	open_file_name.Flags = OFN_EXPLORER | OFN_NOCHANGEDIR | OFN_OVERWRITEPROMPT;

	bool opened = false;
	if (GetOpenFileName(&open_file_name))
	{
		std::string file_path = system_utility::convert_shift_jis(selected_file_path);
		out_selected_file_path = system_utility::convert_to_relative_path(file_path);
		opened = true;
	}
	return opened;
}

bool editor_items::open_file_dialog_multi(const TCHAR* text, std::vector<std::string>& out_selected_file_paths)
{
	out_selected_file_paths.clear();

	constexpr DWORD buffer_size = 1 << 16;
	std::vector<TCHAR> buffer(buffer_size, 0);

	OPENFILENAME open_file_name; // 構造体
	ZeroMemory(&open_file_name, sizeof(open_file_name));
	open_file_name.lStructSize = sizeof(open_file_name);
	open_file_name.hwndOwner = GetForegroundWindow();
	open_file_name.lpstrFilter = text;
	open_file_name.lpstrFile = buffer.data();
	open_file_name.nMaxFile = buffer_size;
	open_file_name.Flags = OFN_EXPLORER | OFN_NOCHANGEDIR | OFN_FILEMUSTEXIST | OFN_ALLOWMULTISELECT;

	if (!GetOpenFileName(&open_file_name))
		return false;

	const TCHAR* ptr = buffer.data();
	std::wstring directory = ptr;
	ptr += directory.size() + 1;

	if (*ptr == 0)
	{
		std::string file_path = system_utility::convert_shift_jis(directory);
		out_selected_file_paths.emplace_back(system_utility::convert_to_relative_path(file_path));
	}
	else
	{
		while (*ptr != 0)
		{
			std::wstring file_name = ptr;
			ptr += file_name.size() + 1;

			std::filesystem::path full_path = std::filesystem::path(directory) / file_name;
			std::string file_path = system_utility::convert_shift_jis(full_path.wstring());
			out_selected_file_paths.emplace_back(system_utility::convert_to_relative_path(file_path));
		}
	}

	return !out_selected_file_paths.empty();
}

bool editor_items::save_file_dialog(const TCHAR* text, std::string& out_selected_file_path, const TCHAR* default_extension)
{
	OPENFILENAME open_file_name; // 構造体
	TCHAR selected_file_path[MAX_PATH] = { 0 };

	ZeroMemory(&open_file_name, sizeof(open_file_name));
	open_file_name.lStructSize = sizeof(open_file_name);
	open_file_name.hwndOwner = GetForegroundWindow();
	open_file_name.lpstrFilter = text;
	open_file_name.lpstrFile = selected_file_path;
	open_file_name.nMaxFile = MAX_PATH;
	open_file_name.lpstrDefExt = default_extension;
	open_file_name.Flags = OFN_EXPLORER | OFN_NOCHANGEDIR | OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;

	bool opened = false;
	if (GetSaveFileName(&open_file_name))
	{
		std::string file_path = system_utility::convert_shift_jis(selected_file_path);
		out_selected_file_path = system_utility::convert_to_relative_path(file_path);
		opened = true;
	}
	return opened;
}

bool editor_items::copy_to_clipboard(const char* text)
{
	// 文字列の長さを計算しメモリ確保
	size_t len = (strlen(text) + 1) * sizeof(char);
	HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, len);
	if (!hMem)
		return false;

	// メモリに文字列を書き込む
	void* pMem = GlobalLock(hMem);
	memcpy(pMem, text, len);
	GlobalUnlock(hMem);

	// クリップボード操作
	if (OpenClipboard(nullptr))
	{
		EmptyClipboard();
		SetClipboardData(CF_TEXT, hMem);
		CloseClipboard();
	}
	else
	{
		GlobalFree(hMem); // エラー時はメモリ解放
		return false;
	}
	return true;
}

#endif	//	defined(USE_IMGUI)


