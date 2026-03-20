// Copyright (c) Martin Schweiger
// Licensed under the MIT License

#include "DynamicLibrary.h"

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace orbiter::platform {

DynamicLibrary::DynamicLibrary(const std::filesystem::path &path)
{
	open(path);
}

DynamicLibrary::~DynamicLibrary()
{
	close();
}

DynamicLibrary::DynamicLibrary(DynamicLibrary &&other) noexcept
{
	*this = std::move(other);
}

DynamicLibrary &DynamicLibrary::operator=(DynamicLibrary &&other) noexcept
{
	if (this == &other) {
		return *this;
	}

	close();
	handle_ = other.handle_;
	path_ = std::move(other.path_);
	other.handle_ = nullptr;
	other.path_.clear();
	return *this;
}

bool DynamicLibrary::open(const std::filesystem::path &path)
{
	close();

#if defined(_WIN32)
	handle_ = LoadLibraryA(path.string().c_str());
#else
	handle_ = static_cast<HMODULE>(dlopen(path.string().c_str(), RTLD_NOW | RTLD_LOCAL));
#endif

	if (!handle_) {
		return false;
	}

	path_ = path;
	return true;
}

void DynamicLibrary::close()
{
	if (!handle_) {
		return;
	}

	close(handle_);
	handle_ = nullptr;
	path_.clear();
}

void *DynamicLibrary::symbol(const char *name) const
{
	if (!handle_) {
		return nullptr;
	}

	return symbol(handle_, name);
}

HMODULE DynamicLibrary::release()
{
	HMODULE handle = handle_;
	handle_ = nullptr;
	path_.clear();
	return handle;
}

std::string DynamicLibrary::last_error()
{
#if defined(_WIN32)
	const DWORD error = GetLastError();
	if (!error) {
		return {};
	}

	char *buffer = nullptr;
	const DWORD size = FormatMessageA(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		nullptr,
		error,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		reinterpret_cast<LPSTR>(&buffer),
		0,
		nullptr);
	std::string message = size && buffer ? std::string(buffer, size) : std::string("Win32 error");
	if (buffer) {
		LocalFree(buffer);
	}
	return message;
#else
	const char *error = dlerror();
	return error ? std::string(error) : std::string();
#endif
}

std::string DynamicLibrary::shared_library_extension()
{
#if defined(_WIN32)
	return ".dll";
#elif defined(__APPLE__)
	return ".dylib";
#else
	return ".so";
#endif
}

std::filesystem::path DynamicLibrary::standalone_module_path(const std::filesystem::path &root, const std::string &name)
{
	return root / (name + shared_library_extension());
}

std::filesystem::path DynamicLibrary::nested_module_path(const std::filesystem::path &root, const std::string &name)
{
	return root / name / (name + shared_library_extension());
}

void *DynamicLibrary::symbol(HMODULE handle, const char *name)
{
	if (!handle) {
		return nullptr;
	}

#if defined(_WIN32)
	return reinterpret_cast<void *>(GetProcAddress(handle, name));
#else
	return dlsym(handle, name);
#endif
}

void DynamicLibrary::close(HMODULE handle)
{
	if (!handle) {
		return;
	}

#if defined(_WIN32)
	FreeLibrary(handle);
#else
	dlclose(handle);
#endif
}

} // namespace orbiter::platform
