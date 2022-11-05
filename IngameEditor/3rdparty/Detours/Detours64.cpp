#include "3rdparty/detours/Detours.h"

#include <Windows.h>

#ifdef _M_AMD64
namespace Detours
{
	namespace X64
	{
		#define HEADER_MAGIC		'@D64'

		#define MAX_INSTRUCT_SIZE	0x20

		#define PUSH_RET_LENGTH_64	0x0E	// push <low 32 addr>; mov [rsp+4h], <hi 32 addr>; ret;
		#define RAX_JUMP_64			0x0C	// mov rax, <addr>; jmp rax;
		#define REL32_JUMP_64		0x05	// jmp <address within +/- 2GB>
		#define REL32_CALL_64		0x05	// call <address within +/- 2GB>

		#define ALIGN_64(x)			DetourAlignAddress((uint64_t)(x), 16);
		#define BREAK_ON_ERROR()	{ if (GetGlobalOptions() & OPT_BREAK_ON_FAIL) __debugbreak(); }

		bool DetourRemove(uintptr_t Trampoline)
		{
			if (!Trampoline)
			{
				BREAK_ON_ERROR();
				return false;
			}

			auto header = reinterpret_cast<JumpTrampolineHeader *>(Trampoline - sizeof(JumpTrampolineHeader));

			if (header->Magic != HEADER_MAGIC)
			{
				BREAK_ON_ERROR();
				return false;
			}

			// Rewrite the backed-up code
			if (!DetourCopyMemory(header->CodeOffset, header->InstructionOffset, header->InstructionLength))
			{
				BREAK_ON_ERROR();
				return false;
			}

			DetourFlushCache(header->CodeOffset, header->InstructionLength);
			VirtualFree(header, 0, MEM_RELEASE);

			return true;
		}

		uintptr_t DetourVTable(uintptr_t Target, uintptr_t Detour, uint32_t TableIndex)
		{
			// Each function is stored in an array
			auto virtualPointer = reinterpret_cast<void *>(Target + (TableIndex * sizeof(void *)));

			DWORD dwOld = 0;
			if (!VirtualProtect(virtualPointer, sizeof(void *), PAGE_EXECUTE_READWRITE, &dwOld))
				return 0;

			auto original = InterlockedExchangePointer(reinterpret_cast<void **>(virtualPointer), reinterpret_cast<void *>(Detour));

			VirtualProtect(virtualPointer, sizeof(void *), dwOld, &dwOld);
			return reinterpret_cast<uintptr_t>(original);
		}

		bool VTableRemove(uintptr_t Target, uintptr_t Function, uint32_t TableIndex)
		{
			// Reverse VTable detour
			return DetourVTable(Target, Function, TableIndex) != 0;
		}

		void DetourWriteStub(JumpTrampolineHeader *Header)
		{
			auto writeJmp = [](uintptr_t Base, uintptr_t Destination)
			{
				uint8_t buffer[14];

				// 'jmp qword ptr [rip + 0x6]' - unaligned
				buffer[0] = 0xFF;
				buffer[1] = 0x25;
				*reinterpret_cast<uint32_t *>(&buffer[2]) = 0;
				*reinterpret_cast<uint64_t *>(&buffer[6]) = Destination;

				return DetourCopyMemory(Base, reinterpret_cast<uintptr_t>(&buffer), sizeof(buffer));
			};

			// Jump to hooked function (Backed up instructions) [UserFunction -> OldInstructions -> THIS -> HookedFunction]
			auto unhookStart = Header->CodeOffset + Header->InstructionLength;
			auto instr_ptr = Header->InstructionOffset + Header->InstructionLength;

			writeJmp(instr_ptr, unhookStart);

			// Jump to user function (Write the trampoline) [HookedFunction -> THIS -> UserFunction]
			writeJmp(Header->TrampolineOffset, Header->DetourOffset);
		}

		bool DetourWritePushRet(JumpTrampolineHeader *Header)
		{
			uint8_t buffer[14];

			// Jump with PUSH/RET (push low32, [rsp+4h] = hi32)
			//
			// push dword low32
			// mov dword [rsp + 0x4], hi32
			// ret
			//
			buffer[0] = 0x68;
			*reinterpret_cast<uint32_t *>(&buffer[1]) = static_cast<uint32_t>(Header->TrampolineOffset & 0xFFFFFFFF);
			buffer[5] = 0xC7;
			buffer[6] = 0x44;
			buffer[7] = 0x24;
			buffer[8] = 0x04;
			*reinterpret_cast<uint32_t *>(&buffer[9]) = static_cast<uint32_t>(Header->TrampolineOffset >> 32);
			buffer[13] = 0x04;

			return DetourCopyMemory(Header->CodeOffset, reinterpret_cast<uintptr_t>(&buffer), sizeof(buffer));
		}

		bool DetourWriteRaxJump(JumpTrampolineHeader *Header)
		{
			// Jump to trampoline (from hooked function)
			//
			// mov rax, offset64
			// jmp rax
			//
			uint8_t buffer[12];
			buffer[0] = 0x48;
			buffer[1] = 0xB8;
			*reinterpret_cast<uint64_t *>(&buffer[2]) = Header->TrampolineOffset;

			buffer[10] = 0xFF;
			buffer[11] = 0xE0;

			return DetourCopyMemory(Header->CodeOffset, reinterpret_cast<uintptr_t>(&buffer), sizeof(buffer));
		}

		bool DetourWriteRel32Jump(JumpTrampolineHeader *Header)
		{
			// Assumes that the delta is less than 2GB. We need to manually write the offset.
			//
			// jmp offset64
			//
			const auto displacement = static_cast<int64_t>(Header->TrampolineOffset - (Header->CodeOffset + 5));

			if (abs(displacement) >= INT_MAX)
				return false;

			uint8_t buffer[5];
			buffer[0] = 0xE9;
			*reinterpret_cast<int32_t *>(&buffer[1]) = static_cast<int32_t>(displacement);

			return DetourCopyMemory(Header->CodeOffset, reinterpret_cast<uintptr_t>(&buffer), sizeof(buffer));
		}

		bool DetourWriteRel32Call(JumpTrampolineHeader *Header)
		{
			// Same as DetourWriteRel32Jump but with a different opcode
			//
			// call offset64
			//
			const auto displacement = static_cast<int64_t>(Header->TrampolineOffset - (Header->CodeOffset + 5));

			if (abs(displacement) >= INT_MAX)
				return false;

			uint8_t buffer[5];
			buffer[0] = 0xE8;
			*reinterpret_cast<int32_t *>(&buffer[1]) = static_cast<int32_t>(displacement);

			return DetourCopyMemory(Header->CodeOffset, reinterpret_cast<uintptr_t>(&buffer), sizeof(buffer));
		}

		uint32_t DetourGetHookLength(X64Option Options)
		{
			uint32_t size = 0;

			switch(Options)
			{
			case X64Option::USE_PUSH_RET:	size += PUSH_RET_LENGTH_64;	break;
			case X64Option::USE_RAX_JUMP:	size += RAX_JUMP_64;		break;
			case X64Option::USE_REL32_JUMP:	size += REL32_JUMP_64;		break;
			case X64Option::USE_REL32_CALL:	size += REL32_CALL_64;		break;
			}

			return size;
		}
	}
}
#endif // _M_AMD64
