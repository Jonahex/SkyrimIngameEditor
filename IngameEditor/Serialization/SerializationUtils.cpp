#include "Serialization/SerializationUtils.h"

#include <RE/T/TESDataHandler.h>

namespace SIE
{
    std::string ToFormKey(const RE::TESForm* form)
	{
		constexpr uint32_t indexMask = 0xFF000000;
		constexpr uint32_t formMask = 0xFFFFFF;

		if (form == nullptr)
		{
			return "null";
		}

		const auto srcFile = form->GetFile(0);
		if (srcFile == nullptr)
		{
			return "null";
		}

		return std::format("{:06X}:{}", form->GetFormID() & formMask, srcFile->GetFilename());
    }
}
