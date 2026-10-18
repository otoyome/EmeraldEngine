#pragma once

#include "texture.h"

#include <memory>

//	@tool フレームグラフから抜き出したテクスチャ（UE の IPooledRenderTarget に相当）
//	@tool 誰かが持っている間は、フレームグラフのプールが別の用途に使い回さない
struct pooled_texture_ref
{
	texture_ptr texture;			//	@tool 抜き出したフレームのグラフを実行する時に入る（それまでと、使われずに除かれた時は null）
	D3D11_TEXTURE2D_DESC desc{};
};
using pooled_texture_ref_ptr = std::shared_ptr<pooled_texture_ref>;
