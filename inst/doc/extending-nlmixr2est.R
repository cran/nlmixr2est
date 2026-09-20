## ----include = FALSE----------------------------------------------------------
knitr::opts_chunk$set(collapse = TRUE, comment = "#>")

## ----eval = FALSE-------------------------------------------------------------
# myMethodControl <- function(maxeval = 1000L, ...) {
#   .ctl <- list(maxeval = as.integer(maxeval), ...)
#   class(.ctl) <- "myMethodControl"
#   .ctl
# }
# 
# #' @exportS3Method nlmixr2est::getValidNlmixrCtl
# getValidNlmixrCtl.myMethod <- function(control) {
#   .ctl <- control[[1]]
#   if (is.null(.ctl)) .ctl <- myMethodControl()
#   if (!inherits(.ctl, "myMethodControl")) {
#     stop("est = 'myMethod' needs control = myMethodControl(...)", call. = FALSE)
#   }
#   .ctl
# }
# 
# #' @exportS3Method nlmixr2est::nlmixr2Est
# nlmixr2Est.myMethod <- function(env, ...) {
#   .ui   <- env$ui
#   .data <- env$data
#   .ctl  <- env$control
#   ## ... run the fit, build the result ...
#   ## return an nlmixr2FitData (see nlmixr2CreateOutputFromUi / the existing methods)
# }

## ----eval = FALSE-------------------------------------------------------------
# nlmixr2est::registerEstInterceptor("myPkg", function(env) {
#   if (!shouldClaim(env$ui)) return(NULL)         # decline -> normal fit proceeds
#   ## claim it: do the work and return a finalized nlmixr2FitData
#   runMyThing(env)
# })

## ----eval = FALSE-------------------------------------------------------------
# nlmixr2(model, data, "focei", foceiControl(), nn = nnControl(...))
# #                                             ^ arrives as env$.nlmixr2Dots$nn

