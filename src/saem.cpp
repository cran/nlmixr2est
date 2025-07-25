#define STRICT_R_HEADER
#include <stdio.h>
#include <stdarg.h>
#include <thread>
#include <chrono>
#include <R_ext/Rdynload.h>
#include <RcppArmadillo.h>
#include <rxode2ptr.h>
#include "utilc.h"
#include "censEst.h"
#include "nearPD.h"
#include "inner.h"

#define _(String) (String)

#define PHI(x) 0.5*(1.0+erf((x)/M_SQRT2))


#ifndef __SAEM_CLASS_RCPP_HPP__
#define __SAEM_CLASS_RCPP_HPP__
#define MAXENDPNT 40
#define max2( a , b )  ( (a) > (b) ? (a) : (b) )
using namespace std;
using namespace arma;
using namespace Rcpp;

typedef void (*fn_ptr) (double *, double *);

extern "C" void nelder_fn(fn_ptr func, int n, double *start, double *step,
			  int itmax, double ftol_rel, double rcoef, double ecoef, double ccoef,
			  int *iconv, int *it, int *nfcall, double *ynewlo, double *xmin,
			  int *iprint);

double *_saemYptr;
double *_saemFptr;
int _saemLen;
int _saemYj;
int _saemAddProp;
double _saemLambda;
double _saemLow;
double _saemHi;
fn_ptr _saemFn;
double *_saemStart;
double *_saemStep;
double _saemLambdaR;
double _saemPowR;
int _saemPropT=0;
bool _warnAtolRtol=false;
int _saemIncreaseTol=0;
int _saemIncreasedTol2=0;
double _saemOdeRecalcFactor = 1.0;
int _saemMaxOdeRecalc = 0;
mat _saemUE;

int _saemFixedIdx[4] = {0, 0, 0, 0};
double _saemFixedValue[4] = {0.0, 0.0, 0.0, 0.0};

// res_mod defines
#define rmAdd 1
#define rmProp 2
#define rmPow 3
#define rmAddProp 4
#define rmAddPow 5
#define rmAddLam 6
#define rmPropLam 7
#define rmPowLam 8
#define rmAddPropLam 9
#define rmAddPowLam 10

static inline double handleF(int powt, double &ft, double &f, bool trunc, bool adjustF) {
  double xmin = 1.0e-200, xmax=1e300;
  double fa = powt ? ft : f;
  if (adjustF && fa == 0.0) {
    fa = 1.0;
  }
  if (trunc){
    if (fa < xmin) fa = xmin;
    else if (fa > xmax) fa = xmax;
  }
  return fa;
}

#define toLambda(x) _powerDi(x, 1.0, 4, -_saemLambdaR, _saemLambdaR)
#define toLambdaEst(x) _powerD((x < -0.99*_saemLambdaR ? -0.99*_saemLambdaR : (x > 0.99*_saemLambdaR ? 0.99*_saemLambdaR : x)), 1.0, 4, -_saemLambdaR, _saemLambdaR)

#define toPow(x) _powerDi(x, 1.0, 4, -_saemPowR, _saemPowR)
#define toPowEst(x) _powerD((x < -0.99*_saemPowR ? -0.99*_saemPowR : (x > 0.99*_saemPowR ? 0.99*_saemPowR : x)), 1.0, 4, -_saemPowR, _saemPowR)

// add+prop
void obj(double *ab, double *fx) {
  int i;
  double g, sum, cur, fa;
  double xmin = 1.0e-200, xmax=1e300, ft, ytr;
  double ab02;
  double ab12;
  int curi = 0;
  if (_saemFixedIdx[0] == 1) {
    ab02 = _saemFixedValue[0];
  } else {
    ab02 = ab[curi++];
  }
  if (_saemFixedIdx[1] == 1) {
    ab12 = _saemFixedValue[1];
  } else {
    ab12 = ab[curi++];
  }
  ab02 = ab02*ab02;
  ab12 = ab12*ab12;
  for (i=0, sum=0; i<_saemLen; ++i) {
    // nelder_() does not al_saemLow _saemLower bounds; we force ab[] be positive here
    ft  = _powerD(_saemFptr[i], _saemLambda, _saemYj, _saemLow, _saemHi);
    ytr = _powerD(_saemYptr[i], _saemLambda, _saemYj, _saemLow, _saemHi);
    // focei: rx_r_ = eff^2 * prop.sd^2 + add_sd^2
    // focei g = sqrt(eff^2*prop.sd^2 + add.sd^2)
    fa = handleF(_saemPropT, ft, _saemFptr[i], false, false);
    if (_saemAddProp == 1) {
      g = ab02 + ab12*fa;
    } else {
      g = sqrt(ab02*ab02 + ab12*ab12*fa*fa);
    }
    if (g < xmin) g = xmin;
    if (g > xmax) g = xmax;
    cur = (ytr-ft)/g;
    sum += cur*cur + 2*log(g);
  }
  *fx = sum;
}

// add + pow
void objC(double *ab, double *fx) {
  int i;
  double g, sum, cur, ft, ytr, fa=1.0;
  double xmin = 1.0e-200, xmax = 1e300;
  double ab02, ab12, ab22;
  int curi = 0;
  if (_saemFixedIdx[0] == 1) {
    ab02 = _saemFixedValue[0];
  } else {
    ab02 = ab[curi++];
  }
  if (_saemFixedIdx[1] == 1) {
    ab12 = _saemFixedValue[1];
  } else {
    ab12 = ab[curi++];
  }
  if (_saemFixedIdx[2] == 1) {
    ab22 = _saemFixedValue[2];
  } else {
    ab22 = ab[curi++];
  }
  double pw = toPow(ab22);
  for (i=0, sum=0; i<_saemLen; ++i) {
    // nelder_() does not al_saemLow _saemLower bounds; we force ab[] be positive here
    ft  = _powerD(_saemFptr[i], _saemLambda, _saemYj, _saemLow, _saemHi);
    ytr = _powerD(_saemYptr[i], _saemLambda, _saemYj, _saemLow, _saemHi);
    // focei: rx_r_ = eff^2 * prop.sd^2 + add_sd^2
    // focei g = sqrt(eff^2*prop.sd^2 + add.sd^2)
    fa = handleF(_saemPropT, ft, _saemFptr[i], false, false);
    if (_saemAddProp == 1){
      g = ab02*ab02 + ab12*ab12*pow(fa, pw);
    } else {
      double ab0 = ab02*ab02;
      double ab1 = ab12*ab12;
      g = ab0*ab0 + ab1*ab1*pow(fa, 2*pw);
    }
    if (g < xmin) g = xmin;
    if (g > xmax) g = xmax;
    cur = (ytr-ft)/g;
    sum += cur * cur + 2*log(g);
  }
  *fx = sum;
}

// Power only
void objD(double *ab, double *fx) {
  int i;
  double g, sum, cur, ft, ytr;
  double xmin = 1.0e-200, xmax = 1e300;
  double ab02, ab12;
  int curi = 0;
  if (_saemFixedIdx[0] == 1) {
    ab02 = _saemFixedValue[0];
  } else {
    ab02 = ab[curi++];
  }
  if (_saemFixedIdx[1] == 1) {
    ab12 = _saemFixedValue[1];
  } else {
    ab12 = ab[curi++];
  }
  double pw = toPow(ab12);
  double fa;
  for (i=0, sum=0; i<_saemLen; ++i) {
    // nelder_() does not al_saemLow _saemLower bounds; we force ab[] be positive here
    ft = _powerD(_saemFptr[i],  _saemLambda, _saemYj, _saemLow, _saemHi);
    ytr = _powerD(_saemYptr[i], _saemLambda, _saemYj, _saemLow, _saemHi);
    fa = handleF(_saemPropT, ft, _saemFptr[i], false, true);
    g = ab02*ab02*pow(fa, pw);
    if (g < xmin) g = xmin;
    if (g > xmax) g = xmax;
    cur = (ytr-ft)/g;
    sum += cur * cur + 2*log(g);
  }
  *fx = sum;
}

// add+_saemLambda only
void objE(double *ab, double *fx) {
  int i;
  double g, sum, cur, ft, ytr;
  double xmin = 1.0e-200, xmax = 1e300;
  double ab02, ab12;
  int curi = 0;
  if (_saemFixedIdx[0] == 1) {
    ab02 = _saemFixedValue[0];
  } else {
    ab02 = ab[curi++];
  }
  if (_saemFixedIdx[1] == 1) {
    ab12 = _saemFixedValue[1];
  } else {
    ab12 = ab[curi++];
  }
  double lambda = toLambda(ab12);
  for (i=0, sum=0; i<_saemLen; ++i) {
    // nelder_() does not al_saemLow _saemLower bounds; we force ab[] be positive here
    ft = _powerD(_saemFptr[i],  lambda, _saemYj, _saemLow, _saemHi);
    ytr = _powerD(_saemYptr[i], lambda, _saemYj, _saemLow, _saemHi);
    g = ab02*ab02;
    if (g < xmin) g = xmin;
    if (g > xmax) g = xmax;
    cur = (ytr-ft)/g;
    sum += cur * cur + 2*log(g);
  }
  *fx = sum;
}

// prop+_saemLambda only
void objF(double *ab, double *fx) {
  int i;
  double g, sum, cur, ft, ytr, fa;
  double xmin = 1.0e-200, xmax = 1e300;
  double ab02, ab12;
  int curi = 0;
  if (_saemFixedIdx[0] == 1) {
    ab02 = _saemFixedValue[0];
  } else {
    ab02 = ab[curi++];
  }
  if (_saemFixedIdx[1] == 1) {
    ab12 = _saemFixedValue[1];
  } else {
    ab12 = ab[curi++];
  }
  double lambda = toLambda(ab12);
  for (i=0, sum=0; i<_saemLen; ++i) {
    // nelder_() does not al_saemLow _saemLower bounds; we force ab[] be positive here
    ft = _powerD(_saemFptr[i],  lambda, _saemYj, _saemLow, _saemHi);
    ytr = _powerD(_saemYptr[i], lambda, _saemYj, _saemLow, _saemHi);
    fa = handleF(_saemPropT, ft, _saemFptr[i], false, true);
    g = ab02*ab02*fa;
    if (g == 0) g = 1;
    if (g < xmin) g = xmin;
    if (g > xmax) g = xmax;
    cur = (ytr-ft)/g;
    sum += cur * cur + 2*log(g);
  }
  *fx = sum;
}

