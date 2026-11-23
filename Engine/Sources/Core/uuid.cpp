#include "Core/uuid.h"

#include <cstring>
#include <windows.h>
#include <rpc.h>

#pragma comment(lib, "rpcrt4.lib")

core::uuid::uuid(const GUID& guid)
{
	static_assert(sizeof(GUID) == sizeof(std::uint64_t) * 2);
	std::memcpy(&high, reinterpret_cast<const unsigned char*>(&guid), sizeof(high));
	std::memcpy(&low, reinterpret_cast<const unsigned char*>(&guid) + sizeof(high), sizeof(low));
}

core::uuid core::uuid::generate()
{
	GUID guid{};
	const RPC_STATUS status = UuidCreate(&guid);
	_ASSERT_EXPR(status == RPC_S_OK, L"UuidCreate failed");
	if (status != RPC_S_OK)
		return uuid();
	return uuid(guid);
}

bool core::uuid::from_string(std::string_view text, uuid& out_value)
{
	std::string terminated(text);
	GUID guid{};
	if (UuidFromStringA(reinterpret_cast<RPC_CSTR>(terminated.data()), &guid) != RPC_S_OK)
		return false;

	out_value = uuid(guid);
	return true;
}

std::string core::uuid::to_string() const
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

GUID core::uuid::to_guid() const
{
	GUID guid{};
	std::memcpy(reinterpret_cast<unsigned char*>(&guid), &high, sizeof(high));
	std::memcpy(reinterpret_cast<unsigned char*>(&guid) + sizeof(high), &low, sizeof(low));
	return guid;
}
