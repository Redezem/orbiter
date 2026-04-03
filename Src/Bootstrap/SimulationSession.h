// Copyright (c) Martin Schweiger
// Licensed under the MIT License

#ifndef ORBITER_BOOTSTRAP_SIMULATION_SESSION_H
#define ORBITER_BOOTSTRAP_SIMULATION_SESSION_H

#include "Bootstrap.h"
#include "OrbiterMesh.h"

#include <cstddef>
#include <string>
#include <vector>

namespace orbiter::bootstrap {

struct Vector3 {
	double x = 0.0;
	double y = 0.0;
	double z = 0.0;
};

struct Color4 {
	float r = 1.0f;
	float g = 1.0f;
	float b = 1.0f;
	float a = 1.0f;
};

struct CameraState {
	Vector3 position;
	Vector3 target;
	float fov_degrees = 40.0f;
};

struct ControlInput {
	double thrust = 0.0;
	double pitch = 0.0;
	double roll = 0.0;
	double yaw = 0.0;
	double camera_yaw_delta = 0.0;
	double camera_pitch_delta = 0.0;
	double camera_zoom_delta = 0.0;
	bool hover_hold = false;
	bool pause_toggle = false;
};

struct MeshAsset {
	std::string id;
	LoadedMesh mesh;
};

struct MeshInstance {
	size_t asset_index = 0;
	Vector3 position;
	Vector3 rotation_degrees;
	Vector3 scale = {1.0, 1.0, 1.0};
	Color4 color;
};

struct TerrainPatch {
	MeshInstance mesh;
};

struct BaseInstance {
	std::string kind;
	MeshInstance mesh;
};

struct FrameScene {
	CameraState camera;
	std::vector<MeshAsset> assets;
	std::vector<MeshInstance> vessel_instances;
	std::vector<TerrainPatch> terrain_patches;
	std::vector<BaseInstance> base_instances;
	bool using_procedural_ground = false;
	std::string focus_name;
};

struct VesselState {
	std::string name;
	std::string class_name;
	Vector3 position;
	Vector3 velocity;
	Vector3 rotation_degrees;
	double throttle = 0.0;
	double simulation_time = 0.0;
	bool paused = false;
};

class SimulationSession {
public:
	bool initialize(const LaunchOptions &options, std::string &error);
	void step(double dt_seconds, const ControlInput &input);

	const FrameScene &frame_scene() const { return frame_scene_; }
	const VesselState &focus_vessel() const { return vessel_state_; }
	const std::vector<std::string> &warnings() const { return warnings_; }
	double simulation_time() const { return simulation_time_; }
	bool paused() const { return paused_; }

private:
	void update_camera(const ControlInput &input);
	void update_focus_instance();

	LaunchOptions options_;
	FrameScene frame_scene_;
	VesselState vessel_state_;
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
