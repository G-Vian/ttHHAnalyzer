#include "Parameters.h"

// matches a jet to the |eta| bin (jet outside acceptance are matched to highest
// |eta| bin or flagged as 'outside')
int MEM::eta_to_bin(const double& eta, bool mark_acceptance) {
  double ae = std::abs(eta);
  if (ae < 1.0) {
    return 0;
  } else if (ae < 2.5) {
    return 1;
  } else {
    if (mark_acceptance)
      return -1;
    else
      return eta_to_bin(2.49);
  }
}

double MEM::deltaR(const LV& a, const LV& b) {
  return sqrt(TMath::Power(a.Eta() - b.Eta(), 2) +
              TMath::Power(TMath::ACos(TMath::Cos(a.Phi() - b.Phi())), 2));
}

bool MEM::descending(double a, double b) { return (a >= b); }

vector<std::size_t> MEM::get_sorted_indexes(const std::vector<double>& in,
                                            const double& cut) {
  std::vector<std::size_t> out;
  double max{0.};
  for (std::size_t id = 0; id < in.size(); ++id) {
    double in_id = in[id];
    if (in_id >= max) max = in_id;
  }
  if (max <= 0.) return out;
  for (std::size_t id = 0; id < in.size(); ++id) {
    if (in[id] / max >= cut) out.push_back(id);
  }
  return out;
}

bool MEM::is_in(const std::vector<std::size_t>& v, const std::size_t& id) {
  if (v.size() < 1) return true;
  for (auto idx : v)
    if (idx == id) return true;
  return false;
}

bool MEM::isQuark(const MEM::TFType::TFType& t) {
  return (t == TFType::bReco || t == TFType::qReco || t == TFType::bLost ||
          t == TFType::qLost);
}
bool MEM::isNeutrino(const MEM::TFType::TFType& t) {
  return (t == TFType::MET);
}
bool MEM::isLepton(const MEM::TFType::TFType& t) {
  return (t == TFType::elReco || t == TFType::muReco);
}

double MEM::Chi2(const double& x, const double& m, const double& s) {
  return s > 0. ? (x - m) * (x - m) / s / s : 99.;
}

double MEM::Chi2Corr(const double& x, const double& y, const double& sx,
                     const double& sy, const double& rho) {
  return 1. / (1 - rho * rho) *
         (Chi2(x, 0., sx) + Chi2(y, 0., sy) - 2 * rho * x * y / sx / sy);
}

/////////////////////////////////
//   y    := observables
//   x    := gen level quantities
//   type := decides the TF
/////////////////////////////////
double MEM::transfer_function(double* y, double* x, const TFType::TFType& type,
                              int& out_of_range, const double& cutoff,
                              const int& debug) {
  // return value
  double w{1.};

  // temporary values;
  double E, H;
  double m1, s1, m2, s2, f, rho, c1, c2;

  // parameters
  const double* par;

  switch (type) {
    case TFType::bReco:
      // x[0] = parton energy ;
      // x[1] = parton eta;
      // y[0] = jet energy;
      E = x[0];
      H = x[1];
      par = TF_B_param[eta_to_bin(H)];

      f = par[10];
      m1 = par[0] + par[1] * E;
      m2 = par[5] + par[6] * E;
      s1 = E * TMath::Sqrt(par[2] * par[2] + par[3] * par[3] / E +
                           par[4] * par[4] / E / E);
      s2 = E * TMath::Sqrt(par[7] * par[7] + par[8] * par[8] / E +
                           par[9] * par[9] / E / E);
      c1 = Chi2(y[0], m1, s1);
      c2 = Chi2(y[0], m2, s2);
      if (c1 > cutoff && c2 > cutoff) ++out_of_range;
      w *= (1. / sqrt(2 * PI) * (f / s1 * TMath::Exp(-0.5 * c1) +
                                 (1 - f) / s2 * TMath::Exp(-0.5 * c2)));
#ifdef DEBUG_MODE
      if (debug & DebugVerbosity::integration)
        cout << "\t\ttransfer_function: Evaluate W(" << y[0] << " | E=" << E
             << ", y=" << H << ", TFType::bReco) = " << w << endl;
#endif
      break;

    case TFType::qReco:
      // x[0] = parton energy ;
      // x[1] = parton eta;
      // y[0] = jet energy;
      E = x[0];
      H = x[1];
      par = TF_Q_param[eta_to_bin(H)];
      m1 = par[0] + par[1] * E;
      s1 = E * TMath::Sqrt(par[2] * par[2] + par[3] * par[3] / E +
                           par[4] * par[4] / E / E);
      c1 = Chi2(y[0], m1, s1);
      if (c1 > cutoff) ++out_of_range;

      w *= (1. / sqrt(2 * PI) / s1 * TMath::Exp(-0.5 * c1));
#ifdef DEBUG_MODE
      if (debug & DebugVerbosity::integration)
        cout << "\t\ttransfer_function: Evaluate W(" << y[0] << " | E=" << E
             << ", y=" << H << ", TFType::qReco) = " << w << endl;
#endif
      break;

    case TFType::MET:
      // x[0] = sum nu_x ; x[1] = sum nu_y
      // y[0] = MET_x    ; y[1] = MET_y

      par = TF_MET_param;
      s1 = par[0];
      s2 = par[1];
      rho = par[2];
      c1 = Chi2Corr(y[0] - x[0], y[1] - x[1], s1, s2, rho);

      if (c1 / 2 > cutoff) ++out_of_range;

      w *= 1. / (2 * PI) / s1 / s2 / sqrt(1. - rho * rho) *
           TMath::Exp(-0.5 * c1);
#ifdef DEBUG_MODE
      if (debug & DebugVerbosity::integration)
        cout << "\t\ttransfer_function: Evaluate W(" << y[0] << "-" << x[0]
             << " , " << y[1] << "-" << x[1] << ", TFType::MET) = " << w
             << endl;
#endif
      break;

    case TFType::Recoil:
      // Sudakov factor
      // x[0] = pT
      // y[0] = rhoT if extra_jets==0, else  par[2]+1GeV

      par = TF_RECOIL_param;
      m1 = par[0];
      s1 = par[1];
      if (y[0] < par[2])
        //possibly missing 1/x[0] factor as required for log-normal?
        w *= TMath::Gaus(log(x[0]), m1, s1, 1);
      else
        w *= 1.;
#ifdef DEBUG_MODE
      if (debug & DebugVerbosity::integration)
        cout << "\t\ttransfer_function: Evaluate W( log(" << x[0]
             << "); TFType::Recoil) = " << w << endl;
#endif
      break;

    case TFType::bLost:
    case TFType::qLost:
      // x[0]     = parton energy ;
      // x[1]     = parton eta;
      // y[0]     = jet energy
      // par: [0]-> eta acceptance, [1]-> pT cut, [2]-> E max, [3]->acceptance
      // (cos*phi)

      if (TMath::Abs(x[1]) > TF_ACC_param[0]) {
        w *= 1.;
#ifdef DEBUG_MODE
        if (debug & DebugVerbosity::integration)
          cout << "\t\ttransfer_function: Evaluate W(" << x[0] << ", " << x[1]
               << ", TFType::qLost) = " << w << endl;
#endif
      } else {
        // x[0]     = parton energy ;
        // x[1]     = parton eta
        // y[0]     = 0.
        E = x[0];
        H = x[1];
        par = TF_Q_param[eta_to_bin(H)];
        double mean_e = (par[0] + par[1] * E);
        double sigma_e = E * TMath::Sqrt(par[2] * par[2] + par[3] * par[3] / E +
                                         par[4] * par[4] / E / E);
        double sign = (TF_ACC_param[1] * TMath::CosH(H) >= mean_e) ? +1. : -1.;
        c1 = Chi2(TF_ACC_param[1] * TMath::CosH(H), mean_e, sigma_e);
        if (c1 > cutoff) ++out_of_range;

        w *= 0.5 * (TMath::Erf(sqrt(c1 / 2.) * sign) + 1);
#ifdef DEBUG_MODE
        if (debug & DebugVerbosity::integration)
          cout << "\t\ttransfer_function: Evaluate W(" << TF_ACC_param[1]
               << " | " << E << ", " << H << ", TFType::qLost) = " << w << endl;
#endif
      }
      break;
    case TFType::Unknown:
      w *= 1.;
#ifdef DEBUG_MODE
      if (debug & DebugVerbosity::integration)
        cout << "\t\ttransfer_function: Evaluate W = 1 " << endl;
#endif
      break;
    default:
      break;
  }

  return w;
}

double MEM::transfer_function_smear(double* x, double* par) {
  TFType::TFType type = static_cast<TFType::TFType>(static_cast<int>(par[2]));

  if (type == TFType::bReco || type == TFType::qReco) {
    double yy[1] = {x[0]};
    double xx[2] = {par[0], par[1]};
    int out_of_range{0};
    double cutoff{6.6};
    int debug{0};
    return transfer_function(yy, xx, type, out_of_range, cutoff, debug);
  } else if (type == TFType::MET) {
    double yy[2] = {x[0], x[1]};
    double xx[2] = {par[0], par[1]};
    int out_of_range{0};
    double cutoff{6.6};
    int debug{0};
    return transfer_function(yy, xx, type, out_of_range, cutoff, debug);
  } else { /* ... */
  }

  return 1.0;
}

inline double fast_exp (double p)
{
  //Return a small value to prevent the TF product becoming 0 in case
  //of a very mis-reconstructed jet
  if (p < -50.0) {
    return 2E-22;
  }
  return exp(p);
}

inline double pow2(float y) {
    return y*y;
}

//Extracted using TF.MakeCDF().Print()
inline double double_gaussian_cdf(double x, double x_gen, const std::array<double, 12>& pars) {
  const auto p0 = x_gen;
//  const auto p1 = pars[1];
// parameters are shifted by one from double_gaussian, since CDF has no p1 (normalization)
  const auto p2 = pars[1];
  const auto p3 = pars[2];
  const auto p4 = pars[3];
  const auto p5 = pars[4];
  const auto p6 = pars[5];
  const auto p7 = pars[6];
  const auto p8 = pars[7];
  const auto p9 = pars[8];
  const auto p10 = pars[9];
  const auto p11 = pars[10];
  const auto s1 = sqrt(p4*p4+p0*p5*p5+p0*p0*p6*p6);
  const auto s2 = sqrt(p9*p9+p0*p10*p10+p0*p0*p11*p11);
  //cout << "mu1=" << p2+p3*p0 << " s1=" << s1 << " mu2=" << p7+p8*p0 << " s2=" << s1+s2 << endl;
  //CDF of Gaussian centered on x (jet pt), with stddev s1, evaluated at p2+p3*p0 (quark pt)
  const auto w = 0.7*ROOT::Math::normal_cdf(x - p2+p3*p0, s1) + (1-0.7)*ROOT::Math::normal_cdf(x - p7+p8*p0, s1+s2);
  return w;
}

//Extracted using TF.Print()
inline double double_gaussian(double x, double x_gen, const std::array<double, 12>& pars) {
  const auto p0 = x_gen;
  const auto p1 = pars[1];
  const auto p2 = pars[2];
  const auto p3 = pars[3];
  const auto p4 = pars[4];
  const auto p5 = pars[5];
  const auto p6 = pars[6];
  const auto p7 = pars[7];
  const auto p8 = pars[8];
  const auto p9 = pars[9];
  const auto p10 = pars[10];
  const auto p11 = pars[11];
  
  const auto s1 = sqrt(p4*p4+p0*p5*p5+p0*p0*p6*p6);
  const auto s2 = sqrt(p9*p9+p0*p10*p10+p0*p0*p11*p11);
  return p1*(0.7*fast_exp(-0.5*pow2((x-((p2+p3*p0)))/s1)) + (1-0.7)*fast_exp(-0.5*pow2((x-(p7+p8*p0))/(s1+s2))));
}

double MEM::transfer_function_reco(double pt, const vector<double>& pt_variations, const std::array<double, MEM::Object::NUM_TF_PARS>& pars, const double* x, vector<double>& obj_variations, const bool eval_variations) {
  float w = 0;
  float w_var = 0;
  
  w = double_gaussian(pt, x[0], pars);
  
  if (eval_variations) {
    obj_variations.reserve(pt_variations.size());
    for (const auto pt_var : pt_variations) {
      w_var = double_gaussian(pt * pt_var, x[0], pars);
      obj_variations.push_back(w_var);
    }
  }
  return w;

}

double MEM::transfer_function_lost(double ptcut, const std::array<double, MEM::Object::NUM_TF_PARS>& pars, const double* x) {
  return double_gaussian_cdf(ptcut, x[0], pars);
}

// Evaluates a transfer function attached to an object
// the tf is a TF1* in a TFType->TF1* map in the object
// the tf is a function of the reconstructed Energy (pt) through tf->Eval(Erec)
// The generator energy is set via tf->SetParameter(0, Egen)
// type - the hypothesis for the object to be tested, e.g. qReco (reconstructed
// light quark)
double MEM::transfer_function2(MEM::Object* obj,  // either MEM::Object or TF1
                               const double* x, const TFType::TFType& type,
                               vector<double>& obj_variations,
                               const bool eval_variations) {
  double w = 1.0;

  // x[0] -> Egen
  switch (type) {
    // W(Erec | Egen) = TF1(Erec, par0:Egen)
    case TFType::qReco:
      w = transfer_function_reco(
        obj->p4().Pt(),
        obj->p4_variations,
        obj->tf_l_parameters,
        x,
        obj_variations, eval_variations
      );
      break;
    case TFType::bReco:
      w = transfer_function_reco(
        obj->p4().Pt(),
        obj->p4_variations,
        obj->tf_b_parameters,
        x,
        obj_variations, eval_variations
      );
      break;
    default:
      throw std::runtime_error("deprecated");
      break;
  }

  return w;
}

/////////////////////////////////
//   y     := observables
//   type  := decides the TF
//   alpha := CL (e.g. 0.95, 0.98, ...)
//   obj   := optionally supply the particle, needed for external TF (obj==null
//   -> internal TF)
/////////////////////////////////
pair<double, double> MEM::get_support(double* y, TFType::TFType type,
                                      double alpha, int debug,
                                      MEM::Object* obj, bool eval_compiled) {
  double deriv = 0.0;
  vector<double> variations;
  
  if (type == TFType::TFType::MET) {
    double alpha_n = TMath::Abs(alpha);
    int sign = alpha > 0 ? 1 : 0;

    // the MET px and py
    double Px = y[0];
    double Py = y[1];

    // return values
    double xLowPhi = -TMath::Pi();
    double xHighPhi = +TMath::Pi();

    double phiStep = 0.04;

    // phi of MET vector
    double Phi =
        Py > 0 ? TMath::ACos(Px / sqrt(Px * Px + Py * Py))
               : 2 * TMath::Pi() - TMath::ACos(Px / sqrt(Px * Px + Py * Py));
    DVLOG(2) << "MET phi at " << Phi;

    // elements of the MET cov matrix
    double Vx = TF_MET_param[0] * TF_MET_param[0];
    double Vy = TF_MET_param[1] * TF_MET_param[1];
    double rho = TF_MET_param[2];

    // chi2 cut to find the CL
    double chi2Cut = TMath::ChisquareQuantile(alpha_n, 2);

    // MET TF at zero
    double tfAtZero = Chi2Corr(Px, Py, sqrt(Vx), sqrt(Vy), rho);

    // nothing to do...
    if (tfAtZero <= chi2Cut) {
      if (VLOG_IS_ON(2))
        DVLOG(2) << "(0,0) is inside the 2-sigma CL => integrate over "
                 << "-pi/+pi";
    }

    // search for boundaries
    else {
      DVLOG(2) << "(0,0) is outside the 2-sigma CL => find phi-window with "
                  "interpolation...";

      for (int dir = 0; dir < 2; ++dir) {
        if (dir != sign) continue;
        DVLOG(2) << "Doing scan along " << (dir ? "+" : "-") << " direction";

        bool stopPhiScan = false;
        for (std::size_t step = 0;
             step <= (std::size_t)(TMath::Pi() / phiStep) && !stopPhiScan;
             ++step) {
          double phi = Phi + double(2. * dir - 1) * phiStep * step;
          // if(phi<0.) phi += 2*TMath::Pi();
          // else if(phi>2*TMath::Pi())  phi -= 2*TMath::Pi();
          if (debug & DebugVerbosity::init_more)
            cout << "\tScan phi=" << phi << endl;
          double sin = TMath::Sin(phi);
          double cos = TMath::Cos(phi);
          bool crossing = false;
          // bool exceeded = false;
          // bool alreadyInTheBox = PxMax*PxMin<=0. && PyMax*PyMin<=0.;

          double p_step = 2.;
          for (std::size_t stepP = 0;
               stepP < 200 && !crossing &&
               /*!exceeded && !crossing &&*/ !stopPhiScan;
               ++stepP) {
            double Px_P = stepP * p_step * cos;
            double Py_P = stepP * p_step * sin;
            // if( alreadyInTheBox && (Px_P>PxMax || Px_P<PxMin || Py_P>PyMax ||
            // Py_P<PyMin)){
            // exceeded = true;
            // if( debug&DebugVerbosity::init_more) cout << "\tWas in box, and
            // got out at P=" << stepP*p_step << endl;
            // continue;
            //}
            // else if( !alreadyInTheBox && (Px_P>PxMax || Px_P<PxMin ||
            // Py_P>PyMax || Py_P<PyMin)){
            // if( debug&DebugVerbosity::init_more) cout << "\tWas not in the
            // box, and I am still out at P=" << stepP*5 << endl;
            // continue;
            //}

            if (Chi2Corr(Px_P - Px, Py_P - Py, sqrt(Vx), sqrt(Vy), rho) <
                chi2Cut) {
              crossing = true;
              if (debug & DebugVerbosity::init_more)
                cout << "\tWas not in the box, and found crossing at (" << Px_P
                     << "," << Py_P << ")" << endl;
            }
          }  // end loop over |P|

          if (!crossing) {
            if (debug & DebugVerbosity::init_more)
              cout << "\tNo crossing at " << phi << " => stop phi scan" << endl;
            if (dir == 0) xLowPhi = phi + 0.5 * phiStep;
            if (dir == 1) xHighPhi = phi - 0.5 * phiStep;
            stopPhiScan = true;
          }
        }
      }

      // xLowPhi  = -TMath::ACos(TMath::Cos( Phi - xLowPhi ));
      // xHighPhi = +TMath::ACos(TMath::Cos( Phi - xHighPhi));
      xLowPhi -= Phi;
      xHighPhi -= Phi;
    }

    return make_pair(xLowPhi, xHighPhi);
  }  // TFType == MET

  // the reconstructed values
  double e_rec = y[0];
  double eta_rec = y[1];

  // start with reconstructed value
  double e_L{e_rec};
  double e_H{e_rec};

  // granularity
  double step_size{2.5};

  double tot{1.};
  int accept{0};
  double cutoff{99.};
  
  while (tot > (1 - alpha) / 2 && e_L > 0.) {
    tot = 0.;
    for (size_t i = 0; i < 500.; ++i) {
      double gen[2] = {e_L, eta_rec};
      double rec[1] = {e_rec + i * step_size};
      if (obj == nullptr) {
        tot += transfer_function(rec, gen, type, accept, cutoff, debug) *
               step_size;
      } else {  // use external TF
        std::vector<double> empty_vec;
        tot +=
          transfer_function2(obj, gen, type, empty_vec, false) *
            step_size;
      }
      if (tot > (1 - alpha) / 2) break;
    }
    e_L -= step_size;
  }
  if (e_L < 0.) e_L = 0.;

  tot = 1.;
  while (tot > (1 - alpha) / 2) {
    tot = 0.;
    for (size_t i = 0; i < 500.; ++i) {
      double gen[2] = {e_H, eta_rec};
      double rec[1] = {e_rec - i * step_size};
      if (rec[0] < 0.) continue;
      if (obj == nullptr) {
        tot += transfer_function(rec, gen, type, accept, cutoff, debug) *
               step_size;
      } else {  // use external TF
        std::vector<double> empty_vec;
        tot +=
          transfer_function2(obj, gen, type, empty_vec, false) *
            step_size;
      }
      if (tot > (1 - alpha) / 2) break;
    }
    e_H += step_size;
  }
#ifdef DEBUG_MODE
  if (debug & DebugVerbosity::init_more)
    cout << "MEM::get_support: E(reco) = " << e_rec << " ==> range at " << alpha
         << " CL is [" << e_L << ", " << e_H << "] (stepping every "
         << step_size << " GeV)" << endl;
#endif
  e_L = TMath::Max(e_L, type == TFType::bReco ? MB : MQ);
  return make_pair(e_L, e_H);
}

