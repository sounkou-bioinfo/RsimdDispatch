# RsimdDispatch — Design Review and Proposed Fixes

------------------------------------------------------------------------

## 1. Package Purpose and Dual Role

### Finding

The package serves two distinct roles simultaneously:

1.  A **working R package** that demonstrates runtime SIMD dispatch
    (exporting `count_nonzero`, `simd_backend`, etc.).
2.  A **scaffolding/template tool** that downstream packages copy via
    [`use_simd_dispatch()`](https://sounkou-bioinfo.github.io/RsimdDispatch/reference/simd_dispatch_template_path.md).

The exported R functions
[`simd_dispatch_template_path()`](https://sounkou-bioinfo.github.io/RsimdDispatch/reference/simd_dispatch_template_path.md)
and
[`use_simd_dispatch()`](https://sounkou-bioinfo.github.io/RsimdDispatch/reference/simd_dispatch_template_path.md)
are developer-tooling functions that do not belong in a normal
user-facing package API. They clutter the namespace and are confusing to
end users who install the package for the dispatch demonstration rather
than as a scaffolding dependency.

### Proposed Fix

Move
[`simd_dispatch_template_path()`](https://sounkou-bioinfo.github.io/RsimdDispatch/reference/simd_dispatch_template_path.md)
and
[`use_simd_dispatch()`](https://sounkou-bioinfo.github.io/RsimdDispatch/reference/simd_dispatch_template_path.md)
into a dedicated `R/zzz_template.R` file and mark them clearly as
developer utilities. Add a `Lifecycle: experimental` or equivalent note.
Alternatively, gate them behind a `Suggests: usethis` dependency so they
are only available in a dev context. At minimum, add a
`@section Developer utilities:` roxygen tag to separate them visually
from user-facing exports.

------------------------------------------------------------------------

## 2. Template Strategy

### 2.1 Template is a static copy with no placeholder substitution

#### Finding

`inst/templates/dispatch-c/` is a near-verbatim duplicate of `src/` and
`R/`. The template files contain no placeholders. The README says
“rename the `rsd_` and `RC_` symbols”, but that is entirely manual.
Concrete strings a downstream adopter must find-replace by hand:

| File | String to change |
|----|----|
| `src/registration.c:18` | `R_init_RsimdDispatch` → `R_init_<PKGNAME>` |
| `R/dispatch.R:1` | `@useDynLib RsimdDispatch` → `@useDynLib <PKGNAME>` |
| `R/dispatch.R:82–107` | Remove [`simd_dispatch_template_path()`](https://sounkou-bioinfo.github.io/RsimdDispatch/reference/simd_dispatch_template_path.md) / [`use_simd_dispatch()`](https://sounkou-bioinfo.github.io/RsimdDispatch/reference/simd_dispatch_template_path.md) entirely |
| All `src/*.c` / `src/*.h` | `rsd_` prefix → package-specific prefix |
| `tools/configure-simd-dispatch.sh:139` | `"RsimdDispatch configure:"` → package name |

There is no rename parameter, no sed substitution, no checklist.
Adopters routinely miss `R_init_RsimdDispatch` in `registration.c`,
producing a silent broken DLL.

#### Proposed Fix

Add a `pkg` argument to
[`use_simd_dispatch()`](https://sounkou-bioinfo.github.io/RsimdDispatch/reference/simd_dispatch_template_path.md)
and perform token substitution on copy:

``` r

use_simd_dispatch <- function(path = ".", pkg = basename(normalizePath(path)),
                               prefix = tolower(pkg), overwrite = FALSE) {
  template <- simd_dispatch_template_path()
  path <- normalizePath(path, mustWork = TRUE)
  files <- list.files(template, recursive = TRUE, all.files = TRUE,
                      no.. = TRUE, full.names = FALSE)
  # Only file entries are returned by list.files; remove dead dir.exists branch
  copied <- character()
  for (file in files) {
    from <- file.path(template, file)
    to   <- file.path(path, file)
    dir.create(dirname(to), recursive = TRUE, showWarnings = FALSE)
    if (file.exists(to) && !isTRUE(overwrite))
      stop("refusing to overwrite existing file: ", to, call. = FALSE)
    txt <- readLines(from, warn = FALSE)
    txt <- gsub("RsimdDispatch", pkg,    txt, fixed = TRUE)
    txt <- gsub("rsd_",          paste0(prefix, "_"), txt, fixed = TRUE)
    txt <- gsub("RC_",           paste0(toupper(prefix), "_C_"), txt, fixed = TRUE)
    writeLines(txt, to)
    copied <- c(copied, to)
  }
  invisible(copied)
}
```

Also ship a `inst/templates/dispatch-c/CHECKLIST.md` enumerating every
substitution point for users who copy manually.

### 2.2 Template duplicates live source — no sync mechanism

#### Finding

`inst/templates/dispatch-c/src/simd_dispatch.c` is byte-for-byte
identical to `src/simd_dispatch.c`. The same holds for
`cpu_features.c/h`, all `kernel_*.c`, `r_api.c`, `registration.c`,
`Makevars.in`, `configure`, `configure.win`, and
`configure-simd-dispatch.sh`. Any bug fixed in the live `src/` must be
manually mirrored in the template. No CI step detects drift.

#### Proposed Fix

Add a Makefile target and CI check that diffs the two trees:

``` makefile
check-template-sync:
    @diff -rq --exclude="*.o" --exclude="*.so" --exclude="Makevars" \
        src/ inst/templates/dispatch-c/src/ \
        || (echo "ERROR: template/src drift detected" && exit 1)
```

Add this to the CI `R-CMD-check.yaml` as a pre-check step:

``` yaml
- name: Check template sync
  run: make check-template-sync
```

Longer term, generate the template from `src/` at build time (applying
the `rsd_` → `@PREFIX@` substitution), so there is only one source of
truth.

### 2.3 Template R file should not contain template-helper functions

#### Finding

`inst/templates/dispatch-c/R/dispatch.R` contains
[`simd_dispatch_template_path()`](https://sounkou-bioinfo.github.io/RsimdDispatch/reference/simd_dispatch_template_path.md)
and
[`use_simd_dispatch()`](https://sounkou-bioinfo.github.io/RsimdDispatch/reference/simd_dispatch_template_path.md).
A downstream package that forgets to remove these will export broken
stubs that call `system.file(..., package = "RsimdDispatch")` and fail
silently unless RsimdDispatch happens to be installed.

#### Proposed Fix

Strip these functions from the template file. The template
`R/dispatch.R` should contain only:

- `@useDynLib <PKGNAME>, .registration = TRUE`
- The user-facing kernel wrapper(s) (`count_nonzero` or the renamed
  equivalent)
- [`simd_set_backend()`](https://sounkou-bioinfo.github.io/RsimdDispatch/reference/simd_set_backend.md),
  [`simd_backend()`](https://sounkou-bioinfo.github.io/RsimdDispatch/reference/simd_backend.md),
  [`simd_info()`](https://sounkou-bioinfo.github.io/RsimdDispatch/reference/simd_info.md)

### 2.4 `use_simd_dispatch()` dead code — `dir.exists(from)` branch unreachable

#### Finding

`R/dispatch.R:96`:

``` r

if (dir.exists(from)) {
  dir.create(to, recursive = TRUE, showWarnings = FALSE)
  next
}
```

`list.files(recursive = TRUE)` returns only file paths, never directory
entries. This branch is never entered.

#### Proposed Fix

Remove the dead branch. The `dir.create(dirname(to), ...)` call on the
line below already handles creating any needed intermediate directories.

``` r

# Before (dead branch present):
if (dir.exists(from)) {
  dir.create(to, recursive = TRUE, showWarnings = FALSE)
  next
}
dir.create(dirname(to), recursive = TRUE, showWarnings = FALSE)

# After:
dir.create(dirname(to), recursive = TRUE, showWarnings = FALSE)
```

------------------------------------------------------------------------

## 3. Configure Script Issues

### 3.1 `SIMDE_INCLUDE` hardcoded to `inst/include`

#### Finding

`tools/configure-simd-dispatch.sh:37`:

``` sh
SIMDE_INCLUDE="-I$ROOT/inst/include"
```

This is correct for RsimdDispatch itself, but the template ships the
identical line. A downstream package relying on RsimdDispatch’s vendored
SIMDe via `LinkingTo` has SIMDe headers under the R library tree, not
under its own `inst/include`. Because `check_cflag` probes with an empty
`main` (see §3.2), flag detection silently succeeds regardless, then the
downstream package fails to compile because the include path is absent.

#### Proposed Fix

Make the SIMDe include path configurable with a sensible auto-detect
fallback:

``` sh
# In configure-simd-dispatch.sh, replace the hardcoded line:
SIMDE_INCLUDE="-I$ROOT/inst/include"

# With:
if [ -z "${RSD_SIMDE_INCLUDE:-}" ]; then
    if [ -d "$ROOT/inst/include/simde" ]; then
        RSD_SIMDE_INCLUDE="-I$ROOT/inst/include"
    elif [ -n "${R_HOME:-}" ]; then
        # Try LinkingTo path from an installed RsimdDispatch
        _rsd_inc=$("${R_HOME}/bin/Rscript" -e \
            'cat(system.file("include", package="RsimdDispatch"))' 2>/dev/null || true)
        RSD_SIMDE_INCLUDE=${_rsd_inc:+"-I${_rsd_inc}"}
    fi
fi
SIMDE_INCLUDE=${RSD_SIMDE_INCLUDE:-""}
```

Document `RSD_SIMDE_INCLUDE` as an overrideable environment variable in
the template README.

### 3.2 `check_cflag` does not include SIMDe headers in the probe

#### Finding

``` sh
cat > "$CONFDIR/conftest.c" <<'EOF'
int main(void) { return 0; }
EOF
```

The probe file is empty of any `#include`. It only tests that the
compiler accepts a `-m` flag, not that SIMDe compiles under it. Some
SIMDe headers trigger errors under specific flag combinations or with
strict warning flags.

#### Proposed Fix

Include the relevant SIMDe header in the probe:

``` sh
check_cflag() {
    flags=$1
    header=${2:-}
    {
        if [ -n "$header" ]; then
            printf '#include <%s>\n' "$header"
        fi
        printf 'int main(void) { return 0; }\n'
    } > "$CONFDIR/conftest.c"
    # shellcheck disable=SC2086
    ${CC} ${CPPFLAGS:-} ${CFLAGS:-} ${SIMDE_INCLUDE} ${flags} \
        -c "$CONFDIR/conftest.c" -o "$CONFDIR/conftest.o" >/dev/null 2>&1
}

# Usage:
if check_cflag "-mavx2" "simde/x86/avx2.h"; then ...
```

### 3.3 `uname -m` detects host arch, not compiler target

#### Finding

``` sh
arch=$(uname -m 2>/dev/null || echo unknown)
```

On a cross-compilation host (e.g. building for arm64 macOS on x86_64
Linux), `uname -m` returns the host arch and enables the wrong backend
set. CRAN’s arm64 macOS builders can exercise this path.

#### Proposed Fix

Query the compiler for its target triple first, fall back to `uname -m`:

``` sh
arch=$( ${CC} -dumpmachine 2>/dev/null | sed 's/-.*//' || uname -m 2>/dev/null || echo unknown )
```

`-dumpmachine` is supported by GCC and Clang and returns the target
triple regardless of the host. Strip everything after the first `-` to
get just the CPU name.

### 3.4 Log message hardcodes package name

#### Finding

`configure-simd-dispatch.sh:139`:

``` sh
echo "RsimdDispatch configure: objects='kernel_scalar.o ${RSD_OBJECTS}'"
echo "RsimdDispatch configure: wrote ${CONFIG_OUT} and ${MAKEVARS_OUT}"
```

When the template is copied to `MyPkg`, the configure output confusingly
says `"RsimdDispatch configure:"` during installation.

#### Proposed Fix

Make the log prefix configurable via an environment variable with a
default:

``` sh
RSD_LOG_PREFIX=${RSD_LOG_PREFIX:-RsimdDispatch}
# ...
echo "${RSD_LOG_PREFIX} configure: objects='kernel_scalar.o ${RSD_OBJECTS}'"
echo "${RSD_LOG_PREFIX} configure: wrote ${CONFIG_OUT} and ${MAKEVARS_OUT}"
```

In the template’s `configure` script, set:

``` sh
RSD_LOG_PREFIX="$(basename "$ROOT")" \
RSD_PACKAGE_ROOT="$ROOT" \
...
sh "$ROOT/tools/configure-simd-dispatch.sh"
```

### 3.5 No `Makevars.win.in` — asymmetric configure pair

#### Finding

`configure.win` passes `RSD_MAKEVARS_OUT=src/Makevars.win` but still
reads from `src/Makevars.in`. This works today because the content is
platform-identical. If Windows-specific compiler flags are ever needed
(MSVC `/arch:AVX2` vs GCC `-mavx2`, PE export attributes, etc.), there
is no clean path to diverge.

#### Proposed Fix

Add a `src/Makevars.win.in` that is initially identical to
`src/Makevars.in`, and have `configure.win` explicitly request it:

``` sh
# configure.win
RSD_PACKAGE_ROOT="$ROOT" \
RSD_MAKEVARS_IN=src/Makevars.win.in \
RSD_MAKEVARS_OUT=src/Makevars.win \
sh "$ROOT/tools/configure-simd-dispatch.sh"
```

Update `cleanup` and `Makefile clean` to also remove `src/Makevars.win`.

### 3.6 `append_object` breaks when paths contain spaces

#### Finding

Object list is built by string concatenation:

``` sh
RSD_OBJECTS="$RSD_OBJECTS $1"
```

If `ROOT` or `TMPDIR` contains spaces, the `check_cflag` invocation
silently misinterprets word-split arguments, causing all flag checks to
return false (everything falls back to scalar), with no warning emitted.

#### Proposed Fix

Quote the conftest paths consistently and validate `ROOT` at script
entry:

``` sh
# Validate early
case "$ROOT" in
    *\ *) echo "WARNING: package root contains spaces; SIMD detection may be unreliable" >&2 ;;
esac
```

For the conftest itself, ensure all paths are quoted — the existing
`"$CONFDIR/conftest.c"` is correct but the `${SIMDE_INCLUDE}` and flag
variables are unquoted in the compiler invocation. Since these are
intentionally word-split (multiple flags), this is acceptable, but
`$ROOT` in `SIMDE_INCLUDE` must not contain spaces.

------------------------------------------------------------------------

## 4. C Dispatch Layer Generality

### 4.1 Single kernel hardcoded throughout

#### Finding

`simd_dispatch.h` exposes exactly one dispatchable function:

``` c
typedef size_t (*rsd_count_nonzero_fn)(const uint8_t *x, size_t n);
size_t rsd_count_nonzero(const uint8_t *x, size_t n);
```

Every backend file, the dispatch table, and the R API are coupled to
this one signature. A package wanting to dispatch multiple kernels with
different signatures must duplicate the entire infrastructure.

#### Proposed Fix

Document explicitly in the template README that each distinct kernel
signature requires its own `simd_dispatch_<name>.c` /
`simd_dispatch_<name>.h` pair, and provide a skeleton for a second
kernel. For a stronger solution, introduce an X-macro table that lists
all backends once and generates the switch logic:

``` c
/* backends.def — included by simd_dispatch.c, cpu_features.c, etc. */
/* X(name, have_macro, cpu_check_fn) */
X(scalar,  1,              1)
X(sse2,    RSD_HAVE_SSE2,  rsd_cpu_has_sse2())
X(sse41,   RSD_HAVE_SSE41, rsd_cpu_has_sse41())
X(avx2,    RSD_HAVE_AVX2,  rsd_cpu_has_avx2())
X(avx512,  RSD_HAVE_AVX512,rsd_cpu_has_avx512())
X(neon,    RSD_HAVE_NEON,  rsd_cpu_has_neon())
```

Then `rsd_backend_known`, `rsd_backend_compiled`, and
`rsd_backend_cpu_supported` each become:

``` c
int rsd_backend_known(const char *b) {
#define X(name, have, cpu) if (strcmp(b, #name) == 0) return 1;
#include "backends.def"
#undef X
    return 0;
}
```

This reduces the 10-location update problem to a single `backends.def`
edit.

### 4.2 `rsd_backends_csv` rotating static buffers are not thread-safe

#### Finding

``` c
static char bufs[3][128];
static int next = 0;
```

Concurrent callers race on
[`next`](https://rdrr.io/r/base/Control.html). In R’s single-threaded
model this is usually harmless, but the functions are also exported in
the template for downstream use where threading guarantees differ.

#### Proposed Fix

Since `RC_simd_info` (the only public caller) already builds proper
`STRSXP` character vectors directly, the CSV functions are redundant as
public API. Mark them `static` (internal linkage) or remove them. If
they must remain for diagnostics/debugging, replace with caller-supplied
buffers:

``` c
/* Caller owns the buffer; no static state. */
void rsd_compiled_backends_csv(char *out, size_t cap);
```

### 4.3 Dispatch is not thread-safe

#### Finding

``` c
static rsd_count_nonzero_fn rsd_count_nonzero_impl = rsd_count_nonzero_scalar;
```

No memory barrier or atomic around reads or writes. A `fork()`ed worker
(from [`parallel::mclapply`](https://rdrr.io/r/parallel/mclapply.html))
that inherits the initialized pointer is safe, but a worker that races
[`simd_set_backend()`](https://sounkou-bioinfo.github.io/RsimdDispatch/reference/simd_set_backend.md)
with `rsd_count_nonzero()` may read a torn pointer.

#### Proposed Fix

For C11 targets, use `_Atomic`:

``` c
#include <stdatomic.h>
static _Atomic rsd_count_nonzero_fn rsd_count_nonzero_impl;
/* write: */ atomic_store(&rsd_count_nonzero_impl, fn);
/* read:  */ return atomic_load(&rsd_count_nonzero_impl)(x, n);
```

For pre-C11 portability, add a `volatile` annotation and document the
limitation. In the template, note that
[`simd_set_backend()`](https://sounkou-bioinfo.github.io/RsimdDispatch/reference/simd_set_backend.md)
is not safe to call concurrently with the kernel.

### 4.4 Backend name buffer size is a magic number

#### Finding

``` c
static char rsd_requested_backend_buf[32] = "auto";
static char rsd_selected_backend_buf[32] = "uninitialized";
```

The size `32` appears three times across the file with no named
constant. A backend name longer than 31 bytes would be silently
truncated.

#### Proposed Fix

``` c
#define RSD_BACKEND_NAME_MAX 32
static char rsd_requested_backend_buf[RSD_BACKEND_NAME_MAX] = "auto";
static char rsd_selected_backend_buf[RSD_BACKEND_NAME_MAX] = "uninitialized";
/* snprintf calls use sizeof(rsd_requested_backend_buf) already — keep as-is */
```

Also add a `static_assert` (C11) or a compile-time sizeof check to
verify the longest known backend name fits:

``` c
/* longest current name: "uninitialized" = 13 chars */
typedef char rsd_backend_name_size_check[
    (RSD_BACKEND_NAME_MAX > 13) ? 1 : -1];
```

### 4.5 `simd_dispatch.c` includes `<R.h>` only for `Rf_error`

#### Finding

The dispatch layer depends on R’s error mechanism, making it
non-portable to non-R C callers (unit test harnesses, embedding, etc.).

#### Proposed Fix

Add a pluggable error callback:

``` c
/* In simd_dispatch.h */
typedef void (*rsd_error_fn)(const char *msg);
void rsd_set_error_handler(rsd_error_fn fn);  /* NULL = use default (abort) */
```

In `r_api.c` (R-specific layer), register `Rf_error` as the handler
during `R_init_<PKGNAME>`. This keeps the dispatch core free of R
headers.

------------------------------------------------------------------------

## 5. R Layer Issues

### 5.1 `simd_set_backend()` validates in R and again in C redundantly

#### Finding

`dispatch.R:39` uses
[`match.arg()`](https://rdrr.io/r/base/match.arg.html) which rejects
unknown strings. The C function then independently validates with
`rsd_backend_known`, `rsd_backend_compiled`,
`rsd_backend_cpu_supported`. The R-side list must be kept in sync with
the C-side list by hand.

#### Proposed Fix

Either remove the R-side `match.arg` and rely on the C error (which
already produces a clear message), or generate the R-side vector from
`simd_info()$compiled_backends` at package load time:

``` r

simd_set_backend <- function(backend = "auto") {
  backend <- match.arg(backend, c("auto", simd_info()$compiled_backends))
  invisible(.Call(RC_simd_set_backend, backend))
}
```

This way the valid set is always in sync with what was actually
compiled.

### 5.2 `count_nonzero` returns `double` instead of `integer`

#### Finding

`r_api.c:31`: `return Rf_ScalarReal((double)count)`. For typical vectors
the count fits in a 32-bit integer. Returning `double` forces downstream
comparisons like `count_nonzero(x) == length(x)` to use floating-point
equality.

#### Proposed Fix

Return an integer when the count fits, double otherwise:

``` c
if (count <= (size_t)INT_MAX) {
    return Rf_ScalarInteger((int)count);
}
return Rf_ScalarReal((double)count);
```

Or consistently return `double` but document this choice explicitly. The
current behaviour should at minimum be documented in the roxygen
`@return` tag.

------------------------------------------------------------------------

## 6. CI and Build System

### 6.1 GitHub checkout action version

#### Finding

The check workflows should use an existing stable checkout action tag.

#### Status

Resolved in the workflow files:

``` yaml
- uses: actions/checkout@v4
```

### 6.2 ARM CI coverage

#### Finding

ARM backends should be covered in CI, especially the NEON path.

#### Status

Resolved in `R-CMD-check.yaml` by adding Linux ARM and Windows ARM
runners:

``` yaml
- {os: windows-11-arm,   r: 'release'}
- {os: ubuntu-24.04-arm, r: 'release'}
```

### 6.3 No CI installation test for the template

#### Finding

There is no job that calls
[`use_simd_dispatch()`](https://sounkou-bioinfo.github.io/RsimdDispatch/reference/simd_dispatch_template_path.md),
copies the template to a temp package, and runs `R CMD INSTALL` on it.
Template drift and renaming failures go undetected.

#### Proposed Fix

Add a dedicated workflow (or a step in the existing check) that:

``` yaml
- name: Install copied template package
  run: |
    Rscript -e '
      tmp <- file.path(tempdir(), "TestSIMD")
      dir.create(tmp)
      writeLines(c(
        "Package: TestSIMD", "Version: 0.0.1", "License: GPL-2"
      ), file.path(tmp, "DESCRIPTION"))
      RsimdDispatch::use_simd_dispatch(tmp, pkg = "TestSIMD")
    '
    R CMD INSTALL "$TMPDIR/TestSIMD"
```

### 6.4 `make build` always regenerates `inst/AUTHORS`

#### Finding

``` makefile
build: update-authors
```

`update-authors` runs a network-touching `tools/update-authors.R` on
every `make build`, modifying `inst/AUTHORS`. This makes builds
non-reproducible if the upstream author list changes between runs.

#### Proposed Fix

Decouple author regeneration from build:

``` makefile
build:
    R CMD build .

authors: update-authors

update-authors:
    Rscript tools/update-authors.R
```

Run `make authors` explicitly before a CRAN submission. Commit the
resulting `inst/AUTHORS` file to version control so the build is
reproducible without a network call.

------------------------------------------------------------------------

## 7. Minor / Style

### 7.1 NEON kernel uses lane-store loop instead of horizontal add

`kernel_neon.c` does not use `rsd_popcount` or a horizontal NEON add:

``` c
simde_vst1q_u8(lanes, eq);
for (int j = 0; j < 16; ++j) {
    acc += lanes[j] == 0;  /* note: eq byte is 0xFF when equal, 0x00 when not */
}
```

The loop is correct but slower than `vaddvq_u8` (available in AArch64)
or a pair of `vpaddlq_u8` / `vaddlvq_u16`. This is intentional for
32-bit ARM compatibility but worth documenting.

**Fix:** Add a comment noting why the lane-store loop is used instead of
`vaddvq_u8`, and add a `#ifdef __aarch64__` fast path for AArch64.

### 7.2 `kernel_neon.c` missing `kernel_common.h` dependency in `Makevars.in`

`Makevars.in` lists `kernel_common.h` as an explicit dependency for all
x86 kernels but not for `kernel_neon.c` or `kernel_scalar.c`. This is
technically correct today (neither file includes `kernel_common.h`), but
if either file gains a `kernel_common.h` include in future, `make` will
not rebuild it. Consistent listing aids correctness.

**Fix:** Either add `kernel_common.h` to the `kernel_neon.o` and
`kernel_scalar.o` rules, or add a comment explaining the omission is
intentional.

### 7.3 `rsd_backends_csv` logic is inverted

`simd_dispatch.c:224–230`:

``` c
if (want_available) {
    include = rsd_backend_available(name);
} else if (want_compiled) {
    include = rsd_backend_compiled(name);
} else if (want_supported) {
    include = rsd_backend_cpu_supported(name);
}
```

The `want_available` branch is checked first even though
`rsd_compiled_backends_csv` passes `(1,0,0)` and
`rsd_available_backends_csv` passes `(0,0,1)`. The logic produces
correct results, but the flag priority order (available \> compiled \>
supported) is the inverse of what the callers suggest, and the three
flags are not mutually exclusive, which is a latent bug if a caller
passes `(1,0,1)`.

**Fix:** Replace the three booleans with a single enum or replace the
function with three distinct static helpers, one per query type.

### 7.4 Template `configure-simd-dispatch.sh` in `tools/` but accessed as `$ROOT/tools/...`

The template’s `configure` script calls
`sh "$ROOT/tools/configure-simd-dispatch.sh"`. When the template is
copied to a downstream package that does not already have a `tools/`
directory, the script path is correct. However, if the downstream
package has its own `tools/configure-simd-dispatch.sh` (from a previous
copy), a diverged version may be executed silently. The path should be
documented explicitly.
