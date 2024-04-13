#include "Gui/TargetEditor.h"

#include "Gui/FootIkEditor.h"
#include "Gui/NiObjectEditor.h"
#include "Gui/NiTransformEditor.h"
#include "Gui/Utils.h"
#include "Gui/WaterEditor.h"
#include "Utils/Engine.h"
#include "Utils/GraphTracker.h"
#include "Utils/RTTICache.h"
#include "Utils/TargetManager.h"

#define FTS_FUZZY_MATCH_IMPLEMENTATION
#include "3rdparty/fts_fuzzy_match.h"
#include "3rdparty/ImGuizmo/ImGuizmo.h"

#include <RE/A/ActorValueList.h>
#include <RE/A/AIProcess.h>
#include <RE/A/AnimationSetDataSingleton.h>
#include <RE/B/BSAnimationGraphManager.h>
#include <RE/B/BShkbAnimationGraph.h>
#include <RE/B/BSPathingSolution.h>
#include <RE/H/hkbBehaviorGraph.h>
#include <RE/H/hkbBehaviorGraphData.h>
#include <RE/H/hkbRagdollDriver.h>
#include <RE/H/hkbSymbolIdMap.h>
#include <RE/H/hkbVariableInfo.h>
#include <RE/H/hkbVariableValueSet.h>
#include <RE/H/hkClass.h>
#include <RE/H/hkClassMember.h>
#include <RE/H/hkClassEnum.h>
#include <RE/H/hkMatrix4.h>
#include <RE/H/hkVariant.h>
#include <RE/M/MovementAgent.h>
#include <RE/M/MovementAgentPathFollowerStandard.h>
#include <RE/M/MovementArbiter.h>
#include <RE/M/MovementControllerNPC.h>
#include <RE/N/NiNode.h>
#include <RE/N/NiRTTI.h>
#include <RE/P/PathingCell.h>
#include <RE/T/TESObjectACTI.h>
#include <RE/T/TESObjectTREE.h>
#include <RE/T/TESPackage.h>
#include <RE/T/TESWaterForm.h>

#include <imgui.h>
#include <imgui_stdlib.h>

namespace SIE
{
	namespace STargetEditor
	{
		template<typename T>
		T* GetMemberValue(void* object, size_t offset)
		{
			return reinterpret_cast<T*>(static_cast<uint8_t*>(object) + offset);
		}

		bool IsHkReferencedObject(const RE::hkClass* objectClass)
		{
			if (objectClass == nullptr)
			{
				return false;
			}
			if (std::strcmp(objectClass->m_name, "hkReferencedObject") == 0)
			{
				return true;
			}
			return IsHkReferencedObject(objectClass->m_parent);
		}

		std::optional<size_t> GetHkTypeSize(RE::hkClassMember::Type memberType, const RE::hkClass* memberClass)
		{
			using enum RE::hkClassMember::Type;

			switch (memberType)
			{
			case TYPE_BOOL:
				return sizeof(bool);
			case TYPE_CHAR:
				return sizeof(signed char);
			case TYPE_INT8:
				return sizeof(int8_t);
			case TYPE_UINT8:
				return sizeof(uint8_t);
			case TYPE_INT16:
				return sizeof(int16_t);
			case TYPE_UINT16:
				return sizeof(uint16_t);
			case TYPE_INT32:
				return sizeof(int32_t);
			case TYPE_UINT32:
				return sizeof(uint32_t);
			case TYPE_INT64:
				return sizeof(int64_t);
			case TYPE_UINT64:
				return sizeof(uint64_t);
			case TYPE_REAL:
				return sizeof(float);
			case TYPE_VECTOR4:
				return sizeof(RE::hkVector4);
			case TYPE_QUATERNION:
				return sizeof(RE::hkQuaternion);
			case TYPE_MATRIX3:
				return sizeof(RE::hkMatrix3);
			case TYPE_ROTATION:
				return sizeof(RE::hkQuaternion);
			case TYPE_QSTRANSFORM:
				return sizeof(RE::hkQsTransform);
			case TYPE_MATRIX4:
				return sizeof(RE::hkMatrix4);
			case TYPE_TRANSFORM:
				return sizeof(RE::hkTransform);
			case TYPE_POINTER:
			case TYPE_FUNCTIONPOINTER:
			case TYPE_ULONG:
				return sizeof(void*);
			case TYPE_ARRAY:
				return sizeof(RE::hkArray<void*>);
			case TYPE_VARIANT:
				return sizeof(RE::hkVariant);
			case TYPE_STRINGPTR:
				return sizeof(const char*);
			case TYPE_ENUM:
				if (memberClass != nullptr)
				{
					return memberClass->m_flags.underlying();
				}
				return {};
			case TYPE_STRUCT:
				if (memberClass != nullptr)
				{
					return static_cast<size_t>(memberClass->m_objectSize);
				}
				return {};
			}

			return {};
		}