// pow+_saemLambda only
void objG(double *ab, double *fx) {
  int i;
  double g, sum, cur, ft, ytr, fa;
  double xmin = 1.0e-200, xmax = 1e300;
  double ab02, ab12, ab22;
  int curi = 0;
  if (_saemFixedIdx[0] == 1) {
    ab02 = _saemFixedValue[0];
  } else {
    ab02 = ab[curi++];
  }
  if (_saemFixedIdx[1] == 1) {
    ab12 = _saemFixedValue[1];
  } else {
    ab12 = ab[curi++];
  }
  if (_saemFixedIdx[2] == 1) {
    ab22 = _saemFixedValue[2];
  } else {
    ab22 = ab[curi++];
  }
  double lambda = toLambda(ab22);
  double pw = toPow(ab12);
  for (i=0, sum=0; i<_saemLen; ++i) {
    // nelder_() does not al_saemLow _saemLower bounds; we force ab[] be positive here
    ft = _powerD(_saemFptr[i],  lambda, _saemYj, _saemLow, _saemHi);
    ytr = _powerD(_saemYptr[i], lambda, _saemYj, _saemLow, _saemHi);
    fa = handleF(_saemPropT, ft, _saemFptr[i], false, true);
    g = ab02*ab02*pow(fa, pw);
    if (g == 0) g = 1.0;
    if (g < xmin) g = xmin;
    if (g > xmax) g = xmax;
    cur = (ytr-ft)/g;
    sum += cur * cur + 2*log(g);
  }
  *fx = sum;
}

// add + prop + _saemLambda
void objH(double *ab, double *fx) {
  int i;
  double g, sum, cur, fa;
  double xmin = 1.0e-200, xmax = 1e300, ft, ytr;
  double ab02, ab12, ab22;
  int curi = 0;
  if (_saemFixedIdx[0] == 1) {
    ab02 = _saemFixedValue[0];
  } else {
    ab02 = ab[curi++];
  }
  if (_saemFixedIdx[1] == 1) {
    ab12 = _saemFixedValue[1];
  } else {
    ab12 = ab[curi++];
  }
  if (_saemFixedIdx[2] == 1) {
    ab22 = _saemFixedValue[2];
  } else {
    ab22 = ab[curi++];
  }
  double lambda = toLambda(ab22);
  for (i=0, sum=0; i<_saemLen; ++i) {
    // nelder_() does not al_saemLow _saemLower bounds; we force ab[] be positive here
    ft = _powerD(_saemFptr[i],  lambda, _saemYj, _saemLow, _saemHi);
    ytr = _powerD(_saemYptr[i], lambda, _saemYj, _saemLow, _saemHi);
    // focei: rx_r_ = eff^2 * prop.sd^2 + add_sd^2
    // focei g = sqrt(eff^2*prop.sd^2 + add.sd^2)
    fa = handleF(_saemPropT, ft, _saemFptr[i], false, false);
    if (_saemAddProp == 1) {
      g = ab02*ab02 + ab12*ab12*fa;
    } else {
      double ab0 = ab02*ab02;
      double ab1 = ab12*ab12;
      g = sqrt(ab0*ab0 + ab1*ab1*fa*fa);
    }
    if (g < xmin) g = xmin;
    if (g > xmax) g = xmax;
    cur = (ytr-ft)/g;
    sum += cur*cur + 2*log(g);
  }
  *fx = sum;
}

// add + pow + _saemLambda
void objI(double *ab, double *fx) {
  int i;
  double g, sum, cur, fa=1.0;
  double xmin = 1.0e-200, xmax = 1e300, ft, ytr;
  double ab02, ab12, ab22, ab32;
  int curi = 0;
  if (_saemFixedIdx[0] == 1) {
    ab02 = _saemFixedValue[0];
  } else {
    ab02 = ab[curi++];
  }
  if (_saemFixedIdx[1] == 1) {
    ab12 = _saemFixedValue[1];
  } else {
    ab12 = ab[curi++];
  }
  if (_saemFixedIdx[2] == 1) {
    ab22 = _saemFixedValue[2];
  } else {
    ab22 = ab[curi++];
  }
  if (_saemFixedIdx[3] == 1) {
    ab32 = _saemFixedValue[3];
  } else {
    ab32 = ab[curi++];
  }

  double lambda = toLambda(ab32);
  double pw = toPow(ab22);
  for (i=0, sum=0; i<_saemLen; ++i) {
    // nelder_() does not al_saemLow _saemLower bounds; we force ab[] be positive here
    ft = _powerD(_saemFptr[i],  lambda, _saemYj, _saemLow, _saemHi);
    ytr = _powerD(_saemYptr[i], lambda, _saemYj, _saemLow, _saemHi);
    fa = handleF(_saemPropT, ft, _saemFptr[i], false, false);
    if (_saemAddProp == 1) {
      g = ab02*ab02 + ab12*ab12*pow(fa, pw);
    } else {
      double ab0 = ab02*ab02;
      double ab1 = ab12*ab12;
      fa = pow(fa, pw);
      g = sqrt(ab0*ab0 + ab1*ab1*fa*fa);
    }
    if (g < xmin) g = xmin;
    if (g > xmax) g = xmax;
    cur = (ytr-ft)/g;
    sum += cur*cur + 2*log(g);
  }
  *fx = sum;
}

int _saemItmax = 100;
double _saemTol = 1e-4;
int _saemType = 1;

static inline void _saemOpt(int n, double *pxmin) {
  if (n == 0) return;
  if (n == 1) {
    // Use R's optimize for unidimensional optimization
    Function loadNamespace("loadNamespace", R_BaseNamespace);
    Environment nlmixr2 = loadNamespace("nlmixr2est");
    Function optimize1 = nlmixr2[".saemOpt1"];
    NumericVector par0(1);
    par0[0] = _saemStart[0];
    double x0 = as<double>(optimize1(par0));
    pxmin[0] = x0;
  } else {
    if (_saemType == 1) {
      int iconv, it, nfcall, iprint=0, itmax=_saemItmax*n;
      double ynewlo;
      nelder_fn(_saemFn, n, _saemStart, _saemStep, itmax, _saemTol, 1.0, 2.0, .5,
                &iconv, &it, &nfcall, &ynewlo, pxmin, &iprint);
    } else if (_saemType == 2) {
      // Try Newoua
      Function loadNamespace("loadNamespace", R_BaseNamespace);
      Environment nlmixr2 = loadNamespace("nlmixr2est");
      Function newuoa = nlmixr2[".newuoa"];
      NumericVector par0(n);
      for (int i = n; i--;) {
        par0[i] = _saemStart[i];
      }
      List ret = newuoa(_["par"] = par0, _["fn"] = nlmixr2[".saemResidF"],
                        _["control"]=List::create(_["rhoend"]=_saemTol,
                                                  _["maxfun"]=_saemItmax*n*n));
      double f = as<double>(ret["value"]);
      if (ISNA(f)) {
        RSprintf("newoua failed, switch to nelder-mead\n");
        int iconv, it, nfcall, iprint=0, itmax=_saemItmax*n;
        double ynewlo;
        nelder_fn(_saemFn, n, _saemStart, _saemStep, itmax, _saemTol, 1.0, 2.0, .5,
                  &iconv, &it, &nfcall, &ynewlo, pxmin, &iprint);
      } else {
        NumericVector x = ret["x"];
        for (int i = n; i--;) {
          pxmin[i] = x[i];
        }
      }
    }
  }
}

extern "C" SEXP _saemResidF(SEXP v) {
  SEXP ret = PROTECT(Rf_allocVector(REALSXP, 1));
  _saemFn(REAL(v),REAL(ret));
  UNPROTECT(1);
  return ret;
}


struct mcmcphi {
  int nphi;
  uvec i;
  mat Gamma_phi;
  mat Gdiag_phi;
  mat IGamma2_phi;
  mat mprior_phiM;
};

struct mcmcaux {
  int nM;
  uvec indioM;
  //double sigma2;  //not needed?
  vec yM;
  mat evtM;
  List optM;
};


uvec getObsIdx(umat m) {
  uvec x;
  x.set_size(0);

  for (unsigned int b=0; b<m.n_rows; ++b) {
    uvec i=linspace<uvec>(m(b,0), m(b,1), m(b,1) - m(b,0) + 1);
    x = join_cols(x, i);
  }
  return x;
}


// class def starts
class SAEM {
  typedef mat (*user_funct) (const mat&, const mat&, const List&);

public:

  SAEM() {
    user_fn = NULL;
  }

  ~SAEM() {}

  void set_fn(user_funct f) {
    user_fn = f;
  }

  mat get_resMat() {
    mat m(nendpnt,4);
    m.col(0) = ares;
    m.col(1) = bres;
    m.col(2) = cres;
    m.col(3) = lres;
    return m;
  }

  mat get_trans() {
    mat m(nendpnt, 4);
    m.col(0) = lambda;
    m.col(1) = conv_to<vec>::from(yj); // convert uvec to double mat
    m.col(2) = low;
    m.col(3) = hi;
    return m;
  }

  mat get_mprior_phi() {
    mat m = mpost_phi;
    m.cols(i1) = mprior_phi1;
    return m;
  }

  mat get_mpost_phi() {
    return mpost_phi;
  }

  mat get_Plambda() {
    return Plambda;
  }

  mat get_Gamma2_phi1() {
    return Gamma2_phi1;
  }

  mat get_Ha() {
    return Ha;
  }

  vec get_sig2() {
    return vcsig2;                                       //FIXME: regression due to multiple endpnts?
  }

  List get_resInfo() {
    vec sig2(bres.size());
    std::copy(sigma2, sigma2+bres.size(), &sig2[0]);
    return List::create(_["sigma2"]  = wrap(sig2),
			_["ares"]    = wrap(ares),
			_["bres"]    = wrap(bres),
			_["cres"]    = wrap(cres),
			_["lres"]    = wrap(lres),
			_["res_mod"] = wrap(res_mod));
  }

  mat get_par_hist() {
    return par_hist;
  }

  mat get_eta() {
    mat eta = mpost_phi.cols(i1);
    eta -= mprior_phi1;
    mat ue = _saemUE.rows(0, eta.n_rows - 1);
    ue = ue.cols(i1);
    eta = eta % ue;
    return eta;
  }

