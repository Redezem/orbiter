// Copyright (c) Martin Schweiger
// Licensed under the MIT License

#include "Runtime.h"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <string>

namespace {

std::filesystem::path content_root()
{
	return std::filesystem::current_path();
}

std::filesystem::path quadcopter_scenario_path()
{
	return content_root() / "Scenarios" / "Quadcopter" / "Quadcopter at KSC.scn";
}

} // namespace

TEST_CASE("Runtime launch options default to bootstrap backend")
{
	char arg0[] = "Orbiter_server";
	char *argv[] = {arg0};

	const orbiter::runtime::LaunchOptions options = orbiter::runtime::parse_launch_options(1, argv);

	REQUIRE(options.error.empty());
	CHECK(options.backend == orbiter::runtime::SessionBackend::Bootstrap);
	CHECK(options.scenario_path.is_absolute());
}

TEST_CASE("Runtime launch options parse orbiter-core backend")
{
	char arg0[] = "Orbiter_server";
	char arg1[] = "--backend=orbiter-core";
	char *argv[] = {arg0, arg1};

	const orbiter::runtime::LaunchOptions options = orbiter::runtime::parse_launch_options(2, argv);

	REQUIRE(options.error.empty());
	CHECK(options.backend == orbiter::runtime::SessionBackend::OrbiterCore);
}

TEST_CASE("Runtime launch options reject unknown backend")
{
	char arg0[] = "Orbiter_server";
	char arg1[] = "--backend=unknown";
	char *argv[] = {arg0, arg1};

	const orbiter::runtime::LaunchOptions options = orbiter::runtime::parse_launch_options(2, argv);

	CHECK(options.error == "Unknown backend: unknown");
}

TEST_CASE("Bootstrap runtime session creates a live session")
{
	orbiter::runtime::LaunchOptions options;
	options.content_root = content_root();
	options.scenario_path = quadcopter_scenario_path();
	options.user_data_root = content_root() / "TestUserData";
	options.backend = orbiter::runtime::SessionBackend::Bootstrap;

	std::string error;
	std::unique_ptr<orbiter::runtime::Session> session = orbiter::runtime::create_session(options, error);

	REQUIRE(error.empty());
	REQUIRE(session);
	CHECK(session->frame_scene().focus_name == session->focus_vessel().name);
	CHECK(session->focus_vessel().name == "QC-01");
	CHECK(session->focus_vessel().class_name == "Quadcopter");
	session->step(1.0 / 60.0, {});
	CHECK(session->simulation_time() > 0.0);
}

TEST_CASE("Orbiter core backend reports the fixed stub error")
{
	orbiter::runtime::LaunchOptions options;
	options.content_root = content_root();
	options.scenario_path = quadcopter_scenario_path();
	options.user_data_root = content_root() / "TestUserData";
	options.backend = orbiter::runtime::SessionBackend::OrbiterCore;

	std::string error;
	std::unique_ptr<orbiter::runtime::Session> session = orbiter::runtime::create_session(options, error);

	CHECK_FALSE(session);
	CHECK(error == "Orbiter core backend is not wired yet");
}

TEST_CASE("Scenario discovery finds staged scenarios")
{
	const auto scenarios = orbiter::runtime::discover_scenarios(content_root());

	CHECK_FALSE(scenarios.empty());
	bool has_quadcopter = false;
	for (const auto &entry : scenarios) {
		if (entry.first == "Quadcopter / Quadcopter at KSC") {
			has_quadcopter = true;
			CHECK(entry.second.extension() == ".scn");
			CHECK(std::filesystem::exists(entry.second));
		}
	}
	CHECK(has_quadcopter);

	// Sorted by display name.
	for (size_t i = 1; i < scenarios.size(); ++i) {
		CHECK(scenarios[i - 1].first <= scenarios[i].first);
	}
}

TEST_CASE("Session can hot-swap to another scenario")
{
	orbiter::runtime::LaunchOptions options;
	options.content_root = content_root();
	options.scenario_path = quadcopter_scenario_path();
	options.user_data_root = content_root() / "TestUserData";
	options.backend = orbiter::runtime::SessionBackend::Bootstrap;

	std::string error;
	std::unique_ptr<orbiter::runtime::Session> session = orbiter::runtime::create_session(options, error);
	REQUIRE(session);
	CHECK(session->focus_vessel().class_name == "Quadcopter");

	const auto scenarios = orbiter::runtime::discover_scenarios(content_root());
	std::filesystem::path other_scenario;
	for (const auto &entry : scenarios) {
		if (entry.first == "Demo / Today") {
			other_scenario = entry.second;
			break;
		}
	}
	REQUIRE(!other_scenario.empty());

	std::string switch_error;
	REQUIRE(session->load_scenario(other_scenario, switch_error));
	CHECK(switch_error.empty());
	// "Today" focuses GL-01, not the first ship in the file.
	CHECK(session->focus_vessel().name == "GL-01");
	CHECK(session->focus_vessel().class_name == "DeltaGlider");
	session->step(1.0 / 60.0, {});
	CHECK(session->simulation_time() > 0.0);
}
