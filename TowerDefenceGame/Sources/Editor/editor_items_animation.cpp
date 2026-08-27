#include "Editor/editor_items.h"

#if defined(USE_IMGUI)
#include <commdlg.h>
#include <random>
#include <set>

namespace
{
	//	アニメーション補間編集
	class animation_interpolation_editor : public ImCurveEdit::Delegate, public editor_items::ibase_editor
	{
	public:
		animation_interpolation_editor()
		: ImCurveEdit::Delegate()
		, editor_items::ibase_editor()
		{
			visible = true;
			range_max = ImVec2(0.0f, 1.0f);
			range_min = ImVec2(0.0f, 0.0f);
			handle_lists[0].resize(10000);
			handle_lists[1].resize(10000);
		}
		~animation_interpolation_editor() override
		{
			selection_lists.clear();
			handle_lists[0].clear();
			handle_lists[1].clear();
			point_list.clear();
		}

		void set_data(animation::data* data)
		{
			animation_data = data;

			point_list.clear();
			if (animation_data == nullptr)
				return;

			range_max = ImVec2(0.0f, 1.0f);
			range_min = ImVec2(0.0f, 0.0f);

			for (auto& d : animation_data->time_curve.datas)
			{
				point_list.emplace_back(d.x, d.y);
			}
		}

		void render(std::shared_ptr<editor_items::context>& context) override
		{
			if (animation_data == nullptr)
				return;

			ImVec2 window_size = ImGui::GetWindowSize();

			//	補間モード設定
			{
				std::vector<const char*> texts;
				for (auto& name : magic_enum::enum_names<animation::interpolation::types>())
				{
					texts.emplace_back(name.data());
				}
				int type = static_cast<int>(animation_data->time_curve.type);
				if (ImGui::Combo("Curve Type", &type, texts.data(), static_cast<int>(texts.size())))
				{
					animation_data->time_curve.type = static_cast<animation::interpolation::types>(type);
				}
			}

			//	リセットボタン
			ImGui::SameLine();
			if (ImGui::Button("Reset##curve_reset"))
			{
				animation_data->time_curve.datas.clear();
				animation_data->time_curve.datas.emplace_back(0.0f, 0.0f);
				animation_data->time_curve.datas.emplace_back(1.0f, 1.0f);
			}

			//	カーブ編集
			ImVector<ImCurveEdit::EditPoint> selected_points;
			//DirectX::XMFLOAT4 range = { range_min.x, range_min.y, range_max.x, range_max.y };
			//ImGui::SliderFloat4(u8"Time Range", &range.x, 0.0f, 1.0f);
			int changed = ImCurveEdit::Edit(*this, ImVec2(window_size.x - 40, 300), 0xFFFF, nullptr, &selected_points, &selection_lists);
			if (!selected_points.empty())
			{
				//	とりあえず右クリックしてたら破棄
				if (ImGui::IsMouseDown(ImGuiMouseButton_Right))
				{
					//	ポイントリストから破棄
					auto& time_curve_data = animation_data->time_curve.datas;
					for (auto& selected_point : selected_points)
					{
						auto it = time_curve_data.begin() + selected_point.pointIndex;
						if (it != time_curve_data.begin() && it != (time_curve_data.end() - 1))
						{
							animation_data->time_curve.datas.erase(it);
						}
					}

					//	選択リストを破棄
					selection_lists.clear();
				}
			}

			//	クランプ
			for (auto& data : animation_data->time_curve.datas)
			{
				data = math::saturate(data);
			}
			ImGui::Text(u8"    アニメーション時間");
		}

	private:
		size_t GetCurveCount() override { return 1; }

		bool IsVisible(size_t curve_index) override
		{
			return visible;
		}
		size_t GetPointCount(size_t curve_index) override
		{
			return point_list.size();
		}

		uint32_t GetCurveColor(size_t curve_index) override
		{
			uint32_t cols[] = { 0xFF0000FF, 0xFF00FF00, 0xFFFF0000 };
			return cols[curve_index];
		}

		ImVec2* GetPoints(size_t curve_index) override
		{
			return point_list.data();
		}

