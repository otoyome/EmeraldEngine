#pragma once

//	バッファビュー
struct buffer_view
{
	DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
	Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
	std::vector<BYTE> row_data;
	size_t stride_in_bytes = 0;
	size_t size_in_bytes = 0;
	size_t offset_in_bytes = 0;
	size_t count() const
	{
		return size_in_bytes / stride_in_bytes;
	}
	size_t offset() const
	{
		return offset_in_bytes / stride_in_bytes;
	}
	buffer_view() = default;
	~buffer_view() {}

	template<class Archive>
	void serialize(Archive& archive, int version)
	{
		archive(CEREAL_NVP(format),
				CEREAL_NVP(row_data),
				CEREAL_NVP(stride_in_bytes),
				CEREAL_NVP(size_in_bytes),
				CEREAL_NVP(offset_in_bytes));
	}
};

//	識別子
class semantic
{
public:
	semantic() {}
	semantic(std::string_view name, UINT index)
	: name(name), index(index)
	{
		build();
	}
	bool operator==(const semantic& l) const
	{
		assert(hash != -1);
		assert(l.hash != -1);
		return hash == l.hash;
	}

	template<class Archive>
	void serialize(Archive& archive, int version)
	{
		archive(CEREAL_NVP(name), CEREAL_NVP(index));
		dirty = true;
		build();	//	とりあえずここで
	}

	void build()
	{
		if (!dirty)
			return;

		dirty = false;
		hash = system_utility::generate_hash(name) + index;
	}

	system_utility::hash_key get_hash_key() const
	{
		assert(hash != -1);
		return hash;
	}
	std::string_view get_name() const {return name;}
	UINT get_index() const {return index;}

private:
	bool dirty = true;
	std::string name;
	UINT index;
	system_utility::hash_key hash = -1;
};


namespace std
{
    template <>
    struct hash<semantic>
	{
		std::size_t operator()(const semantic& item) const
		{
			return item.get_hash_key();
		}
	};
}	//	namespace std
