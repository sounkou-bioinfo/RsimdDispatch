#' Configure an R package for C runtime SIMD dispatch
#'
#' `use_simd_dispatch()` copies the dispatch scaffold into an R package and
#' performs the package-name and C-prefix substitutions needed for a working
#' package. It writes package files, updates `DESCRIPTION`, `.Rbuildignore`,
#' and `.gitignore`, and returns the copied paths invisibly.
#'
#' @section Developer utility:
#' This function is intended for package authors. It is not needed at runtime by
#' users of packages that already include generated dispatch code.
#'
#' @param path Package root where the template should be copied.
#' @param pkg R package name. If `NULL`, the name is read from `DESCRIPTION`.
#' @param prefix C symbol prefix used to replace `rsd_` in the copied sources.
#'   The default is a sanitized lowercase package name.
#' @param overwrite Whether to overwrite existing files.
#' @param quiet Whether to suppress progress messages.
#' @return Invisibly returns copied file paths.
#' @examples
#' simd_dispatch_template_path()
#' @export
simd_dispatch_template_path <- function() {
  system.file("templates", "dispatch-c", package = "RsimdDispatch", mustWork = TRUE)
}

#' @rdname simd_dispatch_template_path
#' @export
use_simd_dispatch <- function(path = ".",
                              pkg = NULL,
                              prefix = NULL,
                              overwrite = FALSE,
                              quiet = FALSE) {
  path <- normalizePath(path, mustWork = TRUE)
  desc_path <- file.path(path, "DESCRIPTION")
  if (is.null(pkg)) {
    if (!file.exists(desc_path)) {
      stop("pkg must be supplied when DESCRIPTION is not present", call. = FALSE)
    }
    pkg <- read.dcf(desc_path)[1L, "Package"]
  }
  if (!is.character(pkg) || length(pkg) != 1L || is.na(pkg) || !nzchar(pkg)) {
    stop("pkg must be a non-empty character scalar", call. = FALSE)
  }
  if (is.null(prefix)) {
    prefix <- rsd_sanitize_c_prefix(pkg)
  }
  if (!is.character(prefix) || length(prefix) != 1L || is.na(prefix) || !nzchar(prefix)) {
    stop("prefix must be a non-empty character scalar", call. = FALSE)
  }
  if (!grepl("^[A-Za-z_][A-Za-z0-9_]*$", prefix)) {
    stop("prefix must be a valid C identifier prefix", call. = FALSE)
  }

  template <- simd_dispatch_template_path()
  files <- list.files(template, recursive = TRUE, all.files = TRUE, no.. = TRUE, full.names = FALSE)
  files <- files[!files %in% c("README.md", "CHECKLIST.md")]
  files <- files[!grepl("^\\.github/", files)]
  copied <- character()
  for (file in files) {
    from <- file.path(template, file)
    to <- file.path(path, file)
    dir.create(dirname(to), recursive = TRUE, showWarnings = FALSE)
    if (file.exists(to) && !isTRUE(overwrite)) {
      stop("refusing to overwrite existing file: ", to, call. = FALSE)
    }
    if (rsd_is_text_file(from)) {
      txt <- readLines(from, warn = FALSE)
      txt <- rsd_template_substitute(txt, pkg = pkg, prefix = prefix, file = file)
      writeLines(txt, to, useBytes = TRUE)
    } else {
      file.copy(from, to, overwrite = TRUE)
    }
    rsd_make_executable_if_script(to)
    copied <- c(copied, to)
    if (!isTRUE(quiet)) {
      message("Wrote ", rsd_rel_path(to, path))
    }
  }

  rsd_update_description(path)
  rsd_update_ignores(path)

  if (!isTRUE(quiet)) {
    message("Configured C SIMD dispatch scaffold for package ", pkg, ".")
    message("Next: replace the demo count_nonzero() and convolve3() kernels with your package kernels and run roxygen2.")
  }
  invisible(copied)
}

rsd_sanitize_c_prefix <- function(pkg) {
  out <- gsub("([a-z])([A-Z])", "\\1_\\2", pkg, perl = TRUE)
  out <- gsub("[^A-Za-z0-9_]", "_", out)
  out <- tolower(out)
  if (grepl("^[0-9]", out)) {
    out <- paste0("_", out)
  }
  out
}

