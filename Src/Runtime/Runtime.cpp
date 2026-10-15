// Copyright (c) Martin Schweiger
// Licensed under the MIT License

#include "Runtime.h"

#include "SimulationSession.h"
#include "Paths.h"

#include <cstdlib>
#include <sstream>

namespace {

constexpr const char *kScenarioArg = "--scenariox=";
constexpr const char *kMaxFramesArg = "--maxframes=";
constexpr const char *kBackendArg = "--backend=";
constexpr const char *kPlanetTexDirArg = "--planetexdir=";
constexpr const char *kSnapshotArg = "--snapshot=";
constexpr const char *kCameraDistanceArg = "--camdist=";
constexpr const char *kOrbiterCoreStubError = "Orbiter core backend is not wired yet";

bool starts_with(const std::string &value, const char *prefix)
{
	return value.rfind(prefix, 0) == 0;
}

bool parse_backend_name(const std::string &value, orbiter::runtime::SessionBackend &backend)
{
	if (value == "bootstrap") {
		backend = orbiter::runtime::SessionBackend::Bootstrap;
		return true;
	}
	if (value == "orbiter-core") {
		backend = orbiter::runtime::SessionBackend::OrbiterCore;
		return true;
	}
	return false;
}

class OrbiterCoreStubSession final : public orbiter::runtime::Session {
public:
	bool initialize(std::string &error)
	{
		error = kOrbiterCoreStubError;
		return false;
	}

	void step(double, const orbiter::runtime::ControlInput &) override {}
	const orbiter::runtime::FrameScene &frame_scene() const override { return frame_scene_; }
	const orbiter::runtime::VesselState &focus_vessel() const override { return vessel_state_; }
	const std::vector<std::string> &warnings() const override { return warnings_; }
	double simulation_time() const override { return 0.0; }
	bool paused() const override { return false; }

private:
	orbiter::runtime::FrameScene frame_scene_;
	orbiter::runtime::VesselState vessel_state_;
	std::vector<std::string> warnings_;
};

} // namespace

namespace orbiter::runtime {

const char *session_backend_name(SessionBackend backend)
{
	switch (backend) {
	case SessionBackend::Bootstrap:
		return "bootstrap";
	case SessionBackend::OrbiterCore:
		return "orbiter-core";
	}
	return "unknown";
}

LaunchOptions parse_launch_options(int argc, char **argv)
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

		if (starts_with(arg, kBackendArg)) {
			const std::string backend_name = arg.substr(std::char_traits<char>::length(kBackendArg));
			if (!parse_backend_name(backend_name, options.backend)) {
				options.error = "Unknown backend: " + backend_name;
			}
			continue;
		}

		if (starts_with(arg, kPlanetTexDirArg)) {
			options.planet_tex_dir = arg.substr(std::char_traits<char>::length(kPlanetTexDirArg));
			continue;
		}

		if (starts_with(arg, kSnapshotArg)) {
			options.snapshot_path = arg.substr(std::char_traits<char>::length(kSnapshotArg));
			continue;
		}

		if (starts_with(arg, kCameraDistanceArg)) {
			try {
				options.initial_camera_distance = std::stod(arg.substr(std::char_traits<char>::length(kCameraDistanceArg)));
			} catch (...) {
				options.error = "Invalid value for --camdist";
			}
			continue;
		}

		options.error = "Unknown argument: " + arg;
	}

	if (!options.error.empty()) {
		return options;
	}

	if (options.planet_tex_dir.empty()) {
		if (const char *env = std::getenv("ORBITER_PLANET_TEX_DIR")) {
			if (*env != '\0') {
				options.planet_tex_dir = env;
			}
		}
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
		<< "Usage: " << executable_name
		<< " [--scenariox=<path>] [--maxframes=<n>] [--backend=<bootstrap|orbiter-core>]\n"
		<< "Default scenario: "
		<< (orbiter::platform::runtime_content_root(executable_name) / orbiter::platform::default_scenario_path()).string()
		<< '\n'
		<< "Default backend: " << session_backend_name(SessionBackend::Bootstrap) << '\n';
	return out.str();
}

std::unique_ptr<Session> create_session(const LaunchOptions &options, std::string &error)
{
	switch (options.backend) {
	case SessionBackend::Bootstrap: {
		auto session = std::make_unique<orbiter::bootstrap::SimulationSession>();
		if (!session->initialize(options, error)) {
			return nullptr;
		}
		return session;
	}
	case SessionBackend::OrbiterCore: {
		auto session = std::make_unique<OrbiterCoreStubSession>();
		if (!session->initialize(error)) {
			return nullptr;
		}
		return session;
	}
	}

	error = "Unknown session backend";
	return nullptr;
}

std::vector<std::pair<std::string, std::filesystem::path>> discover_scenarios(
	const std::filesystem::path &content_root)
{
	std::vector<std::pair<std::string, std::filesystem::path>> found;

	const std::filesystem::path scenarios_dir = content_root / "Scenarios";
	std::error_code ec;
	if (!std::filesystem::is_directory(scenarios_dir, ec)) {
		return found;
	}

	for (const std::filesystem::directory_entry &entry :
		std::filesystem::recursive_directory_iterator(scenarios_dir, ec)) {
		if (!entry.is_regular_file(ec)) {
			continue;
		}
		const std::filesystem::path &path = entry.path();
		if (path.extension() != ".scn") {
			continue;
		}
		const std::filesystem::path relative = std::filesystem::relative(path, scenarios_dir, ec);
		std::string display;
		for (const std::filesystem::path &part : relative) {
			if (!display.empty()) {
				display += " / ";
			}
			display += part.string();
		}
		if (display.size() > 4 && display.compare(display.size() - 4, 4, ".scn") == 0) {
			display.erase(display.size() - 4);
		}
		found.emplace_back(display, path);
	}

	std::sort(found.begin(), found.end(),
		[](const std::pair<std::string, std::filesystem::path> &a,
		   const std::pair<std::string, std::filesystem::path> &b) {
			return a.first < b.first;
		});
	return found;
}

} // namespace orbiter::runtime
