#include "Utils/Hooking.h"

#include <Windows.h>

namespace SIE
{
	void PatchMemory(uintptr_t Address, const uint8_t* Data, size_t Size)
	{
		DWORD d = 0;
		VirtualProtect(reinterpret_cast<LPVOID>(Address), Size, PAGE_EXECUTE_READWRITE, &d);

		for (uintptr_t i = Address; i < (Address + Size); i++)
		{
			*reinterpret_cast<volatile uint8_t*>(i) = *Data++;
		}

		VirtualProtect(reinterpret_cast<LPVOID>(Address), Size, d, &d);
		FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<LPVOID>(Address), Size);
	}

	void PatchMemory(uintptr_t Address, std::initializer_list<uint8_t> Data)
	{
		PatchMemory(Address, Data.begin(), Data.size());
	}

	void SetThreadName(uint32_t ThreadID, const char* ThreadName)
	{
#pragma pack(push, 8)
		const DWORD MS_VC_EXCEPTION = 0x406D1388;

		typedef struct tagTHREADNAME_INFO
		{
			DWORD dwType;      // Must be 0x1000.
			LPCSTR szName;     // Pointer to name (in user addr space).
			DWORD dwThreadID;  // Thread ID (-1=caller thread).
			DWORD dwFlags;     // Reserved for future use, must be zero.
		} THREADNAME_INFO;
#pragma pack(pop)

		THREADNAME_INFO info;
		info.dwType = 0x1000;
		info.szName = ThreadName;
		info.dwThreadID = ThreadID;
		info.dwFlags = 0;

		__try
		{
			RaiseException(MS_VC_EXCEPTION, 0, sizeof(info) / sizeof(ULONG_PTR), (ULONG_PTR*)&info);
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
		}
	}
}
