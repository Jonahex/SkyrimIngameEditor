#include "Utils/RTTICache.h"

#include <RE/N/NiTStringMap.h>

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

		void RegisterNiConstructor(const REL::ID& relId) 
		{
			const auto& typeDescriptor = *REL::Relocation<TypeDescriptor*>(relId);
			const std::string typeName = GetTypeName(typeDescriptor);

			static REL::Relocation<RE::NiTStringPointerMap<RE::NiObject* (*)()>**>
				ConstructorRegistry(RE::Offset::NiObjectConstructorRegistry);

			RTTI::Constructor constructor = nullptr;
			auto it = (*ConstructorRegistry)->find(typeName.c_str());
			if (it != (*ConstructorRegistry)->end())
			{
				auto& cache = RTTICache::Instance();
				auto object = RE::NiPointer(it->second());
				cache.GetRTTI(object.get());
				cache.RegisterConstructor(typeDescriptor,
					reinterpret_cast<RTTI::Constructor>(it->second));
			}
		}
	}

	bool RTTI::IsDescendantOf(const TypeDescriptor& other) const 
	{ 
		for (const auto& base : bases)
		{
			if (base.typeDescriptor == &other)
			{
				return true;
			}
		}
		return false;
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

	void RTTICache::RegisterConstructor(const TypeDescriptor& typeDescriptor,
		RTTI::Constructor constructor)
	{
		auto& rtti = GetFromCache(typeDescriptor);
		rtti.constructor = constructor;
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

	bool RTTICache::BuildEditor(void* object, void* context) 
	{
		const auto col = SRTTICache::GetCompleteObjectLocator(object);
		const auto& rtti = GetFromCache(*col);

		bool finalResult = false;
		for (const auto& base : rtti.bases)
		{
			auto subObject =
				reinterpret_cast<void*>(reinterpret_cast<uint64_t>(object) + base.offset);
			const auto& baseRtti = GetFromCache(*base.typeDescriptor);
			if (baseRtti.objectEditor != nullptr && baseRtti.objectEditor(subObject, context))
			{
				finalResult = true;
			}
		}
		if (rtti.objectEditor != nullptr && rtti.objectEditor(object, context))
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

	std::vector<const RTTI*> RTTICache::GetConstructibleDescendants(
		const TypeDescriptor& typeDescriptor)
	{
		std::vector<const RTTI*> result;
		for (const auto& [descriptor, rtti] : cache)
		{
			if (rtti.constructor != nullptr && rtti.IsDescendantOf(typeDescriptor))
			{
				result.push_back(&rtti);
			}
		}
		return result;
	}

	void RegisterNiConstructors() 
	{
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiAdditionalGeometryData);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiAlphaAccumulator);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiAlphaProperty);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiAmbientLight);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiBillboardNode);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiBinaryExtraData);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiBooleanExtraData);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiBSPNode);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiCamera);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiColorExtraData);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiDefaultAVObjectPalette);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiDirectionalLight);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiExtraData);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiFloatExtraData);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiFloatsExtraData);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiFogProperty);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiIntegerExtraData);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiIntegersExtraData);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiNode);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiParticleMeshes);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiParticleMeshesData);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiParticles);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiParticlesData);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiPointLight);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiShadeProperty);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiSkinData);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiSkinInstance);
		SRTTICache::RegisterNiConstructor(RE::RTTI_BSDismemberSkinInstance);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiSkinPartition);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiSpotLight);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiStringExtraData);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiStringsExtraData);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiSwitchNode);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiSwitchStringExtraData);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiTriShape);
		SRTTICache::RegisterNiConstructor(RE::RTTI_BSSegmentedTriShape);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiTriShapeData);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiTriStrips);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiTriStripsData);
		SRTTICache::RegisterNiConstructor(RE::RTTI_BSLODTriShape);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiVectorExtraData);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiVertWeightsExtraData);
		SRTTICache::RegisterNiConstructor(RE::RTTI_BSTriShape);
		SRTTICache::RegisterNiConstructor(RE::RTTI_BSDynamicTriShape);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiCollisionData);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiBlendAccumTransformInterpolator);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiBlendBoolInterpolator);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiBlendColorInterpolator);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiBlendFloatInterpolator);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiBlendPoint3Interpolator);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiBlendQuaternionInterpolator);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiBlendTransformInterpolator);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiBoolData);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiBoolInterpolator);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiBoolTimelineInterpolator);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiBSplineBasisData);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiBSplineData);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiBSplineColorInterpolator);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiBSplineCompColorInterpolator);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiBSplineCompFloatInterpolator);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiBSplineCompPoint3Interpolator);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiBSplineCompTransformInterpolator);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiBSplineFloatInterpolator);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiBSplinePoint3Interpolator);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiBSplineTransformInterpolator);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiColorData);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiColorExtraDataController);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiColorInterpolator);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiControllerManager);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiControllerSequence);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiFloatData);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiFloatExtraDataController);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiFloatInterpolator);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiFloatsExtraDataController);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiFloatsExtraDataPoint3Controller);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiKeyframeManager);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiLightColorController);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiLightDimmerController);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiLookAtController);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiLookAtInterpolator);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiMorphData);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiMultiTargetTransformController);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiPathController);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiPathInterpolator);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiPoint3Interpolator);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiPosData);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiQuaternionInterpolator);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiRollController);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiRotData);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiSequence);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiSequenceStreamHelper);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiStringPalette);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiTextKeyExtraData);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiTransformController);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiTransformData);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiTransformInterpolator);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiUVData);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiVisController);
		SRTTICache::RegisterNiConstructor(RE::RTTI_BSAnimNotes);
		SRTTICache::RegisterNiConstructor(RE::RTTI_BSGrabIKNote);
		SRTTICache::RegisterNiConstructor(RE::RTTI_BSLookIKNote);
		SRTTICache::RegisterNiConstructor(RE::RTTI_BSRotAccumTransfInterpolator);
		SRTTICache::RegisterNiConstructor(RE::RTTI_BSTreadTransfInterpolator);
		SRTTICache::RegisterNiConstructor(RE::RTTI_BSBlendTreadTransfInterpolator);
		SRTTICache::RegisterNiConstructor(RE::RTTI_BSMultiTargetTreadTransfController);
		SRTTICache::RegisterNiConstructor(RE::RTTI_BSFrustumFOVController);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiMeshParticleSystem);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiMeshPSysData);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiParticleSystem);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiPSysAirFieldAirFrictionCtlr);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiPSysAirFieldInheritVelocityCtlr);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiPSysAirFieldModifier);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiPSysAirFieldSpreadCtlr);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiPSysAgeDeathModifier);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiPSysBombModifier);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiPSysBoundUpdateModifier);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiPSysBoxEmitter);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiPSysColliderManager);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiPSysColorModifier);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiPSysCylinderEmitter);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiPSysData);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiPSysDragFieldModifier);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiPSysDragModifier);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiPSysEmitterCtlr);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiPSysEmitterCtlrData);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiPSysEmitterDeclinationCtlr);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiPSysEmitterDeclinationVarCtlr);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiPSysEmitterInitialRadiusCtlr);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiPSysEmitterLifeSpanCtlr);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiPSysEmitterPlanarAngleCtlr);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiPSysEmitterPlanarAngleVarCtlr);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiPSysEmitterSpeedCtlr);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiPSysFieldAttenuationCtlr);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiPSysFieldMagnitudeCtlr);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiPSysFieldMaxDistanceCtlr);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiPSysGravityModifier);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiPSysGravityFieldModifier);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiPSysGravityStrengthCtlr);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiPSysGrowFadeModifier);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiPSysInitialRotAngleCtlr);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiPSysInitialRotAngleVarCtlr);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiPSysInitialRotSpeedCtlr);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiPSysInitialRotSpeedVarCtlr);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiPSysMeshEmitter);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiPSysMeshUpdateModifier);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiPSysModifierActiveCtlr);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiPSysPlanarCollider);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiPSysPositionModifier);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiPSysRadialFieldModifier);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiPSysResetOnLoopCtlr);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiPSysRotationModifier);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiPSysSpawnModifier);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiPSysSphereEmitter);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiPSysSphericalCollider);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiPSysTurbulenceFieldModifier);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiPSysUpdateCtlr);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiPSysVortexFieldModifier);
		SRTTICache::RegisterNiConstructor(RE::RTTI_BSStripParticleSystem);
		SRTTICache::RegisterNiConstructor(RE::RTTI_BSStripPSysData);
		SRTTICache::RegisterNiConstructor(RE::RTTI_BSPSysHavokUpdateModifier);
		SRTTICache::RegisterNiConstructor(RE::RTTI_BSPSysRecycleBoundModifier);
		SRTTICache::RegisterNiConstructor(RE::RTTI_BSPSysInheritVelocityModifier);
		SRTTICache::RegisterNiConstructor(RE::RTTI_BSReference);
		SRTTICache::RegisterNiConstructor(RE::RTTI_BSNodeReferences);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiBSBoneLODController);
		SRTTICache::RegisterNiConstructor(RE::RTTI_BSBound);
		SRTTICache::RegisterNiConstructor(RE::RTTI_BSXFlags);
		SRTTICache::RegisterNiConstructor(RE::RTTI_BSDecalPlacementVectorExtraData);
		SRTTICache::RegisterNiConstructor(RE::RTTI_BSWindModifier);
		SRTTICache::RegisterNiConstructor(RE::RTTI_BSParentVelocityModifier);
		SRTTICache::RegisterNiConstructor(RE::RTTI_BSFurnitureMarkerNode);
		SRTTICache::RegisterNiConstructor(RE::RTTI_BSAnimInteractionMarker);
		SRTTICache::RegisterNiConstructor(RE::RTTI_BSInvMarker);
		SRTTICache::RegisterNiConstructor(RE::RTTI_BSPSysArrayEmitter);
		SRTTICache::RegisterNiConstructor(RE::RTTI_BSWArray);
		SRTTICache::RegisterNiConstructor(RE::RTTI_BSDamageStage);
		SRTTICache::RegisterNiConstructor(RE::RTTI_BSBlastNode);
		SRTTICache::RegisterNiConstructor(RE::RTTI_BSValueNode);
		SRTTICache::RegisterNiConstructor(RE::RTTI_BSPSysMultiTargetEmitterCtlr);
		SRTTICache::RegisterNiConstructor(RE::RTTI_BSMasterParticleSystem);
		SRTTICache::RegisterNiConstructor(RE::RTTI_BSMultiBound);
		SRTTICache::RegisterNiConstructor(RE::RTTI_BSMultiBoundNode);
		SRTTICache::RegisterNiConstructor(RE::RTTI_BSMultiBoundAABB);
		SRTTICache::RegisterNiConstructor(RE::RTTI_BSMultiBoundOBB);
		SRTTICache::RegisterNiConstructor(RE::RTTI_BSMultiBoundCapsule);
		SRTTICache::RegisterNiConstructor(RE::RTTI_BSMultiBoundSphere);
		SRTTICache::RegisterNiConstructor(RE::RTTI_BSDebrisNode);
		SRTTICache::RegisterNiConstructor(RE::RTTI_BSPSysStripUpdateModifier);
		SRTTICache::RegisterNiConstructor(RE::RTTI_BSPSysSubTexModifier);
		SRTTICache::RegisterNiConstructor(RE::RTTI_BSPSysScaleModifier);
		SRTTICache::RegisterNiConstructor(RE::RTTI_BSBoneLODExtraData);
		SRTTICache::RegisterNiConstructor(RE::RTTI_BSBehaviorGraphExtraData);
		SRTTICache::RegisterNiConstructor(RE::RTTI_BSProceduralLightningController);
		SRTTICache::RegisterNiConstructor(RE::RTTI_BSLagBoneController);
		SRTTICache::RegisterNiConstructor(RE::RTTI_BSNonUniformScaleExtraData);
		SRTTICache::RegisterNiConstructor(RE::RTTI_BSSubIndexTriShape);
		SRTTICache::RegisterNiConstructor(RE::RTTI_BSDistantObjectLargeRefExtraData);
		SRTTICache::RegisterNiConstructor(RE::RTTI_NiBSBoneLODController);
		SRTTICache::RegisterNiConstructor(RE::RTTI_bhkBoxShape);
		SRTTICache::RegisterNiConstructor(RE::RTTI_bhkCompressedMeshShapeData);
		SRTTICache::RegisterNiConstructor(RE::RTTI_bhkCompressedMeshShape);
		SRTTICache::RegisterNiConstructor(RE::RTTI_bhkConvexSweepShape);
		SRTTICache::RegisterNiConstructor(RE::RTTI_bhkConvexTransformShape);
		SRTTICache::RegisterNiConstructor(RE::RTTI_bhkConvexTranslateShape);
		SRTTICache::RegisterNiConstructor(RE::RTTI_bhkConvexVerticesShape);
		SRTTICache::RegisterNiConstructor(RE::RTTI_bhkCylinderShape);
		SRTTICache::RegisterNiConstructor(RE::RTTI_bhkMultiSphereShape);
		SRTTICache::RegisterNiConstructor(RE::RTTI_bhkNiTriStripsShape);
		SRTTICache::RegisterNiConstructor(RE::RTTI_bhkPackedNiTriStripsShape);
		SRTTICache::RegisterNiConstructor(RE::RTTI_hkPackedNiTriStripsData);
		SRTTICache::RegisterNiConstructor(RE::RTTI_bhkPlaneShape);
		SRTTICache::RegisterNiConstructor(RE::RTTI_bhkSphereShape);
		SRTTICache::RegisterNiConstructor(RE::RTTI_bhkTriangleShape);
		SRTTICache::RegisterNiConstructor(RE::RTTI_bhkMoppBvTreeShape);
		SRTTICache::RegisterNiConstructor(RE::RTTI_bhkTransformShape);
		SRTTICache::RegisterNiConstructor(RE::RTTI_bhkCapsuleShape);
		SRTTICache::RegisterNiConstructor(RE::RTTI_bhkListShape);
		SRTTICache::RegisterNiConstructor(RE::RTTI_bhkBallAndSocketConstraint);
		SRTTICache::RegisterNiConstructor(RE::RTTI_bhkBallSocketConstraintChain);
		SRTTICache::RegisterNiConstructor(RE::RTTI_bhkGroupConstraint);
		SRTTICache::RegisterNiConstructor(RE::RTTI_bhkHingeConstraint);
		SRTTICache::RegisterNiConstructor(RE::RTTI_bhkHingeLimitsConstraint);
		SRTTICache::RegisterNiConstructor(RE::RTTI_bhkFixedConstraint);
		SRTTICache::RegisterNiConstructor(RE::RTTI_bhkLimitedHingeConstraint);
		SRTTICache::RegisterNiConstructor(RE::RTTI_bhkPrismaticConstraint);
		SRTTICache::RegisterNiConstructor(RE::RTTI_bhkRagdollConstraint);
		SRTTICache::RegisterNiConstructor(RE::RTTI_bhkRagdollLimitsConstraint);
		SRTTICache::RegisterNiConstructor(RE::RTTI_bhkStiffSpringConstraint);
		SRTTICache::RegisterNiConstructor(RE::RTTI_bhkWheelConstraint);
		SRTTICache::RegisterNiConstructor(RE::RTTI_bhkBreakableConstraint);
		SRTTICache::RegisterNiConstructor(RE::RTTI_bhkMalleableConstraint);
		SRTTICache::RegisterNiConstructor(RE::RTTI_bhkAngularDashpotAction);
		SRTTICache::RegisterNiConstructor(RE::RTTI_bhkDashpotAction);
		SRTTICache::RegisterNiConstructor(RE::RTTI_bhkLiquidAction);
		SRTTICache::RegisterNiConstructor(RE::RTTI_bhkMouseSpringAction);
		SRTTICache::RegisterNiConstructor(RE::RTTI_bhkMotorAction);
		SRTTICache::RegisterNiConstructor(RE::RTTI_bhkOrientHingedBodyAction);
		SRTTICache::RegisterNiConstructor(RE::RTTI_bhkSpringAction);
		SRTTICache::RegisterNiConstructor(RE::RTTI_bhkAabbPhantom);
		SRTTICache::RegisterNiConstructor(RE::RTTI_bhkCollisionObject);
		SRTTICache::RegisterNiConstructor(RE::RTTI_bhkPCollisionObject);
		SRTTICache::RegisterNiConstructor(RE::RTTI_bhkSPCollisionObject);
		SRTTICache::RegisterNiConstructor(RE::RTTI_bhkBlendCollisionObject);
		SRTTICache::RegisterNiConstructor(RE::RTTI_bhkBlendController);
		SRTTICache::RegisterNiConstructor(RE::RTTI_bhkRigidBody);
		SRTTICache::RegisterNiConstructor(RE::RTTI_bhkRigidBodyT);
		SRTTICache::RegisterNiConstructor(RE::RTTI_bhkSimpleShapePhantom);
		SRTTICache::RegisterNiConstructor(RE::RTTI_bhkCachingShapePhantom);
		SRTTICache::RegisterNiConstructor(RE::RTTI_bhkExtraData);
		SRTTICache::RegisterNiConstructor(RE::RTTI_bhkRagdollTemplate);
		SRTTICache::RegisterNiConstructor(RE::RTTI_bhkRagdollTemplateData);
		SRTTICache::RegisterNiConstructor(RE::RTTI_bhkPoseArray);
		SRTTICache::RegisterNiConstructor(RE::RTTI_BSLightingShaderProperty);
		SRTTICache::RegisterNiConstructor(RE::RTTI_BSWaterShaderProperty);
		SRTTICache::RegisterNiConstructor(RE::RTTI_BSEffectShaderProperty);
		SRTTICache::RegisterNiConstructor(RE::RTTI_BSSkyShaderProperty);
		SRTTICache::RegisterNiConstructor(RE::RTTI_BSGrassShaderProperty);
		SRTTICache::RegisterNiConstructor(RE::RTTI_BSShaderTextureSet);
		SRTTICache::RegisterNiConstructor(RE::RTTI_BSFadeNode);
		SRTTICache::RegisterNiConstructor(RE::RTTI_BSTreeNode);
		SRTTICache::RegisterNiConstructor(RE::RTTI_BSLightingShaderPropertyFloatController);
		SRTTICache::RegisterNiConstructor(RE::RTTI_BSLightingShaderPropertyUShortController);
		SRTTICache::RegisterNiConstructor(RE::RTTI_BSLightingShaderPropertyColorController);
		SRTTICache::RegisterNiConstructor(RE::RTTI_BSEffectShaderPropertyFloatController);
		SRTTICache::RegisterNiConstructor(RE::RTTI_BSEffectShaderPropertyColorController);
		SRTTICache::RegisterNiConstructor(RE::RTTI_BSNiAlphaPropertyTestRefController);
		SRTTICache::RegisterNiConstructor(RE::RTTI_BSPSysSimpleColorModifier);
		SRTTICache::RegisterNiConstructor(RE::RTTI_BSPSysLODModifier);
		SRTTICache::RegisterNiConstructor(RE::RTTI_BSOrderedNode);
		SRTTICache::RegisterNiConstructor(RE::RTTI_BSLeafAnimNode);
	}
}
