// Copyright (c) Martin Schweiger
// Licensed under the MIT License

#include "Paths.h"
#include <cstdlib>

namespace orbiter::platform {

namespace {

std::filesystem::path find_content_root(std::filesystem::path start)
{
	std::error_code ec;
	start = std::filesystem::weakly_canonical(start, ec);
	if (ec) {
		start = start.lexically_normal();
	}

	for (std::filesystem::path cursor = start; !cursor.empty(); cursor = cursor.parent_path()) {
		if (std::filesystem::exists(cursor / "Scenarios")) {
			return cursor;
		}
		if (cursor == cursor.root_path()) {
			break;
		}
	}

	return {};
}

} // namespace

std::filesystem::path runtime_content_root(const std::filesystem::path &argv0)
{
	if (!argv0.empty()) {
		const std::filesystem::path from_argv =
			find_content_root(std::filesystem::absolute(argv0).parent_path());
		if (!from_argv.empty()) {
			return from_argv;
		}
	}

	const std::filesystem::path from_cwd = find_content_root(std::filesystem::current_path());
	if (!from_cwd.empty()) {
		return from_cwd;
	}

	return std::filesystem::current_path();
}

std::filesystem::path user_data_root()
{
#if defined(__APPLE__)
	if (const char *home = std::getenv("HOME")) {
		return std::filesystem::path(home) / "Library" / "Application Support" / "Orbiter";
	}
	return std::filesystem::current_path() / "OrbiterUserData";
#elif defined(_WIN32)
	if (const char *appdata = std::getenv("APPDATA")) {
		return std::filesystem::path(appdata) / "Orbiter";
	}
	return std::filesystem::current_path() / "OrbiterUserData";
#else
	if (const char *home = std::getenv("HOME")) {
		return std::filesystem::path(home) / ".local" / "share" / "orbiter";
	}
	return std::filesystem::current_path() / "OrbiterUserData";
#endif
}

std::filesystem::path default_scenario_path()
{
	return std::filesystem::path("Scenarios") / "Checklists" / "Quickstart.scn";
}

} // namespace orbiter::platform
