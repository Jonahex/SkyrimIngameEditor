#pragma once

#include <RE/B/BSTEvent.h>
#include <REL/Relocation.h>

#include <chrono>
#include <mutex>
#include <stack>
#include <string>

namespace RE
{
	class BGSActionData;
	class IAnimationGraphManagerHolder;
	class MovementControllerNPC;
	class TESObjectREFR;

	struct BSAnimationGraphEvent;
}

namespace SIE
{
	class GraphTracker
	{
	public:
		enum class EventType
		{
			eEventSent = 1 << 0,
			eEventReceived = 1 << 1,
			eActionProcessed = 1 << 2,
			eActionProcessFailed = 1 << 3,
			eMovementMessageProcessed = 1 << 4,

			eAll = eEventSent | eEventReceived | eActionProcessed | eActionProcessFailed |
			       eMovementMessageProcessed,
		};

		struct Event
		{
			EventType type = EventType::eEventSent;
			std::string name;
			std::chrono::time_point<std::chrono::system_clock> time;

			std::string ToString() const;
		};

		static GraphTracker& Instance();

		bool GetEnableTracking() const;
		void SetEnableTracking(bool value);
		bool GetEnableLogging() const;
		void SetEnableLogging(bool value);
		void SetEventTypeFilter(EventType filter);
		EventType GetEventTypeFilter() const;
		void SetTarget(RE::TESObjectREFR* aTarget);

		const std::vector<Event>& GetRecordedEvents() const;

	private:
		GraphTracker();

		struct TESObjectREFR_ProcessEvent
		{
			static RE::BSEventNotifyControl thunk(
				RE::BSTEventSink<RE::BSAnimationGraphEvent>* sink,
				const RE::BSAnimationGraphEvent* event,
				RE::BSTEventSource<RE::BSAnimationGraphEvent>* eventSource);

			static inline REL::Relocation<decltype(thunk)> func;
			static constexpr size_t idx = 0x1;
		};

		struct TESObjectREFR_NotifyAnimationGraph
		{
			static bool thunk(RE::IAnimationGraphManagerHolder* holder,
				const RE::BSFixedString& eventName);

			static inline REL::Relocation<decltype(thunk)> func;
			static constexpr size_t idx = 0x1;
		};

		struct ActorMediator_Process
		{
			static bool thunk(void* mediator, RE::BGSActionData* actionData);

			static inline REL::Relocation<decltype(thunk)> func;
		};

		struct MovementControllerNPC_OnMessage
		{
			static void thunk(RE::MovementControllerNPC* controller, void* message);

			static inline REL::Relocation<decltype(thunk)> func;
			static constexpr size_t idx = 0x14;
		};

		static inline bool EnableTracking = false;
		static inline bool EnableLogging = true;
		static inline EventType EventTypeFilter = EventType::eAll;

		static inline RE::TESObjectREFR* Target = nullptr;
		static inline std::mutex Mutex;
		static inline std::vector<Event> Events;
	};
}
