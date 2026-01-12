#include "Debug/Profiler.h"

#include <algorithm>
#include <numeric>

namespace Blackthorn::Debug {

Profiler::Profiler()
	: frameStartTime(0)
	, lastFrameTime(0.0f)
	, enabled(true)
	, maxHistoryFrames(120)
	, frequency(static_cast<float>(SDL_GetPerformanceFrequency()))
{}

Profiler& Profiler::instance() {
	static Profiler profiler;
	return profiler;
}

void Profiler::beginFrame() {
	if (!enabled)
		return;

	frameStartTime = SDL_GetPerformanceCounter();
	currentFrameSamples.clear();
	scopeStack.clear();
}

void Profiler::endFrame() {
	if (!enabled)
		return;

	Uint64 frameEndTime = SDL_GetPerformanceCounter();
	lastFrameTime = static_cast<float>(frameEndTime - frameStartTime) / frequency * 1000.0f;

	frameTimeHistory.push_back(lastFrameTime);
	if (frameTimeHistory.size() > static_cast<size_t>(maxHistoryFrames))
		frameTimeHistory.pop_front();

	lastFrameSamples = currentFrameSamples;

	for (const auto& sample : currentFrameSamples) {
		auto& history = scopeHistory[sample.name];
		history.push_back(sample.duration);

		if (history.size() > static_cast<size_t>(maxHistoryFrames))
			history.pop_front();
	}
}

void Profiler::beginScope(const char* name) {
	if (!enabled)
		return;

	ScopeEntry entry;
	entry.name = name;
	entry.startTime = SDL_GetPerformanceCounter();
	entry.depth = static_cast<int>(scopeStack.size());

	scopeStack.push_back(entry);
}

void Profiler::endScope(const char* name) {
	if (!enabled)
		return;

	if (scopeStack.empty()) {
		#ifdef BLACKTHORN_DEBUG
			SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Profiler: endScope called without matchin beginScope for %s", name);
		#endif

		return;
	}

	Uint64 endTime = SDL_GetPerformanceCounter();
	const ScopeEntry& entry = scopeStack.back();

	if (std::string(entry.name) != std::string(name))
		SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Profiler: Mismatched scope names. Expected '%s', got '%s'", entry.name, name);

	Sample sample;
	sample.name = entry.name;
	sample.startTime = entry.startTime;
	sample.endTime = endTime;
	sample.duration = static_cast<float>(endTime - entry.startTime) / frequency;
	sample.depth = entry.depth;

	currentFrameSamples.push_back(sample);
	scopeStack.pop_back();

}

Profiler::ScopeStats Profiler::getStats(const std::string& name, int frameCount) const {
	ScopeStats stats = {0.0f, 0.0f, 0.0f, 0.0f, 0};

	auto it = scopeHistory.find(name);
	if (it == scopeHistory.end() || it->second.empty())
		return stats;

	const auto& history = it->second;
	int count = std::min(frameCount, static_cast<int>(history.size()));

	if (count == 0)
		return stats;

	auto startIt = history.end() - count;
	auto endIt = history.end();

	stats.callCount = count;
	stats.total = std::accumulate(startIt, endIt, 0.0f);
	stats.average = stats.total / count;
	stats.min = *std::min_element(startIt, endIt);
	stats.max = *std::max_element(startIt, endIt);

	return stats;
}

std::vector<std::string> Profiler::getAllScopeNames() const {
	std::vector<std::string> names;
	names.reserve(scopeHistory.size());

	for (const auto& pair : scopeHistory)
		names.push_back(pair.first);

	std::sort(names.begin(), names.end());
	return names;
}

float Profiler::getAverageFrameTime(int frameCount) const {
	if (frameTimeHistory.empty())
		return 0.0f;

	int count = std::min(frameCount, static_cast<int>(frameTimeHistory.size()));
	if (count == 0)
		return 0.0f;

	auto startIt = frameTimeHistory.end() - count;
	auto endIt = frameTimeHistory.end();

	float total = std::accumulate(startIt, endIt, 0.0f);
	return total / count;
}

void Profiler::clear() {
	scopeHistory.clear();
	frameTimeHistory.clear();
	currentFrameSamples.clear();
	lastFrameSamples.clear();
	scopeStack.clear();
	lastFrameTime = 0.0f;
}

Profiler::ProfileScope::ProfileScope(const char* scopeName)
	: name(scopeName)
	, startTime(0)
	, parentDepth(0)
{
	Profiler& profiler = Profiler::instance();
	if (profiler.isEnabled())
		profiler.beginScope(name);
}

Profiler::ProfileScope::~ProfileScope() {
	Profiler& profiler = Profiler::instance();
	if (profiler.isEnabled())
		profiler.endScope(name);
}

} // namespace Blackthorn::Debug
