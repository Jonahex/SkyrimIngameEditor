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
#include <RE/I/ImageSpaceEffectManager.h>
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

#include <iostream>
#include <tchar.h>
#include <unordered_set>
#include <stacktrace>

#include <Windows.h>

struct RendererShadowState
{
	uint32_t stateUpdateFlags;
	uint8_t pad0[0x8C];
	uint32_t depthStencilStencilMode;
	uint32_t stencilRef;
};

struct BSLightingShader_SetupGeometry
{
	static void thunk(RE::BSShader* shader, RE::BSRenderPass* pass, uint32_t renderFlags)
	{
		func(shader, pass, renderFlags);

		static REL::Relocation<RendererShadowState*> RendererShadowStateInstance(
			RE::Offset::RendererShadowStateInstance);
		if (auto fadeNode = pass->shaderProperty->fadeNode)
		{
			const auto& targetManager = SIE::TargetManager::Instance();
			if (fadeNode->userData != nullptr && fadeNode->userData == targetManager.GetTarget() &&
				targetManager.GetEnableTargetHighlight())
			{
				const bool isOutline = (((pass->passEnum - 1207959597) >> 24) & 0x3F) == 20;
				auto& rss = *RendererShadowStateInstance;
				rss.stateUpdateFlags |= 8;
				if (isOutline)
				{
					rss.depthStencilStencilMode = 0x1C;
					rss.stencilRef = 1;
				}
				else
				{
					rss.depthStencilStencilMode = 2;
					rss.stencilRef = 1;
				}
			}
		}
	}

	static inline REL::Relocation<decltype(thunk)> func;
	static constexpr size_t idx = 0x06;
};

struct BSLightingShaderProperty_GetShaderPasses
{
	static RE::BSShaderProperty::RenderPassArray* thunk(RE::BSLightingShaderProperty* property,
		RE::BSGeometry* geometry, uint32_t unk, RE::BSShaderAccumulator* accumulator)
	{
		auto result = func(property, geometry, unk, accumulator);

		static REL::Relocation<RE::BSRenderPass*(RE::BSShaderProperty::RenderPassArray*,
			RE::BSRenderPass*, RE::BSShader*, RE::BSShaderProperty*, RE::BSGeometry*, uint32_t)>
			AddPass(RELOCATION_ID(98885, 105529));
		static REL::Relocation<RE::BSShader**> LightingShaderPtr(RELOCATION_ID(528150, 415094));
		 
		if (auto fadeNode = property->fadeNode)
		{
			const auto& targetManager = SIE::TargetManager::Instance();
			if (fadeNode->userData != nullptr && fadeNode->userData == targetManager.GetTarget() &&
				targetManager.GetEnableTargetHighlight())
			{
				/*auto actualPass = property->renderPassList.head;
				RE::BSRenderPass* previousPass = nullptr;
				while (actualPass != nullptr && actualPass->shaderProperty == property)
				{
					logger::info("{} {}", actualPass->shader->shaderType.underlying(),
						actualPass->passEnum);
					previousPass = actualPass;
					actualPass = actualPass->next;
				}
				if (previousPass != nullptr)
				{
					previousPass->next = AddPass(&property->renderPassList, actualPass, *LightingShaderPtr,
						property,
						geometry, (20 << 24) + 1207959597);
				}*/
				AddPass(&property->renderPassList, nullptr, *LightingShaderPtr, property,
					geometry, (20 << 24) + 1207959597);
			}
		}

		return result;
	}

