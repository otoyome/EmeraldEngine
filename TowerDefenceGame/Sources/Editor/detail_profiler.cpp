#include "Editor/detail_profiler.h"

#if defined(USE_IMGUI)
#include "Object/game_object.h"

namespace
{
	//	テーブル表示用の行データ
	struct table_row
	{
		static constexpr int max_texts = 2;
		static constexpr int max_values = 8;

		std::array<std::string_view, max_texts> texts{};
		std::array<float, max_values> values{};
		const detail_profiler::history* history = nullptr;
	};

	//	ソート指定に従って行を並べ替える
	void sort_rows(std::vector<table_row>& rows, int text_count)
	{
		const ImGuiTableSortSpecs* specs = ImGui::TableGetSortSpecs();
		if (!specs || specs->SpecsCount <= 0)
			return;

		std::stable_sort(rows.begin(), rows.end(),
			[specs, text_count](const table_row& l, const table_row& r)
			{
				for (int index = 0; index < specs->SpecsCount; ++index)
				{
					const ImGuiTableColumnSortSpecs& spec = specs->Specs[index];
					const int column = spec.ColumnIndex;
					int compare = 0;
					if (column < text_count)
					{
						compare = l.texts[column].compare(r.texts[column]);
					}
					else
					{
						const float lv = l.values[column - text_count];
						const float rv = r.values[column - text_count];
						compare = (lv < rv) ? -1 : (lv > rv ? 1 : 0);
					}
					if (compare != 0)
						return spec.SortDirection == ImGuiSortDirection_Ascending ? compare < 0 : compare > 0;
				}
				return false;
			});
	}

	//	時間の列
	void time_cell(float time, float ratio)
	{
		ImVec4 color = ImGui::GetStyleColorVec4(ImGuiCol_Text);
		if (ratio >= 0.25f)
			color = ImVec4(1.0f, 0.35f, 0.35f, 1.0f);
		else if (ratio >= 0.1f)
			color = ImVec4(1.0f, 0.8f, 0.3f, 1.0f);
		ImGui::TextColored(color, "%.3f", time);
	}

	//	割合バー
	void ratio_bar(float ratio)
	{
		char text[32];
		snprintf(text, sizeof(text), "%.1f%%", ratio * 100.0f);
		ImGui::ProgressBar(std::clamp(ratio, 0.0f, 1.0f), ImVec2(-FLT_MIN, 0.0f), text);
	}
}

void detail_profiler::history::push(float value)
{
	values[head] = value;
	head = (head + 1) % history_size;
	filled = std::min(filled + 1, history_size);

	float sum = 0.0f;
	float max_value = 0.0f;
	for (int index = 0; index < filled; ++index)
	{
		sum += values[index];
		max_value = std::max(max_value, values[index]);
	}
	current = value;
	average = sum / static_cast<float>(filled);
	peak = max_value;
}

detail_profiler::detail_profiler()
: instance_serial(++instance_counter)
{
}

detail_profiler::~detail_profiler()
{
}

void detail_profiler::begin_frame()
{
	std::lock_guard<std::mutex> lock(mutex);
	for (auto& values : frame_values)
		values.clear();
	frame_submitted.fill(false);
	object_frame_values.clear();
	for (auto& buffer : object_thread_buffers)
		buffer->values.clear();
}

void detail_profiler::end_frame()
{
	std::lock_guard<std::mutex> lock(mutex);

	//	スレッド毎のオブジェクト計測結果を統合
	for (auto& buffer : object_thread_buffers)
	{
		for (auto& [id, value] : buffer->values)
		{
			auto it = object_frame_values.find(id);
			if (it == object_frame_values.end())
			{
				object_frame_values.emplace(id, std::move(value));
				continue;
			}
			for (size_t index = 0; index < object_function_count; ++index)
				it->second.times[index] += value.times[index];
		}
		buffer->values.clear();
	}

	if (!paused)
	{
		for (size_t index = 0; index < static_cast<size_t>(category::max); ++index)
		{
			auto kind = static_cast<category>(index);

			//	GPUの結果は数フレーム遅れて届くため、届いたフレームのみ反映する
			if ((kind == category::renderer_gpu || kind == category::gpu_scope) && !frame_submitted[index])
				continue;

			commit_flat(kind);
		}
		commit_objects();
	}

	for (auto& values : frame_values)
		values.clear();
	frame_submitted.fill(false);
	object_frame_values.clear();
}

