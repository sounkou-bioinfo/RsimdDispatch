#' Report vendored SIMDe provenance
#'
#' `simde_info()` reports the version, upstream repository, pinned commit, and
#' commit date for the bundled header-only SIMDe library.
#'
#' @return A named list of character scalars describing the vendored SIMDe copy.
#' @examples
#' simde_info()[c("version", "commit")]
#' @export
simde_info <- function() {
  version_path <- system.file("vendor", "simde", "VERSION", package = "RsimdDispatch", mustWork = TRUE)
  fields <- read.dcf(version_path, all = TRUE)

  get_field <- function(name) {
    if (name %in% colnames(fields)) {
      value <- fields[1L, name]
      if (!is.na(value) && nzchar(value)) {
        return(unname(value))
      }
    }
    NA_character_
  }

  version <- get_field("Version")
  if (is.na(version) || !nzchar(version)) {
    version <- rsd_simde_header_version()
  }

  list(
    component = get_field("Component"),
    version = version,
    repository = get_field("Repository"),
    commit = get_field("Commit"),
    date = get_field("Date"),
    include_tree = get_field("Vendored-include-tree"),
    license_file = get_field("License-file")
  )
}

rsd_simde_header_version <- function() {
  header <- system.file("include", "simde", "simde-common.h", package = "RsimdDispatch", mustWork = TRUE)
  lines <- readLines(header, warn = FALSE)

  read_macro <- function(name) {
    pattern <- paste0("^#define[[:space:]]+", name, "[[:space:]]+([0-9]+)\\b")
    hit <- grep(pattern, lines, value = TRUE)
    if (!length(hit)) {
      return(NA_character_)
    }
    sub(pattern, "\\1", hit[[1L]])
  }

  parts <- vapply(
    c("SIMDE_VERSION_MAJOR", "SIMDE_VERSION_MINOR", "SIMDE_VERSION_MICRO"),
    read_macro,
    character(1L)
  )
  if (any(is.na(parts)) || any(!nzchar(parts))) {
    return(NA_character_)
  }
  paste(parts, collapse = ".")
}
