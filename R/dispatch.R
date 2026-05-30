#' @useDynLib RsimdDispatch, .registration = TRUE
NULL

#' Count non-zero bytes with the selected SIMD backend
#'
#' Demonstration kernel for the runtime dispatch template. `count_nonzero()`
#' counts bytes that are not `00` in a raw vector using the currently selected
#' backend. The default backend is `"auto"`, which selects the best compiled
#' backend supported by the current CPU/runtime.
#'
#' @param x A raw vector.
#' @return A `double` scalar giving the count of non-zero bytes. The return
#'   type is `double` rather than `integer` to accommodate vectors longer than
#'   `.Machine$integer.max` without overflow.
#' @examples
#' count_nonzero(as.raw(c(0, 1, 0, 2)))
#' @export
count_nonzero <- function(x) {
  if (!is.raw(x)) {
    stop("x must be a raw vector", call. = FALSE)
  }
  .Call(RC_count_nonzero, x)
}

#' Full one-dimensional convolution with the selected SIMD backend
#'
#' Demonstration numeric kernel for the runtime dispatch template.
#' `convolve1d()` computes the same full convolution as a simple nested-loop R
#' definition. For each pair of positions it adds `a[i] * b[j]` to
#' `out[i + j - 1]`. SIMD backends vectorize the inner multiply-add over `b`
#' and the shifted output window.
#'
#' @param a,b Numeric vectors.
#' @return A numeric vector of length `length(a) + length(b) - 1`, or
#'   `numeric(0)` when either input is empty.
#' @examples
#' convolve1d(c(1, 2, 3), c(10, 100))
#' @export
convolve1d <- function(a, b) {
  a <- as.double(a)
  b <- as.double(b)
  .Call(RC_convolve1d, a, b)
}

#' Select the runtime SIMD backend
#'
#' Select the backend used by subsequent calls to dispatched demo kernels such
#' as `count_nonzero()` and `convolve1d()`. `RsimdDispatch` keeps all compiled
#' variants in one shared object and switches a guarded resolved operation
#' table. This makes same-process benchmarking possible.
#'
#' @param backend Character scalar. Use `"auto"` to select the best available
#'   backend, or one of `simd_info()$available_backends` for an explicit choice.
#' @return The selected backend summary string, invisibly. Possible values:
#'   \describe{
#'     \item{`"<name>"`}{All operations resolved to the same named backend.}
#'     \item{`"mixed"`}{Different operations resolved to different backends
#'       (only possible under `"auto"`).}
#'     \item{`"unavailable"`}{No operation could be resolved for the requested
#'       backend.}
#'   }
#' @examples
#' old <- simd_backend()
#' simd_set_backend("scalar")
#' simd_set_backend("auto")
#' @export
simd_set_backend <- function(backend = "auto") {
  if (!is.character(backend) || length(backend) != 1L || is.na(backend) || !nzchar(backend)) {
    stop("backend must be a non-empty character scalar", call. = FALSE)
  }
  invisible(.Call(RC_simd_set_backend, backend))
}

#' Report the currently selected SIMD backend
#'
#' @return A character scalar naming the selected backend summary.
#' @examples
#' simd_backend()
#' @export
simd_backend <- function() {
  .Call(RC_simd_backend)
}

#' Report runtime SIMD dispatch diagnostics
#'
#' Returns the requested backend, selected backend, compiled backends,
#' CPU-supported backends, operation-level backend availability, operation-level
#' selected backends, SIMDe-native backends, target information, and SIMDe
#' provenance compiled into the shared library. Calling this initializes the lazy
#' auto-dispatch selection if it has not already been initialized.
#'
#' @return A named list with the following elements:
#'   \describe{
#'     \item{`dispatch_mode`}{Character scalar. Internal dispatch implementation
#'       strategy string (stable across patch releases).}
#'     \item{`requested_backend`}{Character scalar. The last value passed to
#'       `simd_set_backend()`, or `"auto"` if not explicitly set.}
#'     \item{`selected_backend`}{Character scalar. Summary of the currently
#'       active backend: a backend name, `"mixed"`, or `"unavailable"`.}
#'     \item{`compiled_backends`}{Character vector. Backends compiled into
#'       the shared library (not necessarily supported by the current CPU).}
#'     \item{`cpu_supported_backends`}{Character vector. Backends whose
#'       required CPU features are present at runtime.}
#'     \item{`available_backends`}{Character vector. Backends that are both
#'       compiled and CPU-supported (i.e. usable).}
#'     \item{`simde_native_backends`}{Character vector. Backends for which
#'       the compiler emitted native intrinsics rather than SIMDe emulation.}
#'     \item{`operations`}{Character vector. Names of registered operations
#'       (e.g. `"count_nonzero"`, `"convolve1d"`).}
#'     \item{`operation_backends`}{Named list of character vectors. For each
#'       operation, the backends that provide a kernel for it.}
#'     \item{`operation_selected_backends`}{Named character vector. For each
#'       operation, the backend currently selected for dispatch (or `NA` if
#'       none is resolved).}
#'     \item{`cpu_sse2`}{Logical scalar. `TRUE` if SSE2 is detected at runtime.}
#'     \item{`cpu_sse41`}{Logical scalar. `TRUE` if SSE4.1 is detected.}
#'     \item{`cpu_avx2`}{Logical scalar. `TRUE` if AVX2 is detected.}
#'     \item{`cpu_avx512`}{Logical scalar. `TRUE` if AVX-512 (F/BW/VL) is detected.}
#'     \item{`cpu_neon`}{Logical scalar. `TRUE` if NEON/AdvSIMD is detected.}
#'     \item{`cpu_wasm_simd128`}{Logical scalar. `TRUE` if WASM SIMD128 is
#'       available.}
#'     \item{`target_arch`}{Character scalar. Compiler target CPU architecture
#'       (e.g. `"x86_64"`, `"aarch64"`).}
#'     \item{`target_os`}{Character scalar. Compiler target OS family
#'       (e.g. `"linux"`, `"macos"`, `"windows"`, `"emscripten"`).}
#'     \item{`simde_version`}{Character scalar. Vendored SIMDe version string
#'       (e.g. `"0.8.2"`).}
#'     \item{`simde_commit`}{Character scalar. Full 40-character SIMDe git
#'       commit SHA.}
#'   }
#' @examples
#' names(simd_info())
#' @export
simd_info <- function() {
  .Call(RC_simd_info)
}