void detail_profiler::add_time(category kind, std::string_view name, float time)
{
	if (!enabled)
		return;

	std::lock_guard<std::mutex> lock(mutex);
	auto& values = frame_values[static_cast<size_t>(kind)];
	auto it = values.find(std::string(name));
	if (it == values.end())
		it = values.emplace(std::string(name), frame_value{}).first;
	it->second.time += time;
	++it->second.calls;
}

detail_profiler::object_thread_buffer& detail_profiler::get_object_thread_buffer()
{
	thread_local object_thread_buffer* buffer = nullptr;
	thread_local UINT buffer_owner = 0;
	if (!buffer || buffer_owner != instance_serial)
	{
		std::lock_guard<std::mutex> lock(mutex);
		object_thread_buffers.emplace_back(std::make_unique<object_thread_buffer>());
		buffer = object_thread_buffers.back().get();
		buffer_owner = instance_serial;
	}
	return *buffer;
}

void detail_profiler::add_object_time(object_profile_function function, const game_object* object, float time)
{
	if (!enabled || !object)
		return;

	auto& values = get_object_thread_buffer().values;
	auto it = values.find(object->get_id());
	if (it == values.end())
	{
		object_frame_value value;
		value.label = std::string(object->get_name()) + "#" + std::to_string(object->get_id());
		value.type = std::string(object->get_type_name());
		it = values.emplace(object->get_id(), std::move(value)).first;
	}
	it->second.times[static_cast<size_t>(function)] += time;
}

void detail_profiler::begin_gpu_frame()
{
	auto& frame = gpu_scope_frames[gpu_scope_frame_index];
	gpu_scope_recording = false;

	//	再利用するスロットの結果が揃っていなければ今フレームは計測しない
	if (frame.pending)
	{
		bool ready = true;
		for (UINT index = 0; index < frame.used; ++index)
		{
			frame.timers[index]->get_result(true);
			if (!frame.timers[index]->is_finalized())
				ready = false;
		}
		if (!ready)
			return;

		frame.pending = false;
		if (enabled)
			submit_gpu_times(frame);
	}

	frame.used = 0;
	gpu_scope_recording = enabled;
}

void detail_profiler::end_gpu_frame()
{
	if (!gpu_scope_recording)
		return;

	gpu_scope_frames[gpu_scope_frame_index].pending = true;
	gpu_scope_frame_index = (gpu_scope_frame_index + 1) % gpu_scope_latency;
	gpu_scope_recording = false;
}

void detail_profiler::begin_gpu_scope()
{
	if (!gpu_scope_recording)
		return;

	auto& frame = gpu_scope_frames[gpu_scope_frame_index];
	if (frame.used >= frame.timers.size())
	{
		frame.timers.emplace_back(std::make_unique<gpu_timer>(render_data::instance()->get_immediate_context()));
		frame.names.emplace_back();
		frame.owner_names.emplace_back();
	}
	frame.timers[frame.used]->begin();
}

void detail_profiler::end_gpu_scope(std::string_view owner, std::string_view name)
{
	if (!gpu_scope_recording)
		return;

	auto& frame = gpu_scope_frames[gpu_scope_frame_index];
	frame.timers[frame.used]->end();
	frame.names[frame.used] = name;
	frame.owner_names[frame.used] = owner;
	++frame.used;
}

void detail_profiler::submit_gpu_times(const gpu_scope_frame& frame)
{
	std::lock_guard<std::mutex> lock(mutex);
	auto& scope_values = frame_values[static_cast<size_t>(category::gpu_scope)];
	auto& renderer_values = frame_values[static_cast<size_t>(category::renderer_gpu)];
	scope_values.clear();
	renderer_values.clear();
	for (UINT index = 0; index < frame.used; ++index)
	{
		const float time = frame.timers[index]->get_result(true);

		auto& scope = scope_values[frame.owner_names[index] + "/" + frame.names[index]];
		scope.time += time;
		++scope.calls;

		auto& renderer = renderer_values[frame.owner_names[index]];
		renderer.time += time;
		++renderer.calls;
	}
	frame_submitted[static_cast<size_t>(category::gpu_scope)] = true;
	frame_submitted[static_cast<size_t>(category::renderer_gpu)] = true;
}

