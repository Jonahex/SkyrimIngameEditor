#pragma once

#include <Windows.h>

#include <unordered_set>

namespace RE
{
	class TESForm;
}

namespace SIE
{
	class Serializer
	{
	public:
		static Serializer& Instance();

		~Serializer();

		void EnqueueForm(const RE::TESForm& form);
		void Export(const std::string& path) const;

	private:
		Serializer();

		inline static HMODULE Dll = nullptr;
		static inline int (*ExportImpl)(const char*, const char*) = nullptr;

		std::unordered_set<const RE::TESForm*> forms;
    };
}
