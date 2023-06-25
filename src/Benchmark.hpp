#ifndef NML_BENCHMARK_HPP
#define NML_BENCHMARK_HPP

#include <deque>
#include <stack>
#include <chrono>
#include <ratio>
#include <numeric>

using BenchDur = std::chrono::duration<double, std::micro>;

class BenchmarkResult
{
	std::string m_name; ///< Name of benchmark
	BenchDur m_dur; ///< Benchmark duration

	std::deque<BenchmarkResult> m_sub; ///< Sub benchmarks
public:
	/**
	 * @brief Constructor
	 *
	 * @param name Benchmark name
	 * @dur Benchmark duration
	 */
	BenchmarkResult(std::string&& name):
		m_name{std::move(name)} {}

	/**
	 * @brief Sets new name of benchmark
	 *
	 * @param name New name
	 */
	void setName(std::string&& name) { m_name = std::move(name); }

	/**
	 * @brief Sets benchmark duration
	 *
	 * @param dur New benchmark duration
	 */
	void setDuration(BenchDur&& dur) { m_dur = std::move(dur); }

	/**
	 * @brief Gets benchmark duration
	 *
	 * @returns Benchmark duration
	 */
	[[nodiscard]] BenchDur getDuration() const { return m_dur; };

	/**
	 * @brief Gets duration of sub benchmarks
	 *
	 * @return Sum of duration of all sub benchmarks
	 */
	[[nodiscard]] BenchDur getSubDuration() const;

	/**
	 * @brief Adds sub benchmark
	 *
	 * @param sub Sub benchmark to add
	 */
	void addSub(BenchmarkResult&& sub) { m_sub.push_back(std::move(sub)); }

	/**
	 * @brief Displays benchmark and sub benchmarks
	 *
	 * @params depth Maximum benchmark depth (default = infinite depth)
	 * @return Formatted string of benchmark duration
	 */
	[[nodiscard]] std::string display(std::size_t depth = -1) const;
};


class Benchmark
{
	std::chrono::steady_clock m_clk; ///< Clock used for benchmarking

	// store in here when stack goes empty (root)
	std::deque<BenchmarkResult> m_results;
	std::stack<BenchmarkResult> m_benchStack;
	std::stack<std::chrono::time_point<decltype(m_clk)>> m_timeStack;

public:
	/**
	 * @brief Starts a benchmark
	 *
	 * @param name Benchmark name
	 */
	void push(std::string&& name);

	/**
	 * @brief Stops last benchmark
	 */
	void pop();

	/**
	 * @brief Sets new name of current benchmark
	 *
	 * @param name New name
	 */
	void setName(std::string&& name) { m_benchStack.top().setName(std::move(name)); }

	/**
	 * @brief Displays benchmarks
	 *
	 * @params depth Maximum benchmark depth (default = infinite depth)
	 * @return Formatted string of benchmarks durations
	 */
	[[nodiscard]] std::string display(std::size_t depth = -1) const;
};

extern Benchmark Benchmarker;

#endif // NML_BENCHMARK_HPP