std::string detail_profiler::get_display_type_name(const std::type_info& type)
{
	std::string name = type.name();
	for (std::string_view prefix : { std::string_view("class "), std::string_view("struct ") })
	{
		if (name.compare(0, prefix.size(), prefix) == 0)
		{
			name.erase(0, prefix.size());
			break;
		}
	}
	return name;
}

void detail_profiler::commit_flat(category kind)
{
	auto& values = frame_values[static_cast<size_t>(kind)];
	auto& list = entries[static_cast<size_t>(kind)];

	for (auto& [name, value] : values)
	{
		auto& entry = list[name];
		entry.time.push(value.time);
		entry.calls = value.calls;
		entry.missing_frames = 0;
	}

	//	今フレーム計測されなかった項目は0を積み、一定フレーム計測されなければ削除
	for (auto it = list.begin(); it != list.end();)
	{
		if (values.find(it->first) != values.end())
		{
			++it;
			continue;
		}

		it->second.time.push(0.0f);
		it->second.calls = 0;
		if (++it->second.missing_frames > history_size)
			it = list.erase(it);
		else
			++it;
	}
}

void detail_profiler::commit_objects()
{
	//	型単位に集計
	struct type_value
	{
		std::array<float, object_function_count> times{};
		UINT count = 0;
	};
	std::unordered_map<std::string, type_value> type_values;
	for (auto& [id, value] : object_frame_values)
	{
		auto& type = type_values[value.type];
		for (size_t index = 0; index < object_function_count; ++index)
			type.times[index] += value.times[index];
		++type.count;
	}

	auto commit = [](std::map<std::string, object_entry>& list, const std::string& key, const std::string& label, const std::string& type, const std::array<float, object_function_count>& times, UINT count)
	{
		auto& entry = list[key];
		entry.label = label;
		entry.type = type;
		float total = 0.0f;
		for (size_t index = 0; index < object_function_count; ++index)
		{
			entry.times[index].push(times[index]);
			total += times[index];
		}
		entry.total.push(total);
		entry.instance_count = count;
		entry.missing_frames = 0;
	};

	auto remove_missing = [](std::map<std::string, object_entry>& list, const std::unordered_map<std::string, bool>& alive)
	{
		for (auto it = list.begin(); it != list.end();)
		{
			if (alive.find(it->first) != alive.end())
			{
				++it;
				continue;
			}

			for (auto& time : it->second.times)
				time.push(0.0f);
			it->second.total.push(0.0f);
			it->second.instance_count = 0;
			if (++it->second.missing_frames > history_size)
				it = list.erase(it);
			else
				++it;
		}
	};

	std::unordered_map<std::string, bool> alive;
	for (auto& [id, value] : object_frame_values)
	{
		commit(object_entries, value.label, value.label, value.type, value.times, 1);
		alive[value.label] = true;
	}
	remove_missing(object_entries, alive);

	alive.clear();
	for (auto& [type, value] : type_values)
	{
		commit(object_type_entries, type, type, type, value.times, value.count);
		alive[type] = true;
	}
	remove_missing(object_type_entries, alive);
}

float detail_profiler::get_value(const history& data) const
{
	switch (value_mode)
	{
	case display_value::current:	return data.current;
	case display_value::peak:		return data.peak;
	default:						return data.average;
	}
}

