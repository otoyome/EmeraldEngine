#include "Editor/editor_items.h"

#if defined(USE_IMGUI)
#include <algorithm>
#include <cstdint>
#include <functional>
#include <set>
#include <vector>

namespace
{
	//------------------------------------------------------------------------------
	//	context が保持する具象エディターを遅延生成して取得するヘルパー。
	//	（edit_ik 等と同じ「context に unique_ptr を持たせる」方式のボイラープレート削減用）
	//------------------------------------------------------------------------------
	template<class editor_type>
	editor_type& acquire_editor(std::unique_ptr<editor_items::ibase_editor>& holder)
	{
		if (!holder)
			holder = std::make_unique<editor_type>();

		return *static_cast<editor_type*>(holder.get());
	}

	//------------------------------------------------------------------------------
	//	アニメーション時間補間カーブの編集
	//	（animation::data::time_curve を ImCurveEdit で編集する）
	//------------------------------------------------------------------------------
	class animation_interpolation_editor final
		: public ImCurveEdit::Delegate
		, public editor_items::ibase_editor
	{
	public:
		//	編集対象を差し替える。同じ実体なら再構築しない。
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

			//	ホイールズームには対応しないため、表示範囲は毎フレーム固定に戻す。
			range_min = ImVec2(0.0f, 0.0f);
			range_max = ImVec2(0.0f, 1.0f);

			//	補間モード設定
			ImGui::Combo<animation::interpolation::types>("Curve Type", curve.type);

			//	リセット（始点・終点だけの直線に戻す）
			ImGui::SameLine();
			if (ImGui::Button("Reset##curve_reset"))
			{
				curve.datas = { DirectX::XMFLOAT2(0.0f, 0.0f), DirectX::XMFLOAT2(1.0f, 1.0f) };
				reload_points();
			}

			//	カーブ編集本体
			const float width = ImGui::GetWindowSize().x - curve_horizontal_margin;
			ImVector<ImCurveEdit::EditPoint> selected_points;
			ImCurveEdit::Edit(*this, ImVec2(width, curve_height), curve_edit_id, nullptr, &selected_points, &selection_lists);

			//	右クリック中の選択点を削除（始点・終点は残す）
			if (!selected_points.empty() && ImGui::IsMouseDown(ImGuiMouseButton_Right))
				remove_points(selected_points);

			//	0..1 にクランプ
			for (auto& point : curve.datas)
				point = math::saturate(point);

			//	Edit / Reset / クランプで datas 側が変化しているのでキャッシュを追従させる
			reload_points();

			ImGui::Text(u8"    アニメーション時間");
		}

	private:
		//	--- ImCurveEdit::Delegate ------------------------------------------------
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

		//	--- 内部ヘルパー ------------------------------------------------------
		//	datas -> point_cache
		void reload_points()
		{
			point_cache.clear();
			if (animation_data == nullptr)
				return;

			for (const auto& d : animation_data->time_curve.datas)
				point_cache.emplace_back(d.x, d.y);
		}

		//	point_cache -> datas（x 昇順にソートしてから書き戻す）
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

		//	選択点を datas から削除する。始点・終点は必ず残す。
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
		std::vector<ImVec2> point_cache;			//	datas を ImCurveEdit 用に複製したもの
		ImVec2 range_min = ImVec2(0.0f, 0.0f);
		ImVec2 range_max = ImVec2(0.0f, 1.0f);