  void inits(List x) {
    _saemItmax = as<int>(x["itmax"]);
    _saemTol = as<double>(x["tol"]);
    _saemType = as<int>(x["type"]);
    _saemLambdaR = fabs(as<double>(x["lambdaRange"]));
    _saemPowR = fabs(as<double>(x["powRange"]));
    _saemIncreaseTol=0;
    _saemIncreasedTol2=0;
    _saemMaxOdeRecalc = abs(as<int>(x["maxOdeRecalc"]));
    _saemOdeRecalcFactor = fabs(as<double>(x["odeRecalcFactor"]));
    _saemUE = as<mat>(x["ue"]);

    nmc = as<int>(x["nmc"]);
    nu = as<uvec>(x["nu"]);
    niter = as<int>(x["niter"]);
    nb_correl = as<int>(x["nb_correl"]);
    nb_fixOmega = as<int>(x["nb_fixOmega"]);
    nb_fixResid = as<int>(x["nb_fixResid"]);
    resValue = as<vec>(x["resValue"]);
    resFixed = as<uvec>(x["resFixed"]);
    resKeep = find(resFixed==0);
    niter_phi0 = as<int>(x["niter_phi0"]);
    coef_phi0 = as<double>(x["coef_phi0"]);
    nb_sa = as<int>(x["nb_sa"]);
    coef_sa = as<double>(x["coef_sa"]);
    rmcmc = as<double>(x["rmcmc"]);
    pas = as<vec>(x["pas"]);
    pash = as<vec>(x["pash"]);
    minv = as<vec>(x["minv"]);

    N = as<int>(x["N"]);
    ntotal = as<int>(x["ntotal"]);
    y  = as<vec>(x["y"]);
    yM = as<vec>(x["yM"]);
    evt  = as<mat>(x["evt"]);
    evtM = as<mat>(x["evtM"]);
    phiM = as<mat>(x["phiM"]);
    indioM = as<uvec>(x["indioM"]);
    int mlen = as<int>(x["mlen"]);
    nM = N*nmc;

    opt = as<List>(x["opt"]);                                      //CHECKME
    optM = as<List>(x["optM"]);                                    //CHECKME

    pc1 = as<uvec>(x["pc1"]);
    covstruct1 = as<mat>(x["covstruct1"]);
    Mcovariables = as<mat>(x["Mcovariables"]);

    nphi1 = as<int>(x["nphi1"]);
    i1 = as<uvec>(x["i1"]);
    Gamma2_phi1 = as<mat>(x["Gamma2_phi1"]);
    Gamma2_phi1fixedIxIn = as<umat>(x["Gamma2_phi1fixedIx"]);
    Gamma2_phi1fixedIx = find(Gamma2_phi1fixedIxIn);
    Gamma2_phi1fixed = as<int>(x["Gamma2_phi1fixed"]);
    if (Gamma2_phi1fixed==1) {
      Gamma2_phi1fixedValues = as<mat>(x["Gamma2_phi1fixedValues"]);
    }
    mprior_phi1 = as<mat>(x["mprior_phi1"]);
    COV1 = as<mat>(x["COV1"]);
    LCOV1 = as<mat>(x["LCOV1"]);
    COV21 = as<mat>(x["COV21"]);
    MCOV1 = as<mat>(x["MCOV1"]);
    jcov1 = as<uvec>(x["jcov1"]);
    ind_cov1 = as<uvec>(x["ind_cov1"]);
    statphi11 = as<mat>(x["statphi11"]);
    statphi12 = as<mat>(x["statphi12"]);

    nphi0 = as<int>(x["nphi0"]);
    if (nphi0>0) {
      i0 = as<uvec>(x["i0"]);
      Gamma2_phi0 = as<mat>(x["Gamma2_phi0"]);
      // C_i mu_{k+1} = mprior_phi0
      mprior_phi0 = as<mat>(x["mprior_phi0"]);
      COV0 = as<mat>(x["COV0"]);
      LCOV0 = as<mat>(x["LCOV0"]);
      COV20 = as<mat>(x["COV20"]);
      MCOV0 = as<mat>(x["MCOV0"]);
      jcov0 = as<uvec>(x["jcov0"]);
      ind_cov0 = as<uvec>(x["ind_cov0"]);
      statphi01 = as<mat>(x["statphi01"]);
      statphi02 = as<mat>(x["statphi02"]);
    }
    fixedIx0 = as<uvec>(x["fixed.i0"]);
    fixedIx1 = as<uvec>(x["fixed.i1"]);

    nlambda1 = as<int>(x["nlambda1"]);
    nlambda0 = as<int>(x["nlambda0"]);
    nlambda = nlambda1 + nlambda0;
    nb_param = nphi1 + nlambda + 1;
    nphi = nphi1+nphi0;
    Plambda.zeros(nlambda);
    ilambda1 = as<uvec>(x["ilambda1"]);
    ilambda0 = as<uvec>(x["ilambda0"]);

    DYF = zeros<mat>(mlen, nM);
    phi.set_size(N, nphi, nmc);

    //FIXME
    nendpnt=as<int>(x["nendpnt"]);
    ix_sorting=as<uvec>(x["ix_sorting"]);
    ys = y(ix_sorting);    //ys: obs sorted by endpnt
    ysM=as<vec>(x["ysM"]);
    y_offset=as<uvec>(x["y_offset"]);
    res_mod = as<uvec>(x["res.mod"]);
    // REprintf("res.mod\n");
    // Rcpp::print(Rcpp::wrap(res_mod));
    ares = as<vec>(x["ares"]);
    bres = as<vec>(x["bres"]);
    cres = as<vec>(x["cres"]);
    lres = as<vec>(x["lres"]);
    yj = as<uvec>(x["yj"]);
    propT=as<uvec>(x["propT"]);
    // REprintf("yj\n");
    // Rcpp::print(Rcpp::wrap(yj));
    lambda = as<vec>(x["lambda"]);
    low = as<vec>(x["low"]);
    hi = as<vec>(x["hi"]);

    ix_endpnt=as<uvec>(x["ix_endpnt"]);
    ix_idM=as<umat>(x["ix_idM"]);
    res_offset=as<uvec>(x["res_offset"]);
    addProp=as<uvec>(x["addProp"]);
    nres = res_offset.max();
    vcsig2.set_size(nres);
    vecares = ares(ix_endpnt);
    vecbres = bres(ix_endpnt);
    veccres = cres(ix_endpnt);
    veclres = lres(ix_endpnt);
    for (int b=0; b<nendpnt; ++b) {
      sigma2[b] = 10;
      if (res_mod(b) == rmAdd) {
        sigma2[b] = max(ares(b)*ares(b), 10.0);
      }
      if (res_mod(b) == rmProp) {
        sigma2[b] = max(bres(b)*bres(b), 1.0);
      }
      statrese[b] = 0.0;
    }

    print = as<int>(x["print"]);
    par_hist = as<mat>(x["par.hist"]);
    parHistThetaKeep=as<uvec>(x["parHistThetaKeep"]);
    parHistThetaKeep = find(parHistThetaKeep);
    parHistOmegaKeep=as<uvec>(x["parHistOmegaKeep"]);
    parHistOmegaKeep = find(parHistOmegaKeep);

    L  = zeros<vec>(nb_param);
    Ha = zeros<mat>(nb_param,nb_param);
    Hb = zeros<mat>(nb_param,nb_param);
    mpost_phi = zeros<mat>(N, nphi);
    cpost_phi = zeros<mat>(N, nphi);

    //handle situation when nphi0=0
    mprior_phi0.set_size(N, nphi0);
    statphi01.set_size(N, nphi0);

    mx.nM     = nM;
    mx.yM     = yM;
    mx.indioM = indioM;
    mx.evtM   = evtM;
    mx.optM   = optM;

    distribution=as<int>(x["distribution"]);
    DEBUG=as<int>(x["DEBUG"]);
    phiMFile=as<std::vector< std::string > >(x["phiMFile"]);
    //Rcout << phiMFile[0];

  }