MEM::PS::PS(size_t d) { dim = d; }

MEM::PS::~PS() {}

MEM::PSMap::const_iterator MEM::PS::begin() const { return val.begin(); }

MEM::PSMap::const_iterator MEM::PS::end() const { return val.end(); }

LV MEM::PS::lv(const MEM::PSPart::PSPart& p) const {
  return val.find(p) != val.end() ? (val.find(p)->second).lv : LV();
}

int MEM::PS::charge(const MEM::PSPart::PSPart& p) const {
  return val.find(p) != val.end() ? (val.find(p)->second).charge : 0;
}

MEM::TFType::TFType MEM::PS::type(const MEM::PSPart::PSPart& p) const {
  return val.find(p) != val.end() ? (val.find(p)->second).type
                                  : TFType::Unknown;
}

void MEM::PS::set(const MEM::PSPart::PSPart& a, const MEM::GenPart& b) {
  val[a] = b;
}

void MEM::PS::print(ostream& os) const {
  os << "Content of this PS: dim(PS)=" << dim << "..." << endl;
  for (auto p = val.begin(); p != val.end(); ++p) {
    const auto vec = p->second.lv;
    os << "\tPS[" << static_cast<size_t>(p->first) << "] : type("
       << static_cast<size_t>(p->second.type) << "), (pT,h,phi,M)=(" << vec.Pt()
       << ", " << vec.Eta() << ", " << vec.Phi() << ", " << vec.M()
       << "), (px,py,pz,E)=(" << vec.Px() << ", " << vec.Py() << ", "
       << vec.Pz() << ", " << vec.E() << ")" << endl;
  }
}

MEM::Object::Object(const LV& lv, const MEM::ObjectType::ObjectType& ty,
                    DistributionType::DistributionType dtype,
                    DistributionType::DistributionType dtype_bkp) {
  p = lv;
  t = ty;
  dt = dtype;
  dt_bkp = dtype_bkp;
  for (int i=0; i<NUM_TF_PARS; i++) {
    tf_l_parameters[i] = 0;
    tf_b_parameters[i] = 0;
  }
}

MEM::Object::Object() {
  p = LV(1e-06, 0., 0., 1e-06);
  t = ObjectType::Unknown;
}

MEM::Object::~Object() {
}

LV MEM::Object::p4() const { return p; }

void MEM::Object::setp4(const LV& lv) { p = lv; }

MEM::ObjectType::ObjectType MEM::Object::type() const { return t; }

MEM::DistributionType::DistributionType MEM::Object::distribution_type() const {
  return dt;
}

MEM::DistributionType::DistributionType MEM::Object::distribution_type_bkp()
    const {
  return dt_bkp;
}

double MEM::Object::getObs(const MEM::Observable::Observable& name) const {
  return (obs.find(name) != obs.end() ? obs.find(name)->second : 0.);
}

TF1* MEM::Object::getTransferFunction(MEM::TFType::TFType name) const {
  const auto tf_it = transfer_funcs.find(name);
  if (tf_it != transfer_funcs.end()) {
    return tf_it->second;
  } else {
    LOG(ERROR) << "Could not get transfer function " << name;
    return (TF1*)nullptr;
  }
}

bool MEM::Object::isSet(const MEM::Observable::Observable& name) const {
  return obs.find(name) != obs.end();
}

void MEM::Object::addObs(const MEM::Observable::Observable& name,
                         const double& val) {
  obs.insert(make_pair(name, val));
}

void MEM::Object::addTransferFunction(const MEM::TFType::TFType& name,
                                      TF1* val) {
  transfer_funcs.insert(make_pair(name, val));
  
  if (name == TFType::qReco) {
    const auto tf = this->getTransferFunction(TFType::qReco);
    if (tf->GetNpar() != NUM_TF_PARS) {
      cerr << "Expected " << NUM_TF_PARS << " but got " << tf->GetNpar() << " parameters for TF qReco" << endl;
      throw std::runtime_error("transfer function");
    }
    tf->GetParameters(this->tf_l_parameters.data());
  }
  else if (name == TFType::bReco) {
    const auto tf = this->getTransferFunction(TFType::bReco);
    if (tf->GetNpar() != NUM_TF_PARS) {
      cerr << "Expected " << NUM_TF_PARS << " but got " << tf->GetNpar() << " parameters for TF bReco" << endl;
      throw std::runtime_error("transfer function");
    }
    tf->GetParameters(this->tf_b_parameters.data());
  }
}

std::size_t MEM::Object::getNumTransferFunctions() const {
  return transfer_funcs.size();
}

void MEM::Object::print(ostream& os) const {
  os << "\tType: " << static_cast<int>(t) << ", p=(Pt, Eta, Phi, M)=(" << p.Pt()
     << ", " << p.Eta() << ", " << p.Phi() << ", " << p.M() << ")";
  for (auto& kv : obs) {
    os << " " << kv.first << "->" << kv.second;
  }
  for (auto& kv : transfer_funcs) {
    os << " tf " << kv.first << "->" << ((TF1*)kv.second);
  }
}

MEM::MEMConfig::MEMConfig(int nmc, double ab, double re, int ts, int nit, int ic, int pi,
                          double s, double e, std::string pdf, double jCL,
                          double bCL, double mCL, int tfsupp, double tfoff,
                          bool tfrange, int hpf, MEM::TFMethod::TFMethod method,
                          int minim, int permprun, double permrel, int prefit,
                          int _max_permutations) {
  n_max_calls = nmc;
  abs = ab;
  rel = re;
  two_stage = ts;
  niters = nit;
  int_code = ic;
  perm_int = pi;
  sqrts = s;
  emax = e;
  pdfset = pdf;
  is_default = true;
  j_range_CL = jCL;
  b_range_CL = bCL;
  m_range_CL = mCL;
  tf_suppress = tfsupp;
  tf_offscale = tfoff;
  tf_in_range = tfrange;
  highpt_first = hpf;
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 2; ++j) {
      for (int k = 0; k < 6; ++k) {
        calls[i][j][k] = 2000;
      }
    }
  }
  perm_pruning = {};
  transfer_function_method = method;
  do_minimize = minim;
  do_perm_filtering = permprun;
  perm_filtering_rel = permrel;
  do_prefit = prefit;
  max_permutations = _max_permutations;
  save_permutations = false;
  
  integrator_type = IntegratorType::IntegratorType::Vegas;
  cuba_cores = 0;
  
  num_jet_variations = 0;
  interpolate_pdf = false;
  eval_compiled_tf = true;
}

void MEM::MEMConfig::defaultCfg(float nCallsMultiplier) {
  // FinalState::LH
  calls[static_cast<std::size_t>(FinalState::FinalState::LH)]
       [static_cast<std::size_t>(Hypothesis::Hypothesis::TTH)]
       [static_cast<std::size_t>(Assumption::Assumption::ZeroQuarkLost)] = 2000;
  calls[static_cast<std::size_t>(FinalState::FinalState::LH)]
       [static_cast<std::size_t>(Hypothesis::Hypothesis::TTBB)]
       [static_cast<std::size_t>(Assumption::Assumption::ZeroQuarkLost)] = 2000;
  calls[static_cast<std::size_t>(FinalState::FinalState::LH)]
       [static_cast<std::size_t>(Hypothesis::Hypothesis::TTH)]
       [static_cast<std::size_t>(Assumption::Assumption::OneQuarkLost)] = 4000;
  calls[static_cast<std::size_t>(FinalState::FinalState::LH)]
       [static_cast<std::size_t>(Hypothesis::Hypothesis::TTBB)]
       [static_cast<std::size_t>(Assumption::Assumption::OneQuarkLost)] = 4000;
  calls[static_cast<std::size_t>(FinalState::FinalState::LH)]
       [static_cast<std::size_t>(Hypothesis::Hypothesis::TTH)]
       [static_cast<std::size_t>(Assumption::Assumption::TwoQuarkLost)] = 15000;
  calls[static_cast<std::size_t>(FinalState::FinalState::LH)]
       [static_cast<std::size_t>(Hypothesis::Hypothesis::TTBB)]
       [static_cast<std::size_t>(Assumption::Assumption::TwoQuarkLost)] = 15000;

  // FinalState::LL
  calls[static_cast<std::size_t>(FinalState::FinalState::LL)]
       [static_cast<std::size_t>(Hypothesis::Hypothesis::TTH)]
       [static_cast<std::size_t>(Assumption::Assumption::ZeroQuarkLost)] =
           10000;
  calls[static_cast<std::size_t>(FinalState::FinalState::LL)]
       [static_cast<std::size_t>(Hypothesis::Hypothesis::TTBB)]
       [static_cast<std::size_t>(Assumption::Assumption::ZeroQuarkLost)] =
           10000;
  calls[static_cast<std::size_t>(FinalState::FinalState::LL)]
       [static_cast<std::size_t>(Hypothesis::Hypothesis::TTH)]
       [static_cast<std::size_t>(Assumption::Assumption::OneQuarkLost)] = 20000;
  calls[static_cast<std::size_t>(FinalState::FinalState::LL)]
       [static_cast<std::size_t>(Hypothesis::Hypothesis::TTBB)]
       [static_cast<std::size_t>(Assumption::Assumption::OneQuarkLost)] = 20000;

  // FinalState::HH
  calls[static_cast<std::size_t>(FinalState::FinalState::HH)]
       [static_cast<std::size_t>(Hypothesis::Hypothesis::TTH)]
       [static_cast<std::size_t>(Assumption::Assumption::ZeroQuarkLost)] = 1500;
  calls[static_cast<std::size_t>(FinalState::FinalState::HH)]
       [static_cast<std::size_t>(Hypothesis::Hypothesis::TTBB)]
       [static_cast<std::size_t>(Assumption::Assumption::ZeroQuarkLost)] = 1500;
  calls[static_cast<std::size_t>(FinalState::FinalState::HH)]
       [static_cast<std::size_t>(Hypothesis::Hypothesis::TTH)]
       [static_cast<std::size_t>(Assumption::Assumption::OneQuarkLost)] = 4000;
  calls[static_cast<std::size_t>(FinalState::FinalState::HH)]
       [static_cast<std::size_t>(Hypothesis::Hypothesis::TTBB)]
       [static_cast<std::size_t>(Assumption::Assumption::OneQuarkLost)] = 4000;
  calls[static_cast<std::size_t>(FinalState::FinalState::HH)]
       [static_cast<std::size_t>(Hypothesis::Hypothesis::TTH)]
       [static_cast<std::size_t>(Assumption::Assumption::TwoQuarkLost)] = 10000;
  calls[static_cast<std::size_t>(FinalState::FinalState::HH)]
       [static_cast<std::size_t>(Hypothesis::Hypothesis::TTBB)]
       [static_cast<std::size_t>(Assumption::Assumption::TwoQuarkLost)] = 10000;
  calls[static_cast<std::size_t>(FinalState::FinalState::HH)]
       [static_cast<std::size_t>(Hypothesis::Hypothesis::TTH)]
       [static_cast<std::size_t>(Assumption::Assumption::ThreeQuarkLost)] =
           15000;
  calls[static_cast<std::size_t>(FinalState::FinalState::HH)]
       [static_cast<std::size_t>(Hypothesis::Hypothesis::TTBB)]
       [static_cast<std::size_t>(Assumption::Assumption::ThreeQuarkLost)] =
           15000;
  calls[static_cast<std::size_t>(FinalState::FinalState::HH)]
       [static_cast<std::size_t>(Hypothesis::Hypothesis::TTH)]
       [static_cast<std::size_t>(Assumption::Assumption::FourQuarkLost)] =
           20000;  //~ tuned
  calls[static_cast<std::size_t>(FinalState::FinalState::HH)]
       [static_cast<std::size_t>(Hypothesis::Hypothesis::TTBB)]
       [static_cast<std::size_t>(Assumption::Assumption::FourQuarkLost)] =
           20000;
  calls[static_cast<std::size_t>(FinalState::FinalState::HH)]
       [static_cast<std::size_t>(Hypothesis::Hypothesis::TTH)]
       [static_cast<std::size_t>(Assumption::Assumption::FiveQuarkLost)] =
           25000;
  calls[static_cast<std::size_t>(FinalState::FinalState::HH)]
       [static_cast<std::size_t>(Hypothesis::Hypothesis::TTBB)]
       [static_cast<std::size_t>(Assumption::Assumption::FiveQuarkLost)] =
           25000;

  if (nCallsMultiplier != 1.0) {
    for (int i = 0; i < 4; i++) {
      for (int j = 0; j < 2; j++) {
        for (int k = 0; k < 6; k++) {
          calls[i][j][k] = nCallsMultiplier * calls[i][j][k];
        }
      }
    }
  }

  int_code = IntegrandType::IntegrandType::Constant |
             IntegrandType::IntegrandType::ScattAmpl |
             IntegrandType::IntegrandType::DecayAmpl |
             IntegrandType::IntegrandType::Jacobian |
             IntegrandType::IntegrandType::PDF |
             IntegrandType::IntegrandType::Transfer;
  //|IntegrandType::IntegrandType::Sudakov
  //|IntegrandType::IntegrandType::Recoil;

  perm_pruning = {
      Permutations::BTagged, Permutations::QUntagged,
      // Permutations::QQbarSymmetry,Permutations::BBbarSymmetry
      Permutations::QQbarBBbarSymmetry
      //,Permutations::HEPTopTagged
      //,Permutations::HiggsTagged
  };
}

void MEM::MEMConfig::setNCalls(FinalState::FinalState f,
                               Hypothesis::Hypothesis h,
                               Assumption::Assumption a, int n) {
  calls[static_cast<std::size_t>(f)][static_cast<std::size_t>(h)]
       [static_cast<std::size_t>(a)] = n;
}

int MEM::MEMConfig::getNCalls(FinalState::FinalState f,
                              Hypothesis::Hypothesis h,
                              Assumption::Assumption a) {
  return calls[static_cast<std::size_t>(f)][static_cast<std::size_t>(h)]
              [static_cast<std::size_t>(a)];
}

/*
int MEM::getEtaBin(double eta) {
   double ae = std::abs(eta);
   if (ae < 1.0) {
       return 0;
   } else if (ae < 2.5) {
       return 1;
   } else {
       return 0;
   }
}
*/

void MEM::MEMConfig::set_tf_global(TFType::TFType type, int etabin, TF1* tf) {
  VLOG(1) << "Adding global TF type=" << type << " etabin=" << etabin << " tf=" << tf;
  
  std::array<double, Object::NUM_TF_PARS> pars;
  if (tf->GetNpar() != Object::NUM_TF_PARS - 1) {
    cerr << "Expected " << Object::NUM_TF_PARS - 1 << " but got " << tf->GetNpar() << " parameters for TF qReco" << endl;
    throw std::runtime_error("transfer function");
  }
  tf->GetParameters(pars.data());
  
  tf_map[std::make_pair(type, etabin)] = pars;
}

void MEM::MEMConfig::add_distribution_global(
    DistributionType::DistributionType type, TH3D* tf) {
  btag_pdfs.insert(std::make_pair(type, tf));
}

