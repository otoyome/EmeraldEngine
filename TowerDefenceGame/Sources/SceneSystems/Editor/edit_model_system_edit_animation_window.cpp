#include "SceneSystems/Editor/edit_model_system.h"
#include "Object/engine_context.h"

#include "Model/model_instance.h"
#include "Graphics/View/camera.h"

#include "object/light_object.h"

#include "object/model_object.h"

#include "Graphics/Pipeline/custom_pipeline_state.h"

#include "Resource/resource_manager.h"

#if defined(USE_IMGUI)
#include "Editor/editor_camera.h"

namespace
{
	constexpr const TCHAR* AnimationFileDialogFilter =
		L"モデルファイル (*.vmd, *.gltf, *.glb)\0*.vmd;*.gltf;*.glb;\0\
		すべてのファイル (*.*)\0*.*\0";

	//	リターゲティング用ファイルパス
	constexpr std::string_view RetargetFilePath = "Assets/Editor/RetargetFile.rfo";

	void is_item_hover_and_active(bool& is_active)
	{
		is_active = is_active || ImGui::IsItemHovered() || ImGui::IsItemActive() || ImGui::IsItemEdited();
	}

}

bool edit_model_system::initialize_animation()
{
	//	リターゲティング用情報読込
	retarget_data.deserialize(RetargetFilePath);

	return true;
}

bool edit_model_system::uninitialize_animation()
{
	//	リターゲティング用情報読込
	retarget_data.serialize(RetargetFilePath);

	return true;
}

void edit_model_system::update_animation()
{
}

void edit_model_system::imgui_render_animation()
{
	if (!edit_model)
		return;

	bool is_delete = false;
	//	アニメーションプレイヤー
	int current_animation_id = static_cast<int>(edit_model->get_animation_index());
	auto animation_names = edit_model->get_animation_names();
	{
		int animation_id = current_animation_id;
		int animation_max = static_cast<int>(edit_model->get_animation_max());
		float animation_end_time = edit_model->get_animation_end_time();
		bool animation_loop = edit_model->get_animation_loop();
		bool animation_playing = edit_model->get_animation_speed() > 0.5f;
		float animation_time = edit_model->get_animation_time();

		//	アニメーションリスト
		ImGui::Combo("animation", animation_id, animation_names);
		//ImGui::SliderInt("animation", &animation_id, 0, animation_names.size() - 1);

		//	追加/削除
		ImGui::SameLine();
		if (ImGui::Button("Add"))
		{
			std::string file_path;
			if (editor_items::open_file_dialog(AnimationFileDialogFilter, file_path))
			{
				std::filesystem::path path(file_path);
				std::filesystem::path extension = path.extension();

				animation_resource animations;

				auto resource = edit_model->get_model_instance()->get_resource_for_editor();
				if (extension == std::filesystem::path(".vmd"))
				{
					//	MMD用
					data_converter::mmd::convert_animation_data data;
					data.file_path = file_path;
					data.retarget_resource = resource;
					//	アニメーション変換
					data_converter::mmd::convert(animations, data);
				}
				else if (extension == std::filesystem::path(".gltf") || extension == std::filesystem::path(".glb"))
				{
					data_converter::gltf::convert_animation_data data;
					data.file_path = file_path;
					//	リターゲッティング情報登録
					auto it = retarget_data.profiles.find(std::string(resource->get_file_path()));
					if (it != retarget_data.profiles.end())
					{
						data.retarget_profile = it->second[selected_retarget_data_index];
						data.retarget_resource = resource;
					}
					//	アニメーション変換
					data_converter::gltf::convert(animations, data);
				}
				else
				{
					assert(!L"Compatible Format Model.");
					load_model_path.clear();
				}
				//	アニメーション登録
				//	同名アニメーションは上書き
				auto& animation_list = resource->get_animations_for_editor();
				for (auto& animation : animations.get_animation())
				{
					bool find = false;
					for (auto& animation_data : animation_list)
					{
						if (animation.name == animation_data.name)
						{
							animation_data = animation;
							find = true;
							break;
						}
					}
					if (!find)
					{
						animation_list.push_back(animation);
					}
				}
			}
		}
		if (animation_id != 0)
		{
			ImGui::SameLine();
			if (ImGui::Button("delete"))
				is_delete = true;
		}

		//	操作系
		ImGui::Checkbox("loop", &animation_loop);
		ImGui::SameLine();
		if (ImGui::Button(animation_playing ? "stop" : "play"))
		{
			animation_playing = !animation_playing;
		}
		ImGui::SameLine();
		if (ImGui::Button(display_bone ? "hide bone" : "display bone"))
		{
			display_bone = !display_bone;
		}

		ImGui::SliderFloat("animation time", &animation_time, 0.0f, animation_end_time + 0.001f, "%.6f");
		ImGui::Separator();
		//	編集モデルのアニメーション変更
		if (current_animation_id != animation_id)
		{
			edit_model->play_animation(animation_id, animation_loop, 0.0f);
		}
		else
		{
			edit_model->set_animation_loop(animation_loop);
			edit_model->set_animation_time(animation_time);
		}
		edit_model->set_animation_speed(animation_playing ? 1.0f : 0.0f);
	}

	//	アニメーション情報編集
	if (current_animation_id > 0 && current_animation_id < animation_names.size())
	{
		auto& animation_datas = edit_model->get_model_instance()->get_resource()->get_animations_for_editor();

		//	アニメーション情報編集
		editor_items::edit_animation_data(editor_context, animation_datas[current_animation_id]);
	}

	if (is_delete)
	{
		auto& animation_datas = edit_model->get_model_instance()->get_resource()->get_animations_for_editor();
		animation_datas.erase(animation_datas.begin() + current_animation_id);
		if (current_animation_id >= animation_names.size())
			edit_model->play_animation(animation_names.size(), edit_model->get_animation_loop(), 0.0f);
	}

	//	編集モデルのボーン表示
	if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) && display_bone)
	{
		editor_items::render_model_bones(get_context(), *edit_model->get_model_instance(), selected_bones);
	}
}

