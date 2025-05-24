#include "Hooks.h"

#include "Core/ShaderCache.h"
#include "Serialization/Serializer.h"
#include "Utils/Hooking.h"
#include "Utils/TargetManager.h"

#include "RE/A/Actor.h"
#include "RE/A/ActorSpeedChannel.h"
#include "RE/A/AIProcess.h"
#include "RE/B/BGSActionData.h"
#include "RE/B/BGSDefaultObjectManager.h"
#include "RE/B/bhkRigidBody.h"
#include "RE/B/BSAnimationGraphEvent.h"
#include "RE/B/BSAnimationGraphManager.h"
#include "RE/B/BShkbAnimationGraph.h"
#include "RE/B/BSTAnimationGraphDataChannel.h"
#include "RE/C/CommandTable.h"
#include "RE/C/ConsoleLog.h"
#include "RE/H/hkbBehaviorGraph.h"
#include "RE/H/hkbBehaviorGraphData.h"
#include "RE/H/hkbVariableValueSet.h"
#include "RE/M/MiddleHighProcessData.h"
#include "RE/S/Sky.h"
#include "RE/T/TESWeather.h"
#include <RE/T/TESWaterObject.h>
#include <RE/B/BSWaterShaderMaterial.h>
#include <RE/A/ActorCopyGraphVariableChannel.h>
#include <RE/A/ActorDirectionChannel.h>
#include <RE/A/ActorLeftWeaponSpeedChannel.h>
#include <RE/A/ActorLookAtChannel.h>
#include <RE/A/ActorPitchChannel.h>
#include <RE/A/ActorPitchDeltaChannel.h>
#include <RE/A/ActorRollChannel.h>
#include <RE/A/ActorTimeDeltaChannel.h>
#include <RE/A/ActorWantBlockChannel.h>
#include <RE/A/ActorWardHealthChannel.h>
#include <RE/A/ActorTargetSpeedChannel.h>
#include <RE/A/ActorWeaponSpeedChannel.h>
#include <RE/A/ActorTurnDeltaChannel.h>
#include <RE/T/TESWaterForm.h>
#include <RE/P/PlayerCharacter.h>
#include <RE/B/BSShaderMaterial.h>
#include <RE/B/BSLightingShaderMaterialBase.h>
#include <RE/B/BSLightingShaderMaterialLandscape.h>
#include <RE/B/BSLightingShaderMaterialLODLandscape.h>
#include <RE/T/TESLandTexture.h>
#include <RE/B/BSLightingShaderProperty.h>
#include <RE/E/ExtraGhost.h>
#include <RE/I/ImageSpaceManager.h>
#include <RE/B/BSXFlags.h>
#include <RE/B/BSRenderPass.h>
#include <RE/N/NiStream.h>
#include <RE/B/bhkCompressedMeshShape.h>
#include <RE/N/NiRTTI.h>
#include <RE/N/NiSkinInstance.h>
#include <RE/N/NiSkinData.h>
#include <RE/N/NiSkinPartition.h>
#include <RE/B/BSGeometry.h>
#include <RE/B/BSFadeNode.h>
#include <RE/B/BSLightingShaderMaterialParallax.h>
#include <RE/R/Renderer.h>
#include <RE/B/BSShadowLight.h>
#include <RE/B/BSBatchRenderer.h>
#include <RE/R/RendererState.h>
#include <RE/B/BSShaderAccumulator.h>

#include <magic_enum/magic_enum.hpp>

#include <iostream>
#include <tchar.h>
#include <unordered_set>
#include <stacktrace>

#include <d3d11.h>
#include <Windows.h>

#include <D3d11_1.h>
#include <wrl/client.h>

namespace FrameAnnotations
{
	struct ScopedFrameEvent
	{
		ScopedFrameEvent(const std::string& eventName)
		{
			HRESULT hr = RE::BSGraphics::Renderer::GetDeviceContext()->QueryInterface(
				reinterpret_cast<const REX::W32::GUID&>(__uuidof(annotation)),
				reinterpret_cast<void**>(&annotation));
			if (!FAILED(hr))
			{
				annotation->BeginEvent(std::wstring(eventName.begin(), eventName.end()).c_str());
			}
		}

		~ScopedFrameEvent()
		{
			if (annotation != nullptr)
			{
				annotation->EndEvent();
			}
		}

	private:
		ID3DUserDefinedAnnotation* annotation = nullptr;
	};

	void BeginFrameEvent(const std::string& eventName)
	{
		ID3DUserDefinedAnnotation* annotation = nullptr;
		HRESULT hr = RE::BSGraphics::Renderer::GetDeviceContext()->QueryInterface(
			reinterpret_cast<const REX::W32::GUID&>(__uuidof(annotation)),
			reinterpret_cast<void**>(&annotation));
		if (!FAILED(hr))
		{
			annotation->BeginEvent(std::wstring(eventName.begin(), eventName.end()).c_str());
		}
	}

	void EndFrameEvent()
	{
		ID3DUserDefinedAnnotation* annotation = nullptr;
		HRESULT hr = RE::BSGraphics::Renderer::GetDeviceContext()->QueryInterface(
			reinterpret_cast<const REX::W32::GUID&>(__uuidof(annotation)),
			reinterpret_cast<void**>(&annotation));
		if (!FAILED(hr))
		{
			annotation->EndEvent();
		}
	}

	template<RE::BSShader::Type ShaderType>
	struct BSShader_SetupGeometry
	{
		static void thunk(RE::BSShader* shader, RE::BSRenderPass* pass, uint32_t renderFlags)
		{
			const std::string passName = std::format("[{}:{:x}] <{}> {}", magic_enum::enum_name(ShaderType), pass->passEnum,
					pass->accumulationHint.underlying(), pass->geometry->name.c_str());
			BeginFrameEvent(passName);

			func(shader, pass, renderFlags);
		}
	
		static inline REL::Relocation<decltype(thunk)> func;
		static constexpr size_t idx = 0x06;
	};

	template<RE::BSShader::Type ShaderType>
	struct BSShader_RestoreGeometry
	{
		static void thunk(RE::BSShader* shader, RE::BSRenderPass* pass, uint32_t renderFlags)
		{
			func(shader, pass, renderFlags);

			EndFrameEvent();
		}

		static inline REL::Relocation<decltype(thunk)> func;
		static constexpr size_t idx = 0x07;
	};

	template <size_t RendererIndex>
	struct AnnotatedRenderer
	{
		static inline void (*OriginalRenderer)(RE::BSShaderAccumulator*, uint32_t) = nullptr;

		static void Init()
		{
			static REL::Relocation<void (**)(RE::BSShaderAccumulator*, uint32_t)> RendererArray(
				RELOCATION_ID(527779, 414716));

			if (RendererArray.get()[RendererIndex] != nullptr)
			{
				OriginalRenderer = RendererArray.get()[RendererIndex];
				RendererArray.get()[RendererIndex] = &AnnotatedRenderer<RendererIndex>::Render;
			}

			if constexpr (RendererIndex > 0)
			{
				AnnotatedRenderer<RendererIndex - 1>::Init();
			}
		}

		static void Render(RE::BSShaderAccumulator* shaderAccumulator, uint32_t renderFlags)
		{
			if (OriginalRenderer != nullptr)
			{
				BeginFrameEvent(std::format("Render Pass {} <{}>", RendererIndex, renderFlags));

				OriginalRenderer(shaderAccumulator, renderFlags);

				EndFrameEvent();
			}
		}
	};
	
	struct BSBatchRenderer_Unk03
	{
		static void thunk(void* renderer, uint32_t firstPass, uint32_t lastPass, uint32_t renderFlags)
		{
			BeginFrameEvent(
				std::format("BSBatchRenderer::Unk03 ({},{}) <{}>", firstPass, lastPass, renderFlags));

			func(renderer, firstPass, lastPass, renderFlags);

			EndFrameEvent();
		}

		static inline REL::Relocation<decltype(thunk)> func;
		static constexpr size_t idx = 0x03;
	};

	struct BSBatchRenderer_RenderBatches
	{
		static bool thunk(void* renderer, uint32_t* currentPass, uint32_t* bucketIndex,
			void* passIndexList,
			uint32_t renderFlags)
		{
			BeginFrameEvent(std::format("BSBatchRenderer::RenderBatches ({})[{}] <{}>", *currentPass, *bucketIndex,
				renderFlags));

			const bool result =
				func(renderer, currentPass, bucketIndex, passIndexList, renderFlags);

			EndFrameEvent();

			return result;
		}

		static inline REL::Relocation<decltype(thunk)> func;
	};

	struct BSShaderAccumulator_FinishAccumulatingDispatch
	{
		static void thunk(RE::BSShaderAccumulator* shaderAccumulator, uint32_t renderFlags)
		{
			BeginFrameEvent(std::format("BSShaderAccumulator::FinishAccumulatingDispatch [{}] <{}>",
				static_cast<uint32_t>(shaderAccumulator->renderMode), renderFlags));

			func(shaderAccumulator, renderFlags);

			EndFrameEvent();
		}

		static inline REL::Relocation<decltype(thunk)> func;
		static constexpr size_t idx = 0x2A;
	};

	struct BSShaderAccumulator_Unk2B
	{
		static void thunk(RE::BSShaderAccumulator* shaderAccumulator, uint32_t renderFlags)
		{
			BeginFrameEvent(std::format("BSShaderAccumulator::Unk2B [{}] <{}>",
				static_cast<uint32_t>(shaderAccumulator->renderMode), renderFlags));

			func(shaderAccumulator, renderFlags);

			EndFrameEvent();
		}

		static inline REL::Relocation<decltype(thunk)> func;
		static constexpr size_t idx = 0x2B;
	};

	struct BSCubeMapCamera_RenderCubemap
	{
		static void thunk(RE::NiAVObject* camera, int a2, bool a3, bool a4, bool a5)
		{
			BeginFrameEvent("Cubemap");

			func(camera, a2, a3, a4, a5);

			EndFrameEvent();
		}

		static inline REL::Relocation<decltype(thunk)> func;
		static constexpr size_t idx = 0x35;
	};

	template <RE::ImageSpaceManager::ImageSpaceEffectEnum EffectType>
	struct BSImagespaceShader_Render
	{
		static void thunk(void* imageSpaceShader, RE::BSTriShape* shape, RE::ImageSpaceEffectParam* param)
		{
			BeginFrameEvent(std::format("{}", magic_enum::enum_name(EffectType)));

			func(imageSpaceShader, shape, param);

			EndFrameEvent();
		}

		static inline REL::Relocation<decltype(thunk)> func;
		static constexpr size_t idx = 0x01;
	};

	struct TESWaterSystem_Update
	{
		static bool thunk(void* waterReflections)
		{
			BeginFrameEvent("Water Reflections");

			const bool result = func(waterReflections);

			EndFrameEvent();

			return result;
		}

		static inline REL::Relocation<decltype(thunk)> func;
	};

	struct BSShadowDirectionalLight_RenderShadowmaps
	{
		static void thunk(RE::BSShadowLight* light, void* a2)
		{
			BeginFrameEvent("Directional Light Shadowmaps");

			func(light, a2);

			EndFrameEvent();
		}

		static inline REL::Relocation<decltype(thunk)> func;
		static constexpr size_t idx = 0x0A;
	};

	struct BSShadowFrustumLight_RenderShadowmaps
	{
		static void thunk(RE::BSShadowLight* light, void* a2)
		{
			BeginFrameEvent("Spot Light Shadowmaps");

			func(light, a2);

			EndFrameEvent();
		}

		static inline REL::Relocation<decltype(thunk)> func;
		static constexpr size_t idx = 0x0A;
	};

	struct BSShadowParabolicLight_RenderShadowmaps
	{
		static void thunk(RE::BSShadowLight* light, void* a2)
		{
			BeginFrameEvent("Omnidirectional Light Shadowmaps");

			func(light, a2);

			EndFrameEvent();
		}

		static inline REL::Relocation<decltype(thunk)> func;
		static constexpr size_t idx = 0x0A;
	};

	struct Main_RenderDepth
	{
		static void thunk(bool a1, bool a2)
		{
			BeginFrameEvent("Depth");

			func(a1, a2);

			EndFrameEvent();
		}

		static inline REL::Relocation<decltype(thunk)> func;
	};

	struct Main_RenderShadowmasks
	{
		static void thunk(bool a1)
		{
			BeginFrameEvent("Shadowmasks");

			func(a1);

			EndFrameEvent();
		}

