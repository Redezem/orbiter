// Copyright (c) Martin Schweiger
// Licensed under the MIT License

#ifndef ORBITER_PLATFORM_PATHS_H
#define ORBITER_PLATFORM_PATHS_H

#include <filesystem>

namespace orbiter::platform {

std::filesystem::path runtime_content_root(const std::filesystem::path &argv0);
std::filesystem::path user_data_root();
std::filesystem::path default_scenario_path();

} // namespace orbiter::platform

#endif
