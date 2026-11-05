#pragma once

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <exception>
#include <format>
#include <future>
#include <iostream>
#include <memory>
#include <ostream>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

/**
 * @brief Concatenates two tokens after they have been macro-expanded.
 */
#define BT_CONCAT_IMPL(a, b) a##b
#define BT_CONCAT(a, b) BT_CONCAT_IMPL(a, b)

namespace {

template <typename... Args>
void print(std::format_string<Args...> fmt, Args&&... args) {
	std::string message = std::format(fmt, std::forward<Args>(args)...);
	std::cout << message;
}

}

namespace Blackthorn::Tests {

enum ExitCode {
	Success = 0,
	Failure = 1,
};

/**
 * @brief Exception thrown by BT_ASSERT to abort a test immediately upon failure.
 */
class AssertionFailure : public std::exception {
public:
	/**
	 * @brief Constructs the failure with a pre-formatted message.
	 * @param m Human-readable description of what failed and where.
	 */
	explicit AssertionFailure(std::string m) : message(std::move(m)) {}

	/**
	 * @brief Returns the failure message.
	 * @return const char* Null-terminated failure description.
	 */
	const char* what() const noexcept override { return message.c_str(); }

private:
	std::string message;
};

/**
 * @brief Exception thrown by BT_SKIP to mark the current test as skipped.
 *
 * A caught SkipException does not count towards the failure total.
 */
class SkipException : public std::exception {
public:
	/**
	 * @brief Constructs the skip with a human-readable reason.
	 * @param r Why the test was skipped.
	 */
	explicit SkipException(std::string r) : reason(std::move(r)) {}