		static inline REL::Relocation<decltype(thunk)> func;
	};

	struct Main_RenderWorld
	{
		static void thunk(bool a1)
		{
			BeginFrameEvent("World");

			func(a1);

			EndFrameEvent();
		}

		static inline REL::Relocation<decltype(thunk)> func;
	};

	struct Main_RenderFirstPersonView
	{
		static void thunk(bool a1, bool a2)
		{
			BeginFrameEvent("First Person View");

			func(a1, a2);

			EndFrameEvent();
		}

		static inline REL::Relocation<decltype(thunk)> func;
	};

	struct Main_RenderWaterEffects
	{
		static void thunk()
		{
			BeginFrameEvent("Water Effects");

			func();

			EndFrameEvent();
		}

		static inline REL::Relocation<decltype(thunk)> func;
	};

	struct Main_RenderPlayerView
	{
		static void thunk(void* a1, bool a2, bool a3)
		{
			BeginFrameEvent("Player View");

			func(a1, a2, a3);

			EndFrameEvent();
		}

		static inline REL::Relocation<decltype(thunk)> func;
	};

	enum class RenderBatchesPass
	{
		Objects,
		Grass,
		Sky,
		Particles,
		Water,
		SunGlare
	};

	template<RenderBatchesPass Pass>
	struct RenderBatches_Objects
	{
		static void thunk(void* shaderAccumulator, uint32_t firstPass, uint32_t lastPass, uint32_t renderFlags, int groupIndex)
		{
			BeginFrameEvent(magic_enum::enum_name(Pass).data());

			func(shaderAccumulator, firstPass, lastPass, renderFlags, groupIndex);

			EndFrameEvent();
		}

		static inline REL::Relocation<decltype(thunk)> func;
	};

	template <uint32_t GroupIndex>
	struct RenderPersistentPassList
	{
		static void thunk(void* group, uint32_t renderFlags)
		{
			BeginFrameEvent(std::format("Geometry Group {}", GroupIndex));

			func(group, renderFlags);

			EndFrameEvent();
		}

		static inline REL::Relocation<decltype(thunk)> func;
	};

	template <uint32_t GroupIndex>
	struct RenderBatches_GeometryGroup
	{
		static void thunk(void* shaderAccumulator, uint32_t firstPass, uint32_t lastPass,
			uint32_t renderFlags, int groupIndex)
		{
			BeginFrameEvent(std::format("Geometry Group {}", GroupIndex));

			func(shaderAccumulator, firstPass, lastPass, renderFlags, groupIndex);

			EndFrameEvent();
		}

		static inline REL::Relocation<decltype(thunk)> func;
	};

	template <bool BeforeWater>
	struct RenderEffects
	{
		static void thunk(void* shaderAccumulator, uint32_t renderFlags)
		{
			BeginFrameEvent("Effects");

			func(shaderAccumulator, renderFlags);

			EndFrameEvent();
		}

		static inline REL::Relocation<decltype(thunk)> func;
	};

