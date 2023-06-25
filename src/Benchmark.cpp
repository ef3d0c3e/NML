#include "Benchmark.hpp"
#include <functional>
#include <fmt/format.h>
#include <fmt/chrono.h>

Benchmark Benchmarker{};

[[nodiscard]] BenchDur BenchmarkResult::getSubDuration() const
{
	BenchDur dur;
	for (const auto& bench : m_sub)
		dur += bench.m_dur;

	return dur;
};

[[nodiscard]] std::string BenchmarkResult::display(std::size_t depth) const
{
	std::function<std::string(const BenchmarkResult&, std::size_t)> walk = [&walk, depth](const BenchmarkResult& bench, std::size_t d)
	{
		std::string s;
		auto append = [d, &s](std::string_view content)
		{
			return s.append(std::string(d, ' ')).append(content);
		};

		if (bench.m_sub.empty() || depth == d) // Do not recurse
		{
			append(fmt::format(" - {} [{}]\n", bench.m_name, bench.m_dur));
		}
		else // Recurse
		{
			append(fmt::format(" * {} [{}]:\n", bench.m_name, bench.m_dur));

			BenchDur subTime;
			for (const auto& res : bench.m_sub)
			{
				subTime += res.m_dur;
				s.append(walk(res, d+1));
			}

			append(fmt::format("  (Subtotal {})\n", bench.m_dur-subTime));
		}

		return s;
	};

	return walk(*this, 0);
}

void Benchmark::push(std::string&& name)
{
	m_benchStack.push(BenchmarkResult{std::move(name)});
	m_timeStack.push(m_clk.now());
}

void Benchmark::pop()
{
	BenchDur d = (m_clk.now() - m_timeStack.top());
	m_timeStack.pop();

	BenchmarkResult bench = m_benchStack.top();
	m_benchStack.pop();

	bench.setDuration(std::move(d));
	if (m_benchStack.empty()) // Add to global
		m_results.push_back(std::move(bench));
	else // Add to previous benchmark
		m_benchStack.top().addSub(std::move(bench));
}

[[nodiscard]] std::string Benchmark::display(std::size_t depth) const
{
	std::string s;
	for (BenchmarkResult res : m_results)
		s.append(res.display(depth));
	
	return s;
}