//DS hardcode permutations to save ~400 sec in 9j category
vector<vector<int>> MEM::get_permutations(std::size_t nb, std::size_t nq, std::vector<std::size_t>& lost){
  
  vector<vector<int>> perms; //return value
  size_t nlost = lost.size();

  if(nq==4 && nb==4){ //8j,4b
    if(nlost==0){ //4w2h2t
      vector<vector<int>> vec = {{4,5,0,6,7,1,2,3},
				 {4,5,0,6,7,2,1,3},
				 {4,5,0,6,7,3,1,2},
				 {4,5,1,6,7,0,2,3},
				 {4,5,1,6,7,2,0,3},
				 {4,5,1,6,7,3,0,2},
				 {4,5,2,6,7,0,1,3},
				 {4,5,2,6,7,1,0,3},
				 {4,5,2,6,7,3,0,1},
				 {4,5,3,6,7,0,1,2},
				 {4,5,3,6,7,1,0,2},
				 {4,5,3,6,7,2,0,1},
				 {4,6,0,5,7,1,2,3},
				 {4,6,0,5,7,2,1,3},
				 {4,6,0,5,7,3,1,2},
				 {4,6,1,5,7,0,2,3},
				 {4,6,1,5,7,2,0,3},
				 {4,6,1,5,7,3,0,2},
				 {4,6,2,5,7,0,1,3},
				 {4,6,2,5,7,1,0,3},
				 {4,6,2,5,7,3,0,1},
				 {4,6,3,5,7,0,1,2},
				 {4,6,3,5,7,1,0,2},
				 {4,6,3,5,7,2,0,1},
				 {4,7,0,5,6,1,2,3},
				 {4,7,0,5,6,2,1,3},
				 {4,7,0,5,6,3,1,2},
				 {4,7,1,5,6,0,2,3},
				 {4,7,1,5,6,2,0,3},
				 {4,7,1,5,6,3,0,2},
				 {4,7,2,5,6,0,1,3},
				 {4,7,2,5,6,1,0,3},
				 {4,7,2,5,6,3,0,1},
				 {4,7,3,5,6,0,1,2},
				 {4,7,3,5,6,1,0,2},
				 {4,7,3,5,6,2,0,1},
				 {5,6,0,4,7,1,2,3},
				 {5,6,0,4,7,2,1,3},
				 {5,6,0,4,7,3,1,2},
				 {5,6,1,4,7,0,2,3},
				 {5,6,1,4,7,2,0,3},
				 {5,6,1,4,7,3,0,2},
				 {5,6,2,4,7,0,1,3},
				 {5,6,2,4,7,1,0,3},
				 {5,6,2,4,7,3,0,1},
				 {5,6,3,4,7,0,1,2},
				 {5,6,3,4,7,1,0,2},
				 {5,6,3,4,7,2,0,1},
				 {5,7,0,4,6,1,2,3},
				 {5,7,0,4,6,2,1,3},
				 {5,7,0,4,6,3,1,2},
				 {5,7,1,4,6,0,2,3},
				 {5,7,1,4,6,2,0,3},
				 {5,7,1,4,6,3,0,2},
				 {5,7,2,4,6,0,1,3},
				 {5,7,2,4,6,1,0,3},
				 {5,7,2,4,6,3,0,1},
				 {5,7,3,4,6,0,1,2},
				 {5,7,3,4,6,1,0,2},
				 {5,7,3,4,6,2,0,1},
				 {6,7,0,4,5,1,2,3},
				 {6,7,0,4,5,2,1,3},
				 {6,7,0,4,5,3,1,2},
				 {6,7,1,4,5,0,2,3},
				 {6,7,1,4,5,2,0,3},
				 {6,7,1,4,5,3,0,2},
				 {6,7,2,4,5,0,1,3},
				 {6,7,2,4,5,1,0,3},
				 {6,7,2,4,5,3,0,1},
				 {6,7,3,4,5,0,1,2},
				 {6,7,3,4,5,1,0,2},
				 {6,7,3,4,5,2,0,1}};
      perms = vec;
    }
    else if(nlost==1 && lost[0]==1){ //3w2h2t - qbar1
      vector<vector<int>> vec = {{4,-2,0,6,7,1,2,3},
				 {4,-2,0,6,7,2,1,3},
				 {4,-2,0,6,7,3,1,2},
				 {4,-2,1,6,7,0,2,3},
				 {4,-2,1,6,7,2,0,3},
				 {4,-2,1,6,7,3,0,2},
				 {4,-2,2,6,7,0,1,3},
				 {4,-2,2,6,7,1,0,3},
				 {4,-2,2,6,7,3,0,1},
				 {4,-2,3,6,7,0,1,2},
				 {4,-2,3,6,7,1,0,2},
				 {4,-2,3,6,7,2,0,1},
				 {4,-2,0,5,7,1,2,3},
				 {4,-2,0,5,7,2,1,3},
				 {4,-2,0,5,7,3,1,2},
				 {4,-2,1,5,7,0,2,3},
				 {4,-2,1,5,7,2,0,3},
				 {4,-2,1,5,7,3,0,2},
				 {4,-2,2,5,7,0,1,3},
				 {4,-2,2,5,7,1,0,3},
				 {4,-2,2,5,7,3,0,1},
				 {4,-2,3,5,7,0,1,2},
				 {4,-2,3,5,7,1,0,2},
				 {4,-2,3,5,7,2,0,1},
				 {4,-2,0,5,6,1,2,3},
				 {4,-2,0,5,6,2,1,3},
				 {4,-2,0,5,6,3,1,2},
				 {4,-2,1,5,6,0,2,3},
				 {4,-2,1,5,6,2,0,3},
				 {4,-2,1,5,6,3,0,2},
				 {4,-2,2,5,6,0,1,3},
				 {4,-2,2,5,6,1,0,3},
				 {4,-2,2,5,6,3,0,1},
				 {4,-2,3,5,6,0,1,2},
				 {4,-2,3,5,6,1,0,2},
				 {4,-2,3,5,6,2,0,1},
				 {5,-2,0,6,7,1,2,3},
				 {5,-2,0,6,7,2,1,3},
				 {5,-2,0,6,7,3,1,2},
				 {5,-2,1,6,7,0,2,3},
				 {5,-2,1,6,7,2,0,3},
				 {5,-2,1,6,7,3,0,2},
				 {5,-2,2,6,7,0,1,3},
				 {5,-2,2,6,7,1,0,3},
				 {5,-2,2,6,7,3,0,1},
				 {5,-2,3,6,7,0,1,2},
				 {5,-2,3,6,7,1,0,2},
				 {5,-2,3,6,7,2,0,1},
				 {5,-2,0,4,7,1,2,3},
				 {5,-2,0,4,7,2,1,3},
				 {5,-2,0,4,7,3,1,2},
				 {5,-2,1,4,7,0,2,3},
				 {5,-2,1,4,7,2,0,3},
				 {5,-2,1,4,7,3,0,2},
				 {5,-2,2,4,7,0,1,3},
				 {5,-2,2,4,7,1,0,3},
				 {5,-2,2,4,7,3,0,1},
				 {5,-2,3,4,7,0,1,2},
				 {5,-2,3,4,7,1,0,2},
				 {5,-2,3,4,7,2,0,1},
				 {5,-2,0,4,6,1,2,3},
				 {5,-2,0,4,6,2,1,3},
				 {5,-2,0,4,6,3,1,2},
				 {5,-2,1,4,6,0,2,3},
				 {5,-2,1,4,6,2,0,3},
				 {5,-2,1,4,6,3,0,2},
				 {5,-2,2,4,6,0,1,3},
				 {5,-2,2,4,6,1,0,3},
				 {5,-2,2,4,6,3,0,1},
				 {5,-2,3,4,6,0,1,2},
				 {5,-2,3,4,6,1,0,2},
				 {5,-2,3,4,6,2,0,1},
				 {6,-2,0,5,7,1,2,3},
				 {6,-2,0,5,7,2,1,3},
				 {6,-2,0,5,7,3,1,2},
				 {6,-2,1,5,7,0,2,3},
				 {6,-2,1,5,7,2,0,3},
				 {6,-2,1,5,7,3,0,2},
				 {6,-2,2,5,7,0,1,3},
				 {6,-2,2,5,7,1,0,3},
				 {6,-2,2,5,7,3,0,1},
				 {6,-2,3,5,7,0,1,2},
				 {6,-2,3,5,7,1,0,2},
				 {6,-2,3,5,7,2,0,1},
				 {6,-2,0,4,7,1,2,3},
				 {6,-2,0,4,7,2,1,3},
				 {6,-2,0,4,7,3,1,2},
				 {6,-2,1,4,7,0,2,3},
				 {6,-2,1,4,7,2,0,3},
				 {6,-2,1,4,7,3,0,2},
				 {6,-2,2,4,7,0,1,3},
				 {6,-2,2,4,7,1,0,3},
				 {6,-2,2,4,7,3,0,1},
				 {6,-2,3,4,7,0,1,2},
				 {6,-2,3,4,7,1,0,2},
				 {6,-2,3,4,7,2,0,1},
				 {6,-2,0,4,5,1,2,3},
				 {6,-2,0,4,5,2,1,3},
				 {6,-2,0,4,5,3,1,2},
				 {6,-2,1,4,5,0,2,3},
				 {6,-2,1,4,5,2,0,3},
				 {6,-2,1,4,5,3,0,2},
				 {6,-2,2,4,5,0,1,3},
				 {6,-2,2,4,5,1,0,3},
				 {6,-2,2,4,5,3,0,1},
				 {6,-2,3,4,5,0,1,2},
				 {6,-2,3,4,5,1,0,2},
				 {6,-2,3,4,5,2,0,1},
				 {7,-2,0,5,6,1,2,3},
				 {7,-2,0,5,6,2,1,3},
				 {7,-2,0,5,6,3,1,2},
				 {7,-2,1,5,6,0,2,3},
				 {7,-2,1,5,6,2,0,3},
				 {7,-2,1,5,6,3,0,2},
				 {7,-2,2,5,6,0,1,3},
				 {7,-2,2,5,6,1,0,3},
				 {7,-2,2,5,6,3,0,1},
				 {7,-2,3,5,6,0,1,2},
				 {7,-2,3,5,6,1,0,2},
				 {7,-2,3,5,6,2,0,1},
				 {7,-2,0,4,6,1,2,3},
				 {7,-2,0,4,6,2,1,3},
				 {7,-2,0,4,6,3,1,2},
				 {7,-2,1,4,6,0,2,3},
				 {7,-2,1,4,6,2,0,3},
				 {7,-2,1,4,6,3,0,2},
				 {7,-2,2,4,6,0,1,3},
				 {7,-2,2,4,6,1,0,3},
				 {7,-2,2,4,6,3,0,1},
				 {7,-2,3,4,6,0,1,2},
				 {7,-2,3,4,6,1,0,2},
				 {7,-2,3,4,6,2,0,1},
				 {7,-2,0,4,5,1,2,3},
				 {7,-2,0,4,5,2,1,3},
				 {7,-2,0,4,5,3,1,2},
				 {7,-2,1,4,5,0,2,3},
				 {7,-2,1,4,5,2,0,3},
				 {7,-2,1,4,5,3,0,2},
				 {7,-2,2,4,5,0,1,3},
				 {7,-2,2,4,5,1,0,3},
				 {7,-2,2,4,5,3,0,1},
				 {7,-2,3,4,5,0,1,2},
				 {7,-2,3,4,5,1,0,2},
				 {7,-2,3,4,5,2,0,1}};
      perms = vec;
    }
    else if(nlost==2 && lost[0]==0 && lost[1]==1){ //0w2w2h2t - q1,qbar1
      vector<vector<int>> vec = {{-2,-2,0,6,7,1,2,3},
				 {-2,-2,0,6,7,2,1,3},
				 {-2,-2,0,6,7,3,1,2},
				 {-2,-2,1,6,7,0,2,3},
				 {-2,-2,1,6,7,2,0,3},
				 {-2,-2,1,6,7,3,0,2},
				 {-2,-2,2,6,7,0,1,3},
				 {-2,-2,2,6,7,1,0,3},
				 {-2,-2,2,6,7,3,0,1},
				 {-2,-2,3,6,7,0,1,2},
				 {-2,-2,3,6,7,1,0,2},
				 {-2,-2,3,6,7,2,0,1},
				 {-2,-2,0,5,7,1,2,3},
				 {-2,-2,0,5,7,2,1,3},
				 {-2,-2,0,5,7,3,1,2},
				 {-2,-2,1,5,7,0,2,3},
				 {-2,-2,1,5,7,2,0,3},
				 {-2,-2,1,5,7,3,0,2},
				 {-2,-2,2,5,7,0,1,3},
				 {-2,-2,2,5,7,1,0,3},
				 {-2,-2,2,5,7,3,0,1},
				 {-2,-2,3,5,7,0,1,2},
				 {-2,-2,3,5,7,1,0,2},
				 {-2,-2,3,5,7,2,0,1},
				 {-2,-2,0,5,6,1,2,3},
				 {-2,-2,0,5,6,2,1,3},
				 {-2,-2,0,5,6,3,1,2},
				 {-2,-2,1,5,6,0,2,3},
				 {-2,-2,1,5,6,2,0,3},
				 {-2,-2,1,5,6,3,0,2},
				 {-2,-2,2,5,6,0,1,3},
				 {-2,-2,2,5,6,1,0,3},
				 {-2,-2,2,5,6,3,0,1},
				 {-2,-2,3,5,6,0,1,2},
				 {-2,-2,3,5,6,1,0,2},
				 {-2,-2,3,5,6,2,0,1},
				 {-2,-2,0,4,7,1,2,3},
				 {-2,-2,0,4,7,2,1,3},
				 {-2,-2,0,4,7,3,1,2},
				 {-2,-2,1,4,7,0,2,3},
				 {-2,-2,1,4,7,2,0,3},
				 {-2,-2,1,4,7,3,0,2},
				 {-2,-2,2,4,7,0,1,3},
				 {-2,-2,2,4,7,1,0,3},
				 {-2,-2,2,4,7,3,0,1},
				 {-2,-2,3,4,7,0,1,2},
				 {-2,-2,3,4,7,1,0,2},
				 {-2,-2,3,4,7,2,0,1},
				 {-2,-2,0,4,6,1,2,3},
				 {-2,-2,0,4,6,2,1,3},
				 {-2,-2,0,4,6,3,1,2},
				 {-2,-2,1,4,6,0,2,3},
				 {-2,-2,1,4,6,2,0,3},
				 {-2,-2,1,4,6,3,0,2},
				 {-2,-2,2,4,6,0,1,3},
				 {-2,-2,2,4,6,1,0,3},
				 {-2,-2,2,4,6,3,0,1},
				 {-2,-2,3,4,6,0,1,2},
				 {-2,-2,3,4,6,1,0,2},
				 {-2,-2,3,4,6,2,0,1},
				 {-2,-2,0,4,5,1,2,3},
				 {-2,-2,0,4,5,2,1,3},
				 {-2,-2,0,4,5,3,1,2},
				 {-2,-2,1,4,5,0,2,3},
				 {-2,-2,1,4,5,2,0,3},
				 {-2,-2,1,4,5,3,0,2},
				 {-2,-2,2,4,5,0,1,3},
				 {-2,-2,2,4,5,1,0,3},
				 {-2,-2,2,4,5,3,0,1},
				 {-2,-2,3,4,5,0,1,2},
				 {-2,-2,3,4,5,1,0,2},
				 {-2,-2,3,4,5,2,0,1}};
      perms = vec;
    }
    else if(nlost==2 && lost[0]==1 && lost[1]==4){ //1w1w2h2t - qbar1,qbar2
      vector<vector<int>> vec = {{4,-2,0,6,-2,1,2,3},
				 {4,-2,0,6,-2,2,1,3},
				 {4,-2,0,6,-2,3,1,2},
				 {4,-2,0,7,-2,1,2,3},
				 {4,-2,0,7,-2,2,1,3},
				 {4,-2,0,7,-2,3,1,2},
				 {4,-2,1,6,-2,0,2,3},
				 {4,-2,1,6,-2,2,0,3},
				 {4,-2,1,6,-2,3,0,2},
				 {4,-2,1,7,-2,0,2,3},
				 {4,-2,1,7,-2,2,0,3},
				 {4,-2,1,7,-2,3,0,2},
				 {4,-2,2,6,-2,0,1,3},
				 {4,-2,2,6,-2,1,0,3},
				 {4,-2,2,6,-2,3,0,1},
				 {4,-2,2,7,-2,0,1,3},
				 {4,-2,2,7,-2,1,0,3},
				 {4,-2,2,7,-2,3,0,1},
				 {4,-2,3,6,-2,0,1,2},
				 {4,-2,3,6,-2,1,0,2},
				 {4,-2,3,6,-2,2,0,1},
				 {4,-2,3,7,-2,0,1,2},
				 {4,-2,3,7,-2,1,0,2},
				 {4,-2,3,7,-2,2,0,1},
				 {4,-2,0,5,-2,1,2,3},
				 {4,-2,0,5,-2,2,1,3},
				 {4,-2,0,5,-2,3,1,2},
				 {4,-2,1,5,-2,0,2,3},
				 {4,-2,1,5,-2,2,0,3},
				 {4,-2,1,5,-2,3,0,2},
				 {4,-2,2,5,-2,0,1,3},
				 {4,-2,2,5,-2,1,0,3},
				 {4,-2,2,5,-2,3,0,1},
				 {4,-2,3,5,-2,0,1,2},
				 {4,-2,3,5,-2,1,0,2},
				 {4,-2,3,5,-2,2,0,1},
				 {5,-2,0,6,-2,1,2,3},
				 {5,-2,0,6,-2,2,1,3},
				 {5,-2,0,6,-2,3,1,2},
				 {5,-2,0,7,-2,1,2,3},
				 {5,-2,0,7,-2,2,1,3},
				 {5,-2,0,7,-2,3,1,2},
				 {5,-2,1,6,-2,0,2,3},
				 {5,-2,1,6,-2,2,0,3},
				 {5,-2,1,6,-2,3,0,2},
				 {5,-2,1,7,-2,0,2,3},
				 {5,-2,1,7,-2,2,0,3},
				 {5,-2,1,7,-2,3,0,2},
				 {5,-2,2,6,-2,0,1,3},
				 {5,-2,2,6,-2,1,0,3},
				 {5,-2,2,6,-2,3,0,1},
				 {5,-2,2,7,-2,0,1,3},
				 {5,-2,2,7,-2,1,0,3},
				 {5,-2,2,7,-2,3,0,1},
				 {5,-2,3,6,-2,0,1,2},
				 {5,-2,3,6,-2,1,0,2},
				 {5,-2,3,6,-2,2,0,1},
				 {5,-2,3,7,-2,0,1,2},
				 {5,-2,3,7,-2,1,0,2},
				 {5,-2,3,7,-2,2,0,1},
				 {5,-2,0,4,-2,1,2,3},
				 {5,-2,0,4,-2,2,1,3},
				 {5,-2,0,4,-2,3,1,2},
				 {5,-2,1,4,-2,0,2,3},
				 {5,-2,1,4,-2,2,0,3},
				 {5,-2,1,4,-2,3,0,2},
				 {5,-2,2,4,-2,0,1,3},
				 {5,-2,2,4,-2,1,0,3},
				 {5,-2,2,4,-2,3,0,1},
				 {5,-2,3,4,-2,0,1,2},
				 {5,-2,3,4,-2,1,0,2},
				 {5,-2,3,4,-2,2,0,1},
				 {6,-2,0,5,-2,1,2,3},
				 {6,-2,0,5,-2,2,1,3},
				 {6,-2,0,5,-2,3,1,2},
				 {6,-2,0,7,-2,1,2,3},
				 {6,-2,0,7,-2,2,1,3},
				 {6,-2,0,7,-2,3,1,2},
				 {6,-2,1,5,-2,0,2,3},
				 {6,-2,1,5,-2,2,0,3},
				 {6,-2,1,5,-2,3,0,2},
				 {6,-2,1,7,-2,0,2,3},
				 {6,-2,1,7,-2,2,0,3},
				 {6,-2,1,7,-2,3,0,2},
				 {6,-2,2,5,-2,0,1,3},
				 {6,-2,2,5,-2,1,0,3},
				 {6,-2,2,5,-2,3,0,1},
				 {6,-2,2,7,-2,0,1,3},
				 {6,-2,2,7,-2,1,0,3},
				 {6,-2,2,7,-2,3,0,1},
				 {6,-2,3,5,-2,0,1,2},
				 {6,-2,3,5,-2,1,0,2},
				 {6,-2,3,5,-2,2,0,1},
				 {6,-2,3,7,-2,0,1,2},
				 {6,-2,3,7,-2,1,0,2},
				 {6,-2,3,7,-2,2,0,1},
				 {6,-2,0,4,-2,1,2,3},
				 {6,-2,0,4,-2,2,1,3},
				 {6,-2,0,4,-2,3,1,2},
				 {6,-2,1,4,-2,0,2,3},
				 {6,-2,1,4,-2,2,0,3},
				 {6,-2,1,4,-2,3,0,2},
				 {6,-2,2,4,-2,0,1,3},
				 {6,-2,2,4,-2,1,0,3},
				 {6,-2,2,4,-2,3,0,1},
				 {6,-2,3,4,-2,0,1,2},
				 {6,-2,3,4,-2,1,0,2},
				 {6,-2,3,4,-2,2,0,1},
				 {7,-2,0,5,-2,1,2,3},
				 {7,-2,0,5,-2,2,1,3},
				 {7,-2,0,5,-2,3,1,2},
				 {7,-2,0,6,-2,1,2,3},
				 {7,-2,0,6,-2,2,1,3},
				 {7,-2,0,6,-2,3,1,2},
				 {7,-2,1,5,-2,0,2,3},
				 {7,-2,1,5,-2,2,0,3},
				 {7,-2,1,5,-2,3,0,2},
				 {7,-2,1,6,-2,0,2,3},
				 {7,-2,1,6,-2,2,0,3},
				 {7,-2,1,6,-2,3,0,2},
				 {7,-2,2,5,-2,0,1,3},
				 {7,-2,2,5,-2,1,0,3},
				 {7,-2,2,5,-2,3,0,1},
				 {7,-2,2,6,-2,0,1,3},
				 {7,-2,2,6,-2,1,0,3},
				 {7,-2,2,6,-2,3,0,1},
				 {7,-2,3,5,-2,0,1,2},
				 {7,-2,3,5,-2,1,0,2},
				 {7,-2,3,5,-2,2,0,1},
				 {7,-2,3,6,-2,0,1,2},
				 {7,-2,3,6,-2,1,0,2},
				 {7,-2,3,6,-2,2,0,1},
				 {7,-2,0,4,-2,1,2,3},
				 {7,-2,0,4,-2,2,1,3},
				 {7,-2,0,4,-2,3,1,2},
				 {7,-2,1,4,-2,0,2,3},
				 {7,-2,1,4,-2,2,0,3},
				 {7,-2,1,4,-2,3,0,2},
				 {7,-2,2,4,-2,0,1,3},
				 {7,-2,2,4,-2,1,0,3},
				 {7,-2,2,4,-2,3,0,1},
				 {7,-2,3,4,-2,0,1,2},
				 {7,-2,3,4,-2,1,0,2},
				 {7,-2,3,4,-2,2,0,1}};
      perms = vec;
    }
    else if(nlost==4 && lost[0]==0 && lost[1]==1 && lost[2]==3 &&
	    lost[3]==4){ //0w0w2h2t - q1,qbar1,q2,qbar2
      vector<vector<int>> vec = {{-2,-2,0,-2,-2,1,2,3},
				 {-2,-2,0,-2,-2,2,1,3},
				 {-2,-2,0,-2,-2,3,1,2},
				 {-2,-2,1,-2,-2,0,2,3},
				 {-2,-2,1,-2,-2,2,0,3},
				 {-2,-2,1,-2,-2,3,0,2},
				 {-2,-2,2,-2,-2,0,1,3},
				 {-2,-2,2,-2,-2,1,0,3},
				 {-2,-2,2,-2,-2,3,0,1},
				 {-2,-2,3,-2,-2,0,1,2},
				 {-2,-2,3,-2,-2,1,0,2},
				 {-2,-2,3,-2,-2,2,0,1}};
      perms = vec;
    }
    else if(nlost==5 && lost[0]==0 && lost[1]==1 && lost[2]==3 &&
	    lost[3]==4 && lost[4]==2 ){ //0w0w2h1t - q1,qbar1,q2,qbar2,b1
      vector<vector<int>> vec = {{-2,-2,-2,-2,-2,1,2,3},
				 {-2,-2,-2,-2,-2,2,1,3},
				 {-2,-2,-2,-2,-2,3,1,2},
				 {-2,-2,-2,-2,-2,0,2,3},
				 {-2,-2,-2,-2,-2,2,0,3},
				 {-2,-2,-2,-2,-2,3,0,2},
				 {-2,-2,-2,-2,-2,0,1,3},
				 {-2,-2,-2,-2,-2,1,0,3},
				 {-2,-2,-2,-2,-2,3,0,1},
				 {-2,-2,-2,-2,-2,0,1,2},
				 {-2,-2,-2,-2,-2,1,0,2},
				 {-2,-2,-2,-2,-2,2,0,1}};
      perms = vec;
    }
    else cout << "8j,4b "<< nlost <<" lost not defined" << endl;
  }
  else if(nq==5 && nb==3){ //8j,3b
    if(nlost==1 && lost[0]==2){ //4w2h1t - b1
      vector<vector<int>> vec = {{3,4,-2,6,7,0,1,2},
				 {3,4,-2,6,7,1,0,2},
				 {3,4,-2,6,7,2,0,1},
				 {3,4,-2,5,7,0,1,2},
				 {3,4,-2,5,7,1,0,2},
				 {3,4,-2,5,7,2,0,1},
				 {3,4,-2,5,6,0,1,2},
				 {3,4,-2,5,6,1,0,2},
				 {3,4,-2,5,6,2,0,1},
				 {3,5,-2,6,7,0,1,2},
				 {3,5,-2,6,7,1,0,2},
				 {3,5,-2,6,7,2,0,1},
				 {3,5,-2,4,7,0,1,2},
				 {3,5,-2,4,7,1,0,2},
				 {3,5,-2,4,7,2,0,1},
				 {3,5,-2,4,6,0,1,2},
				 {3,5,-2,4,6,1,0,2},
				 {3,5,-2,4,6,2,0,1},
				 {3,6,-2,5,7,0,1,2},
				 {3,6,-2,5,7,1,0,2},
				 {3,6,-2,5,7,2,0,1},
				 {3,6,-2,4,7,0,1,2},
				 {3,6,-2,4,7,1,0,2},
				 {3,6,-2,4,7,2,0,1},
				 {3,6,-2,4,5,0,1,2},
				 {3,6,-2,4,5,1,0,2},
				 {3,6,-2,4,5,2,0,1},
				 {3,7,-2,5,6,0,1,2},
				 {3,7,-2,5,6,1,0,2},
				 {3,7,-2,5,6,2,0,1},
				 {3,7,-2,4,6,0,1,2},
				 {3,7,-2,4,6,1,0,2},
				 {3,7,-2,4,6,2,0,1},
				 {3,7,-2,4,5,0,1,2},
				 {3,7,-2,4,5,1,0,2},
				 {3,7,-2,4,5,2,0,1},
				 {4,5,-2,6,7,0,1,2},
				 {4,5,-2,6,7,1,0,2},
				 {4,5,-2,6,7,2,0,1},
				 {4,5,-2,3,7,0,1,2},
				 {4,5,-2,3,7,1,0,2},
				 {4,5,-2,3,7,2,0,1},
				 {4,5,-2,3,6,0,1,2},
				 {4,5,-2,3,6,1,0,2},
				 {4,5,-2,3,6,2,0,1},
				 {4,6,-2,5,7,0,1,2},
				 {4,6,-2,5,7,1,0,2},
				 {4,6,-2,5,7,2,0,1},
				 {4,6,-2,3,7,0,1,2},
				 {4,6,-2,3,7,1,0,2},
				 {4,6,-2,3,7,2,0,1},
				 {4,6,-2,3,5,0,1,2},
				 {4,6,-2,3,5,1,0,2},
				 {4,6,-2,3,5,2,0,1},
				 {4,7,-2,5,6,0,1,2},
				 {4,7,-2,5,6,1,0,2},
				 {4,7,-2,5,6,2,0,1},
				 {4,7,-2,3,6,0,1,2},
				 {4,7,-2,3,6,1,0,2},
				 {4,7,-2,3,6,2,0,1},
				 {4,7,-2,3,5,0,1,2},
				 {4,7,-2,3,5,1,0,2},
				 {4,7,-2,3,5,2,0,1},
				 {5,6,-2,4,7,0,1,2},
				 {5,6,-2,4,7,1,0,2},
				 {5,6,-2,4,7,2,0,1},
				 {5,6,-2,3,7,0,1,2},
				 {5,6,-2,3,7,1,0,2},
				 {5,6,-2,3,7,2,0,1},
				 {5,6,-2,3,4,0,1,2},
				 {5,6,-2,3,4,1,0,2},
				 {5,6,-2,3,4,2,0,1},
				 {5,7,-2,4,6,0,1,2},
				 {5,7,-2,4,6,1,0,2},
				 {5,7,-2,4,6,2,0,1},
				 {5,7,-2,3,6,0,1,2},
				 {5,7,-2,3,6,1,0,2},
				 {5,7,-2,3,6,2,0,1},
				 {5,7,-2,3,4,0,1,2},
				 {5,7,-2,3,4,1,0,2},
				 {5,7,-2,3,4,2,0,1},
				 {6,7,-2,4,5,0,1,2},
				 {6,7,-2,4,5,1,0,2},
				 {6,7,-2,4,5,2,0,1},
				 {6,7,-2,3,5,0,1,2},
				 {6,7,-2,3,5,1,0,2},
				 {6,7,-2,3,5,2,0,1},
				 {6,7,-2,3,4,0,1,2},
				 {6,7,-2,3,4,1,0,2},
				 {6,7,-2,3,4,2,0,1}};
      perms = vec;
    }
    else if(nlost==1 && lost[0]==7){ //4w1h2t - bbar
      vector<vector<int>> vec = {{3,4,2,6,7,0,1,-2},
				 {3,4,2,6,7,1,0,-2},
				 {3,4,1,6,7,2,0,-2},
				 {3,4,2,5,7,0,1,-2},
				 {3,4,2,5,7,1,0,-2},
				 {3,4,1,5,7,2,0,-2},
				 {3,4,2,5,6,0,1,-2},
				 {3,4,2,5,6,1,0,-2},
				 {3,4,1,5,6,2,0,-2},
				 {3,5,2,6,7,0,1,-2},
				 {3,5,2,6,7,1,0,-2},
				 {3,5,1,6,7,2,0,-2},
				 {3,5,2,4,7,0,1,-2},
				 {3,5,2,4,7,1,0,-2},
				 {3,5,1,4,7,2,0,-2},
				 {3,5,2,4,6,0,1,-2},
				 {3,5,2,4,6,1,0,-2},
				 {3,5,1,4,6,2,0,-2},
				 {3,6,2,5,7,0,1,-2},
				 {3,6,2,5,7,1,0,-2},
				 {3,6,1,5,7,2,0,-2},
				 {3,6,2,4,7,0,1,-2},
				 {3,6,2,4,7,1,0,-2},
				 {3,6,1,4,7,2,0,-2},
				 {3,6,2,4,5,0,1,-2},
				 {3,6,2,4,5,1,0,-2},
				 {3,6,1,4,5,2,0,-2},
				 {3,7,2,5,6,0,1,-2},
				 {3,7,2,5,6,1,0,-2},
				 {3,7,1,5,6,2,0,-2},
				 {3,7,2,4,6,0,1,-2},
				 {3,7,2,4,6,1,0,-2},
				 {3,7,1,4,6,2,0,-2},
				 {3,7,2,4,5,0,1,-2},
				 {3,7,2,4,5,1,0,-2},
				 {3,7,1,4,5,2,0,-2},
				 {4,5,2,6,7,0,1,-2},
				 {4,5,2,6,7,1,0,-2},
				 {4,5,1,6,7,2,0,-2},
				 {4,5,2,3,7,0,1,-2},
				 {4,5,2,3,7,1,0,-2},
				 {4,5,1,3,7,2,0,-2},
				 {4,5,2,3,6,0,1,-2},
				 {4,5,2,3,6,1,0,-2},
				 {4,5,1,3,6,2,0,-2},
				 {4,6,2,5,7,0,1,-2},
				 {4,6,2,5,7,1,0,-2},
				 {4,6,1,5,7,2,0,-2},
				 {4,6,2,3,7,0,1,-2},
				 {4,6,2,3,7,1,0,-2},
				 {4,6,1,3,7,2,0,-2},
				 {4,6,2,3,5,0,1,-2},
				 {4,6,2,3,5,1,0,-2},
				 {4,6,1,3,5,2,0,-2},
				 {4,7,2,5,6,0,1,-2},
				 {4,7,2,5,6,1,0,-2},
				 {4,7,1,5,6,2,0,-2},
				 {4,7,2,3,6,0,1,-2},
				 {4,7,2,3,6,1,0,-2},
				 {4,7,1,3,6,2,0,-2},
				 {4,7,2,3,5,0,1,-2},
				 {4,7,2,3,5,1,0,-2},
				 {4,7,1,3,5,2,0,-2},
				 {5,6,2,4,7,0,1,-2},
				 {5,6,2,4,7,1,0,-2},
				 {5,6,1,4,7,2,0,-2},
				 {5,6,2,3,7,0,1,-2},
				 {5,6,2,3,7,1,0,-2},
				 {5,6,1,3,7,2,0,-2},
				 {5,6,2,3,4,0,1,-2},
				 {5,6,2,3,4,1,0,-2},
				 {5,6,1,3,4,2,0,-2},
				 {5,7,2,4,6,0,1,-2},
				 {5,7,2,4,6,1,0,-2},
				 {5,7,1,4,6,2,0,-2},
				 {5,7,2,3,6,0,1,-2},
				 {5,7,2,3,6,1,0,-2},
				 {5,7,1,3,6,2,0,-2},
				 {5,7,2,3,4,0,1,-2},
				 {5,7,2,3,4,1,0,-2},
				 {5,7,1,3,4,2,0,-2},
				 {6,7,2,4,5,0,1,-2},
				 {6,7,2,4,5,1,0,-2},
				 {6,7,1,4,5,2,0,-2},
				 {6,7,2,3,5,0,1,-2},
				 {6,7,2,3,5,1,0,-2},
				 {6,7,1,3,5,2,0,-2},
				 {6,7,2,3,4,0,1,-2},
				 {6,7,2,3,4,1,0,-2},
				 {6,7,1,3,4,2,0,-2},
				 {3,4,0,6,7,2,1,-2},
				 {3,4,1,6,7,2,0,-2},
				 {3,4,2,6,7,1,0,-2},
				 {3,4,0,5,7,2,1,-2},
				 {3,4,1,5,7,2,0,-2},
				 {3,4,2,5,7,1,0,-2},
				 {3,4,0,5,6,2,1,-2},
				 {3,4,1,5,6,2,0,-2},
				 {3,4,2,5,6,1,0,-2},
				 {3,5,0,6,7,2,1,-2},
				 {3,5,1,6,7,2,0,-2},
				 {3,5,2,6,7,1,0,-2},
				 {3,5,0,4,7,2,1,-2},
				 {3,5,1,4,7,2,0,-2},
				 {3,5,2,4,7,1,0,-2},
				 {3,5,0,4,6,2,1,-2},
				 {3,5,1,4,6,2,0,-2},
				 {3,5,2,4,6,1,0,-2},
				 {3,6,0,5,7,2,1,-2},
				 {3,6,1,5,7,2,0,-2},
				 {3,6,2,5,7,1,0,-2},
				 {3,6,0,4,7,2,1,-2},
				 {3,6,1,4,7,2,0,-2},
				 {3,6,2,4,7,1,0,-2},
				 {3,6,0,4,5,2,1,-2},
				 {3,6,1,4,5,2,0,-2},
				 {3,6,2,4,5,1,0,-2},
				 {3,7,0,5,6,2,1,-2},
				 {3,7,1,5,6,2,0,-2},
				 {3,7,2,5,6,1,0,-2},
				 {3,7,0,4,6,2,1,-2},
				 {3,7,1,4,6,2,0,-2},
				 {3,7,2,4,6,1,0,-2},
				 {3,7,0,4,5,2,1,-2},
				 {3,7,1,4,5,2,0,-2},
				 {3,7,2,4,5,1,0,-2},
				 {4,5,0,6,7,2,1,-2},
				 {4,5,1,6,7,2,0,-2},
				 {4,5,2,6,7,1,0,-2},
				 {4,5,0,3,7,2,1,-2},
				 {4,5,1,3,7,2,0,-2},
				 {4,5,2,3,7,1,0,-2},
				 {4,5,0,3,6,2,1,-2},
				 {4,5,1,3,6,2,0,-2},
				 {4,5,2,3,6,1,0,-2},
				 {4,6,0,5,7,2,1,-2},
				 {4,6,1,5,7,2,0,-2},
				 {4,6,2,5,7,1,0,-2},
				 {4,6,0,3,7,2,1,-2},
				 {4,6,1,3,7,2,0,-2},
				 {4,6,2,3,7,1,0,-2},
				 {4,6,0,3,5,2,1,-2},
				 {4,6,1,3,5,2,0,-2},
				 {4,6,2,3,5,1,0,-2},
				 {4,7,0,5,6,2,1,-2},
				 {4,7,1,5,6,2,0,-2},
				 {4,7,2,5,6,1,0,-2},
				 {4,7,0,3,6,2,1,-2},
				 {4,7,1,3,6,2,0,-2},
				 {4,7,2,3,6,1,0,-2},
				 {4,7,0,3,5,2,1,-2},
				 {4,7,1,3,5,2,0,-2},
				 {4,7,2,3,5,1,0,-2},
				 {5,6,0,4,7,2,1,-2},
				 {5,6,1,4,7,2,0,-2},
				 {5,6,2,4,7,1,0,-2},
				 {5,6,0,3,7,2,1,-2},
				 {5,6,1,3,7,2,0,-2},
				 {5,6,2,3,7,1,0,-2},
				 {5,6,0,3,4,2,1,-2},
				 {5,6,1,3,4,2,0,-2},
				 {5,6,2,3,4,1,0,-2},
				 {5,7,0,4,6,2,1,-2},
				 {5,7,1,4,6,2,0,-2},
				 {5,7,2,4,6,1,0,-2},
				 {5,7,0,3,6,2,1,-2},
				 {5,7,1,3,6,2,0,-2},
				 {5,7,2,3,6,1,0,-2},
				 {5,7,0,3,4,2,1,-2},
				 {5,7,1,3,4,2,0,-2},
				 {5,7,2,3,4,1,0,-2},
				 {6,7,0,4,5,2,1,-2},
				 {6,7,1,4,5,2,0,-2},
				 {6,7,2,4,5,1,0,-2},
				 {6,7,0,3,5,2,1,-2},
				 {6,7,1,3,5,2,0,-2},
				 {6,7,2,3,5,1,0,-2},
				 {6,7,0,3,4,2,1,-2},
				 {6,7,1,3,4,2,0,-2},
				 {6,7,2,3,4,1,0,-2}};
      perms = vec;
    }
    else if(nlost==2 && lost[0]==1 && lost[1]==2){ //3w2h1t - qbar1,b1
      vector<vector<int>> vec = {{3,-2,-2,6,7,0,1,2},
				 {3,-2,-2,6,7,1,0,2},
				 {3,-2,-2,6,7,2,0,1},
				 {3,-2,-2,5,7,0,1,2},
				 {3,-2,-2,5,7,1,0,2},
				 {3,-2,-2,5,7,2,0,1},
				 {3,-2,-2,5,6,0,1,2},
				 {3,-2,-2,5,6,1,0,2},
				 {3,-2,-2,5,6,2,0,1},
				 {3,-2,-2,4,7,0,1,2},
				 {3,-2,-2,4,7,1,0,2},
				 {3,-2,-2,4,7,2,0,1},
				 {3,-2,-2,4,6,0,1,2},
				 {3,-2,-2,4,6,1,0,2},
				 {3,-2,-2,4,6,2,0,1},
				 {3,-2,-2,4,5,0,1,2},
				 {3,-2,-2,4,5,1,0,2},
				 {3,-2,-2,4,5,2,0,1},
				 {4,-2,-2,6,7,0,1,2},
				 {4,-2,-2,6,7,1,0,2},
				 {4,-2,-2,6,7,2,0,1},
				 {4,-2,-2,5,7,0,1,2},
				 {4,-2,-2,5,7,1,0,2},
				 {4,-2,-2,5,7,2,0,1},
				 {4,-2,-2,5,6,0,1,2},
				 {4,-2,-2,5,6,1,0,2},
				 {4,-2,-2,5,6,2,0,1},
				 {4,-2,-2,3,7,0,1,2},
				 {4,-2,-2,3,7,1,0,2},
				 {4,-2,-2,3,7,2,0,1},
				 {4,-2,-2,3,6,0,1,2},
				 {4,-2,-2,3,6,1,0,2},
				 {4,-2,-2,3,6,2,0,1},
				 {4,-2,-2,3,5,0,1,2},
				 {4,-2,-2,3,5,1,0,2},
				 {4,-2,-2,3,5,2,0,1},
				 {5,-2,-2,6,7,0,1,2},
				 {5,-2,-2,6,7,1,0,2},
				 {5,-2,-2,6,7,2,0,1},
				 {5,-2,-2,4,7,0,1,2},
				 {5,-2,-2,4,7,1,0,2},
				 {5,-2,-2,4,7,2,0,1},
				 {5,-2,-2,4,6,0,1,2},
				 {5,-2,-2,4,6,1,0,2},
				 {5,-2,-2,4,6,2,0,1},
				 {5,-2,-2,3,7,0,1,2},
				 {5,-2,-2,3,7,1,0,2},
				 {5,-2,-2,3,7,2,0,1},
				 {5,-2,-2,3,6,0,1,2},
				 {5,-2,-2,3,6,1,0,2},
				 {5,-2,-2,3,6,2,0,1},
				 {5,-2,-2,3,4,0,1,2},
				 {5,-2,-2,3,4,1,0,2},
				 {5,-2,-2,3,4,2,0,1},
				 {6,-2,-2,5,7,0,1,2},
				 {6,-2,-2,5,7,1,0,2},
				 {6,-2,-2,5,7,2,0,1},
				 {6,-2,-2,4,7,0,1,2},
				 {6,-2,-2,4,7,1,0,2},
				 {6,-2,-2,4,7,2,0,1},
				 {6,-2,-2,4,5,0,1,2},
				 {6,-2,-2,4,5,1,0,2},
				 {6,-2,-2,4,5,2,0,1},
				 {6,-2,-2,3,7,0,1,2},
				 {6,-2,-2,3,7,1,0,2},
				 {6,-2,-2,3,7,2,0,1},
				 {6,-2,-2,3,5,0,1,2},
				 {6,-2,-2,3,5,1,0,2},
				 {6,-2,-2,3,5,2,0,1},
				 {6,-2,-2,3,4,0,1,2},
				 {6,-2,-2,3,4,1,0,2},
				 {6,-2,-2,3,4,2,0,1},
				 {7,-2,-2,5,6,0,1,2},
				 {7,-2,-2,5,6,1,0,2},
				 {7,-2,-2,5,6,2,0,1},
				 {7,-2,-2,4,6,0,1,2},
				 {7,-2,-2,4,6,1,0,2},
				 {7,-2,-2,4,6,2,0,1},
				 {7,-2,-2,4,5,0,1,2},
				 {7,-2,-2,4,5,1,0,2},
				 {7,-2,-2,4,5,2,0,1},
				 {7,-2,-2,3,6,0,1,2},
				 {7,-2,-2,3,6,1,0,2},
				 {7,-2,-2,3,6,2,0,1},
				 {7,-2,-2,3,5,0,1,2},
				 {7,-2,-2,3,5,1,0,2},
				 {7,-2,-2,3,5,2,0,1},
				 {7,-2,-2,3,4,0,1,2},
				 {7,-2,-2,3,4,1,0,2},
				 {7,-2,-2,3,4,2,0,1}};
      perms = vec;
    }
    else if(nlost==5 && lost[0]==0 && lost[1]==1 && lost[2]==3 &&
	    lost[3]==4 && lost[4]==2 ){ //0w0w2h1t - q1,qbar1,q2,qbar2,b1
      vector<vector<int>> vec = {{-2,-2,-2,-2,-2,0,1,2},
				 {-2,-2,-2,-2,-2,1,0,2},
				 {-2,-2,-2,-2,-2,2,0,1}};
      perms = vec;
    }
    else cout << "8j,3b "<< nlost <<" lost not defined" << endl;
  }
  else if(nq==3 && nb==4){ //7j,4b
    if(nlost==1 && lost[0]==1){ //3w2h2t - qbar1
      vector<vector<int>> vec = {{4,-2,0,5,6,1,2,3},
				 {4,-2,0,5,6,2,1,3},
				 {4,-2,0,5,6,3,1,2},
				 {4,-2,1,5,6,0,2,3},
				 {4,-2,1,5,6,2,0,3},
				 {4,-2,1,5,6,3,0,2},
				 {4,-2,2,5,6,0,1,3},
				 {4,-2,2,5,6,1,0,3},
				 {4,-2,2,5,6,3,0,1},
				 {4,-2,3,5,6,0,1,2},
				 {4,-2,3,5,6,1,0,2},
				 {4,-2,3,5,6,2,0,1},
				 {5,-2,0,4,6,1,2,3},
				 {5,-2,0,4,6,2,1,3},
				 {5,-2,0,4,6,3,1,2},
				 {5,-2,1,4,6,0,2,3},
				 {5,-2,1,4,6,2,0,3},
				 {5,-2,1,4,6,3,0,2},
				 {5,-2,2,4,6,0,1,3},
				 {5,-2,2,4,6,1,0,3},
				 {5,-2,2,4,6,3,0,1},
				 {5,-2,3,4,6,0,1,2},
				 {5,-2,3,4,6,1,0,2},
				 {5,-2,3,4,6,2,0,1},
				 {6,-2,0,4,5,1,2,3},
				 {6,-2,0,4,5,2,1,3},
				 {6,-2,0,4,5,3,1,2},
				 {6,-2,1,4,5,0,2,3},
				 {6,-2,1,4,5,2,0,3},
				 {6,-2,1,4,5,3,0,2},
				 {6,-2,2,4,5,0,1,3},
				 {6,-2,2,4,5,1,0,3},
				 {6,-2,2,4,5,3,0,1},
				 {6,-2,3,4,5,0,1,2},
				 {6,-2,3,4,5,1,0,2},
				 {6,-2,3,4,5,2,0,1}};
      perms = vec;
    }
    else if(nlost==2 && lost[0]==0 && lost[1]==1){ //0w2w2h2t - q1,qbar1
      vector<vector<int>> vec = {{-2,-2,0,5,6,1,2,3},
				 {-2,-2,0,5,6,2,1,3},
				 {-2,-2,0,5,6,3,1,2},
				 {-2,-2,1,5,6,0,2,3},
				 {-2,-2,1,5,6,2,0,3},
				 {-2,-2,1,5,6,3,0,2},
				 {-2,-2,2,5,6,0,1,3},
				 {-2,-2,2,5,6,1,0,3},
				 {-2,-2,2,5,6,3,0,1},
				 {-2,-2,3,5,6,0,1,2},
				 {-2,-2,3,5,6,1,0,2},
				 {-2,-2,3,5,6,2,0,1},
				 {-2,-2,0,4,6,1,2,3},
				 {-2,-2,0,4,6,2,1,3},
				 {-2,-2,0,4,6,3,1,2},
				 {-2,-2,1,4,6,0,2,3},
				 {-2,-2,1,4,6,2,0,3},
				 {-2,-2,1,4,6,3,0,2},
				 {-2,-2,2,4,6,0,1,3},
				 {-2,-2,2,4,6,1,0,3},
				 {-2,-2,2,4,6,3,0,1},
				 {-2,-2,3,4,6,0,1,2},
				 {-2,-2,3,4,6,1,0,2},
				 {-2,-2,3,4,6,2,0,1},
				 {-2,-2,0,4,5,1,2,3},
				 {-2,-2,0,4,5,2,1,3},
				 {-2,-2,0,4,5,3,1,2},
				 {-2,-2,1,4,5,0,2,3},
				 {-2,-2,1,4,5,2,0,3},
				 {-2,-2,1,4,5,3,0,2},
				 {-2,-2,2,4,5,0,1,3},
				 {-2,-2,2,4,5,1,0,3},
				 {-2,-2,2,4,5,3,0,1},
				 {-2,-2,3,4,5,0,1,2},
				 {-2,-2,3,4,5,1,0,2},
				 {-2,-2,3,4,5,2,0,1}};
      perms = vec;
    }
    else if(nlost==2 && lost[0]==1 && lost[1]==4){ //1w1w2h2t - qbar1,qbar2
      vector<vector<int>> vec = {{4,-2,0,5,-2,1,2,3},
				 {4,-2,0,5,-2,2,1,3},
				 {4,-2,0,5,-2,3,1,2},
				 {4,-2,0,6,-2,1,2,3},
				 {4,-2,0,6,-2,2,1,3},
				 {4,-2,0,6,-2,3,1,2},
				 {4,-2,1,5,-2,0,2,3},
				 {4,-2,1,5,-2,2,0,3},
				 {4,-2,1,5,-2,3,0,2},
				 {4,-2,1,6,-2,0,2,3},
				 {4,-2,1,6,-2,2,0,3},
				 {4,-2,1,6,-2,3,0,2},
				 {4,-2,2,5,-2,0,1,3},
				 {4,-2,2,5,-2,1,0,3},
				 {4,-2,2,5,-2,3,0,1},
				 {4,-2,2,6,-2,0,1,3},
				 {4,-2,2,6,-2,1,0,3},
				 {4,-2,2,6,-2,3,0,1},
				 {4,-2,3,5,-2,0,1,2},
				 {4,-2,3,5,-2,1,0,2},
				 {4,-2,3,5,-2,2,0,1},
				 {4,-2,3,6,-2,0,1,2},
				 {4,-2,3,6,-2,1,0,2},
				 {4,-2,3,6,-2,2,0,1},
				 {5,-2,0,4,-2,1,2,3},
				 {5,-2,0,4,-2,2,1,3},
				 {5,-2,0,4,-2,3,1,2},
				 {5,-2,0,6,-2,1,2,3},
				 {5,-2,0,6,-2,2,1,3},
				 {5,-2,0,6,-2,3,1,2},
				 {5,-2,1,4,-2,0,2,3},
				 {5,-2,1,4,-2,2,0,3},
				 {5,-2,1,4,-2,3,0,2},
				 {5,-2,1,6,-2,0,2,3},
				 {5,-2,1,6,-2,2,0,3},
				 {5,-2,1,6,-2,3,0,2},
				 {5,-2,2,4,-2,0,1,3},
				 {5,-2,2,4,-2,1,0,3},
				 {5,-2,2,4,-2,3,0,1},
				 {5,-2,2,6,-2,0,1,3},
				 {5,-2,2,6,-2,1,0,3},
				 {5,-2,2,6,-2,3,0,1},
				 {5,-2,3,4,-2,0,1,2},
				 {5,-2,3,4,-2,1,0,2},
				 {5,-2,3,4,-2,2,0,1},
				 {5,-2,3,6,-2,0,1,2},
				 {5,-2,3,6,-2,1,0,2},
				 {5,-2,3,6,-2,2,0,1},
				 {6,-2,0,4,-2,1,2,3},
				 {6,-2,0,4,-2,2,1,3},
				 {6,-2,0,4,-2,3,1,2},
				 {6,-2,0,5,-2,1,2,3},
				 {6,-2,0,5,-2,2,1,3},
				 {6,-2,0,5,-2,3,1,2},
				 {6,-2,1,4,-2,0,2,3},
				 {6,-2,1,4,-2,2,0,3},
				 {6,-2,1,4,-2,3,0,2},
				 {6,-2,1,5,-2,0,2,3},
				 {6,-2,1,5,-2,2,0,3},
				 {6,-2,1,5,-2,3,0,2},
				 {6,-2,2,4,-2,0,1,3},
				 {6,-2,2,4,-2,1,0,3},
				 {6,-2,2,4,-2,3,0,1},
				 {6,-2,2,5,-2,0,1,3},
				 {6,-2,2,5,-2,1,0,3},
				 {6,-2,2,5,-2,3,0,1},
				 {6,-2,3,4,-2,0,1,2},
				 {6,-2,3,4,-2,1,0,2},
				 {6,-2,3,4,-2,2,0,1},
				 {6,-2,3,5,-2,0,1,2},
				 {6,-2,3,5,-2,1,0,2},
				 {6,-2,3,5,-2,2,0,1}};
      perms = vec;
    }
    else if(nlost==4 && lost[0]==0 && lost[1]==1 && lost[2]==3 &&
	    lost[3]==4){ //0w0w2h2t - q1,qbar1,q2,qbar2
      vector<vector<int>> vec = {{-2,-2,0,-2,-2,1,2,3},
				 {-2,-2,0,-2,-2,2,1,3},
				 {-2,-2,0,-2,-2,3,1,2},
				 {-2,-2,1,-2,-2,0,2,3},
				 {-2,-2,1,-2,-2,2,0,3},
				 {-2,-2,1,-2,-2,3,0,2},
				 {-2,-2,2,-2,-2,0,1,3},
				 {-2,-2,2,-2,-2,1,0,3},
				 {-2,-2,2,-2,-2,3,0,1},
				 {-2,-2,3,-2,-2,0,1,2},
				 {-2,-2,3,-2,-2,1,0,2},
				 {-2,-2,3,-2,-2,2,0,1}};
      perms = vec;
    }
    else if(nlost==5 && lost[0]==0 && lost[1]==1 && lost[2]==3 &&
	    lost[3]==4 && lost[4]==2 ){ //0w0w2h1t - q1,qbar1,q2,qbar2,b1
      vector<vector<int>> vec = {{-2,-2,-2,-2,-2,1,2,3},
				 {-2,-2,-2,-2,-2,2,1,3},
				 {-2,-2,-2,-2,-2,3,1,2},
				 {-2,-2,-2,-2,-2,0,2,3},
				 {-2,-2,-2,-2,-2,2,0,3},
				 {-2,-2,-2,-2,-2,3,0,2},
				 {-2,-2,-2,-2,-2,0,1,3},
				 {-2,-2,-2,-2,-2,1,0,3},
				 {-2,-2,-2,-2,-2,3,0,1},
				 {-2,-2,-2,-2,-2,0,1,2},
				 {-2,-2,-2,-2,-2,1,0,2},
				 {-2,-2,-2,-2,-2,2,0,1}};
      perms = vec;
    }
    else cout << "7j,4b "<< nlost <<" lost not defined" << endl;
  }
  else if(nq==4 && nb==3){ //7j,3b
    if(nlost==1 && lost[0]==2){ //4w2h1t - b1
      vector<vector<int>> vec = {{3,4,-2,5,6,0,1,2},
				 {3,4,-2,5,6,1,0,2},
				 {3,4,-2,5,6,2,0,1},
				 {3,5,-2,4,6,0,1,2},
				 {3,5,-2,4,6,1,0,2},
				 {3,5,-2,4,6,2,0,1},
				 {3,6,-2,4,5,0,1,2},
				 {3,6,-2,4,5,1,0,2},
				 {3,6,-2,4,5,2,0,1},
				 {4,5,-2,3,6,0,1,2},
				 {4,5,-2,3,6,1,0,2},
				 {4,5,-2,3,6,2,0,1},
				 {4,6,-2,3,5,0,1,2},
				 {4,6,-2,3,5,1,0,2},
				 {4,6,-2,3,5,2,0,1},
				 {5,6,-2,3,4,0,1,2},
				 {5,6,-2,3,4,1,0,2},
				 {5,6,-2,3,4,2,0,1}};
      perms = vec;
    }
    else if(nlost==1 && lost[0]==7){ //4w1h2t - bbar
      vector<vector<int>> vec = {{3,4,2,5,6,0,1,-2},
				 {3,4,2,5,6,1,0,-2},
				 {3,4,1,5,6,2,0,-2},
				 {3,5,2,4,6,0,1,-2},
				 {3,5,2,4,6,1,0,-2},
				 {3,5,1,4,6,2,0,-2},
				 {3,6,2,4,5,0,1,-2},
				 {3,6,2,4,5,1,0,-2},
				 {3,6,1,4,5,2,0,-2},
				 {4,5,2,3,6,0,1,-2},
				 {4,5,2,3,6,1,0,-2},
				 {4,5,1,3,6,2,0,-2},
				 {4,6,2,3,5,0,1,-2},
				 {4,6,2,3,5,1,0,-2},
				 {4,6,1,3,5,2,0,-2},
				 {5,6,2,3,4,0,1,-2},
				 {5,6,2,3,4,1,0,-2},
				 {5,6,1,3,4,2,0,-2},
				 {3,4,0,5,6,2,1,-2},
				 {3,4,1,5,6,2,0,-2},
				 {3,4,2,5,6,1,0,-2},
				 {3,5,0,4,6,2,1,-2},
				 {3,5,1,4,6,2,0,-2},
				 {3,5,2,4,6,1,0,-2},
				 {3,6,0,4,5,2,1,-2},
				 {3,6,1,4,5,2,0,-2},
				 {3,6,2,4,5,1,0,-2},
				 {4,5,0,3,6,2,1,-2},
				 {4,5,1,3,6,2,0,-2},
				 {4,5,2,3,6,1,0,-2},
				 {4,6,0,3,5,2,1,-2},
				 {4,6,1,3,5,2,0,-2},
				 {4,6,2,3,5,1,0,-2},
				 {5,6,0,3,4,2,1,-2},
				 {5,6,1,3,4,2,0,-2},
				 {5,6,2,3,4,1,0,-2}};
      perms = vec;
    }
    else if(nlost==2 && lost[0]==1 && lost[1]==2){ //3w2h1t - qbar1,b1
      vector<vector<int>> vec = {{3,-2,-2,5,6,0,1,2},
				 {3,-2,-2,5,6,1,0,2},
				 {3,-2,-2,5,6,2,0,1},
				 {3,-2,-2,4,6,0,1,2},
				 {3,-2,-2,4,6,1,0,2},
				 {3,-2,-2,4,6,2,0,1},
				 {3,-2,-2,4,5,0,1,2},
				 {3,-2,-2,4,5,1,0,2},
				 {3,-2,-2,4,5,2,0,1},
				 {4,-2,-2,5,6,0,1,2},
				 {4,-2,-2,5,6,1,0,2},
				 {4,-2,-2,5,6,2,0,1},
				 {4,-2,-2,3,6,0,1,2},
				 {4,-2,-2,3,6,1,0,2},
				 {4,-2,-2,3,6,2,0,1},
				 {4,-2,-2,3,5,0,1,2},
				 {4,-2,-2,3,5,1,0,2},
				 {4,-2,-2,3,5,2,0,1},
				 {5,-2,-2,4,6,0,1,2},
				 {5,-2,-2,4,6,1,0,2},
				 {5,-2,-2,4,6,2,0,1},
				 {5,-2,-2,3,6,0,1,2},
				 {5,-2,-2,3,6,1,0,2},
				 {5,-2,-2,3,6,2,0,1},
				 {5,-2,-2,3,4,0,1,2},
				 {5,-2,-2,3,4,1,0,2},
				 {5,-2,-2,3,4,2,0,1},
				 {6,-2,-2,4,5,0,1,2},
				 {6,-2,-2,4,5,1,0,2},
				 {6,-2,-2,4,5,2,0,1},
				 {6,-2,-2,3,5,0,1,2},
				 {6,-2,-2,3,5,1,0,2},
				 {6,-2,-2,3,5,2,0,1},
				 {6,-2,-2,3,4,0,1,2},
				 {6,-2,-2,3,4,1,0,2},
				 {6,-2,-2,3,4,2,0,1}};
      perms = vec;
    }
    else if(nlost==5 && lost[0]==0 && lost[1]==1 && lost[2]==3 &&
	    lost[3]==4 && lost[4]==2 ){ //0w0w2h1t - q1,qbar1,q2,qbar2,b1
      vector<vector<int>> vec = {{-2,-2,-2,-2,-2,0,1,2},
				 {-2,-2,-2,-2,-2,1,0,2},
				 {-2,-2,-2,-2,-2,2,0,1}};
      perms = vec;
    }
    else cout << "7j,3b "<< nlost <<" lost not defined" << endl;
  }
  else if(nq==5 && nb==4){ //9j,4b
    if(nlost==0){ //4w2h2t
      vector<vector<int>> vec = {{4,5,0,6,7,1,2,3,8},
				 {4,5,0,6,7,2,1,3,8},
				 {4,5,0,6,7,3,1,2,8},
				 {4,5,0,6,8,1,2,3,7},
				 {4,5,0,6,8,2,1,3,7},
				 {4,5,0,6,8,3,1,2,7},
				 {4,5,0,7,8,1,2,3,6},
				 {4,5,0,7,8,2,1,3,6},
				 {4,5,0,7,8,3,1,2,6},
				 {4,5,1,6,7,0,2,3,8},
				 {4,5,1,6,7,2,0,3,8},
				 {4,5,1,6,7,3,0,2,8},
				 {4,5,1,6,8,0,2,3,7},
				 {4,5,1,6,8,2,0,3,7},
				 {4,5,1,6,8,3,0,2,7},
				 {4,5,1,7,8,0,2,3,6},
				 {4,5,1,7,8,2,0,3,6},
				 {4,5,1,7,8,3,0,2,6},
				 {4,5,2,6,7,0,1,3,8},
				 {4,5,2,6,7,1,0,3,8},
				 {4,5,2,6,7,3,0,1,8},
				 {4,5,2,6,8,0,1,3,7},
				 {4,5,2,6,8,1,0,3,7},
				 {4,5,2,6,8,3,0,1,7},
				 {4,5,2,7,8,0,1,3,6},
				 {4,5,2,7,8,1,0,3,6},
				 {4,5,2,7,8,3,0,1,6},
				 {4,5,3,6,7,0,1,2,8},
				 {4,5,3,6,7,1,0,2,8},
				 {4,5,3,6,7,2,0,1,8},
				 {4,5,3,6,8,0,1,2,7},
				 {4,5,3,6,8,1,0,2,7},
				 {4,5,3,6,8,2,0,1,7},
				 {4,5,3,7,8,0,1,2,6},
				 {4,5,3,7,8,1,0,2,6},
				 {4,5,3,7,8,2,0,1,6},
				 {4,6,0,5,7,1,2,3,8},
				 {4,6,0,5,7,2,1,3,8},
				 {4,6,0,5,7,3,1,2,8},
				 {4,6,0,5,8,1,2,3,7},
				 {4,6,0,5,8,2,1,3,7},
				 {4,6,0,5,8,3,1,2,7},
				 {4,6,0,7,8,1,2,3,5},
				 {4,6,0,7,8,2,1,3,5},
				 {4,6,0,7,8,3,1,2,5},
				 {4,6,1,5,7,0,2,3,8},
				 {4,6,1,5,7,2,0,3,8},
				 {4,6,1,5,7,3,0,2,8},
				 {4,6,1,5,8,0,2,3,7},
				 {4,6,1,5,8,2,0,3,7},
				 {4,6,1,5,8,3,0,2,7},
				 {4,6,1,7,8,0,2,3,5},
				 {4,6,1,7,8,2,0,3,5},
				 {4,6,1,7,8,3,0,2,5},
				 {4,6,2,5,7,0,1,3,8},
				 {4,6,2,5,7,1,0,3,8},
				 {4,6,2,5,7,3,0,1,8},
				 {4,6,2,5,8,0,1,3,7},
				 {4,6,2,5,8,1,0,3,7},
				 {4,6,2,5,8,3,0,1,7},
				 {4,6,2,7,8,0,1,3,5},
				 {4,6,2,7,8,1,0,3,5},
				 {4,6,2,7,8,3,0,1,5},
				 {4,6,3,5,7,0,1,2,8},
				 {4,6,3,5,7,1,0,2,8},
				 {4,6,3,5,7,2,0,1,8},
				 {4,6,3,5,8,0,1,2,7},
				 {4,6,3,5,8,1,0,2,7},
				 {4,6,3,5,8,2,0,1,7},
				 {4,6,3,7,8,0,1,2,5},
				 {4,6,3,7,8,1,0,2,5},
				 {4,6,3,7,8,2,0,1,5},
				 {4,7,0,5,6,1,2,3,8},
				 {4,7,0,5,6,2,1,3,8},
				 {4,7,0,5,6,3,1,2,8},
				 {4,7,0,5,8,1,2,3,6},
				 {4,7,0,5,8,2,1,3,6},
				 {4,7,0,5,8,3,1,2,6},
				 {4,7,0,6,8,1,2,3,5},
				 {4,7,0,6,8,2,1,3,5},
				 {4,7,0,6,8,3,1,2,5},
				 {4,7,1,5,6,0,2,3,8},
				 {4,7,1,5,6,2,0,3,8},
				 {4,7,1,5,6,3,0,2,8},
				 {4,7,1,5,8,0,2,3,6},
				 {4,7,1,5,8,2,0,3,6},
				 {4,7,1,5,8,3,0,2,6},
				 {4,7,1,6,8,0,2,3,5},
				 {4,7,1,6,8,2,0,3,5},
				 {4,7,1,6,8,3,0,2,5},
				 {4,7,2,5,6,0,1,3,8},
				 {4,7,2,5,6,1,0,3,8},
				 {4,7,2,5,6,3,0,1,8},
				 {4,7,2,5,8,0,1,3,6},
				 {4,7,2,5,8,1,0,3,6},
				 {4,7,2,5,8,3,0,1,6},
				 {4,7,2,6,8,0,1,3,5},
				 {4,7,2,6,8,1,0,3,5},
				 {4,7,2,6,8,3,0,1,5},
				 {4,7,3,5,6,0,1,2,8},
				 {4,7,3,5,6,1,0,2,8},
				 {4,7,3,5,6,2,0,1,8},
				 {4,7,3,5,8,0,1,2,6},
				 {4,7,3,5,8,1,0,2,6},
				 {4,7,3,5,8,2,0,1,6},
				 {4,7,3,6,8,0,1,2,5},
				 {4,7,3,6,8,1,0,2,5},
				 {4,7,3,6,8,2,0,1,5},
				 {4,8,0,5,6,1,2,3,7},
				 {4,8,0,5,6,2,1,3,7},
				 {4,8,0,5,6,3,1,2,7},
				 {4,8,0,5,7,1,2,3,6},
				 {4,8,0,5,7,2,1,3,6},
				 {4,8,0,5,7,3,1,2,6},
				 {4,8,0,6,7,1,2,3,5},
				 {4,8,0,6,7,2,1,3,5},
				 {4,8,0,6,7,3,1,2,5},
				 {4,8,1,5,6,0,2,3,7},
				 {4,8,1,5,6,2,0,3,7},
				 {4,8,1,5,6,3,0,2,7},
				 {4,8,1,5,7,0,2,3,6},
				 {4,8,1,5,7,2,0,3,6},
				 {4,8,1,5,7,3,0,2,6},
				 {4,8,1,6,7,0,2,3,5},
				 {4,8,1,6,7,2,0,3,5},
				 {4,8,1,6,7,3,0,2,5},
				 {4,8,2,5,6,0,1,3,7},
				 {4,8,2,5,6,1,0,3,7},
				 {4,8,2,5,6,3,0,1,7},
				 {4,8,2,5,7,0,1,3,6},
				 {4,8,2,5,7,1,0,3,6},
				 {4,8,2,5,7,3,0,1,6},
				 {4,8,2,6,7,0,1,3,5},
				 {4,8,2,6,7,1,0,3,5},
				 {4,8,2,6,7,3,0,1,5},
				 {4,8,3,5,6,0,1,2,7},
				 {4,8,3,5,6,1,0,2,7},
				 {4,8,3,5,6,2,0,1,7},
				 {4,8,3,5,7,0,1,2,6},
				 {4,8,3,5,7,1,0,2,6},
				 {4,8,3,5,7,2,0,1,6},
				 {4,8,3,6,7,0,1,2,5},
				 {4,8,3,6,7,1,0,2,5},
				 {4,8,3,6,7,2,0,1,5},
				 {5,6,0,4,7,1,2,3,8},
				 {5,6,0,4,7,2,1,3,8},
				 {5,6,0,4,7,3,1,2,8},
				 {5,6,0,4,8,1,2,3,7},
				 {5,6,0,4,8,2,1,3,7},
				 {5,6,0,4,8,3,1,2,7},
				 {5,6,0,7,8,1,2,3,4},
				 {5,6,0,7,8,2,1,3,4},
				 {5,6,0,7,8,3,1,2,4},
				 {5,6,1,4,7,0,2,3,8},
				 {5,6,1,4,7,2,0,3,8},
				 {5,6,1,4,7,3,0,2,8},
				 {5,6,1,4,8,0,2,3,7},
				 {5,6,1,4,8,2,0,3,7},
				 {5,6,1,4,8,3,0,2,7},
				 {5,6,1,7,8,0,2,3,4},
				 {5,6,1,7,8,2,0,3,4},
				 {5,6,1,7,8,3,0,2,4},
				 {5,6,2,4,7,0,1,3,8},
				 {5,6,2,4,7,1,0,3,8},
				 {5,6,2,4,7,3,0,1,8},
				 {5,6,2,4,8,0,1,3,7},
				 {5,6,2,4,8,1,0,3,7},
				 {5,6,2,4,8,3,0,1,7},
				 {5,6,2,7,8,0,1,3,4},
				 {5,6,2,7,8,1,0,3,4},
				 {5,6,2,7,8,3,0,1,4},
				 {5,6,3,4,7,0,1,2,8},
				 {5,6,3,4,7,1,0,2,8},
				 {5,6,3,4,7,2,0,1,8},
				 {5,6,3,4,8,0,1,2,7},
				 {5,6,3,4,8,1,0,2,7},
				 {5,6,3,4,8,2,0,1,7},
				 {5,6,3,7,8,0,1,2,4},
				 {5,6,3,7,8,1,0,2,4},
				 {5,6,3,7,8,2,0,1,4},
				 {5,7,0,4,6,1,2,3,8},
				 {5,7,0,4,6,2,1,3,8},
				 {5,7,0,4,6,3,1,2,8},
				 {5,7,0,4,8,1,2,3,6},
				 {5,7,0,4,8,2,1,3,6},
				 {5,7,0,4,8,3,1,2,6},
				 {5,7,0,6,8,1,2,3,4},
				 {5,7,0,6,8,2,1,3,4},
				 {5,7,0,6,8,3,1,2,4},
				 {5,7,1,4,6,0,2,3,8},
				 {5,7,1,4,6,2,0,3,8},
				 {5,7,1,4,6,3,0,2,8},
				 {5,7,1,4,8,0,2,3,6},
				 {5,7,1,4,8,2,0,3,6},
				 {5,7,1,4,8,3,0,2,6},
				 {5,7,1,6,8,0,2,3,4},
				 {5,7,1,6,8,2,0,3,4},
				 {5,7,1,6,8,3,0,2,4},
				 {5,7,2,4,6,0,1,3,8},
				 {5,7,2,4,6,1,0,3,8},
				 {5,7,2,4,6,3,0,1,8},
				 {5,7,2,4,8,0,1,3,6},
				 {5,7,2,4,8,1,0,3,6},
				 {5,7,2,4,8,3,0,1,6},
				 {5,7,2,6,8,0,1,3,4},
				 {5,7,2,6,8,1,0,3,4},
				 {5,7,2,6,8,3,0,1,4},
				 {5,7,3,4,6,0,1,2,8},
				 {5,7,3,4,6,1,0,2,8},
				 {5,7,3,4,6,2,0,1,8},
				 {5,7,3,4,8,0,1,2,6},
				 {5,7,3,4,8,1,0,2,6},
				 {5,7,3,4,8,2,0,1,6},
				 {5,7,3,6,8,0,1,2,4},
				 {5,7,3,6,8,1,0,2,4},
				 {5,7,3,6,8,2,0,1,4},
				 {5,8,0,4,6,1,2,3,7},
				 {5,8,0,4,6,2,1,3,7},
				 {5,8,0,4,6,3,1,2,7},
				 {5,8,0,4,7,1,2,3,6},
				 {5,8,0,4,7,2,1,3,6},
				 {5,8,0,4,7,3,1,2,6},
				 {5,8,0,6,7,1,2,3,4},
				 {5,8,0,6,7,2,1,3,4},
				 {5,8,0,6,7,3,1,2,4},
				 {5,8,1,4,6,0,2,3,7},
				 {5,8,1,4,6,2,0,3,7},
				 {5,8,1,4,6,3,0,2,7},
				 {5,8,1,4,7,0,2,3,6},
				 {5,8,1,4,7,2,0,3,6},
				 {5,8,1,4,7,3,0,2,6},
				 {5,8,1,6,7,0,2,3,4},
				 {5,8,1,6,7,2,0,3,4},
				 {5,8,1,6,7,3,0,2,4},
				 {5,8,2,4,6,0,1,3,7},
				 {5,8,2,4,6,1,0,3,7},
				 {5,8,2,4,6,3,0,1,7},
				 {5,8,2,4,7,0,1,3,6},
				 {5,8,2,4,7,1,0,3,6},
				 {5,8,2,4,7,3,0,1,6},
				 {5,8,2,6,7,0,1,3,4},
				 {5,8,2,6,7,1,0,3,4},
				 {5,8,2,6,7,3,0,1,4},
				 {5,8,3,4,6,0,1,2,7},
				 {5,8,3,4,6,1,0,2,7},
				 {5,8,3,4,6,2,0,1,7},
				 {5,8,3,4,7,0,1,2,6},
				 {5,8,3,4,7,1,0,2,6},
				 {5,8,3,4,7,2,0,1,6},
				 {5,8,3,6,7,0,1,2,4},
				 {5,8,3,6,7,1,0,2,4},
				 {5,8,3,6,7,2,0,1,4},
				 {6,7,0,4,5,1,2,3,8},
				 {6,7,0,4,5,2,1,3,8},
				 {6,7,0,4,5,3,1,2,8},
				 {6,7,0,4,8,1,2,3,5},
				 {6,7,0,4,8,2,1,3,5},
				 {6,7,0,4,8,3,1,2,5},
				 {6,7,0,5,8,1,2,3,4},
				 {6,7,0,5,8,2,1,3,4},
				 {6,7,0,5,8,3,1,2,4},
				 {6,7,1,4,5,0,2,3,8},
				 {6,7,1,4,5,2,0,3,8},
				 {6,7,1,4,5,3,0,2,8},
				 {6,7,1,4,8,0,2,3,5},
				 {6,7,1,4,8,2,0,3,5},
				 {6,7,1,4,8,3,0,2,5},
				 {6,7,1,5,8,0,2,3,4},
				 {6,7,1,5,8,2,0,3,4},
				 {6,7,1,5,8,3,0,2,4},
				 {6,7,2,4,5,0,1,3,8},
				 {6,7,2,4,5,1,0,3,8},
				 {6,7,2,4,5,3,0,1,8},
				 {6,7,2,4,8,0,1,3,5},
				 {6,7,2,4,8,1,0,3,5},
				 {6,7,2,4,8,3,0,1,5},
				 {6,7,2,5,8,0,1,3,4},
				 {6,7,2,5,8,1,0,3,4},
				 {6,7,2,5,8,3,0,1,4},
				 {6,7,3,4,5,0,1,2,8},
				 {6,7,3,4,5,1,0,2,8},
				 {6,7,3,4,5,2,0,1,8},
				 {6,7,3,4,8,0,1,2,5},
				 {6,7,3,4,8,1,0,2,5},
				 {6,7,3,4,8,2,0,1,5},
				 {6,7,3,5,8,0,1,2,4},
				 {6,7,3,5,8,1,0,2,4},
				 {6,7,3,5,8,2,0,1,4},
				 {6,8,0,4,5,1,2,3,7},
				 {6,8,0,4,5,2,1,3,7},
				 {6,8,0,4,5,3,1,2,7},
				 {6,8,0,4,7,1,2,3,5},
				 {6,8,0,4,7,2,1,3,5},
				 {6,8,0,4,7,3,1,2,5},
				 {6,8,0,5,7,1,2,3,4},
				 {6,8,0,5,7,2,1,3,4},
				 {6,8,0,5,7,3,1,2,4},
				 {6,8,1,4,5,0,2,3,7},
				 {6,8,1,4,5,2,0,3,7},
				 {6,8,1,4,5,3,0,2,7},
				 {6,8,1,4,7,0,2,3,5},
				 {6,8,1,4,7,2,0,3,5},
				 {6,8,1,4,7,3,0,2,5},
				 {6,8,1,5,7,0,2,3,4},
				 {6,8,1,5,7,2,0,3,4},
				 {6,8,1,5,7,3,0,2,4},
				 {6,8,2,4,5,0,1,3,7},
				 {6,8,2,4,5,1,0,3,7},
				 {6,8,2,4,5,3,0,1,7},
				 {6,8,2,4,7,0,1,3,5},
				 {6,8,2,4,7,1,0,3,5},
				 {6,8,2,4,7,3,0,1,5},
				 {6,8,2,5,7,0,1,3,4},
				 {6,8,2,5,7,1,0,3,4},
				 {6,8,2,5,7,3,0,1,4},
				 {6,8,3,4,5,0,1,2,7},
				 {6,8,3,4,5,1,0,2,7},
				 {6,8,3,4,5,2,0,1,7},
				 {6,8,3,4,7,0,1,2,5},
				 {6,8,3,4,7,1,0,2,5},
				 {6,8,3,4,7,2,0,1,5},
				 {6,8,3,5,7,0,1,2,4},
				 {6,8,3,5,7,1,0,2,4},
				 {6,8,3,5,7,2,0,1,4},
				 {7,8,0,4,5,1,2,3,6},
				 {7,8,0,4,5,2,1,3,6},
				 {7,8,0,4,5,3,1,2,6},
				 {7,8,0,4,6,1,2,3,5},
				 {7,8,0,4,6,2,1,3,5},
				 {7,8,0,4,6,3,1,2,5},
				 {7,8,0,5,6,1,2,3,4},
				 {7,8,0,5,6,2,1,3,4},
				 {7,8,0,5,6,3,1,2,4},
				 {7,8,1,4,5,0,2,3,6},
				 {7,8,1,4,5,2,0,3,6},
				 {7,8,1,4,5,3,0,2,6},
				 {7,8,1,4,6,0,2,3,5},
				 {7,8,1,4,6,2,0,3,5},
				 {7,8,1,4,6,3,0,2,5},
				 {7,8,1,5,6,0,2,3,4},
				 {7,8,1,5,6,2,0,3,4},
				 {7,8,1,5,6,3,0,2,4},
				 {7,8,2,4,5,0,1,3,6},
				 {7,8,2,4,5,1,0,3,6},
				 {7,8,2,4,5,3,0,1,6},
				 {7,8,2,4,6,0,1,3,5},
				 {7,8,2,4,6,1,0,3,5},
				 {7,8,2,4,6,3,0,1,5},
				 {7,8,2,5,6,0,1,3,4},
				 {7,8,2,5,6,1,0,3,4},
				 {7,8,2,5,6,3,0,1,4},
				 {7,8,3,4,5,0,1,2,6},
				 {7,8,3,4,5,1,0,2,6},
				 {7,8,3,4,5,2,0,1,6},
				 {7,8,3,4,6,0,1,2,5},
				 {7,8,3,4,6,1,0,2,5},
				 {7,8,3,4,6,2,0,1,5},
				 {7,8,3,5,6,0,1,2,4},
				 {7,8,3,5,6,1,0,2,4},
				 {7,8,3,5,6,2,0,1,4}};
      perms = vec;
    }
    else if(nlost==1 && lost[0]==1){ //3w2h2t - qbar1
      vector<vector<int>> vec = {{4,-2,0,6,7,1,2,3,8},
				 {4,-2,0,6,7,2,1,3,8},
				 {4,-2,0,6,7,3,1,2,8},
				 {4,-2,0,6,8,1,2,3,7},
				 {4,-2,0,6,8,2,1,3,7},
				 {4,-2,0,6,8,3,1,2,7},
				 {4,-2,0,7,8,1,2,3,6},
				 {4,-2,0,7,8,2,1,3,6},
				 {4,-2,0,7,8,3,1,2,6},
				 {4,-2,1,6,7,0,2,3,8},
				 {4,-2,1,6,7,2,0,3,8},
				 {4,-2,1,6,7,3,0,2,8},
				 {4,-2,1,6,8,0,2,3,7},
				 {4,-2,1,6,8,2,0,3,7},
				 {4,-2,1,6,8,3,0,2,7},
				 {4,-2,1,7,8,0,2,3,6},
				 {4,-2,1,7,8,2,0,3,6},
				 {4,-2,1,7,8,3,0,2,6},
				 {4,-2,2,6,7,0,1,3,8},
				 {4,-2,2,6,7,1,0,3,8},
				 {4,-2,2,6,7,3,0,1,8},
				 {4,-2,2,6,8,0,1,3,7},
				 {4,-2,2,6,8,1,0,3,7},
				 {4,-2,2,6,8,3,0,1,7},
				 {4,-2,2,7,8,0,1,3,6},
				 {4,-2,2,7,8,1,0,3,6},
				 {4,-2,2,7,8,3,0,1,6},
				 {4,-2,3,6,7,0,1,2,8},
				 {4,-2,3,6,7,1,0,2,8},
				 {4,-2,3,6,7,2,0,1,8},
				 {4,-2,3,6,8,0,1,2,7},
				 {4,-2,3,6,8,1,0,2,7},
				 {4,-2,3,6,8,2,0,1,7},
				 {4,-2,3,7,8,0,1,2,6},
				 {4,-2,3,7,8,1,0,2,6},
				 {4,-2,3,7,8,2,0,1,6},
				 {4,-2,0,5,7,1,2,3,8},
				 {4,-2,0,5,7,2,1,3,8},
				 {4,-2,0,5,7,3,1,2,8},
				 {4,-2,0,5,8,1,2,3,7},
				 {4,-2,0,5,8,2,1,3,7},
				 {4,-2,0,5,8,3,1,2,7},
				 {4,-2,1,5,7,0,2,3,8},
				 {4,-2,1,5,7,2,0,3,8},
				 {4,-2,1,5,7,3,0,2,8},
				 {4,-2,1,5,8,0,2,3,7},
				 {4,-2,1,5,8,2,0,3,7},
				 {4,-2,1,5,8,3,0,2,7},
				 {4,-2,2,5,7,0,1,3,8},
				 {4,-2,2,5,7,1,0,3,8},
				 {4,-2,2,5,7,3,0,1,8},
				 {4,-2,2,5,8,0,1,3,7},
				 {4,-2,2,5,8,1,0,3,7},
				 {4,-2,2,5,8,3,0,1,7},
				 {4,-2,3,5,7,0,1,2,8},
				 {4,-2,3,5,7,1,0,2,8},
				 {4,-2,3,5,7,2,0,1,8},
				 {4,-2,3,5,8,0,1,2,7},
				 {4,-2,3,5,8,1,0,2,7},
				 {4,-2,3,5,8,2,0,1,7},
				 {4,-2,0,5,6,1,2,3,8},
				 {4,-2,0,5,6,2,1,3,8},
				 {4,-2,0,5,6,3,1,2,8},
				 {4,-2,1,5,6,0,2,3,8},
				 {4,-2,1,5,6,2,0,3,8},
				 {4,-2,1,5,6,3,0,2,8},
				 {4,-2,2,5,6,0,1,3,8},
				 {4,-2,2,5,6,1,0,3,8},
				 {4,-2,2,5,6,3,0,1,8},
				 {4,-2,3,5,6,0,1,2,8},
				 {4,-2,3,5,6,1,0,2,8},
				 {4,-2,3,5,6,2,0,1,8},
				 {5,-2,0,6,7,1,2,3,8},
				 {5,-2,0,6,7,2,1,3,8},
				 {5,-2,0,6,7,3,1,2,8},
				 {5,-2,0,6,8,1,2,3,7},
				 {5,-2,0,6,8,2,1,3,7},
				 {5,-2,0,6,8,3,1,2,7},
				 {5,-2,0,7,8,1,2,3,6},
				 {5,-2,0,7,8,2,1,3,6},
				 {5,-2,0,7,8,3,1,2,6},
				 {5,-2,1,6,7,0,2,3,8},
				 {5,-2,1,6,7,2,0,3,8},
				 {5,-2,1,6,7,3,0,2,8},
				 {5,-2,1,6,8,0,2,3,7},
				 {5,-2,1,6,8,2,0,3,7},
				 {5,-2,1,6,8,3,0,2,7},
				 {5,-2,1,7,8,0,2,3,6},
				 {5,-2,1,7,8,2,0,3,6},
				 {5,-2,1,7,8,3,0,2,6},
				 {5,-2,2,6,7,0,1,3,8},
				 {5,-2,2,6,7,1,0,3,8},
				 {5,-2,2,6,7,3,0,1,8},
				 {5,-2,2,6,8,0,1,3,7},
				 {5,-2,2,6,8,1,0,3,7},
				 {5,-2,2,6,8,3,0,1,7},
				 {5,-2,2,7,8,0,1,3,6},
				 {5,-2,2,7,8,1,0,3,6},
				 {5,-2,2,7,8,3,0,1,6},
				 {5,-2,3,6,7,0,1,2,8},
				 {5,-2,3,6,7,1,0,2,8},
				 {5,-2,3,6,7,2,0,1,8},
				 {5,-2,3,6,8,0,1,2,7},
				 {5,-2,3,6,8,1,0,2,7},
				 {5,-2,3,6,8,2,0,1,7},
				 {5,-2,3,7,8,0,1,2,6},
				 {5,-2,3,7,8,1,0,2,6},
				 {5,-2,3,7,8,2,0,1,6},
				 {5,-2,0,4,7,1,2,3,8},
				 {5,-2,0,4,7,2,1,3,8},
				 {5,-2,0,4,7,3,1,2,8},
				 {5,-2,0,4,8,1,2,3,7},
				 {5,-2,0,4,8,2,1,3,7},
				 {5,-2,0,4,8,3,1,2,7},
				 {5,-2,1,4,7,0,2,3,8},
				 {5,-2,1,4,7,2,0,3,8},
				 {5,-2,1,4,7,3,0,2,8},
				 {5,-2,1,4,8,0,2,3,7},
				 {5,-2,1,4,8,2,0,3,7},
				 {5,-2,1,4,8,3,0,2,7},
				 {5,-2,2,4,7,0,1,3,8},
				 {5,-2,2,4,7,1,0,3,8},
				 {5,-2,2,4,7,3,0,1,8},
				 {5,-2,2,4,8,0,1,3,7},
				 {5,-2,2,4,8,1,0,3,7},
				 {5,-2,2,4,8,3,0,1,7},
				 {5,-2,3,4,7,0,1,2,8},
				 {5,-2,3,4,7,1,0,2,8},
				 {5,-2,3,4,7,2,0,1,8},
				 {5,-2,3,4,8,0,1,2,7},
				 {5,-2,3,4,8,1,0,2,7},
				 {5,-2,3,4,8,2,0,1,7},
				 {5,-2,0,4,6,1,2,3,8},
				 {5,-2,0,4,6,2,1,3,8},
				 {5,-2,0,4,6,3,1,2,8},
				 {5,-2,1,4,6,0,2,3,8},
				 {5,-2,1,4,6,2,0,3,8},
				 {5,-2,1,4,6,3,0,2,8},
				 {5,-2,2,4,6,0,1,3,8},
				 {5,-2,2,4,6,1,0,3,8},
				 {5,-2,2,4,6,3,0,1,8},
				 {5,-2,3,4,6,0,1,2,8},
				 {5,-2,3,4,6,1,0,2,8},
				 {5,-2,3,4,6,2,0,1,8},
				 {6,-2,0,5,7,1,2,3,8},
				 {6,-2,0,5,7,2,1,3,8},
				 {6,-2,0,5,7,3,1,2,8},
				 {6,-2,0,5,8,1,2,3,7},
				 {6,-2,0,5,8,2,1,3,7},
				 {6,-2,0,5,8,3,1,2,7},
				 {6,-2,0,7,8,1,2,3,5},
				 {6,-2,0,7,8,2,1,3,5},
				 {6,-2,0,7,8,3,1,2,5},
				 {6,-2,1,5,7,0,2,3,8},
				 {6,-2,1,5,7,2,0,3,8},
				 {6,-2,1,5,7,3,0,2,8},
				 {6,-2,1,5,8,0,2,3,7},
				 {6,-2,1,5,8,2,0,3,7},
				 {6,-2,1,5,8,3,0,2,7},
				 {6,-2,1,7,8,0,2,3,5},
				 {6,-2,1,7,8,2,0,3,5},
				 {6,-2,1,7,8,3,0,2,5},
				 {6,-2,2,5,7,0,1,3,8},
				 {6,-2,2,5,7,1,0,3,8},
				 {6,-2,2,5,7,3,0,1,8},
				 {6,-2,2,5,8,0,1,3,7},
				 {6,-2,2,5,8,1,0,3,7},
				 {6,-2,2,5,8,3,0,1,7},
				 {6,-2,2,7,8,0,1,3,5},
				 {6,-2,2,7,8,1,0,3,5},
				 {6,-2,2,7,8,3,0,1,5},
				 {6,-2,3,5,7,0,1,2,8},
				 {6,-2,3,5,7,1,0,2,8},
				 {6,-2,3,5,7,2,0,1,8},
				 {6,-2,3,5,8,0,1,2,7},
				 {6,-2,3,5,8,1,0,2,7},
				 {6,-2,3,5,8,2,0,1,7},
				 {6,-2,3,7,8,0,1,2,5},
				 {6,-2,3,7,8,1,0,2,5},
				 {6,-2,3,7,8,2,0,1,5},
				 {6,-2,0,4,7,1,2,3,8},
				 {6,-2,0,4,7,2,1,3,8},
				 {6,-2,0,4,7,3,1,2,8},
				 {6,-2,0,4,8,1,2,3,7},
				 {6,-2,0,4,8,2,1,3,7},
				 {6,-2,0,4,8,3,1,2,7},
				 {6,-2,1,4,7,0,2,3,8},
				 {6,-2,1,4,7,2,0,3,8},
				 {6,-2,1,4,7,3,0,2,8},
				 {6,-2,1,4,8,0,2,3,7},
				 {6,-2,1,4,8,2,0,3,7},
				 {6,-2,1,4,8,3,0,2,7},
				 {6,-2,2,4,7,0,1,3,8},
				 {6,-2,2,4,7,1,0,3,8},
				 {6,-2,2,4,7,3,0,1,8},
				 {6,-2,2,4,8,0,1,3,7},
				 {6,-2,2,4,8,1,0,3,7},
				 {6,-2,2,4,8,3,0,1,7},
				 {6,-2,3,4,7,0,1,2,8},
				 {6,-2,3,4,7,1,0,2,8},
				 {6,-2,3,4,7,2,0,1,8},
				 {6,-2,3,4,8,0,1,2,7},
				 {6,-2,3,4,8,1,0,2,7},
				 {6,-2,3,4,8,2,0,1,7},
				 {6,-2,0,4,5,1,2,3,8},
				 {6,-2,0,4,5,2,1,3,8},
				 {6,-2,0,4,5,3,1,2,8},
				 {6,-2,1,4,5,0,2,3,8},
				 {6,-2,1,4,5,2,0,3,8},
				 {6,-2,1,4,5,3,0,2,8},
				 {6,-2,2,4,5,0,1,3,8},
				 {6,-2,2,4,5,1,0,3,8},
				 {6,-2,2,4,5,3,0,1,8},
				 {6,-2,3,4,5,0,1,2,8},
				 {6,-2,3,4,5,1,0,2,8},
				 {6,-2,3,4,5,2,0,1,8},
				 {7,-2,0,5,6,1,2,3,8},
				 {7,-2,0,5,6,2,1,3,8},
				 {7,-2,0,5,6,3,1,2,8},
				 {7,-2,0,5,8,1,2,3,6},
				 {7,-2,0,5,8,2,1,3,6},
				 {7,-2,0,5,8,3,1,2,6},
				 {7,-2,0,6,8,1,2,3,5},
				 {7,-2,0,6,8,2,1,3,5},
				 {7,-2,0,6,8,3,1,2,5},
				 {7,-2,1,5,6,0,2,3,8},
				 {7,-2,1,5,6,2,0,3,8},
				 {7,-2,1,5,6,3,0,2,8},
				 {7,-2,1,5,8,0,2,3,6},
				 {7,-2,1,5,8,2,0,3,6},
				 {7,-2,1,5,8,3,0,2,6},
				 {7,-2,1,6,8,0,2,3,5},
				 {7,-2,1,6,8,2,0,3,5},
				 {7,-2,1,6,8,3,0,2,5},
				 {7,-2,2,5,6,0,1,3,8},
				 {7,-2,2,5,6,1,0,3,8},
				 {7,-2,2,5,6,3,0,1,8},
				 {7,-2,2,5,8,0,1,3,6},
				 {7,-2,2,5,8,1,0,3,6},
				 {7,-2,2,5,8,3,0,1,6},
				 {7,-2,2,6,8,0,1,3,5},
				 {7,-2,2,6,8,1,0,3,5},
				 {7,-2,2,6,8,3,0,1,5},
				 {7,-2,3,5,6,0,1,2,8},
				 {7,-2,3,5,6,1,0,2,8},
				 {7,-2,3,5,6,2,0,1,8},
				 {7,-2,3,5,8,0,1,2,6},
				 {7,-2,3,5,8,1,0,2,6},
				 {7,-2,3,5,8,2,0,1,6},
				 {7,-2,3,6,8,0,1,2,5},
				 {7,-2,3,6,8,1,0,2,5},
				 {7,-2,3,6,8,2,0,1,5},
				 {7,-2,0,4,6,1,2,3,8},
				 {7,-2,0,4,6,2,1,3,8},
				 {7,-2,0,4,6,3,1,2,8},
				 {7,-2,0,4,8,1,2,3,6},
				 {7,-2,0,4,8,2,1,3,6},
				 {7,-2,0,4,8,3,1,2,6},
				 {7,-2,1,4,6,0,2,3,8},
				 {7,-2,1,4,6,2,0,3,8},
				 {7,-2,1,4,6,3,0,2,8},
				 {7,-2,1,4,8,0,2,3,6},
				 {7,-2,1,4,8,2,0,3,6},
				 {7,-2,1,4,8,3,0,2,6},
				 {7,-2,2,4,6,0,1,3,8},
				 {7,-2,2,4,6,1,0,3,8},
				 {7,-2,2,4,6,3,0,1,8},
				 {7,-2,2,4,8,0,1,3,6},
				 {7,-2,2,4,8,1,0,3,6},
				 {7,-2,2,4,8,3,0,1,6},
				 {7,-2,3,4,6,0,1,2,8},
				 {7,-2,3,4,6,1,0,2,8},
				 {7,-2,3,4,6,2,0,1,8},
				 {7,-2,3,4,8,0,1,2,6},
				 {7,-2,3,4,8,1,0,2,6},
				 {7,-2,3,4,8,2,0,1,6},
				 {7,-2,0,4,5,1,2,3,8},
				 {7,-2,0,4,5,2,1,3,8},
				 {7,-2,0,4,5,3,1,2,8},
				 {7,-2,1,4,5,0,2,3,8},
				 {7,-2,1,4,5,2,0,3,8},
				 {7,-2,1,4,5,3,0,2,8},
				 {7,-2,2,4,5,0,1,3,8},
				 {7,-2,2,4,5,1,0,3,8},
				 {7,-2,2,4,5,3,0,1,8},
				 {7,-2,3,4,5,0,1,2,8},
				 {7,-2,3,4,5,1,0,2,8},
				 {7,-2,3,4,5,2,0,1,8},
				 {8,-2,0,5,6,1,2,3,7},
				 {8,-2,0,5,6,2,1,3,7},
				 {8,-2,0,5,6,3,1,2,7},
				 {8,-2,0,5,7,1,2,3,6},
				 {8,-2,0,5,7,2,1,3,6},
				 {8,-2,0,5,7,3,1,2,6},
				 {8,-2,0,6,7,1,2,3,5},
				 {8,-2,0,6,7,2,1,3,5},
				 {8,-2,0,6,7,3,1,2,5},
				 {8,-2,1,5,6,0,2,3,7},
				 {8,-2,1,5,6,2,0,3,7},
				 {8,-2,1,5,6,3,0,2,7},
				 {8,-2,1,5,7,0,2,3,6},
				 {8,-2,1,5,7,2,0,3,6},
				 {8,-2,1,5,7,3,0,2,6},
				 {8,-2,1,6,7,0,2,3,5},
				 {8,-2,1,6,7,2,0,3,5},
				 {8,-2,1,6,7,3,0,2,5},
				 {8,-2,2,5,6,0,1,3,7},
				 {8,-2,2,5,6,1,0,3,7},
				 {8,-2,2,5,6,3,0,1,7},
				 {8,-2,2,5,7,0,1,3,6},
				 {8,-2,2,5,7,1,0,3,6},
				 {8,-2,2,5,7,3,0,1,6},
				 {8,-2,2,6,7,0,1,3,5},
				 {8,-2,2,6,7,1,0,3,5},
				 {8,-2,2,6,7,3,0,1,5},
				 {8,-2,3,5,6,0,1,2,7},
				 {8,-2,3,5,6,1,0,2,7},
				 {8,-2,3,5,6,2,0,1,7},
				 {8,-2,3,5,7,0,1,2,6},
				 {8,-2,3,5,7,1,0,2,6},
				 {8,-2,3,5,7,2,0,1,6},
				 {8,-2,3,6,7,0,1,2,5},
				 {8,-2,3,6,7,1,0,2,5},
				 {8,-2,3,6,7,2,0,1,5},
				 {8,-2,0,4,6,1,2,3,7},
				 {8,-2,0,4,6,2,1,3,7},
				 {8,-2,0,4,6,3,1,2,7},
				 {8,-2,0,4,7,1,2,3,6},
				 {8,-2,0,4,7,2,1,3,6},
				 {8,-2,0,4,7,3,1,2,6},
				 {8,-2,1,4,6,0,2,3,7},
				 {8,-2,1,4,6,2,0,3,7},
				 {8,-2,1,4,6,3,0,2,7},
				 {8,-2,1,4,7,0,2,3,6},
				 {8,-2,1,4,7,2,0,3,6},
				 {8,-2,1,4,7,3,0,2,6},
				 {8,-2,2,4,6,0,1,3,7},
				 {8,-2,2,4,6,1,0,3,7},
				 {8,-2,2,4,6,3,0,1,7},
				 {8,-2,2,4,7,0,1,3,6},
				 {8,-2,2,4,7,1,0,3,6},
				 {8,-2,2,4,7,3,0,1,6},
				 {8,-2,3,4,6,0,1,2,7},
				 {8,-2,3,4,6,1,0,2,7},
				 {8,-2,3,4,6,2,0,1,7},
				 {8,-2,3,4,7,0,1,2,6},
				 {8,-2,3,4,7,1,0,2,6},
				 {8,-2,3,4,7,2,0,1,6},
				 {8,-2,0,4,5,1,2,3,7},
				 {8,-2,0,4,5,2,1,3,7},
				 {8,-2,0,4,5,3,1,2,7},
				 {8,-2,1,4,5,0,2,3,7},
				 {8,-2,1,4,5,2,0,3,7},
				 {8,-2,1,4,5,3,0,2,7},
				 {8,-2,2,4,5,0,1,3,7},
				 {8,-2,2,4,5,1,0,3,7},
				 {8,-2,2,4,5,3,0,1,7},
				 {8,-2,3,4,5,0,1,2,7},
				 {8,-2,3,4,5,1,0,2,7},
				 {8,-2,3,4,5,2,0,1,7}};
      perms = vec;
    }
    else if(nlost==2 && lost[0]==0 && lost[1]==1){ //0w2w2h2t - q1,qbar1
      vector<vector<int>> vec = {{-2,-2,0,6,7,1,2,3,8},
				 {-2,-2,0,6,7,2,1,3,8},
				 {-2,-2,0,6,7,3,1,2,8},
				 {-2,-2,0,6,8,1,2,3,7},
				 {-2,-2,0,6,8,2,1,3,7},
				 {-2,-2,0,6,8,3,1,2,7},
				 {-2,-2,0,7,8,1,2,3,6},
				 {-2,-2,0,7,8,2,1,3,6},
				 {-2,-2,0,7,8,3,1,2,6},
				 {-2,-2,1,6,7,0,2,3,8},
				 {-2,-2,1,6,7,2,0,3,8},
				 {-2,-2,1,6,7,3,0,2,8},
				 {-2,-2,1,6,8,0,2,3,7},
				 {-2,-2,1,6,8,2,0,3,7},
				 {-2,-2,1,6,8,3,0,2,7},
				 {-2,-2,1,7,8,0,2,3,6},
				 {-2,-2,1,7,8,2,0,3,6},
				 {-2,-2,1,7,8,3,0,2,6},
				 {-2,-2,2,6,7,0,1,3,8},
				 {-2,-2,2,6,7,1,0,3,8},
				 {-2,-2,2,6,7,3,0,1,8},
				 {-2,-2,2,6,8,0,1,3,7},
				 {-2,-2,2,6,8,1,0,3,7},
				 {-2,-2,2,6,8,3,0,1,7},
				 {-2,-2,2,7,8,0,1,3,6},
				 {-2,-2,2,7,8,1,0,3,6},
				 {-2,-2,2,7,8,3,0,1,6},
				 {-2,-2,3,6,7,0,1,2,8},
				 {-2,-2,3,6,7,1,0,2,8},
				 {-2,-2,3,6,7,2,0,1,8},
				 {-2,-2,3,6,8,0,1,2,7},
				 {-2,-2,3,6,8,1,0,2,7},
				 {-2,-2,3,6,8,2,0,1,7},
				 {-2,-2,3,7,8,0,1,2,6},
				 {-2,-2,3,7,8,1,0,2,6},
				 {-2,-2,3,7,8,2,0,1,6},
				 {-2,-2,0,5,7,1,2,3,8},
				 {-2,-2,0,5,7,2,1,3,8},
				 {-2,-2,0,5,7,3,1,2,8},
				 {-2,-2,0,5,8,1,2,3,7},
				 {-2,-2,0,5,8,2,1,3,7},
				 {-2,-2,0,5,8,3,1,2,7},
				 {-2,-2,1,5,7,0,2,3,8},
				 {-2,-2,1,5,7,2,0,3,8},
				 {-2,-2,1,5,7,3,0,2,8},
				 {-2,-2,1,5,8,0,2,3,7},
				 {-2,-2,1,5,8,2,0,3,7},
				 {-2,-2,1,5,8,3,0,2,7},
				 {-2,-2,2,5,7,0,1,3,8},
				 {-2,-2,2,5,7,1,0,3,8},
				 {-2,-2,2,5,7,3,0,1,8},
				 {-2,-2,2,5,8,0,1,3,7},
				 {-2,-2,2,5,8,1,0,3,7},
				 {-2,-2,2,5,8,3,0,1,7},
				 {-2,-2,3,5,7,0,1,2,8},
				 {-2,-2,3,5,7,1,0,2,8},
				 {-2,-2,3,5,7,2,0,1,8},
				 {-2,-2,3,5,8,0,1,2,7},
				 {-2,-2,3,5,8,1,0,2,7},
				 {-2,-2,3,5,8,2,0,1,7},
				 {-2,-2,0,5,6,1,2,3,8},
				 {-2,-2,0,5,6,2,1,3,8},
				 {-2,-2,0,5,6,3,1,2,8},
				 {-2,-2,1,5,6,0,2,3,8},
				 {-2,-2,1,5,6,2,0,3,8},
				 {-2,-2,1,5,6,3,0,2,8},
				 {-2,-2,2,5,6,0,1,3,8},
				 {-2,-2,2,5,6,1,0,3,8},
				 {-2,-2,2,5,6,3,0,1,8},
				 {-2,-2,3,5,6,0,1,2,8},
				 {-2,-2,3,5,6,1,0,2,8},
				 {-2,-2,3,5,6,2,0,1,8},
				 {-2,-2,0,4,7,1,2,3,8},
				 {-2,-2,0,4,7,2,1,3,8},
				 {-2,-2,0,4,7,3,1,2,8},
				 {-2,-2,0,4,8,1,2,3,7},
				 {-2,-2,0,4,8,2,1,3,7},
				 {-2,-2,0,4,8,3,1,2,7},
				 {-2,-2,1,4,7,0,2,3,8},
				 {-2,-2,1,4,7,2,0,3,8},
				 {-2,-2,1,4,7,3,0,2,8},
				 {-2,-2,1,4,8,0,2,3,7},
				 {-2,-2,1,4,8,2,0,3,7},
				 {-2,-2,1,4,8,3,0,2,7},
				 {-2,-2,2,4,7,0,1,3,8},
				 {-2,-2,2,4,7,1,0,3,8},
				 {-2,-2,2,4,7,3,0,1,8},
				 {-2,-2,2,4,8,0,1,3,7},
				 {-2,-2,2,4,8,1,0,3,7},
				 {-2,-2,2,4,8,3,0,1,7},
				 {-2,-2,3,4,7,0,1,2,8},
				 {-2,-2,3,4,7,1,0,2,8},
				 {-2,-2,3,4,7,2,0,1,8},
				 {-2,-2,3,4,8,0,1,2,7},
				 {-2,-2,3,4,8,1,0,2,7},
				 {-2,-2,3,4,8,2,0,1,7},
				 {-2,-2,0,4,6,1,2,3,8},
				 {-2,-2,0,4,6,2,1,3,8},
				 {-2,-2,0,4,6,3,1,2,8},
				 {-2,-2,1,4,6,0,2,3,8},
				 {-2,-2,1,4,6,2,0,3,8},
				 {-2,-2,1,4,6,3,0,2,8},
				 {-2,-2,2,4,6,0,1,3,8},
				 {-2,-2,2,4,6,1,0,3,8},
				 {-2,-2,2,4,6,3,0,1,8},
				 {-2,-2,3,4,6,0,1,2,8},
				 {-2,-2,3,4,6,1,0,2,8},
				 {-2,-2,3,4,6,2,0,1,8},
				 {-2,-2,0,4,5,1,2,3,8},
				 {-2,-2,0,4,5,2,1,3,8},
				 {-2,-2,0,4,5,3,1,2,8},
				 {-2,-2,1,4,5,0,2,3,8},
				 {-2,-2,1,4,5,2,0,3,8},
				 {-2,-2,1,4,5,3,0,2,8},
				 {-2,-2,2,4,5,0,1,3,8},
				 {-2,-2,2,4,5,1,0,3,8},
				 {-2,-2,2,4,5,3,0,1,8},
				 {-2,-2,3,4,5,0,1,2,8},
				 {-2,-2,3,4,5,1,0,2,8},
				 {-2,-2,3,4,5,2,0,1,8}};
      perms = vec;
    }
    else if(nlost==2 && lost[0]==1 && lost[1]==4){ //1w1w2h2t - qbar1,qbar2
      vector<vector<int>> vec = {{4,-2,0,6,-2,1,2,3,8},
				 {4,-2,0,6,-2,2,1,3,8},
				 {4,-2,0,6,-2,3,1,2,8},
				 {4,-2,0,7,-2,1,2,3,8},
				 {4,-2,0,7,-2,2,1,3,8},
				 {4,-2,0,7,-2,3,1,2,8},
				 {4,-2,0,8,-2,1,2,3,7},
				 {4,-2,0,8,-2,2,1,3,7},
				 {4,-2,0,8,-2,3,1,2,7},
				 {4,-2,1,6,-2,0,2,3,8},
				 {4,-2,1,6,-2,2,0,3,8},
				 {4,-2,1,6,-2,3,0,2,8},
				 {4,-2,1,7,-2,0,2,3,8},
				 {4,-2,1,7,-2,2,0,3,8},
				 {4,-2,1,7,-2,3,0,2,8},
				 {4,-2,1,8,-2,0,2,3,7},
				 {4,-2,1,8,-2,2,0,3,7},
				 {4,-2,1,8,-2,3,0,2,7},
				 {4,-2,2,6,-2,0,1,3,8},
				 {4,-2,2,6,-2,1,0,3,8},
				 {4,-2,2,6,-2,3,0,1,8},
				 {4,-2,2,7,-2,0,1,3,8},
				 {4,-2,2,7,-2,1,0,3,8},
				 {4,-2,2,7,-2,3,0,1,8},
				 {4,-2,2,8,-2,0,1,3,7},
				 {4,-2,2,8,-2,1,0,3,7},
				 {4,-2,2,8,-2,3,0,1,7},
				 {4,-2,3,6,-2,0,1,2,8},
				 {4,-2,3,6,-2,1,0,2,8},
				 {4,-2,3,6,-2,2,0,1,8},
				 {4,-2,3,7,-2,0,1,2,8},
				 {4,-2,3,7,-2,1,0,2,8},
				 {4,-2,3,7,-2,2,0,1,8},
				 {4,-2,3,8,-2,0,1,2,7},
				 {4,-2,3,8,-2,1,0,2,7},
				 {4,-2,3,8,-2,2,0,1,7},
				 {4,-2,0,5,-2,1,2,3,8},
				 {4,-2,0,5,-2,2,1,3,8},
				 {4,-2,0,5,-2,3,1,2,8},
				 {4,-2,1,5,-2,0,2,3,8},
				 {4,-2,1,5,-2,2,0,3,8},
				 {4,-2,1,5,-2,3,0,2,8},
				 {4,-2,2,5,-2,0,1,3,8},
				 {4,-2,2,5,-2,1,0,3,8},
				 {4,-2,2,5,-2,3,0,1,8},
				 {4,-2,3,5,-2,0,1,2,8},
				 {4,-2,3,5,-2,1,0,2,8},
				 {4,-2,3,5,-2,2,0,1,8},
				 {5,-2,0,6,-2,1,2,3,8},
				 {5,-2,0,6,-2,2,1,3,8},
				 {5,-2,0,6,-2,3,1,2,8},
				 {5,-2,0,7,-2,1,2,3,8},
				 {5,-2,0,7,-2,2,1,3,8},
				 {5,-2,0,7,-2,3,1,2,8},
				 {5,-2,0,8,-2,1,2,3,7},
				 {5,-2,0,8,-2,2,1,3,7},
				 {5,-2,0,8,-2,3,1,2,7},
				 {5,-2,1,6,-2,0,2,3,8},
				 {5,-2,1,6,-2,2,0,3,8},
				 {5,-2,1,6,-2,3,0,2,8},
				 {5,-2,1,7,-2,0,2,3,8},
				 {5,-2,1,7,-2,2,0,3,8},
				 {5,-2,1,7,-2,3,0,2,8},
				 {5,-2,1,8,-2,0,2,3,7},
				 {5,-2,1,8,-2,2,0,3,7},
				 {5,-2,1,8,-2,3,0,2,7},
				 {5,-2,2,6,-2,0,1,3,8},
				 {5,-2,2,6,-2,1,0,3,8},
				 {5,-2,2,6,-2,3,0,1,8},
				 {5,-2,2,7,-2,0,1,3,8},
				 {5,-2,2,7,-2,1,0,3,8},
				 {5,-2,2,7,-2,3,0,1,8},
				 {5,-2,2,8,-2,0,1,3,7},
				 {5,-2,2,8,-2,1,0,3,7},
				 {5,-2,2,8,-2,3,0,1,7},
				 {5,-2,3,6,-2,0,1,2,8},
				 {5,-2,3,6,-2,1,0,2,8},
				 {5,-2,3,6,-2,2,0,1,8},
				 {5,-2,3,7,-2,0,1,2,8},
				 {5,-2,3,7,-2,1,0,2,8},
				 {5,-2,3,7,-2,2,0,1,8},
				 {5,-2,3,8,-2,0,1,2,7},
				 {5,-2,3,8,-2,1,0,2,7},
				 {5,-2,3,8,-2,2,0,1,7},
				 {5,-2,0,4,-2,1,2,3,8},
				 {5,-2,0,4,-2,2,1,3,8},
				 {5,-2,0,4,-2,3,1,2,8},
				 {5,-2,1,4,-2,0,2,3,8},
				 {5,-2,1,4,-2,2,0,3,8},
				 {5,-2,1,4,-2,3,0,2,8},
				 {5,-2,2,4,-2,0,1,3,8},
				 {5,-2,2,4,-2,1,0,3,8},
				 {5,-2,2,4,-2,3,0,1,8},
				 {5,-2,3,4,-2,0,1,2,8},
				 {5,-2,3,4,-2,1,0,2,8},
				 {5,-2,3,4,-2,2,0,1,8},
				 {6,-2,0,5,-2,1,2,3,8},
				 {6,-2,0,5,-2,2,1,3,8},
				 {6,-2,0,5,-2,3,1,2,8},
				 {6,-2,0,7,-2,1,2,3,8},
				 {6,-2,0,7,-2,2,1,3,8},
				 {6,-2,0,7,-2,3,1,2,8},
				 {6,-2,0,8,-2,1,2,3,7},
				 {6,-2,0,8,-2,2,1,3,7},
				 {6,-2,0,8,-2,3,1,2,7},
				 {6,-2,1,5,-2,0,2,3,8},
				 {6,-2,1,5,-2,2,0,3,8},
				 {6,-2,1,5,-2,3,0,2,8},
				 {6,-2,1,7,-2,0,2,3,8},
				 {6,-2,1,7,-2,2,0,3,8},
				 {6,-2,1,7,-2,3,0,2,8},
				 {6,-2,1,8,-2,0,2,3,7},
				 {6,-2,1,8,-2,2,0,3,7},
				 {6,-2,1,8,-2,3,0,2,7},
				 {6,-2,2,5,-2,0,1,3,8},
				 {6,-2,2,5,-2,1,0,3,8},
				 {6,-2,2,5,-2,3,0,1,8},
				 {6,-2,2,7,-2,0,1,3,8},
				 {6,-2,2,7,-2,1,0,3,8},
				 {6,-2,2,7,-2,3,0,1,8},
				 {6,-2,2,8,-2,0,1,3,7},
				 {6,-2,2,8,-2,1,0,3,7},
				 {6,-2,2,8,-2,3,0,1,7},
				 {6,-2,3,5,-2,0,1,2,8},
				 {6,-2,3,5,-2,1,0,2,8},
				 {6,-2,3,5,-2,2,0,1,8},
				 {6,-2,3,7,-2,0,1,2,8},
				 {6,-2,3,7,-2,1,0,2,8},
				 {6,-2,3,7,-2,2,0,1,8},
				 {6,-2,3,8,-2,0,1,2,7},
				 {6,-2,3,8,-2,1,0,2,7},
				 {6,-2,3,8,-2,2,0,1,7},
				 {6,-2,0,4,-2,1,2,3,8},
				 {6,-2,0,4,-2,2,1,3,8},
				 {6,-2,0,4,-2,3,1,2,8},
				 {6,-2,1,4,-2,0,2,3,8},
				 {6,-2,1,4,-2,2,0,3,8},
				 {6,-2,1,4,-2,3,0,2,8},
				 {6,-2,2,4,-2,0,1,3,8},
				 {6,-2,2,4,-2,1,0,3,8},
				 {6,-2,2,4,-2,3,0,1,8},
				 {6,-2,3,4,-2,0,1,2,8},
				 {6,-2,3,4,-2,1,0,2,8},
				 {6,-2,3,4,-2,2,0,1,8},
				 {7,-2,0,5,-2,1,2,3,8},
				 {7,-2,0,5,-2,2,1,3,8},
				 {7,-2,0,5,-2,3,1,2,8},
				 {7,-2,0,6,-2,1,2,3,8},
				 {7,-2,0,6,-2,2,1,3,8},
				 {7,-2,0,6,-2,3,1,2,8},
				 {7,-2,0,8,-2,1,2,3,6},
				 {7,-2,0,8,-2,2,1,3,6},
				 {7,-2,0,8,-2,3,1,2,6},
				 {7,-2,1,5,-2,0,2,3,8},
				 {7,-2,1,5,-2,2,0,3,8},
				 {7,-2,1,5,-2,3,0,2,8},
				 {7,-2,1,6,-2,0,2,3,8},
				 {7,-2,1,6,-2,2,0,3,8},
				 {7,-2,1,6,-2,3,0,2,8},
				 {7,-2,1,8,-2,0,2,3,6},
				 {7,-2,1,8,-2,2,0,3,6},
				 {7,-2,1,8,-2,3,0,2,6},
				 {7,-2,2,5,-2,0,1,3,8},
				 {7,-2,2,5,-2,1,0,3,8},
				 {7,-2,2,5,-2,3,0,1,8},
				 {7,-2,2,6,-2,0,1,3,8},
				 {7,-2,2,6,-2,1,0,3,8},
				 {7,-2,2,6,-2,3,0,1,8},
				 {7,-2,2,8,-2,0,1,3,6},
				 {7,-2,2,8,-2,1,0,3,6},
				 {7,-2,2,8,-2,3,0,1,6},
				 {7,-2,3,5,-2,0,1,2,8},
				 {7,-2,3,5,-2,1,0,2,8},
				 {7,-2,3,5,-2,2,0,1,8},
				 {7,-2,3,6,-2,0,1,2,8},
				 {7,-2,3,6,-2,1,0,2,8},
				 {7,-2,3,6,-2,2,0,1,8},
				 {7,-2,3,8,-2,0,1,2,6},
				 {7,-2,3,8,-2,1,0,2,6},
				 {7,-2,3,8,-2,2,0,1,6},
				 {7,-2,0,4,-2,1,2,3,8},
				 {7,-2,0,4,-2,2,1,3,8},
				 {7,-2,0,4,-2,3,1,2,8},
				 {7,-2,1,4,-2,0,2,3,8},
				 {7,-2,1,4,-2,2,0,3,8},
				 {7,-2,1,4,-2,3,0,2,8},
				 {7,-2,2,4,-2,0,1,3,8},
				 {7,-2,2,4,-2,1,0,3,8},
				 {7,-2,2,4,-2,3,0,1,8},
				 {7,-2,3,4,-2,0,1,2,8},
				 {7,-2,3,4,-2,1,0,2,8},
				 {7,-2,3,4,-2,2,0,1,8},
				 {8,-2,0,5,-2,1,2,3,7},
				 {8,-2,0,5,-2,2,1,3,7},
				 {8,-2,0,5,-2,3,1,2,7},
				 {8,-2,0,6,-2,1,2,3,7},
				 {8,-2,0,6,-2,2,1,3,7},
				 {8,-2,0,6,-2,3,1,2,7},
				 {8,-2,0,7,-2,1,2,3,6},
				 {8,-2,0,7,-2,2,1,3,6},
				 {8,-2,0,7,-2,3,1,2,6},
				 {8,-2,1,5,-2,0,2,3,7},
				 {8,-2,1,5,-2,2,0,3,7},
				 {8,-2,1,5,-2,3,0,2,7},
				 {8,-2,1,6,-2,0,2,3,7},
				 {8,-2,1,6,-2,2,0,3,7},
				 {8,-2,1,6,-2,3,0,2,7},
				 {8,-2,1,7,-2,0,2,3,6},
				 {8,-2,1,7,-2,2,0,3,6},
				 {8,-2,1,7,-2,3,0,2,6},
				 {8,-2,2,5,-2,0,1,3,7},
				 {8,-2,2,5,-2,1,0,3,7},
				 {8,-2,2,5,-2,3,0,1,7},
				 {8,-2,2,6,-2,0,1,3,7},
				 {8,-2,2,6,-2,1,0,3,7},
				 {8,-2,2,6,-2,3,0,1,7},
				 {8,-2,2,7,-2,0,1,3,6},
				 {8,-2,2,7,-2,1,0,3,6},
				 {8,-2,2,7,-2,3,0,1,6},
				 {8,-2,3,5,-2,0,1,2,7},
				 {8,-2,3,5,-2,1,0,2,7},
				 {8,-2,3,5,-2,2,0,1,7},
				 {8,-2,3,6,-2,0,1,2,7},
				 {8,-2,3,6,-2,1,0,2,7},
				 {8,-2,3,6,-2,2,0,1,7},
				 {8,-2,3,7,-2,0,1,2,6},
				 {8,-2,3,7,-2,1,0,2,6},
				 {8,-2,3,7,-2,2,0,1,6},
				 {8,-2,0,4,-2,1,2,3,7},
				 {8,-2,0,4,-2,2,1,3,7},
				 {8,-2,0,4,-2,3,1,2,7},
				 {8,-2,1,4,-2,0,2,3,7},
				 {8,-2,1,4,-2,2,0,3,7},
				 {8,-2,1,4,-2,3,0,2,7},
				 {8,-2,2,4,-2,0,1,3,7},
				 {8,-2,2,4,-2,1,0,3,7},
				 {8,-2,2,4,-2,3,0,1,7},
				 {8,-2,3,4,-2,0,1,2,7},
				 {8,-2,3,4,-2,1,0,2,7},
				 {8,-2,3,4,-2,2,0,1,7}};
      perms = vec;
    }
    else if(nlost==4 && lost[0]==0 && lost[1]==1 && lost[2]==3 &&
	    lost[3]==4){ //0w0w2h2t - q1,qbar1,q2,qbar2
      vector<vector<int>> vec = {{-2,-2,0,-2,-2,1,2,3,8},
				 {-2,-2,0,-2,-2,2,1,3,8},
				 {-2,-2,0,-2,-2,3,1,2,8},
				 {-2,-2,1,-2,-2,0,2,3,8},
				 {-2,-2,1,-2,-2,2,0,3,8},
				 {-2,-2,1,-2,-2,3,0,2,8},
				 {-2,-2,2,-2,-2,0,1,3,8},
				 {-2,-2,2,-2,-2,1,0,3,8},
				 {-2,-2,2,-2,-2,3,0,1,8},
				 {-2,-2,3,-2,-2,0,1,2,8},
				 {-2,-2,3,-2,-2,1,0,2,8},
				 {-2,-2,3,-2,-2,2,0,1,8}};
      perms = vec;
    }
    else if(nlost==5 && lost[0]==0 && lost[1]==1 && lost[2]==3 &&
	    lost[3]==4 && lost[4]==2 ){ //0w0w2h1t - q1,qbar1,q2,qbar2,b1
      vector<vector<int>> vec = {{-2,-2,-2,-2,-2,1,2,3,8},
				 {-2,-2,-2,-2,-2,2,1,3,8},
				 {-2,-2,-2,-2,-2,3,1,2,8},
				 {-2,-2,-2,-2,-2,0,2,3,8},
				 {-2,-2,-2,-2,-2,2,0,3,8},
				 {-2,-2,-2,-2,-2,3,0,2,8},
				 {-2,-2,-2,-2,-2,0,1,3,8},
				 {-2,-2,-2,-2,-2,1,0,3,8},
				 {-2,-2,-2,-2,-2,3,0,1,8},
				 {-2,-2,-2,-2,-2,0,1,2,8},
				 {-2,-2,-2,-2,-2,1,0,2,8},
				 {-2,-2,-2,-2,-2,2,0,1,8}};
      perms = vec;
    }
    else cout << "9j,4b "<< nlost <<" lost not defined" << endl; 
  }
  else cout << "perms for "<< nq+nb <<" quarks not defined" << endl;
  
  return perms;
}