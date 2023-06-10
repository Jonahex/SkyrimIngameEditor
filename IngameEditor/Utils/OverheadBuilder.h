#pragma once

namespace SIE
{
	class OverheadBuilder
	{
	public:
		static OverheadBuilder& Instance();

		void Start();
		void Process(std::chrono::microseconds deltaTime);
		void Finish();

		bool IsRunning() const;

		bool isFirst = true;
		bool ortho = false;
		float nearPlane = 0.1f;
		float farPlane = 10000.f;
		float left = 1.f;
		float right = 1.f;
		float top = 1.f;
		float bottom = 1.f;

		int16_t minX = -55;
		int16_t minY = -65;
		int16_t maxX = 50;
		int16_t maxY = 25;

		int16_t halfGrid = 4;
		float shootingTime = 12.f;

	private:
		OverheadBuilder() = default;

		bool isRunning = false;

		std::chrono::microseconds timeToSkip = 1s;

		int16_t currentX;
		int16_t currentY;
		std::string nextName;
		std::chrono::high_resolution_clock::time_point startWait;

		bool firstFrame = true;
		bool needFinish = false;
	};
}
