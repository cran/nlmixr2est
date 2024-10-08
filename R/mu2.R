mu2env <- new.env(parent=baseenv())
mu2env$pow <- function(x, y) {
  (x)^(y)
}
mu2env$erf <- rxode2::erf
#mu2env$erfinv <- rxode2::erfinv
mu2env$R_pow <- mu2env$pow
mu2env$R_pow_di <- mu2env$pow
mu2env$Rx_pow_di <- mu2env$pow
mu2env$Rx_pow <- mu2env$pow
mu2env$logit <- rxode2::logit
mu2env$expit <- rxode2::expit

#' UI modify covariates with reps
#'
#' @param expr expression to change
#' @param old old expression to change
#' @param new new expression to change
#' @return new expression with replacement
#' @author Matthew L. Fidler
#' @noRd
.uiModifyForCovsRep <- function(expr, old, new) {
  if (identical(expr, old)) return(new)
  if (is.call(expr)) {
    as.call(c(expr[[1]],lapply(expr[-1], .uiModifyForCovsRep, old=old, new=new)))
  } else {
    expr
  }
}
.uiGetMu3 <- function(data, .datEnv, .tmp) {
  .tmp <- eval(str2lang(paste0("rxode2::rxToSE(", .tmp, ", NULL)")))
  .tmp <- str2lang(paste0("with(.datEnv$symengine, ", .tmp, ")"))
  .tmp <- eval(.tmp)
  .tmp <- as.character(.tmp)
  .tmp <- str2lang(paste0("rxode2::rxFromSE(", .tmp, ")"))
  .tmp <- eval(.tmp)
  .tmp <- str2lang(paste0("with(.datEnv, with(data,",  .tmp, "))"))
  eval(.tmp)
}
#' This function handles mu2 covariates
#'
#' In general the dataset is modified with nlmixrMuDerCov# and the mu2
#' expressions are changed to traditional mu-expressions
#'
#' @param data input dataset
#' @param ui input ui
#' @return a list with list(ui=mu referenced ui, data=mu referenced dataset)
#' @author Matthew L. Fidler
#' @noRd
.uiModifyForCovs <- function(ui, data) {
  .datEnv <- new.env(parent=mu2env)
  .datEnv$data <- data
  .datEnv$model <- rxode2::as.model(ui)
  .datEnv$ui  <- ui
  .datEnv$symengine <- NULL
  if (use.utf()) {
    .mu2 <- "\u03BC\u2082"
    .mu3 <- "\u03BC\u2083"
  } else {
    .mu2 <- "mu2"
    .mu3 <- "mu3"
  }

  lapply(seq_along(ui$mu2RefCovariateReplaceDataFrame$covariate),
         function(i) {
           .datEnv$i <- i
           .tmp <- try(with(.datEnv,
                        with(data,
                             eval(str2lang(ui$mu2RefCovariateReplaceDataFrame$covariate[i])))),
                       silent=TRUE)
           if (inherits(.tmp, "try-error")) {
             if (is.null(.datEnv$symengine)) {
               .minfo(paste0("loading model to look for ", .mu3, "references"))
               .datEnv$symengine <- ui$loadPruneSaem
               .minfo("done")
             }
             .tmp <- try(.uiGetMu3(data, .datEnv,
                                   ui$mu2RefCovariateReplaceDataFrame$covariate[i]), silent=TRUE)
             if (!inherits(.tmp, "try-error")) {
               .txt <- paste0(.mu3, " item: ", ui$mu2RefCovariateReplaceDataFrame$covariate[i])
               .minfo(.txt)
               # Will put into the fit information
               warning(.txt, call.=FALSE)
             }
           } else {
             .txt <- paste0(.mu2, " item: ", ui$mu2RefCovariateReplaceDataFrame$covariate[i])
             .minfo(.txt)
             warning(.txt, call.=FALSE)
           }
           if (!inherits(.tmp, "try-error")) {
             .datEnv$data[[paste0("nlmixrMuDerCov", i)]] <- .tmp
             .new <- str2lang(paste0("nlmixrMuDerCov", i, "*",
                                     ui$mu2RefCovariateReplaceDataFrame$covariateParameter[i]))
             .old <- str2lang(ui$mu2RefCovariateReplaceDataFrame$modelExpression[i])
             .datEnv$model <- .uiModifyForCovsRep(.datEnv$model, .old, .new)
           } else {
             .txt <- paste0("not ",.mu2," or ", .mu3, " item: ", ui$mu2RefCovariateReplaceDataFrame$covariate[i])
             .minfo(.txt)
             warning(.txt, call.=FALSE)
           }
           invisible()
         })
  ui2 <- ui
  rxode2::model(ui2) <- .datEnv$model
  ui2 <- rxode2::rxUiDecompress(ui2)
  list(ui=ui2, data=.datEnv$data)
}
#' This is an internal function for modifying the UI to apply mu2 referencing
#'
#' mu2 referencing is algebraic mu-referencing by converting to the
#' transformation to a single value in the original dataset, and
#' moving that around
#'
#' @param env Environment needed for nlmixr2 fits
#' @return Either the original model({}) block (if changed) or NULL if
#'   not changed
#' @export
#' @author Matthew L. Fidler
#' @keywords internal
.uiApplyMu2 <- function(env) {
  if (isTRUE(env$control$muRefCovAlg) &&
        length(env$ui$mu2RefCovariateReplaceDataFrame$covariate) > 0L) {
    .lst     <- .uiModifyForCovs(env$ui, env$data)
    .model <- rxode2::as.model(env$ui)
    env$ui   <- .lst$ui
    env$data <- .lst$data
    return(.model)
  }
  NULL
}

#' This is an internal function for replacing the ui with original
#' model and dropping artificial data in output
#'
#' @param ret The object that would be returned, without modification
#' @param model The original model to apply
#' @return modified fit updated to show the original model and without
#'   the internal transformations
#' @export
#' @author Matthew L. Fidler
#' @keywords internal
.uiFinalizeMu2 <- function(ret, model) {
  if (!is.null(model)) {
    if (is.null(ret$ui)) return(ret)
    .ui2 <- rxode2::rxUiDecompress(ret$ui)
    if (is.null(.ui2)) return(ret)
    rm("control", envir=.ui2)
    rxode2::model(.ui2) <- model
    assign("ui", .ui2, envir=ret$env)
    if (inherits(ret, "data.frame")) {
      .w <- which(grepl("nlmixrMuDerCov[0-9]+", names(ret)))
      if (length(.w) > 0L) {
        .cls <- class(ret)
        class(ret) <- "data.frame"
        ret <- ret[,-.w]
        class(ret) <- .cls
      }
    }
  }
  # Reset symengine environments
  .saemModelEnv$symengine <- NULL
  .saemModelEnv$predSymengine <- NULL
  ret
}
