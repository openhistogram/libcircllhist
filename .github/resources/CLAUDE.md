# libcircllhist — PR Review Context

Context for the automated Claude PR review (`.github/workflows/claude-review.yml`).
Read this first to ground feedback in how this repo actually works, cut
redundant exploration, and avoid false positives.

## What this is

**circllhist** — a C implementation of **OpenHistogram log-linear histograms**
(Apache-2.0, originally Circonus). A small, widely-embedded C library: the same
histogram type is consumed downstream by TSDBs (e.g. IRONdb/snowth) and by Lua
and Python bindings, so its **public API, struct layout, and serialized format
are contracts**, not internal details.

- Core: `src/circllhist.c` (the implementation) and `src/circllhist.h` (the
  public API/header); `src/circllhist_print.c` (formatting/printing helpers).
- **Vendored numerics:** `src/dcdflib.c`, `src/cdflib.h`, `src/ipmpar.c` are the
  public-domain **DCDFLIB** CDF/quantile routines (transliterated-Fortran C
  style). Third-party, not this project's code.
- Build: **GNU autotools** — `autoconf` generates `configure` from
  `configure.ac`; `Makefile.in`/`src/Makefile.in`; `make`, `make tests`,
  `make docs` (Doxygen via `Doxyfile`).
- **Bindings** in `src/lua/` (LuaJIT FFI) and `src/python/` (cffi package),
  produced/prepared by `src/prepareFFI.sh`, `src/generateLuaFiles.sh`,
  `src/generatePythonFiles.sh` from the public header.
- Tests: `src/test/` (`histogram_test.c`, `histogram_perf.c`,
  `circllhist_test.lua`, `histogram_c_test.lua`, `runTest.sh`).
- CI already present: legacy `.travis.yml`, and `.github/workflows/publish-to-pypi.yml`
  (publishes the Python bindings). Leave those unless the PR changes them.

## Review focus (in priority order)

1. **C memory safety and correctness (highest).** Bin/bucket array indexing and
   bounds, integer overflow in bucket exponent/value arithmetic, unchecked
   `malloc`/`realloc`/`calloc`, use-after-free / double-free, off-by-one. This is
   a C library whose job is correct accumulation — these are the bugs that crash
   or corrupt callers.
2. **Histogram / numerical correctness.** The log-linear bucket scheme
   (exponent + bin), value→bucket mapping, and quantile/mean/sum/CDF computation
   are the heart of the library; watch edge cases (zero, negative, subnormal,
   inf/nan, empty histogram) and precision. A subtle math error silently corrupts
   every downstream metric.
3. **Serialized-format & ABI compatibility.** The on-wire/serialized histogram
   forms and the public structs/functions in `circllhist.h` are consumed by the
   FFI bindings and by downstream stores. Renaming/removing/retyping a public
   symbol, changing struct layout, or changing the serialized encoding breaks
   stored data and every binding — treat as high severity. Additive changes are
   safer than changes to existing layout.
4. **Binding sync.** The Lua/Python bindings and their FFI cdefs are derived from
   `circllhist.h` via the `prepare/generate*` scripts. A change to the public
   header must be reflected in the bindings, or they drift and can segfault at
   the FFI boundary — flag a header change that leaves the bindings untouched.
5. **Portability.** Targets gcc/clang across platforms (note the `WIN32`
   `ssize_t` shim in the header); flag non-portable assumptions (endianness,
   word size, GNU-only extensions) and anything that breaks the autotools build.
6. **Hot-path performance.** Insert and quantile are called at high frequency in
   TSDBs; watch for per-insert allocations or linear scans where the bucket
   structure allows better.

## Known-intentional — do NOT flag as issues

- **The vendored DCDFLIB** (`dcdflib.c`, `cdflib.h`, `ipmpar.c`): public-domain
  third-party numerics with ALL-CAPS names, `goto`s, and single-letter
  variables. Not this project's code — do not restyle, modernize, or relitigate
  it; review only genuine integration bugs where the core calls into it.
- **Autotools generated/vendored files:** `config.guess`, `config.sub`,
  `install-sh`, `buildtools/mkinstalldirs`, and the generated `configure` (not
  committed) — not review targets.
- **Portable/older C idioms** in the core (conservative C, explicit memory
  management, no C11-isms) are deliberate for a widely-embedded library — don't
  push modern-C rewrites.
- **Serialized bucket layout and bin constants** are deliberate and
  compatibility-critical; don't propose "optimizing" the bucket scheme or
  encoding.
- **Committed binding artifacts** in `src/lua/` and `src/python/` are expected
  (they ship with the library); the review concern is that they stay in sync
  with the header (focus 4), not that they exist.
- **Apache-2.0 license headers** on every file are intentional.
