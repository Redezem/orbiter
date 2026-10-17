// macOS port shim: Win32 kernel-object primitives used by the Lua script
// stack (LuaInline interpreter thread, Interpreter exec/wait mutexes).
// Win32 mutexes here are used for handover signalling only: they may be
// released by a thread other than the holder, so they are implemented as
// binary counting semaphores, NOT std::mutex (which is owner-bound).
//
// Lifetime model (mirrors Win32 kernel-handle refcounting):
// - WaitForSingleObject never frees the object; a raw handle stays safe.
// - CloseHandle removes the "open" registration. Thread bookkeeping is
//   deleted only when the thread has finished; while a possibly-abandoned
//   thread may still reference it (Orbiter's TerminateThread last resort),
//   the object is intentionally leaked rather than freed, so a late wait
//   cannot dangle. Mutex objects always leak on close: Orbiter's script
//   stack may wait on a mutex after its owning Interpreter is deleted
//   (a race upstream tolerates via kernel handle validation); a few hundred
//   bytes per interpreter is the safe port equivalent.
#ifndef ORBITER_MACOS_SHIM_SYNC_H
#define ORBITER_MACOS_SHIM_SYNC_H

#include "windows.h"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <unordered_set>

#ifndef WAIT_OBJECT_0
#define WAIT_OBJECT_0 0x00000000L
#endif
#ifndef WAIT_TIMEOUT
#define WAIT_TIMEOUT 0x00000102L
#endif
#ifndef WAIT_FAILED
#define WAIT_FAILED 0xFFFFFFFFL
#endif

namespace orb_shim {

struct HandleBase {
	enum Kind { kMutex, kThread } kind;
	virtual ~HandleBase() {}
};
struct SemaphoreHandle : HandleBase {
	SemaphoreHandle() { kind = kMutex; }
	std::mutex m;
	std::condition_variable cv;
	int count = 0; // 1 = released (Windows mutex state), 0 = held
};
struct ThreadHandle : HandleBase {
	ThreadHandle() { kind = kThread; }
	std::atomic<bool> done{false};
};

inline std::mutex& handleMutex() { static std::mutex m; return m; }
inline std::unordered_set<HandleBase*>& liveHandles()
{
	static std::unordered_set<HandleBase*> s;
	return s;
}
inline HandleBase* registerHandle(HandleBase* h)
{
	std::lock_guard<std::mutex> g(handleMutex());
	liveHandles().insert(h);
	return h;
}
inline bool claimHandle(HandleBase* h)
{
	std::lock_guard<std::mutex> g(handleMutex());
	return liveHandles().erase(h) == 1;
}

} // namespace orb_shim

// CreateMutexA(initialOwner=TRUE) starts held (count 0); FALSE starts released.
inline HANDLE CreateMutexA(void *sec, bool initialOwner, const char *name)
{
	(void)sec; (void)name;
	auto *h = new orb_shim::SemaphoreHandle;
	h->count = initialOwner ? 0 : 1;
	return orb_shim::registerHandle(h);
}
#ifndef CreateMutex
#define CreateMutex CreateMutexA
#endif

inline bool ReleaseMutex(HANDLE h)
{
	if (!h) return false;
	orb_shim::SemaphoreHandle *s = (orb_shim::SemaphoreHandle*)h;
	std::lock_guard<std::mutex> g(s->m);
	if (s->count < 1) s->count = 1;
	s->cv.notify_all();
	return true;
}

// MSVC _beginthreadex: returns a thread handle (0 on failure). Stack size is
// ignored (pthread default); Orbiter's request (4 KB) is far below it.
inline uintptr_t _beginthreadex(void *sec, unsigned stack,
	unsigned (*start)(void *), void *arg, unsigned flags, unsigned *tid)
{
	(void)sec; (void)stack; (void)flags;
	auto *h = new orb_shim::ThreadHandle;
	std::thread([h, start, arg] { start(arg); h->done.store(true); }).detach();
	if (tid) *tid = 0;
	return (uintptr_t)orb_shim::registerHandle(h);
}
inline unsigned _endthreadex(unsigned retval) { (void)retval; return 0; }

inline DWORD WaitForSingleObject(HANDLE h, DWORD ms)
{
	if (!h) return WAIT_FAILED;
	orb_shim::HandleBase *hb = (orb_shim::HandleBase*)h;
	if (hb->kind == orb_shim::HandleBase::kMutex) {
		orb_shim::SemaphoreHandle *s = (orb_shim::SemaphoreHandle*)h;
		std::unique_lock<std::mutex> lk(s->m);
		if (ms == 0xFFFFFFFFu) {
			s->cv.wait(lk, [&] { return s->count > 0; });
		} else {
			if (!s->cv.wait_for(lk, std::chrono::milliseconds(ms),
			        [&] { return s->count > 0; })) return WAIT_TIMEOUT;
		}
		s->count = 0; // acquired
		return WAIT_OBJECT_0;
	}
	// thread handle
	orb_shim::ThreadHandle *th = (orb_shim::ThreadHandle*)h;
	const bool infinite = (ms == 0xFFFFFFFFu);
	auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
	for (;;) {
		if (th->done.load()) return WAIT_OBJECT_0;
		if (!infinite && std::chrono::steady_clock::now() >= deadline) return WAIT_TIMEOUT;
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
}

inline bool TerminateThread(HANDLE h, DWORD code)
{
	// Abandon: Orbiter's comment history marks this as a last resort on
	// Windows too. There is no safe cancellation; the detached thread keeps
	// running and its bookkeeping leaks by design (see header).
	(void)code; (void)h;
	return true;
}

// The existing windows.h shim declares an inert CloseHandle; override via
// macro so script-stack handles get real teardown semantics while
// pseudo-handles (GetCurrentProcess etc.) keep the inert shim behaviour.
inline bool CloseHandleReal(HANDLE h)
{
	if (!h) return true;
	orb_shim::HandleBase *hb = (orb_shim::HandleBase*)h;
	if (!orb_shim::claimHandle(hb)) return true; // stale handle: ignored
	if (hb->kind == orb_shim::HandleBase::kMutex) return true; // leak: see header
	if (!((orb_shim::ThreadHandle*)h)->done.load()) return true; // thread alive: leak
	delete hb;
	return true;
}
#ifdef CloseHandle
#undef CloseHandle
#endif
#define CloseHandle CloseHandleReal

#endif // ORBITER_MACOS_SHIM_SYNC_H
