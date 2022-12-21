#include "Utils/RTTICache.h"

#include <windows.h>
#include <DbgHelp.h>

#include <ehdata.h>

#include <functional>

extern "C" char* __unDName(char*, const char*, int, void*, void*, int);

namespace SIE
{
	namespace SRTTICache
	{
		struct VirtualBaseTable
		{
			int topOffset;
			int virtualBaseOffsets[];
		};

		const _RTTICompleteObjectLocator* GetCompleteObjectLocator(const void* object)
		{
			/*const auto vbTable = reinterpret_cast<VirtualBaseTable* const*>(object)[0];
			const void* vfTable =
				vbTable->topOffset == 0 ?
					reinterpret_cast<const void*>(
						reinterpret_cast<uint64_t>(object) + vbTable->virtualBaseOffsets[0]) :
					reinterpret_cast<void* const*>(object)[0];*/
			const auto col =
				(reinterpret_cast<_RTTICompleteObjectLocator* const* const*>(object))[0][-1];
			return col;
		}

		const TypeDescriptor* GetTypeDescriptor(const void* object)
		{
			const auto col = GetCompleteObjectLocator(object);
			return GetTypeDescriptor(col);
		}

		const TypeDescriptor* GetTypeDescriptor(const _RTTICompleteObjectLocator& col)
		{
			const uintptr_t imageBase = reinterpret_cast<uintptr_t>(&col) - col.pSelf;
			const auto typeDescriptor =
				reinterpret_cast<TypeDescriptor*>(imageBase + col.pTypeDescriptor);
			return typeDescriptor;
		}

		std::string GetTypeName(const TypeDescriptor& typeDescriptor)
		{
			char outputBuffer[512];
			__unDName(outputBuffer, typeDescriptor.name + 1, 512, malloc, free, 0x3800);
			return outputBuffer;
		}

		std::string GetTypeName(const void* object)
		{
			const auto typeDescriptor = GetTypeDescriptor(object);
			return GetTypeName(*typeDescriptor);
		}

		std::vector<RTTI::Base> GetBaseInfo(int hierarchyDescriptorOffset, uintptr_t imageBase)
		{
			std::vector<RTTI::Base> result;

			const auto hierarchyDescriptor = reinterpret_cast<_RTTIClassHierarchyDescriptor*>(
				imageBase + hierarchyDescriptorOffset);
			const auto baseClassArray = reinterpret_cast<_RTTIBaseClassArray*>(
				imageBase + hierarchyDescriptor->pBaseClassArray);
			for (size_t baseIndex = 1; baseIndex < hierarchyDescriptor->numBaseClasses; ++baseIndex)
			{
				const auto baseDescriptor = reinterpret_cast<_RTTIBaseClassDescriptor*>(
					imageBase + baseClassArray->arrayOfBaseClassDescriptors[baseIndex]);
				const auto baseTypeDescriptor =
					reinterpret_cast<TypeDescriptor*>(imageBase + baseDescriptor->pTypeDescriptor);
				result.push_back(
					{ baseTypeDescriptor, baseDescriptor->where.mdisp });
			}
			return result;
		}
	}

	RTTICache& RTTICache::Instance()
	{ 
		static RTTICache instance;
		return instance;
	}

	void RTTICache::RegisterEditor(const TypeDescriptor& typeDescriptor,
		RTTI::ObjectEditor objectEditor)
	{
		auto& rtti = GetFromCache(typeDescriptor);
		rtti.objectEditor = objectEditor;
	}

	const RTTI& RTTICache::GetRTTI(const void* object) 
	{
		const auto col = SRTTICache::GetCompleteObjectLocator(object);
		return GetFromCache(*col);
	}

	const std::string& RTTICache::GetTypeName(const void* object)
	{
		const auto col = SRTTICache::GetCompleteObjectLocator(object);
		const auto& rtti = GetFromCache(*col);
		return rtti.typeName;
	}

	bool RTTICache::BuildEditor(void* object) 
	{
		const auto col = SRTTICache::GetCompleteObjectLocator(object);
		const auto& rtti = GetFromCache(*col);

		logger::info("building: {}", rtti.typeName);

		std::function<bool(const RTTI::Base&, void*)> buildBaseEditor;
		buildBaseEditor = [&buildBaseEditor, this](const RTTI::Base& base, void* object)
		{
			bool result = false;
			auto subObject =
				reinterpret_cast<void*>(reinterpret_cast<uint64_t>(object) + base.offset);
			RTTI& baseRtti = GetFromCache(*base.typeDescriptor);
			logger::info("building base: {}", baseRtti.typeName);
			for (const auto& subBase : baseRtti.bases)
			{
				if (buildBaseEditor(subBase, subObject))
				{
					result = true;
				}
			}
			if (baseRtti.objectEditor != nullptr && baseRtti.objectEditor(subObject))
			{
				result = true;
			}
			return result;
		};

		bool finalResult = false;
		for (const auto& base : rtti.bases)
		{
			if (buildBaseEditor(base, object))
			{
				finalResult = true;
			}
		}
		if (rtti.objectEditor != nullptr && rtti.objectEditor(object))
		{
			finalResult = true;
		}

		return finalResult;
	}

	void RTTICache::CacheBases(int hierarchyDescriptorOffset,
		uintptr_t imageBase) 
	{
		const auto hierarchyDescriptor =
			reinterpret_cast<_RTTIClassHierarchyDescriptor*>(imageBase + hierarchyDescriptorOffset);
		const auto baseClassArray = reinterpret_cast<_RTTIBaseClassArray*>(
			imageBase + hierarchyDescriptor->pBaseClassArray);
		for (size_t baseIndex = 0; baseIndex < hierarchyDescriptor->numBaseClasses; ++baseIndex)
		{
			const auto baseDescriptor = reinterpret_cast<_RTTIBaseClassDescriptor*>(
				imageBase + baseClassArray->arrayOfBaseClassDescriptors[baseIndex]);
			const auto baseTypeDescriptor =
				reinterpret_cast<TypeDescriptor*>(imageBase + baseDescriptor->pTypeDescriptor);
			RTTI& rtti = GetFromCache(*baseTypeDescriptor);
			if (!rtti.isFullyInitialized)
			{
				rtti.bases = SRTTICache::GetBaseInfo(baseDescriptor->pClassDescriptor, imageBase);
				rtti.isFullyInitialized = true;
			}
			if (baseIndex > 0)
			{
				CacheBases(baseDescriptor->pClassDescriptor, imageBase);
			}
		}
	}

	RTTI& RTTICache::GetFromCache(const _RTTICompleteObjectLocator& col)
	{
		const auto imageBase = reinterpret_cast<uintptr_t>(&col) - col.pSelf;
		CacheBases(col.pClassDescriptor, imageBase);

		const auto typeDescriptor = SRTTICache::GetTypeDescriptor(col);
		return cache[typeDescriptor];
	}

	RTTI& RTTICache::GetFromCache(const TypeDescriptor& typeDescriptor)
	{
		auto cacheIt = cache.find(&typeDescriptor);
		if (cacheIt != cache.cend())
		{
			return cacheIt->second;
		}

		RTTI newRecord;
		newRecord.typeDescriptor = &typeDescriptor;
		newRecord.typeName = SRTTICache::GetTypeName(typeDescriptor);
		return cache.insert_or_assign(&typeDescriptor, std::move(newRecord)).first->second;
	}
}
