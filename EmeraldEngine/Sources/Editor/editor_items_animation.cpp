#include "Editor/editor_items.h"

#if defined(USE_IMGUI)
#include <algorithm>
#include <cstdint>
#include <functional>
#include <set>
#include <vector>

namespace
{
	//	context が保持する具象エディターを遅延生成して取得するヘルパー。
	template<class editor_type>
	editor_type& acquire_editor(std::unique_ptr<editor_items::ibase_editor>& holder)
	{
		if (!holder)
			holder = std::make_unique<editor_type>();

		return *static_cast<editor_type*>(holder.get());
	}

	//	アニメーション時間補間カーブの編集
	class animation_interpolation_editor final
		: public ImCurveEdit::Delegate
		, public editor_items::ibase_editor
	{
		static constexpr float curve_horizontal_margin = 40.0f;
		static constexpr float curve_height = 300.0f;
		static constexpr unsigned int curve_edit_id = 0xFFFF;
		static constexpr uint32_t curve_color = 0xFF0000FF;

	public:
		void set_data(animation::data* data)
		{
			if (animation_data == data)
				return;

			animation_data = data;
			reload_points();
		}

		void render(std::shared_ptr<editor_items::context>& context) override
		{
			if (animation_data == nullptr)
				return;

			auto& curve = animation_data->time_curve;

			//	ズームには対応しない
			range_min = ImVec2(0.0f, 0.0f);
			range_max = ImVec2(0.0f, 1.0f);

			//	補間モード設定
			ImGui::Combo<animation::interpolation::types>("Curve Type", curve.type);

			//	リセット（始点・終点だけの直線に戻す）
			ImGui::SameLine();
			if (ImGui::Button("Reset##curve_reset"))
			{
				curve.datas = { math::float2(0.0f, 0.0f), math::float2(1.0f, 1.0f) };
				reload_points();
			}

			//	カーブ編集本体
			const float width = ImGui::GetWindowSize().x - curve_horizontal_margin;
			ImVector<ImCurveEdit::EditPoint> selected_points;
			ImCurveEdit::Edit(*this, ImVec2(width, curve_height), curve_edit_id, nullptr, &selected_points, &selection_lists);

			//	右クリック中の選択点を削除
			if (!selected_points.empty() && ImGui::IsMouseDown(ImGuiMouseButton_Right))
				remove_points(selected_points);

			//	0～1 にクランプ
			for (auto& point : curve.datas)
				point = math::saturate(math::vector2f(point)).to_float2();

			//	Edit / Reset / クランプで datas 側が変化しているのでキャッシュを追従させる
			reload_points();

			static core::cache_string animation_time = "    アニメーション時間";
			ImGui::Text(animation_time.c_str());
		}

	private:
		//	ImCurveEdit関係
		size_t GetCurveCount() override { return 1; }
		bool IsVisible(size_t /*curve_index*/) override { return true; }
		size_t GetPointCount(size_t /*curve_index*/) override { return point_cache.size(); }
		uint32_t GetCurveColor(size_t /*curve_index*/) override { return curve_color; }
		ImVec2* GetPoints(size_t /*curve_index*/) override { return point_cache.data(); }
		ImVec2& GetMin() override { return range_min; }
		ImVec2& GetMax() override { return range_max; }
		UINT GetBackgroundColor() override { return 0x00000000; }

		ImCurveEdit::CurveType GetCurveType(size_t /*curve_index*/) const override
		{
			if (animation_data == nullptr)
				return ImCurveEdit::CurveSmooth;

			return static_cast<ImCurveEdit::CurveType>(animation_data->time_curve.type);
		}

		int EditPoint(size_t /*curve_index*/, int point_index, ImVec2 value) override
		{
			point_cache[point_index] = value;
			sync_to_data();

			//	ソートで並び替わった後の新しいインデックスを返す
			for (size_t index = 0; index < point_cache.size(); ++index)
			{
				if (point_cache[index].x == value.x)
					return static_cast<int>(index);
			}
			return point_index;
		}

		void AddPoint(size_t /*curve_index*/, ImVec2 value) override
		{
			point_cache.push_back(value);
			sync_to_data();
		}

		//	datas -> point_cache
		void reload_points()
		{
			point_cache.clear();
			if (animation_data == nullptr)
				return;

			for (const auto& d : animation_data->time_curve.datas)
				point_cache.emplace_back(d.x, d.y);
		}

		//	point_cache -> datas
		void sync_to_data()
		{
			std::sort(point_cache.begin(), point_cache.end(),
				[](const ImVec2& a, const ImVec2& b) { return a.x < b.x; });

			if (animation_data == nullptr)
				return;

			auto& datas = animation_data->time_curve.datas;
			datas.clear();
			for (const auto& p : point_cache)
				datas.emplace_back(p.x, p.y);
		}

		void remove_points(const ImVector<ImCurveEdit::EditPoint>& targets)
		{
			auto& datas = animation_data->time_curve.datas;

			//	インデックスが大きい方から消せば添字がずれない
			std::vector<int> indices;
			for (const auto& target : targets)
				indices.push_back(target.pointIndex);
			std::sort(indices.begin(), indices.end(), std::greater<int>());

			for (int index : indices)
			{
				if (index <= 0 || index >= static_cast<int>(datas.size()) - 1)
					continue;
				datas.erase(datas.begin() + index);
			}

			selection_lists.clear();
		}

	private:
		animation::data* animation_data = nullptr;
		std::set<ImCurveEdit::EditPoint> selection_lists;
		std::vector<ImVec2> point_cache;
		ImVec2 range_min = ImVec2(0.0f, 0.0f);
		ImVec2 range_max = ImVec2(0.0f, 1.0f);
	};

//	アニメーションタイムライン編集
	//	ﾃｷﾄｰ('ω')-ﾄｷﾃ
	class animation_timeline_editor final : public editor_items::ibase_editor
	{
		//	どのタイムライン行が編集対象として選択されているか
		enum class timeline_category
		{
			none,
			event,
			node,
		};

