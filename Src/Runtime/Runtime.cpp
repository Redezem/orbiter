// Copyright (c) Martin Schweiger
// Licensed under the MIT License

#include "Runtime.h"

#include "SimulationSession.h"
#include "Paths.h"

#include <sstream>

namespace {

constexpr const char *kScenarioArg = "--scenariox=";
constexpr const char *kMaxFramesArg = "--maxframes=";
constexpr const char *kBackendArg = "--backend=";
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

} // namespace orbiter::runtime