  void saem_fit() {
    //arma_rng::set_seed(99);
    double double_xmin = 1.0e-200; //FIXME hard-coded xmin, also in neldermean.hpp
    double xmax = 1e300;
    ofstream phiFile;
    _warnAtolRtol = false;
    phiFile.open(phiMFile[0].c_str());

    if (DEBUG>0) {
      RSprintf("initialization successful\n");
    }
    fsaveMat = user_fn(phiM, evtM, optM);
    limit = fsaveMat.col(2);
    limitT = fsaveMat.col(2);
    cens = fsaveMat.col(1);
    fsave = fsaveMat.col(0);
    if (DEBUG>0){
      RSprintf("initial user_fn successful\n");
    }
    for (unsigned int kiter=0; kiter<(unsigned int)(niter); kiter++) {
      gamma2_phi1=Gamma2_phi1.diag();
      IGamma2_phi1=inv_sympd(Gamma2_phi1);
      D1Gamma21=LCOV1*IGamma2_phi1;
      D2Gamma21=D1Gamma21*LCOV1.t();
      CGamma21=COV21%D2Gamma21;

      gamma2_phi0=Gamma2_phi0.diag();
      IGamma2_phi0=inv_sympd(Gamma2_phi0);
      D1Gamma20=LCOV0*IGamma2_phi0;
      D2Gamma20=D1Gamma20*LCOV0.t();
      CGamma20=COV20%D2Gamma20;

      //    MCMC
      mcmcphi mphi1, mphi0;
      set_mcmcphi(mphi1, i1, nphi1, Gamma2_phi1, IGamma2_phi1, mprior_phi1);
      set_mcmcphi(mphi0, i0, nphi0, Gamma2_phi0, IGamma2_phi0, mprior_phi0);

      // CHG hard coded 20
      int nu1, nu2, nu3;
      if (kiter==0) {
        nu1=20*nu(0);
        nu2=20*nu(1);
        nu3=20*nu(2);
      } else {
        nu1=nu(0);
        nu2=nu(1);
        nu3=nu(2);
      }

      vec f = fsave;
      fsave = f;
      if (distribution == 1){
        // REprintf("dist=1\n");
        vec ft = f;
        vec ftT(ft.size());
        vec yt = yM;
        for (int i = ft.size(); i--;) {
          int cur = ix_endpnt(i);
          limitT[i] = _powerD(limit[i], lambda(cur), yj(cur), low(cur), hi(cur));
          ft(i)   = _powerD(f(i), lambda(cur), yj(cur), low(cur), hi(cur));
          yt(i)   = _powerD(yM(i), lambda(cur), yj(cur), low(cur), hi(cur));
          ftT(i)  = handleF(propT(cur), ft(i), f(i), false, true);
        }
        // focei: rx_r_ = eff^2 * prop.sd^2 + add_sd^2
        // focei g = sqrt(eff^2*prop.sd^2 + add.sd^2)
        // This does not match focei's definition of add+prop
        vec g;
        g = vecares + vecbres % abs(ftT); //make sure g > 0
        g.elem( find( g == 0.0) ).fill(1.0); // like Uppusla IWRES allows prop when f=0
        g.elem( find( g < double_xmin) ).fill(double_xmin);
        g.elem( find(g > xmax)).fill(xmax);

        DYF(indioM)=0.5*(((yt-ft)/g)%((yt-ft)/g)) + log(g);
        doCens(DYF, cens, limitT, f, g, yM);
      } else if (distribution == 2){
        DYF(indioM)=-yM%log(f)+f;
      } else if (distribution == 3) {
        DYF(indioM)=-yM%log(f)-(1-yM)%log(1-f);
      }
      else {
        RSprintf("unknown distribution (id=%d)\n", distribution);
        return;
      }
      //U_y is a vec of subject llik; summed over obs for each subject
      vec U_y=sum(DYF, 0).t();

      if(nphi1>0) {
        vec U_phi;
        do_mcmc(1, nu1, mx, mphi1, DYF, phiM, U_y, U_phi);
        mat dphi = phiM.cols(i1)-mphi1.mprior_phiM;
        U_phi    = 0.5*sum(dphi%(dphi*IGamma2_phi1),1);
        do_mcmc(2, nu2, mx, mphi1, DYF, phiM, U_y, U_phi);
        do_mcmc(3, nu3, mx, mphi1, DYF, phiM, U_y, U_phi);
      }
      if(nphi0>0) {
        vec U_phi;
        do_mcmc(1, nu1, mx, mphi0, DYF, phiM, U_y, U_phi);
        mat dphi = phiM.cols(i0)-mphi0.mprior_phiM;
        U_phi    = 0.5*sum(dphi%(dphi*IGamma2_phi0),1);
        do_mcmc(2, nu2, mx, mphi0, DYF, phiM, U_y, U_phi);
        do_mcmc(3, nu3, mx, mphi0, DYF, phiM, U_y, U_phi);
      }
      if (DEBUG>0) Rcout << "mcmc successful\n";
      phiFile << phiM;
      //mat dphi=phiM.cols(i1)-mphi1.mprior_phiM;
      //vec U_phi=0.5*sum(dphi%(dphi*IGamma2_phi1),1);

      //  MCMC stochastic approximation
      mat Statphi11=zeros<mat>(N,nphi1);
      mat Statphi01=zeros<mat>(N,nphi0);
      mat Statphi12=zeros<mat>(nphi1,nphi1);
      mat Statphi02=zeros<mat>(nphi0,nphi0);
      double statr[MAXENDPNT], resk;
      for(int b=0; b<nendpnt; ++b) {
        statr[b]= 0;
      }

      vec D1 = zeros<vec>(nb_param);    //CHG!!!
      mat D11 = zeros<mat>(nb_param,nb_param);
      mat D2 = zeros<mat>(nb_param,nb_param);
      vec resy(nmc);
      mat d2logk = zeros<mat>(nb_param,nb_param);

      d2logk(span(0,nlambda1-1), span(0,nlambda1-1))=-CGamma21;
      if (nphi0 > 0) {
        d2logk(span(nlambda1,nlambda-1), span(nlambda1,nlambda-1))=-CGamma20;
      }

      vec fsM;
      fsM.set_size(0);
      //integration
      for(int k=0; k<nmc; k++) {
        phi.slice(k)=phiM.rows(span(k*N, (k+1)*N-1));

        Statphi11 += phi.slice(k).cols(i1);
        Statphi01 += phi.slice(k).cols(i0);
        mat phik=phi.slice(k);
        mat phi1k=phik.cols(i1);
        mat phi0k=phik.cols(i0);
        Statphi12=Statphi12+phi1k.t()*phi1k;
        Statphi02=Statphi02+phi0k.t()*phi0k;

        vec fk = fsave(span(k*ntotal, (k+1)*ntotal-1));
        fk = fk(ix_sorting);    //sorted by endpnt
        fsM = join_cols(fsM, fk);
        // vec resid_all(ys.size());// = ys - fk;
        vec gk, y_cur, f_cur;
        double ft, fa;
        //loop thru endpoints here
        for(int b=0; b<nendpnt; ++b) {
          y_cur = ys(span(y_offset(b), y_offset(b+1)-1));
          f_cur = fk(span(y_offset(b), y_offset(b+1)-1));
          vec resid(y_cur.size());
          for (int i = y_cur.size(); i--;){
            // lambda(cur), yj(cur), low(cur), hi(cur)
            resid(i) = _powerD(y_cur[i], lambda(b), yj(b), low(b), hi(b));
            if (std::isnan(resid(i))) {
              Rcpp::stop(_("NaN in data or transformed data; please check transformation/data"));
            }
            ft = _powerD(f_cur[i], lambda(b), yj(b), low(b), hi(b));
            resid(i) -=  ft;
            if (res_mod(b) == rmProp) {
              fa = handleF(propT(b), ft, f_cur[i], true, true);
              if (fa <= double_xmin) {
                fa = 1;
              }
              resid(i) = resid(i)/fa;
            }
          }
#if 0
          uvec iix = find(resid>1e9);
          Rcout << b << " " <<iix;
          Rcout << ys(iix) << fk(iix) << gk(iix);
#endif

          if (res_mod(b) <= rmProp) {
            resk = dot(resid, resid);
            if (resk > xmax) {
              resk = xmax;
            } else if (resk < double_xmin) {
              resk = double_xmin;
            }
          }
          else {
            resk = 1;                                              //FIXME
          }

          statr[b]=statr[b]+resk;
          resy(k) = resk;                                          //FIXME: resy(b,k)?
        }
        if (DEBUG>1) Rcout << "star[] successful\n";

        mat dphi1k=phi1k-mprior_phi1;
        mat dphi0k=phi0k-mprior_phi0;
        vec sdg1=sum(dphi1k%dphi1k,0).t()/gamma2_phi1;
        mat Md1=(IGamma2_phi1*(dphi1k.t()*Mcovariables)).t();
        mat Md0=(IGamma2_phi0*(dphi0k.t()*Mcovariables)).t();
        vec d1_mu_phi1=Md1(ind_cov1);                              //CHK!! vec or mat
        vec d1_mu_phi0=Md0(ind_cov0);                              //CHK!! vec or mat
        vec d1_loggamma2_phi1=0.5*sdg1-0.5*N;
        vec d1_logsigma2(1);
        d1_logsigma2[0] =  0.5*resy(k)/sigma2[0]-0.5*ntotal; //FIXME: sigma2[0], sigma2[b] instead?
        vec d1logk=join_cols(d1_mu_phi1, join_cols(d1_mu_phi0, join_cols(d1_loggamma2_phi1, d1_logsigma2)));
        D1 = D1+d1logk;
        D11= D11+d1logk*d1logk.t();

        vec w2phi=-0.5*sdg1;                                       //CHK!!!
        for(int j=0, l=0; j<nphi1; j++) {
          for(unsigned int jj=0; jj<pc1(j); jj++) {
            double temp=-dot(COV1.col(l),dphi1k.col(j))/gamma2_phi1(j);
            d2logk(l,nlambda+j)=temp;
            d2logk(nlambda+j,l)=temp;
            l=l+1;
          }
          d2logk(nlambda+j,nlambda+j)=w2phi(j);
        }
        d2logk(nb_param-1,nb_param-1)=-0.5*resy(k)/sigma2[0];      //FIXME: sigma2[0], sigma2[b] instead?
        D2=D2+d2logk;
      }//k
      if (DEBUG>0) Rcout << "integration successful\n";

      statphi11=statphi11+pas(kiter)*(Statphi11/nmc-statphi11);
      statphi12=statphi12+pas(kiter)*(Statphi12/nmc-statphi12);
      statphi01=statphi01+pas(kiter)*(Statphi01/nmc-statphi01);
      // s_{2, k} = statphi02
      statphi02=statphi02+pas(kiter)*(Statphi02/nmc-statphi02);
      for(int b=0; b<nendpnt; ++b) {
        statrese[b]=statrese[b]+pas(kiter)*(statr[b]/nmc-statrese[b]);
      }

      // update parameters
      vec Plambda1, Plambda0;
      Plambda1=inv_sympd(CGamma21)*sum((D1Gamma21%(COV1.t()*statphi11)),1);
      if (fixedIx1.n_elem>0) {
        Plambda1(fixedIx1) = MCOV1(jcov1(fixedIx1));
      }
      MCOV1(jcov1)=Plambda1;
      if (nphi0>0) {
        Plambda0=inv_sympd(CGamma20)*sum((D1Gamma20%(COV0.t()*statphi01)),1);
        if (fixedIx0.n_elem>0) {
          Plambda0(fixedIx0) = MCOV0(jcov0(fixedIx0));
        }
        MCOV0(jcov0)=Plambda0;
      }
      mprior_phi1=COV1*MCOV1;
      mprior_phi0=COV0*MCOV0;
      mprior_phi0.set_size(N, nphi0);                              // deal w/ nphi0=0

      mat G1=(statphi12+mprior_phi1.t()*mprior_phi1- statphi11.t()*mprior_phi1 - mprior_phi1.t()*statphi11)/N;
      if (kiter<=(unsigned int)(nb_sa)) {
        Gamma2_phi1=max(Gamma2_phi1*coef_sa, diagmat(G1));
      } else {
        Gamma2_phi1=G1;
      }
      Gamma2_phi1=Gamma2_phi1%covstruct1;
      vec Gmin=minv(i1);
      uvec jDmin=find(Gamma2_phi1.diag()<Gmin);
      for(unsigned int jm=0; jm<jDmin.n_elem; jm++) {
        Gamma2_phi1(jDmin(jm),jDmin(jm))=Gmin(jDmin(jm));
      }
      // fix before diagonals are enforced
      if (Gamma2_phi1fixed==1 && kiter > (unsigned int)(nb_fixOmega)) {
        Gamma2_phi1.elem(Gamma2_phi1fixedIx) = Gamma2_phi1fixedValues(Gamma2_phi1fixedIx);
      }

      if (kiter<=(unsigned int)(nb_correl)) {
        Gamma2_phi1 = diagmat(Gamma2_phi1);
      }

      if (nphi0>0) {
        if (kiter<=(unsigned int)(niter_phi0)) {
          // omega estimation
          Gamma2_phi0=(statphi02 + mprior_phi0.t()*mprior_phi0 - statphi01.t()*mprior_phi0 - mprior_phi0.t()*statphi01)/N;
          Gmin=minv(i0);
          jDmin=find(Gamma2_phi0.diag()<Gmin);
          for(unsigned int jm=0; jm<jDmin.n_elem; jm++) {
            Gamma2_phi0(jDmin(jm),jDmin(jm))=Gmin(jDmin(jm));
          }
          dGamma2_phi0=Gamma2_phi0.diag();
        } else {
          dGamma2_phi0=dGamma2_phi0*coef_phi0;
        }
        Gamma2_phi0=diagmat(dGamma2_phi0);                         //CHK
      }
      //CHECK the following seg on b & yptr & fptr
      for(int b=0; b<nendpnt; ++b) {
        double sig2=statrese[b]/(y_offset(b+1)-y_offset(b));       //CHK: range
        int offsetR = res_offset[b];
        _saemFixedIdx[0] = _saemFixedIdx[1] = _saemFixedIdx[2] = _saemFixedIdx[3] = 0;
        switch (res_mod(b)) {
        case rmAdd:
          {
            if (resFixed[offsetR] == 1 && kiter > (unsigned int)(nb_fixResid)) {
              ares(b) = resValue[offsetR];
            } else {
              ares(b) = sqrt(sig2);
            }
          }
          break;
        case rmProp:
          {
            if (resFixed[offsetR] == 1 && kiter > (unsigned int)(nb_fixResid)) {
              bres(b) = resValue[offsetR];
            } else {
              if (sig2 == 0) sig2 = 1;
              bres(b) = sqrt(sig2);
            }
          }
          break;
        case rmAddProp:
          {
            uvec idx;
            idx = find(ix_endpnt==b);
            vec ysb, fsb;

            ysb = ysM(idx);
            fsb = fsM(idx);

            // yptr = ysb.memptr();
            // fptr = fsb.memptr();
            //len = ysb.n_elem;                                        //CHK: needed by nelder
            vec xmin(2);
            double *pxmin = xmin.memptr();
            int n=2;
            double start[2]={sqrt(fabs(ares(b))), sqrt(fabs(bres(b)))};                  //force are & bres to be positive
            double step[2]={-.2, -.2};

            if (kiter > (unsigned int)(nb_fixResid)) {
              n = 0;
              int curi=0;
              if (resFixed[offsetR] == 1) {
                ares(b) = resValue[offsetR];
                _saemFixedIdx[0] = 1;
                _saemFixedValue[0] = sqrt(fabs(ares(b)));
              } else {
                start[curi++] = sqrt(fabs(ares(b)));
                n++;
              }
              if (resFixed[offsetR + 1] == 1) {
                bres(b) = resValue[offsetR + 1];
                _saemFixedIdx[1] = 1;
                _saemFixedValue[1] = sqrt(fabs(bres(b)));
              } else {
                start[curi++] = sqrt(fabs(bres(b)));
                n++;
              }
            }

            // f = sum((ytr-ft)/g);
            _saemYptr = ysb.memptr();
            _saemFptr = fsb.memptr();
            _saemLen  = ysb.n_elem;
            _saemYj   = yj(b);
            _saemPropT = propT(b);
            _saemAddProp=addProp(b);
            _saemLambda = lambda(b);
            _saemLow = low(b);
            _saemHi = hi(b);
            _saemFn = obj;
            _saemStep = step;
            _saemStart=start;
            _saemOpt(n, pxmin);
            // Adjust back
            if (kiter > (unsigned int)(nb_fixResid)) {
              int curi = 0;
              if (resFixed[offsetR] == 0) {
                double ab02 = pxmin[curi++];
                ares(b) = ares(b) + pas(kiter)*(ab02*ab02 - ares(b));    //force are & bres to be positive
              }
              if (resFixed[offsetR + 1] == 0) {
                double ab12 = pxmin[curi++];
                bres(b) = bres(b) + pas(kiter)*(ab12*ab12 - bres(b));    //force are & bres to be positive
              }
            } else {
              double ab02 = pxmin[0];
              double ab12 = pxmin[1];
              ares(b) = ares(b) + pas(kiter)*(ab02*ab02 - ares(b));    //force are & bres to be positive
              bres(b) = bres(b) + pas(kiter)*(ab12*ab12 - bres(b));    //force are & bres to be positive
            }
          }
          break;
        case rmAddPow:
          { // add + pow
            uvec idx;
            idx = find(ix_endpnt==b);
            vec ysb, fsb;

            ysb = ysM(idx);
            fsb = fsM(idx);

            // yptr = ysb.memptr();
            // fptr = fsb.memptr();
            //len = ysb.n_elem;                                        //CHK: needed by nelder
            vec xmin(3);
            double *pxmin = xmin.memptr();
            int n=3;

            // REprintf("ares: %f bres: %f cres: %f\n", ares(b), bres(b), cres(b));
            double start[3]={sqrt(fabs(ares(b))), sqrt(fabs(bres(b))), toPowEst(cres(b))}; //force are & bres to be positive
            double step[3]={-.2, -.2, -.2};
            if (kiter > (unsigned int)(nb_fixResid)) {
              n = 0;
              int curi=0;
              if (resFixed[offsetR] == 1) {
                ares(b) = resValue[offsetR];
                _saemFixedIdx[0] = 1;
                _saemFixedValue[0] = sqrt(fabs(ares(b)));
              } else {
                start[curi++] = sqrt(fabs(ares(b)));
                n++;
              }
              if (resFixed[offsetR + 1] == 1) {
                bres(b) = resValue[offsetR + 1];
                _saemFixedIdx[1] = 1;
                _saemFixedValue[1] = sqrt(fabs(bres(b)));
              } else {
                start[curi++] = sqrt(fabs(bres(b)));
                n++;
              }
              if (resFixed[offsetR + 2] == 1) {
                cres(b) = resValue[offsetR + 2];
                _saemFixedIdx[2] = 1;
                _saemFixedValue[2] = toPowEst(cres(b));
              } else {
                start[curi++] = toPowEst(cres(b));
                n++;
              }
            }
            _saemYptr = ysb.memptr();
            _saemFptr = fsb.memptr();
            _saemLen  = ysb.n_elem;
            _saemYj   = yj(b);
            _saemPropT = propT(b);
            _saemAddProp = addProp(b);
            _saemLambda = lambda(b);
            _saemLow = low(b);
            _saemHi = hi(b);
            _saemStep = step;
            _saemStart = start;
            _saemFn = objC;
            _saemOpt(n, pxmin);
            // REprintf("\tares: %f bres: %f cres: %f\n", pxmin[0], pxmin[1], pxmin[2]);
            if (kiter > (unsigned int)(nb_fixResid)) {
              int curi = 0;
              if (resFixed[offsetR] == 0) {
                double ab02 = pxmin[curi++];
                ares(b) = ares(b) + pas(kiter)*(ab02*ab02 - ares(b));    //force are & bres to be positive
              }
              if (resFixed[offsetR + 1] == 0) {
                double ab12 = pxmin[curi++];
                bres(b) = bres(b) + pas(kiter)*(ab12*ab12 - bres(b));    //force are & bres to be positive
              }
              if (resFixed[offsetR + 2] == 0) {
                cres(b) = cres(b) + pas(kiter)*(toPow(pxmin[curi++]) - cres(b));    //force are & bres to be positive
              }
            } else {
              ares(b) = ares(b) + pas(kiter)*(pxmin[0]*pxmin[0] - ares(b)); //force ares & bres to be positive
              bres(b) = bres(b) + pas(kiter)*(pxmin[1]*pxmin[1] - bres(b)); //force ares & bres to be positive
              cres(b) = cres(b) + pas(kiter)*(toPow(pxmin[2]) - cres(b));
            }
          }
          break;
        case rmPow:
          { // power
            uvec idx;
            idx = find(ix_endpnt==b);
            vec ysb, fsb;

            ysb = ysM(idx);
            fsb = fsM(idx);

            //len = ysb.n_elem;                                        //CHK: needed by nelder
            vec xmin(2);
            double *pxmin = xmin.memptr();
            int n=2;
            double start[2]={sqrt(fabs(bres(b))), toPowEst(cres(b))};                  //force are & bres to be positive
            double step[2]={ -.2, -.2};
            if (kiter > (unsigned int)(nb_fixResid)) {
              n = 0;
              int curi=0;
              if (resFixed[offsetR] == 1) {
                bres(b) = resValue[offsetR];
                _saemFixedIdx[0] = 1;
                _saemFixedValue[0] = sqrt(fabs(bres(b)));
              } else {
                start[curi++] = sqrt(fabs(bres(b)));
                n++;
              }
              if (resFixed[offsetR + 1] == 1) {
                cres(b) = resValue[offsetR + 1];
                _saemFixedIdx[1] = 1;
                _saemFixedValue[1] = toPowEst(cres(b));
              } else {
                start[curi++] = toPowEst(cres(b));
                n++;
              }
            }
            _saemYptr = ysb.memptr();
            _saemFptr = fsb.memptr();
            _saemLen  = ysb.n_elem;
            _saemYj   = yj(b);
            _saemPropT = propT(b);
            _saemAddProp =addProp(b);
            _saemLambda = lambda(b);
            _saemLow = low(b);
            _saemHi = hi(b);
            _saemStep = step;
            _saemStart = start;
            _saemFn = objD;
            _saemOpt(n, pxmin);
            if (kiter > (unsigned int)(nb_fixResid)) {
              int curi = 0;
              if (resFixed[offsetR] == 0) {
                double ab02 = pxmin[curi++];
                bres(b) = bres(b) + pas(kiter)*(ab02*ab02 - bres(b));    //force are & bres to be positive
              }
              if (resFixed[offsetR + 1] == 0) {
                double ab12 = pxmin[curi++];
                cres(b) = cres(b) + pas(kiter)*(toPow(ab12) - cres(b));    //force are & bres to be positive
              }
            } else {
              bres(b) = bres(b) + pas(kiter)*(pxmin[0]*pxmin[1] - bres(b));    //force are & bres to be positive
              cres(b) = cres(b) + pas(kiter)*(toPow(pxmin[1]) - cres(b));      //force are & bres to be positive
            }
          }
          break;
        case rmAddLam:
          { // additive + lambda
            uvec idx;
            idx = find(ix_endpnt==b);
            vec ysb, fsb;

            ysb = ysM(idx);
            fsb = fsM(idx);

            //len = ysb.n_elem;                                        //CHK: needed by nelder
            vec xmin(2);
            double *pxmin = xmin.memptr();
            int n=2;
            double start[2]={sqrt(fabs(ares(b))), toLambdaEst(lres(b))};                  //force are & bres to be positive
            double step[2]={ -.2, -.2};
            if (kiter > (unsigned int)(nb_fixResid)) {
              n = 0;
              int curi=0;
              if (resFixed[offsetR] == 1) {
                ares(b) = resValue[offsetR];
                _saemFixedIdx[0] = 1;
                _saemFixedValue[0] = sqrt(fabs(ares(b)));
              } else {
                start[curi++] = sqrt(fabs(ares(b)));
                n++;
              }
              if (resFixed[offsetR + 1] == 1) {
                lres(b) = resValue[offsetR + 1];
                _saemFixedIdx[1] = 1;
                _saemFixedValue[1] = toLambdaEst(lres(b));
              } else {
                start[curi++] = toLambdaEst(lres(b));
                n++;
              }
            }
            _saemYptr = ysb.memptr();
            _saemFptr = fsb.memptr();
            _saemLen  = ysb.n_elem;
            _saemYj   = yj(b);
            _saemPropT = propT(b);
            _saemAddProp = addProp(b);
            _saemLambda = lambda(b);
            _saemLow = low(b);
            _saemHi = hi(b);
            _saemStep = step;
            _saemStart = start;
            _saemFn = objE;
            _saemOpt(n, pxmin);
            if (kiter > (unsigned int)(nb_fixResid)) {
              int curi = 0;
              if (resFixed[offsetR] == 0) {
                double ab02 = pxmin[curi++];
                ares(b) = ares(b) + pas(kiter)*(ab02*ab02 - ares(b));    //force are & bres to be positive
              }
              if (resFixed[offsetR + 1] == 0) {
                double ab12 = pxmin[curi++];
                lres(b) = lres(b) + pas(kiter)*(toLambda(ab12) - lres(b));    //force are & bres to be positive
              }
            } else {
              ares(b) = ares(b) + pas(kiter)*(pxmin[0]*pxmin[0] - ares(b));    //force are & bres to be positive
              lres(b) = lres(b) + pas(kiter)*(toLambda(pxmin[1]) - lres(b));   //force are & bres to be positive
            }
          }
          break;
        case rmPropLam:
          { // prop + lambda
            uvec idx;
            idx = find(ix_endpnt==b);
            vec ysb, fsb;

            ysb = ysM(idx);
            fsb = fsM(idx);

            //len = ysb.n_elem;                                        //CHK: needed by nelder
            vec xmin(2);
            double *pxmin = xmin.memptr();
            int n=2;
            double start[2]={sqrt(fabs(bres(b))), toLambdaEst(lres(b))};                  //force are & bres to be positive
            double step[2]={ -.2, -.2};
            if (kiter > (unsigned int)(nb_fixResid)) {
              n = 0;
              int curi=0;
              if (resFixed[offsetR] == 1) {
                bres(b) = resValue[offsetR];
                _saemFixedIdx[0] = 1;
                _saemFixedValue[0] = sqrt(fabs(bres(b)));
              } else {
                start[curi++] = sqrt(fabs(bres(b)));
                n++;
              }
              if (resFixed[offsetR + 1] == 1) {
                lres(b) = resValue[offsetR + 1];
                _saemFixedIdx[1] = 1;
                _saemFixedValue[1] = toLambdaEst(lres(b));
              } else {
                start[curi++] = toLambdaEst(lres(b));
                n++;
              }
            }
            _saemYptr = ysb.memptr();
            _saemFptr = fsb.memptr();
            _saemLen  = ysb.n_elem;
            _saemYj   = yj(b);
            _saemPropT = propT(b);
            _saemAddProp = addProp(b);
            _saemLambda = lambda(b);
            _saemLow = low(b);
            _saemHi = hi(b);
            _saemStep = step;
            _saemStart = start;
            _saemFn = objF;
            _saemOpt(n, pxmin);
            if (kiter > (unsigned int)(nb_fixResid)) {
              int curi = 0;
              if (resFixed[offsetR] == 0) {
                double ab02 = pxmin[curi++];
                bres(b) = bres(b) + pas(kiter)*(ab02*ab02 - bres(b));    //force are & bres to be positive
              }
              if (resFixed[offsetR + 1] == 0) {
                double ab12 = pxmin[curi++];
                lres(b) = lres(b) + pas(kiter)*(toLambda(ab12) - lres(b));    //force are & bres to be positive
              }
            } else {
              bres(b) = bres(b) + pas(kiter)*(pxmin[0]*pxmin[0] - bres(b));    //force are & bres to be positive
              lres(b) = lres(b) + pas(kiter)*(toLambda(pxmin[1]) - lres(b));            //force are & bres to be positive
            }
          }
          break;
        case rmPowLam:
          { // pow + lambda
            uvec idx;
            idx = find(ix_endpnt==b);
            vec ysb, fsb;

            ysb = ysM(idx);
            fsb = fsM(idx);

            //len = ysb.n_elem;                                        //CHK: needed by nelder
            vec xmin(2);
            double *pxmin = xmin.memptr();
            int n=3;
            double start[3]={sqrt(fabs(bres(b))), toPowEst(cres(b)), toLambdaEst(lres(b))};                  //force are & bres to be positive
            double step[3]={ -.2, -.2, -.2};
            if (kiter > (unsigned int)(nb_fixResid)) {
              n = 0;
              int curi=0;
              if (resFixed[offsetR] == 1) {
                bres(b) = resValue[offsetR];
                _saemFixedIdx[0] = 1;
                _saemFixedValue[0] = sqrt(fabs(bres(b)));
              } else {
                start[curi++] = sqrt(fabs(bres(b)));
                n++;
              }
              if (resFixed[offsetR+1] == 1) {
                cres(b) = resValue[offsetR+1];
                _saemFixedIdx[1] = 1;
                _saemFixedValue[1] = toPowEst(cres(b));
              } else {
                start[curi++] = toPowEst(cres(b));
                n++;
              }
              if (resFixed[offsetR + 2] == 1) {
                lres(b) = resValue[offsetR + 2];
                _saemFixedIdx[2] = 1;
                _saemFixedValue[2] = toLambdaEst(lres(b));
              } else {
                start[curi++] = toLambdaEst(lres(b));
                n++;
              }
            }
            _saemYptr = ysb.memptr();
            _saemFptr = fsb.memptr();
            _saemLen  = ysb.n_elem;
            _saemYj   = yj(b);
            _saemPropT = propT(b);
            _saemAddProp = addProp(b);
            _saemLambda = lambda(b);
            _saemLow = low(b);
            _saemHi = hi(b);
            _saemStep = step;
            _saemStart = start;
            _saemFn = objG;
            _saemOpt(n, pxmin);
            if (kiter > (unsigned int)(nb_fixResid)) {
              int curi = 0;
              if (resFixed[offsetR] == 0) {
                double ab02 = pxmin[curi++];
                bres(b) = bres(b) + pas(kiter)*(ab02*ab02 - bres(b));    //force are & bres to be positive
              }
              if (resFixed[offsetR+1] == 0) {
                double ab02 = pxmin[curi++];
                cres(b) = cres(b) + pas(kiter)*(toPow(ab02) - cres(b));    //force are & bres to be positive
              }
              if (resFixed[offsetR + 2] == 0) {
                double ab12 = pxmin[curi++];
                lres(b) = lres(b) + pas(kiter)*(toLambda(ab12) - lres(b));    //force are & bres to be positive
              }
            } else {
              bres(b) = bres(b) + pas(kiter)*(pxmin[0]*pxmin[0] - bres(b));    //force are & bres to be positive
              cres(b) = cres(b) + pas(kiter)*(toPow(pxmin[1]) - cres(b));    //force are & bres to be positive
              lres(b) = lres(b) + pas(kiter)*(toLambda(pxmin[2]) - lres(b));            //force are & bres to be positive
            }
          }
          break;
        case rmAddPropLam:
          { // add + prop + lambda
            uvec idx;
            idx = find(ix_endpnt==b);
            vec ysb, fsb;

            ysb = ysM(idx);
            fsb = fsM(idx);

            //len = ysb.n_elem;                                        //CHK: needed by nelder
            vec xmin(2);
            double *pxmin = xmin.memptr();
            int n=3;
            double start[3]={sqrt(fabs(ares(b))), sqrt(fabs(bres(b))), toLambdaEst(lres(b))};                  //force are & bres to be positive
            double step[3]={ -.2, -.2, -.2};
            if (kiter > (unsigned int)(nb_fixResid)) {
              n = 0;
              int curi=0;
              if (resFixed[offsetR] == 1) {
                ares(b) = resValue[offsetR];
                _saemFixedIdx[0] = 1;
                _saemFixedValue[0] = sqrt(fabs(ares(b)));
              } else {
                start[curi++] = sqrt(fabs(ares(b)));
                n++;
              }
              if (resFixed[offsetR+1] == 1) {
                bres(b) = resValue[offsetR+1];
                _saemFixedIdx[1] = 1;
                _saemFixedValue[1] = sqrt(fabs(bres(b)));
              } else {
                start[curi++] = sqrt(fabs(bres(b)));
                n++;
              }
              if (resFixed[offsetR + 2] == 1) {
                lres(b) = resValue[offsetR + 2];
                _saemFixedIdx[2] = 1;
                _saemFixedValue[2] = toLambdaEst(lres(b));
              } else {
                start[curi++] = toLambdaEst(lres(b));
                n++;
              }
            }
            _saemYptr = ysb.memptr();
            _saemFptr = fsb.memptr();
            _saemLen  = ysb.n_elem;
            _saemYj   = yj(b);
            _saemPropT = propT(b);
            _saemAddProp = addProp(b);
            _saemLambda = lambda(b);
            _saemLow = low(b);
            _saemHi = hi(b);
            _saemStep = step;
            _saemStart = start;
            _saemFn = objH;
            _saemOpt(n, pxmin);
            if (kiter > (unsigned int)(nb_fixResid)) {
              int curi = 0;
              if (resFixed[offsetR] == 0) {
                double ab02 = pxmin[curi++];
                ares(b) = ares(b) + pas(kiter)*(ab02*ab02 - ares(b));    //force are & bres to be positive
              }
              if (resFixed[offsetR+1] == 0) {
                double ab02 = pxmin[curi++];
                bres(b) = bres(b) + pas(kiter)*(ab02*ab02 - bres(b));    //force are & bres to be positive
              }
              if (resFixed[offsetR + 2] == 0) {
                double ab12 = pxmin[curi++];
                lres(b) = lres(b) + pas(kiter)*(toLambda(ab12) - lres(b));    //force are & bres to be positive
              }
            } else {
              ares(b) = ares(b) + pas(kiter)*(pxmin[0]*pxmin[0] - ares(b));    //force are & bres to be positive
              bres(b) = bres(b) + pas(kiter)*(pxmin[1]*pxmin[1] - bres(b));    //force are & bres to be positive
              lres(b) = lres(b) + pas(kiter)*(toLambda(pxmin[2]) - lres(b));            //force are & bres to be positive
            }
          }
          break;
        case rmAddPowLam:
          { // add + pow + lambda
            uvec idx;
            idx = find(ix_endpnt==b);
            vec ysb, fsb;

            ysb = ysM(idx);
            fsb = fsM(idx);

            //len = ysb.n_elem;                                        //CHK: needed by nelder
            vec xmin(2);
            double *pxmin = xmin.memptr();
            int n=4;
            double start[4]={sqrt(fabs(ares(b))), sqrt(fabs(bres(b))), toPowEst(cres(b)), toLambdaEst(lres(b))};                  //force are & bres to be positive
            double step[4]={ -.2, -.2, -.2, -.2};
            if (kiter > (unsigned int)(nb_fixResid)) {
              n = 0;
              int curi=0;
              if (resFixed[offsetR] == 1) {
                ares(b) = resValue[offsetR];
                _saemFixedIdx[0] = 1;
                _saemFixedValue[0] = sqrt(fabs(ares(b)));
              } else {
                start[curi++] = sqrt(fabs(ares(b)));
                n++;
              }
              if (resFixed[offsetR+1] == 1) {
                bres(b) = resValue[offsetR+1];
                _saemFixedIdx[1] = 1;
                _saemFixedValue[1] = sqrt(fabs(bres(b)));
              } else {
                start[curi++] = sqrt(fabs(bres(b)));
                n++;
              }
              if (resFixed[offsetR+2] == 1) {
                cres(b) = resValue[offsetR+2];
                _saemFixedIdx[2] = 1;
                _saemFixedValue[2] = toPowEst(cres(b));
              } else {
                start[curi++] = toPowEst(cres(b));
                n++;
              }
              if (resFixed[offsetR + 3] == 1) {
                lres(b) = resValue[offsetR + 3];
                _saemFixedIdx[3] = 1;
                _saemFixedValue[3] = toLambdaEst(lres(b));
              } else {
                start[curi++] = toLambdaEst(lres(b));
                n++;
              }
            }
            _saemYptr = ysb.memptr();
            _saemFptr = fsb.memptr();
            _saemLen  = ysb.n_elem;
            _saemYj   = yj(b);
            _saemPropT = propT(b);
            _saemAddProp = addProp(b);
            _saemLambda = lambda(b);
            _saemLow = low(b);
            _saemHi = hi(b);
            _saemStep = step;
            _saemStart = start;
            _saemFn = objI;
            _saemOpt(n, pxmin);
            if (kiter > (unsigned int)(nb_fixResid)) {
              int curi = 0;
              if (resFixed[offsetR] == 0) {
                double ab02 = pxmin[curi++];
                ares(b) = ares(b) + pas(kiter)*(ab02*ab02 - ares(b));    //force are & bres to be positive
              }
              if (resFixed[offsetR+1] == 0) {
                double ab02 = pxmin[curi++];
                bres(b) = bres(b) + pas(kiter)*(ab02*ab02 - bres(b));    //force are & bres to be positive
              }
              if (resFixed[offsetR + 2] == 0) {
                double ab12 = pxmin[curi++];
                cres(b) = cres(b) + pas(kiter)*(toPow(ab12) - cres(b));    //force are & bres to be positive
              }
              if (resFixed[offsetR + 3] == 0) {
                double ab12 = pxmin[curi++];
                lres(b) = lres(b) + pas(kiter)*(toLambda(ab12) - lres(b));    //force are & bres to be positive
              }
            } else {
              ares(b) = ares(b) + pas(kiter)*(pxmin[0]*pxmin[0] - ares(b));    //force are & bres to be positive
              bres(b) = bres(b) + pas(kiter)*(pxmin[1]*pxmin[1] - bres(b));    //force are & bres to be positive
              cres(b) = cres(b) + pas(kiter)*(toPow(pxmin[2]) - cres(b));    //force are & bres to be positive
              lres(b) = lres(b) + pas(kiter)*(toLambda(pxmin[3]) - lres(b));            //force are & bres to be positive
            }
          }
          break;
        }
        sigma2[b] = sig2;                                          //CHK: sigma2[] use
        if (sigma2[b]>1.0e99) sigma2[b] = 1.0e99;
        if (std::isnan(sigma2[b])) sigma2[b] = 1.0e99;
      }
      vecares = ares(ix_endpnt);
      vecbres = bres(ix_endpnt);
      if (DEBUG>0) Rcout << "par update successful\n";

      //    Fisher information
      DDa=(D1/nmc)*(D1/nmc).t()-D11/nmc-D2/nmc;
      DDb=-D11/nmc-D2/nmc;
      L=L+pash(kiter)*(D1/nmc-L);
      Ha=Ha+pash(kiter)*(DDa- Ha);
      Hb=Hb+pash(kiter)*(DDb- Hb);
      cube phi2 = phi%phi;
      mat sphi1 = sum(phi ,2);
      mat sphi2 = sum(phi2,2);
      mpost_phi=mpost_phi+pash(kiter)*(sphi1/nmc-mpost_phi);
      cpost_phi=cpost_phi+pash(kiter)*(sphi2/nmc-cpost_phi);
      mpost_phi.cols(i0)=mprior_phi0;

      //FIXME: chg according to multiple endpnts; need to chg dim(par_hist)
      for (int b=0; b<nendpnt; ++b) {
        int offset = res_offset[b];
        switch ((int)(res_mod(b))) {
        case rmAdd:
          vcsig2[offset] = ares(b);//sigma2[b];
          // because of old translation use variance
          break;
        case rmProp:
          vcsig2[offset] = bres(b);
          break;
        case rmPow:
          vcsig2[offset]     = bres(b);
          vcsig2[offset + 1] = cres(b);
          break;
        case rmAddProp:
          vcsig2[offset]   = ares(b);
          vcsig2[offset+1] = bres(b);
          break;
        case rmAddPow:
          vcsig2[offset]   = ares(b);
          vcsig2[offset+1] = bres(b);
          vcsig2[offset+2] = cres(b);
          break;
        case rmAddLam:
          vcsig2[offset]   = ares(b);
          vcsig2[offset+1] = lres(b);
          break;
        case rmPropLam:
          vcsig2[offset]   = bres(b);
          vcsig2[offset+1] = lres(b);
          break;
        case rmPowLam:
          vcsig2[offset]   = bres(b);
          vcsig2[offset+1] = cres(b);
          vcsig2[offset+2] = lres(b);
          break;
        case rmAddPropLam:
          vcsig2[offset]   = ares(b);
          vcsig2[offset+1] = bres(b);
          vcsig2[offset+2] = lres(b);
          break;
        case rmAddPowLam:
          vcsig2[offset]   = ares(b);
          vcsig2[offset+1] = bres(b);
          vcsig2[offset+2] = cres(b);
          vcsig2[offset+3] = lres(b);
          break;
        }
      }
      Plambda(ilambda1) = Plambda1;
      Plambda(ilambda0) = Plambda0;
      vec pl = Plambda.elem(parHistThetaKeep);
      vec g2 = Gamma2_phi1.diag();
      g2 = g2.elem(parHistOmegaKeep);
      pl = join_cols(pl, g2);
      g2 = vcsig2.elem(resKeep);
      pl = join_cols(pl, g2);
      par_hist.row(kiter) = pl.t();
      if (print != 0 && (kiter==0 || (kiter+1)%print==0)) {
        RSprintf("%03d: ", kiter+1);
        for (arma::uword j=0; j < pl.size(); ++j) {
          RSprintf("%f\t", pl[j]);
        }
        RSprintf("\n");
      }
      Rcpp::checkUserInterrupt();
    }//kiter
    phiFile.close();
  }


private:

