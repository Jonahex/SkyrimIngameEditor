#include "Utils/GraphTracker.h"

#include "Utils/Engine.h"
#include "Utils/RTTICache.h"

#include <RE/B/BGSActionData.h>
#include <RE/B/BSAnimationGraphEvent.h>
#include <RE/M/MovementControllerNPC.h>

#include <magic_enum/magic_enum.hpp>

namespace SIE
{
	std::string GraphTracker::Event::ToString() const 
	{ 
		return std::format("{:%H:%M:%S} {} {}",
			std::chrono::floor<std::chrono::milliseconds>(time), name,
			magic_enum::enum_name(type));
	}

	GraphTracker& GraphTracker::Instance()
	{ 
		static GraphTracker instance;
		return instance;
	}

	bool GraphTracker::GetEnableTracking() const
	{ 
		return EnableTracking;
	}
	
	void GraphTracker::SetEnableTracking(bool value) 
	{
		if (EnableTracking == value)
		{
			return;
		}

		if (EnableLogging && Target != nullptr)
		{
			if (EnableTracking && !value)
			{
				logger::info("Stopped tracking graph of reference {}", GetFullName(*Target));
			}
			else if (value && !EnableTracking)
			{
				logger::info("Started tracking graph of reference {}", GetFullName(*Target));
			}
		}

		EnableTracking = value;

		if (!EnableTracking)
		{
			Events.clear();
		}
	}

	bool GraphTracker::GetEnableLogging() const
	{ 
		return EnableLogging;
	}
	
	void GraphTracker::SetEnableLogging(bool value) 
	{ 
		EnableLogging = value;
	}

	void GraphTracker::SetEventTypeFilter(EventType filter)
	{
		EventTypeFilter = filter;
	}

	GraphTracker::EventType GraphTracker::GetEventTypeFilter() const
	{
		return EventTypeFilter;
	}

	void GraphTracker::SetTarget(RE::TESObjectREFR* aTarget)
	{
		if (aTarget == Target)
		{
			return;
		}

		if (EnableLogging)
		{
			if (Target != nullptr)
			{
				logger::info("Stopped tracking graph of reference {}", GetFullName(*Target));
			}
		}

		Target = aTarget;

		if (Target == nullptr)
		{
			Events.clear();
		}

		if (EnableLogging)
		{
			if (Target != nullptr)
			{
				logger::info("Started tracking graph of reference {}", GetFullName(*Target));
			}
		}
	}

	const std::vector<GraphTracker::Event>& GraphTracker::GetRecordedEvents() const 
	{
		return Events;
	}

	GraphTracker::GraphTracker() 
	{
		{
			stl::write_vfunc<RE::TESObjectREFR, 0x2, TESObjectREFR_ProcessEvent>();
			const std::array targets{
				REL::Relocation<std::uintptr_t>(RELOCATION_ID(36972, 37997), OFFSET(34, 34)),
			};
			for (const auto& target : targets)
			{
				stl::write_thunk_call<TESObjectREFR_ProcessEvent>(target.address());
			}
		}

		{
			stl::write_vfunc<RE::TESObjectREFR, 0x3, TESObjectREFR_NotifyAnimationGraph>();
			const std::array targets{
				REL::Relocation<std::uintptr_t>(RELOCATION_ID(37020, 38048), OFFSET(0, 0)),
			};
			for (const auto& target : targets)
			{
				stl::write_thunk_jmp<TESObjectREFR_NotifyAnimationGraph>(target.address());
			}
		}

		{
			const std::array targets{
				REL::Relocation<std::uintptr_t>(RELOCATION_ID(40551, 41557),
					OFFSET(0xA, 0xA)),
			};
			for (const auto& target : targets)
			{
				stl::write_thunk_jmp<ActorMediator_Process>(target.address());
			}
		}

		{
			const std::array targets{
				REL::Relocation<std::uintptr_t>(RELOCATION_ID(37997, 38951), OFFSET(0x36, 0x36)),
			};
			for (const auto& target : targets)
			{
				stl::write_thunk_call<ActorMediator_Process>(target.address());
			}
		}

		{
			stl::write_vfunc<MovementControllerNPC_OnMessage>(RE::VTABLE_MovementControllerNPC[0]);
		}
	}

