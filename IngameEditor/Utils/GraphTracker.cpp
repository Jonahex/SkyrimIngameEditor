#include "Utils/GraphTracker.h"

#include "Utils/Engine.h"

#include <RE/B/BSAnimationGraphEvent.h>

namespace SIE
{
	std::string GraphTracker::Event::ToString() const 
	{ 
		return std::format("{:%H:%M:%S} Event {} was {} graph",
			std::chrono::floor<std::chrono::milliseconds>(time), name,
			type == EventType::eEventReceived ? "received from" : "sent to");
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
	}

	RE::BSEventNotifyControl GraphTracker::TESObjectREFR_ProcessEvent::thunk(
		RE::BSTEventSink<RE::BSAnimationGraphEvent>* sink, const RE::BSAnimationGraphEvent* event,
		RE::BSTEventSource<RE::BSAnimationGraphEvent>* eventSource)
	{
		if (EnableTracking && Target == reinterpret_cast<RE::TESObjectREFR*>(
								  (reinterpret_cast<std::ptrdiff_t>(sink) - 0x30)))
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
								  (reinterpret_cast<std::ptrdiff_t>(holder) - 0x38)))
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
}
