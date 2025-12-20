// Timer.cpp
#include "Timer.h"

Timer::Timer()
{
  m_StartTime = std::chrono::high_resolution_clock::now();
  m_PreviousTime = m_StartTime;
}

void Timer::Tick()
{
  auto currentTime = std::chrono::high_resolution_clock::now();

  // Вычисляем дельта-время в секундах
  auto deltaTime = std::chrono::duration<float>(currentTime - m_PreviousTime);
  m_DeltaTime = deltaTime.count();

  // Вычисляем общее время
  auto totalTime = std::chrono::duration<float>(currentTime - m_StartTime);
  m_TotalTime = totalTime.count();

  m_PreviousTime = currentTime;
}
