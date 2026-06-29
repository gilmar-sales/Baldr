# AGENTS.md

This file gives Kilo (and other AI coding agents) the conventions and commands used in this repository.

## Build (C++)

Baldr uses CMake. Configure and build from the repo root:

```bash
cmake -S . -B build
cmake --build build --config Release
```

This builds the `baldr` library and all examples under `examples/`. To build without examples:

```bash
cmake -S . -B build -DBALDR_BUILD_EXAMPLES=OFF
cmake --build build
```

CMake requires **3.28+** and a **C++20** compiler (GCC 11+, Clang 14+, MSVC 19.30+).

## Tests

Tests are under `test/` and built when `BALDR_BUILD_TESTS=ON` (default when Baldr is the top-level project):

```bash
ctest --test-dir build --output-on-failure
```

## Documentation

This repository's documentation site lives in `docs/` and is built with [Zensical](https://zensical.org). Site configuration is in `zensical.toml` at the repo root.

### Preview locally

```bash
pip install zensical
zensical serve
```

The preview server defaults to `http://localhost:8000`.

### Build the static site

```bash
zensical build --clean
```

The generated site is written to `site/` (ignored by git).

### Authoring conventions

- Use kebab-case filenames (`get-started.md`, `dependency-injection.md`).
- Keep pages focused — one concept per page.
- Reference source files and example programs with GitHub permalinks so snippets stay traceable.
- Code fences must include a `title=` attribute and `linenums="1"` for long snippets.
- Use Material admonitions (`!!! note`, `!!! tip`, `!!! warning`) for callouts.
- Use Material grid cards (`<div class="grid cards" markdown>`) for "next steps" sections.

### Publishing

Pushes to `main` that touch `docs/**`, `zensical.toml`, or `.github/workflows/docs.yml` trigger `.github/workflows/docs.yml`, which builds the site and deploys it to GitHub Pages via `actions/deploy-pages`. The deployment requires **Settings → Pages → Source = "GitHub Actions"** in the repository settings.