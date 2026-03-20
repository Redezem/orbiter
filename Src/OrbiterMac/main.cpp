// Copyright (c) Martin Schweiger
// Licensed under the MIT License

#include "Bootstrap.h"
#include "OrbiterMacApp.h"
#include <filesystem>
#include <iostream>

int main(int argc, char **argv)
{
	orbiter::bootstrap::LaunchOptions options = orbiter::bootstrap::parse_arguments(argc, argv);
	if (!options.error.empty()) {
		std::cerr << options.error << '\n';
		return 2;
	}

	if (options.show_help) {
		std::cout << orbiter::bootstrap::help_text(argv[0]);
		return 0;
	}

	// The interactive app should stay open until the user closes it unless
	// an explicit frame limit was requested on the command line.
	if (!options.max_frames_explicit) {
		options.max_frames = 0;
	}

	if (!std::filesystem::exists(options.scenario_path)) {
		std::cerr << "Scenario not found: " << options.scenario_path << '\n';
		return 3;
	}

	return orbiter::macos::run_app(options);
}