		ImCurveEdit::CurveType GetCurveType(size_t curve_index) const override
		{
			if (animation_data == nullptr)
				return ImCurveEdit::CurveSmooth;

			return static_cast<ImCurveEdit::CurveType>(animation_data->time_curve.type);
		}

		int EditPoint(size_t curve_index, int point_index, ImVec2 value) override
		{
			point_list[point_index] = ImVec2(value.x, value.y);
			set_values(curve_index);
			for (size_t index = 0; index < GetPointCount(curve_index); ++index)
			{
				if (GetPoints(curve_index)[index].x == value.x)
					return static_cast<int>(index);
			}
			return point_index;
		}

		void AddPoint(size_t curve_index, ImVec2 value) override
		{
			point_list.emplace_back(value);
			set_values(curve_index);
		}

		ImVec2& GetMax() override
		{
			return range_max;
		}

		ImVec2& GetMin() override
		{
			return range_min;
		}

		UINT GetBackgroundColor() override
		{
			return 0x00000000;
		}

		void set_values(size_t curve_index)
		{
			std::sort(point_list.begin(), point_list.end(), [](ImVec2 a, ImVec2 b) { return a.x < b.x; });
			if (animation_data == nullptr)
				return;

			animation_data->time_curve.datas.clear();
			for (auto& d : point_list)
			{
				animation_data->time_curve.datas.emplace_back(d.x, d.y);
			}
		}

	private:
		animation::data* animation_data;
		std::set<ImCurveEdit::EditPoint> selection_lists;
		std::vector<ImCurveEdit::EditPoint> handle_lists[2];
		std::vector<ImVec2> point_list;
		ImVec2 range_min, range_max;
		bool visible;

	};

	//	アニメーションタイムライン編集
	//	ﾃｷﾄｰ('ω')-ﾄｷﾃ
	class animation_timeline_editor : public editor_items::ibase_editor
	{
		enum class timeline_category
		{
			none,
			event,
			node,
		};
		struct selected_data
		{
			timeline_category category;
			UINT indices[2];
		};

		static constexpr std::string_view SequencerName = "Sequencer";

	private:
		//	時間からフレーム番号算出
		int calcurate_frame_index(float second) const
		{
			return static_cast<int>(system_units::convert_sec_to_fps(second * animation_frame_scaler));
		}
		//	フレーム番号から時間算出
		float calcurate_second(int frame_index) const
		{
			return system_units::convert_fps_to_sec(static_cast<float>(frame_index)) / animation_frame_scaler;
		}

		//	イベント/ノードキーフレームを時間順に再ソートする
		void sort_animation_data()
		{
			std::sort(animation_data->events.begin(), animation_data->events.end(),
				[](const animation::event& l, const animation::event& r) { return l.time < r.time; });

			for (auto& track : animation_data->nodes)
			{
				std::sort(track.nodes.begin(), track.nodes.end(),
					[](const animation::node& l, const animation::node& r) { return l.time < r.time; });
			}
		}

		//	選択データ(複数可)を実データから削除する。
		//	インデックスが大きい方から消していくことで、
		//	「他の選択データのインデックスをずらす」処理そのものが不要になる。
		//	※ selected 自体からの除去は呼び出し側で行うこと。
		void delete_selected_data(const std::vector<selected_data>& targets)
		{
			std::vector<UINT> event_indices;
			std::vector<std::pair<UINT, UINT>> node_key_indices; //	{node_index, time_index}

			for (auto& data : targets)
			{
				if (data.category == timeline_category::event)
					event_indices.push_back(data.indices[0]);
				else if (data.category == timeline_category::node)
					node_key_indices.push_back({ data.indices[0], data.indices[1] });
			}

			//	イベント: インデックスが大きい方から削除
			std::sort(event_indices.begin(), event_indices.end(), std::greater<UINT>());
			for (UINT index : event_indices)
				animation_data->events.erase(animation_data->events.begin() + index);

			//	ノードのキーフレーム: time_index が大きい方から削除
			//	(node_index が違えば別の配列なので、time_index の降順だけで十分)
			std::sort(node_key_indices.begin(), node_key_indices.end(),
				[](auto& l, auto& r) { return l.second > r.second; });
			for (auto& [node_index, time_index] : node_key_indices)
			{
				auto& track = animation_data->nodes[node_index];
				track.nodes.erase(track.nodes.begin() + time_index);
			}

			sort_animation_data();
		}

