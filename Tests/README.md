# Orbiter integration and unit tests

## Unit tests

Unit tests are written as .cpp files in this directory and registered using `add_test_file` CMake function. Naming convention is "Module.Test.cpp"

Unit tests have a default timeout of 30 seconds for whole suite

The native runtime/session seam now also has a Catch2 target:

- `Runtime.Session` verifies launch-option parsing, backend selection, bootstrap session creation, and the expected fail-fast `orbiter-core` stub behavior.

## Integration tests

The legacy integration test path is implemented by

1. Adding a Lua script to Script\Tests directory
1. Adding a corresponding scenario referencing this script to Scenarios\Tests directory

Lua script must:

1. Ensure test runs for limited time (under 60 seconds)
1. Call oapi.exit(code) when test is finished, pass non-zero return code in case of failed test, and 0 - if all tests are successful
1. Print information about execution using oapi.write_log

The native host path now also has direct `Orbiter_server` smoke coverage:

1. `Scenario.QuadcopterBootstrap` verifies the working bootstrap backend against `Scenarios/Quadcopter/Quadcopter at KSC.scn`
1. `Scenario.OrbiterCoreStubFails` verifies that the selectable `orbiter-core` backend currently fails with the expected fixed error while the real runtime integration is still pending
