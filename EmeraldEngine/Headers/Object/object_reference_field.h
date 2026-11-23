#pragma once

#if defined(USE_IMGUI)
#include <memory>
#include <string_view>

#include "imgui.h"
#include "Object/engine_context.h"
#include "Object/object_manager.h"

//	他のオブジェクトへの参照をインスペクターで選ぶ
class object_reference_field
{
public:
	//	ワールドにある Type のオブジェクトから参照先を選ぶ
	template<class Type>
	static bool imgui_combo(const char* label, engine_context& context, std::shared_ptr<Type>& target)
	{
		bool changed = false;
		const core::cache_string current_name(target ? target->get_name() : std::string_view("(none)"));
		if (ImGui::BeginCombo(label, current_name.get<core::utf8_cache>().data()))
		{
			if (ImGui::Selectable("(none)", !target))
			{
				target.reset();
				changed = true;
			}
			for (const std::shared_ptr<Type>& object : context.get_objects().search_objects<Type>())
			{
				ImGui::PushID(object.get());
				const core::cache_string name(object->get_name());
				if (ImGui::Selectable(name.get<core::utf8_cache>().data(), object == target))
				{
					target = object;
					changed = true;
				}
				ImGui::PopID();
			}
			ImGui::EndCombo();
		}
		return changed;
	}

	//	ワールドにある Type のオブジェクトから参照先を選ぶ
	template<class Type>
	static bool imgui_combo(const char* label, engine_context& context, std::weak_ptr<Type>& target)
	{
		std::shared_ptr<Type> locked = target.lock();
		if (!imgui_combo(label, context, locked))
			return false;
		target = locked;
		return true;
	}
};
#endif	//	defined(USE_IMGUI)
