#!/bin/sh
# tokenize-template.sh — apply @PKG_NAME@ tokenization to a source file.
# Usage: sh tools/tokenize-template.sh FILE
# Reads FILE, writes tokenized content to stdout.
#
# These substitutions mark the three places that differ between the
# RsimdDispatch development package and downstream consumer packages:
#
#   src/registration.c
#     R_init_RsimdDispatch  ->  R_init_@PKG_NAME@
#
#   tools/simdDispatch/simd_dispatch.c
#     "RsimdDispatch error:  ->  "@PKG_NAME@ error:
#
#   tools/configure-simd-dispatch.sh
#     RSD_LOG_PREFIX=${RSD_LOG_PREFIX:-RsimdDispatch}
#       ->  RSD_LOG_PREFIX=${RSD_LOG_PREFIX:-@PKG_NAME@}
#
# The @PKG_NAME@ token is expanded by rsd_template_substitute() in R/template.R
# before any C-prefix substitutions when use_simd_dispatch() deploys the
# scaffold into a downstream package.

sed \
  -e 's/R_init_RsimdDispatch/R_init_@PKG_NAME@/g' \
  -e 's/"RsimdDispatch error:/"@PKG_NAME@ error:/g' \
  -e 's/:-RsimdDispatch}/:-@PKG_NAME@}/g' \
  "$1"
