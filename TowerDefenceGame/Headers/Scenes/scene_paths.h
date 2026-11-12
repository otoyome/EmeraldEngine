#pragma once

#include <string_view>

//	エンジンが使うシーンファイル
namespace scene_paths
{
	//	起動シーンの指定が無いときに読むシーン
	constexpr std::string_view default_scene = "EngineAssets/Scenes/Default.escene";

	//	エディター用のシーン
	constexpr std::string_view model_editor = "EngineAssets/Scenes/Editor/ModelEditor.escene";
	constexpr std::string_view material_editor = "EngineAssets/Scenes/Editor/MaterialEditor.escene";
	constexpr std::string_view particle_editor = "EngineAssets/Scenes/Editor/ParticleEditor.escene";
	constexpr std::string_view timeline_editor = "EngineAssets/Scenes/Editor/TimelineEditor.escene";
}