rsd_template_substitute <- function(x, pkg, prefix, file = "") {
  upper_prefix <- toupper(prefix)
  x <- gsub("RsimdDispatch", pkg, x, fixed = TRUE)
  x <- gsub("rsd_", paste0(prefix, "_"), x, fixed = TRUE)
  x <- gsub("Rsd", rsd_camel_prefix(prefix), x, fixed = TRUE)
  x <- gsub("RSD_", paste0(upper_prefix, "_"), x, fixed = TRUE)
  x <- gsub("RC_", paste0(upper_prefix, "_C_"), x, fixed = TRUE)
  if (identical(file, file.path("tools", "configure-simd-dispatch.sh"))) {
    x <- gsub(paste0('package = "', pkg, '"'), 'package = "RsimdDispatch"', x, fixed = TRUE)
    x <- gsub(paste0('package="', pkg, '"'), 'package="RsimdDispatch"', x, fixed = TRUE)
  }
  x
}

rsd_camel_prefix <- function(prefix) {
  parts <- strsplit(prefix, "_+", perl = TRUE)[[1L]]
  parts <- parts[nzchar(parts)]
  paste0(toupper(substring(parts, 1L, 1L)), substring(parts, 2L), collapse = "")
}

rsd_make_executable_if_script <- function(path) {
  if (.Platform$OS.type == "unix" && basename(path) %in% c("configure", "configure.win", "cleanup", "cleanup.win", "configure-simd-dispatch.sh")) {
    Sys.chmod(path, mode = "0755")
  }
  invisible(path)
}

rsd_is_text_file <- function(path) {
  base <- basename(path)
  ext <- tolower(sub("^.*\\.", "", base))
  if (identical(ext, base)) {
    ext <- ""
  }
  base %in% c("configure", "configure.win", "cleanup", "README", "CHECKLIST") ||
    ext %in% c("c", "h", "in", "r", "md", "sh", "yml", "yaml", "txt")
}

rsd_update_description <- function(path) {
  desc_path <- file.path(path, "DESCRIPTION")
  if (!file.exists(desc_path)) {
    return(invisible(FALSE))
  }
  d <- read.dcf(desc_path, all = TRUE)
  d <- rsd_dcf_set(d, "LinkingTo", rsd_append_dep(rsd_dcf_get(d, "LinkingTo"), "RsimdDispatch"))
  d <- rsd_dcf_set(d, "SystemRequirements", rsd_append_csv(rsd_dcf_get(d, "SystemRequirements"), "GNU make"))
  write.dcf(d, file = desc_path, keep.white = colnames(d))
  invisible(TRUE)
}

rsd_update_ignores <- function(path) {
  rsd_append_unique_lines(
    file.path(path, ".Rbuildignore"),
    c("^src/Makevars$", "^src/Makevars\\.win$", "^src/config\\.h$", "^src/.*\\.o$", "^src/rsd-kernels$", "^src/rsd-kernels/.*$")
  )
  rsd_append_unique_lines(
    file.path(path, ".gitignore"),
    c("src/Makevars", "src/Makevars.win", "src/config.h", "src/*.o", "src/rsd-kernels/")
  )
  invisible(TRUE)
}

rsd_dcf_get <- function(d, field) {
  if (field %in% colnames(d)) {
    d[1L, field]
  } else {
    NA_character_
  }
}

rsd_dcf_set <- function(d, field, value) {
  if (!(field %in% colnames(d))) {
    extra <- matrix(NA_character_, nrow = nrow(d), ncol = 1L)
    colnames(extra) <- field
    d <- cbind(d, extra)
  }
  d[1L, field] <- value
  d
}

rsd_append_dep <- function(x, package) {
  if (length(x) == 0L || is.na(x) || !nzchar(x)) {
    return(package)
  }
  parts <- trimws(strsplit(x, ",", fixed = TRUE)[[1L]])
  names <- sub("\\s*\\(.*$", "", parts)
  if (!(package %in% names)) {
    parts <- c(parts, package)
  }
  paste(parts[nzchar(parts)], collapse = ",\n    ")
}

rsd_append_csv <- function(x, value) {
  if (length(x) == 0L || is.na(x) || !nzchar(x)) {
    return(value)
  }
  parts <- trimws(strsplit(x, ",", fixed = TRUE)[[1L]])
  parts <- unique(c(parts[nzchar(parts)], value))
  paste(parts, collapse = ", ")
}

rsd_append_unique_lines <- function(path, lines) {
  old <- if (file.exists(path)) readLines(path, warn = FALSE) else character()
  new <- c(old, setdiff(lines, old))
  writeLines(new, path, useBytes = TRUE)
}

rsd_rel_path <- function(path, root) {
  sub(paste0("^", normalizePath(root, winslash = "/", mustWork = FALSE), "/?"), "", normalizePath(path, winslash = "/", mustWork = FALSE))
}
