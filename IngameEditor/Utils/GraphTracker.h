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
			eEventSent,
			eEventReceived,
			eActionProcessed,
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

		static inline bool EnableTracking = false;
		static inline bool EnableLogging = true;

		static inline RE::TESObjectREFR* Target = nullptr;
		static inline std::mutex Mutex;
		static inline std::vector<Event> Events;
	};
}
