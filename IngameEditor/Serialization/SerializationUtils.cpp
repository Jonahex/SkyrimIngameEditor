#include "Serialization/SerializationUtils.h"

#include <RE/T/TESDataHandler.h>

namespace SIE
{
    std::string ToFormKey(const RE::TESForm* form)
	{
		constexpr uint32_t indexMask = 0xFF000000;
		constexpr uint32_t formMask = 0xFFFFFF;

		if (form == nullptr || form->sourceFiles.array == nullptr || form->sourceFiles.array->empty())
		{
			return "null";
		}

		const auto srcFile = form->sourceFiles.array->front();
		return std::format("{:06X}:{}", form->GetFormID() & formMask, srcFile->GetFilename());
    }
}
