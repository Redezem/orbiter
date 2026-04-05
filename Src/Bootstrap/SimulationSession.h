// Copyright (c) Martin Schweiger
// Licensed under the MIT License

#ifndef ORBITER_BOOTSTRAP_SIMULATION_SESSION_H
#define ORBITER_BOOTSTRAP_SIMULATION_SESSION_H

#include "OrbiterMesh.h"
#include "Runtime.h"

#include <string>

namespace orbiter::bootstrap {

class SimulationSession final : public orbiter::runtime::Session {
public:
	bool initialize(const orbiter::runtime::LaunchOptions &options, std::string &error);
	void step(double dt_seconds, const orbiter::runtime::ControlInput &input) override;

	const orbiter::runtime::FrameScene &frame_scene() const override { return frame_scene_; }
	const orbiter::runtime::VesselState &focus_vessel() const override { return vessel_state_; }
	const std::vector<std::string> &warnings() const override { return warnings_; }
	double simulation_time() const override { return simulation_time_; }
	bool paused() const override { return paused_; }

private:
	void update_camera(const orbiter::runtime::ControlInput &input);
	void update_focus_instance();

	orbiter::runtime::LaunchOptions options_;
	orbiter::runtime::FrameScene frame_scene_;
	orbiter::runtime::VesselState vessel_state_;
	std::vector<std::string> warnings_;
	double simulation_time_ = 0.0;
	bool paused_ = false;
	double camera_distance_ = 80.0;
	double camera_yaw_ = 0.0;
	double camera_pitch_ = 0.35;
	double target_hover_altitude_ = 0.0;
	double target_throttle_ = 0.0;
	double pitch_rate_ = 0.0;
	double yaw_rate_ = 0.0;
	double roll_rate_ = 0.0;
};

} // namespace orbiter::bootstrap

#endif