  user_funct user_fn;

  uvec nu;
  int niter;
  int nb_sa;
  int nb_correl;
  int nb_fixOmega;
  int nb_fixResid;
  int niter_phi0;
  double coef_phi0;
  double rmcmc;
  double coef_sa;
  vec pas, pash;
  vec minv;
  int nmc;
  int nM;

  int ntotal, N;
  vec y, yM, ys;    //ys is y sorted by endpnt
  mat evt, evtM;
  mat phiM;
  uvec indioM;
  mat Mcovariables;
  List opt, optM;

  int nphi0, nphi1, nphi;
  mat covstruct1;
  uvec i1, i0, fixedIx1, fixedIx0;
  umat Gamma2_phi1fixedIxIn;
  uvec Gamma2_phi1fixedIx;
  int Gamma2_phi1fixed;
  mat Gamma2_phi1fixedValues;
  uvec pc1;
  mat COV1, COV0, LCOV1, LCOV0, COV21, COV20, MCOV1, MCOV0;
  mat Gamma2_phi1, Gamma2_phi0, mprior_phi1, mprior_phi0;
  mat IGamma2_phi1, D1Gamma21, D2Gamma21, CGamma21;
  mat IGamma2_phi0, D1Gamma20, D2Gamma20, CGamma20;
  mat Gamma_phi1, Gdiag_phi1, Gamma_phi0, Gdiag_phi0;
  vec gamma2_phi1, gamma2_phi0;
  uvec ind_cov1, ind_cov0, jcov1, jcov0;
  vec dGamma2_phi0;
  vec Plambda;