bool edit_model_system::has_editable_animation() const
{
	if (!edit_model)
		return false;

	const size_t current_animation_id = edit_model->get_animation_index();
	return current_animation_id > 0 && current_animation_id < edit_model->get_animation_names().size();
}

void edit_model_system::imgui_render_animation_timeline()
{
	if (!has_editable_animation())
		return;

	auto& animation_datas = edit_model->get_model_instance()->get_resource()->get_animations_for_editor();
	const size_t current_animation_id = edit_model->get_animation_index();
	if (current_animation_id >= animation_datas.size())
		return;

	//	左にタイムライン、右に選んだキーの詳細
	if (ImGui::BeginTable("##model_bottom", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV))
	{
		ImGui::TableSetupColumn("##timeline", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("##inspector", ImGuiTableColumnFlags_WidthFixed, 320.0f);
		ImGui::TableNextRow();

		//	アニメーションタイムライン情報編集
		//	ドラッグで選ぶ時に窓が動かないようにする
		ImGui::TableSetColumnIndex(0);
		if (ImGui::BeginChild("##timeline", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None, ImGuiWindowFlags_NoMove))
			editor_items::edit_animation_data_timeline_embedded(editor_context, animation_datas[current_animation_id]);
		ImGui::EndChild();

		ImGui::TableSetColumnIndex(1);
		if (ImGui::BeginChild("##inspector", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None, ImGuiWindowFlags_NoMove))
			editor_items::edit_animation_timeline_inspector(editor_context);
		ImGui::EndChild();

		ImGui::EndTable();
	}
}

void edit_model_system::imgui_render_ik()
{
	//	IKリソース生成ボタン
	if (!edit_model->get_ik_resource())
	{
		if (ImGui::Button("Generate"))
		{
			auto ik_resource = std::make_shared<::ik_resource>();
			auto file_path = edit_model->get_model_instance()->get_resource()->get_file_path();
			std::filesystem::path path = file_path;
			path.replace_extension(ik_resource::extension);
			resource_manager::instance()->save(ik_resource, path.string());
			edit_model->set_ik_resource(ik_resource);
		}
	}
	else
	{
		if (ImGui::Button("Save"))
		{
			resource_manager::instance()->save(edit_model->get_ik_resource());
		}
		ImGui::SameLine();
		if (ImGui::Button("Load"))
		{
			auto file_path = edit_model->get_model_instance()->get_resource()->get_file_path();
			std::filesystem::path path = file_path;
			path.replace_extension(ik_resource::extension);
			auto operation = resource_manager::instance()->load<ik_resource>(path.string(), true);
			if (operation->is_succeeded())
			{
				edit_model->set_ik_resource(operation->get_resource());
			}
		}

		//	IK情報ウィンドウ
		auto model_instance = edit_model->get_model_instance();
		auto ik_resource = edit_model->get_ik_resource();
		editor_items::edit_ik(editor_context, get_context(), model_instance, ik_resource);
	}
}

void edit_model_system::imgui_retarget_window()
{
	if (!edit_model)
		return;

	std::string file_name = std::string(edit_model->get_model_instance()->get_resource()->get_file_path());
	auto it = retarget_data.profiles.find(file_name);
	//	選択リスト
	{
		int max_index = static_cast<int>(it->second.size());
		int index = static_cast<int>(selected_retarget_data_index);
		ImGui::SliderInt("index", &index, 0, max_index - 1);
		selected_retarget_data_index = static_cast<UINT>(index);
		ImGui::SameLine();
		if (ImGui::Button("+"))
		{
			retarget_profile profile;
			profile.bone_map.clear();
			auto& node_list = edit_model->get_model_instance()->get_nodes();
			auto& mesh_list = edit_model->get_model_instance()->get_meshes();
			for (auto& mesh : mesh_list)
			{
				for (auto& bone : mesh.mesh->bones)
				{
					auto& bone_list = profile.bone_map;

					auto node_name = node_list[bone.node_index].name;
					if (bone_list.find(node_name) == bone_list.end())
					{
						retarget_profile::bone_data data;
						data.target_name = "";
						profile.bone_map.insert(std::make_pair(node_name, data));
					}
				}
			}
			it->second.emplace_back(profile);
		}
		if (it->second.size() > 1)
		{
			ImGui::SameLine();
			if (ImGui::Button("-"))
			{
				it->second.erase(it->second.begin() + selected_retarget_data_index);
				selected_retarget_data_index = std::min(selected_retarget_data_index, it->second.size() - 1);
			}
		}
	}
	auto& retarget_page = it->second[selected_retarget_data_index];
	//	ベーススケーリング
	ImGui::DragFloat3("base_scaling", &retarget_page.base_scaling.x, 0.001f, 0.0f, 1000.0f);

	ImGui::Separator();
	//	ボーン選択
	selected_bones.clear();
	for (auto& bone_data : retarget_page.bone_map)
	{
		//	名前のないボーンは無視
		if (bone_data.first.empty())
			continue;

		ImGui::PushID(bone_data.first.data());
		bool is_active = false;
		if (ImGui::TreeNode(bone_data.first.data()))
		{
			is_item_hover_and_active(is_active);
			ImGui::InputTextUtf8("target_node", bone_data.second.target_name); is_item_hover_and_active(is_active);
			ImGui::DragFloat3("scale", &bone_data.second.scale.x); is_item_hover_and_active(is_active);
			ImGui::DragFloat4("rotation", &bone_data.second.rotation.x); is_item_hover_and_active(is_active);
			ImGui::DragFloat3("translation", &bone_data.second.translation.x); is_item_hover_and_active(is_active);
			ImGui::Checkbox("lock_scale", &bone_data.second.is_lock_scale); is_item_hover_and_active(is_active);
			ImGui::SameLine();
			ImGui::Checkbox("lock_rotation", &bone_data.second.is_lock_rotation); is_item_hover_and_active(is_active);
			ImGui::SameLine();
			ImGui::Checkbox("lock_translation", &bone_data.second.is_lock_translation); is_item_hover_and_active(is_active);
			ImGui::TreePop();
		}
		if (is_active)
		{
			selected_bones.push_back(bone_data.first);
		}
		ImGui::Separator();
		ImGui::PopID();
	}

	//	編集モデルのボーン表示
	if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows))
	{
		editor_items::render_model_bones(get_context(), *edit_model->get_model_instance(), selected_bones);
	}
}

#endif	//	defined(USE_IMGUI)
