# Contributing to cnetBoot

Thank you for improving cnetBoot. Keep changes reusable, provider-neutral, and free of
industry-specific behavior.

## Before changing code

1. Read `AGENTS.md` and the complete `3rdparty/cnetmod/AGENTS.md`.
2. Initialize and prebuild the pinned cnetmod submodule.
3. Keep the existing controller, DTO, service, domain, mapper, and composition-root boundaries.

## Build and test

On WSL with Clang:

```bash
./script/wsl_clang_3rdparty_build.sh
CNETBOOT_BUILD_TESTING=ON ./script/wsl_clang_build.sh
ctest --test-dir backend/build-clang --output-on-failure -j2
```

On Windows with MSVC:

```powershell
.\script\windows_3rdparty_build.ps1
.\script\windows_build.ps1 -BuildTests
ctest --test-dir backend/build-windows --output-on-failure -j2
```

The mandatory architecture gate runs as part of every default build. Pull requests should include
tests for behavior changes and must not add secrets, vertical-business vocabulary, or CJK text to
source string literals.
