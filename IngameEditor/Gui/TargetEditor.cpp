#include "Gui/TargetEditor.h"

#include "Gui/FootIkEditor.h"
#include "Gui/Utils.h"
#include "Gui/WaterEditor.h"
#include "Utils/Engine.h"
#include "Utils/GraphTracker.h"
#include "Utils/TargetManager.h"

#define FTS_FUZZY_MATCH_IMPLEMENTATION
#include "3rdparty/fts_fuzzy_match.h"

#include <RE/B/BSAnimationGraphManager.h>
#include <RE/B/BShkbAnimationGraph.h>
#include <RE/H/hkbBehaviorGraph.h>
#include <RE/H/hkbBehaviorGraphData.h>
#include <RE/H/hkbVariableInfo.h>
#include <RE/H/hkbVariableValueSet.h>
#include <RE/T/TESObjectACTI.h>
#include <RE/T/TESWaterForm.h>

#include <imgui.h>
#include <imgui_stdlib.h>

namespace SIE
{
	void TargetEditor(const char* label)
    {
		ImGui::PushID(label);

		const auto target = TargetManager::Instance().GetTarget();
		if (target != nullptr)
		{
			const auto base = target->GetBaseObject();

			ImGui::Text(std::format("Target: {}", GetTypedName(*target)).c_str());
			ImGui::Text(std::format("Base: {}", GetTypedName(*base)).c_str());

			if (target->formType == RE::FormType::ActorCharacter)
			{
				if (PushingCollapsingHeader("ActorState"))
				{
					const RE::Actor* targetActor = static_cast<RE::Actor*>(target);
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
			}

			if (base->formType == RE::FormType::Activator)
			{
				if (target->IsWater())
				{
					const auto activator = static_cast<RE::TESObjectACTI*>(base);
					if (PushingCollapsingHeader("Water"))
					{
						FormEditor(activator,
							FormSelector<false>("Water Type", activator->waterForm));
						if (activator->waterForm != nullptr)
						{
							WaterEditor("WaterEditor", *activator->waterForm);
						}
						ImGui::TreePop();
					}
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
								if (ImGui::Button("Send"))
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
