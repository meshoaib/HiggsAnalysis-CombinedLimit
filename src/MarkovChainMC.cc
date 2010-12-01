#include "HiggsAnalysis/CombinedLimit/interface/MarkovChainMC.h"
#include "RooRealVar.h"
#include "RooArgSet.h"
#include "RooUniform.h"
#include "RooWorkspace.h"
#include "RooFitResult.h"
#include "RooStats/MCMCCalculator.h"
#include "RooStats/MCMCInterval.h"
#include "RooStats/ModelConfig.h"
#include "RooStats/ProposalHelper.h"
#include "RooStats/ProposalFunction.h"
#include "HiggsAnalysis/CombinedLimit/interface/Combine.h"

using namespace RooStats;

MarkovChainMC::MarkovChainMC() : 
    LimitAlgo("Markov Chain MC specific options") 
{
    options_.add_options()
        ("uniformProposal,u", "Uniform proposal")
        ("iteration,i", boost::program_options::value<unsigned int>(&iterations_)->default_value(20000), "Number of iterations")
        ("burnInSteps,b", boost::program_options::value<unsigned int>(&burnInSteps_)->default_value(500), "Burn in steps")
        ("nBins,B", boost::program_options::value<unsigned int>(&numberOfBins_)->default_value(1000), "Number of bins")
    ;
}

void MarkovChainMC::applyOptions(const boost::program_options::variables_map &vm) {
    uniformProposal_ = vm.count("uniformProposal");
}
bool MarkovChainMC::run(RooWorkspace *w, RooAbsData &data, double &limit) {
  RooRealVar *r = w->var("r");
  RooArgSet  poi(*r);
  RooArgSet const &obs = *w->set("observables");
  
  RooUniform  flatPrior("flatPrior","flatPrior",*r);
  RooFitResult *fit = w->pdf("model_s")->fitTo(data, RooFit::Save());
  if (fit == 0) { std::cerr << "Fit failed." << std::endl; return false; }
  fit->Print("V");
  w->loadSnapshot("clean");

  if (withSystematics && (w->set("nuisances") == 0)) {
    std::cerr << "ERROR: nuisances not set. Perhaps you wanted to run with no systematics?\n" << std::endl;
    abort();
  }
  
  ModelConfig modelConfig("sb_model", w);
  modelConfig.SetPdf(*w->pdf("model_s"));
  modelConfig.SetObservables(obs);
  modelConfig.SetParametersOfInterest(poi);
  if (withSystematics) modelConfig.SetNuisanceParameters(*w->set("nuisances"));
  
  ProposalHelper ph;
  ph.SetVariables((RooArgSet&)fit->floatParsFinal());
  ph.SetCovMatrix(fit->covarianceMatrix());
  ph.SetUpdateProposalParameters(true);
  ph.SetCacheSize(100);
  ProposalFunction* pdfProp = ph.GetProposalFunction();  // that was easyA
  if (uniformProposal_) { pdfProp = new UniformProposal(); } // might do this in a cleaner way in the future
  
  MCMCCalculator mc(data, modelConfig);
  mc.SetNumIters(iterations_); 
  mc.SetConfidenceLevel(cl);
  mc.SetNumBurnInSteps(burnInSteps_); 
  mc.SetProposalFunction(*pdfProp);
  mc.SetNumBins (numberOfBins_) ; // bins to use for RooRealVars in histograms
  mc.SetLeftSideTailFraction(0);
  mc.SetPriorPdf(flatPrior);
  
  MCMCInterval* mcInt = (MCMCInterval*)mc.GetInterval(); 
  if (mcInt == 0) return false;
  limit = mcInt->UpperLimit(*r);
  if (verbose > 0) {
    std::cout << "\n -- MCMC, flat prior -- " << "\n";
    std::cout << "Limit: r < " << limit << " @ " << cl * 100 << "% CL" << std::endl;
    std::cout << "Interval:    [ " << mcInt->LowerLimit(*r)             << " , " << mcInt->UpperLimit(*r)             << " ] @ " << cl * 100 << "% CL" << std::endl;
    std::cout << "Interval DH: [ " << mcInt->LowerLimitByDataHist(*r)   << " , " << mcInt->UpperLimitByDataHist(*r)   << " ] @ " << cl * 100 << "% CL" << std::endl;
    std::cout << "Interval H:  [ " << mcInt->LowerLimitByHist(*r)       << " , " << mcInt->UpperLimitByHist(*r)       << " ] @ " << cl * 100 << "% CL" << std::endl;
    //std::cout << "Interval K:  [ " << mcInt->LowerLimitByKeys(*r)       << " , " << mcInt->UpperLimitByKeys(*r)       << " ] @ " << cl * 100 << "% CL" << std::endl;
    std::cout << "Interval S:  [ " << mcInt->LowerLimitShortest(*r)     << " , " << mcInt->UpperLimitShortest(*r)     << " ] @ " << cl * 100 << "% CL" << std::endl;
    std::cout << "Interval TF: [ " << mcInt->LowerLimitTailFraction(*r) << " , " << mcInt->UpperLimitTailFraction(*r) << " ] @ " << cl * 100 << "% CL" << std::endl;
  }
  return true;
}
