#pragma once

#include "input_key.h"

namespace input
{
	//	下流レイヤーへの入力ブロックモード
	enum class layer_blocks : uint8_t
	{
		none,		//	止めない
		used_keys,	//	自分のアクションに登録したキーだけ止める
		all,		//	全部止める
	};

	//	入力レイヤー
	class layer
	{
	public:
		layer(std::string_view name, layer_blocks blocks);
		layer(const layer&) = delete;
		layer& operator=(const layer&) = delete;

		std::string_view get_name() const;
		layer_blocks get_blocks() const;

	private:
		std::string name;
		layer_blocks blocks;
	};

	//	積む・外す。反映は次の update_layers から
	void push_layer(const layer& target);
	void pop_layer(const layer& target);
	bool is_layer_active(const layer& target);

	//	デバイス更新の後に毎フレーム呼ぶ
	void update_layers();

	//	レイヤーに属するキーが止められているか
	bool is_key_blocked(const layer& target, key k, size_t pad_index);

	namespace detail
	{
		//	アクションのキーをレイヤーに登録・解除する
		void add_layer_keys(const layer& target, const std::vector<key>& keys);
		void remove_layer_keys(const layer& target, const std::vector<key>& keys);
	}
}