	/**
	 * @brief Returns the skip reason.
	 * @return const char* Null-terminated reason string.
	 */
	const char* what() const noexcept override { return reason.c_str(); }

private:
	std::string reason;
};

/// @brief Function pointer type for a registered test.
using TestFunction = void (*)();

/**
 * @brief Metadata describing one registered test.
 */
struct TestCase {
	std::string name; ///< Display name shown in the report.
	TestFunction func; ///< Test body to invoke.
	const char* file; ///< Source file BT_TEST was called from.
	int line; ///< Source line BT_TEST was called from.
	double timeoutMs; ///< Per-test timeout override; negative means "use RunOptions::defaultTimeoutMs".
};

/**
 * @brief Outcome classification for a single test run.
 */
enum class Status {
	Passed, ///< Ran to completion with no failed assertions.
	Failed, ///< A BT_ASSERT_*/BT_EXPECT_* failed, or an exception escaped.
	Skipped, ///< The test called BT_SKIP.
	TimedOut, ///< The test exceeded its timeout; treated as a failure for counting purposes.
};

/**
 * @brief Per-test mutable state used while a test is executing.
 *
 * Tracks soft failures raised via BT_EXPECT so a test can keep
 * running after a non-fatal assertion fails, unlike BT_ASSERT
 * which throws and aborts the test immediately. Also tracks
 * whether BT_SKIP was called.
 */
struct TestContext {
	bool failed = false; ///< True if any BT_ASSERT/BT_EXPECT failed.
	std::vector<std::string> failureMessages; ///< One entry per failed assertion.
	bool skipped = false; ///< True if BT_SKIP was called.
	std::string skipReason; ///< Reason passed to BT_SKIP, if skipped.
};

/**
 * @brief Outcome of running a single test, produced by Blackthorn::Tests::runAll().
 */
struct TestResult {
	std::string name; ///< Test's display name (decorated with a run index if repeated).
	Status status; ///< Pass/fail/skip/timeout classification.
	double milliseconds; ///< Wall-clock duration of the test body.
	std::vector<std::string> messages; ///< Failure messages, or a single skip reason.
};

namespace Detail {

/**
 * @brief Returns the process-wide list of registered tests.
 *
 * @return std::vector<TestCase>& Mutable reference to the registry.
 */
inline std::vector<TestCase>& registry() {
	static std::vector<TestCase> tests;
	return tests;
}

/**
 * @brief Returns the TestContext of the test currently executing, if any.
 *
 * BT_ASSERT and BT_EXPECT read/write through this pointer to report
 * failures back to Blackthorn::Tests::runAll(). It is thread_local so tests could,
 * in principle, be run concurrently without interfering with one
 * another.
 *
 * @return TestContext*& Reference to the thread-local current-context pointer.
 */
inline TestContext*& currentContext() {
	thread_local TestContext* ctx = nullptr;
	return ctx;
}

/**
 * @brief Registers one TestCase as a side effect of static initialization.
 *
 * BT_TEST declares a single static Registrar per test; its
 * constructor runs before main() and appends the test to
 * Detail::registry().
 */
struct Registrar {
	/**
	 * @brief Appends a TestCase built from the given arguments to the registry.
	 * @param name Display name for the test.
	 * @param func Test body, signature `void()`.
	 * @param file Source file of the BT_TEST call (from __FILE__).
	 * @param line Source line of the BT_TEST call (from __LINE__).
	 * @param timeoutMs Per-test timeout override in milliseconds; negative
	 * (the default) means "use RunOptions::defaultTimeoutMs".
	 */
	Registrar(const std::string& name, TestFunction func, const char* file, int line, double timeoutMs = -1.0) {
		registry().push_back(TestCase{name, func, file, line, timeoutMs});
	}
};

/**
 * @brief Detects whether `std::ostream << T` is a valid expression.
 *
 * Used by toString() so comparison macros (BT_ASSERT_EQUAL, etc.)
 * can print actual operand values for streamable types and fall
 * back gracefully for types that have no operator<<.
 */
template <typename T, typename = void>
struct IsStreamable : std::false_type {};

/// @brief Specialization selected when `std::ostream << T` compiles.
template <typename T>
struct IsStreamable<T, std::void_t<decltype(std::declval<std::ostream&>() << std::declval<const T&>())>>
	: std::true_type {};

/**
 * @brief Renders @p value as a string for failure messages.
 *
 * Falls back to a placeholder for types without operator<<, so
 * comparison macros compile for any type, not just printable ones.
 *
 * @param value Value to render.
 * @return std::string Textual representation, or "<unprintable>".
 */
template <typename T>
std::string toString(const T& value) {
	if constexpr (IsStreamable<T>::value) {
		std::ostringstream oss;
		oss << value;
		return oss.str();
	} else {
		(void)value;
		return "<unprintable>";
	}
}

/**
 * @brief Milliseconds elapsed since @p start, measured with the steady clock.
 * @param start Start time, as previously captured with std::chrono::steady_clock::now().
 * @return double Elapsed duration in milliseconds.
 */
inline double elapsedMs(std::chrono::steady_clock::time_point start) {
	return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
}

/**
 * @brief Converts a finished TestContext into a TestResult.
 * @param name Test's display name.
 * @param ctx Context populated by BT_ASSERT/BT_EXPECT/BT_SKIP during the run.
 * @param ms Measured duration of the run, in milliseconds.
 * @return TestResult Status::Skipped if BT_SKIP fired, Status::Failed if any
 * assertion failed, Status::Passed otherwise.
 */
inline TestResult makeResult(const std::string& name, const TestContext& ctx, double ms) {
	TestResult result;
	result.name = name;
	result.milliseconds = ms;
	if (ctx.skipped) {
		result.status = Status::Skipped;
		result.messages.push_back(ctx.skipReason);
	} else if (ctx.failed) {
		result.status = Status::Failed;
		result.messages = ctx.failureMessages;
	} else {
		result.status = Status::Passed;
	}
	return result;
}

/**
 * @brief Runs one test, enforcing @p timeoutMs if positive, and returns its outcome.
 *
 * When @p timeoutMs is greater than zero, the test body runs on a
 * separate, detached thread so the caller can give up waiting after
 * the timeout elapses.
 *
 * A timed-out test's thread is left running
 * in the background for the lifetime of the process.
 *
 * The test's TestContext is heap-allocated (via shared_ptr) precisely
 * so that background thread can keep writing to it safely even
 * after this function has returned.
 *
 * @param test Test to run.
 * @param timeoutMs Timeout in milliseconds; zero or negative disables it.
 * @return TestResult Outcome of the run, or Status::TimedOut if it exceeded @p timeoutMs.
 */
inline TestResult runSingleTest(const TestCase& test, double timeoutMs) {
	auto ctx = std::make_shared<TestContext>();

	auto body = [&test, ctx]() {
		currentContext() = ctx.get();

		try {
			test.func();
		} catch (const SkipException& e) {
			ctx->skipped = true;
			ctx->skipReason = e.what();
		} catch (const AssertionFailure& e) {
			ctx->failed = true;
			ctx->failureMessages.emplace_back(e.what());
		} catch (const std::exception& e) {
			ctx->failed = true;
			ctx->failureMessages.emplace_back(std::string("unhandled exception: ") + e.what());
		} catch (...) {
			ctx->failed = true;
			ctx->failureMessages.emplace_back("unhandled non-standard exception");
		}

		currentContext() = nullptr;
	};

	const auto start = std::chrono::steady_clock::now();

	if (timeoutMs <= 0.0) {
		body();
		return makeResult(test.name, *ctx, elapsedMs(start));
	}

	auto donePromise = std::make_shared<std::promise<void>>();
	std::future<void> doneFuture = donePromise->get_future();

	std::thread worker([body, donePromise]() mutable {
		body();
		donePromise->set_value();
	});

	worker.detach();

	const std::future_status waitStatus =
		doneFuture.wait_for(std::chrono::duration<double, std::milli>(timeoutMs));
	const double ms = elapsedMs(start);

	if (waitStatus == std::future_status::timeout) {
		std::ostringstream oss;
		oss << "exceeded timeout of " << timeoutMs << " ms (its thread is still running in the background)";
		TestResult result;
		result.name = test.name;
		result.status = Status::TimedOut;
		result.milliseconds = ms;
		result.messages.push_back(oss.str());
		return result;
	}

	return makeResult(test.name, *ctx, ms);
}

/**
 * @brief Returns the short label printed in the RESULT column for @p status.
 * @param status Status to label.
 * @return const char* One of "PASS", "FAIL", "SKIP", "TIMEOUT".
 */
inline const char* statusLabel(Status status) {
	switch (status) {
		case Status::Passed:
			return "PASS";
		case Status::Failed:
			return "FAIL";
		case Status::Skipped:
			return "SKIP";
		case Status::TimedOut:
			return "TIMEOUT";
	}

	return "?";
}

} // namespace Detail

/**
 * @brief Configures how Blackthorn::Tests::runAll() orders, repeats, and time-limits tests.
 */
struct RunOptions {
	/// @brief Test execution order.
	enum class Order {
		Declared, ///< Run tests in the order BT_TEST/BT_TEST_INLINE registered them.
		Shuffled, ///< Run tests in a randomized order (reshuffled on every repeat pass).
	};

