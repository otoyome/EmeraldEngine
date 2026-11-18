#include "Editor/place_objects_window.h"

#if defined(USE_IMGUI)
#include <algorithm>
#include <cctype>
#include <string>

#include "imgui.h"
#include "Editor/object_edit_operations.h"
#include "Object/engine_context.h"
#include "Object/world.h"
#include "Object/game_object_registry.h"
#include "Object/spatial_object.h"
#include "Scenes/scene_resource_utility.h"

void place_objects_window::render_placeable_types(std::string_view filter,
	const std::function<bool(const char* category)>& begin_category,
	const std::function<void()>& end_category,
	const std::function<void(const std::string& type)>& render_type)
{
	if (!game_object_registry::is_valid())
		return;

	//	ŒŸõ‚Í‘å•¶šE¬•¶š‚ğ‹æ•Ê‚µ‚È‚¢
	auto to_lower = [](std::string text)
	{
		std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return text;
	};
	const std::string lower_filter = to_lower(std::string(filter));

	//	¡ŠJ‚¢‚Ä‚¢‚éƒJƒeƒSƒŠ[‚ÌŠK‘w
	struct opened_category
	{
		std::string name;
		bool open = false;
	};
	std::vector<opened_category> path;
	auto close_to = [&](size_t depth)
	{
		while (path.size() > depth)
		{
			if (path.back().open)
				end_category();
			path.pop_back();
		}
	};

	for (const game_object_registry::type_entry& entry : game_object_registry::instance()->get_placeable_types())
	{
		if (!lower_filter.empty() && to_lower(entry.name).find(lower_filter) == std::string::npos)
			continue;

		//	ƒJƒeƒSƒŠ[‚ğ / ‚ÅŠK‘w‚É•ª‚¯‚é
		std::vector<std::string> names;
		for (size_t begin = 0; begin <= entry.category.size();)
		{
			size_t end = entry.category.find('/', begin);
			if (end == std::string::npos)
				end = entry.category.size();
			names.emplace_back(entry.category.substr(begin, end - begin));
			begin = end + 1;
		}

		//	‘O‚ÌŒ^‚Æ“¯‚¶ŠK‘w‚ÍŠJ‚¢‚½‚Ü‚Üg‚¢Aˆá‚¤Š‚©‚ç•Â‚¶‚ÄŠJ‚«’¼‚·
		size_t common = 0;
		while (common < path.size() && common < names.size() && path[common].name == names[common])
			++common;
		close_to(common);
		for (size_t i = common; i < names.size(); ++i)
		{
			const bool parent_open = path.empty() || path.back().open;
			path.push_back({ names[i], parent_open && begin_category(names[i].c_str()) });
		}

		if (path.empty() || path.back().open)
			render_type(entry.name);
	}
	close_to(0);
}

void place_objects_window::imgui_render(object_edit_operations& editor)
{
	//	Œ^‚Ìˆê——
	if (ImGui::Begin(editor.get_edit_context().make_window_name("place objects").c_str()))
		imgui_render_contents(editor);
	ImGui::End();
}

void place_objects_window::imgui_render_contents(object_edit_operations& editor)
{
	ImGui::SetNextItemWidth(-FLT_MIN);
	ImGui::InputTextWithHint("##filter", "search", filter, sizeof(filter));

	const bool filtering = filter[0] != '\0';
	render_placeable_types(filter,
		[filtering](const char* category)
		{
			if (filtering)
				ImGui::SetNextItemOpen(true);
			return ImGui::TreeNodeEx(category, ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth);
		},
		[]() { ImGui::TreePop(); },
		[&editor](const std::string& type)
		{
			ImGui::Selectable(type.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick);
			if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
				editor.place_object(type);
			if (ImGui::BeginDragDropSource())
			{
				ImGui::SetDragDropPayload(payload_type, type.c_str(), type.size() + 1);
				ImGui::TextUnformatted(type.c_str());
				ImGui::EndDragDropSource();
			}
		});
}
#endif	//	defined(USE_IMGUI)
