#pragma once

//	エンジンシェーダーの置き場所
#define	ENGINE_ASSETS_DIRECTORY		"EngineAssets"

//	エンジンシェーダーの置き場所
#define	ENGINE_SHADER_DIRECTORY		ENGINE_ASSETS_DIRECTORY "/EngineShader"

//	エンジンシェーダーのパス生成マクロ
//	例: ENGINE_SHADER_PATH("Common/xxx.hlsl")
#define	ENGINE_SHADER_PATH(path)	ENGINE_SHADER_DIRECTORY "/" path