void detail_profiler::imgui_render()
{
	ImGui::SetNextWindowSize(ImVec2(960.0f, 640.0f), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("detail_profiler"))
	{
		ImGui::End();
		return;
	}

	std::lock_guard<std::mutex> lock(mutex);

	//	操作
	{
		ImGui::Checkbox("enable", &enabled);
		ImGui::SameLine();
		ImGui::Checkbox("pause", &paused);
		ImGui::SameLine();
		int mode = static_cast<int>(value_mode);
		ImGui::RadioButton("current", &mode, static_cast<int>(display_value::current));
		ImGui::SameLine();
		ImGui::RadioButton("average", &mode, static_cast<int>(display_value::average));
		ImGui::SameLine();
		ImGui::RadioButton("peak", &mode, static_cast<int>(display_value::peak));
		value_mode = static_cast<display_value>(mode);
		ImGui::SameLine();
		filter.Draw("filter", 200.0f);
		ImGui::TextDisabled("average / peak : last %d frames (GPU : frames with results)", history_size);
	}

	if (ImGui::BeginTabBar("detail_profiler_tabs"))
	{
		if (ImGui::BeginTabItem("frame"))
		{
			imgui_render_flat_table("frame_table", category::frame, "cpu ms");
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("object type"))
		{
			imgui_render_object_table("object_type_table", object_type_entries, true);
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("object instance"))
		{
			imgui_render_object_table("object_instance_table", object_entries, false);
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("scene renderer"))
		{
			imgui_render_renderer_table();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("render job cpu"))
		{
			imgui_render_flat_table("render_job_cpu_table", category::render_job_cpu, "cpu ms");
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("gpu scope"))
		{
			imgui_render_flat_table("gpu_scope_table", category::gpu_scope, "gpu ms");
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}

	ImGui::End();
}

void detail_profiler::imgui_render_flat_table(const char* id, category kind, const char* time_label)
{
	auto& list = entries[static_cast<size_t>(kind)];

	//	行データ作成
	std::vector<table_row> rows;
	rows.reserve(list.size());
	float total = 0.0f;
	for (auto& [name, entry] : list)
	{
		if (!filter.PassFilter(name.data()))
			continue;

		table_row row;
		row.texts[0] = name;
		row.values[0] = static_cast<float>(entry.calls);
		row.values[1] = get_value(entry.time);
		row.history = &entry.time;
		total += row.values[1];
		rows.emplace_back(row);
	}

	//	frameカテゴリは入れ子の計測を含むため、割合は最大値を基準にする
	float base = total;
	if (kind == category::frame)
	{
		base = 0.0f;
		for (auto& row : rows)
			base = std::max(base, row.values[1]);
	}
	ImGui::Text("total : %.3f ms  (%d items)", kind == category::frame ? base : total, static_cast<int>(rows.size()));

	constexpr ImGuiTableFlags flags = ImGuiTableFlags_Sortable | ImGuiTableFlags_SortMulti | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit;
	if (ImGui::BeginTable(id, 4, flags))
	{
		ImGui::TableSetupScrollFreeze(0, 1);
		ImGui::TableSetupColumn("name", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("calls", ImGuiTableColumnFlags_PreferSortDescending);
		ImGui::TableSetupColumn(time_label, ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_PreferSortDescending);
		ImGui::TableSetupColumn("ratio", ImGuiTableColumnFlags_NoSort | ImGuiTableColumnFlags_WidthFixed, 140.0f);
		ImGui::TableHeadersRow();

		sort_rows(rows, 1);

		for (auto& row : rows)
		{
			const float ratio = base > 0.0f ? row.values[1] / base : 0.0f;
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::TextUnformatted(row.texts[0].data(), row.texts[0].data() + row.texts[0].size());
			if (ImGui::IsItemHovered())
				imgui_render_history_tooltip(std::string(row.texts[0]).data(), *row.history);
			ImGui::TableSetColumnIndex(1);
			ImGui::Text("%u", static_cast<UINT>(row.values[0]));
			ImGui::TableSetColumnIndex(2);
			time_cell(row.values[1], ratio);
			ImGui::TableSetColumnIndex(3);
			ratio_bar(ratio);
		}
		ImGui::EndTable();
	}
}

void detail_profiler::imgui_render_renderer_table()
{
	auto& issue_list = entries[static_cast<size_t>(category::renderer_issue)];
	auto& job_list = entries[static_cast<size_t>(category::renderer_job_cpu)];
	auto& gpu_list = entries[static_cast<size_t>(category::renderer_gpu)];

	//	3種の計測をレンダラー名でまとめる
	std::map<std::string_view, table_row> merged;
	auto merge = [&](std::map<std::string, flat_entry>& list, int value_index, int calls_index)
	{
		for (auto& [name, entry] : list)
		{
			if (!filter.PassFilter(name.data()))
				continue;

			auto& row = merged[name];
			row.texts[0] = name;
			row.values[value_index] = get_value(entry.time);
			if (calls_index >= 0)
				row.values[calls_index] = static_cast<float>(entry.calls);
			if (value_index == 3 || !row.history)
				row.history = &entry.time;
		}
	};
	merge(issue_list, 0, -1);
	merge(job_list, 1, 2);
	merge(gpu_list, 3, -1);

	std::vector<table_row> rows;
	rows.reserve(merged.size());
	float cpu_total = 0.0f;
	float gpu_total = 0.0f;
	for (auto& [name, row] : merged)
	{
		cpu_total += row.values[0] + row.values[1];
		gpu_total += row.values[3];
		rows.emplace_back(row);
	}
	ImGui::Text("cpu total (issue + job) : %.3f ms   gpu total : %.3f ms", cpu_total, gpu_total);
	ImGui::TextDisabled("issue : renderer::render() on main thread / job : command recording on worker threads");

	constexpr ImGuiTableFlags flags = ImGuiTableFlags_Sortable | ImGuiTableFlags_SortMulti | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit;
	if (ImGui::BeginTable("renderer_table", 6, flags))
	{
		ImGui::TableSetupScrollFreeze(0, 1);
		ImGui::TableSetupColumn("renderer", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("issue cpu ms", ImGuiTableColumnFlags_PreferSortDescending);
		ImGui::TableSetupColumn("job cpu ms", ImGuiTableColumnFlags_PreferSortDescending);
		ImGui::TableSetupColumn("jobs", ImGuiTableColumnFlags_PreferSortDescending);
		ImGui::TableSetupColumn("gpu ms", ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_PreferSortDescending);
		ImGui::TableSetupColumn("gpu ratio", ImGuiTableColumnFlags_NoSort | ImGuiTableColumnFlags_WidthFixed, 140.0f);
		ImGui::TableHeadersRow();

		sort_rows(rows, 1);

		for (auto& row : rows)
		{
			const float cpu_ratio_issue = cpu_total > 0.0f ? row.values[0] / cpu_total : 0.0f;
			const float cpu_ratio_job = cpu_total > 0.0f ? row.values[1] / cpu_total : 0.0f;
			const float gpu_ratio = gpu_total > 0.0f ? row.values[3] / gpu_total : 0.0f;
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::TextUnformatted(row.texts[0].data(), row.texts[0].data() + row.texts[0].size());
			if (ImGui::IsItemHovered() && row.history)
				imgui_render_history_tooltip(std::string(row.texts[0]).data(), *row.history);
			ImGui::TableSetColumnIndex(1);
			time_cell(row.values[0], cpu_ratio_issue);
			ImGui::TableSetColumnIndex(2);
			time_cell(row.values[1], cpu_ratio_job);
			ImGui::TableSetColumnIndex(3);
			ImGui::Text("%u", static_cast<UINT>(row.values[2]));
			ImGui::TableSetColumnIndex(4);
			time_cell(row.values[3], gpu_ratio);
			ImGui::TableSetColumnIndex(5);
			ratio_bar(gpu_ratio);
		}
		ImGui::EndTable();
	}
}

void detail_profiler::imgui_render_object_table(const char* id, std::map<std::string, object_entry>& list, bool type_mode)
{
	constexpr int function_count = static_cast<int>(object_function_count);
	static_assert(function_count + 2 <= table_row::max_values);

	//	行データ作成
	std::vector<table_row> rows;
	rows.reserve(list.size());
	float total = 0.0f;
	for (auto& [key, entry] : list)
	{
		if (!filter.PassFilter(entry.label.data()) && !filter.PassFilter(entry.type.data()))
			continue;

		table_row row;
		row.texts[0] = entry.label;
		row.texts[1] = entry.type;
		for (int index = 0; index < function_count; ++index)
			row.values[index] = get_value(entry.times[index]);
		row.values[function_count] = get_value(entry.total);
		row.values[function_count + 1] = static_cast<float>(entry.instance_count);
		row.history = &entry.total;
		total += row.values[function_count];
		rows.emplace_back(row);
	}
	ImGui::Text("total : %.3f ms  (%d items)", total, static_cast<int>(rows.size()));
	ImGui::TextDisabled("sum of per-object cpu time (objects run in parallel, so this can exceed wall time)");

	//	型モード: 名前/個数/各関数/合計、個別モード: 名前/型/各関数/合計
	const int column_count = 2 + function_count + 1;
	constexpr ImGuiTableFlags flags = ImGuiTableFlags_Sortable | ImGuiTableFlags_SortMulti | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_Hideable;
	if (ImGui::BeginTable(id, column_count, flags))
	{
		ImGui::TableSetupScrollFreeze(1, 1);
		ImGui::TableSetupColumn(type_mode ? "type" : "object", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn(type_mode ? "count" : "type", type_mode ? ImGuiTableColumnFlags_PreferSortDescending : ImGuiTableColumnFlags_None);
		for (int index = 0; index < function_count; ++index)
			ImGui::TableSetupColumn(magic_enum::enum_names<object_profile_function>()[index].data(), ImGuiTableColumnFlags_PreferSortDescending);
		ImGui::TableSetupColumn("total ms", ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_PreferSortDescending);
		ImGui::TableHeadersRow();

		//	型モードの個数列は数値でソートするため、列番号を値配列へ割り当て直す
		if (type_mode)
		{
			for (auto& row : rows)
			{
				std::array<float, table_row::max_values> values{};
				values[0] = row.values[function_count + 1];
				for (int index = 0; index <= function_count; ++index)
					values[index + 1] = row.values[index];
				row.values = values;
			}
			sort_rows(rows, 1);
		}
		else
		{
			sort_rows(rows, 2);
		}

		const int value_offset = type_mode ? 1 : 0;
		for (auto& row : rows)
		{
			const float row_total = row.values[value_offset + function_count];
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::TextUnformatted(row.texts[0].data(), row.texts[0].data() + row.texts[0].size());
			if (ImGui::IsItemHovered())
				imgui_render_history_tooltip(std::string(row.texts[0]).data(), *row.history);
			ImGui::TableSetColumnIndex(1);
			if (type_mode)
				ImGui::Text("%u", static_cast<UINT>(row.values[0]));
			else
				ImGui::TextUnformatted(row.texts[1].data(), row.texts[1].data() + row.texts[1].size());
			for (int index = 0; index < function_count; ++index)
			{
				ImGui::TableSetColumnIndex(2 + index);
				const float value = row.values[value_offset + index];
				if (value > 0.0f)
					time_cell(value, total > 0.0f ? value / total : 0.0f);
				else
					ImGui::TextDisabled("-");
			}
			ImGui::TableSetColumnIndex(2 + function_count);
			time_cell(row_total, total > 0.0f ? row_total / total : 0.0f);
		}
		ImGui::EndTable();
	}
}

void detail_profiler::imgui_render_history_tooltip(const char* label, const history& data)
{
	ImGui::BeginTooltip();
	ImGui::TextUnformatted(label);
	ImGui::Text("current %.3f / average %.3f / peak %.3f ms", data.current, data.average, data.peak);

	//	古い順に並べ直して表示
	std::array<float, history_size> values{};
	const int start = data.filled < history_size ? 0 : data.head;
	for (int index = 0; index < data.filled; ++index)
		values[index] = data.values[(start + index) % history_size];
	ImGui::PlotLines("##history", values.data(), data.filled, 0, nullptr, 0.0f, std::max(data.peak, 0.001f), ImVec2(320.0f, 80.0f));
	ImGui::EndTooltip();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
detail_profiler_cpu_scope::detail_profiler_cpu_scope(detail_profiler::category kind, std::string_view name)
: active(detail_profiler::is_valid() && detail_profiler::instance()->is_enabled())
, kind(kind)
, name(name)
{
	if (active)
		timer.begin();
}

detail_profiler_cpu_scope::~detail_profiler_cpu_scope()
{
	if (!active)
		return;

	timer.end();
	detail_profiler::instance()->add_time(kind, name, timer.get_result());
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
detail_profiler_object_scope::detail_profiler_object_scope(object_profile_function function, const game_object* object)
: active(object && detail_profiler::is_valid() && detail_profiler::instance()->is_enabled())
, function(function)
, object(object)
{
	if (active)
		timer.begin();
}

detail_profiler_object_scope::~detail_profiler_object_scope()
{
	if (!active)
		return;

	timer.end();
	detail_profiler::instance()->add_object_time(function, object, timer.get_result());
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
detail_profiler_render_job_scope::detail_profiler_render_job_scope(std::string_view owner, std::string_view name)
: active(detail_profiler::is_valid() && detail_profiler::instance()->is_enabled())
, owner(owner)
, name(name)
{
	if (active)
		timer.begin();
}

detail_profiler_render_job_scope::~detail_profiler_render_job_scope()
{
	if (!active)
		return;

	timer.end();
	const float time = timer.get_result();

	std::string job_name;
	job_name.reserve(owner.size() + 1 + name.size());
	job_name.append(owner).append("/").append(name);
	detail_profiler::instance()->add_time(detail_profiler::category::render_job_cpu, job_name, time);
	detail_profiler::instance()->add_time(detail_profiler::category::renderer_job_cpu, owner, time);
}

#endif	//	defined(USE_IMGUI)
