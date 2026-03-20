// Copyright (c) Martin Schweiger
// Licensed under the MIT License

#ifndef ORBITER_PLATFORM_DYNAMICLIBRARY_H
#define ORBITER_PLATFORM_DYNAMICLIBRARY_H

#include "OrbiterPlatform.h"
#include <filesystem>
#include <string>

namespace orbiter::platform {

struct ModuleMetadataV1 {
	const char *name;
	const char *category;
	const char *version;
	const char *description;
};

using ModuleMetadataFn = const ModuleMetadataV1 *(*)();

class DynamicLibrary {
public:
	DynamicLibrary() = default;
	explicit DynamicLibrary(const std::filesystem::path &path);
	~DynamicLibrary();

	DynamicLibrary(const DynamicLibrary &) = delete;
	DynamicLibrary &operator=(const DynamicLibrary &) = delete;

	DynamicLibrary(DynamicLibrary &&other) noexcept;
	DynamicLibrary &operator=(DynamicLibrary &&other) noexcept;

	bool open(const std::filesystem::path &path);
	void close();
	bool is_open() const { return handle_ != nullptr; }
	void *symbol(const char *name) const;
	HMODULE native_handle() const { return handle_; }
	HMODULE release();
	const std::filesystem::path &path() const { return path_; }

	static std::string last_error();
	static std::string shared_library_extension();
	static std::filesystem::path standalone_module_path(const std::filesystem::path &root, const std::string &name);
	static std::filesystem::path nested_module_path(const std::filesystem::path &root, const std::string &name);
	static void *symbol(HMODULE handle, const char *name);
	static void close(HMODULE handle);

private:
	HMODULE handle_ = nullptr;
	std::filesystem::path path_;
};

} // namespace orbiter::platform

#endif