	void InstallThunks()
	{
		stl::write_vfunc<BSShader_SetupGeometry<RE::BSShader::Type::Lighting>>(
			RE::VTABLE_BSLightingShader[0]);
		stl::write_vfunc<BSShader_SetupGeometry<RE::BSShader::Type::Effect>>(
			RE::VTABLE_BSEffectShader[0]);
		stl::write_vfunc<BSShader_SetupGeometry<RE::BSShader::Type::Water>>(
			RE::VTABLE_BSWaterShader[0]);
		stl::write_vfunc<BSShader_SetupGeometry<RE::BSShader::Type::Utility>>(
			RE::VTABLE_BSUtilityShader[0]);
		stl::write_vfunc<BSShader_SetupGeometry<RE::BSShader::Type::Particle>>(
			RE::VTABLE_BSParticleShader[0]);
		stl::write_vfunc<BSShader_SetupGeometry<RE::BSShader::Type::Grass>>(
			RE::VTABLE_BSGrassShader[0]);
		stl::write_vfunc<BSShader_SetupGeometry<RE::BSShader::Type::DistantTree>>(
			RE::VTABLE_BSDistantTreeShader[0]);
		stl::write_vfunc<BSShader_SetupGeometry<RE::BSShader::Type::BloodSplatter>>(
			RE::VTABLE_BSBloodSplatterShader[0]);
		stl::write_vfunc<BSShader_SetupGeometry<RE::BSShader::Type::Sky>>(
			RE::VTABLE_BSSkyShader[0]);

		stl::write_vfunc<BSShader_RestoreGeometry<RE::BSShader::Type::Lighting>>(
			RE::VTABLE_BSLightingShader[0]);
		stl::write_vfunc<BSShader_RestoreGeometry<RE::BSShader::Type::Effect>>(
			RE::VTABLE_BSEffectShader[0]);
		stl::write_vfunc<BSShader_RestoreGeometry<RE::BSShader::Type::Water>>(
			RE::VTABLE_BSWaterShader[0]);
		stl::write_vfunc<BSShader_RestoreGeometry<RE::BSShader::Type::Utility>>(
			RE::VTABLE_BSUtilityShader[0]);
		stl::write_vfunc<BSShader_RestoreGeometry<RE::BSShader::Type::Particle>>(
			RE::VTABLE_BSParticleShader[0]);
		stl::write_vfunc<BSShader_RestoreGeometry<RE::BSShader::Type::Grass>>(
			RE::VTABLE_BSGrassShader[0]);
		stl::write_vfunc<BSShader_RestoreGeometry<RE::BSShader::Type::DistantTree>>(
			RE::VTABLE_BSDistantTreeShader[0]);
		stl::write_vfunc<BSShader_RestoreGeometry<RE::BSShader::Type::BloodSplatter>>(
			RE::VTABLE_BSBloodSplatterShader[0]);
		stl::write_vfunc<BSShader_RestoreGeometry<RE::BSShader::Type::Sky>>(
			RE::VTABLE_BSSkyShader[0]);

		stl::write_vfunc<BSImagespaceShader_Render<RE::ImageSpaceManager::ISFXAA>>(
			RE::VTABLE_BSImagespaceShaderFXAA[3]);
		stl::write_vfunc<BSImagespaceShader_Render<RE::ImageSpaceManager::ISCopy>>(
			RE::VTABLE_BSImagespaceShaderCopy[3]);
		stl::write_vfunc<
			BSImagespaceShader_Render<RE::ImageSpaceManager::ISCopyDynamicFetchDisabled>>(
			RE::VTABLE_BSImagespaceShaderCopyDynamicFetchDisabled[3]);
		stl::write_vfunc<BSImagespaceShader_Render<RE::ImageSpaceManager::ISCopyScaleBias>>(
			RE::VTABLE_BSImagespaceShaderCopyScaleBias[3]);
		stl::write_vfunc<BSImagespaceShader_Render<RE::ImageSpaceManager::ISCopyCustomViewport>>(
			RE::VTABLE_BSImagespaceShaderCopyCustomViewport[3]);
		stl::write_vfunc<BSImagespaceShader_Render<RE::ImageSpaceManager::ISCopyGrayScale>>(
			RE::VTABLE_BSImagespaceShaderGreyScale[3]);
		stl::write_vfunc<BSImagespaceShader_Render<RE::ImageSpaceManager::ISRefraction>>(
			RE::VTABLE_BSImagespaceShaderRefraction[3]);
		stl::write_vfunc<BSImagespaceShader_Render<RE::ImageSpaceManager::ISDoubleVision>>(
			RE::VTABLE_BSImagespaceShaderDoubleVision[3]);
		stl::write_vfunc<BSImagespaceShader_Render<RE::ImageSpaceManager::ISCopyTextureMask>>(
			RE::VTABLE_BSImagespaceShaderTextureMask[3]);
		stl::write_vfunc<BSImagespaceShader_Render<RE::ImageSpaceManager::ISMap>>(
			RE::VTABLE_BSImagespaceShaderMap[3]);
		stl::write_vfunc<BSImagespaceShader_Render<RE::ImageSpaceManager::ISWorldMap>>(
			RE::VTABLE_BSImagespaceShaderWorldMap[3]);
		stl::write_vfunc<BSImagespaceShader_Render<RE::ImageSpaceManager::ISWorldMapNoSkyBlur>>(
			RE::VTABLE_BSImagespaceShaderWorldMapNoSkyBlur[3]);
		stl::write_vfunc<BSImagespaceShader_Render<RE::ImageSpaceManager::ISDepthOfField>>(
			RE::VTABLE_BSImagespaceShaderDepthOfField[3]);
		stl::write_vfunc<BSImagespaceShader_Render<RE::ImageSpaceManager::ISDepthOfFieldFogged>>(
			RE::VTABLE_BSImagespaceShaderDepthOfFieldFogged[3]);
		stl::write_vfunc<
			BSImagespaceShader_Render<RE::ImageSpaceManager::ISDepthOfFieldMaskedFogged>>(
			RE::VTABLE_BSImagespaceShaderDepthOfFieldMaskedFogged[3]);
		stl::write_vfunc<BSImagespaceShader_Render<RE::ImageSpaceManager::ISDistantBlur>>(
			RE::VTABLE_BSImagespaceShaderDistantBlur[3]);
		stl::write_vfunc<BSImagespaceShader_Render<RE::ImageSpaceManager::ISDistantBlurFogged>>(
			RE::VTABLE_BSImagespaceShaderDistantBlurFogged[3]);
		stl::write_vfunc<
			BSImagespaceShader_Render<RE::ImageSpaceManager::ISDistantBlurMaskedFogged>>(
			RE::VTABLE_BSImagespaceShaderDistantBlurMaskedFogged[3]);
		stl::write_vfunc<BSImagespaceShader_Render<RE::ImageSpaceManager::ISRadialBlur>>(
			RE::VTABLE_BSImagespaceShaderRadialBlur[3]);
		stl::write_vfunc<BSImagespaceShader_Render<RE::ImageSpaceManager::ISRadialBlurMedium>>(
			RE::VTABLE_BSImagespaceShaderRadialBlurMedium[3]);
		stl::write_vfunc<BSImagespaceShader_Render<RE::ImageSpaceManager::ISRadialBlurHigh>>(
			RE::VTABLE_BSImagespaceShaderRadialBlurHigh[3]);
		stl::write_vfunc<
			BSImagespaceShader_Render<RE::ImageSpaceManager::ISHDRTonemapBlendCinematic>>(
			RE::VTABLE_BSImagespaceShaderHDRTonemapBlendCinematic[3]);
		stl::write_vfunc<
			BSImagespaceShader_Render<RE::ImageSpaceManager::ISHDRTonemapBlendCinematicFade>>(
			RE::VTABLE_BSImagespaceShaderHDRTonemapBlendCinematicFade[3]);
		stl::write_vfunc<BSImagespaceShader_Render<RE::ImageSpaceManager::ISHDRDownSample16>>(
			RE::VTABLE_BSImagespaceShaderHDRDownSample16[3]);
		stl::write_vfunc<BSImagespaceShader_Render<RE::ImageSpaceManager::ISHDRDownSample4>>(
			RE::VTABLE_BSImagespaceShaderHDRDownSample4[3]);
		stl::write_vfunc<BSImagespaceShader_Render<RE::ImageSpaceManager::ISHDRDownSample16Lum>>(
			RE::VTABLE_BSImagespaceShaderHDRDownSample16Lum[3]);
		stl::write_vfunc<BSImagespaceShader_Render<RE::ImageSpaceManager::ISHDRDownSample4RGB2Lum>>(
			RE::VTABLE_BSImagespaceShaderHDRDownSample4RGB2Lum[3]);
		stl::write_vfunc<
			BSImagespaceShader_Render<RE::ImageSpaceManager::ISHDRDownSample4LumClamp>>(
			RE::VTABLE_BSImagespaceShaderHDRDownSample4LumClamp[3]);
		stl::write_vfunc<
			BSImagespaceShader_Render<RE::ImageSpaceManager::ISHDRDownSample4LightAdapt>>(
			RE::VTABLE_BSImagespaceShaderHDRDownSample4LightAdapt[3]);
		stl::write_vfunc<
			BSImagespaceShader_Render<RE::ImageSpaceManager::ISHDRDownSample16LumClamp>>(
			RE::VTABLE_BSImagespaceShaderHDRDownSample16LumClamp[3]);
		stl::write_vfunc<
			BSImagespaceShader_Render<RE::ImageSpaceManager::ISHDRDownSample16LightAdapt>>(
			RE::VTABLE_BSImagespaceShaderHDRDownSample16LightAdapt[3]);
		stl::write_vfunc<BSImagespaceShader_Render<RE::ImageSpaceManager::ISBlur3>>(
			RE::VTABLE_BSImagespaceShaderBlur3[3]);
		stl::write_vfunc<BSImagespaceShader_Render<RE::ImageSpaceManager::ISBlur5>>(
			RE::VTABLE_BSImagespaceShaderBlur5[3]);
		stl::write_vfunc<BSImagespaceShader_Render<RE::ImageSpaceManager::ISBlur7>>(
			RE::VTABLE_BSImagespaceShaderBlur7[3]);
		stl::write_vfunc<BSImagespaceShader_Render<RE::ImageSpaceManager::ISBlur9>>(
			RE::VTABLE_BSImagespaceShaderBlur9[3]);
		stl::write_vfunc<BSImagespaceShader_Render<RE::ImageSpaceManager::ISBlur11>>(
			RE::VTABLE_BSImagespaceShaderBlur11[3]);
		stl::write_vfunc<BSImagespaceShader_Render<RE::ImageSpaceManager::ISBlur13>>(
			RE::VTABLE_BSImagespaceShaderBlur13[3]);
		stl::write_vfunc<BSImagespaceShader_Render<RE::ImageSpaceManager::ISBlur15>>(
			RE::VTABLE_BSImagespaceShaderBlur15[3]);
		stl::write_vfunc<BSImagespaceShader_Render<RE::ImageSpaceManager::ISNonHDRBlur3>>(
			RE::VTABLE_BSImagespaceShaderNonHDRBlur3[3]);
		stl::write_vfunc<BSImagespaceShader_Render<RE::ImageSpaceManager::ISNonHDRBlur5>>(
			RE::VTABLE_BSImagespaceShaderNonHDRBlur5[3]);
		stl::write_vfunc<BSImagespaceShader_Render<RE::ImageSpaceManager::ISNonHDRBlur7>>(
			RE::VTABLE_BSImagespaceShaderNonHDRBlur7[3]);
		stl::write_vfunc<BSImagespaceShader_Render<RE::ImageSpaceManager::ISNonHDRBlur9>>(
			RE::VTABLE_BSImagespaceShaderNonHDRBlur9[3]);
		stl::write_vfunc<BSImagespaceShader_Render<RE::ImageSpaceManager::ISNonHDRBlur11>>(
			RE::VTABLE_BSImagespaceShaderNonHDRBlur11[3]);
		stl::write_vfunc<BSImagespaceShader_Render<RE::ImageSpaceManager::ISNonHDRBlur13>>(
			RE::VTABLE_BSImagespaceShaderNonHDRBlur13[3]);
		stl::write_vfunc<BSImagespaceShader_Render<RE::ImageSpaceManager::ISNonHDRBlur15>>(
			RE::VTABLE_BSImagespaceShaderNonHDRBlur15[3]);
		stl::write_vfunc<BSImagespaceShader_Render<RE::ImageSpaceManager::ISBrightPassBlur3>>(
			RE::VTABLE_BSImagespaceShaderBrightPassBlur3[3]);
		stl::write_vfunc<BSImagespaceShader_Render<RE::ImageSpaceManager::ISBrightPassBlur5>>(
			RE::VTABLE_BSImagespaceShaderBrightPassBlur5[3]);
		stl::write_vfunc<BSImagespaceShader_Render<RE::ImageSpaceManager::ISBrightPassBlur7>>(
			RE::VTABLE_BSImagespaceShaderBrightPassBlur7[3]);
		stl::write_vfunc<BSImagespaceShader_Render<RE::ImageSpaceManager::ISBrightPassBlur9>>(
			RE::VTABLE_BSImagespaceShaderBrightPassBlur9[3]);
		stl::write_vfunc<BSImagespaceShader_Render<RE::ImageSpaceManager::ISBrightPassBlur11>>(
			RE::VTABLE_BSImagespaceShaderBrightPassBlur11[3]);
		stl::write_vfunc<BSImagespaceShader_Render<RE::ImageSpaceManager::ISBrightPassBlur13>>(
			RE::VTABLE_BSImagespaceShaderBrightPassBlur13[3]);
		stl::write_vfunc<BSImagespaceShader_Render<RE::ImageSpaceManager::ISBrightPassBlur15>>(
			RE::VTABLE_BSImagespaceShaderBrightPassBlur15[3]);
		stl::write_vfunc<
			BSImagespaceShader_Render<RE::ImageSpaceManager::ISWaterDisplacementClearSimulation>>(
			RE::VTABLE_BSImagespaceShaderWaterDisplacementClearSimulation[3]);
		stl::write_vfunc<
			BSImagespaceShader_Render<RE::ImageSpaceManager::ISWaterDisplacementTexOffset>>(
			RE::VTABLE_BSImagespaceShaderWaterDisplacementTexOffset[3]);
		stl::write_vfunc<
			BSImagespaceShader_Render<RE::ImageSpaceManager::ISWaterDisplacementWadingRipple>>(
			RE::VTABLE_BSImagespaceShaderWaterDisplacementWadingRipple[3]);
		stl::write_vfunc<
			BSImagespaceShader_Render<RE::ImageSpaceManager::ISWaterDisplacementRainRipple>>(
			RE::VTABLE_BSImagespaceShaderWaterDisplacementRainRipple[3]);
		stl::write_vfunc<BSImagespaceShader_Render<RE::ImageSpaceManager::ISWaterWadingHeightmap>>(
			RE::VTABLE_BSImagespaceShaderWaterWadingHeightmap[3]);
		stl::write_vfunc<BSImagespaceShader_Render<RE::ImageSpaceManager::ISWaterRainHeightmap>>(
			RE::VTABLE_BSImagespaceShaderWaterRainHeightmap[3]);
		stl::write_vfunc<BSImagespaceShader_Render<RE::ImageSpaceManager::ISWaterBlendHeightmaps>>(
			RE::VTABLE_BSImagespaceShaderWaterBlendHeightmaps[3]);
		stl::write_vfunc<BSImagespaceShader_Render<RE::ImageSpaceManager::ISWaterSmoothHeightmap>>(
			RE::VTABLE_BSImagespaceShaderWaterSmoothHeightmap[3]);
		stl::write_vfunc<
			BSImagespaceShader_Render<RE::ImageSpaceManager::ISWaterDisplacementNormals>>(
			RE::VTABLE_BSImagespaceShaderWaterDisplacementNormals[3]);
		stl::write_vfunc<BSImagespaceShader_Render<RE::ImageSpaceManager::ISNoiseScrollAndBlend>>(
			RE::VTABLE_BSImagespaceShaderNoiseScrollAndBlend[3]);
		stl::write_vfunc<BSImagespaceShader_Render<RE::ImageSpaceManager::ISNoiseNormalmap>>(
			RE::VTABLE_BSImagespaceShaderNoiseNormalmap[3]);
		stl::write_vfunc<BSImagespaceShader_Render<RE::ImageSpaceManager::ISVolumetricLighting>>(
			RE::VTABLE_BSImagespaceShaderVolumetricLighting[3]);
		stl::write_vfunc<BSImagespaceShader_Render<RE::ImageSpaceManager::ISLocalMap>>(
			RE::VTABLE_BSImagespaceShaderLocalMap[3]);
		stl::write_vfunc<BSImagespaceShader_Render<RE::ImageSpaceManager::ISAlphaBlend>>(
			RE::VTABLE_BSImagespaceShaderAlphaBlend[3]);
		stl::write_vfunc<BSImagespaceShader_Render<RE::ImageSpaceManager::ISLensFlare>>(
			RE::VTABLE_BSImagespaceShaderLensFlare[3]);
		stl::write_vfunc<BSImagespaceShader_Render<RE::ImageSpaceManager::ISLensFlareVisibility>>(
			RE::VTABLE_BSImagespaceShaderLensFlareVisibility[3]);
		stl::write_vfunc<BSImagespaceShader_Render<RE::ImageSpaceManager::ISApplyReflections>>(
			RE::VTABLE_BSImagespaceShaderApplyReflections[3]);
		stl::write_vfunc<
			BSImagespaceShader_Render<RE::ImageSpaceManager::ISApplyVolumetricLighting>>(
			RE::VTABLE_BSImagespaceShaderISApplyVolumetricLighting[3]);
		stl::write_vfunc<BSImagespaceShader_Render<RE::ImageSpaceManager::ISBasicCopy>>(
			RE::VTABLE_BSImagespaceShaderISBasicCopy[3]);
		stl::write_vfunc<BSImagespaceShader_Render<RE::ImageSpaceManager::ISBlur>>(
			RE::VTABLE_BSImagespaceShaderISBlur[3]);
		stl::write_vfunc<
			BSImagespaceShader_Render<RE::ImageSpaceManager::ISVolumetricLightingBlurHCS>>(
			RE::VTABLE_BSImagespaceShaderISVolumetricLightingBlurHCS[3]);
		stl::write_vfunc<
			BSImagespaceShader_Render<RE::ImageSpaceManager::ISVolumetricLightingBlurVCS>>(
			RE::VTABLE_BSImagespaceShaderISVolumetricLightingBlurVCS[3]);
		stl::write_vfunc<BSImagespaceShader_Render<RE::ImageSpaceManager::ISReflectionBlurHCS>>(
			RE::VTABLE_BSImagespaceShaderReflectionBlurHCS[3]);
		stl::write_vfunc<BSImagespaceShader_Render<RE::ImageSpaceManager::ISReflectionBlurVCS>>(
			RE::VTABLE_BSImagespaceShaderReflectionBlurVCS[3]);
		stl::write_vfunc<BSImagespaceShader_Render<RE::ImageSpaceManager::ISParallaxMaskBlurHCS>>(
			RE::VTABLE_BSImagespaceShaderISParallaxMaskBlurHCS[3]);
		stl::write_vfunc<BSImagespaceShader_Render<RE::ImageSpaceManager::ISParallaxMaskBlurVCS>>(
			RE::VTABLE_BSImagespaceShaderISParallaxMaskBlurVCS[3]);
		stl::write_vfunc<BSImagespaceShader_Render<RE::ImageSpaceManager::ISDepthOfFieldBlurHCS>>(
			RE::VTABLE_BSImagespaceShaderISDepthOfFieldBlurHCS[3]);
		stl::write_vfunc<BSImagespaceShader_Render<RE::ImageSpaceManager::ISDepthOfFieldBlurVCS>>(
			RE::VTABLE_BSImagespaceShaderISDepthOfFieldBlurVCS[3]);
		stl::write_vfunc<
			BSImagespaceShader_Render<RE::ImageSpaceManager::ISCompositeVolumetricLighting>>(
			RE::VTABLE_BSImagespaceShaderISCompositeVolumetricLighting[3]);
		stl::write_vfunc<BSImagespaceShader_Render<RE::ImageSpaceManager::ISCompositeLensFlare>>(
			RE::VTABLE_BSImagespaceShaderISCompositeLensFlare[3]);
		stl::write_vfunc<BSImagespaceShader_Render<
			RE::ImageSpaceManager::ISCompositeLensFlareVolumetricLighting>>(
			RE::VTABLE_BSImagespaceShaderISCompositeLensFlareVolumetricLighting[3]);
		stl::write_vfunc<BSImagespaceShader_Render<RE::ImageSpaceManager::ISCopySubRegionCS>>(
			RE::VTABLE_BSImagespaceShaderISCopySubRegionCS[3]);
		stl::write_vfunc<BSImagespaceShader_Render<RE::ImageSpaceManager::ISDebugSnow>>(
			RE::VTABLE_BSImagespaceShaderISDebugSnow[3]);
		stl::write_vfunc<BSImagespaceShader_Render<RE::ImageSpaceManager::ISDownsample>>(
			RE::VTABLE_BSImagespaceShaderISDownsample[3]);
		stl::write_vfunc<
			BSImagespaceShader_Render<RE::ImageSpaceManager::ISDownsampleIgnoreBrightest>>(
			RE::VTABLE_BSImagespaceShaderISDownsampleIgnoreBrightest[3]);
		stl::write_vfunc<BSImagespaceShader_Render<RE::ImageSpaceManager::ISDownsampleCS>>(
			RE::VTABLE_BSImagespaceShaderISDownsampleCS[3]);
		stl::write_vfunc<
			BSImagespaceShader_Render<RE::ImageSpaceManager::ISDownsampleIgnoreBrightestCS>>(
			RE::VTABLE_BSImagespaceShaderISDownsampleIgnoreBrightestCS[3]);
		stl::write_vfunc<BSImagespaceShader_Render<RE::ImageSpaceManager::ISExp>>(
			RE::VTABLE_BSImagespaceShaderISExp[3]);
		stl::write_vfunc<BSImagespaceShader_Render<RE::ImageSpaceManager::ISIBLensFlares>>(
			RE::VTABLE_BSImagespaceShaderISIBLensFlares[3]);
		stl::write_vfunc<BSImagespaceShader_Render<RE::ImageSpaceManager::ISLightingComposite>>(
			RE::VTABLE_BSImagespaceShaderISLightingComposite[3]);
		stl::write_vfunc<BSImagespaceShader_Render<
			RE::ImageSpaceManager::ISLightingCompositeNoDirectionalLight>>(
			RE::VTABLE_BSImagespaceShaderISLightingCompositeNoDirectionalLight[3]);
		stl::write_vfunc<BSImagespaceShader_Render<RE::ImageSpaceManager::ISLightingCompositeMenu>>(
			RE::VTABLE_BSImagespaceShaderISLightingCompositeMenu[3]);
		stl::write_vfunc<BSImagespaceShader_Render<RE::ImageSpaceManager::ISPerlinNoiseCS>>(
			RE::VTABLE_BSImagespaceShaderISPerlinNoiseCS[3]);
		stl::write_vfunc<BSImagespaceShader_Render<RE::ImageSpaceManager::ISPerlinNoise2DCS>>(
			RE::VTABLE_BSImagespaceShaderISPerlinNoise2DCS[3]);
		stl::write_vfunc<BSImagespaceShader_Render<RE::ImageSpaceManager::ISReflectionsRayTracing>>(
			RE::VTABLE_BSImagespaceShaderReflectionsRayTracing[3]);
		stl::write_vfunc<
			BSImagespaceShader_Render<RE::ImageSpaceManager::ISReflectionsDebugSpecMask>>(
			RE::VTABLE_BSImagespaceShaderReflectionsDebugSpecMask[3]);
		stl::write_vfunc<BSImagespaceShader_Render<RE::ImageSpaceManager::ISSAOBlurH>>(
			RE::VTABLE_BSImagespaceShaderISSAOBlurH[3]);
		stl::write_vfunc<BSImagespaceShader_Render<RE::ImageSpaceManager::ISSAOBlurV>>(
			RE::VTABLE_BSImagespaceShaderISSAOBlurV[3]);
		stl::write_vfunc<BSImagespaceShader_Render<RE::ImageSpaceManager::ISSAOBlurHCS>>(
			RE::VTABLE_BSImagespaceShaderISSAOBlurHCS[3]);
		stl::write_vfunc<BSImagespaceShader_Render<RE::ImageSpaceManager::ISSAOBlurVCS>>(
			RE::VTABLE_BSImagespaceShaderISSAOBlurVCS[3]);
		stl::write_vfunc<BSImagespaceShader_Render<RE::ImageSpaceManager::ISSAOCameraZ>>(
			RE::VTABLE_BSImagespaceShaderISSAOCameraZ[3]);
		stl::write_vfunc<BSImagespaceShader_Render<RE::ImageSpaceManager::ISSAOCameraZAndMipsCS>>(
			RE::VTABLE_BSImagespaceShaderISSAOCameraZAndMipsCS[3]);
		stl::write_vfunc<BSImagespaceShader_Render<RE::ImageSpaceManager::ISSAOCompositeSAO>>(
			RE::VTABLE_BSImagespaceShaderISSAOCompositeSAO[3]);
		stl::write_vfunc<BSImagespaceShader_Render<RE::ImageSpaceManager::ISSAOCompositeFog>>(
			RE::VTABLE_BSImagespaceShaderISSAOCompositeFog[3]);
		stl::write_vfunc<BSImagespaceShader_Render<RE::ImageSpaceManager::ISSAOCompositeSAOFog>>(
			RE::VTABLE_BSImagespaceShaderISSAOCompositeSAOFog[3]);
		stl::write_vfunc<BSImagespaceShader_Render<RE::ImageSpaceManager::ISMinify>>(
			RE::VTABLE_BSImagespaceShaderISMinify[3]);
		stl::write_vfunc<BSImagespaceShader_Render<RE::ImageSpaceManager::ISMinifyContrast>>(
			RE::VTABLE_BSImagespaceShaderISMinifyContrast[3]);
		stl::write_vfunc<BSImagespaceShader_Render<RE::ImageSpaceManager::ISSAORawAO>>(
			RE::VTABLE_BSImagespaceShaderISSAORawAO[3]);
		stl::write_vfunc<BSImagespaceShader_Render<RE::ImageSpaceManager::ISSAORawAONoTemporal>>(
			RE::VTABLE_BSImagespaceShaderISSAORawAONoTemporal[3]);
		stl::write_vfunc<BSImagespaceShader_Render<RE::ImageSpaceManager::ISSAORawAOCS>>(
			RE::VTABLE_BSImagespaceShaderISSAORawAOCS[3]);
		stl::write_vfunc<BSImagespaceShader_Render<RE::ImageSpaceManager::ISSILComposite>>(
			RE::VTABLE_BSImagespaceShaderISSILComposite[3]);
		stl::write_vfunc<BSImagespaceShader_Render<RE::ImageSpaceManager::ISSILRawInd>>(
			RE::VTABLE_BSImagespaceShaderISSILRawInd[3]);
		stl::write_vfunc<BSImagespaceShader_Render<RE::ImageSpaceManager::ISSimpleColor>>(
			RE::VTABLE_BSImagespaceShaderISSimpleColor[3]);
		stl::write_vfunc<BSImagespaceShader_Render<RE::ImageSpaceManager::ISDisplayDepth>>(
			RE::VTABLE_BSImagespaceShaderISDisplayDepth[3]);
		stl::write_vfunc<BSImagespaceShader_Render<RE::ImageSpaceManager::ISSnowSSS>>(
			RE::VTABLE_BSImagespaceShaderISSnowSSS[3]);
		stl::write_vfunc<BSImagespaceShader_Render<RE::ImageSpaceManager::ISTemporalAA>>(
			RE::VTABLE_BSImagespaceShaderISTemporalAA[3]);
		stl::write_vfunc<BSImagespaceShader_Render<RE::ImageSpaceManager::ISTemporalAA_UI>>(
			RE::VTABLE_BSImagespaceShaderISTemporalAA_UI[3]);
		stl::write_vfunc<BSImagespaceShader_Render<RE::ImageSpaceManager::ISTemporalAA_Water>>(
			RE::VTABLE_BSImagespaceShaderISTemporalAA_Water[3]);
		stl::write_vfunc<
			BSImagespaceShader_Render<RE::ImageSpaceManager::ISUpsampleDynamicResolution>>(
			RE::VTABLE_BSImagespaceShaderISUpsampleDynamicResolution[3]);
		stl::write_vfunc<BSImagespaceShader_Render<RE::ImageSpaceManager::ISWaterBlend>>(
			RE::VTABLE_BSImagespaceShaderISWaterBlend[3]);
		stl::write_vfunc<BSImagespaceShader_Render<RE::ImageSpaceManager::ISUnderwaterMask>>(
			RE::VTABLE_BSImagespaceShaderISUnderwaterMask[3]);
		stl::write_vfunc<BSImagespaceShader_Render<RE::ImageSpaceManager::ISWaterFlow>>(
			RE::VTABLE_BSImagespaceShaderWaterFlow[3]);

		stl::write_vfunc<BSShaderAccumulator_FinishAccumulatingDispatch>(
			RE::VTABLE_BSShaderAccumulator[0]);
		stl::write_vfunc<BSShaderAccumulator_Unk2B>(
			RE::VTABLE_BSShaderAccumulator[0]);

		stl::write_vfunc<BSCubeMapCamera_RenderCubemap>(RE::VTABLE_BSCubeMapCamera[0]);

		stl::write_vfunc<BSBatchRenderer_Unk03>(RE::VTABLE_BSBatchRenderer[0]);

		stl::write_vfunc<BSShadowDirectionalLight_RenderShadowmaps>(
			RE::VTABLE_BSShadowDirectionalLight[0]);
		stl::write_vfunc<BSShadowFrustumLight_RenderShadowmaps>(
			RE::VTABLE_BSShadowFrustumLight[0]);
		stl::write_vfunc<BSShadowParabolicLight_RenderShadowmaps>(
			RE::VTABLE_BSShadowParabolicLight[0]);

		{
			const std::array targets{
				REL::Relocation<std::uintptr_t>(RELOCATION_ID(99963, 106609), OFFSET(0x10A, 0xF9)),
			};
			for (const auto& target : targets)
			{
				stl::write_thunk_call<BSBatchRenderer_RenderBatches>(target.address());
			}
		}
		{
			const std::array targets{
				REL::Relocation<std::uintptr_t>(RELOCATION_ID(31383, 32174), OFFSET(0x8A0, 0xC1C)),
			};
			for (const auto& target : targets)
			{
				stl::write_thunk_call<TESWaterSystem_Update>(target.address());
			}
		}
		{
			const auto RenderPlayerViewRelocationId = RELOCATION_ID(35560, 36559);
			stl::write_thunk_call<Main_RenderWaterEffects>(
				REL::Relocation<std::uintptr_t>(RenderPlayerViewRelocationId, 0x172).address());
			stl::write_thunk_call<Main_RenderDepth>(
				REL::Relocation<std::uintptr_t>(RenderPlayerViewRelocationId, 0x395).address());
			stl::write_thunk_call<Main_RenderShadowmasks>(
				REL::Relocation<std::uintptr_t>(RenderPlayerViewRelocationId, 0x39C).address());
			stl::write_thunk_call<Main_RenderWorld>(
				REL::Relocation<std::uintptr_t>(RenderPlayerViewRelocationId, OFFSET(0x831, 0x841))
					.address());
			stl::write_thunk_call<Main_RenderFirstPersonView>(
				REL::Relocation<std::uintptr_t>(RenderPlayerViewRelocationId, OFFSET(0x944, 0x954))
					.address());
		}
		{
			const std::array targets{
				REL::Relocation<std::uintptr_t>(RELOCATION_ID(35559, 36558), 0x11D),
			};
			for (const auto& target : targets)
			{
				stl::write_thunk_call<Main_RenderPlayerView>(target.address());
			}
		}
		{
			const auto PreResolveDepthRelocationId = RELOCATION_ID(99938, 106583);
			stl::write_thunk_call<RenderBatches_Objects<RenderBatchesPass::Objects>>(
				REL::Relocation<std::uintptr_t>(PreResolveDepthRelocationId, OFFSET(0x8E, 0x84))
					.address());
			stl::write_thunk_call<RenderPersistentPassList<9>>(
				REL::Relocation<std::uintptr_t>(PreResolveDepthRelocationId, OFFSET(0xAE, 0xA4))
					.address());
			stl::write_thunk_call<RenderBatches_GeometryGroup<9>>(
				REL::Relocation<std::uintptr_t>(PreResolveDepthRelocationId, OFFSET(0xCE, 0xC4))
					.address());
			stl::write_thunk_call<RenderBatches_Objects<RenderBatchesPass::Grass>>(
				REL::Relocation<std::uintptr_t>(PreResolveDepthRelocationId, OFFSET(0xEA, 0xE0))
					.address());
			stl::write_thunk_call<RenderPersistentPassList<8>>(
				REL::Relocation<std::uintptr_t>(PreResolveDepthRelocationId, OFFSET(0x10A, 0x100))
					.address());
			stl::write_thunk_call<RenderBatches_GeometryGroup<8>>(
				REL::Relocation<std::uintptr_t>(PreResolveDepthRelocationId, OFFSET(0x12A, 0x120))
					.address());
			stl::write_thunk_call<RenderPersistentPassList<1>>(
				REL::Relocation<std::uintptr_t>(PreResolveDepthRelocationId, OFFSET(0x147, 0x13D))
					.address());
			stl::write_thunk_call<RenderBatches_GeometryGroup<1>>(
				REL::Relocation<std::uintptr_t>(PreResolveDepthRelocationId, OFFSET(0x167, 0x15D))
					.address());
			stl::write_thunk_call<RenderPersistentPassList<0>>(
				REL::Relocation<std::uintptr_t>(PreResolveDepthRelocationId, OFFSET(0x1C6, 0x1B7))
					.address());
			stl::write_thunk_call<RenderBatches_GeometryGroup<0>>(
				REL::Relocation<std::uintptr_t>(PreResolveDepthRelocationId, OFFSET(0x1E2, 0x1D3))
					.address());
			stl::write_thunk_call<RenderBatches_Objects<RenderBatchesPass::Sky>>(
				REL::Relocation<std::uintptr_t>(PreResolveDepthRelocationId, OFFSET(0x247, 0x237))
					.address());
			stl::write_thunk_call<RenderPersistentPassList<13>>(
				REL::Relocation<std::uintptr_t>(PreResolveDepthRelocationId, OFFSET(0x284, 0x274))
					.address());
			stl::write_thunk_call<RenderBatches_GeometryGroup<13>>(
				REL::Relocation<std::uintptr_t>(PreResolveDepthRelocationId, OFFSET(0x3A4, 0x294))
					.address());
			stl::write_thunk_call<RenderBatches_Objects<RenderBatchesPass::Particles>>(
				REL::Relocation<std::uintptr_t>(PreResolveDepthRelocationId, OFFSET(0x4A6, 0x492))
					.address());

			const auto PostResolveDepthRelocationId = RELOCATION_ID(99939, 106584);
			stl::write_thunk_call<RenderEffects<true>>(
				REL::Relocation<std::uintptr_t>(PostResolveDepthRelocationId, OFFSET(0x54, 0x51))
					.address());
			stl::write_thunk_call<RenderPersistentPassList<10>>(
				REL::Relocation<std::uintptr_t>(PostResolveDepthRelocationId, OFFSET(0x126, 0x123))
					.address());
			stl::write_thunk_call<RenderBatches_GeometryGroup<10>>(
				REL::Relocation<std::uintptr_t>(PostResolveDepthRelocationId, OFFSET(0x146, 0x143))
					.address());
			stl::write_thunk_call<RenderBatches_Objects<RenderBatchesPass::Water>>(
				REL::Relocation<std::uintptr_t>(PostResolveDepthRelocationId, OFFSET(0x164, 0x161))
					.address());
			stl::write_thunk_call<RenderPersistentPassList<11>>(
				REL::Relocation<std::uintptr_t>(PostResolveDepthRelocationId, OFFSET(0x279, 0x273))
					.address());
			stl::write_thunk_call<RenderBatches_GeometryGroup<11>>(
				REL::Relocation<std::uintptr_t>(PostResolveDepthRelocationId, OFFSET(0x299, 0x293))
					.address());
			stl::write_thunk_call<RenderPersistentPassList<12>>(
				REL::Relocation<std::uintptr_t>(PostResolveDepthRelocationId, OFFSET(0x2D6, 0x2D0))
					.address());
			stl::write_thunk_call<RenderBatches_GeometryGroup<12>>(
				REL::Relocation<std::uintptr_t>(PostResolveDepthRelocationId, OFFSET(0x2F6, 0x2F0))
					.address());
			stl::write_thunk_call<RenderPersistentPassList<7>>(
				REL::Relocation<std::uintptr_t>(PostResolveDepthRelocationId, OFFSET(0x394, 0x389))
					.address());
			stl::write_thunk_call<RenderBatches_GeometryGroup<7>>(
				REL::Relocation<std::uintptr_t>(PostResolveDepthRelocationId, OFFSET(0x3B4, 0x3A9))
					.address());
			stl::write_thunk_call<RenderEffects<false>>(
				REL::Relocation<std::uintptr_t>(PostResolveDepthRelocationId, OFFSET(0x551, 0x543))
					.address());
			stl::write_thunk_call<RenderBatches_Objects<RenderBatchesPass::SunGlare>>(
				REL::Relocation<std::uintptr_t>(PostResolveDepthRelocationId, OFFSET(0x616, 0x605))
					.address());

			const auto RenderDecalsRelocationId = RELOCATION_ID(99941, 106586);
			stl::write_thunk_call<RenderPersistentPassList<3>>(
				REL::Relocation<std::uintptr_t>(RenderDecalsRelocationId, OFFSET(0x9D, 0x93))
					.address());
			stl::write_thunk_call<RenderBatches_GeometryGroup<3>>(
				REL::Relocation<std::uintptr_t>(RenderDecalsRelocationId, OFFSET(0xBD, 0xB3))
					.address());
			stl::write_thunk_call<RenderPersistentPassList<2>>(
				REL::Relocation<std::uintptr_t>(RenderDecalsRelocationId, OFFSET(0x119, 0x10F))
					.address());
			stl::write_thunk_call<RenderBatches_GeometryGroup<2>>(
				REL::Relocation<std::uintptr_t>(RenderDecalsRelocationId, OFFSET(0x139, 0x12F))
					.address());

			
			const auto Render4RelocationId = RELOCATION_ID(99942, 106587);
			stl::write_thunk_call<RenderPersistentPassList<4>>(
				REL::Relocation<std::uintptr_t>(Render4RelocationId, OFFSET(0xF4, 0xF5))
					.address());
			stl::write_thunk_call<RenderBatches_GeometryGroup<4>>(
				REL::Relocation<std::uintptr_t>(Render4RelocationId, OFFSET(0x111, 0x112))
					.address());
		}

		//AnnotatedRenderer<29>::Init();

		auto renderer = RE::BSGraphics::Renderer::GetSingleton();

		for (size_t renderTargetIndex = 0;
			 renderTargetIndex < RE::RENDER_TARGETS::kTOTAL; ++renderTargetIndex)
		{
			const auto renderTargetName = magic_enum::enum_name(
				static_cast<RE::RENDER_TARGETS::RENDER_TARGET>(renderTargetIndex));
			if (auto texture = renderer->data.renderTargets[renderTargetIndex].texture)
			{
				texture->SetPrivateData(WKPDID_D3DDebugObjectName,
					static_cast<UINT>(renderTargetName.size()), renderTargetName.data());
			}
		}

		for (size_t renderTargetIndex = 0;
			 renderTargetIndex < RE::RENDER_TARGETS_CUBEMAP::kTOTAL;
			 ++renderTargetIndex)
		{
			const auto renderTargetName = magic_enum::enum_name(
				static_cast<RE::RENDER_TARGETS_CUBEMAP::RENDER_TARGET_CUBEMAP>(renderTargetIndex));
			if (auto texture = renderer->data.cubemapRenderTargets[renderTargetIndex].texture)
			{
				texture->SetPrivateData(WKPDID_D3DDebugObjectName,
					static_cast<UINT>(renderTargetName.size()), renderTargetName.data());
			}
		}

		/*for (size_t renderTargetIndex = 0; renderTargetIndex < RE::RENDER_TARGETS_3D::kTOTAL;
			 ++renderTargetIndex)
		{
			const auto renderTargetName = magic_enum::enum_name(
				static_cast<RE::RENDER_TARGETS_3D::RENDER_TARGET_3D>(renderTargetIndex));
			renderer
				->data.texture3DRenderTargets[renderTargetIndex]
				.texture->SetPrivateData(WKPDID_D3DDebugObjectName,
					static_cast<UINT>(renderTargetName.size()), renderTargetName.data());
		}*/

		for (size_t renderTargetIndex = 0;
			 renderTargetIndex < RE::RENDER_TARGETS_DEPTHSTENCIL::kTOTAL;
			 ++renderTargetIndex)
		{
			const auto renderTargetName = magic_enum::enum_name(
				static_cast<RE::RENDER_TARGETS_DEPTHSTENCIL::RENDER_TARGET_DEPTHSTENCIL>(
					renderTargetIndex));
			if (auto texture = renderer->data.depthStencils[renderTargetIndex].texture)
			{
				texture->SetPrivateData(WKPDID_D3DDebugObjectName,
					static_cast<UINT>(renderTargetName.size()), renderTargetName.data());
			}
		}
	}
}

