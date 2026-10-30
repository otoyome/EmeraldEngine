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

	//	レイヤースタック
	class layer_stack
	{
	public:
		//	積む・外す。反映は次の update_layers から
		void push(const layer& target);
		void pop(const layer& target);
		bool is_active(const layer& target) const;

		//	デバイス更新の後に毎フレーム呼ぶ
		void update();

		//	レイヤーに属するキーが止められているか
		bool is_key_blocked(const layer& target, key k, size_t pad_index) const;

		//	アクションのキーをレイヤーに登録・解除する
		void add_keys(const layer& target, const std::vector<key>& keys);
		void remove_keys(const layer& target, const std::vector<key>& keys);

	private:
		struct pad_key
		{
			key k;
			size_t pad_index;
		};

		struct layer_state
		{
			std::vector<key> keys;				//	所属アクションのキー
			bool active = false;				//	アクティブレイヤーか
			bool blocked_all = false;			//	上のレイヤーに全部止められているか
			std::vector<key> blocked_keys;		//	上のレイヤーに止められているキー
			bool was_active = false;			//	前アクティブレイヤーか？
			bool was_blocked_all = false;		//	前フレームの blocked_all
			std::vector<key> was_blocked_keys;	//	前フレームの blocked_keys
			std::vector<pad_key> ignored_keys;	//	離されるまで無視するキー
		};

		struct layer_request
		{
			const layer* target;
			bool push;
		};

		static bool is_ignored(const layer_state& s, key k, size_t pad_index);

	private:
		std::unordered_map<const layer*, layer_state> states;
		std::vector<const layer*> stack;
		std::vector<layer_request> requests;
	};
}
