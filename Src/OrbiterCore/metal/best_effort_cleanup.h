#pragma once
#include <cstdio>
#include <exception>
namespace core_metal {
// Each independent teardown action is attempted even after an earlier diagnostic.
// The caller must aggregate false results, never translate them into PASS.
template<class F> bool cleanupStep(const char *name,F action) noexcept {
    try { action(); return true; }
    catch(const std::exception &e) {fprintf(stderr,"[core-metal] FAIL cleanup %s: %s\n",name,e.what());}
    catch(...) {fprintf(stderr,"[core-metal] FAIL cleanup %s: unknown exception\n",name);}
    return false;
}
}