struct BSShader_BeginTechnique
{
	static bool thunk(RE::BSShader* shader, int vertexDescriptor, int pixelDescriptor,
		bool skipPixelShader)
	{
		static REL::Relocation<void(void*, RE::BSGraphics::VertexShader*)> SetVertexShader(
			RELOCATION_ID(75550, 77351));
		static REL::Relocation<void(void*, RE::BSGraphics::PixelShader*)> SetPixelShader(
			RELOCATION_ID(75555, 77356));

		auto& shaderCache = SIE::ShaderCache::Instance();
		RE::BSGraphics::VertexShader* vertexShader = nullptr;
		if (shaderCache.IsEnabledForClass(SIE::ShaderClass::Vertex))
		{
			vertexShader = shaderCache.GetVertexShader(*shader, vertexDescriptor);
		}
		if (vertexShader == nullptr)
		{
			for (auto item : shader->vertexShaders)
			{
				if (item->id == vertexDescriptor)
				{
					vertexShader = item;
					break;
				}
			}
		}
		else
		{
			for (auto item : shader->vertexShaders)
			{
				if (item->id == vertexDescriptor)
				{
					if (vertexShader->shaderDesc != item->shaderDesc)
					{
						logger::info("{} {}", vertexShader->shaderDesc, item->shaderDesc);
					}
					break;
				}
			}
		}
		RE::BSGraphics::PixelShader* pixelShader = nullptr;
		if (shaderCache.IsEnabledForClass(SIE::ShaderClass::Pixel))
		{
			pixelShader = shaderCache.GetPixelShader(*shader, pixelDescriptor);
		}
		if (pixelShader == nullptr)
		{
			for (auto item : shader->pixelShaders)
			{
				if (item->id == pixelDescriptor)
				{
					pixelShader = item;
					break;
				}
			}
		}
		if (vertexShader == nullptr || pixelShader == nullptr)
		{
			return false;
		}
		SetVertexShader(nullptr, vertexShader);
		if (skipPixelShader)
		{
			pixelShader = nullptr;
		}
		SetPixelShader(nullptr, pixelShader);
		return true;
	}