		void HkObjectViewer(void* object, const RE::hkClass& objectClass);

		template<typename UnderlyingType>
		void HkEnumEditor(void* object, size_t offset, const char* memberName, const RE::hkClassEnum& memberEnum)
		{
			auto enumValue = GetMemberValue<UnderlyingType>(object, offset);
			const char* previewItemName = nullptr;
			for (int itemIndex = 0; itemIndex < memberEnum.m_numItems; ++itemIndex)
			{
				if (static_cast<UnderlyingType>(memberEnum.m_items[itemIndex].m_value) == *enumValue)
				{
					previewItemName = memberEnum.m_items[itemIndex].m_name;
				}
			}
			if (ImGui::BeginCombo(memberName, previewItemName))
			{
				for (int itemIndex = 0; itemIndex < memberEnum.m_numItems; ++itemIndex)
				{
					const bool isSelected = static_cast<UnderlyingType>(memberEnum.m_items[itemIndex].m_value) == *enumValue;
					if (ImGui::Selectable(memberEnum.m_items[itemIndex].m_name, isSelected))
					{
						*enumValue = static_cast<UnderlyingType>(memberEnum.m_items[itemIndex].m_value);
					}
				}
				ImGui::EndCombo();
			}
		}

		void HkMemberEditor(void* object, size_t offset, RE::hkClassMember::Type memberType,
			const char* memberName, RE::hkClassMember::Type subType,
			const RE::hkClass* memberClass,
			const RE::hkClassEnum* memberEnum)
		{
			using enum RE::hkClassMember::FlagValues;
			using enum RE::hkClassMember::Type;

			switch (memberType)
			{
			case TYPE_BOOL:
				ImGui::Checkbox(memberName, GetMemberValue<bool>(object, offset));
				break;
			case TYPE_CHAR:
			case TYPE_INT8:
				ImGui::DragScalarN(memberName, ImGuiDataType_S8,
					GetMemberValue<char>(object, offset), 1);
				break;
			case TYPE_UINT8:
				ImGui::DragScalarN(memberName, ImGuiDataType_U8,
					GetMemberValue<uint8_t>(object, offset), 1);
				break;
			case TYPE_INT16:
				ImGui::DragScalarN(memberName, ImGuiDataType_S16,
					GetMemberValue<int16_t>(object, offset), 1);
				break;
			case TYPE_UINT16:
				ImGui::DragScalarN(memberName, ImGuiDataType_U16,
					GetMemberValue<uint16_t>(object, offset), 1);
				break;
			case TYPE_INT32:
				ImGui::DragScalarN(memberName, ImGuiDataType_S32,
					GetMemberValue<int32_t>(object, offset), 1);
				break;
			case TYPE_UINT32:
				ImGui::DragScalarN(memberName, ImGuiDataType_U32,
					GetMemberValue<uint32_t>(object, offset), 1);
				break;
			case TYPE_INT64:
				ImGui::DragScalarN(memberName, ImGuiDataType_S64,
					GetMemberValue<int64_t>(object, offset), 1);
				break;
			case TYPE_UINT64:
			case TYPE_ULONG:
				ImGui::DragScalarN(memberName, ImGuiDataType_U64,
					GetMemberValue<uint64_t>(object, offset), 1);
				break;
			case TYPE_REAL:
				ImGui::DragFloat(memberName, GetMemberValue<float>(object,offset));
				break;
			case TYPE_VECTOR4:
				ImGui::DragFloat4(memberName,
					GetMemberValue<RE::hkVector4>(object, offset)->quad.m128_f32);
				break;
			case TYPE_QUATERNION:
				SliderFloatNormalized<4>(memberName,
					GetMemberValue<RE::hkQuaternion>(object, offset)->vec.quad.m128_f32,
					-1.f, 1.f);
				break;
			case TYPE_MATRIX3:
			case TYPE_ROTATION:
				{
					auto matrix = GetMemberValue<RE::hkMatrix3>(object, offset);
					if (PushingCollapsingHeader(memberName))
					{
						ImGui::DragFloat4("##Col0", matrix->col0.quad.m128_f32);
						ImGui::DragFloat4("##Col1", matrix->col1.quad.m128_f32);
						ImGui::DragFloat4("##Col2", matrix->col2.quad.m128_f32);
						ImGui::TreePop();
					}
					break;
				}
			case TYPE_QSTRANSFORM:
				{
					auto transform = GetMemberValue<RE::hkQsTransform>(object, offset);
					if (PushingCollapsingHeader(memberName))
					{
						ImGui::DragFloat4("Translation", transform->translation.quad.m128_f32);
						ImGui::DragFloat4("Rotation", transform->rotation.vec.quad.m128_f32);
						ImGui::DragFloat4("Scale", transform->scale.quad.m128_f32);
						ImGui::TreePop();
					}
					break;
				}
			case TYPE_MATRIX4:
				{
					auto matrix = GetMemberValue<RE::hkMatrix4>(object, offset);
					if (PushingCollapsingHeader(memberName))
					{
						ImGui::DragFloat4("##Col0", matrix->col0.quad.m128_f32);
						ImGui::DragFloat4("##Col1", matrix->col1.quad.m128_f32);
						ImGui::DragFloat4("##Col2", matrix->col2.quad.m128_f32);
						ImGui::DragFloat4("##Col3", matrix->col3.quad.m128_f32);
						ImGui::TreePop();
					}
					break;
				}
			case TYPE_TRANSFORM:
				{
					auto transform = GetMemberValue<RE::hkTransform>(object, offset);
					if (PushingCollapsingHeader(memberName))
					{
						ImGui::DragFloat4("Translation", transform->translation.quad.m128_f32);
						if (PushingCollapsingHeader("Rotation"))
						{
							ImGui::DragFloat4("##Col0", transform->rotation.col0.quad.m128_f32);
							ImGui::DragFloat4("##Col1", transform->rotation.col1.quad.m128_f32);
							ImGui::DragFloat4("##Col2", transform->rotation.col2.quad.m128_f32);
							ImGui::TreePop();
						}
						ImGui::TreePop();
					}
					break;
				}
			case TYPE_ARRAY:
				{
					auto array = GetMemberValue<RE::hkArray<void*>>(object, offset);
					const auto itemSize = GetHkTypeSize(subType, memberClass);
					if (itemSize.has_value())
					{
						if (PushingCollapsingHeader(memberName))
						{
							for (int32_t itemIndex = 0; itemIndex < array->size(); ++itemIndex)
							{
								HkMemberEditor(reinterpret_cast<void*>(array->data()),
									*itemSize * itemIndex, subType,
									std::to_string(itemIndex).c_str(), TYPE_VOID,
									memberClass, memberEnum);
							}
							ImGui::TreePop();
						}
					}
					break;
				}
			case TYPE_STRINGPTR:
				{
					auto stringPtr = GetMemberValue<RE::hkStringPtr>(object, offset);
					std::string string = stringPtr->data();
					ImGui::InputText(memberName, &string);
					break;
				}
			case TYPE_VARIANT:
				{
					auto variant = GetMemberValue<RE::hkVariant>(object, offset);
					if (variant->m_class != nullptr && variant->m_object != nullptr)
					{
						auto actualClass =
							IsHkReferencedObject(memberClass) ?
								static_cast<RE::hkReferencedObject*>(variant->m_object)
									->GetClassType() :
								variant->m_class;
						actualClass = actualClass ? actualClass : variant->m_class;
						if (PushingCollapsingHeader(
								std::format("[{}] {}", actualClass->m_name, memberName)
									.c_str()))
						{
							HkObjectViewer(variant->m_object, *actualClass);
							ImGui::TreePop();
						}
					}
					break;
				}
			case TYPE_POINTER:
				{
					auto objectPtr = GetMemberValue<void*>(object, offset);
					if (*objectPtr != nullptr && memberClass != nullptr)
					{
						auto actualClass =
							IsHkReferencedObject(memberClass) ?
								static_cast<RE::hkReferencedObject*>(*objectPtr)->GetClassType() :
								memberClass;
						actualClass = actualClass ? actualClass : memberClass;
						if (PushingCollapsingHeader(
								std::format("[{}] {}", actualClass->m_name, memberName).c_str()))
						{
							HkObjectViewer(*objectPtr, *actualClass);
							ImGui::TreePop();
						}
					}
					break;
				}
			case TYPE_STRUCT:
				{
					auto objectPtr = GetMemberValue<void>(object, offset);
					if (objectPtr != nullptr && memberClass != nullptr)
					{
						auto actualClass =
							IsHkReferencedObject(memberClass) ?
								static_cast<RE::hkReferencedObject*>(objectPtr)->GetClassType() :
								memberClass;
						actualClass = actualClass ? actualClass : memberClass;
						if (PushingCollapsingHeader(
								std::format("[{}] {}", actualClass->m_name, memberName).c_str()))
						{
							HkObjectViewer(objectPtr, *actualClass);
							ImGui::TreePop();
						}
					}
					break;
				}
			case TYPE_ENUM:
				{
					if (memberEnum != nullptr)
					{
						if (subType == TYPE_INT8)
						{
							HkEnumEditor<int8_t>(object, offset, memberName, *memberEnum);
						}
						else if (subType == TYPE_UINT8)
						{
							HkEnumEditor<uint8_t>(object, offset, memberName, *memberEnum);
						}
						else if (subType == TYPE_INT16)
						{
							HkEnumEditor<int16_t>(object, offset, memberName, *memberEnum);
						}
						else if (subType == TYPE_UINT16)
						{
							HkEnumEditor<uint16_t>(object, offset, memberName, *memberEnum);
						}
						else if (subType == TYPE_INT32)
						{
							HkEnumEditor<int32_t>(object, offset, memberName, *memberEnum);
						}
						else if (subType == TYPE_UINT32)
						{
							HkEnumEditor<uint32_t>(object, offset, memberName, *memberEnum);
						}
					}
					break;
				}
			}
		}

