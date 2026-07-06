# 6. Testing

Baldr's tests live under `test/src/` and use GoogleTest. The `Tests_run` binary discovers them automatically via `gtest_discover_tests`. Run the suite with `ctest` or filter to a subset of specs with `--gtest_filter`.

## Run all tests

After configuring:

```bash title="terminal"
ctest --test-dir build --output-on-failure
```

The first time this runs CMake fetches GoogleTest (`v1.17.0`) via `FetchContent`.

## Run a focused binary

The default target produces a single `Tests_run` executable. Pass `--gtest_filter` to narrow:

```bash title="terminal"
./build/test/Tests_run --gtest_filter=RouterSpec.*
./build/test/Tests_run --gtest_filter='Middleware/*:Application/HealthChecksSpec.*'
```

`--gtest_filter` accepts the `Suite.Subcase` form, wildcard `*`, and `:` for OR.

## Anatomy of a spec

Specs follow the `*Spec.cpp` convention and live next to the code they cover:

| Path | Covers |
|---|---|
| `test/src/Http/RouterSpec.cpp` | Router matching, params, prefix groups |
| `test/src/Application/HealthChecksSpec.cpp` | `MapHealthChecks` aggregation |
| `test/src/Middleware/CompressionSpec.cpp` | `CompressionMiddleware` negotiation |

A typical spec is short and focused:

```cpp title="test/src/Example/SampleSpec.cpp" linenums="1"
#include <gtest/gtest.h>

TEST(SampleSpec, AlwaysPasses)
{
    EXPECT_EQ(1 + 1, 2);
}
```

Files placed under `test/src/` are picked up automatically by the `GLOB_RECURSE` in `test/CMakeLists.txt` — no manual registration needed.

## Next steps

- Browse the [Usage overview](../usage/overview.md) for cross-cutting concerns.
- See the [Examples](../authoring/examples.md) page for end-to-end programs you can build on.
- Read [Contributing](../community/contributing.md) for the project's coding and documentation conventions.