  int nlambda1, nlambda0, nlambda, nb_param;
  uvec ilambda1, ilambda0;

  mat statphi01, statphi02, statphi11, statphi12;
  double statrese[MAXENDPNT];
  double sigma2[MAXENDPNT];
  vec ares, bres, cres, lres, lambda, low, hi;
  vec vecares, vecbres, veccres, veclres;
  uvec res_mod, yj, propT, addProp;

  mat DYF;
  cube phi;

  vec L;
  mat Ha, Hb, DDa, DDb;
  mat mpost_phi, cpost_phi;

  vec resValue;
  uvec resFixed;
  uvec resKeep;

  mcmcaux mx;

  int print;
  mat par_hist;
  uvec parHistThetaKeep;
  uvec parHistOmegaKeep;

  int distribution;

  int nendpnt;
  uvec ix_endpnt;
  umat ix_idM;
  uvec y_offset;
  uvec res_offset;
  vec vcsig2;
  int nres;
  uvec ix_sorting;
  vec ysM;
  mat fsaveMat;
  vec cens;
  vec limit;
  vec limitT;
  vec fsave;

  int DEBUG;
  std::vector< std::string > phiMFile;

  void set_mcmcphi(mcmcphi &mphi1,
		   const uvec i1,
		   const int nphi1,
		   const mat Gamma2_phi1,
		   const mat IGamma2_phi1,
		   const mat mprior_phi1) {
    mphi1.i = i1;
    mphi1.nphi = nphi1;
    mphi1.Gamma_phi=chol(Gamma2_phi1);
    mphi1.IGamma2_phi = IGamma2_phi1;
    mphi1.Gdiag_phi.zeros(nphi1, nphi1);
    mphi1.Gdiag_phi.diag() = sqrt(Gamma2_phi1.diag())*rmcmc;
    mphi1.mprior_phiM = repmat(mprior_phi1,nmc,1);
  }