	static inline REL::Relocation<decltype(thunk)> func;
};

struct NiSkinInstance_RegisterStreamables
{
	static bool thunk(RE::NiSkinInstance* skin, RE::NiStream* stream) 
	{
		const bool result = stream->RegisterSaveObject(skin);
		if (result)
		{
			skin->skinData->RegisterStreamables(*stream);
			if (skin->skinPartition != nullptr)
			{
				skin->skinPartition->RegisterStreamables(*stream);
			}
			skin->rootParent->RegisterStreamables(*stream);
			if (skin->skinData->bones > 0)
			{
				logger::info("{}", skin->skinData->bones);
				for (uint32_t boneIndex = 0; boneIndex < skin->skinData->bones; ++boneIndex)
				{
					logger::info("{}", reinterpret_cast<void*>(skin->bones[boneIndex]));
					if (auto bone = skin->bones[boneIndex])
					{
						skin->bones[boneIndex]->RegisterStreamables(*stream);
					}
				}
				return true;
			}
		}
		return result;
	}

	static inline REL::Relocation<decltype(thunk)> func;
	static constexpr size_t idx = 0x1A;
};

struct bhkSerializable_SaveBinary
{
	static void thunk(RE::bhkSerializable* serializable, RE::NiStream* stream) 
	{
		if (auto rigidBody = serializable->AsBhkRigidBody())
		{
			std::array<uint8_t, 12> skipData;
			std::fill_n(skipData.begin(), skipData.size(), 0);

			bool isLoaded = false;
			auto data = static_cast<RE::bhkRigidBody::SerializedData*>(rigidBody->Unk_2F(isLoaded));

			stream->oStr->write(&data->collisionFilterInfo, 1);
			const auto shapeKey1 = reinterpret_cast<int32_t>(data->shape);
			stream->oStr->write(&shapeKey1, 1);
			stream->oStr->write(&data->broadphaseHandleType, 1);
			stream->oStr->write(skipData.data(), 3);
			stream->oStr->write(skipData.data(), 12);
			stream->oStr->write(&data->unk28, 1);
			stream->oStr->write(&data->unk2A, 1);
			stream->oStr->write(skipData.data(), 1);
			stream->oStr->write(skipData.data(), 4);
			stream->oStr->write(&data->unk30.collisionFilterInfo, 1);
			const auto shapeKey2 = reinterpret_cast<int32_t>(data->unk30.shape);
			stream->oStr->write(&shapeKey2, 1);
			const auto localFrameKey = reinterpret_cast<int32_t>(data->unk30.localFrame);
			stream->oStr->write(&localFrameKey, 1);
			stream->oStr->write(&data->unk30.responseType, 1);
			stream->oStr->write(&data->unk30.contactPointCallbackDelay, 1);
			stream->oStr->write(skipData.data(), 1);
			stream->oStr->write(&data->unk30.translation, 1);
			stream->oStr->write(&data->unk30.rotation, 1);
			stream->oStr->write(&data->unk30.linearVelocity, 1);
			stream->oStr->write(&data->unk30.angularVelocity, 1);
			stream->oStr->write(&data->unk30.inertiaLocal, 1);
			stream->oStr->write(&data->unk30.centerOfMassLocal, 1);
			stream->oStr->write(&data->unk30.mass, 1);
			stream->oStr->write(&data->unk30.linearDamping, 1);
			stream->oStr->write(&data->unk30.angularDamping, 1);
			stream->oStr->write(&data->unk30.timeFactor, 1);
			stream->oStr->write(&data->unk30.gravityFactor, 1);
			stream->oStr->write(&data->unk30.friction, 1);
			stream->oStr->write(&data->unk30.rollingFrictionMultiplier, 1);
			stream->oStr->write(&data->unk30.restitution, 1);
			stream->oStr->write(&data->unk30.maxLinearVelocity, 1);
			stream->oStr->write(&data->unk30.maxAngularVelocity, 1);
			stream->oStr->write(&data->unk30.allowedPenetrationDepth, 1);
			stream->oStr->write(&data->unk30.motionType, 1);
			stream->oStr->write(&data->unk30.isDeactivationIntegrateCounterValid, 1);
			stream->oStr->write(&data->unk30.deactivationClass, 1);
			stream->oStr->write(&data->unk30.objectQualityType, 1);
			stream->oStr->write(&data->unk30.autoRemoveLevel, 1);
			stream->oStr->write(&data->unk30.unkD1, 1);
			stream->oStr->write(&data->unk30.numShapeKeysInContactPointProperties, 1);
			stream->oStr->write(&data->unk30.isForceCollideOntoPpu, 1);
			stream->oStr->write(skipData.data(), 12);

			rigidBody->Unk_2B(isLoaded);
		}
		else if (serializable->GetRTTI() ==
				 &*REL::Relocation<RE::NiRTTI*>(RE::NiRTTI_bhkCompressedMeshShape))
		{
			std::array<std::uint8_t, 32> skipData;
			std::fill_n(skipData.begin(), skipData.size(), 0);
			stream->oStr->write(skipData.data(), 32);
		}
		else if (serializable->GetRTTI() ==
				 &*REL::Relocation<RE::NiRTTI*>(RE::NiRTTI_bhkMoppBvTreeShape))
		{
			bool isLoaded = false;
			auto data = reinterpret_cast<int32_t*>(serializable->Unk_2F(isLoaded));

			stream->oStr->write(&data[0], 1);
			stream->oStr->write(&data[2], 1);
			stream->oStr->write(&data[4], 1);
			stream->oStr->write(&data[6], 1);

			serializable->Unk_2B(isLoaded);
		}
		else if (serializable->GetRTTI() ==
					 &*REL::Relocation<RE::NiRTTI*>(RE::NiRTTI_bhkConvexVerticesShape) ||
				 serializable->GetRTTI() ==
					 &*REL::Relocation<RE::NiRTTI*>(RE::NiRTTI_bhkCharControllerShape))
		{
			std::array<std::uint8_t, 24> skipData;

			bool isLoaded = false;
			auto data = reinterpret_cast<int32_t*>(serializable->Unk_2F(isLoaded));

			stream->oStr->write(&data[0], 1);
			stream->oStr->write(&data[1], 1);
			stream->oStr->write(skipData.data(), 24);

			serializable->Unk_2B(isLoaded);
		}
		else if (serializable->GetRTTI() ==
				 &*REL::Relocation<RE::NiRTTI*>(RE::NiRTTI_bhkListShape))
		{
			bool isLoaded = false;
			auto data = reinterpret_cast<int32_t*>(serializable->Unk_2F(isLoaded));

			std::array<std::uint8_t, 24> skipData;
			stream->oStr->write(&data[0], 1);
			stream->oStr->write(skipData.data(), 24);

			serializable->Unk_2B(isLoaded);
		}
		else if (serializable->GetRTTI() == &*REL::Relocation<RE::NiRTTI*>(RE::NiRTTI_bhkNiTriStripsShape))
		{
			bool isLoaded = false;
			auto data = reinterpret_cast<int32_t*>(serializable->Unk_2F(isLoaded));

			std::array<std::uint8_t, 24> skipData;
			stream->oStr->write(&data[0], 1);
			stream->oStr->write(&data[1], 1);
			stream->oStr->write(skipData.data(), 24);
			stream->oStr->write(&data[12], 1);

			serializable->Unk_2B(isLoaded);
		}
		else
		{
			func(serializable, stream);
		}
	}
	static inline REL::Relocation<decltype(thunk)> func;
	static constexpr size_t idx = 0x1B;
};