	public:
		animation_timeline_editor()
		: editor_items::ibase_editor()
		{
		}

		~animation_timeline_editor() override
		{
		}

		void clear_selected_selection_data()
		{
			selected.clear();
			ImGui::NeoClearSelection(sequencerID);
		}

		void clear_selected_timeline_data()
		{
			selected_timeline = timeline_category::none;
			ImGui::SetSelectedTimeline(sequencerID, "");
		}

		void set_data(animation::data* data)
		{
			if (animation_data == data)
				return;

			animation_data = data;
			if (!animation_data)
				return;

			current_frame = 0;
			start_frame = first_frame = 0;
			end_frame = last_frame = calcurate_frame_index(animation_data->end_time);
			clear_selected_selection_data();
			clear_selected_timeline_data();
		}

		void render(std::shared_ptr<editor_items::context>& context)
		{
			if (!animation_data)
				return;

			if (ImGui::Begin("Animation Timeline"))
			{
				switch (selected_timeline)
				{
					case timeline_category::node:
					{
						if (selected_node_index >= animation_data->nodes.size())
							break;

						if (ImGui::Button("Add Keyframe"))
						{
							auto& track = animation_data->nodes[selected_node_index];
							dummy_node.time = calcurate_second(current_frame);
							current_frame = calcurate_frame_index(dummy_node.time);
							track.nodes.push_back(dummy_node);
							sort_animation_data();
						}
						break;
					}
					case timeline_category::event:
					{
						if (ImGui::Button("Add Event"))
						{
							dummy_event.time = calcurate_second(current_frame);
							current_frame = calcurate_frame_index(dummy_event.time);
							animation_data->events.push_back(dummy_event);
							sort_animation_data();
						}
						break;
					}
					case timeline_category::none:
					{
						if (selected.empty())
							break;

						//	何かしらあれば AllDelete コマンドを表示する
						if (ImGui::Button("Selected All Delete"))
						{
							delete_selected_data(selected);
							clear_selected_selection_data();
							clear_selected_timeline_data();
						}
						break;
					}
				}
				ImGui::Separator();
				if (ImGui::BeginNeoSequencer(SequencerName.data(), &current_frame, &start_frame, &end_frame, { 0, 0 },
					ImGuiNeoSequencerFlags_AllowLengthChanging | ImGuiNeoSequencerFlags_EnableSelection |
					ImGuiNeoSequencerFlags_AlwaysShowHeader | ImGuiNeoSequencerFlags_Selection_EnableDragging |
					ImGuiNeoSequencerFlags_Selection_EnableDeletion))
				{
					//	シーケンサーのメモリリーク修正のため
					//	@remainder	選択中にモーションを変えてしまうとリークする（時間がある時に修正）
					{
						ImGuiWindow* window = ImGui::GetCurrentWindow();
						sequencerID = window->IDStack[window->IDStack.size() - 1];
					}
					selected.clear();
					render_events(context);
					render_nodes(context);
					ImGui::EndNeoSequencer();
				}
			}
			ImGui::End();

			render_inspector(context);
		}

		void render_events(std::shared_ptr<editor_items::context>& context)
		{
			//Eventsは必ず表示
			//if (animation_data->events.empty())
			//	return;

			ImGui::PushID("##Events");
			if (ImGui::BeginNeoGroup(""))
			{
				if (ImGui::BeginNeoTimelineEx("Events"))
				{
					UINT node_index = 0;
					for (auto& event : animation_data->events)
					{
						ImGui::PushID(context->get_item_id());
						int frame = calcurate_frame_index(event.time);
						ImGui::NeoKeyframe(&frame);
						if (ImGui::IsNeoKeyframeSelected())
						{
							if (selected.empty())
								clear_selected_timeline_data();
							selected.push_back({ timeline_category::event, node_index, 0 });
						}
						ImGui::PopID();
						++node_index;
					}
					//	このタイムラインの選択中か判定
					if (ImGui::IsNeoTimelineSelected())
					{
						clear_selected_selection_data();
						selected_timeline = timeline_category::event;
					}
					ImGui::EndNeoTimeLine();
				}
				ImGui::EndNeoGroup();
			}
			ImGui::PopID();
		}