	Order order = Order::Declared; ///< Execution order for the whole suite.
	unsigned repeat = 1; ///< Number of times to run the entire suite (useful for catching flaky tests).
	unsigned shuffleSeed = 0; ///< Seed for Order::Shuffled; 0 means "pick a random seed and print it".
	double defaultTimeoutMs = 0.0; ///< Default per-test timeout in ms; 0 or negative disables it unless a test overrides it.
};

/**
 * @brief Runs every registered test according to @p options, prints a report, and returns the failure count.
 *
 * Each test is timed independently using std::chrono::steady_clock.
 * A Blackthorn::Tests::AssertionFailure escaping the test body (from BT_ASSERT or a
 * BT_ASSERT_* comparison) stops that test immediately and marks it
 * failed.
 * BT_EXPECT/BT_EXPECT_* failures are recorded but let the
 * test keep running.
 * A Blackthorn::Tests::SkipException (from BT_SKIP) marks the
 * test skipped instead.
 * A test that runs longer than its resolved
 * timeout (RunOptions::defaultTimeoutMs, or a per-test override
 * passed to BT_TEST/BT_TEST_INLINE_TIMEOUT) is reported as timed out
 * and counted as a failure.
 * @c Detail::runSingleTest for the background-thread caveat this implies.
 *
 * The report prints a compact PASS/FAIL/SKIP/TIMEOUT table first,
 * then a "Failures:" section listing every failure's messages, then
 * a "Skipped:" section listing skip reasons, then a one-line
 * numeric summary.
 *
 * @param options Ordering, repeat count, shuffle seed, and default timeout to use.
 * @return int Number of failed tests, including timeouts (0 means everything
 *             passed or was skipped). Suitable for returning directly from main().
 */
inline ExitCode runAll(const RunOptions& options) {
	std::vector<TestCase>& tests = Detail::registry();

	std::vector<const TestCase*> order;
	order.reserve(tests.size());

	for (const TestCase& test : tests)
		order.push_back(&test);

	unsigned seed = options.shuffleSeed;
	if (options.order == RunOptions::Order::Shuffled) {
		if (seed == 0)
			seed = std::random_device{}();

		print("shuffle seed: {} (rerun with --seed={} to reproduce this order)", seed, seed);
	}
	std::mt19937 rng(seed);

	const unsigned repeatCount = options.repeat == 0 ? 1 : options.repeat;

	std::vector<TestResult> results;
	results.reserve(tests.size() * repeatCount);

	int failedCount = 0;
	int skippedCount = 0;
	int timedOutCount = 0;
	double totalMs = 0.0;

	for (unsigned pass = 0; pass < repeatCount; ++pass) {
		if (options.order == RunOptions::Order::Shuffled)
			std::shuffle(order.begin(), order.end(), rng);

		for (const TestCase* test : order) {
			const double timeoutMs = (test->timeoutMs >= 0.0) ? test->timeoutMs : options.defaultTimeoutMs;

			TestResult result = Detail::runSingleTest(*test, timeoutMs);

			if (repeatCount > 1)
				result.name += " [run " + std::to_string(pass + 1) + "/" + std::to_string(repeatCount) + "]";

			totalMs += result.milliseconds;
			switch (result.status) {
				case Status::Failed:
					++failedCount;
					break;
				case Status::TimedOut:
					++failedCount;
					++timedOutCount;
					break;
				case Status::Skipped:
					++skippedCount;
					break;
				case Status::Passed:
					break;
			}

			results.push_back(std::move(result));
		}
	}

	print("{:<48} {:<8} {:>15}\n", "Name", "Result", "Time");
	print("{:-<73}\n", "");

	for (const TestResult& result : results)
		print("{:<48} {:<8} {:>12.3f} ms\n", result.name, Detail::statusLabel(result.status), result.milliseconds);

	if (failedCount > 0) {
		print("\nFailures:\n");

		for (const TestResult& result : results) {
			if (result.status == Status::Failed || result.status == Status::TimedOut) {
				print(" {}\n", result.name);

				for (const std::string& message : result.messages)
					print("\t-> {}\n", message);
			}
		}
	}

	if (skippedCount > 0) {
		print("\nSkipped:\n");

		for (const TestResult& result : results) {
			if (result.status == Status::Skipped) {
				const std::string& reason = result.messages.empty() ? std::string() : result.messages.front();
				if (reason.empty()) {
					print(" {}\n", result.name);
				} else {
					print(" {} -- {}\n", result.name, reason);
				}
			}
		}
	}

	const int passedCount = static_cast<int>(results.size()) - failedCount - skippedCount;
	print("\n{} tests, {} passed, {} failed", results.size(), passedCount, failedCount);

	if (timedOutCount > 0)
		print(" (%d timed out)", timedOutCount);

	print(", {} skipped, total time {:.3f} ms\n", skippedCount, totalMs);

	return failedCount == 0 ? ExitCode::Success : ExitCode::Failure;
}

/// @brief Runs every registered test with default RunOptions (declared order, no repeat, no timeout).
inline ExitCode runAll() {
	return runAll(RunOptions{});
}

namespace Detail {

/**
 * @brief Parses recognized CLI flags into @p options, leaving anything unset unchanged.
 *
 * Recognized flags: `--shuffle` (sets Order::Shuffled), `--seed=N`,
 * `--repeat=N`, `--timeout=MS`. Unrecognized arguments are ignored so
 * the binary's own argument parsing (if any) is unaffected.
 *
 * @param argc Argument count, as passed to main().
 * @param argv Argument vector, as passed to main().
 * @param options Base options; CLI flags override the corresponding field.
 * @return RunOptions Resulting options after applying CLI flags.
 */
inline RunOptions parseArgs(int argc, char** argv, RunOptions options) {
	for (int i = 1; i < argc; ++i) {
		const std::string arg = argv[i];
		if (arg == "--shuffle") {
			options.order = RunOptions::Order::Shuffled;
		} else if (arg.rfind("--seed=", 0) == 0) {
			options.shuffleSeed = static_cast<unsigned>(std::stoul(arg.substr(7)));
		} else if (arg.rfind("--repeat=", 0) == 0) {
			options.repeat = static_cast<unsigned>(std::stoul(arg.substr(9)));
		} else if (arg.rfind("--timeout=", 0) == 0) {
			options.defaultTimeoutMs = std::stod(arg.substr(10));
		}
	}

	return options;
}

} // namespace Detail

/**
 * @brief Runs every registered test, first applying options parsed from CLI flags.
 *
 * Recognized flags: `--shuffle`, `--seed=N`, `--repeat=N`, `--timeout=MS`.
 *
 * @param argc Argument count, as passed to main().
 * @param argv Argument vector, as passed to main().
 * @return int Number of failed tests; see runAll(const RunOptions&).
 */
inline int runAll(int argc, char** argv) {
	return runAll(Detail::parseArgs(argc, argv, RunOptions{}));
}

/**
 * @brief Runs every registered test with @p options as a base, overridden by any CLI flags present.
 * @param argc Argument count, as passed to main().
 * @param argv Argument vector, as passed to main().
 * @param options Base options; CLI flags override the corresponding field.
 * @return int Number of failed tests; see runAll(const RunOptions&).
 */
inline int runAll(int argc, char** argv, const RunOptions& options) {
	return runAll(Detail::parseArgs(argc, argv, options));
}

} // namespace Blackthorn::Tests

