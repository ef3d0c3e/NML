#include <iostream>
#include <ranges>

int main()
{
	for (const auto n : std::views::iota(1, 100))
	{
		if (!(n % 3))
			std::cout << "fizz";
		if (!(n % 5))
			std::cout << "buzz";
		if (n % 3 && n % 5)
			std::cout << n;

		std::cout << "\n";
	}

	return 0;
}
