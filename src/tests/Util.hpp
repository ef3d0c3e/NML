/* NML a simple yet powerful markup language
   Copyright © 2023 ef3d0c3e

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU Affero General Public License as
   published by the Free Software Foundation, either version 3 of the
   License, or any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Affero General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>. */

#ifndef TESTS_UTIL_HPP
#define TESTS_UTIL_HPP

#include <string>
#include <random>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_adapters.hpp>

/**
 * @brief Generates a random unicode string
 * @param mt Random source
 * @param len Stirng length (in codepoint)
 */
std::string randomString(std::mt19937& mt, std::size_t len);

namespace
{
	class StringGenerator : public Catch::Generators::IGenerator<std::string>
	{
		std::mt19937 m_mt;
		std::uniform_int_distribution<std::size_t> m_dist;

		std::string m_current;
		public:
		StringGenerator(std::size_t lo, std::size_t hi):
			m_mt(std::mt19937(std::random_device{}())),
			m_dist(lo, hi)
			{
				next();
			}

		const std::string& get() const override { return m_current; }

		bool next() override 
		{
			m_current = randomString(m_mt, m_dist(m_mt));
			return true;
		}
	};
	//
	// This helper function provides a nicer UX when instantiating the generator
	// Notice that it returns an instance of GeneratorWrapper<int>, which
	// is a value-wrapper around std::unique_ptr<IGenerator<int>>.
	Catch::Generators::GeneratorWrapper<std::string> random(std::size_t lo, std::size_t hi) {
		return Catch::Generators::GeneratorWrapper<std::string>(
				new StringGenerator(lo, hi)
				);
	}
}


#endif // TESTS_UTIL_HPP
