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
#' variants in one shared object and switches guarded operation tables. This
#' makes same-process benchmarking possible.
#'
#' @param backend Character scalar. Use `"auto"` to select the best available
#'   backend, or one of `simd_info()$available_backends` for an explicit choice.
#' @return The selected backend, invisibly. For `"auto"`, this is the backend
#'   chosen from the compiled and CPU-supported set.
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
#' CPU-supported backends, SIMDe-native backends, target information, and SIMDe
#' provenance compiled into the shared library. Calling this initializes the lazy auto-dispatch
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