		static constexpr float curve_horizontal_margin = 40.0f;
		static constexpr float curve_height = 300.0f;
		static constexpr unsigned int curve_edit_id = 0xFFFF;
		static constexpr uint32_t curve_color = 0xFF0000FF;
	};

	//------------------------------------------------------------------------------
	//	アニメーションタイムライン編集
	//	ﾃｷﾄｰ('ω')-ﾄｷﾃ
	//------------------------------------------------------------------------------
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

		//	サブフレーム表示倍率。
		//	シーケンサーは整数フレームでしか扱えないため、fps にこの倍率を掛けて
		//	1 フレーム未満のキー時刻も区別できるようにしている。
		static constexpr float sequencer_time_scale = 10.0f;

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
			keyframe_move_pending = false;
			clear_selection();
			clear_active_timeline();
		}

		void render(std::shared_ptr<editor_items::context>& context) override
		{
			if (animation_data == nullptr)
				return;

			if (ImGui::Begin("Animation Timeline"))
			{
				render_toolbar();

				ImGui::Separator();

				if (ImGui::BeginNeoSequencer(sequencer_name.data(), &current_frame, &start_frame, &end_frame, { 0, 0 },
					ImGuiNeoSequencerFlags_AllowLengthChanging | ImGuiNeoSequencerFlags_EnableSelection |
					ImGuiNeoSequencerFlags_AlwaysShowHeader | ImGuiNeoSequencerFlags_Selection_EnableDragging |
					ImGuiNeoSequencerFlags_Selection_EnableDeletion))
				{
					//	シーケンサーのメモリリーク対策で ID を控えておく。
					//	@remainder	選択中にモーションを差し替えるとリークする（時間のある時に修正）
					ImGuiWindow* window = ImGui::GetCurrentWindow();
					sequencer_id = window->IDStack[window->IDStack.size() - 1];

					//	選択情報は毎フレーム ImGui 側の状態から作り直す
					selected.clear();
					render_event_timeline(context);
					render_node_timelines(context);

					const bool dragging = ImGui::NeoIsDraggingSelection();
					ImGui::EndNeoSequencer();

					//	キーフレームのドラッグ移動が終わったら時間順に並べ直す。
					//	（ドラッグ中に並べ替えると ImGui ID がずれて操作が壊れるため待つ）
					if (keyframe_move_pending && !dragging)
					{
						sort_animation_data();
						clear_selection();
						keyframe_move_pending = false;
					}
				}
			}
			ImGui::End();

			render_inspector(context);
		}

	private:
		//	時間 -> サブフレーム番号
		int calculate_frame_index(float second) const
		{
			return static_cast<int>(system_units::convert_sec_to_fps(second * sequencer_time_scale));
		}
		//	サブフレーム番号 -> 時間
		float calculate_second(int frame_index) const
		{
			return system_units::convert_fps_to_sec(static_cast<float>(frame_index)) / sequencer_time_scale;
		}

		//	NeoKeyframe がドラッグでフレーム値を変えていたら時刻へ書き戻す。
		//	並べ替えはドラッグ完了後（render 内）にまとめて行う。
		void write_back_dragged_time(float& time, int frame_before, int frame_after)
		{
			if (frame_after == frame_before)
				return;

			time = std::max(calculate_second(frame_after), 0.0f);
			keyframe_move_pending = true;
		}

		//	track 内に frame と同じフレームのキーが既にあるか（exclude は判定から除外）。
		//	ノードのキーは同一フレームに複数置けない（イベントは重複可）。
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

		//	イベント/ノードのキーフレームを時間順にそろえ直す
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

		//	選択キー(複数可)を実データから削除する。
		//	添字が大きい方から消していくことで「他の選択キーの添字をずらす」処理が不要になる。
		//	※ selected 自体からの除去は呼び出し側で行うこと。
		void delete_selected_data(const std::vector<selected_key>& targets)
		{
			if (animation_data == nullptr)
				return;

			std::vector<UINT> event_indices;
			std::vector<std::pair<UINT, UINT>> node_key_indices;	//	{track_index, key_index}

			for (const auto& target : targets)
			{
				if (target.category == timeline_category::event)
					event_indices.push_back(target.track_index);
				else if (target.category == timeline_category::node)
					node_key_indices.emplace_back(target.track_index, target.key_index);
			}

			//	イベント: 添字が大きい方から削除
			std::sort(event_indices.begin(), event_indices.end(), std::greater<UINT>());
			for (UINT index : event_indices)
				animation_data->events.erase(animation_data->events.begin() + index);

			//	ノードのキーフレーム: key_index が大きい方から削除
			//	(track_index が違えば別配列なので key_index の降順だけで十分)
			std::sort(node_key_indices.begin(), node_key_indices.end(),
				[](const auto& l, const auto& r) { return l.second > r.second; });
			for (const auto& [track_index, key_index] : node_key_indices)
			{
				auto& track = animation_data->nodes[track_index];
				track.nodes.erase(track.nodes.begin() + key_index);
			}

			sort_animation_data();
		}

		//	タイムライン操作ツールバー（追加 / 削除）。
		//	ボタンは常に表示し、対象を選択していないときは押せないようにする。
		void render_toolbar()
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
					current_frame = calculate_frame_index(event.time);
					animation_data->events.push_back(event);
				}
				else
				{
					animation::node key;
					key.time = calculate_second(current_frame);
					current_frame = calculate_frame_index(key.time);
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
							if (selected.empty())
								clear_active_timeline();
							selected.push_back({ timeline_category::event, event_index, 0 });
						}

						ImGui::PopID();
						++event_index;
					}

					//	この行が選択中なら編集対象にする
					if (ImGui::IsNeoTimelineSelected())
					{
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
					auto label = system_utility::convert_utf8(track.node_name);
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
								if (selected.empty())
									clear_active_timeline();
								selected.push_back({ timeline_category::node, track_index, key_index });
							}

							ImGui::PopID();
							++key_index;
						}

						//	この行が選択中なら編集対象にする
						if (ImGui::IsNeoTimelineSelected())
						{
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
			ImGui::DragFloat("time", &node.time, system_units::convert_fps_to_sec(1.0f), 0.0f, 0.0f, "%.6f");
			ImGui::DragFloat3("scale", &node.scale.x, 0.01f);

			math::rotator rotator = node.rotation;
			ImGui::DragFloat3("rotation", &rotator.x, 0.01f);
			node.rotation = rotator;

			ImGui::DragFloat3("translation", &node.translation.x, 0.01f);
		}

		void edit_event_inspector(animation::event& event)
		{
			ImGui::DragFloat("time", &event.time, system_units::convert_fps_to_sec(1.0f), 0.0f, 0.0f, "%.6f");
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
			if (ImGui::Begin("Animation Inspector"))
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
							auto label = system_utility::convert_utf8(track.node_name);
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
			ImGui::End();
		}

	private:
		animation::data* animation_data = nullptr;
		ImGuiID sequencer_id = 0;
		timeline_category active_timeline = timeline_category::none;

		int32_t current_frame = 0;
		int32_t start_frame = 0;
		int32_t end_frame = 0;

		std::vector<selected_key> selected;
		UINT selected_node_index = 0;
		bool keyframe_move_pending = false;	//	ドラッグ移動があり、まだ並べ替えていない
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
	//	フレーム間のカーブは取り扱っていない
	if (ImGui::TreeNode("Curve Editor"))
	{
		auto& editor = acquire_editor<animation_interpolation_editor>(context->interpolation_editor);
		editor.set_data(&animation_data);
		editor.render(context);

		ImGui::TreePop();
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

#endif	//	defined(USE_IMGUI)