		void render_nodes(std::shared_ptr<editor_items::context>& context)
		{
			if (animation_data->nodes.empty())
				return;

			ImGui::PushID("##Nodes");
			if (ImGui::BeginNeoGroup(""))
			{
				UINT node_index = 0;
				for (auto& track : animation_data->nodes)
				{
					auto text = system_utility::convert_utf8(track.node_name);
					if (ImGui::BeginNeoTimelineEx(text.data()))
					{
						UINT time_index = 0;
						for (auto& keyframe : track.nodes)
						{
							ImGui::PushID(context->get_item_id());

							int frame = calcurate_frame_index(keyframe.time);
							ImGui::NeoKeyframe(&frame);

							if (ImGui::IsNeoKeyframeSelected())
							{
								if (selected.empty())
									clear_selected_timeline_data();
								selected.push_back({ timeline_category::node, node_index, time_index });
							}
							ImGui::PopID();
							++time_index;
						}
						//	このタイムラインの選択中か判定
						if (ImGui::IsNeoTimelineSelected())
						{
							clear_selected_selection_data();
							selected_timeline = timeline_category::node;
							selected_node_index = node_index;
						}
						ImGui::EndNeoTimeLine();
					}
					++node_index;
				}
				ImGui::EndNeoGroup();
			}
			ImGui::PopID();
		}

		void render_inspector(std::shared_ptr<editor_items::context>& context)
		{
			//	Event編集用関数
			auto node_edit = [&](animation::node& node)
			{
				ImGui::DragFloat("time", &node.time,
					system_units::convert_fps_to_sec(1.0f),
					0.0f, 0.0f,
					"%.6f");
				ImGui::DragFloat3("scale", &node.scale.x, 0.01f);
				math::rotator rotator = node.rotation;
				ImGui::DragFloat3("rotation", &rotator.x, 0.01f);
				node.rotation = rotator;
				ImGui::DragFloat3("translation", &node.translation.x, 0.01f);
			};
			auto event_edit = [&](animation::event& event)
			{
				ImGui::DragFloat("time", &event.time,
					system_units::convert_fps_to_sec(1.0f),
					0.0f, 0.0f,
					"%.6f");

				ImGui::InputTextUtf8("Function Name", event.function_name);
				std::vector<std::string> row_texts;
				std::vector<char*> combo_texts;
				for (auto& name : magic_enum::enum_names<animation::event::variable_type>())
				{
					row_texts.emplace_back(name);
				}
				for (auto& name : row_texts)
				{
					combo_texts.emplace_back(name.data());
				}
				int index = static_cast<int>(event.type);
				ImGui::Combo("variable_type", &index, combo_texts.data(), static_cast<int>(combo_texts.size()));
				event.type = static_cast<animation::event::variable_type>(index);
				switch (event.type)
				{
					case ::animation::event::variable_type::String:
					{
						ImGui::InputTextUtf8("Value", event.variable);
						break;
					}
					case ::animation::event::variable_type::Float:
					{
						float value;
						event.get_variable<float>(value);
						ImGui::DragFloat("Value", &value);
						event.set_variable<float>(value);
						break;
					}
					case ::animation::event::variable_type::Int:
					{
						int value;
						event.get_variable<int>(value);
						ImGui::DragInt("Value", &value);
						event.set_variable<int>(value);
						break;
					}
				}
			};

			if (ImGui::Begin("Animation Inspector"))
			{
				int delete_data_index = -1;
				int data_index = 0;
				for (auto& data : selected)
				{
					ImGui::PushID(context->get_item_id());
					switch (data.category)
					{
						case timeline_category::event:
						{
							ImGui::Text("Event[%d]", data.indices[0]);
							ImGui::SameLine();
							if (ImGui::Button("Delete"))
								delete_data_index = data_index;
							event_edit(animation_data->events[data.indices[0]]);
							break;
						}
						case timeline_category::node:
						{
							auto& node_data = animation_data->nodes[data.indices[0]];
							ImGui::Text("%s[%d]", node_data.node_name.c_str(), data.indices[1]);
							ImGui::SameLine();
							if (ImGui::Button("Delete"))
								delete_data_index = data_index;
							node_edit(node_data.nodes[data.indices[1]]);
							break;
						}
					}
					ImGui::PopID();
					ImGui::Separator();
					++data_index;
				}
				if (delete_data_index >= 0)
				{
					delete_selected_data({ selected[delete_data_index] });
					selected.erase(selected.begin() + delete_data_index);
				}
			}
			ImGui::End();
		}