	static inline REL::Relocation<decltype(thunk)> func;
	static constexpr size_t idx = 0x2A;
};

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
			vertexShader = shaderCache.GetVertexShader(shader->shaderType.get(), vertexDescriptor);
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
			pixelShader = shaderCache.GetPixelShader(shader->shaderType.get(), pixelDescriptor);
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
		else
		{
			for (auto item : shader->pixelShaders)
			{
				if (item->id == pixelDescriptor)
				{
					pixelShader->constantBuffers[0] = item->constantBuffers[0];
					pixelShader->constantBuffers[1] = item->constantBuffers[1];
					pixelShader->constantBuffers[2] = item->constantBuffers[2];
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

	struct ActorMediator_Process
	{
		static bool thunk(void* mediator, RE::BGSActionData* actionData)
		{
			const bool processed = func(mediator, actionData);

			if (enable && processed && refr == actionData->source.get()) 
			{
				std::lock_guard lock(mutex);
				//logger::info(std::format("Action {} was processed", actionData->action->formEditorID.data()));
			}

			return func(mediator, actionData);
		}
		static inline REL::Relocation<decltype(thunk)> func;

		static inline RE::TESObjectREFR* refr = nullptr;
		static inline bool enable = true;
	};

	struct Test
	{
		static void thunk(void* mediator, RE::BGSActionData* action)
		{
			func(mediator, action);

			if (action->unk28 == "HorseExit") 
			{
				RE::BSTSmartPointer<RE::BSAnimationGraphManager> manager;
				action->target->GetAnimationGraphManager(manager);
				auto dbData = manager->graphs[0]->projectDBData;

				for (const auto& name : dbData->synchronizedClipGenerators) 
				{
					//logger::info(name.data());
				}

				logger::info("{} {} {} {} {}", action->action->formEditorID.data(), action->source ? action->source->GetDisplayFullName() : "null", action->target ? action->target->GetDisplayFullName() : "null", action->unk28.data(), action->unk30.data());
			}
		}
		static inline REL::Relocation<decltype(thunk)> func;
	};

	namespace Console
	{
		void Print(const char* a_fmt)
	    {
			if (RE::ConsoleLog::IsConsoleMode()) 
			{
				RE::ConsoleLog::GetSingleton()->Print(a_fmt);
			}
		};

		namespace StartGraphTracking
		{
			bool Execute(const RE::SCRIPT_PARAMETER* a_paramInfo, RE::SCRIPT_FUNCTION::ScriptData* a_scriptData, RE::TESObjectREFR* refr, RE::TESObjectREFR* a_containingObj, RE::Script* a_scriptObj, RE::ScriptLocals* a_locals, double& a_result, std::uint32_t& a_opcodeOffsetPtr)
			{
				if (refr != nullptr) 
				{
					int eventsState = 0;
					int varsState = 0;
					int actionsState = 0;
					if (auto chunk = a_scriptData->GetIntegerChunk(); a_scriptData->chunkSize >= 7) 
					{
						eventsState = chunk->GetInteger();
						if (chunk = chunk->GetNext()->AsInteger(); a_scriptData->chunkSize >= 12) 
						{
							varsState = chunk->GetInteger();
							if (chunk = chunk->GetNext()->AsInteger(); a_scriptData->chunkSize >= 17) 
							{
								actionsState = chunk->GetInteger();
							}
						}
					}
					
					RE::Actor* actor = nullptr;
					if (refr->formType == RE::FormType::ActorCharacter) 
					{
						actor = static_cast<RE::Actor*>(refr);
					}
					{
						const bool enable = !(actionsState & 0b1);

						ActorMediator_Process::refr = refr;
						ActorMediator_Process::enable = enable;
					}
					{
						const bool enableProcess = !(eventsState & 0b1);
						const bool enableSend = !(eventsState & 0b10);
					}
					{
						const bool enableSetNonEveryFrame = !(varsState & 0b1);
						const bool enableSetEveryFrame = !(varsState & 0b10);
						const bool enableGetSuccessful = !(varsState & 0b100);
						const bool enableGetFailed = !(varsState & 0b1000);

						BShkbAnimationGraph_SetGraphVariableFloat::refr = refr;
						BShkbAnimationGraph_SetGraphVariableFloat::enableNonEveryFrame = enableSetNonEveryFrame;

						BShkbAnimationGraph_GetGraphVariableFloat::refr = refr;
						BShkbAnimationGraph_GetGraphVariableFloat::enableSuccessful = enableGetSuccessful;
						BShkbAnimationGraph_GetGraphVariableFloat::enableFailed = enableGetFailed;

						BShkbAnimationGraph_SetGraphVariableBool::refr = refr;
						BShkbAnimationGraph_SetGraphVariableBool::enableNonEveryFrame = enableSetNonEveryFrame;

						BShkbAnimationGraph_GetGraphVariableBool::refr = refr;
						BShkbAnimationGraph_GetGraphVariableBool::enableSuccessful = enableGetSuccessful;
						BShkbAnimationGraph_GetGraphVariableBool::enableFailed = enableGetFailed;

						BShkbAnimationGraph_SetGraphVariableInt::refr = refr;
						BShkbAnimationGraph_SetGraphVariableInt::enableNonEveryFrame = enableSetNonEveryFrame;

						BShkbAnimationGraph_GetGraphVariableInt::refr = refr;
						BShkbAnimationGraph_GetGraphVariableInt::enableSuccessful = enableGetSuccessful;
						BShkbAnimationGraph_GetGraphVariableInt::enableFailed = enableGetFailed;

						BShkbAnimationGraph_SetGraphVariableIntRef::refr = refr;
						BShkbAnimationGraph_SetGraphVariableIntRef::enableNonEveryFrame = enableSetNonEveryFrame;

						BShkbAnimationGraph_SetGraphVariableVector::refr = refr;
						BShkbAnimationGraph_SetGraphVariableVector::enableNonEveryFrame = enableSetNonEveryFrame;
						BShkbAnimationGraph_SetGraphVariableVector::enableEveryFrame = enableSetEveryFrame;

						BShkbAnimationGraph_GetGraphVariableVector::refr = refr;
						BShkbAnimationGraph_GetGraphVariableVector::enableSuccessful = enableGetSuccessful;
						BShkbAnimationGraph_GetGraphVariableVector::enableFailed = enableGetFailed;

						BSAnimationGraphChannel_PollChannelUpdateImpl<RE::ActorCopyGraphVariableChannel, float>::actor = actor;
						BSAnimationGraphChannel_PollChannelUpdateImpl<RE::ActorCopyGraphVariableChannel, float>::enable = enableSetEveryFrame;

						BSAnimationGraphChannel_PollChannelUpdateImpl<RE::ActorCopyGraphVariableChannel, int>::actor = actor;
						BSAnimationGraphChannel_PollChannelUpdateImpl<RE::ActorCopyGraphVariableChannel, int>::enable = enableSetEveryFrame;

						BSAnimationGraphChannel_PollChannelUpdateImpl<RE::ActorDirectionChannel, float>::actor = actor;
						BSAnimationGraphChannel_PollChannelUpdateImpl<RE::ActorDirectionChannel, float>::enable = enableSetEveryFrame;

						BSAnimationGraphChannel_PollChannelUpdateImpl<RE::ActorLeftWeaponSpeedChannel, float>::actor = actor;
						BSAnimationGraphChannel_PollChannelUpdateImpl<RE::ActorLeftWeaponSpeedChannel, float>::enable = enableSetEveryFrame;

						BSAnimationGraphChannel_PollChannelUpdateImpl<RE::ActorLookAtChannel, bool>::actor = actor;
						BSAnimationGraphChannel_PollChannelUpdateImpl<RE::ActorLookAtChannel, bool>::enable = enableSetEveryFrame;

						BSAnimationGraphChannel_PollChannelUpdateImpl<RE::ActorPitchChannel, float>::actor = actor;
						BSAnimationGraphChannel_PollChannelUpdateImpl<RE::ActorPitchChannel, float>::enable = enableSetEveryFrame;

						BSAnimationGraphChannel_PollChannelUpdateImpl<RE::ActorPitchDeltaChannel, float>::actor = actor;
						BSAnimationGraphChannel_PollChannelUpdateImpl<RE::ActorPitchDeltaChannel, float>::enable = enableSetEveryFrame;

						BSAnimationGraphChannel_PollChannelUpdateImpl<RE::ActorRollChannel, float>::actor = actor;
						BSAnimationGraphChannel_PollChannelUpdateImpl<RE::ActorRollChannel, float>::enable = enableSetEveryFrame;

						BSAnimationGraphChannel_PollChannelUpdateImpl<RE::ActorSpeedChannel, float>::actor = actor;
						BSAnimationGraphChannel_PollChannelUpdateImpl<RE::ActorSpeedChannel, float>::enable = enableSetEveryFrame;

						BSAnimationGraphChannel_PollChannelUpdateImpl<RE::ActorTargetSpeedChannel, float>::actor = actor;
						BSAnimationGraphChannel_PollChannelUpdateImpl<RE::ActorTargetSpeedChannel, float>::enable = enableSetEveryFrame;

						BSAnimationGraphChannel_PollChannelUpdateImpl<RE::ActorTimeDeltaChannel, float>::actor = actor;
						BSAnimationGraphChannel_PollChannelUpdateImpl<RE::ActorTimeDeltaChannel, float>::enable = enableSetEveryFrame;

						BSAnimationGraphChannel_PollChannelUpdateImpl<RE::ActorTurnDeltaChannel, float>::actor = actor;
						BSAnimationGraphChannel_PollChannelUpdateImpl<RE::ActorTurnDeltaChannel, float>::enable = enableSetEveryFrame;

						BSAnimationGraphChannel_PollChannelUpdateImpl<RE::ActorWantBlockChannel, int>::actor = actor;
						BSAnimationGraphChannel_PollChannelUpdateImpl<RE::ActorWantBlockChannel, int>::enable = enableSetEveryFrame;

						BSAnimationGraphChannel_PollChannelUpdateImpl<RE::ActorWardHealthChannel, float>::actor = actor;
						BSAnimationGraphChannel_PollChannelUpdateImpl<RE::ActorWardHealthChannel, float>::enable = enableSetEveryFrame;

						BSAnimationGraphChannel_PollChannelUpdateImpl<RE::ActorWeaponSpeedChannel, float>::actor = actor;
						BSAnimationGraphChannel_PollChannelUpdateImpl<RE::ActorWeaponSpeedChannel, float>::enable = enableSetEveryFrame;
					}

					logger::info("Started tracking graph of reference {} {:x}", refr->GetDisplayFullName(), refr->GetFormID());
					Print(std::format("Started tracking graph of reference {} {:x}", refr->GetDisplayFullName(), refr->GetFormID()).c_str());
				}
			    else 
				{
					Print("Target object is not found!");
				}

				return true;
			}
	    }
	}

	void Install()
	{
		/*{
#ifndef SKYRIM_SUPPORT_AE
			const std::array callTargets{
				REL::Relocation<std::uintptr_t>(REL::ID(36698), 0xB9),
				REL::Relocation<std::uintptr_t>(REL::ID(36699), 0xC6),
				REL::Relocation<std::uintptr_t>(REL::ID(36764), 0x11C),
				REL::Relocation<std::uintptr_t>(REL::ID(36765), 0x131),
				REL::Relocation<std::uintptr_t>(REL::ID(36766), 0x155),
				REL::Relocation<std::uintptr_t>(REL::ID(36768), 0x11F),
				REL::Relocation<std::uintptr_t>(REL::ID(36771), 0x11B),
				REL::Relocation<std::uintptr_t>(REL::ID(37035), 0x208),
				REL::Relocation<std::uintptr_t>(REL::ID(37842), 0xD0),
				REL::Relocation<std::uintptr_t>(REL::ID(37997), 0x36),
			};
			const std::array jumpTargets{
				REL::Relocation<std::uintptr_t>(REL::ID(40551), 0xA),
			};
#endif
			for (const auto& target : callTargets) {
				stl::write_thunk_call<ActorMediator_Process>(target.address());
			}
			for (const auto& target : jumpTargets) {
				stl::write_thunk_jmp<ActorMediator_Process>(target.address());
			}
		}*/

		/*{
			const std::array targets{
				REL::Relocation<std::uintptr_t>(REL::ID(37998), 0x6F),
			};
			for (const auto& target : targets) {
				stl::write_thunk_call<Test>(target.address());
			}
		}*/

		/*{
			c
			stl::write_vfunc<RE::BSTAnimationGraphDataChannel<RE::Actor, int, RE::ActorCopyGraphVariableChannel>, BSAnimationGraphChannel_PollChannelUpdateImpl<RE::ActorCopyGraphVariableChannel, int>>(RE::VTABLE_BSTAnimationGraphDataChannel_Actor_int_ActorCopyGraphVariableChannel_[0]);
			stl::write_vfunc<RE::BSTAnimationGraphDataChannel<RE::Actor, float, RE::ActorDirectionChannel>, BSAnimationGraphChannel_PollChannelUpdateImpl<RE::ActorDirectionChannel, float>>(RE::VTABLE_BSTAnimationGraphDataChannel_Actor_float_ActorDirectionChannel_[0]);
			stl::write_vfunc<RE::BSTAnimationGraphDataChannel<RE::Actor, float, RE::ActorLeftWeaponSpeedChannel>, BSAnimationGraphChannel_PollChannelUpdateImpl<RE::ActorLeftWeaponSpeedChannel, float>>(RE::VTABLE_BSTAnimationGraphDataChannel_Actor_float_ActorLeftWeaponSpeedChannel_[0]);
			stl::write_vfunc<RE::BSTAnimationGraphDataChannel<RE::Actor, bool, RE::ActorLookAtChannel>, BSAnimationGraphChannel_PollChannelUpdateImpl<RE::ActorLookAtChannel, bool>>(RE::VTABLE_BSTAnimationGraphDataChannel_Actor_bool_ActorLookAtChannel_[0]);
			stl::write_vfunc<RE::BSTAnimationGraphDataChannel<RE::Actor, float, RE::ActorPitchChannel>, BSAnimationGraphChannel_PollChannelUpdateImpl<RE::ActorPitchChannel, float>>(RE::VTABLE_BSTAnimationGraphDataChannel_Actor_float_ActorPitchChannel_[0]);
			stl::write_vfunc<RE::BSTAnimationGraphDataChannel<RE::Actor, float, RE::ActorPitchDeltaChannel>, BSAnimationGraphChannel_PollChannelUpdateImpl<RE::ActorPitchDeltaChannel, float>>(RE::VTABLE_BSTAnimationGraphDataChannel_Actor_float_ActorPitchDeltaChannel_[0]);
			stl::write_vfunc<RE::BSTAnimationGraphDataChannel<RE::Actor, float, RE::ActorRollChannel>, BSAnimationGraphChannel_PollChannelUpdateImpl<RE::ActorRollChannel, float>>(RE::VTABLE_BSTAnimationGraphDataChannel_Actor_float_ActorRollChannel_[0]);
			stl::write_vfunc<RE::BSTAnimationGraphDataChannel<RE::Actor, float, RE::ActorSpeedChannel>, BSAnimationGraphChannel_PollChannelUpdateImpl<RE::ActorSpeedChannel, float>>(RE::VTABLE_BSTAnimationGraphDataChannel_Actor_float_ActorSpeedChannel_[0]);
			stl::write_vfunc<RE::BSTAnimationGraphDataChannel<RE::Actor, float, RE::ActorTargetSpeedChannel>, BSAnimationGraphChannel_PollChannelUpdateImpl<RE::ActorTargetSpeedChannel, float>>(RE::VTABLE_BSTAnimationGraphDataChannel_Actor_float_ActorTargetSpeedChannel_[0]);
			stl::write_vfunc<RE::BSTAnimationGraphDataChannel<RE::Actor, float, RE::ActorTimeDeltaChannel>, BSAnimationGraphChannel_PollChannelUpdateImpl<RE::ActorTimeDeltaChannel, float>>(RE::VTABLE_BSTAnimationGraphDataChannel_Actor_float_ActorTimeDeltaChannel_[0]);
			stl::write_vfunc<RE::BSTAnimationGraphDataChannel<RE::Actor, float, RE::ActorTurnDeltaChannel>, BSAnimationGraphChannel_PollChannelUpdateImpl<RE::ActorTurnDeltaChannel, float>>(RE::VTABLE_BSTAnimationGraphDataChannel_Actor_float_ActorTurnDeltaChannel_[0]);
			stl::write_vfunc<RE::BSTAnimationGraphDataChannel<RE::Actor, int, RE::ActorWantBlockChannel>, BSAnimationGraphChannel_PollChannelUpdateImpl<RE::ActorWantBlockChannel, int>>(RE::VTABLE_BSTAnimationGraphDataChannel_Actor_int_ActorWantBlockChannel_[0]);
			stl::write_vfunc<RE::BSTAnimationGraphDataChannel<RE::Actor, float, RE::ActorWardHealthChannel>, BSAnimationGraphChannel_PollChannelUpdateImpl<RE::ActorWardHealthChannel, float>>(RE::VTABLE_BSTAnimationGraphDataChannel_Actor_float_ActorWardHealthChannel_[0]);
			stl::write_vfunc<RE::BSTAnimationGraphDataChannel<RE::Actor, float, RE::ActorWeaponSpeedChannel>, BSAnimationGraphChannel_PollChannelUpdateImpl<RE::ActorWeaponSpeedChannel, float>>(RE::VTABLE_BSTAnimationGraphDataChannel_Actor_float_ActorWeaponSpeedChannel_[0]);
		}

		{
			const std::array targets{
				REL::Relocation<std::uintptr_t>(RELOCATION_ID(62708, 63609), OFFSET(0, 0)),
			};
			for (const auto& target : targets) {
				stl::write_thunk_jmp<BShkbAnimationGraph_SetGraphVariableInt>(target.address());
			}
		}

		{
			const std::array targets{
				REL::Relocation<std::uintptr_t>(RELOCATION_ID(62694, 63615), OFFSET(0, 0)),
			};
			for (const auto& target : targets) {
				stl::write_thunk_jmp<BShkbAnimationGraph_GetGraphVariableInt>(target.address());
			}
		}

		{
			const std::array targets{
				REL::Relocation<std::uintptr_t>(RELOCATION_ID(62709, 63608), OFFSET(0, 0)),
			};
			for (const auto& target : targets) {
				stl::write_thunk_jmp<BShkbAnimationGraph_SetGraphVariableFloat>(target.address());
			}
		}

		{
			const std::array targets{
				REL::Relocation<std::uintptr_t>(RELOCATION_ID(62695, 63614), OFFSET(0, 0)),
			};
			for (const auto& target : targets) {
				stl::write_thunk_jmp<BShkbAnimationGraph_GetGraphVariableFloat>(target.address());
			}
		}

		{
			const std::array targets{
				REL::Relocation<std::uintptr_t>(RELOCATION_ID(62710, 63607), OFFSET(0, 0)),
			};
			for (const auto& target : targets) {
				stl::write_thunk_jmp<BShkbAnimationGraph_SetGraphVariableBool>(target.address());
			}
		}

		{
			const std::array targets{
				REL::Relocation<std::uintptr_t>(RELOCATION_ID(62696, 63613), OFFSET(0, 0)),
			};
			for (const auto& target : targets) {
				stl::write_thunk_jmp<BShkbAnimationGraph_GetGraphVariableBool>(target.address());
			}
		}

		{
			const std::array targets{
				REL::Relocation<std::uintptr_t>(RELOCATION_ID(62707, 63651), OFFSET(0, 0)),
#ifdef SKYRIM_SUPPORT_AE
				REL::Relocation<std::uintptr_t>(REL::ID(63652), 0),
#endif
			};
			for (const auto& target : targets) {
				stl::write_thunk_jmp<BShkbAnimationGraph_SetGraphVariableVector>(target.address());
			}
		}

		{
			const std::array targets{
				REL::Relocation<std::uintptr_t>(RELOCATION_ID(62692, 63636), OFFSET(0, 0)),
				REL::Relocation<std::uintptr_t>(RELOCATION_ID(62693, 63637), OFFSET(0, 0)),
			};
			for (const auto& target : targets) {
				stl::write_thunk_jmp<BShkbAnimationGraph_GetGraphVariableVector>(target.address());
			}
		}

#ifdef SKYRIM_SUPPORT_AE
		{
			const std::array targets{
				REL::Relocation<std::uintptr_t>(REL::ID(63653), 0),
			};
			for (const auto& target : targets) {
				stl::write_thunk_jmp<BShkbAnimationGraph_SetGraphVariableIntRef>(target.address());
			}
		}
#endif*/
		
		if (const auto function = RE::SCRIPT_FUNCTION::LocateConsoleCommand("ClearAchievement"); function != nullptr) {
			static RE::SCRIPT_PARAMETER params[] = {
				{ "EventsState", RE::SCRIPT_PARAM_TYPE::kInt, true },
				{ "VariablesState", RE::SCRIPT_PARAM_TYPE::kInt, true },
				{ "ActionsState", RE::SCRIPT_PARAM_TYPE::kInt, true },
			};

			function->functionName = "StartGraphTracking";
			function->shortName = "sgt";
			function->helpString = "";
			function->referenceFunction = false;
			function->SetParameters(params);
			function->executeFunction = &Console::StartGraphTracking::Execute;
			function->conditionFunction = nullptr;

			logger::info("installed StartGraphTracking");
		}

		logger::info("installed BehaviorGraph");
	}
}

namespace Hooks
{
	void Install()
	{
		//BehaviorGraph::Install();
	}

	void OnPostPostLoad()
	{
		/*{
			const auto moduleBase = reinterpret_cast<uintptr_t>(GetModuleHandle(nullptr));

			const auto saveBinaryAddress =
				REL::Relocation<std::uintptr_t>(RELOCATION_ID(76234, 78064), 0x3F)
					.address();
			SIE::PatchMemory(saveBinaryAddress, PBYTE("\xE9\x59\x00\x00\x00"), 5);

			const auto registerStreamablesAddress =
				REL::Relocation<std::uintptr_t>(RELOCATION_ID(76233, 78063), 0x21).address();
			SIE::PatchMemory(registerStreamablesAddress, PBYTE("\xE9\x29\x00\x00\x00"), 5);
		}*/

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

		/*stl::write_vfunc<BSLightingShaderProperty_GetShaderPasses>(
			RE::VTABLE_BSLightingShaderProperty[0]);
		stl::write_vfunc<BSLightingShader_SetupGeometry>(
			RE::VTABLE_BSLightingShader[0]);*/

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

			{
				const std::array targets{
					REL::Relocation<std::uintptr_t>(RE::Offset::WinMain, OFFSET(0x35, 0x1AE)),
				};
				for (const auto& target : targets)
				{
					stl::write_thunk_call<WinMain_OnQuitGame>(target.address());
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
			auto isem = RE::ImageSpaceEffectManager::GetSingleton();
			isem->shaderInfo.blurCSShaderInfo->isEnabled = false;
#endif
		
#ifdef SKYRIM_SUPPORT_AE
			{
				const auto renderPassCacheCtor = REL::ID(107500);
				const int32_t passCount = 4194240;
				const int32_t passSize = 4194240 * sizeof(RE::BSRenderPass);
				const int32_t lightsCount = passCount * 16;
				const int32_t lightsSize = lightsCount * sizeof(void*);
				const int32_t lastPassIndex = passCount - 1;
				const int32_t lastPassOffset =
					(passCount - 1) * sizeof(RE::BSRenderPass);
				const int32_t lastPassNextOffset =
					(passCount - 1) * sizeof(RE::BSRenderPass) + offsetof(RE::BSRenderPass, next);
				SIE::PatchMemory(
					REL::Relocation<std::uintptr_t>(renderPassCacheCtor, 0x76).address(),
					reinterpret_cast<const uint8_t*>(&lightsSize), 4);
				SIE::PatchMemory(
					REL::Relocation<std::uintptr_t>(renderPassCacheCtor, 0xAD).address(),
					reinterpret_cast<const uint8_t*>(&passSize), 4);
				SIE::PatchMemory(
					REL::Relocation<std::uintptr_t>(renderPassCacheCtor, 0xCB).address(),
					reinterpret_cast<const uint8_t*>(&lastPassIndex), 4);
				SIE::PatchMemory(
					REL::Relocation<std::uintptr_t>(renderPassCacheCtor, 0xF0).address(),
					reinterpret_cast<const uint8_t*>(&lastPassNextOffset), 4);
				SIE::PatchMemory(
					REL::Relocation<std::uintptr_t>(renderPassCacheCtor, 0xFD).address(),
					reinterpret_cast<const uint8_t*>(&lastPassOffset), 4);
				SIE::PatchMemory(
					REL::Relocation<std::uintptr_t>(renderPassCacheCtor, 0x191).address(),
					reinterpret_cast<const uint8_t*>(&passCount), 4);
			}
#endif

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
