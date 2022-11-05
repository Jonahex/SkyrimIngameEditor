#pragma once

namespace SIE
{
	void PatchMemory(uintptr_t Address, const uint8_t* Data, size_t Size);
	void PatchMemory(uintptr_t Address, std::initializer_list<uint8_t> Data);
	void SetThreadName(uint32_t ThreadID, const char* ThreadName);
}