/// @cond BT_DETAIL_MACRO_DISPATCH
#define BT_TEST_2(name, function) \
	namespace { \
		static const Blackthorn::Tests::Detail::Registrar BT_CONCAT(bt_registrar_, __LINE__)( \
			(name), (function), __FILE__, __LINE__); \
	}

#define BT_TEST_3(name, function, timeoutMs) \
	namespace { \
		static const Blackthorn::Tests::Detail::Registrar BT_CONCAT(bt_registrar_, __LINE__)( \
			(name), (function), __FILE__, __LINE__, (timeoutMs)); \
	}

#define BT_GET_TEST_MACRO(_1, _2, _3, NAME, ...) NAME
/// @endcond

/**
 * @def BT_TEST(name, function, timeoutMs)
 * @brief Registers @p function under the display name @p name to be run by BT_RUN_ALL().
 *
 * @p function must already be declared with signature `void function()`.
 * Registration happens automatically before main() runs, via a static
 * Blackthorn::Tests::Detail::Registrar, so no manual list of tests needs to be
 * maintained anywhere. @p timeoutMs is optional; when omitted the test
 * uses RunOptions::defaultTimeoutMs (0 by default, meaning no timeout).
 *
 * @code
 * void MyAdditionTest() {
 *     BT_ASSERT(1 + 1 == 2);
 * }
 * BT_TEST("Addition works", MyAdditionTest);
 *
 * void MySlowTest() { ... }
 * BT_TEST("Slow test capped at 50ms", MySlowTest, 50.0);
 * @endcode
 */
