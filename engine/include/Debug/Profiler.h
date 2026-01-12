#pragma once

#include <deque>
#include <string>
#include <unordered_map>
#include <vector>

#include <SDL3/SDL.h>

namespace Blackthorn::Debug {

class Profiler {
public:
	struct Sample {
		std::string name;
		float duration;
		int depth;
		Uint64 startTime;
		Uint64 endTime;
	};

	struct ScopeStats {
		float average;
		float min;
		float max;
		float total;
		int callCount;
	};

	class ProfileScope {
	public:
		ProfileScope(const char* name);
		~ProfileScope();

	private:
		const char* name;
		Uint64 startTime;
		int parentDepth;
	};

	static Profiler& instance();

	void beginFrame();
	void endFrame();

	void beginScope(const char* name);
	void endScope(const char* name);

	const std::vector<Sample>& getLastFrameSamples() const { return lastFrameSamples; }

	ScopeStats getStats(const std::string& name, int frameCount = 60) const;

	std::vector<std::string> getAllScopeNames() const;

	float getLastFrameTime() const { return lastFrameTime; }
	float getAverageFrameTime(int frameCount = 60) const;

	void clear();

	void setEnabled(bool isEnabled) { this->enabled = isEnabled; }
	bool isEnabled() const { return enabled; }

private:
	Profiler();
	~Profiler() = default;

	Profiler(const Profiler&) = delete;
	Profiler& operator=(const Profiler&) = delete;

	struct ScopeEntry {
		const char* name;
		Uint64 startTime;
		int depth;
	};

	std::vector<ScopeEntry> scopeStack;
	std::vector<Sample> currentFrameSamples;
	std::vector<Sample> lastFrameSamples;

	std::unordered_map<std::string, std::deque<float>> scopeHistory;
	std::deque<float> frameTimeHistory;

	Uint64 frameStartTime;
	float lastFrameTime;

	bool enabled;
	int maxHistoryFrames;

	float frequency;
};

} // namespace Blackthorn::Debug

#ifdef BLACKTHORN_DEBUG
	#define PROFILE_SCOPE(name) \
		do { \
			Blackthorn::Debug::Profiler::ProfileScope _profile_scope(name); \
		} while (0)
	#define PROFILE_FUNCTION() PROFILE_SCOPE(__FUNCTION__)
#else
	#define PROFILE_SCOPE(name) do {} while (0)
	#define PROFILE_FUNCTION() do {} while (0)
#endif
