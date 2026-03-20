// Copyright (c) Martin Schweiger
// Licensed under the MIT License

#include "Bootstrap.h"
#include "Paths.h"
#include <sstream>

namespace {

constexpr const char *kScenarioArg = "--scenariox=";
constexpr const char *kMaxFramesArg = "--maxframes=";

bool starts_with(const std::string &value, const char *prefix)
{
	return value.rfind(prefix, 0) == 0;
}

} // namespace

namespace orbiter::bootstrap {

LaunchOptions parse_arguments(int argc, char **argv)
{
	LaunchOptions options;
	options.content_root = orbiter::platform::runtime_content_root(argc > 0 ? argv[0] : "");
	options.user_data_root = orbiter::platform::user_data_root();
	options.scenario_path = options.content_root / orbiter::platform::default_scenario_path();

	for (int i = 1; i < argc; ++i) {
		const std::string arg = argv[i];
		if (arg == "--help" || arg == "-h") {
			options.show_help = true;
			continue;
		}

		if (starts_with(arg, kScenarioArg)) {
			options.scenario_path = arg.substr(std::char_traits<char>::length(kScenarioArg));
			continue;
		}

		if (starts_with(arg, kMaxFramesArg)) {
			try {
				options.max_frames = std::stoi(arg.substr(std::char_traits<char>::length(kMaxFramesArg)));
				options.max_frames_explicit = true;
			} catch (...) {
				options.error = "Invalid value for --maxframes";
			}
			continue;
		}

		options.error = "Unknown argument: " + arg;
	}

	if (!options.error.empty()) {
		return options;
	}

	if (options.scenario_path.is_relative()) {
		options.scenario_path = std::filesystem::absolute(options.content_root / options.scenario_path);
	}

	if (options.max_frames < 0) {
		options.error = "--maxframes must be >= 0";
	}

	return options;
}

std::string help_text(const char *executable_name)
{
	std::ostringstream out;
	out
		<< "Usage: " << executable_name << " [--scenariox=<path>] [--maxframes=<n>]\n"
		<< "Default scenario: "
		<< (orbiter::platform::runtime_content_root(executable_name) / orbiter::platform::default_scenario_path()).string()
		<< '\n';
	return out.str();
}

} // namespace orbiter::bootstrap