		//	選択中のキーフレーム 1 個分の参照情報
		struct selected_key
		{
			timeline_category category = timeline_category::none;
			UINT track_index = 0;	//	event: events 配列の添字 / node: nodes(トラック) 配列の添字
			UINT key_index = 0;		//	node のときのみ有効。track.nodes 配列の添字
		};

		static constexpr std::string_view sequencer_name = "Sequencer";

		//	FPSプリセット（ツールバーのショートカットボタン用）
		static constexpr std::array<float, 4> fps_presets = { 24.0f, 30.0f, 60.0f, 120.0f };

	public:
		void set_data(animation::data* data)
		{
			if (animation_data == data)
				return;

			animation_data = data;
			if (animation_data == nullptr)
				return;

			current_frame = 0;
			start_frame = 0;
			end_frame = calculate_frame_index(animation_data->end_time);
			cached_end_time = animation_data->end_time;
			keyframe_move_pending = false;
			selection_dragging = false;
			sequencer_rect_valid = false;
			clear_selection();
			clear_active_timeline();
		}

		void render(std::shared_ptr<editor_items::context>& context) override
		{
			if (animation_data == nullptr)
				return;

			compact_toolbar = false;

			bool mouse_down_in_sequencer = false;
			if (sequencer_rect_valid && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
			{
				const ImVec2 mouse_pos = ImGui::GetMousePos();
				mouse_down_in_sequencer =
					mouse_pos.x >= sequencer_rect_min.x && mouse_pos.x <= sequencer_rect_max.x &&
					mouse_pos.y >= sequencer_rect_min.y && mouse_pos.y <= sequencer_rect_max.y;
			}

			const ImGuiWindowFlags timeline_window_flags =
				(selection_dragging || mouse_down_in_sequencer) ? ImGuiWindowFlags_NoMove : ImGuiWindowFlags_None;

			//	end time の変更をタイムラインへ反映
			if (animation_data->end_time != cached_end_time)
			{
				end_frame = calculate_frame_index(animation_data->end_time);
				cached_end_time = animation_data->end_time;
			}

			if (editor_items::begin_window(context, "Animation Timeline", nullptr, timeline_window_flags))
				render_timeline_body(context);
			ImGui::End();

			render_inspector(context);
		}

		//	窓を作らず、今の領域にタイムラインを描く
		void render_embedded(std::shared_ptr<editor_items::context>& context)
		{
			if (animation_data == nullptr)
				return;

			//	end time の変更をタイムラインへ反映
			if (animation_data->end_time != cached_end_time)
			{
				end_frame = calculate_frame_index(animation_data->end_time);
				cached_end_time = animation_data->end_time;
			}

			compact_toolbar = true;
			render_timeline_body(context);
		}

		//	窓を作らず、今の領域に選んだキーの詳細を描く
		void render_inspector_embedded(std::shared_ptr<editor_items::context>& context)
		{
			if (animation_data != nullptr)
				render_inspector_body(context);
		}

	private:
		//	ツールバーとシーケンサー
		void render_timeline_body(std::shared_ptr<editor_items::context>& context)
		{
			if (compact_toolbar)
				render_compact_toolbar();
			else
				render_toolbar();

			ImGui::Separator();

			//	今回描画するシーケンサー領域を記録しておく（次フレームの予測判定に使う）
			const ImVec2 sequencer_pos = ImGui::GetCursorScreenPos();
			const float sequencer_width = ImGui::GetContentRegionAvail().x;

			const int end_frame_before = end_frame;
			if (ImGui::BeginNeoSequencer(sequencer_name.data(), &current_frame, &start_frame, &end_frame, { 0, 0 },
				ImGuiNeoSequencerFlags_AllowLengthChanging | ImGuiNeoSequencerFlags_EnableSelection |
				ImGuiNeoSequencerFlags_AlwaysShowHeader | ImGuiNeoSequencerFlags_Selection_EnableDragging |
				ImGuiNeoSequencerFlags_Selection_EnableDeletion))
			{
				//	シーケンサーのメモリリーク対策で ID を控えておく
				//	@remainder	選択中にモーションを差し替えるとリークする（時間のある時に修正）
				ImGuiWindow* window = ImGui::GetCurrentWindow();
				sequencer_id = window->IDStack[window->IDStack.size() - 1];

				//	選択情報は毎フレーム ImGui 側の状態から作り直す
				const bool had_selection = !selected.empty();
				selected.clear();
				render_event_timeline(context);
				render_node_timelines(context);

				// キーフレーム選択が開始したフレームだけラインの選択を外す
				if (!selected.empty() && !had_selection)
					clear_active_timeline();

				const bool dragging = ImGui::NeoIsDraggingSelection();
				selection_dragging = dragging;	//	次フレームの Begin で NoMove 判定に使う
				ImGui::EndNeoSequencer();

				//	タイムラインで変えた長さを end time へ反映
				if (end_frame != end_frame_before)
				{
					animation_data->end_time = calculate_second(end_frame);
					cached_end_time = animation_data->end_time;
				}

				//	キーフレームのドラッグ移動が終わったら時間順に並べ直す
				if (keyframe_move_pending && !dragging)
				{
					sort_animation_data();
					clear_selection();
					keyframe_move_pending = false;
				}

				sequencer_rect_min = sequencer_pos;
				sequencer_rect_max = ImVec2(sequencer_pos.x + sequencer_width, ImGui::GetCursorScreenPos().y);
				sequencer_rect_valid = true;
			}
			else
			{
				sequencer_rect_valid = false;
			}
		}

	private:
		int calculate_frame_index(float second) const
		{
			return static_cast<int>(std::lround(second * fps));
		}
		float calculate_second(int frame_index) const
		{
			return static_cast<float>(frame_index) / fps;
		}

		void set_fps(float new_fps)
		{
			new_fps = std::max(new_fps, 1.0f);
			if (new_fps == fps)
				return;

			const float current_time = calculate_second(current_frame);
			const float start_time = calculate_second(start_frame);
			const float end_time = calculate_second(end_frame);

			fps = new_fps;
			fps_input = fps;

			current_frame = calculate_frame_index(current_time);
			start_frame = calculate_frame_index(start_time);
			end_frame = calculate_frame_index(end_time);
		}

		void write_back_dragged_time(float& time, int frame_before, int frame_after)
		{
			if (frame_after == frame_before)
				return;

			time = std::max(calculate_second(frame_after), 0.0f);
			keyframe_move_pending = true;
		}

		//	track 内に frame と同じフレームのキーが既にあるか
		bool node_frame_occupied(const animation::node_data& track, int frame, const animation::node* exclude) const
		{
			for (const auto& key : track.nodes)
			{
				if (&key == exclude)
					continue;
				if (calculate_frame_index(key.time) == frame)
					return true;
			}
			return false;
		}

		void clear_selection()
		{
			selected.clear();
			ImGui::NeoClearSelection(sequencer_id);
		}

		void clear_active_timeline()
		{
			active_timeline = timeline_category::none;
			ImGui::SetSelectedTimeline(sequencer_id, "");
		}

		void sort_animation_data()
		{
			if (animation_data == nullptr)
				return;

			std::sort(animation_data->events.begin(), animation_data->events.end(),
				[](const animation::event& l, const animation::event& r) { return l.time < r.time; });

			for (auto& track : animation_data->nodes)
			{
				std::sort(track.nodes.begin(), track.nodes.end(),
					[](const animation::node& l, const animation::node& r) { return l.time < r.time; });
			}
		}

		void delete_selected_data(const std::vector<selected_key>& targets)
		{
			if (animation_data == nullptr)
				return;

			std::vector<UINT> event_indices;
			std::vector<std::pair<UINT, UINT>> node_key_indices;

			for (const auto& target : targets)
			{
				if (target.category == timeline_category::event)
					event_indices.push_back(target.track_index);
				else if (target.category == timeline_category::node)
					node_key_indices.emplace_back(target.track_index, target.key_index);
			}

			//	添字が大きい方から削除
			std::sort(event_indices.begin(), event_indices.end(), std::greater<UINT>());
			for (UINT index : event_indices)
				animation_data->events.erase(animation_data->events.begin() + index);

			//	ノードのキーフレーム: key_index が大きい方から削除
			std::sort(node_key_indices.begin(), node_key_indices.end(),
				[](const auto& l, const auto& r) { return l.second > r.second; });
			for (const auto& [track_index, key_index] : node_key_indices)
			{
				auto& track = animation_data->nodes[track_index];
				track.nodes.erase(track.nodes.begin() + key_index);
			}

			sort_animation_data();
		}

		void render_fps_controls()
		{
			ImGui::SetNextItemWidth(120.0f);
			ImGui::InputFloat("Sampling Rate (FPS)", &fps_input, 1.0f, 10.0f, "%.2f");
			if (ImGui::IsItemDeactivatedAfterEdit())
				set_fps(fps_input);

			for (float preset : fps_presets)
			{
				ImGui::SameLine();
				ImGui::PushID(static_cast<int>(preset));
				char label[16];
				std::snprintf(label, sizeof(label), "%gfps", preset);
				if (ImGui::Button(label))
					set_fps(preset);
				ImGui::PopID();
			}
		}

		void render_toolbar()
		{
			render_fps_controls();
			ImGui::Separator();
			render_key_buttons();
		}

		//	1行にまとめたツールバー（終端時間・サンプリングレート・キーの追加と削除）
		void render_compact_toolbar()
		{
			ImGui::SetNextItemWidth(100.0f);
			ImGui::DragFloat("end time", &animation_data->end_time, system_units::convert_fps_to_sec(1.0f), system_units::convert_fps_to_sec(1.0f), FLT_MAX);
			ImGui::SameLine();
			render_fps_controls();
			ImGui::SameLine();
			render_key_buttons();
		}

		//	キーの追加と削除
		void render_key_buttons()
		{
			const bool node_selected =

				active_timeline == timeline_category::node &&
				selected_node_index < animation_data->nodes.size();
			const bool can_add_event = active_timeline == timeline_category::event;

			//	ノードは同じフレームに複数キーを置けないので、再生位置が空いているときだけ追加可
			const bool can_add_keyframe = node_selected &&
				!node_frame_occupied(animation_data->nodes[selected_node_index], current_frame, nullptr);

			//	追加：ノード行 or イベント行を選択しているときだけ押せる
			ImGui::BeginDisabled(!can_add_keyframe && !can_add_event);
			if (ImGui::Button(can_add_event ? "Add Event" : "Add Keyframe"))
			{
				if (can_add_event)
				{
					animation::event event;
					event.type = animation::event::variable_type::None;
					event.time = calculate_second(current_frame);
					animation_data->events.push_back(event);
				}
				else
				{
					animation::node key;
					key.time = calculate_second(current_frame);
					animation_data->nodes[selected_node_index].nodes.push_back(key);
				}
				sort_animation_data();
			}
			ImGui::EndDisabled();

			//	削除：キーを選択しているときだけ押せる
			ImGui::SameLine();
			ImGui::BeginDisabled(selected.empty());
			if (ImGui::Button("Delete"))
			{
				delete_selected_data(selected);
				clear_selection();
				clear_active_timeline();
			}
			ImGui::EndDisabled();
		}

		void render_event_timeline(std::shared_ptr<editor_items::context>& context)
		{
			//	Events 行は空でも常に表示する
			ImGui::PushID("##Events");
			if (ImGui::BeginNeoGroup(""))
			{
				if (ImGui::BeginNeoTimelineEx("Events"))
				{
					UINT event_index = 0;
					for (auto& event : animation_data->events)
					{
						ImGui::PushID(context->get_item_id());

						int frame = calculate_frame_index(event.time);
						const int frame_before = frame;
						ImGui::NeoKeyframe(&frame);
						write_back_dragged_time(event.time, frame_before, frame);

						if (ImGui::IsNeoKeyframeSelected())
						{
							selected.push_back({ timeline_category::event, event_index, 0 });
						}

						ImGui::PopID();
						++event_index;
					}

					//	この行が選択中なら編集対象にする
					if (ImGui::IsNeoTimelineSelected())
					{
						if (active_timeline != timeline_category::event)
							clear_selection();
						active_timeline = timeline_category::event;
					}
					ImGui::EndNeoTimeLine();
				}
				ImGui::EndNeoGroup();
			}
			ImGui::PopID();
		}

		void render_node_timelines(std::shared_ptr<editor_items::context>& context)
		{
			if (animation_data->nodes.empty())
				return;

			ImGui::PushID("##Nodes");
			if (ImGui::BeginNeoGroup(""))
			{
				UINT track_index = 0;
				for (auto& track : animation_data->nodes)
				{
					auto& label = track.node_name.get<core::utf8_cache>();
					if (ImGui::BeginNeoTimelineEx(label.data()))
					{
						UINT key_index = 0;
						for (auto& keyframe : track.nodes)
						{
							ImGui::PushID(context->get_item_id());

							int frame = calculate_frame_index(keyframe.time);
							const int frame_before = frame;
							ImGui::NeoKeyframe(&frame);

							//	移動先フレームに別キーがあるなら移動を受け付けない（同一フレーム禁止）
							if (!node_frame_occupied(track, frame, &keyframe))
								write_back_dragged_time(keyframe.time, frame_before, frame);

							if (ImGui::IsNeoKeyframeSelected())
							{
								selected.push_back({ timeline_category::node, track_index, key_index });
							}

							ImGui::PopID();
							++key_index;
						}

						//	この行が選択中なら編集対象にする
						if (ImGui::IsNeoTimelineSelected())
						{
							if (!(active_timeline == timeline_category::node && selected_node_index == track_index))
								clear_selection();
							active_timeline = timeline_category::node;
							selected_node_index = track_index;
						}
						ImGui::EndNeoTimeLine();
					}
					++track_index;
				}
				ImGui::EndNeoGroup();
			}
			ImGui::PopID();
		}

		void edit_node_inspector(animation::node& node)
		{
			ImGui::DragFloat("time", &node.time, 1.0f / fps, 0.0f, 0.0f, "%.6f");
			ImGui::DragFloat3("scale", &node.scale.x, 0.01f);

			math::rotatorf rotator(math::quaternionf(node.rotation));
			ImGui::DragFloat3("rotation", &rotator.x, 0.01f);
			node.rotation = rotator.to_quaternion().to_float4();

			ImGui::DragFloat3("translation", &node.translation.x, 0.01f);
		}

		void edit_event_inspector(animation::event& event)
		{
			ImGui::DragFloat("time", &event.time, 1.0f / fps, 0.0f, 0.0f, "%.6f");
			ImGui::InputTextUtf8("Function Name", event.function_name);

			ImGui::Combo<animation::event::variable_type>("variable_type", event.type);
			switch (event.type)
			{
				case animation::event::variable_type::String:
				{
					ImGui::InputTextUtf8("Value", event.variable);
					break;
				}
				case animation::event::variable_type::Float:
				{
					float value = 0.0f;
					event.get_variable<float>(value);
					if (ImGui::DragFloat("Value", &value))
						event.set_variable<float>(value);
					break;
				}
				case animation::event::variable_type::Int:
				{
					int value = 0;
					event.get_variable<int>(value);
					if (ImGui::DragInt("Value", &value))
						event.set_variable<int>(value);
					break;
				}
				case animation::event::variable_type::None:
					break;
			}
		}

		void render_inspector(std::shared_ptr<editor_items::context>& context)
		{
			if (editor_items::begin_window(context, "Animation Inspector"))
				render_inspector_body(context);
			ImGui::End();
		}

		void render_inspector_body(std::shared_ptr<editor_items::context>& context)
		{
			for (const auto& key : selected)
			{
				ImGui::PushID(context->get_item_id());
				switch (key.category)
				{
					case timeline_category::event:
					{
						ImGui::Text("Event[%d]", key.track_index);
						edit_event_inspector(animation_data->events[key.track_index]);
						break;
					}
					case timeline_category::node:
					{
						auto& track = animation_data->nodes[key.track_index];
						auto& label = track.node_name.get<core::utf8_cache>();
						ImGui::Text("%s[%d]", label.data(), key.key_index);
						edit_node_inspector(track.nodes[key.key_index]);
						break;
					}
					case timeline_category::none:
						break;
				}
				ImGui::PopID();
				ImGui::Separator();
			}
		}

	private:
		animation::data* animation_data = nullptr;
		ImGuiID sequencer_id = 0;
		timeline_category active_timeline = timeline_category::none;
		bool selection_dragging = false;

		ImVec2 sequencer_rect_min = ImVec2(0.0f, 0.0f);
		ImVec2 sequencer_rect_max = ImVec2(0.0f, 0.0f);
		bool sequencer_rect_valid = false;

		float fps = 60.0f;
		bool compact_toolbar = false;
		float fps_input = fps;

		int32_t current_frame = 0;
		int32_t start_frame = 0;
		int32_t end_frame = 0;
		float cached_end_time = 0.0f;

		std::vector<selected_key> selected;
		UINT selected_node_index = 0;
		bool keyframe_move_pending = false;
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
		ImGui::DragFloat("end time", &animation_data.end_time, system_units::convert_fps_to_sec(1.0f), system_units::convert_fps_to_sec(1.0f), FLT_MAX);
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
	if (!context->flag_maps[context::EditAnimationHideCurve])
	{
		if (ImGui::TreeNode("Curve Editor"))
		{
			auto& editor = acquire_editor<animation_interpolation_editor>(context->interpolation_editor);
			editor.set_data(&animation_data);
			editor.render(context);

			ImGui::TreePop();
		}
	}
	return true;
}

bool editor_items::edit_animation_data_timeline(std::shared_ptr<context>& context, animation::data& animation_data)
{
	//	タイムラインエディター
	auto& editor = acquire_editor<animation_timeline_editor>(context->timeline_editor);
	editor.set_data(&animation_data);
	editor.render(context);

	return true;
}

bool editor_items::edit_animation_data_timeline_embedded(std::shared_ptr<context>& context, animation::data& animation_data)
{
	//	タイムラインエディター
	auto& editor = acquire_editor<animation_timeline_editor>(context->timeline_editor);
	editor.set_data(&animation_data);
	editor.render_embedded(context);

	return true;
}

void editor_items::edit_animation_timeline_inspector(std::shared_ptr<context>& context)
{
	auto& editor = acquire_editor<animation_timeline_editor>(context->timeline_editor);
	editor.render_inspector_embedded(context);
}

#endif	//	defined(USE_IMGUI)
