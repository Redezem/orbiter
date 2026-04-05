// Copyright (c) Martin Schweiger
// Licensed under the MIT License

#include "Runtime.h"

#include <iostream>

int main(int argc, char **argv)
{
	const orbiter::runtime::LaunchOptions options = orbiter::runtime::parse_launch_options(argc, argv);
	if (!options.error.empty()) {
		std::cerr << options.error << '\n';
		return 2;
	}

	if (options.show_help) {
		std::cout << orbiter::runtime::help_text(argv[0]);
		return 0;
	}

	std::string error;
	std::unique_ptr<orbiter::runtime::Session> session = orbiter::runtime::create_session(options, error);
	if (!session) {
		std::cerr << error << '\n';
		return 3;
	}

	for (int frame = 0; frame < options.max_frames; ++frame) {
		session->step(1.0 / 60.0, {});
	}

	const orbiter::runtime::VesselState &focus = session->focus_vessel();

	std::cout
		<< "Orbiter_server session\n"
		<< "Content root: " << options.content_root << '\n'
		<< "Scenario: " << options.scenario_path << '\n'
		<< "Max frames: " << options.max_frames << '\n'
		<< "User data: " << options.user_data_root << '\n'
		<< "Focus vessel: " << focus.name << " (" << focus.class_name << ")\n"
		<< "Sim time: " << focus.simulation_time << " s\n"
		<< "Position ENU: " << focus.position.x << ' ' << focus.position.y << ' ' << focus.position.z << '\n'
		<< "Velocity ENU: " << focus.velocity.x << ' ' << focus.velocity.y << ' ' << focus.velocity.z << '\n'
		<< "Rotation deg: " << focus.rotation_degrees.x << ' ' << focus.rotation_degrees.y << ' ' << focus.rotation_degrees.z << '\n'
		<< "Throttle: " << focus.throttle << '\n';

	for (const std::string &warning : session->warnings()) {
		std::cout << "Warning: " << warning << '\n';
	}
	return 0;
}
