.nlmixr2Md5 <- function(obj) {
  if (!exists(".md5", obj$env)) {
    .cls <- class(obj)
    attr(.cls, ".foceiEnv") <- NULL
    .tmp <- list(
      .cls, obj$uif$ini, obj$fun.txt,
      obj$origControl
    )
    if (inherits(obj, "data.frame")) {
      .tmp <- c(.tmp, list(as.data.frame(obj)))
    }
    assign(".md5", digest::digest(.tmp), envir = obj$env)
  }
  return(obj$env$.md5)
}
