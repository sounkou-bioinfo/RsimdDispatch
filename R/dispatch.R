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
#' @return A numeric scalar count.
#' @examples
#' count_nonzero(as.raw(c(0, 1, 0, 2)))
#' @export
count_nonzero <- function(x) {
  if (!is.raw(x)) {
    stop("x must be a raw vector", call. = FALSE)
  }
  .Call(RC_count_nonzero, x)
}

#' Select the runtime SIMD backend
#'
#' Select the backend used by subsequent calls to `count_nonzero()`.
#' `RsimdDispatch` keeps all compiled variants in one shared object and switches
#' guarded function pointers. This makes same-process benchmarking possible.
#'
#' @param backend One of `"auto"`, `"scalar"`, `"sse2"`, `"sse41"`,
#'   `"avx2"`, `"avx512"`, or `"neon"`.
#' @return The selected backend, invisibly. For `"auto"`, this is the backend
#'   chosen from the compiled and CPU-supported set.
#' @examples
#' old <- simd_backend()
#' simd_set_backend("scalar")
#' simd_set_backend("auto")
#' @export
simd_set_backend <- function(backend = c("auto", "scalar", "sse2", "sse41", "avx2", "avx512", "neon")) {
  backend <- match.arg(backend)
  invisible(.Call(RC_simd_set_backend, backend))
}

#' Report the currently selected SIMD backend
#'
#' @return A character scalar naming the selected backend.
#' @examples
#' simd_backend()
#' @export
simd_backend <- function() {
  .Call(RC_simd_backend)
}

#' Report runtime SIMD dispatch diagnostics
#'
#' Returns the requested backend, selected backend, compiled backends,
#' CPU-supported backends, target information, and SIMDe provenance compiled
#' into the shared library. Calling this initializes the lazy auto-dispatch
#' selection if it has not already been initialized.
#'
#' @return A named list of dispatch and CPU feature diagnostics. Backend-set
#'   entries are character vectors, not comma-separated strings.
#' @examples
#' names(simd_info())
#' @export
simd_info <- function() {
  .Call(RC_simd_info)
}