		void HkObjectViewer(void* object, const RE::hkClass& objectClass)
		{
			if (auto parentClass = objectClass.m_parent)
			{
				HkObjectViewer(object, *parentClass);
			}
			for (int memberIndex = 0; memberIndex < objectClass.m_numDeclaredMembers;
				 ++memberIndex)
			{
				const auto& member = objectClass.m_declaredMembers[memberIndex];
				HkMemberEditor(object, member.m_offset, member.m_type.get(), member.m_name,
					member.m_subtype.get(), member.m_class, member.m_enum);
			}
		}

		void GraphViewer(const RE::BSAnimationGraphManager& graphManager)
		{
			int graphIndex = 0;
			for (const auto& currentGraph : graphManager.graphs)
			{
				if (PushingCollapsingHeader(std::format("{}##{}",
						currentGraph->behaviorGraph->name.c_str(), graphIndex).c_str()))
				{
					ReadOnlyCheckbox("Active", graphIndex == graphManager.activeGraph);

					const bool hasRagdoll = currentGraph->HasRagdoll();
					ReadOnlyCheckbox("Has Ragdoll", hasRagdoll);

					const auto graph = currentGraph->behaviorGraph;
					if (graph->rootGenerator != nullptr &&
						graph->rootGenerator->GetClassType())
					{
						if (PushingCollapsingHeader(std::format("Root - [{}] {}", graph->rootGenerator->GetClassType()->m_name,
									graph->rootGenerator->name.c_str())
														.c_str()))
						{
							HkObjectViewer(graph->rootGenerator.get(),
								*graph->rootGenerator->GetClassType());
							ImGui::TreePop();
						}
					}

					if (PushingCollapsingHeader("Active Nodes"))
					{
						if (graph->activeNodeTemplateToIndexMap != nullptr)
						{
							std::map<int, RE::hkbNode*> nodes;
							auto it = graph->activeNodeTemplateToIndexMap->getIterator();
							while (graph->activeNodeTemplateToIndexMap->isValid(it))
							{
								const auto& key = graph->activeNodeTemplateToIndexMap->getKey(it);
								const auto& value =
									graph->activeNodeTemplateToIndexMap->getValue(it);
								auto cloneIt = graph->nodeTemplateToCloneMap->findKey(key);
								if (graph->nodeTemplateToCloneMap->isValid(cloneIt))
								{
									nodes[value] = graph->nodeTemplateToCloneMap->getValue(cloneIt);
								}
								it = graph->activeNodeTemplateToIndexMap->getNext(it);
							}
							for (const auto& [index, node] : nodes)
							{
								auto nodeClass = node->GetClassType();
								if (nodeClass != nullptr)
								{
									if (PushingCollapsingHeader(std::format("[{}] {}",
											node->GetClassType()->m_name, node->name.c_str())
																	.c_str()))
									{
										HkObjectViewer(node, *nodeClass);
										ImGui::TreePop();
									}
								}
								else
								{
									ImGui::Text(
										std::format("[Unknown] {}", node->name.c_str())
											.c_str());
								}
							}
						}
						ImGui::TreePop();
					}
					ImGui::TreePop();
				}
				++graphIndex;
			}
		}