  static inline void doCens(mat &DYF, vec &cens, vec &limit, vec &fc, vec &r, const vec &dv) {
    for (int j = (int)cens.size(); j--;) {
      DYF(j) = doCensNormal1(cens[j], dv[j], limit[j], DYF(j), fc[j], r[j], 0);
    }
  }

  void do_mcmc(const int method,
               const int nu,
               const mcmcaux &mx,
               const mcmcphi &mphi,
               mat &DYF,
               mat &phiM,
               vec &U_y,
               vec &U_phi) {
    mat fcMat;
    vec fc, fs, Uc_y, Uc_phi, deltu;
    uvec ind;
    vec gc;

    uvec i=mphi.i;
    double double_xmin = 1.0e-200;                               //FIXME hard-coded xmin, also in neldermean.hpp
    double xmax = 1e300;
    for (int u=0; u<nu; u++)
      for (int k1=0; k1<mphi.nphi; k1++) {
        mat phiMc=phiM;
        switch (method) {
        case 1:
          phiMc.cols(i)=randn<mat>(mx.nM,mphi.nphi)*mphi.Gamma_phi % _saemUE.cols(i) +
            mphi.mprior_phiM;
          break;
        case 2:
          phiMc.cols(i)=phiM.cols(i) +
            randn<mat>(mx.nM,mphi.nphi)*mphi.Gdiag_phi % _saemUE.cols(i);
          break;
        case 3:
          phiMc.col(i(k1))=phiM.col(i(k1))+
            randn<vec>(mx.nM)*mphi.Gdiag_phi(k1,k1) % _saemUE.col(k1);
          // Rcpp::print(Rcpp::wrap(phiM.cols(i(k1))));
          break;
        }

        fcMat = user_fn(phiMc, mx.evtM, mx.optM);
        limit = fcMat.col(2);
        limitT = fcMat.col(2);
        cens = fcMat.col(1);

        fc = fcMat.col(0);
        vec fcT(fc.size());
        fs = fc;
        vec yt(fc.size());
        for (int i = fc.size(); i--;) {
          int cur = ix_endpnt(i);
          limitT[i] = _powerD(limit[i], lambda(cur), yj(cur), low(cur), hi(cur));
          fc(i)     = _powerD(fc(i), lambda(cur), yj(cur), low(cur), hi(cur));
          yt(i)     = _powerD(mx.yM(i), lambda(cur), yj(cur), low(cur), hi(cur));
          fcT(i)    = handleF(propT(cur), fs(i), fc(i), false, true);
        }
        gc = vecares + vecbres % abs(fcT); //make sure gc > 0
        gc.elem( find( gc == 0.0) ).fill(1);
        gc.elem( find( gc < double_xmin) ).fill(double_xmin);
        gc.elem( find( gc > xmax) ).fill(xmax);

        switch (distribution) {
        case 1:
          DYF(mx.indioM)=0.5*(((yt-fc)/gc)%((yt-fc)/gc))+log(gc);
          break;
        case 2:
          DYF(mx.indioM)=-mx.yM%log(fc)+fc;
          break;
        case 3:
          DYF(mx.indioM)=-mx.yM%log(fc)-(1-mx.yM)%log(1-fc);
          break;
        }
        doCens(DYF, cens, limitT, fc, gc, mx.yM);

        Uc_y=sum(DYF,0).t();
        if (method==1) {
          deltu=Uc_y-U_y;
        }
        else {
          mat dphic=phiMc.cols(i)-mphi.mprior_phiM;
          Uc_phi=0.5*sum(dphic%(dphic*mphi.IGamma2_phi),1);
          deltu=Uc_y-U_y+Uc_phi-U_phi;
        }

        ind=find( deltu < -log(randu<vec>(mx.nM)) );
        phiM(ind,i)=phiMc(ind,i);
        U_y(ind)=Uc_y(ind);
        if (method>1) {
          U_phi(ind)=Uc_phi(ind);
        }
        ind = getObsIdx(ix_idM.rows(ind));
        fsave(ind)=fs(ind);
        if (method<3) {
          break;
        }
      }
  }
};


