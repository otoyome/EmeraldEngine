#pragma once

#include <cstdint>
#include <memory>

#include "render_pass.h"

//	@tool 描画の出力先（バックバッファ、または ImGui で表示するテクスチャ）
class render_output
{
public:
	enum class kinds
	{
		back_buffer,	//	@tool スワップチェーン
		texture,		//	@tool スワップチェーンと同じ形式のテクスチャ
	};

public:
	static std::shared_ptr<render_output> create_back_buffer();
	static std::shared_ptr<render_output> create_texture(int width, int height);

	render_output(const render_output&) = delete;
	render_output& operator=(const render_output&) = delete;

public:
	kinds get_kind() const { return kind; }
	int get_width() const { return width; }
	int get_height() const { return height; }

	//	@tool 描画パスに入れる描画先
	render_target_view make_render_target_view();

	//	@tool テクスチャのときだけ有効（ImGui::Image に渡す）
	const texture_ptr& get_texture() const { return texture; }

	//	@tool テクスチャは作り直し、バックバッファはスワップチェーンの大きさに合わせる
	bool resize(int width, int height);

	//	@tool 作り直した回数（描画側が描画先を作り直す判定に使う）
	std::uint32_t get_generation() const { return generation; }

private:
	explicit render_output(kinds kind);

	bool create_texture_core(int width, int height);

	//	@tool スワップチェーンの大きさと形式
	static DXGI_SWAP_CHAIN_DESC get_swap_chain_desc();

private:
	kinds kind;
	texture_ptr texture;
	int width = 0;
	int height = 0;
	std::uint32_t generation = 0;
};
