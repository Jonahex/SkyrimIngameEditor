#include "Gui/TargetEditor.h"

#include "Gui/FootIkEditor.h"
#include "Gui/Utils.h"
#include "Gui/WaterEditor.h"
#include "Utils/Engine.h"

#include <RE/B/BSAnimationGraphManager.h>
#include <RE/B/BShkbAnimationGraph.h>
#include <RE/C/Console.h>
#include <RE/H/hkbBehaviorGraph.h>
#include <RE/H/hkbBehaviorGraphData.h>
#include <RE/H/hkbVariableInfo.h>
#include <RE/H/hkbVariableValueSet.h>
#include <RE/T/TESObjectACTI.h>
#include <RE/T/TESWaterForm.h>

#include <imgui.h>

namespace SIE
{
	void TargetEditor(const char* label)
    {
		ImGui::PushID(label);

		const auto target = RE::Console::GetSelectedRef();
		if (target != nullptr)
		{
			const auto base = target->GetBaseObject();

			ImGui::Text(std::format("Target: {}", GetTypedName(*target)).c_str());
			ImGui::Text(std::format("Base: {}", GetTypedName(*base)).c_str());

			if (target->formType == RE::FormType::ActorCharacter)
			{
				if (PushingCollapsingHeader("ActorState"))
				{
					const RE::Actor* targetActor = static_cast<RE::Actor*>(target.get());
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
				if (PushingCollapsingHeader("Graph variables"))
				{
					for (const auto& graph : manager->graphs)
					{
						std::map<std::string, int> variables;
						for (const auto& [name, index] : graph->projectDBData->variables)
						{
							variables[name.c_str()] = index;
						}
						for (const auto& [name, index] : variables)
						{
							const auto varType =
								graph->behaviorGraph->data->variableInfos[index].m_type;
							const auto& variableValues = graph->behaviorGraph->variableValueSet;
							if (varType.get() ==
								RE::hkbVariableInfo::VariableType::VARIABLE_TYPE_BOOL)
							{
								const auto value = variableValues->m_wordVariableValues[index].b;
								ImGui::Text(std::format("{} = {}", name.data(), value).c_str());
							}
							else if (varType.get() ==
									 RE::hkbVariableInfo::VariableType::VARIABLE_TYPE_INT32)
							{
								const auto value = variableValues->m_wordVariableValues[index].i;
								ImGui::Text(std::format("{} = {}", name.data(), value).c_str());
							}
							else if (varType.get() ==
									 RE::hkbVariableInfo::VariableType::VARIABLE_TYPE_REAL)
							{
								const auto value = variableValues->m_wordVariableValues[index].f;
								ImGui::Text(std::format("{} = {}", name.data(), value).c_str());
							}
							else if (varType.get() ==
									 RE::hkbVariableInfo::VariableType::VARIABLE_TYPE_VECTOR4)
							{
								const auto quadIndex =
									variableValues->m_wordVariableValues[index].i;
								const auto& vector =
									variableValues->m_quadVariableValues[quadIndex].quad.m128_f32;
								ImGui::Text(std::format("{} = [{}, {}, {}, {}]", name.data(),
									vector[0], vector[1], vector[2], vector[3])
												.c_str());
							}
						}
					}
					ImGui::TreePop();
				}

				FootIkEditor("##FootIkEditor",
					manager->graphs[manager->activeGraph]->characterInstance);
			}
		}
		else
		{
			ImGui::Text("Target is not selected");
		}
		ImGui::PopID();
    }
}