// closing for #ifndef __SAEM_CLASS_RCPP_HPP__
#endif

using namespace Rcpp;



t_calc_lhs saem_lhs = NULL;
t_update_inis saem_inis = NULL;


rx_solve* _rx = NULL;

RObject mat2NumMat(const mat &m) {
  RObject x = wrap( m.memptr() , m.memptr() + m.n_elem ) ;
  x.attr( "dim" ) = Dimension( m.n_rows, m.n_cols ) ;
  return x;
}

CharacterVector parNames;

mat user_function(const mat &_phi, const mat &_evt, const List &_opt) {
  // yp has all the observations in the dataset
  rx_solving_options_ind *ind;
  rx_solving_options *op = getSolvingOptions(_rx);
  vec _id = _evt.col(0);
  int _Nnlmixr2=(int)(_id.max()+1);
  SEXP paramUpdate = _opt["paramUpdate"];
  int *doParam = INTEGER(paramUpdate);
  int nPar = Rf_length(paramUpdate);
  // Fill in subject parameter information
  for (int _i = 0; _i < _Nnlmixr2; ++_i) {
    ind = getSolvingOptionsInd(_rx, _i);
    setIndSolve(ind, -1);
    int k=0;
    for (int _j = 0; _j < nPar; _j++){
      if (doParam[_j] == 1) {
        setIndParPtr(ind, _j, _phi(_i, k++));
      }
    }
  }
  resetRxBadSolve(_rx);
  par_solve(_rx); // Solve the complete system (possibly in parallel)
  int j=0;
  while (hasRxBadSolve(_rx) && j < _saemMaxOdeRecalc){
    _saemIncreaseTol=1;
    rxode2::atolRtolFactor_(_saemOdeRecalcFactor);
    resetRxBadSolve(_rx);
    par_solve(_rx);
    j++;
  }
  if (j != 0) {
    // Not thread safe
    rxode2::atolRtolFactor_(pow(_saemOdeRecalcFactor, -j));
  }
  mat g(getRxNobs2(_rx), 3); // nobs EXCLUDING EVID=2
  int elt=0;
  bool hasNan = false;
  for (int id = 0; id < _Nnlmixr2; ++id) {
    ind = getSolvingOptionsInd(_rx, id);
    iniSubjectE(getOpNeq(op), 1, ind, op, _rx, saem_inis);
    for (int j = 0; j < getIndNallTimes(ind); ++j) {
      setIndIdx(ind, j);
      int kk = getIndIx(ind, getIndIdx(ind));
      double curT = getTime(kk, ind);
      double *lhs = getIndLhs(ind);
      if (isDose(getIndEvid(ind, kk))) {
        // Need to calculate for advan sensitivities
        saem_lhs((int)id, curT,
                 getOpIndSolve(op, ind, j), lhs);
      } else if (getIndEvid(ind,kk) == 0) {
        saem_lhs((int)id, curT,
                 getOpIndSolve(op, ind, j), lhs);
        double cur = lhs[0];
        if (std::isnan(cur)) {
          cur = 1.0e99;
          hasNan = true;
        }
        g(elt, 0) = cur;
        if (hasRxCens(_rx)) {
          g(elt, 1) = getIndCens(ind, kk);
        } else {
          g(elt, 1) = 0;
        }
        if (hasRxLimit(_rx)) {
          g(elt, 2) = getIndLimit(ind, kk);
        } else {
          g(elt, 2) = R_NegInf;
        }
        elt++;
      } // evid=2 does not need to be calculated
    }
  }
  if (getOpStiff(op) == 2) { // liblsoda
    // Order by the overall solve time
    // Should it be done every time? Every x times?
    sortIds(_rx, 0);
  }
  if (hasNan && !_warnAtolRtol) {
    RSprintf("NaN in prediction; Consider: relax atol & rtol; change initials; change seed; change structural model\n  warning only issued once per problem\n");
    _warnAtolRtol = true;
  }
  return g;
}

void setupRx(List &opt, SEXP evt, SEXP evtM) {
  RObject obj = opt[".rx"];
  List mv = _rxode2_rxModelVars_(obj);
  rxUpdateFuns(mv["trans"], &rxInner);
  parNames = mv[RxMv_params];

  if (!Rf_isNull(obj)){
    // Now need to get the largest item to setup the solving space
    RObject pars = opt[".pars"];
    List odeO = opt["rxControl"];
    // SEXP evt = x["evt"];
    // SEXP evtM = x["evtM"];
    int nEvt = INTEGER(Rf_getAttrib(evt, R_DimSymbol))[0];
    int nEvtM = INTEGER(Rf_getAttrib(evtM, R_DimSymbol))[0];
    SEXP ev;
    if (nEvt > nEvtM) {
      ev = evt;
    } else {
      ev = evtM;
    }
    if (Rf_isNull(pars)) {
      stop("params must be non-nil");
    }
    rxode2::rxSolve_(obj, odeO,
     		    R_NilValue,//const Nullable<CharacterVector> &specParams =
     		    R_NilValue,//const Nullable<List> &extraArgs =
     		    pars,//const RObject &params =
     		    ev,//const RObject &events =
     		    R_NilValue, // inits
     		    1);//const int setupOnly = 0
  } else {
    stop("cannot find rxode2 model");
  }
}

//[[Rcpp::export]]
SEXP saem_do_pred(SEXP in_phi, SEXP in_evt, SEXP in_opt) {
  List opt = List(in_opt);
  setupRx(opt, in_evt, in_evt);
  saem_lhs = rxInner.calc_lhs;
  saem_inis = rxInner.update_inis;
  _rx=getRxSolve_();
  mat phi = as<mat>(in_phi);
  mat evt = as<mat>(in_evt);
  mat gMat = user_function(phi, evt, opt);
  vec g = gMat.col(0);
  return wrap(g);
}


//[[Rcpp::export]]
SEXP saem_fit(SEXP xSEXP) {
  List x(xSEXP);
  List opt = x["opt"];
  setupRx(opt,x["evt"],x["evtM"]);

  // if (rxSingleSolve == NULL) rxSingleSolve = (rxSingleSolve_t) R_GetCCallable("rxode2","rxSingleSolve");
  saem_lhs = rxInner.calc_lhs;
  saem_inis = rxInner.update_inis;
  _rx=getRxSolve_();

  SAEM saem;
  saem.inits(x);
  saem.set_fn(user_function);
  saem.saem_fit();

  List out = List::create(
    Named("resMat") = saem.get_resMat(),
    Named("transMat") = saem.get_trans(),
    Named("mprior_phi") = saem.get_mprior_phi(),
    Named("mpost_phi") = saem.get_mpost_phi(),
    Named("Gamma2_phi1") = saem.get_Gamma2_phi1(),
    Named("Plambda") = saem.get_Plambda(),
    Named("Ha") = saem.get_Ha(),
    Named("sig2") = saem.get_sig2(),
    Named("eta") = saem.get_eta(),
    Named("par_hist") = saem.get_par_hist(),
    Named("res_info") = saem.get_resInfo()
  );
  out.attr("saem.cfg") = x;
  out.attr("class") = "saemFit";
  return out;
}
