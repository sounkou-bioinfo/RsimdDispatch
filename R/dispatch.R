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
#' CPU-supported backends, and target information. Calling this initializes the
#' lazy auto-dispatch selection if it has not already been initialized.
#'
#' @return A named list of dispatch and CPU feature diagnostics. Backend-set
#'   entries are character vectors, not comma-separated strings.
#' @examples
#' names(simd_info())
#' @export
simd_info <- function() {
  .Call(RC_simd_info)
}

#' Locate or copy the C runtime dispatch template
#'
#' `simd_dispatch_template_path()` returns the installed template directory.
#' `use_simd_dispatch()` copies the template into a package directory. It is a
#' deliberately small helper; downstream packages should review and rename the
#' generated symbols for their own kernels.
#'
#' @param path Package root where the template should be copied.
#' @param overwrite Whether to overwrite existing files.
#' @return `use_simd_dispatch()` invisibly returns copied file paths.
#' @examples
#' simd_dispatch_template_path()
#' @export
simd_dispatch_template_path <- function() {
  system.file("templates", "dispatch-c", package = "RsimdDispatch", mustWork = TRUE)
}

#' @rdname simd_dispatch_template_path
#' @export
use_simd_dispatch <- function(path = ".", overwrite = FALSE) {
  template <- simd_dispatch_template_path()
  path <- normalizePath(path, mustWork = TRUE)
  files <- list.files(template, recursive = TRUE, all.files = TRUE, no.. = TRUE, full.names = FALSE)
  copied <- character()
  for (file in files) {
    from <- file.path(template, file)
    to <- file.path(path, file)
    if (dir.exists(from)) {
      dir.create(to, recursive = TRUE, showWarnings = FALSE)
      next
    }
    dir.create(dirname(to), recursive = TRUE, showWarnings = FALSE)
    if (file.exists(to) && !isTRUE(overwrite)) {
      stop("refusing to overwrite existing file: ", to, call. = FALSE)
    }
    file.copy(from, to, overwrite = overwrite)
    copied <- c(copied, to)
  }
  invisible(copied)
}
