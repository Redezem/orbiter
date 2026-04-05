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
