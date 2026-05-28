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

#' Three-tap numeric convolution with the selected SIMD backend
#'
#' Demonstration numeric kernel for the runtime dispatch template.
#' `convolve3()` computes a valid three-tap convolution/FIR pass over a numeric
#' vector using the currently selected backend:
#' `out[i] = kernel[1] * x[i] + kernel[2] * x[i + 1] + kernel[3] * x[i + 2]`.
#'
#' @param x A numeric vector.
#' @param kernel Numeric vector of length 3. The default is a symmetric
#'   smoothing kernel.
#' @return A numeric vector of length `max(length(x) - 2, 0)`.
#' @examples
#' convolve3(c(1, 2, 3, 4), c(1, 0, -1))
#' @export
convolve3 <- function(x, kernel = c(0.25, 0.5, 0.25)) {
  x <- as.double(x)
  kernel <- as.double(kernel)
  if (length(kernel) != 3L) {
    stop("kernel must be a numeric vector of length 3", call. = FALSE)
  }
  .Call(RC_convolve3, x, kernel)
}

#' Select the runtime SIMD backend
#'
#' Select the backend used by subsequent calls to dispatched demo kernels such
#' as `count_nonzero()` and `convolve3()`. `RsimdDispatch` keeps all compiled
#' variants in one shared object and switches guarded operation tables. This
#' makes same-process benchmarking possible.
#'
#' @param backend One of `"auto"`, `"scalar"`, `"sse2"`, `"sse41"`,
#'   `"avx2"`, `"avx512"`, `"neon"`, or `"wasm_simd128"`.
#' @return The selected backend, invisibly. For `"auto"`, this is the backend
#'   chosen from the compiled and CPU-supported set.
#' @examples
#' old <- simd_backend()
#' simd_set_backend("scalar")
#' simd_set_backend("auto")
#' @export
simd_set_backend <- function(backend = c("auto", "scalar", "sse2", "sse41", "avx2", "avx512", "neon", "wasm_simd128")) {
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