#define BT_TEST(...) BT_GET_TEST_MACRO(__VA_ARGS__, BT_TEST_3, BT_TEST_2, BT_TEST_UNUSED)(__VA_ARGS__)

/**
 * @def BT_TEST_INLINE(name, body)
 * @brief Registers an anonymous test whose body is given inline as a brace-enclosed block.
 *
 * Unlike BT_TEST, no separate function needs to exist beforehand: a
 * uniquely-named static function is generated at the call site and
 * @p body becomes its body verbatim. @p body must be a `{ ... }` block
 * (it is captured through `...` / __VA_ARGS__, so commas inside the
 * block, e.g. `int a = 1, b = 2;`, are preserved correctly).
 *
 * @code
 *     BT_TEST_INLINE("Addition works", {
 *     BT_ASSERT_EQUAL(1 + 1, 2);
 * });
 * @endcode
 */
#define BT_TEST_INLINE(name, ...) \
	static void BT_CONCAT(bt_test_fn_, __LINE__)(); \
	namespace { \
		static const Blackthorn::Tests::Detail::Registrar BT_CONCAT(bt_registrar_, __LINE__)( \
			(name), &BT_CONCAT(bt_test_fn_, __LINE__), __FILE__, __LINE__); \
	} \
	static void BT_CONCAT(bt_test_fn_, __LINE__)() __VA_ARGS__