struct TESWaterForm_Load
{
	static bool thunk(RE::TESWaterForm* form, RE::TESFile* a_mod)
	{
		const auto result = func(form, a_mod);

		form->noiseTextures[0].textureName = {};
		form->noiseTextures[1].textureName = {};
		form->noiseTextures[2].textureName = {};
		form->noiseTextures[3].textureName = {};

		form->data.reflectionAmount = 0.f;
		form->data.sunSpecularPower = 1000000.f;
		form->data.sunSpecularMagnitude = 0.f;

		return result;
	}
	static inline REL::Relocation<decltype(thunk)> func;
	static constexpr size_t idx = 0x06;
};

struct TESLandTexture_Load
{
	static bool thunk(RE::TESLandTexture* form, RE::TESFile* a_mod)
	{
		const auto result = func(form, a_mod);

		form->specularExponent = 0;

		return result;
	}
	static inline REL::Relocation<decltype(thunk)> func;
	static constexpr size_t idx = 0x06;
};

struct BSXFlags_Load
{
	static void thunk(RE::BSXFlags* flags, RE::NiStream& a_stream)
	{
		func(flags, a_stream);

		flags->value &= ~static_cast<int32_t>(RE::BSXFlags::Flag::kHavok);
	}
	static inline REL::Relocation<decltype(thunk)> func;
	static constexpr size_t idx = 0x18;
};

struct NiAlphaProperty_Load
{
	static void thunk(RE::NiAlphaProperty* property, RE::NiStream& a_stream)
	{
		func(property, a_stream);

		if (property->alphaThreshold > 64)
		{
			property->alphaThreshold = 64;
		}
	}
	static inline REL::Relocation<decltype(thunk)> func;
	static constexpr size_t idx = 0x18;
};

struct TESNPC_Load
{
	static bool thunk(RE::TESNPC* form, RE::TESFile* a_mod)
	{
		const auto result = func(form, a_mod);

		form->actorData.actorBaseFlags.reset(RE::ACTOR_BASE_DATA::Flag::kIsGhost);

		return result;
	}
	static inline REL::Relocation<decltype(thunk)> func;
	static constexpr size_t idx = 0x06;
};

struct Actor_InitItemImpl
{
	static void thunk(RE::Actor* form)
	{
		func(form);

		//if (form != RE::PlayerCharacter::GetSingleton())
		{
			form->Disable();
		}
	}
	static inline REL::Relocation<decltype(thunk)> func;
	static constexpr size_t idx = 0x13;
};

struct PlayerCharacter_MoveToQueuedLocation
{
	static void thunk(RE::PlayerCharacter* player)
	{
		player->queuedTargetLoc.location.z = 50000.f;

		func(player);
	}
	static inline REL::Relocation<decltype(thunk)> func;
};

struct WinMain_OnQuitGame
{
	static void thunk()
	{
		SIE::Serializer::Instance().OnQuitGame();

		func();
	}
	static inline REL::Relocation<decltype(thunk)> func;
};

struct BSLightingShader
{
	char _pad0[0x94];
	uint32_t m_CurrentRawTechnique;
	char _pad1[96];
};

//struct BSLightingShader_SetupMaterial
//{
//	static void thunk(BSLightingShader* shader, RE::BSShaderMaterial* material)
//	{
//		if (shader->m_CurrentRawTechnique == )
//		static const REL::Relocation<bool*> LodBlendingEnabled(REL::ID(390936));
//		*LodBlendingEnabled = !(shader->m_CurrentRawTechnique & 0x200000);
//
//		func(shader, material);
//	}
//	static inline REL::Relocation<decltype(thunk)> func;
//	static constexpr size_t idx = 0x04;
//};

struct BSShaderProperty__SetFlag__InitLand
{
	static void thunk(RE::BSShaderProperty* property, uint8_t flag, bool value)
	{
		bool actualValue = false;
		if (auto material = static_cast<RE::BSLightingShaderMaterialLandscape*>(property->material))
		{
			for (size_t index = 0; index < 6; ++index)
			{
				if (material->textureIsSnow[index])
				{
					actualValue = true;
					break;
				}
			}
		}

		func(property, flag, actualValue);
	}
	static inline REL::Relocation<decltype(thunk)> func;
};

namespace BehaviorGraph
{
	std::mutex mutex;

	std::unordered_map<uint32_t, std::string> EditorIDs;

	struct WaterFlows
	{
		static bool thunk(const char* path)
		{
			logger::info("{}", path);
			return func(path);
		}
		static inline REL::Relocation<decltype(thunk)> func;
	};

	struct TESForm_GetFormEditorID
	{
		static const char* thunk(const RE::TESWeather* form)
		{
			auto it = EditorIDs.find(form->GetFormID());
			if (it == EditorIDs.cend())
			{
				return "";
			}
			return it->second.c_str();
		}
		static inline REL::Relocation<decltype(thunk)> func;
		static constexpr size_t idx = 0x32;
	};

	struct TESForm_SetFormEditorID
	{
		static bool thunk(RE::TESWeather* form, const char* editorId)
		{
			if (IsBadStringPtrA(editorId, 256))
			{
				return true;
			}
			EditorIDs[form->GetFormID()] = editorId;
			return true;
		}
		static inline REL::Relocation<decltype(thunk)> func;
		static constexpr size_t idx = 0x33;
	};

	struct TESWaterObject_dtor
	{
		static void thunk(RE::TESWaterObject* waterObject, bool a2) 
		{ 
			func(waterObject, a2);
		}
		static inline REL::Relocation<decltype(thunk)> func;
		static constexpr size_t idx = 0x0;
	};

	struct TestHook
	{
		static void thunk(void* a1, RE::BSWaterShaderMaterial* a2) 
		{
			logger::info("{} {} {} {}", a2->specularPower, a2->specularRadius,
				a2->specularBrightness, a2->uvScaleA[0]);
			//logger::info("{}", std::to_string(std::stacktrace::current()));
			func(a1, a2);
		}
		static inline REL::Relocation<decltype(thunk)> func;
		static constexpr size_t idx = 0x04;
	};

	template<template<typename, typename> typename T, typename Val>
	struct BSAnimationGraphChannel_PollChannelUpdateImpl
	{
		static void thunk(RE::BSTAnimationGraphDataChannel<RE::Actor, Val, T>* channel, bool arg)
		{
			if (enable && actor == channel->type) 
			{
				std::lock_guard lock(mutex);
				logger::info(std::format("Value {} was set into {} channel", *reinterpret_cast<Val*>(&channel->value), channel->channelName.c_str()));
			}

			func(channel, arg);
		}
		static inline REL::Relocation<decltype(thunk)> func;
		static constexpr size_t idx = 0x1;

		static inline RE::Actor* actor = nullptr;
		static inline bool enable = true;
	};

	struct BShkbAnimationGraph_SetGraphVariableFloat
	{
		static bool thunk(RE::BShkbAnimationGraph* graph, const RE::BSFixedString& variableName, float value)
		{
			bool success = false;

			const auto behaviorGraph = graph->behaviorGraph;
			if (behaviorGraph != nullptr) {
				const auto& variables = behaviorGraph->variableValueSet;
				if (variables != nullptr) {
					const auto dbData = graph->projectDBData;
					const auto it = dbData->variables.find(variableName);
					if (it != dbData->variables.end() && it->second >= 0 && it->second < variables->m_wordVariableValues.size()) {
						variables->m_wordVariableValues[it->second].f = value;
						success = true;
					}
				}
			}

			if (enableNonEveryFrame && refr == graph->holder) 
			{
				std::lock_guard lock(mutex);
				if (success) {
					logger::info("Float variable {} was set to {}", variableName.c_str(), value);
				} else {
					logger::info("Failed to set float variable {} to {}", variableName.c_str(), value);
				}
			}

			return success;
		}
		static inline REL::Relocation<decltype(thunk)> func;

