#pragma once

#include <deque>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "Core/Export.h"
#include "Core/Types/Numeric.h"

namespace Blackthorn::Debug {

/**
 * @brief Thread-safe hierarchical CPU profiler.
 *
 * @details
 * Profiling scopes are tracked independently on each thread. The per-thread
 * scopeStack is thread_local, so beginScope() and endScope() acquire no
 * locks on the hot path.
 *
 * Completed scopes are accumulated in the shared scopeHistory, protected by
 * historyMutex. frameTimeHistory is protected by the same mutex. Only
 * endScope() writes to these histories, while getStats() reads them.
 *
 * beginFrame() and endFrame() are expected to be called from the main thread.
 * The resulting lastFrameSamples are protected by historyMutex and contain
 * only samples from the main thread.
 *
 * Worker-thread scopes are accumulated into scopeHistory automatically, but
 * are not included in lastFrameSamples.
 */
class BLACKTHORN_API Profiler {
public:
	struct Sample {
		std::string name;
		float duration; // seconds
		int depth;
		U64 startTime;
		U64 endTime;
	};

	struct ScopeStats {
		float average = 0.0f;
		float min = 0.0f;
		float max = 0.0f;
		float total = 0.0f;
		int callCount = 0;
	};

	// RAII scope guard
	class BLACKTHORN_API ProfileScope {
	public:
		explicit ProfileScope(const char* name);
		~ProfileScope();
	private:
		const char* name;
	};

	static Profiler& instance();

	// Frame boundary (main thread only)
	void beginFrame();
	void endFrame();

	// Scope markers (thread-safe)
	void beginScope(const char* name);
	void endScope(const char* name);

	/// Returns aggregated stats for named scope across recent frames.
	ScopeStats getStats(const std::string& name, int frameCount = 60) const;

	/// Snapshot of samples from the last completed main-thread frame.
	std::vector<Sample> getLastFrameSamples() const;

	std::vector<std::string> getAllScopeNames() const;

	float getLastFrameTime() const;
	float getAverageFrameTime(int frameCount = 60) const;

	void clear();
	void setEnabled(bool isEnabled) { enabled = isEnabled; }
	bool isEnabled() const { return enabled; }

private:
	Profiler();
	~Profiler() = default;

	Profiler(const Profiler&) = delete;
	Profiler& operator=(const Profiler&) = delete;

	/// Per-thread scope stack.
	struct ScopeEntry {
		const char* name;
		U64 startTime;
		int depth;
	};

	/// Returns the scope stack for the calling thread.
	static std::vector<ScopeEntry>& threadScopeStack();

	mutable std::mutex historyMutex;

	std::unordered_map<std::string, std::deque<float>> scopeHistory;
	std::deque<float> frameTimeHistory;
	std::vector<Sample> lastFrameSamples;

	/// Main-thread frame tracking
	U64 frameStartTime = 0;
	float lastFrameTime = 0.0f;

	bool enabled = true;
	int maxHistoryFrames = 120;
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