/**
 * @def BT_TEST_INLINE_TIMEOUT(name, timeoutMs, body)
 * @brief Like BT_TEST_INLINE, but with an explicit per-test timeout override.
 *
 * @p timeoutMs must come immediately after @p name, before the `{ ... }`
 * block, so the macro can tell it apart from body content that may
 * itself contain commas.
 *
 * @code
 *     BT_TEST_INLINE_TIMEOUT("Bounded work", 50.0, {
 *     BT_ASSERT(DoBoundedWork());
 * });
 * @endcode
 */
#define BT_TEST_INLINE_TIMEOUT(name, timeoutMs, ...) \
	static void BT_CONCAT(bt_test_fn_, __LINE__)(); \
	namespace { \
		static const Blackthorn::Tests::Detail::Registrar BT_CONCAT(bt_registrar_, __LINE__)( \
			(name), &BT_CONCAT(bt_test_fn_, __LINE__), __FILE__, __LINE__, (timeoutMs)); \
	} \
	static void BT_CONCAT(bt_test_fn_, __LINE__)() __VA_ARGS__

/**
 * @def BT_ASSERT(condition)
 * @brief Fatal assertion: if @p condition is false, aborts the current test immediately.
 *
 * Throws Blackthorn::Tests::AssertionFailure, which BT_RUN_ALL() catches and records.
 * Use this when continuing the test after failure would be meaningless
 * or unsafe (e.g. a null pointer that later lines would dereference).
 */
#define BT_ASSERT(condition) \
	do { \
		if (!(condition)) { \
			std::ostringstream bt_oss; \
			bt_oss << "BT_ASSERT(" #condition ") failed at " << __FILE__ << ":" << __LINE__; \
			throw Blackthorn::Tests::AssertionFailure(bt_oss.str()); \
		} \
	} while (false)

/**
 * @def BT_EXPECT(condition)
 * @brief Non-fatal assertion: if @p condition is false, marks the test failed but keeps running.
 *
 * Records a failure message against the currently-executing test's
 * Blackthorn::Tests::TestContext instead of throwing. Must be called from within a
 * function registered via BT_TEST; if no test is currently running the
 * check is silently skipped.
 */
#define BT_EXPECT(condition) \
	do { \
		if (!(condition)) { \
			Blackthorn::Tests::TestContext* bt_ctx = Blackthorn::Tests::Detail::currentContext(); \
			if (bt_ctx != nullptr) { \
				std::ostringstream bt_oss; \
				bt_oss << "BT_EXPECT(" #condition ") failed at " << __FILE__ << ":" << __LINE__; \
				bt_ctx->failed = true; \
				bt_ctx->failureMessages.push_back(bt_oss.str()); \
			} \
		} \
	} while (false)

