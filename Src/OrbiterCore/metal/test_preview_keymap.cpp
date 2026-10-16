// Unit tests for the preview laptop keymap seed. The engine Keymap parser is
// the authority; this test proves every seed line parses through the real
// Keymap::Read/ScanStr, resolves through the engine's own Write/PrintStr back
// to the intended laptop base key + modifiers, every bound base key is one the
// preview publisher physically delivers, and unlisted defaults survive.
// GetItemString is stubbed with the same first-label-wins semantics Config.cpp
// implements, so the real Keymap::Read parse path runs unchanged.
#include <windows.h>
#include "Orbitersdk.h"
#include "Keymap.h"
#include "session_input.h"
#include "preview_keymap.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>
using namespace std;

bool GetItemString(istream &is, const char *label, char *val) {
    // Matches Config.cpp semantics: scan whole file, first exact label with a
    // non-empty value wins; END_PARSE stops the scan.
    is.clear(); is.seekg(0, ios::beg);
    char line[512];
    while (is.getline(line, 512)) {
        string s = line;
        size_t b = s.find_first_not_of(" \t"); if (b == string::npos) continue;
        size_t e = s.find_last_not_of(" \t\r"); s = s.substr(b, e - b + 1);
        if (!_stricmp(s.c_str(), "END_PARSE")) return false;
        size_t eq = s.find('=');
        string key = eq == string::npos ? s : s.substr(0, eq);
        while (!key.empty() && (key.back() == ' ' || key.back() == '\t')) key.pop_back();
        if (_stricmp(key.c_str(), label)) continue;
        string v = eq == string::npos ? "" : s.substr(eq + 1);
        size_t vb = v.find_first_not_of(" \t"); if (vb == string::npos) return false;
        size_t ve = v.find_last_not_of(" \t\r"); v = v.substr(vb, ve - vb + 1);
        strcpy(val, v.c_str());
        return true;
    }
    is.clear(); return false;
}

static void check(bool b, const char *s) { if (!b) { fprintf(stderr, "FAIL: %s\n", s); exit(1); } }

static unordered_map<string, string> readKeymapFile(const char *path) {
    unordered_map<string, string> out;
    ifstream in(path);
    check(in.good(), path);
    string line;
    while (getline(in, line)) {
        size_t eq = line.find('='); if (eq == string::npos) continue;
        string key = line.substr(0, eq), val = line.substr(eq + 1);
        auto trim = [](string &s) {
            size_t b = s.find_first_not_of(" \t"), e = s.find_last_not_of(" \t\r");
            s = b == string::npos ? "" : s.substr(b, e - b + 1);
        };
        trim(key); trim(val);
        if (!key.empty()) out[key] = val; // first label wins, mirroring Read
    }
    return out;
}