		std::string GetPathingPointText(size_t pointIndex, const RE::PathingPoint& point)
		{
			if (point.pathingCell != nullptr)
			{
				if (auto cell = RE::TESForm::LookupByID(point.pathingCell->cellID))
				{
					return std::format("{}: {} ({}, {}, {}), tri {}", pointIndex,
						cell->GetFormEditorID(), point.position.x,
						point.position.y, point.position.z, point.triIndex);
				}
			}
			return std::format("{}: ({}, {}, {})", pointIndex, point.position.x, point.position.y,
				point.position.z);
		}
	}

	void TargetEditor(const char* label)
    {
		ImGui::PushID(label);

		const auto target = TargetManager::Instance().GetTarget();
		if (target != nullptr)
		{
			const auto base = target->GetBaseObject();

			ImGui::Text(std::format("Target: {}", GetTypedName(*target)).c_str());
			ImGui::Text(std::format("Base: {}", GetTypedName(*base)).c_str());

			bool enabled = !target->IsDisabled();
			if (ImGui::Checkbox("Enabled", &enabled))
			{
				if (enabled)
				{
					target->Enable(false);
				}
				else
				{
					target->Disable();
				}
			}

			if (FormEditor(target, ReferenceTransformEditor("Transform", *target)))
			{

			}

			if (auto object3d = target->Get3D())
			{
				if (PushingCollapsingHeader("3D"))
				{
					if (DispatchableNiObjectEditor("", *object3d))
					{
						RE::NiUpdateData updateData;
						updateData.flags.set(RE::NiUpdateData::Flag::kDirty);
						object3d->Update(updateData);
					}
					ImGui::TreePop();
				}
			}

			if (target->formType == RE::FormType::ActorCharacter)
			{
				RE::Actor* targetActor = static_cast<RE::Actor*>(target);
				if (PushingCollapsingHeader("ActorState"))
				{
					ImGui::Text(
						std::format("movingBack is {}", bool(targetActor->actorState1.movingBack))
							.c_str());
					ImGui::Text(std::format("movingForward is {}",
						bool(targetActor->actorState1.movingForward))
									.c_str());
					ImGui::Text(
						std::format("movingRight is {}", bool(targetActor->actorState1.movingRight))
							.c_str());
					ImGui::Text(
						std::format("movingLeft is {}", bool(targetActor->actorState1.movingLeft))
							.c_str());
					ImGui::Text(std::format("walking is {}", bool(targetActor->actorState1.walking))
									.c_str());
					ImGui::Text(std::format("running is {}", bool(targetActor->actorState1.running))
									.c_str());
					ImGui::Text(
						std::format("sprinting is {}", bool(targetActor->actorState1.sprinting))
							.c_str());
					ImGui::Text(
						std::format("sneaking is {}", bool(targetActor->actorState1.sneaking))
							.c_str());
					ImGui::Text(
						std::format("swimming is {}", bool(targetActor->actorState1.swimming))
							.c_str());
					ImGui::Text(std::format("sitSleepState is {}",
						magic_enum::enum_name(targetActor->actorState1.sitSleepState))
									.c_str());
					ImGui::Text(std::format("flyState is {}",
						magic_enum::enum_name(targetActor->actorState1.flyState))
									.c_str());
					ImGui::Text(std::format("lifeState is {}",
						magic_enum::enum_name(targetActor->actorState1.lifeState))
									.c_str());
					ImGui::Text(std::format("knockState is {}",
						magic_enum::enum_name(targetActor->actorState1.knockState))
									.c_str());
					ImGui::Text(std::format("meleeAttackState is {}",
						magic_enum::enum_name(targetActor->actorState1.meleeAttackState))
									.c_str());
					ImGui::Text(std::format("talkingToPlayer is {}",
						bool(targetActor->actorState2.talkingToPlayer))
									.c_str());
					ImGui::Text(
						std::format("forceRun is {}", bool(targetActor->actorState2.forceRun))
							.c_str());
					ImGui::Text(
						std::format("forceSneak is {}", bool(targetActor->actorState2.forceSneak))
							.c_str());
					ImGui::Text(std::format("headTracking is {}",
						bool(targetActor->actorState2.headTracking))
									.c_str());
					ImGui::Text(
						std::format("reanimating is {}", bool(targetActor->actorState2.reanimating))
							.c_str());
					ImGui::Text(std::format("weaponState is {}",
						magic_enum::enum_name(targetActor->actorState2.weaponState))
									.c_str());
					ImGui::Text(std::format("wantBlocking is {}",
						bool(targetActor->actorState2.wantBlocking))
									.c_str());
					ImGui::Text(std::format("flightBlocked is {}",
						bool(targetActor->actorState2.flightBlocked))
									.c_str());
					ImGui::Text(
						std::format("recoil is {}", bool(targetActor->actorState2.recoil)).c_str());
					ImGui::Text(
						std::format("allowFlying is {}", bool(targetActor->actorState2.allowFlying))
							.c_str());
					ImGui::Text(
						std::format("staggered is {}", bool(targetActor->actorState2.staggered))
							.c_str());
					ImGui::TreePop();
				}

				if (PushingCollapsingHeader("Actor Values"))
				{
					auto actorValues = RE::ActorValueList::GetSingleton();
					for (size_t actorValueIndex = 0; actorValueIndex < static_cast<size_t>(RE::ActorValue::kTotal); ++actorValueIndex)
					{
						auto actorValue = static_cast<RE::ActorValue>(actorValueIndex);
						auto actorValueInfo = actorValues->GetActorValue(actorValue);
						auto value = targetActor->GetActorValue(actorValue);
						if (ImGui::DragFloat(actorValueInfo->enumName, &value, 0.1f))
						{
							targetActor->SetActorValue(actorValue, value);
						}
					}
					ImGui::TreePop();
				}

				if (PushingCollapsingHeader("AI"))
				{
					if (ImGui::Button("Evaluate Package"))
					{
						targetActor->EvaluatePackage();
					}
					if (auto aiProcess = targetActor->currentProcess)
					{
						ImGui::Text(std::format("Current process level is {}",
							magic_enum::enum_name(targetActor->currentProcess->processLevel.get()))
										.c_str());

						if (auto package = targetActor->GetCurrentPackage())
						{
							ImGui::Text(std::format("Package {} is running", GetFullName(*package))
											.c_str());
							ImGui::Text(std::format("Current procedure index is {}",
								aiProcess->currentPackage.currentProcedureIndex)
											.c_str());
						}
						if (auto movementController = targetActor->movementController)
						{
							ImGui::Text(std::format("Current movement controller state is {}",
								magic_enum::enum_name(movementController->currentState.get()))
											.c_str());

							if (PushingCollapsingHeader("Arbiters"))
							{
								for (const auto& arbiter : movementController->movementArbiters)
								{
									if (arbiter.Get() != nullptr)
									{
										if (PushingCollapsingHeader(
												arbiter.Get()->GetArbiterName().c_str()))
										{
											ImGui::TreePop();
										}
									}
								}
								ImGui::TreePop();
							}
							if (PushingCollapsingHeader("Agents"))
							{
								for (const auto& agent : movementController->movementAgents)
								{
									if (agent.Get() != nullptr)
									{
										if (PushingCollapsingHeader(agent.Get()->GetName().c_str()))
										{
											if (agent.Get()->GetName() == "Path Follower")
											{
												auto follower = reinterpret_cast<
													RE::MovementAgentPathFollowerStandard*>(
													agent.Get());
												ImGui::Text(
													std::format("Current path follower state is {}",
														magic_enum::enum_name(follower->currentStateType.get()))
														.c_str());
												ImGui::Text(std::format("Path speed: {}",
													follower->pathSpeed)
																.c_str());
												ImGui::Text(std::format("Allow rotation: {}",
													follower->isAllowRotationState)
																.c_str());
												if (follower->pathingSolution != nullptr)
												{
													if (PushingCollapsingHeader("Pathing Solution"))
													{
														if (PushingCollapsingHeader(
																"High-Level Path"))
														{
															size_t pointIndex = 0;
															for (const auto& point :
																follower->pathingSolution
																	->highLevelPath)
															{
																ImGui::Text(
																	STargetEditor::GetPathingPointText(pointIndex, point).c_str());
																++pointIndex;
															}
															ImGui::TreePop();
														}
														if (PushingCollapsingHeader(
																"Detailed Path"))
														{
															size_t pointIndex = 0;
															for (const auto& point :
																follower->pathingSolution
																	->highLevelPath)
															{
																ImGui::Text(
																	STargetEditor::
																		GetPathingPointText(
																			pointIndex, point)
																			.c_str());
																++pointIndex;
															}
															ImGui::TreePop();
														}
														ImGui::TreePop();
													}
												}
											}
											ImGui::TreePop();
										}
									}
								}
								ImGui::TreePop();
							}
						}
					}
					ImGui::TreePop();
				}
			} 
			else if (base->formType == RE::FormType::Activator)
			{
				if (target->IsWater())
				{
					const auto activator = static_cast<RE::TESObjectACTI*>(base);
					if (PushingCollapsingHeader("Water"))
					{
						if (FormEditor(activator,
							FormSelector<false>("Water Type", activator->waterForm)))
						{
							ReloadWaterObjects();
						}
						if (activator->waterForm != nullptr)
						{
							if (WaterEditor("WaterEditor", *activator->waterForm))
							{
								ReloadWaterObjects();
							}
						}
						ImGui::TreePop();
					}
				}
			}
			else if (base->formType == RE::FormType::Tree)
			{
				if (PushingCollapsingHeader("Tree"))
				{
					RE::TESObjectTREE* tree = static_cast<RE::TESObjectTREE*>(base);

					ImGui::DragFloat("Trunk Flexibility", &tree->data.trunkFlexibility, 0.01f);
					ImGui::DragFloat("Branch Flexibility", &tree->data.branchFlexibility, 0.01f);
					ImGui::DragFloat("Leaf Amplitude", &tree->data.leafAmplitude, 0.01f);
					ImGui::DragFloat("Leaf Frequency", &tree->data.leafFrequency, 0.01f);

					tree->ReplaceModel();

					ImGui::TreePop();
				}
			}

			RE::BSTSmartPointer<RE::BSAnimationGraphManager> manager;
			target->GetAnimationGraphManager(manager);
			if (manager != nullptr)
			{
				if (PushingCollapsingHeader("Behavior"))
				{
					if (PushingCollapsingHeader("Variables"))
					{
						static std::string filter;
						ImGui::InputText("Filter", &filter);

						std::map<std::string, std::pair<RE::BShkbAnimationGraph*, int>> variables;
						for (const auto& graph : manager->graphs)
						{
							for (const auto& [name, index] : graph->projectDBData->variables)
							{
								if (filter.empty() ||
									fts::fuzzy_match_simple(filter.c_str(), name.c_str()))
								{
									variables[name.c_str()] = { graph.get(), index };
								}
							}
						}
						for (const auto& [name, varData] : variables)
						{
							const auto& [graph, index] = varData;
							const auto varType =
								graph->behaviorGraph->data->variableInfos[index].m_type;
							const auto& variableValues = graph->behaviorGraph->variableValueSet;
							if (varType.get() ==
								RE::hkbVariableInfo::VariableType::VARIABLE_TYPE_BOOL)
							{
								auto& value = variableValues->m_wordVariableValues[index].b;
								ImGui::Checkbox(name.data(), &value);
							}
							else if (varType.get() ==
									 RE::hkbVariableInfo::VariableType::VARIABLE_TYPE_INT32)
							{
								auto& value = variableValues->m_wordVariableValues[index].i;
								ImGui::DragInt(name.data(), &value);
							}
							else if (varType.get() ==
									 RE::hkbVariableInfo::VariableType::VARIABLE_TYPE_REAL)
							{
								auto& value = variableValues->m_wordVariableValues[index].f;
								ImGui::DragFloat(name.data(), &value);
							}
							else if (varType.get() ==
										 RE::hkbVariableInfo::VariableType::VARIABLE_TYPE_VECTOR4 ||
									 varType.get() ==
										 RE::hkbVariableInfo::VariableType::VARIABLE_TYPE_QUATERNION)
							{
								const auto quadIndex =
									variableValues->m_wordVariableValues[index].i;
								auto& vector =
									variableValues->m_quadVariableValues[quadIndex].quad.m128_f32;
								ImGui::DragFloat4(name.data(), vector);
							}
						}
						ImGui::TreePop();
					}

					if (PushingCollapsingHeader("Event Simulator"))
					{
						static std::string filter;
						ImGui::InputText("Filter", &filter);

						std::set<std::string> events;
						for (const auto& graph : manager->graphs)
						{
							for (const auto& [name, index] : graph->projectDBData->events)
							{
								if (filter.empty() || fts::fuzzy_match_simple(filter.c_str(), name.c_str()))
								{
									events.insert(name.c_str());
								}
							}
						}
						if (ImGui::BeginTable("EventsTable", 2))
						{
							for (const auto& name : events)
							{
								ImGui::TableNextRow();
								ImGui::TableNextColumn();
								ImGui::Text(name.c_str());
								ImGui::TableNextColumn();
								if (ImGui::Button(("Send##" + name).c_str()))
								{
									target->NotifyAnimationGraph(name.c_str());
								}
							}
							ImGui::EndTable();
						}
						ImGui::TreePop();
					}

					if (PushingCollapsingHeader("Graph Tracking"))
					{
						GraphTracker& tracker = GraphTracker::Instance();

						bool enableTracking = tracker.GetEnableTracking();
						if (ImGui::Checkbox("Tracking", &enableTracking))
						{
							tracker.SetEnableTracking(enableTracking);
						}

						if (enableTracking)
						{
							bool enableLogging = tracker.GetEnableLogging();
							if (ImGui::Checkbox("Logging", &enableLogging))
							{
								tracker.SetEnableTracking(enableLogging);
							}

							for (const auto& [value, name] : magic_enum::enum_entries<GraphTracker::EventType>())
							{
								if (value == GraphTracker::EventType::eAll)
								{
									continue;
								}
								bool isTracked =
									(static_cast<uint32_t>(value) &
										static_cast<uint32_t>(tracker.GetEventTypeFilter()));
								if (ImGui::Checkbox(std::format("Track {}", name).c_str(),
										&isTracked))
								{
									tracker.SetEventTypeFilter(static_cast<GraphTracker::EventType>(
										isTracked ? (static_cast<uint32_t>(value) |
														static_cast<uint32_t>(
															tracker.GetEventTypeFilter())) :
													(~static_cast<uint32_t>(value) &
														static_cast<uint32_t>(
															tracker.GetEventTypeFilter()))));
								}
							}

							ImGui::BeginListBox("##RecordedEvents");
							constexpr size_t maxShownEvents = 1000;

							const auto& recordedEvents = tracker.GetRecordedEvents();
							size_t lineCount = 0;
							for (auto it = recordedEvents.rbegin(); it != recordedEvents.rend();
								 ++it)
							{
								if (lineCount > maxShownEvents)
								{
									break;
								}
								ImGui::Text(it->ToString().c_str());
								++lineCount;
							}
							ImGui::EndListBox();
						}

						ImGui::TreePop();
					}

					if (PushingCollapsingHeader("Graph"))
					{
						STargetEditor::GraphViewer(*manager);
						ImGui::TreePop();
					}

					FootIkEditor("##FootIkEditor",
						manager->graphs[manager->activeGraph]->characterInstance);
					ImGui::TreePop();
				}
			}
		}
		else
		{
			ImGui::Text("Target is not selected");
		}
		ImGui::PopID();
    }
}
