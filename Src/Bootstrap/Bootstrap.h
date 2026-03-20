// Copyright (c) Martin Schweiger
// Licensed under the MIT License

#ifndef ORBITER_BOOTSTRAP_BOOTSTRAP_H
#define ORBITER_BOOTSTRAP_BOOTSTRAP_H

#include <filesystem>
#include <string>

namespace orbiter::bootstrap {

struct LaunchOptions {
	std::filesystem::path content_root;
	std::filesystem::path scenario_path;
	std::filesystem::path user_data_root;
	int max_frames = 300;
	bool max_frames_explicit = false;
	bool show_help = false;
	std::string error;
};

LaunchOptions parse_arguments(int argc, char **argv);
std::string help_text(const char *executable_name);

} // namespace orbiter::bootstrap

#endif
