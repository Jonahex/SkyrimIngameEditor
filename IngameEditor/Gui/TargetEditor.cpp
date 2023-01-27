#include "Gui/TargetEditor.h"

#include "Gui/FootIkEditor.h"
#include "Gui/NiObjectEditor.h"
#include "Gui/NiTransformEditor.h"
#include "Gui/Utils.h"
#include "Gui/WaterEditor.h"
#include "Utils/Engine.h"
#include "Utils/GraphTracker.h"
#include "Utils/TargetManager.h"

#define FTS_FUZZY_MATCH_IMPLEMENTATION
#include "3rdparty/fts_fuzzy_match.h"
#include "3rdparty/ImGuizmo/ImGuizmo.h"

#include <RE/A/ActorValueList.h>
#include <RE/A/AnimationSetDataSingleton.h>
#include <RE/B/BSAnimationGraphManager.h>
#include <RE/B/BShkbAnimationGraph.h>
#include <RE/H/hkbBehaviorGraph.h>
#include <RE/H/hkbBehaviorGraphData.h>
#include <RE/H/hkbSymbolIdMap.h>
#include <RE/H/hkbVariableInfo.h>
#include <RE/H/hkbVariableValueSet.h>
#include <RE/H/hkClass.h>
#include <RE/N/NiNode.h>
#include <RE/N/NiRTTI.h>
#include <RE/T/TESObjectACTI.h>
#include <RE/T/TESObjectTREE.h>
#include <RE/T/TESWaterForm.h>

#include <imgui.h>
#include <imgui_stdlib.h>

namespace SIE
{
	namespace STargetEditor
	{
		void AnimationSetDataViewer(const RE::BSAnimationGraphManager& graphManager)
		{ 
			const auto& activeGraph = graphManager.graphs[graphManager.activeGraph];
			const auto& projectName = activeGraph->projectName;
			const auto sets = RE::AnimationSetDataSingleton::GetSingleton();
			for (const auto& [name, sets] : sets->projects)
			{
				if (PushingCollapsingHeader(name.c_str()))
				{
					uint32_t setIndex = 0;
					for (const auto& set : *sets)
					{
						if (PushingCollapsingHeader(std::to_string(setIndex).c_str()))
						{
							if (PushingCollapsingHeader("Events"))
							{
								for (const auto& event : set.events)
								{
									ImGui::Text(event.c_str());
								}
								ImGui::TreePop();
							}
							if (PushingCollapsingHeader("Variables"))
							{
								for (const auto& variable : set.variables)
								{
									ImGui::Text(std::format("{}: [{}, {}]",
										variable.variable.c_str(), variable.min, variable.max)
													.c_str());
								}
								ImGui::TreePop();
							}
							if (PushingCollapsingHeader("Attacks"))
							{
								for (const auto& attack : set.attacks)
								{
									if (PushingCollapsingHeader(std::format("{}: {}", attack.eventName.c_str(), attack.unk10).c_str()))
									{
										if (attack.clipNames != nullptr)
										{
											for (const auto& clip : *attack.clipNames)
											{
												ImGui::Text(clip.c_str());
											}
										}
										ImGui::TreePop();
									}
								}
								ImGui::TreePop();
							}
							if (PushingCollapsingHeader("Hashes"))
							{
								for (uint32_t folderIndex = 0;
									 folderIndex < set.hashes.folderHashes.size(); ++folderIndex)
								{
									if (PushingCollapsingHeader(
											std::to_string(set.hashes.folderHashes[folderIndex])
												.c_str()))
									{
										for (const auto& nameHash :
											set.hashes.nameHashes[folderIndex])
										{
											ImGui::Text(std::to_string(nameHash).c_str());
										}
										ImGui::TreePop();
									}
								}
								ImGui::TreePop();
							}
							ImGui::TreePop();
						}
						++setIndex;
					}
					ImGui::TreePop();
				}
			}
		}

		void PrintClassName(const RE::hkRefVariant& variant, const std::string_view& name)
		{ 
			if (variant != nullptr)
			{
				if (auto cl = variant->GetClassType())
				{
					logger::info("{}: {}", name, cl->m_name);
				}
			}
		}

		void GraphViewer(const RE::BSAnimationGraphManager& graphManager) 
		{
			const auto& activeGraph = graphManager.graphs[graphManager.activeGraph];
			ImGui::Text(activeGraph->behaviorGraph->name.c_str());
			if (PushingCollapsingHeader("Active Nodes"))
			{
				const auto graph = activeGraph->behaviorGraph;
				if (graph->activeNodeTemplateToIndexMap != nullptr)
				{
					auto it = graph->activeNodeTemplateToIndexMap->getIterator();
					while (graph->activeNodeTemplateToIndexMap->isValid(it))
					{
						const auto& key = graph->activeNodeTemplateToIndexMap->getKey(it);
						const auto& value = graph->activeNodeTemplateToIndexMap->getValue(it);
						ImGui::Text(std::format("{} {}", key->name.c_str(), value)
										.c_str());
						it = graph->activeNodeTemplateToIndexMap->getNext(it);
					}
				}
				ImGui::TreePop();
			}
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
					target->Enable();
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
									 RE::hkbVariableInfo::VariableType::VARIABLE_TYPE_VECTOR4)
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

							ImGui::ListBoxHeader("##RecordedEvents");
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
							ImGui::ListBoxFooter();
						}

						ImGui::TreePop();
					}

					if (PushingCollapsingHeader("Animation Sets"))
					{
						STargetEditor::AnimationSetDataViewer(*manager);
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
