#pragma once

#include <rttidata.h>

#include <string>
#include <vector>
#include <unordered_map>

namespace SIE
{
	class RTTI
	{
	public:
		using ObjectEditor = bool (*)(void*, void*);
		using Constructor = void* (*)();

		struct Base
		{
			const TypeDescriptor* typeDescriptor = nullptr;
			int offset = 0;
		};

		bool IsDescendantOf(const TypeDescriptor& other) const;

		std::string typeName;
		std::vector<Base> bases;
		ObjectEditor objectEditor = nullptr;
		Constructor constructor = nullptr;

		const TypeDescriptor* typeDescriptor = nullptr;

	private:
		friend class RTTICache;

		bool isFullyInitialized = false;
	};

	class RTTICache
	{
	public:
		static RTTICache& Instance();

		void RegisterEditor(const TypeDescriptor& typeDescriptor, RTTI::ObjectEditor objectEditor);
		void RegisterConstructor(const TypeDescriptor& typeDescriptor,
			RTTI::Constructor constructor);

		const RTTI& GetRTTI(const void* object);
		const std::string& GetTypeName(const void* object);
		bool BuildEditor(void* object, void* context = nullptr);
		std::vector<const RTTI*> GetConstructibleDescendants(const TypeDescriptor& typeDescriptor);

	private:
		void CacheBases(int hierarchyDescriptorOffset, uintptr_t imageBase);
		RTTI& GetFromCache(const _RTTICompleteObjectLocator& col);
		RTTI& GetFromCache(const TypeDescriptor& typeDescriptor);

		std::unordered_map<const TypeDescriptor*, RTTI> cache;
	};

	void RegisterNiConstructors();
}
