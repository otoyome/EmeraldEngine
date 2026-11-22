#pragma once

#include <cstring>
#include <span>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "System/system_utility.h"
#include "System/variable_position_allocator.h"

//	ブロック共有型可変長アロケーター
template<class Element>
class shared_block_allocator
{
	static_assert(std::is_trivially_copyable_v<Element>, "Element must be trivially copyable");
	using Elements = std::vector<Element>;

public:
	//	取得ブロック
	struct handle
	{
		system_utility::hash_key key = 0;	//	キー情報
		UINT head = 0;						//	先頭位置
		UINT size = 0;						//	要素数

		bool is_valid() const { return key != 0; }
	};

	//	確保情報
	struct entry
	{
		UINT head = 0;		//	先頭位置
		UINT size = 0;		//	要素数
		UINT ref_count = 0;	//	参照数
		Elements elements;	//	要素
	};

public:
	//	転送待ちがあるか
	bool has_pending() const { return !pending.empty(); }

	//	全体の容量
	UINT get_capacity() const { return allocator.get_capacity(); }

	//	使用中の要素数
	UINT get_used_size() const { return allocator.get_used_size(); }

	//	保持しているブロック数
	size_t get_block_count() const { return entries.size(); }

public:
	//	初期化
	bool initialize(UINT capacity)
	{
		entries.clear();
		pending.clear();
		return allocator.initialize(capacity);
	}

	//	ブロック取得
	handle acquire(std::span<const Element> elements)
	{
		if (elements.empty())
			return {};

		system_utility::hash_key key = calculate_key(elements);
		for (;;)
		{
			auto it = entries.find(key);
			if (it == entries.end())
				break;

			//	同じ内容であれば参照カウンターを増やして処理
			if (is_same_elements(it->second.elements, elements))
			{
				++it->second.ref_count;
				return { key, it->second.head, it->second.size };
			}

			//	ハッシュが衝突したので次のキーを探す(0 は無効値なので飛ばす)
			key = (key + 1 == 0) ? 1 : key + 1;
		}

		//	新規確保
		const UINT size = static_cast<UINT>(elements.size());
		const UINT head = allocator.allocate(size);
		if (head == variable_position_allocator::allocate_failed_code)
			return {};

		entry& new_entry = entries[key];
		new_entry.head = head;
		new_entry.size = size;
		new_entry.ref_count = 1;
		new_entry.elements.assign(elements.begin(), elements.end());
		pending.push_back(key);
		return { key, head, size };
	}

	//	ブロック解放
	void release(const handle& target)
	{
		if (!target.is_valid())
			return;

		auto it = entries.find(target.key);
		if (it == entries.end())
			return;

		if (it->second.ref_count > 1)
		{
			--it->second.ref_count;
			return;
		}

		allocator.deallocate(it->second.head);
		entries.erase(it);
	}

	//	容量の拡張
	bool grow(UINT additional_size)
	{
		return allocator.grow(additional_size);
	}

public:
	//	
	template<class Function>
	void consume_pending(Function&& function)
	{
		for (system_utility::hash_key key : pending)
		{
			auto it = entries.find(key);
			if (it == entries.end())
				continue;

			const entry& target = it->second;
			function(target.head, std::span<const Element>(target.elements));
		}
		pending.clear();
	}

	template<class Function>
	void for_each(Function&& function) const
	{
		for (const auto& [key, target] : entries)
			function(target.head, std::span<const Element>(target.elements));
	}

private:
	//	内容からキーを作る
	static system_utility::hash_key calculate_key(std::span<const Element> elements)
	{
		const system_utility::hash_key key = system_utility::generate_hash(elements.data(), elements.size_bytes());
		return key == 0 ? 1 : key;
	}

	//	内容の比較
	static bool is_same_elements(const Elements& lhs, std::span<const Element> rhs)
	{
		return lhs.size() == rhs.size() && std::memcmp(lhs.data(), rhs.data(), rhs.size_bytes()) == 0;
	}

private:
	variable_position_allocator allocator;
	std::unordered_map<system_utility::hash_key, entry> entries;
	std::vector<system_utility::hash_key> pending;
};