/**
 * @def BT_SKIP(reason)
 * @brief Marks the current test as skipped and aborts it immediately.
 *
 * @p reason is streamed into the skip message via `operator<<`, so both
 * string literals and streamable values work. Throws Blackthorn::Tests::SkipException,
 * which BT_RUN_ALL() catches and reports in its "Skipped:" section --
 * skipped tests do not count as failures.
 *
 * @code
 * void PlatformSpecificTest() {
 * #ifndef __linux__
 * BT_SKIP("Linux-only test");
 * #endif
 * BT_ASSERT(true);
 * }
 * BT_TEST("Linux-only feature", PlatformSpecificTest);
 * @endcode
 */
#define BT_SKIP(reason) \
	do { \
		std::ostringstream bt_oss; \
		bt_oss << (reason); \
		throw Blackthorn::Tests::SkipException(bt_oss.str()); \
	} while (false)

/**
 * @def BT_DETAIL_ASSERT_CMP(a, b, op, opname)
 * @brief Implementation engine behind every fatal BT_ASSERT_* comparison macro.
 *
 * Evaluates `(a) op (b)`; on failure builds a message including both
 * operand values (via Blackthorn::Tests::Detail::toString, which degrades gracefully
 * for non-streamable types) and throws Blackthorn::Tests::AssertionFailure, aborting
 * the current test exactly like BT_ASSERT. Not intended to be used
 * directly -- use one of the BT_ASSERT_EQUAL / BT_ASSERT_GREATER / etc.
 * macros below instead.
 */
#define BT_DETAIL_ASSERT_CMP(a, b, op, opname) \
	do { \
		auto&& bt_lhs = (a); \
		auto&& bt_rhs = (b); \
		if (!(bt_lhs op bt_rhs)) { \
			std::ostringstream bt_oss; \
			bt_oss << "BT_ASSERT_" opname "(" #a ", " #b ") failed at " << __FILE__ << ":" << __LINE__ \
				<< " -- lhs = " << Blackthorn::Tests::Detail::toString(bt_lhs) \
				<< ", rhs = " << Blackthorn::Tests::Detail::toString(bt_rhs); \
			throw Blackthorn::Tests::AssertionFailure(bt_oss.str()); \
		} \
	} while (false)

/**
 * @def BT_DETAIL_EXPECT_CMP(a, b, op, opname)
 * @brief Implementation engine behind every non-fatal BT_EXPECT_* comparison macro.
 *
 * Evaluates `(a) op (b)`; on failure records a message (including both
 * operand values) against the currently-running test's Blackthorn::Tests::TestContext
 * and lets the test keep executing, exactly like BT_EXPECT. Not
 * intended to be used directly -- use one of the BT_EXPECT_EQUAL /
 * BT_EXPECT_GREATER / etc. macros below instead.
 */
#define BT_DETAIL_EXPECT_CMP(a, b, op, opname) \
	do { \
		auto&& bt_lhs = (a); \
		auto&& bt_rhs = (b); \
		if (!(bt_lhs op bt_rhs)) { \
			Blackthorn::Tests::TestContext* bt_ctx = Blackthorn::Tests::Detail::currentContext(); \
			if (bt_ctx != nullptr) { \
				std::ostringstream bt_oss; \
				bt_oss << "BT_EXPECT_" opname "(" #a ", " #b ") failed at " << __FILE__ << ":" << __LINE__ \
					<< " -- lhs = " << Blackthorn::Tests::Detail::toString(bt_lhs) \
					<< ", rhs = " << Blackthorn::Tests::Detail::toString(bt_rhs); \
				bt_ctx->failed = true; \
				bt_ctx->failureMessages.push_back(bt_oss.str()); \
			} \
		} \
	} while (false)

/// @def BT_ASSERT_EQUAL(a, b)
/// @brief Fatal: aborts the test unless `(a) == (b)`.
#define BT_ASSERT_EQUAL(a, b) BT_DETAIL_ASSERT_CMP(a, b, ==, "EQUAL")

/// @def BT_ASSERT_NEQUAL(a, b)
/// @brief Fatal: aborts the test unless `(a) != (b)`.
#define BT_ASSERT_NEQUAL(a, b) BT_DETAIL_ASSERT_CMP(a, b, !=, "NEQUAL")