	RE::BSEventNotifyControl GraphTracker::TESObjectREFR_ProcessEvent::thunk(
		RE::BSTEventSink<RE::BSAnimationGraphEvent>* sink, const RE::BSAnimationGraphEvent* event,
		RE::BSTEventSource<RE::BSAnimationGraphEvent>* eventSource)
	{
		if (EnableTracking && Target == reinterpret_cast<RE::TESObjectREFR*>(
						  (reinterpret_cast<std::ptrdiff_t>(sink) - 0x30)) &&
			(static_cast<uint32_t>(EventTypeFilter) &
				static_cast<uint32_t>(EventType::eEventReceived)) != 0)
		{
			std::lock_guard lock(Mutex);
			Events.push_back(
				{ EventType::eEventReceived, event->tag.data(), std::chrono::system_clock::now() });
			if (EnableLogging)
			{
				logger::info("Event {} was received from graph", event->tag.data());
			}
		}

		return func(sink, event, eventSource);
	}

	bool GraphTracker::TESObjectREFR_NotifyAnimationGraph::thunk(
		RE::IAnimationGraphManagerHolder* holder,
		const RE::BSFixedString& eventName)
	{
		if (EnableTracking && Target == reinterpret_cast<RE::TESObjectREFR*>(
						  (reinterpret_cast<std::ptrdiff_t>(holder) - 0x38)) &&
			(static_cast<uint32_t>(EventTypeFilter) &
				static_cast<uint32_t>(EventType::eEventSent)) != 0)
		{
			std::lock_guard lock(Mutex);
			Events.push_back(
				{ EventType::eEventSent, eventName.data(), std::chrono::system_clock::now() });
			if (EnableLogging)
			{
				logger::info("Event {} was sent to graph", eventName.data());
			}
		}

		return func(holder, eventName);
	}

	bool GraphTracker::ActorMediator_Process::thunk(void* mediator, RE::BGSActionData* actionData)
	{
		const bool processed = func(mediator, actionData);

		if (EnableTracking && Target == actionData->source.get())
		{
			std::lock_guard lock(Mutex);
			if (processed && (static_cast<uint32_t>(EventTypeFilter) &
								 static_cast<uint32_t>(EventType::eActionProcessed)) != 0)
			{
				Events.push_back({ EventType::eActionProcessed,
					actionData->action->formEditorID.data(), std::chrono::system_clock::now() });
				if (EnableLogging)
				{
					logger::info("Action {} was processed",
						actionData->action->formEditorID.data());
				}
			}
			else if ((static_cast<uint32_t>(EventTypeFilter) &
						 static_cast<uint32_t>(EventType::eActionProcessFailed)) != 0)
			{
				Events.push_back({ EventType::eActionProcessFailed,
					actionData->action->formEditorID.data(), std::chrono::system_clock::now() });
				if (EnableLogging)
				{
					logger::info("Action {} processing failed",
						actionData->action->formEditorID.data());
				}
			}
		}

		return processed;
	}

	void GraphTracker::MovementControllerNPC_OnMessage::thunk(RE::MovementControllerNPC* controller,
		void* message)
	{
		func(controller, message);

		if (controller != nullptr && message != nullptr && EnableTracking &&
			Target == controller->owner &&
			(static_cast<uint32_t>(EventTypeFilter) &
				static_cast<uint32_t>(EventType::eMovementMessageProcessed)) != 0)
		{
			std::lock_guard lock(Mutex);
			const auto& messageTypeName = RTTICache::Instance().GetRTTI(message).typeName;
			Events.push_back({ EventType::eMovementMessageProcessed, messageTypeName,
				std::chrono::system_clock::now() });
			if (EnableLogging)
			{
				logger::info("Movement message {} processed", messageTypeName);
			}
		}
	}
}