int main() {
    const char *fixture = "fixtures/laptop-keymap-lines.txt";
    { ifstream probe(fixture); check(probe.good(), "laptop seed fixture must exist"); }

    // The engine generates its shipped defaults itself; write them and seed
    // them, then parse with the real Keymap::Read (ScanStr) and Write
    // (PrintStr) to observe the resolved DIK encoding in the engine's own
    // vocabulary.
    {
        Keymap gen; gen.Write("keymap.cfg");
        ofstream combined("laptop-test-keymap.cfg", ios::trunc);
        ifstream seed(fixture); combined << seed.rdbuf(); combined << '\n';
        ifstream base("keymap.cfg"); combined << base.rdbuf();
    }
    Keymap seeded;
    check(seeded.Read("laptop-test-keymap.cfg"), "engine Keymap::Read must accept the seeded file");
    seeded.Write("seeded-roundtrip.cfg");

    auto defaults = readKeymapFile("keymap.cfg");
    auto roundtrip = readKeymapFile("seeded-roundtrip.cfg");
    check(defaults.size() == size_t(LKEY_COUNT) && roundtrip.size() == size_t(LKEY_COUNT),
          "engine defaults must cover every logical key");

    // Expected resolved encodings (engine keyname table + modifier words).
    const pair<const char *, const char *> expects[] = {
        {"IncMainThrust", "1 CTRL"}, {"DecMainThrust", "2 CTRL"},
        {"KillMainRetroThrust", "3"}, {"OverrideFullMainThrust", "4"},
        {"OverrideFullRetroThrust", "7"}, {"IncHoverThrust", "9"}, {"DecHoverThrust", "6"},
        {"RCSEnable", "SLASH CTRL"}, {"RCSMode", "SLASH"},
        {"RCSPitchUp", "J"}, {"RCSPitchDown", "K"}, {"RCSYawLeft", "H"}, {"RCSYawRight", "L"},
        {"RCSBankLeft", "1"}, {"RCSBankRight", "2"},
        {"RCSUp", "DOWN"}, {"RCSDown", "UP"}, {"RCSLeft", "LEFT"}, {"RCSRight", "RIGHT"},
        {"RCSForward", "INSERT"}, {"RCSBack", "HOME"},
        {"LPRCSPitchUp", "J CTRL"}, {"LPRCSPitchDown", "K CTRL"},
        {"LPRCSYawLeft", "H CTRL"}, {"LPRCSYawRight", "L CTRL"},
        {"LPRCSBankLeft", "1 CTRL"}, {"LPRCSBankRight", "2 CTRL"},
        {"LPRCSUp", "DOWN CTRL"}, {"LPRCSDown", "UP CTRL"},
        {"LPRCSLeft", "LEFT CTRL"}, {"LPRCSRight", "RIGHT CTRL"},
        {"LPRCSForward", "INSERT CTRL"}, {"LPRCSBack", "HOME CTRL"},
    };
    for (const auto &e : expects) {
        auto it = roundtrip.find(e.first);
        check(it != roundtrip.end(), "missing overridden label after round-trip");
        check(!_stricmp(it->second.c_str(), e.second),
              ("seeded binding did not resolve as intended: " + string(e.first) +
               " -> " + it->second + " (want " + e.second + ")").c_str());
    }
    int overridden = 0;
    for (const auto &kv : defaults) {
        auto it = roundtrip.find(kv.first);
        if (it == roundtrip.end() || _stricmp(it->second.c_str(), kv.second.c_str())) ++overridden;
    }
    check(overridden == int(sizeof(expects) / sizeof(expects[0])),
          "seed must override exactly the keypad-only defaults");

    // Every seed base key must be physically deliverable: the SDL->DIK table
    // maps it and the preview allowlist names it. Engine names match SDL
    // physical positions for exactly these letters/digits/arrows.
    unordered_map<string, unsigned> physical;
    for (auto code : core_metal::previewLaptopPhysicalKeys()) {
        const char *name = nullptr;
        switch (code) {
            case SDL_SCANCODE_1: name = "1"; break; case SDL_SCANCODE_2: name = "2"; break;
            case SDL_SCANCODE_3: name = "3"; break; case SDL_SCANCODE_4: name = "4"; break;
            case SDL_SCANCODE_6: name = "6"; break; case SDL_SCANCODE_7: name = "7"; break;
            case SDL_SCANCODE_9: name = "9"; break; case SDL_SCANCODE_H: name = "H"; break;
            case SDL_SCANCODE_J: name = "J"; break; case SDL_SCANCODE_K: name = "K"; break;
            case SDL_SCANCODE_L: name = "L"; break; case SDL_SCANCODE_SLASH: name = "SLASH"; break;
            case SDL_SCANCODE_HOME: name = "HOME"; break; case SDL_SCANCODE_UP: name = "UP"; break;
            case SDL_SCANCODE_LEFT: name = "LEFT"; break; case SDL_SCANCODE_RIGHT: name = "RIGHT"; break;
            case SDL_SCANCODE_DOWN: name = "DOWN"; break; case SDL_SCANCODE_INSERT: name = "INSERT"; break;
            default: check(false, "physical allowlist contains an unexpected key"); break;
        }
        check(physical.emplace(name, core_metal::directInputScancode(code)).second, "duplicate allowlist key");
        check(physical[name] != 0, "SDL->DIK table must map every physical allowlist key");
    }
    ifstream seed(fixture); string line; int lines = 0;
    while (getline(seed, line)) {
        size_t eq = line.find('='); if (eq == string::npos || line.empty()) continue;
        string v = line.substr(eq + 1);
        size_t vb = v.find_first_not_of(" \t"); v = v.substr(vb);
        size_t sp = v.find(' '); string base = sp == string::npos ? v : v.substr(0, sp);
        check(physical.count(base) > 0, ("seed base key not physically deliverable: " + base).c_str());
        ++lines;
    }
    check(lines == int(sizeof(expects) / sizeof(expects[0])), "seed line count must match expectations");

    // IsLogicalKey proves the parsed encoding actually matches a synthetic
    // engine key state built from the physical DIK codes.
    char kstate[256] = {};
    kstate[physical["SLASH"]] = char(0x80);
    Keymap slashOnly; slashOnly.SetDefault();
    { ofstream single("single-label.cfg"); single << "RCSMode = SLASH\n"; }
    check(seeded.IsLogicalKey(kstate, OAPI_LKEY_RCSMode), "Ctrl-free physical slash arms RCSMode");
    kstate[OAPI_KEY_LCONTROL] = char(0x80);
    check(!seeded.IsLogicalKey(kstate, OAPI_LKEY_RCSMode), "modifier must not leak into plain binding");
    kstate[physical["SLASH"]] = 0;
    check(seeded.IsLogicalKey(kstate, OAPI_LKEY_RCSEnable) == false, "RCSEnable needs the key held");
    kstate[physical["SLASH"]] = char(0x80);
    check(seeded.IsLogicalKey(kstate, OAPI_LKEY_RCSEnable), "Ctrl+physical slash arms RCSEnable");
    (void)slashOnly;

    fprintf(stderr, "PASS: engine Keymap parses the laptop seed; %d bindings resolve; all bases physically deliverable\n", lines);
    return 0;
}