/// @def BT_ASSERT_GREATER(a, b)
/// @brief Fatal: aborts the test unless `(a) > (b)`.
#define BT_ASSERT_GREATER(a, b) BT_DETAIL_ASSERT_CMP(a, b, >, "GREATER")

/// @def BT_ASSERT_GREATER_EQUAL(a, b)
/// @brief Fatal: aborts the test unless `(a) >= (b)`.
#define BT_ASSERT_GREATER_EQUAL(a, b) BT_DETAIL_ASSERT_CMP(a, b, >=, "GREATER_EQUAL")

/// @def BT_ASSERT_LESS(a, b)
/// @brief Fatal: aborts the test unless `(a) < (b)`.
#define BT_ASSERT_LESS(a, b) BT_DETAIL_ASSERT_CMP(a, b, <, "LESS")

/// @def BT_ASSERT_LESS_EQUAL(a, b)
/// @brief Fatal: aborts the test unless `(a) <= (b)`.
#define BT_ASSERT_LESS_EQUAL(a, b) BT_DETAIL_ASSERT_CMP(a, b, <=, "LESS_EQUAL")

/// @def BT_EXPECT_EQUAL(a, b)
/// @brief Non-fatal: records a failure and continues unless `(a) == (b)`.
#define BT_EXPECT_EQUAL(a, b) BT_DETAIL_EXPECT_CMP(a, b, ==, "EQUAL")

/// @def BT_EXPECT_NEQUAL(a, b)
/// @brief Non-fatal: records a failure and continues unless `(a) != (b)`.
#define BT_EXPECT_NEQUAL(a, b) BT_DETAIL_EXPECT_CMP(a, b, !=, "NEQUAL")

/// @def BT_EXPECT_GREATER(a, b)
/// @brief Non-fatal: records a failure and continues unless `(a) > (b)`.
#define BT_EXPECT_GREATER(a, b) BT_DETAIL_EXPECT_CMP(a, b, >, "GREATER")

/// @def BT_EXPECT_GREATER_EQUAL(a, b)
/// @brief Non-fatal: records a failure and continues unless `(a) >= (b)`.
#define BT_EXPECT_GREATER_EQUAL(a, b) BT_DETAIL_EXPECT_CMP(a, b, >=, "GREATER_EQUAL")

/// @def BT_EXPECT_LESS(a, b)
/// @brief Non-fatal: records a failure and continues unless `(a) < (b)`.
#define BT_EXPECT_LESS(a, b) BT_DETAIL_EXPECT_CMP(a, b, <, "LESS")

/// @def BT_EXPECT_LESS_EQUAL(a, b)
/// @brief Non-fatal: records a failure and continues unless `(a) <= (b)`.
#define BT_EXPECT_LESS_EQUAL(a, b) BT_DETAIL_EXPECT_CMP(a, b, <=, "LESS_EQUAL")

/**
 * @def BT_RUN_ALL(...)
 * @brief Convenience wrapper around Blackthorn::Tests::runAll() for use in main(); forwards all arguments.
 *
 * Accepts whatever arguments match one of the Blackthorn::Tests::runAll() overloads:
 * none, a Blackthorn::Tests::RunOptions, `(argc, argv)`, or `(argc, argv, options)`.
 * The `(argc, argv)` forms also parse `--shuffle`, `--seed=N`,
 * `--repeat=N`, and `--timeout=MS` from the command line.
 *
 * @code
 * int main(int argc, char** argv) {
 *     return BT_RUN_ALL(argc, argv); // non-zero exit code if any test failed
 * }
 *
 * int main() {
 *     Blackthorn::Tests::RunOptions options;
 *     options.order = Blackthorn::Tests::RunOptions::Order::Shuffled;
 *     options.repeat = 3;
 *     return BT_RUN_ALL(options);
 * }
 * @endcode
 */
#define BT_RUN_ALL(...) Blackthorn::Tests::runAll(__VA_ARGS__)