		static inline RE::TESObjectREFR* refr = nullptr;
		static inline bool enableNonEveryFrame = true;
	};

	struct BShkbAnimationGraph_GetGraphVariableFloat
	{
		static bool thunk(RE::BShkbAnimationGraph* graph, const RE::BSFixedString& variableName, float& value)
		{
			bool success = false;

			const auto behaviorGraph = graph->behaviorGraph;
			if (behaviorGraph != nullptr) 
			{
				const auto& variables = behaviorGraph->variableValueSet;
				if (variables != nullptr) 
				{
					const auto dbData = graph->projectDBData;
					const auto it = dbData->variables.find(variableName);
					if (it != dbData->variables.end() && it->second >= 0 && it->second < variables->m_wordVariableValues.size()) 
					{
						value = variables->m_wordVariableValues[it->second].f;
						success = true;
					}
				}
			}

			if (((enableSuccessful && success) || (enableFailed && !success)) && refr == graph->holder) 
			{
				std::lock_guard lock(mutex);
				if (success) 
				{
					logger::info("Got value {} of float variable {}", value, variableName.c_str());
				}
			    else 
				{
					logger::info("Failed to get value of float variable {}", variableName.c_str());
				}
			}

			return success;
		}
		static inline REL::Relocation<decltype(thunk)> func;

		static inline RE::TESObjectREFR* refr = nullptr;
		static inline bool enableSuccessful = true;
		static inline bool enableFailed = true;
	};

	struct BShkbAnimationGraph_SetGraphVariableInt
	{
		static bool thunk(RE::BShkbAnimationGraph* graph, const RE::BSFixedString& variableName, int value)
		{
			bool success = false;

			const auto behaviorGraph = graph->behaviorGraph;
			if (behaviorGraph != nullptr) {
				const auto& variables = behaviorGraph->variableValueSet;
				if (variables != nullptr) {
					const auto dbData = graph->projectDBData;
					const auto it = dbData->variables.find(variableName);
					if (it != dbData->variables.end() && it->second >= 0 && it->second < variables->m_wordVariableValues.size()) {
						variables->m_wordVariableValues[it->second].i = value;
						success = true;
					}
				}
			}

			if (enableNonEveryFrame && refr == graph->holder) {
				std::lock_guard lock(mutex);
				if (success) {
					logger::info("Int variable {} was set to {}", variableName.c_str(), value);
				} else {
					logger::info("Failed to set int variable {} to {}", variableName.c_str(), value);
				}
			}

			return success;
		}
		static inline REL::Relocation<decltype(thunk)> func;

		static inline RE::TESObjectREFR* refr = nullptr;
		static inline bool enableNonEveryFrame = true;
	};

	struct BShkbAnimationGraph_GetGraphVariableInt
	{
		static bool thunk(RE::BShkbAnimationGraph* graph, const RE::BSFixedString& variableName, int& value)
		{
			bool success = false;

			const auto behaviorGraph = graph->behaviorGraph;
			if (behaviorGraph != nullptr) {
				const auto& variables = behaviorGraph->variableValueSet;
				if (variables != nullptr) {
					const auto dbData = graph->projectDBData;
					const auto it = dbData->variables.find(variableName);
					if (it != dbData->variables.end() && it->second >= 0 && it->second < variables->m_wordVariableValues.size()) {
						value = variables->m_wordVariableValues[it->second].i;
						success = true;
					}
				}
			}

			if (((enableSuccessful && success) || (enableFailed && !success)) && refr == graph->holder)
			{
				std::lock_guard lock(mutex);
				if (success) {
					logger::info("Got value {} of int variable {}", value, variableName.c_str());
				} else {
					logger::info("Failed to get value of int variable {}", variableName.c_str());
				}
			}

			return success;
		}
		static inline REL::Relocation<decltype(thunk)> func;

		static inline RE::TESObjectREFR* refr = nullptr;
		static inline bool enableSuccessful = true;
		static inline bool enableFailed = true;
	};

	struct BShkbAnimationGraph_SetGraphVariableBool
	{
		static bool thunk(RE::BShkbAnimationGraph* graph, const RE::BSFixedString& variableName, bool value)
		{
			bool success = false;

			const auto behaviorGraph = graph->behaviorGraph;
			if (behaviorGraph != nullptr) {
				const auto& variables = behaviorGraph->variableValueSet;
				if (variables != nullptr) {
					const auto dbData = graph->projectDBData;
					const auto it = dbData->variables.find(variableName);
					if (it != dbData->variables.end() && it->second >= 0 && it->second < variables->m_wordVariableValues.size()) {
						variables->m_wordVariableValues[it->second].b = value;
						success = true;
					}
				}
			}

			if (enableNonEveryFrame && refr == graph->holder) {
				std::lock_guard lock(mutex);
				if (success) {
					logger::info("Bool variable {} was set to {}", variableName.c_str(), value);
				} else {
					logger::info("Failed to set bool variable {} to {}", variableName.c_str(), value);
				}
			}

			return success;
		}
		static inline REL::Relocation<decltype(thunk)> func;

		static inline RE::TESObjectREFR* refr = nullptr;
		static inline bool enableNonEveryFrame = true;
	};

	struct BShkbAnimationGraph_GetGraphVariableBool
	{
		static inline constexpr std::array EveryFrameFilter{
			"bIsSynced"sv, "bSpeedSynced"sv, "bDisableInterp"sv, "bHumanoidFootIKDisable"sv
		};

		static bool thunk(RE::BShkbAnimationGraph* graph, const RE::BSFixedString& variableName, bool& value)
		{
			bool success = false;

			const auto behaviorGraph = graph->behaviorGraph;
			if (behaviorGraph != nullptr) {
				const auto& variables = behaviorGraph->variableValueSet;
				if (variables != nullptr) {
					const auto dbData = graph->projectDBData;
					const auto it = dbData->variables.find(variableName);
					if (it != dbData->variables.end() && it->second >= 0 && it->second < variables->m_wordVariableValues.size()) {
						value = variables->m_wordVariableValues[it->second].b;
						success = true;
					}
				}
			}

			if (((enableSuccessful && success) || (enableFailed && !success)) && refr == graph->holder) {
				std::lock_guard lock(mutex);
				if (success) {
					logger::info("Got value {} of bool variable {}", value, variableName.c_str());
				} else {
					logger::info("Failed to get value of bool variable {}", variableName.c_str());
				}
			}

			return success;
		}
		static inline REL::Relocation<decltype(thunk)> func;

		static inline RE::TESObjectREFR* refr = nullptr;
		static inline bool enableSuccessful = true;
		static inline bool enableFailed = true;
	};

	struct BShkbAnimationGraph_SetGraphVariableIntRef
	{
		static bool thunk(RE::BShkbAnimationGraph* graph, const RE::BSFixedString& variableName, const int& value)
		{
			bool success = false;

			const auto behaviorGraph = graph->behaviorGraph;
			if (behaviorGraph != nullptr) {
				const auto& variables = behaviorGraph->variableValueSet;
				if (variables != nullptr) {
					const auto dbData = graph->projectDBData;
					const auto it = dbData->variables.find(variableName);
					if (it != dbData->variables.end() && it->second >= 0 && it->second < variables->m_wordVariableValues.size()) {
						variables->m_wordVariableValues[it->second].i = value;
						success = true;
					}
				}
			}

			if (enableNonEveryFrame && refr == graph->holder) {
				std::lock_guard lock(mutex);
				if (success) {
					logger::info("Int variable {} was set to {}", variableName.c_str(), value);
				} else {
					logger::info("Failed to set int variable {} to {}", variableName.c_str(), value);
				}
			}

			return success;
		}
		static inline REL::Relocation<decltype(thunk)> func;

		static inline RE::TESObjectREFR* refr = nullptr;
		static inline bool enableNonEveryFrame = true;
	};

	struct BShkbAnimationGraph_SetGraphVariableVector
	{
		static bool thunk(RE::BShkbAnimationGraph* graph, const RE::BSFixedString& variableName, const float* value)
		{
			static constexpr auto TargetLocationVar = "TargetLocation"sv;

			bool success = false;

			const auto behaviorGraph = graph->behaviorGraph;
			if (behaviorGraph != nullptr) {
				const auto& variables = behaviorGraph->variableValueSet;
				if (variables != nullptr) {
					const auto dbData = graph->projectDBData;
					const auto it = dbData->variables.find(variableName);
					if (it != dbData->variables.end() && it->second >= 0 && it->second < variables->m_wordVariableValues.size()) {
						const auto quadIndex = variables->m_wordVariableValues[it->second].i;
						if (quadIndex >= 0 && quadIndex < variables->m_quadVariableValues.size()) {
							variables->m_quadVariableValues[quadIndex] = RE::hkVector4(value[0], value[1], value[2], value[3]);
							success = true;
						}
					}
				}
			}

			if (((enableNonEveryFrame && variableName != TargetLocationVar) || (enableEveryFrame && variableName == TargetLocationVar)) && refr == graph->holder) {
				std::lock_guard lock(mutex);
				if (success) {
					logger::info("Vector variable {} was set to [{}, {}, {}, {}]", variableName.c_str(), value[0], value[1], value[2], value[3]);
				} else {
					logger::info("Failed to set vector variable {} was set to [{}, {}, {}, {}]", variableName.c_str(), value[0], value[1], value[2], value[3]);
				}
			}

			return success;
		}
		static inline REL::Relocation<decltype(thunk)> func;

		static inline RE::TESObjectREFR* refr = nullptr;
		static inline bool enableNonEveryFrame = true;
		static inline bool enableEveryFrame = true;
	};

	struct BShkbAnimationGraph_GetGraphVariableVector
	{
		static bool thunk(RE::BShkbAnimationGraph* graph, const RE::BSFixedString& variableName, __m128& value)
		{
			bool success = false;

		    const auto behaviorGraph = graph->behaviorGraph;
			if (behaviorGraph != nullptr) {
				const auto& variables = behaviorGraph->variableValueSet;
				if (variables != nullptr) {
					const auto dbData = graph->projectDBData;
					const auto it = dbData->variables.find(variableName);
					if (it != dbData->variables.end()) 
					{
						const auto quadIndex = variables->m_wordVariableValues[it->second].i;
						if (quadIndex >= 0 && quadIndex < variables->m_quadVariableValues.size()) {
							const auto& result = variables->m_quadVariableValues[quadIndex];
							value = result.quad;
							success = true;
						}
					}
				}
			}

			if (((enableSuccessful && success) || (enableFailed && !success)) && refr == graph->holder) 
			{
				std::lock_guard lock(mutex);
				if (success) {
					logger::info("Got value [{}, {}, {}, {}] of vector variable", value.m128_f32[0], value.m128_f32[1], value.m128_f32[2], value.m128_f32[3], variableName.c_str());
				} else {
					logger::info("Failed to get value of vector variable {}", variableName.c_str());
				}
			}

			return success;
		}
		static inline REL::Relocation<decltype(thunk)> func;

		static inline RE::TESObjectREFR* refr = nullptr;
		static inline bool enableSuccessful = true;
		static inline bool enableFailed = true;
	};

	struct Test
	{
		static void thunk(void* mediator, RE::BGSActionData* action)
		{
			func(mediator, action);

			if (action->animEvent == "HorseExit") 
			{
				RE::BSTSmartPointer<RE::BSAnimationGraphManager> manager;
				action->target->GetAnimationGraphManager(manager);
				auto dbData = manager->graphs[0]->projectDBData;

				for (const auto& name : dbData->synchronizedClipGenerators) 
				{
					//logger::info(name.data());
				}

				logger::info("{} {} {} {} {}", action->action->formEditorID.data(),
					action->source ? action->source->GetDisplayFullName() : "null",
					action->target ? action->target->GetDisplayFullName() : "null",
					action->animEvent.data(), action->targetAnimEvent.data());
			}
		}
		static inline REL::Relocation<decltype(thunk)> func;
	};
}