	private:
		ImGuiID sequencerID;
		timeline_category selected_timeline;
		animation::data* animation_data;
		float animation_frame_scaler = 10.0f;
		int32_t current_frame = 0;
		int32_t start_frame, end_frame;
		int32_t first_frame, last_frame;
		std::vector<selected_data> selected;
		UINT selected_node_index = 0;

		animation::event dummy_event;
		animation::node dummy_node;
	};
}

bool editor_items::edit_animation_data(std::shared_ptr<context>& context, animation::data& animation_data)
{
	//	名前編集
	if (!context->flag_maps[context::EditAnimationHideName])
	{
		ImGui::InputTextUtf8("name", animation_data.name);
	}

	//	終端時間
	{
		ImGui::DragFloat("end time", &animation_data.end_time, system_units::convert_fps_to_sec(1.0f), system_units::convert_fps_to_sec(1.0f),FLT_MAX);
	}

	//	ルートモーション関係
	if (!context->flag_maps[context::EditAnimationHideRootMotion])
	{
		ImGui::Text("root_motion");
		ImGui::SameLine();
		ImGui::Checkbox("xz", &animation_data.root_motion_xz);
		ImGui::SameLine();
		ImGui::Checkbox("y", &animation_data.root_motion_y);
		ImGui::SameLine();
		ImGui::Checkbox("rotation", &animation_data.root_rotation);
		if (animation_data.root_motion_xz || animation_data.root_motion_y || animation_data.root_rotation)
		{
			ImGui::InputTextUtf8("root_motion_node_name", animation_data.root_motion_node_name);
		}
		ImGui::Separator();
	}

	//	IK関係
	if (!context->flag_maps[context::EditAnimationHideIK])
	{
		ImGui::Checkbox("foot_ik", &animation_data.foot_ik);
		ImGui::SameLine();
		ImGui::Checkbox("hand_ik", &animation_data.hand_ik);
		ImGui::SameLine();
		ImGui::Checkbox("aim_ik", &animation_data.aim_ik);
		ImGui::Separator();
	}

	//	アニメーションカーブエディター
	//	フレーム間のカーブは取り扱っていない
	if (ImGui::TreeNode("Curve Editor"))
	{
		if (!context->interpolation_editor)
			context->interpolation_editor = std::make_unique<animation_interpolation_editor>();

		animation_interpolation_editor* editor = static_cast<animation_interpolation_editor*>(context->interpolation_editor.get());
		editor->set_data(&animation_data);
		editor->render(context);

		ImGui::TreePop();
	}
	return true;
}

bool editor_items::edit_animation_data_timeline(std::shared_ptr<context>& context, animation::data& animation_data)
{
	auto data_hash_key = system_utility::fnv1a_hash(sizeof(animation_data), reinterpret_cast<char*>(&animation_data));

	//	タイムラインエディター
	{
		if (!context->timeline_editor)
			context->timeline_editor = std::make_unique<animation_timeline_editor>();

		animation_timeline_editor* editor = static_cast<animation_timeline_editor*>(context->timeline_editor.get());
		editor->set_data(&animation_data);
		editor->render(context);
	}
	return true;
}

#endif	//	defined(USE_IMGUI)


