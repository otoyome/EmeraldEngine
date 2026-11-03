#include "System/universally_unique_identifier.h"

#include <cstring>
#include <windows.h>
#include <rpc.h>

#pragma comment(lib, "rpcrt4.lib")

universally_unique_identifier::universally_unique_identifier(const GUID& guid)
{
	static_assert(sizeof(GUID) == sizeof(std::uint64_t) * 2);
	std::memcpy(&high, reinterpret_cast<const unsigned char*>(&guid), sizeof(high));
	std::memcpy(&low, reinterpret_cast<const unsigned char*>(&guid) + sizeof(high), sizeof(low));
}

universally_unique_identifier universally_unique_identifier::generate()
{
	GUID guid{};
	const RPC_STATUS status = UuidCreate(&guid);
	_ASSERT_EXPR(status == RPC_S_OK, L"UuidCreate failed");
	if (status != RPC_S_OK)
		return universally_unique_identifier();
	return universally_unique_identifier(guid);
}

bool universally_unique_identifier::from_string(std::string_view text, universally_unique_identifier& out_value)
{
	std::string terminated(text);
	GUID guid{};
	if (UuidFromStringA(reinterpret_cast<RPC_CSTR>(terminated.data()), &guid) != RPC_S_OK)
		return false;

	out_value = universally_unique_identifier(guid);
	return true;
}

std::string universally_unique_identifier::to_string() const
{
	std::string result;
	GUID guid = to_guid();
	RPC_CSTR text = nullptr;
	if (UuidToStringA(&guid, &text) == RPC_S_OK)
	{
		result = reinterpret_cast<const char*>(text);
		RpcStringFreeA(&text);
	}
	return result;
}

GUID universally_unique_identifier::to_guid() const
{
	GUID guid{};
	std::memcpy(reinterpret_cast<unsigned char*>(&guid), &high, sizeof(high));
	std::memcpy(reinterpret_cast<unsigned char*>(&guid) + sizeof(high), &low, sizeof(low));
	return guid;
}
