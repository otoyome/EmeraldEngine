#include "Editor/Windows/model_buffer_pool_window.h"

#if defined(USE_IMGUI)
#include "imgui.h"

#include "Model/model_buffer_pool.h"

void model_buffer_pool_window::render_window(std::shared_ptr<editor_items::context>& /*context*/, bool& open)
{
	if (ImGui::Begin("model_buffer_pool", &open))
	{
		//	モデル頂点/インデックスバッファプールの使用量表示
		if (model_buffer_pool_manager::is_valid())
		{
			for (auto& usage : model_buffer_pool_manager::instance()->get_pool_usages())
			{
				float fraction = usage.capacity_in_bytes > 0 ?
					static_cast<float>(usage.used_in_bytes) / static_cast<float>(usage.capacity_in_bytes) : 0.0f;

				std::string text = std::to_string(usage.used_in_bytes / 1024);
				text += " / ";
				text += std::to_string(usage.capacity_in_bytes / 1024);
				text += " [KB]";

				ImGui::Text("%s", usage.label.c_str());
				ImGui::ProgressBar(fraction, ImVec2(-1, 0), text.c_str());
			}
		}
	}
	ImGui::End();
}
#endif	//	defined(USE_IMGUI)