struct MissingMeshCrashFix
{
	static void thunk(void* a1, RE::TESObjectREFR* a2)
	{
		if (a2 != nullptr)
		{
			if (auto model = a2->Get3D2())
			{
				if (auto fadeNode = model->AsFadeNode())
				{
					fadeNode->currentDistance = 0;
				}
			}
		}
	}
	static inline REL::Relocation<decltype(thunk)> func;
};

namespace Hooks
{
	void OnPostLoad()
	{
		//BehaviorGraph::Install();
	}

	void OnDataLoaded() 
	{ 
		FrameAnnotations::InstallThunks();
	}

	void OnPostPostLoad()
	{
		{
			const std::array targets{
				REL::Relocation<std::uintptr_t>(RELOCATION_ID(75964, 77790), 0xE8),
				REL::Relocation<std::uintptr_t>(RELOCATION_ID(76793, 78669), 0x45),
			};
			for (const auto& target : targets)
			{
				stl::write_thunk_call<bhkSerializable_SaveBinary>(target.address());
			}
		}
		stl::write_vfunc<NiSkinInstance_RegisterStreamables>(RE::VTABLE_NiSkinInstance[0]);
		stl::write_vfunc<NiSkinInstance_RegisterStreamables>(RE::VTABLE_BSDismemberSkinInstance[0]);

		{
			const std::array targets{
				REL::Relocation<std::uintptr_t>(RELOCATION_ID(101341, 108328), 0),
			};
			for (const auto& target : targets)
			{
				stl::write_thunk_jmp<BSShader_BeginTechnique>(target.address());
			}
		}

		{
			stl::write_vfunc<BehaviorGraph::TESForm_GetFormEditorID>(RE::VTABLE_TESObjectREFR[0]);
			stl::write_vfunc<BehaviorGraph::TESForm_SetFormEditorID>(RE::VTABLE_TESObjectREFR[0]);

			stl::write_vfunc<BehaviorGraph::TESForm_GetFormEditorID>(RE::VTABLE_Character[0]);
			stl::write_vfunc<BehaviorGraph::TESForm_SetFormEditorID>(RE::VTABLE_Character[0]);

			stl::write_vfunc<BehaviorGraph::TESForm_GetFormEditorID>(RE::VTABLE_TESNPC[0]);
			stl::write_vfunc<BehaviorGraph::TESForm_SetFormEditorID>(RE::VTABLE_TESNPC[0]);

			stl::write_vfunc<BehaviorGraph::TESForm_GetFormEditorID>(RE::VTABLE_TESObjectSTAT[0]);
			stl::write_vfunc<BehaviorGraph::TESForm_SetFormEditorID>(RE::VTABLE_TESObjectSTAT[0]);

			stl::write_vfunc<BehaviorGraph::TESForm_GetFormEditorID>(RE::VTABLE_TESContainer[0]);
			stl::write_vfunc<BehaviorGraph::TESForm_SetFormEditorID>(RE::VTABLE_TESContainer[0]);

			stl::write_vfunc<BehaviorGraph::TESForm_GetFormEditorID>(RE::VTABLE_TESObjectMISC[0]);
			stl::write_vfunc<BehaviorGraph::TESForm_SetFormEditorID>(RE::VTABLE_TESObjectMISC[0]);

			stl::write_vfunc<BehaviorGraph::TESForm_GetFormEditorID>(RE::VTABLE_TESObjectACTI[0]);
			stl::write_vfunc<BehaviorGraph::TESForm_SetFormEditorID>(RE::VTABLE_TESObjectACTI[0]);

			stl::write_vfunc<BehaviorGraph::TESForm_GetFormEditorID>(RE::VTABLE_BGSMovableStatic[0]);
			stl::write_vfunc<BehaviorGraph::TESForm_SetFormEditorID>(
				RE::VTABLE_BGSMovableStatic[0]);

			stl::write_vfunc<BehaviorGraph::TESForm_GetFormEditorID>(RE::VTABLE_TESWeather[0]);
			stl::write_vfunc<BehaviorGraph::TESForm_SetFormEditorID>(RE::VTABLE_TESWeather[0]);

			stl::write_vfunc<BehaviorGraph::TESForm_GetFormEditorID>(RE::VTABLE_TESImageSpace[0]);
			stl::write_vfunc<BehaviorGraph::TESForm_SetFormEditorID>(RE::VTABLE_TESImageSpace[0]);

			stl::write_vfunc<BehaviorGraph::TESForm_GetFormEditorID>(
				RE::VTABLE_BGSShaderParticleGeometryData[0]);
			stl::write_vfunc<BehaviorGraph::TESForm_SetFormEditorID>(
				RE::VTABLE_BGSShaderParticleGeometryData[0]);

			stl::write_vfunc<BehaviorGraph::TESForm_GetFormEditorID>(
				RE::VTABLE_BGSVolumetricLighting[0]);
			stl::write_vfunc<BehaviorGraph::TESForm_SetFormEditorID>(
				RE::VTABLE_BGSVolumetricLighting[0]);

			stl::write_vfunc<BehaviorGraph::TESForm_GetFormEditorID>(RE::VTABLE_BGSReferenceEffect[0]);
			stl::write_vfunc<BehaviorGraph::TESForm_SetFormEditorID>(
				RE::VTABLE_BGSReferenceEffect[0]);

			stl::write_vfunc<BehaviorGraph::TESForm_GetFormEditorID>(
				RE::VTABLE_BGSSoundDescriptorForm[0]);
			stl::write_vfunc<BehaviorGraph::TESForm_SetFormEditorID>(
				RE::VTABLE_BGSSoundDescriptorForm[0]);

			stl::write_vfunc<BehaviorGraph::TESForm_GetFormEditorID>(
				RE::VTABLE_TESWaterForm[0]);
			stl::write_vfunc<BehaviorGraph::TESForm_SetFormEditorID>(RE::VTABLE_TESWaterForm[0]);

			stl::write_vfunc<BehaviorGraph::TESForm_GetFormEditorID>(RE::VTABLE_BGSMaterialType[0]);
			stl::write_vfunc<BehaviorGraph::TESForm_SetFormEditorID>(RE::VTABLE_BGSMaterialType[0]);

			stl::write_vfunc<BehaviorGraph::TESForm_GetFormEditorID>(RE::VTABLE_SpellItem[0]);
			stl::write_vfunc<BehaviorGraph::TESForm_SetFormEditorID>(RE::VTABLE_SpellItem[0]);

			stl::write_vfunc<BehaviorGraph::TESForm_GetFormEditorID>(RE::VTABLE_BGSLightingTemplate[0]);
			stl::write_vfunc<BehaviorGraph::TESForm_SetFormEditorID>(
				RE::VTABLE_BGSLightingTemplate[0]);

			stl::write_vfunc<BehaviorGraph::TESForm_GetFormEditorID>(
				RE::VTABLE_TESRegion[0]);
			stl::write_vfunc<BehaviorGraph::TESForm_SetFormEditorID>(RE::VTABLE_TESRegion[0]);

			stl::write_vfunc<BehaviorGraph::TESForm_GetFormEditorID>(RE::VTABLE_TESPackage[0]);
			stl::write_vfunc<BehaviorGraph::TESForm_SetFormEditorID>(RE::VTABLE_TESPackage[0]);

			{
				const std::array targets{
					REL::Relocation<std::uintptr_t>(RE::Offset::WinMain, OFFSET(0x35, 0x1AE)),
				};
				for (const auto& target : targets)
				{
					stl::write_thunk_call<WinMain_OnQuitGame>(target.address());
				}
			}

			{
				const std::array targets{
					REL::Relocation<std::uintptr_t>(RELOCATION_ID(12880, 13023),
						OFFSET(0x1F6, 0x228)),
				};
				for (const auto& target : targets)
				{
					stl::write_thunk_call<MissingMeshCrashFix>(target.address());
				}
			}

#ifdef OVERHEAD_TOOL
			stl::write_vfunc<TESWaterForm_Load>(RE::VTABLE_TESWaterForm[0]);
			stl::write_vfunc<BSXFlags_Load>(RE::VTABLE_BSXFlags[0]);
			stl::write_vfunc<NiAlphaProperty_Load>(RE::VTABLE_NiAlphaProperty[0]);
			//stl::write_vfunc<Actor_InitItemImpl>(RE::VTABLE_Actor[0]);
			//stl::write_vfunc<Actor_InitItemImpl>(RE::VTABLE_Character[0]);
			//stl::write_vfunc<TESLandTexture_Load>(RE::VTABLE_TESLandTexture[0]);

			{
				const std::array targets{
					REL::Relocation<std::uintptr_t>(RELOCATION_ID(39365, 40437), 0x26A),
				};
				for (const auto& target : targets)
				{
					stl::write_thunk_call<PlayerCharacter_MoveToQueuedLocation>(target.address());
				}
			}

			{
				const std::array targets{
					REL::Relocation<std::uintptr_t>(RELOCATION_ID(18368, 18791), 0x2D8),
				};
				for (const auto& target : targets)
				{
					stl::write_thunk_call<BSShaderProperty__SetFlag__InitLand>(target.address());
				}
			}

			static const REL::Relocation<bool*> IsLodBlendingEnabled(RELOCATION_ID(513195, 390936));
			*IsLodBlendingEnabled = false;
			static const REL::Relocation<bool*> IsHDREnabled(RE::Offset::HDREnabledFlag);
			*IsHDREnabled = false;
			auto ism = RE::ImageSpaceManager::GetSingleton();
			ism->shaderInfo.blurCSShaderInfo->isEnabled = false;
#endif
		
//#ifdef SKYRIM_SUPPORT_AE
//			{
//				const auto renderPassCacheCtor = REL::ID(107500);
//				const int32_t passCount = 4194240;
//				const int32_t passSize = 4194240 * sizeof(RE::BSRenderPass);
//				const int32_t lightsCount = passCount * 16;
//				const int32_t lightsSize = lightsCount * sizeof(void*);
//				const int32_t lastPassIndex = passCount - 1;
//				const int32_t lastPassOffset =
//					(passCount - 1) * sizeof(RE::BSRenderPass);
//				const int32_t lastPassNextOffset =
//					(passCount - 1) * sizeof(RE::BSRenderPass) + offsetof(RE::BSRenderPass, next);
//				SIE::PatchMemory(
//					REL::Relocation<std::uintptr_t>(renderPassCacheCtor, 0x76).address(),
//					reinterpret_cast<const uint8_t*>(&lightsSize), 4);
//				SIE::PatchMemory(
//					REL::Relocation<std::uintptr_t>(renderPassCacheCtor, 0xAD).address(),
//					reinterpret_cast<const uint8_t*>(&passSize), 4);
//				SIE::PatchMemory(
//					REL::Relocation<std::uintptr_t>(renderPassCacheCtor, 0xCB).address(),
//					reinterpret_cast<const uint8_t*>(&lastPassIndex), 4);
//				SIE::PatchMemory(
//					REL::Relocation<std::uintptr_t>(renderPassCacheCtor, 0xF0).address(),
//					reinterpret_cast<const uint8_t*>(&lastPassNextOffset), 4);
//				SIE::PatchMemory(
//					REL::Relocation<std::uintptr_t>(renderPassCacheCtor, 0xFD).address(),
//					reinterpret_cast<const uint8_t*>(&lastPassOffset), 4);
//				SIE::PatchMemory(
//					REL::Relocation<std::uintptr_t>(renderPassCacheCtor, 0x191).address(),
//					reinterpret_cast<const uint8_t*>(&passCount), 4);
//			}
//#endif

			//stl::write_vfunc<BSLightingShader_SetupMaterial>(RE::VTABLE_BSLightingShader[0]);

			//stl::write_vfunc<BehaviorGraph::TESWaterObject_dtor>(RE::VTABLE_TESWaterObject[0]);

			//stl::write_vfunc<BehaviorGraph::TestHook>(RE::VTABLE_BSWaterShader[0]);

			//stl::write_vfunc<BehaviorGraph::TestHook>(RE::VTABLE_BSWaterShaderMaterial[0]);

			//{
			//	const std::array targets{
			//		/*REL::Relocation<std::uintptr_t>(RELOCATION_ID(31391, 32182),
			//			OFFSET(0x52, 0x4D)),*/
			//		REL::Relocation<std::uintptr_t>(RELOCATION_ID(31388, 32179),
			//			OFFSET(0xA3, 0x4D)),
			//	};
			//	for (const auto& target : targets)
			//	{
			//		stl::write_thunk_call<BehaviorGraph::TestHook>(
			//			target.address());
			//	}
			//}
		}
	}
}
