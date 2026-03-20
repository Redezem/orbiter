// Copyright (c) Martin Schweiger
// Licensed under the MIT License

#include "Bootstrap.h"
#include <filesystem>
#include <iostream>

int main(int argc, char **argv)
{
	const orbiter::bootstrap::LaunchOptions options = orbiter::bootstrap::parse_arguments(argc, argv);
	if (!options.error.empty()) {
		std::cerr << options.error << '\n';
		return 2;
	}

	if (options.show_help) {
		std::cout << orbiter::bootstrap::help_text(argv[0]);
		return 0;
	}

	if (!std::filesystem::exists(options.scenario_path)) {
		std::cerr << "Scenario not found: " << options.scenario_path << '\n';
		return 3;
	}

	std::cout
		<< "Orbiter_server bootstrap\n"
		<< "Content root: " << options.content_root << '\n'
		<< "Scenario: " << options.scenario_path << '\n'
		<< "Max frames: " << options.max_frames << '\n'
		<< "User data: " << options.user_data_root << '\n';
	return 0;
}
