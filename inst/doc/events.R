## ----include = FALSE----------------------------------------------------------
knitr::opts_chunk$set(collapse = TRUE, comment = "#>")
has_bus <- exists("rxEventListen", asNamespace("rxode2"))
knitr::opts_chunk$set(eval = has_bus)

## ----include = FALSE----------------------------------------------------------
rxode2::rxSetProgressBar(0)

## ----echo = FALSE, eval = !has_bus, results = "asis"--------------------------
# cat("> **Note:** the installed `rxode2` has no event bus, so the code in",
#     "this vignette is shown but not run.  With such an `rxode2`, nlmixr2est",
#     "emits nothing and behaves exactly as before.\n")

## -----------------------------------------------------------------------------
library(nlmixr2est)

events <- list()
rxode2::rxEventListen("demo", function(event, ...) {
  p <- list(...)
  fields <- p[c("kind", "what", "objName", "name", "inPlace")]
  fields <- fields[!vapply(fields, is.null, logical(1))]
  events[[length(events) + 1L]] <<- paste0(
    event, "(",
    paste(names(fields), vapply(fields, deparse, character(1)),
          sep = " = ", collapse = ", "),
    ")"
  )
})
show <- function() {
  if (length(events) == 0L) cat("(no events)\n") else writeLines(unlist(events))
  events <<- list()
}

## ----results = "hide"---------------------------------------------------------
one.cmt <- function() {
  ini({
    tka <- log(1.57)
    tcl <- log(2.72)
    tv <- log(31.5)
    eta.ka ~ 0.6
    eta.cl ~ 0.3
    eta.v ~ 0.1
    add.sd <- 0.7
  })
  model({
    ka <- exp(tka + eta.ka)
    cl <- exp(tcl + eta.cl)
    v <- exp(tv + eta.v)
    linCmt() ~ add(add.sd)
  })
}

fit <- suppressMessages(
  nlmixr2(one.cmt, nlmixr2data::theo_sd, est = "posthoc")
)

## -----------------------------------------------------------------------------
show()

## -----------------------------------------------------------------------------
s <- suppressMessages(simulate(fit))
p <- suppressMessages(predict(fit, nlmixr2data::theo_sd))
v <- suppressMessages(vpcSim(fit, n = 3))
show()

## -----------------------------------------------------------------------------
f2 <- suppressMessages(addTable(fit))
fit <- suppressMessages(addTable(fit, updateObject = TRUE))
fit <- suppressMessages(addTable(fit, updateObject = TRUE,
                                 table = tableControl(npde = TRUE)))
show()

## -----------------------------------------------------------------------------
fit2 <- suppressMessages(nlmixr2(one.cmt, nlmixr2data::theo_sd, est = "saem",
                                 control = saemControl(print = 0)))
show()
o1 <- suppressMessages(fit2$objf)
o2 <- fit2$objf
show()

## -----------------------------------------------------------------------------
fit2 <- suppressMessages(addCwres(fit2))
show()
fit2 <- suppressMessages(addCwres(fit2))
show()

## -----------------------------------------------------------------------------
rxode2::rxEventUnlisten("demo")

