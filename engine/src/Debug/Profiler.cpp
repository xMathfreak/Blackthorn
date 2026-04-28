#include "Debug/Profiler.h"

#include <algorithm>
#include <numeric>

#include <SDL3/SDL_timer.h>

#include "Debug/Logger.h"

namespace Blackthorn::Debug {

Profiler::Profiler()
	: frequency(static_cast<float>(SDL_GetPerformanceFrequency()))
{}

Profiler& Profiler::instance() {
	static Profiler profiler;
	return profiler;
}

std::vector<Profiler::ScopeEntry>& Profiler::threadScopeStack() {
	thread_local std::vector<ScopeEntry> stack;
	return stack;
}

void Profiler::beginFrame() {
	if (!enabled)
		return;

	frameStartTime = SDL_GetPerformanceCounter();

	std::lock_guard<std::mutex> lock(historyMutex);
	lastFrameSamples.clear();
}

void Profiler::endFrame() {
	if (!enabled)
		return;

	const U64 frameEnd = SDL_GetPerformanceCounter();
	lastFrameTime = static_cast<float>(frameEnd - frameStartTime) / frequency * 1000.0f;

	std::lock_guard<std::mutex> lock(historyMutex);
	frameTimeHistory.push_back(lastFrameTime);

	if (static_cast<int>(frameTimeHistory.size()) > maxHistoryFrames)
		frameTimeHistory.pop_front();
}

void Profiler::beginScope(const char* name) {
	if (!enabled)
		return;

	auto& stack = threadScopeStack();
	stack.push_back({
		name,
		SDL_GetPerformanceCounter(),
		static_cast<int>(stack.size())
	});
}

void Profiler::endScope(const char* name) {
	if (!enabled)
		return;

	const U64 endTime = SDL_GetPerformanceCounter();

	auto& stack = threadScopeStack();
	if (stack.empty()) {
		BT_WARN("Profiler: endScope('{}') called with empty stack", name);
		return;
	}

	const ScopeEntry& entry = stack.back();

	// Validate nesting - mismatched names indicate incorrect RAII usage.
	if (std::string_view(entry.name) != std::string_view(name)) {
		BT_WARN(
			"Profiler: scope mismatch - expected '{}', got '{}'",
			entry.name, name
		);
	}

	const float duration = static_cast<float>(endTime - entry.startTime) / frequency;

	Sample sample;
	sample.name = entry.name;
	sample.startTime = entry.startTime;
	sample.endTime = endTime;
	sample.duration = duration;
	sample.depth = entry.depth;

	stack.pop_back();

	// Write into shared history - this is the only place we lock.
	{
		std::lock_guard<std::mutex> lock(historyMutex);

		auto& history = scopeHistory[sample.name];
		history.push_back(duration);
		if (static_cast<int>(history.size()) > maxHistoryFrames)
			history.pop_front();

		// Only accumulate into lastFrameSamples for the main thread.
		// Worker scopes show up in scopeHistory but not in the per-frame list,
		// keeping the frame profiler readable without per-worker noise.
		// If you want worker samples in the frame list, remove this check.
		if (entry.depth >= 0)
			lastFrameSamples.push_back(sample);
	}
}

Profiler::ScopeStats Profiler::getStats(const std::string& name, int frameCount) const {
	ScopeStats stats;
	std::lock_guard<std::mutex> lock(historyMutex);

	auto it = scopeHistory.find(name);
	if (it == scopeHistory.end() || it->second.empty())
		return stats;

	const auto& history = it->second;
	const int count = std::min(frameCount, static_cast<int>(history.size()));
	if (count == 0)
		return stats;

	auto startIt = history.end() - count;
	auto endIt= history.end();

	stats.callCount = count;
	stats.total = std::accumulate(startIt, endIt, 0.0f);
	stats.average = stats.total / count;
	stats.min = *std::min_element(startIt, endIt);
	stats.max = *std::max_element(startIt, endIt);

	return stats;
}

std::vector<Profiler::Sample> Profiler::getLastFrameSamples() const {
	std::lock_guard<std::mutex> lock(historyMutex);
	return lastFrameSamples;
}

std::vector<std::string> Profiler::getAllScopeNames() const {
	std::lock_guard<std::mutex> lock(historyMutex);
	std::vector<std::string> names;
	names.reserve(scopeHistory.size());

	for (const auto& [name, _] : scopeHistory)
		names.push_back(name);

	std::sort(names.begin(), names.end());
	return names;
}

float Profiler::getLastFrameTime() const {
	return lastFrameTime;
}

float Profiler::getAverageFrameTime(int frameCount) const {
	std::lock_guard<std::mutex> lock(historyMutex);

	if (frameTimeHistory.empty())
		return 0.0f;

	const int count = std::min(frameCount, static_cast<int>(frameTimeHistory.size()));

	if (count == 0)
		return 0.0f;

	auto startIt = frameTimeHistory.end() - count;
	return std::accumulate(startIt, frameTimeHistory.end(), 0.0f) / count;
}

void Profiler::clear() {
	std::lock_guard<std::mutex> lock(historyMutex);
	scopeHistory.clear();
	frameTimeHistory.clear();
	lastFrameSamples.clear();
	lastFrameTime = 0.0f;
}

Profiler::ProfileScope::ProfileScope(const char* scopeName)
	: name(scopeName)
{
	Profiler::instance().beginScope(name);
}

Profiler::ProfileScope::~ProfileScope() {
	Profiler::instance().endScope(name);
}

} // namespace Blackthorn::Debug