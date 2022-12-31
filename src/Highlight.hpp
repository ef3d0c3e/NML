#ifndef NML_HIGHLIGHT_HPP
#define NML_HIGHLIGHT_HPP

#include "Util.hpp"
#include <sys/wait.h>
#include <unistd.h>
#include <fmt/format.h>
#include <iostream>
#include <thread>
#include <chrono>

/**
 * @brief Target outputs for highlighting
 */
MAKE_CENUM_Q(HighlightTarget, std::uint8_t,
	HTML, 0,
	LATEX, 1,
);

template <HighlightTarget Target>
[[nodiscard]] static std::string Highlight(const std::string& src, const std::string& lang, const std::string& style)
{
	std::string command(fmt::format("source-highlight --style-file={} --src-lang={} --tab=4", style, lang));

	if constexpr (Target == HighlightTarget::HTML)
	{
		command += " --out-format=html";

		std::string result("");
		int wp[2], rp[2];
		if (pipe(wp) == -1 || pipe(rp) == -1) [[unlikely]]
			throw Error("Pipe creation failed");

		pid_t pid;
		if ((pid = fork()) == 0)
		{
			close(wp[1]); // Parent write
			close(rp[0]); // Parent read

			dup2(wp[0], 0);
			close(wp[0]); // Child read
			dup2(rp[1], 1);
			close(rp[1]); // Child write


			execlp("/bin/sh", "sh", "-c", command.c_str(), (char*)0);
			//execlp("/usr/bin/sh", "sh", "-c", command.c_str(), (char*)0);
			exit(0);
		}
		else if (pid == -1)
		{
			throw Error("fork() failed");
		}
		else
		{
			close(wp[0]); // Child read
			close(rp[1]); // Child write

			write(wp[1], src.c_str(), src.size()+1);
			close(wp[1]);

			std::array<char, 1024> buf;
			while (true)
			{
				std::size_t n = read(rp[0], buf.data(), buf.size());
				if (n == 0)
					break;
				else if (n < 0) [[unlikely]]
					throw Error("read failed");

				result.append(std::string(buf.data(), n));
			}
			close (rp[0]);
		}

		return result;
	}
	else
	{
		[]<bool v = false>()
		{
			static_assert(v, "Unsupported target.");
		}();
	}
}

#endif // NML_HIGHLIGHT_HPP
