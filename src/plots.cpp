#include <algorithm>
#include <iomanip>
#include <iostream>
#include <set>
#include <map>
#include <string>
#include <sstream>
#include <vector>
#include <TCanvas.h>
#include <TColor.h>
#include <TFile.h>
#include <TH1.h>
#include <TH1F.h>
#include <TH2F.h>
#include <TH3F.h>
#include <TNtuple.h>
#include <TLegend.h>

#include <sxmc/plots.h>
#include <sxmc/signal.h>
#include <sxmc/observable.h>
#include <sxmc/systematic.h>

const int ncolors = 6;
const int colors[6] = {
  kRed, kRed, kBlack, kBlack, kBlue, kGreen+1
};
const int styles[6] = {
  1, 2, 1, 2, 3, 1
};


SpectralPlot::SpectralPlot(int _line_width, float _xmin, float _xmax,
                           float _ymin, float _ymax, bool _logy,
                           std::string _title,
                           std::string _xtitle,
                           std::string _ytitle)
    : logy(_logy), line_width(_line_width),
      xmin(_xmin), xmax(_xmax), ymin(_ymin), ymax(_ymax),
      title(_title), xtitle(_xtitle), ytitle(_ytitle) {
  this->c = new TCanvas();
  this->c->SetCanvasSize(500, 500);

  if (this->logy) {
    this->c->SetLogy();
  }

  this->c->SetRightMargin(0.18);

  this->legend = new TLegend(0.85, 0.1, 0.995, 0.9);
  this->legend->SetBorderSize(0);
  this->legend->SetFillColor(kWhite);
}


SpectralPlot::SpectralPlot(const SpectralPlot& o) {
  this->logy = o.logy;
  this->line_width = o.line_width;
  this->xmin = o.xmin;
  this->xmax = o.xmax;
  this->ymin = o.ymin;
  this->ymax = o.ymax;
  this->title = o.title;
  this->xtitle = o.xtitle;
  this->ytitle = o.ytitle;
  for (size_t i=0; i<o.histograms.size(); i++) {
    this->histograms.push_back(o.histograms[i]);
  }
  this->c = new TCanvas();
  this->c->SetCanvasSize(500, 500);

  if (o.logy) {
    this->c->SetLogy();
  }

  this->c->SetRightMargin(o.c->GetRightMargin());

  this->legend = (TLegend*) o.legend->Clone("");
}


SpectralPlot::~SpectralPlot() {
  for (size_t i=0; i<this->histograms.size(); i++) {
    delete this->histograms[i];
  }
  delete this->legend;
}


void SpectralPlot::add(TH1* _h, std::string objname, std::string title, std::string options) {
  TH1* h = dynamic_cast<TH1*>(_h->Clone(("__" + objname).c_str()));
  h->SetDirectory(NULL);

  h->SetLineWidth(this->line_width);
  h->SetTitle(this->title.c_str());
  h->SetXTitle(xtitle.c_str());
  h->SetYTitle(ytitle.c_str());

  this->legend->AddEntry(h, title.c_str());

  if (!h || h->Integral() == 0) {
    return;
  }

  this->histograms.push_back(h);

  if (this->histograms.size() == 1) {
    if (!(this->ymin == -1 && this->ymax == -1)) {
      h->SetAxisRange(this->ymin, this->ymax, "Y");
    }
    h->SetAxisRange(this->xmin, this->xmax, "X");
    h->GetXaxis()->SetRangeUser(this->xmin, this->xmax);
    h->GetXaxis()->SetLabelFont(132);
    h->GetXaxis()->SetTitleFont(132);
    h->GetYaxis()->SetLabelFont(132);
    h->GetYaxis()->SetTitleFont(132);
    if (this->logy) {
      this->c->SetLogy();
    }
    this->c->cd();
    h->DrawClone(options.c_str());
  }
  else {
    this->c->cd();
    h->DrawClone(("same " + options).c_str());
  }
  this->c->Update();
}


void SpectralPlot::save(std::string filename) {
  this->c->cd();
  this->legend->SetTextFont(132);
  this->legend->Draw();
  this->c->Update();
  this->c->SaveAs((filename + ".pdf").c_str(), "q");
  this->c->SaveAs((filename + ".png").c_str(), "q");
  this->c->SaveAs((filename + ".tex").c_str(), "q");
  this->c->SaveAs((filename + ".C").c_str(), "q");
  this->c->SaveAs((filename + ".root").c_str(), "q");
}


TH1* SpectralPlot::make_like(TH1* h, std::string name) {
  TH1* hnew = dynamic_cast<TH1*>(h->Clone(name.c_str()));
  hnew->Reset();
  return hnew;
}


void plot_fit(std::map<std::string, Interval> best_fit, float live_time,
              std::vector<Source>& sources,
              std::vector<Signal>& signals,
              std::vector<Systematic>& systematics,
              std::vector<Observable>& observables,
              std::set<unsigned>& datasets,
              std::vector<float>& data,
              std::string output_path) {
  std::map<unsigned, std::vector<SpectralPlot> > all_plots;
  std::map<unsigned, std::vector<TH1F*> > all_totals;
  std::map<unsigned, TH1*> all_totals_nd;

  for (std::set<unsigned>::iterator it=datasets.begin();
       it!=datasets.end(); ++it) {
    unsigned dataset = *it;

    std::vector<SpectralPlot> plots;
    std::vector<TH1F*> totals(observables.size(), NULL);

    // Set up plots for each observable
    for (size_t i=0; i<observables.size(); i++) {
      Observable* o = &observables[i];
      std::stringstream ytitle;
      ytitle << "Events/" << std::setprecision(3)
             << (o->upper - o->lower) / o->bins << " " << o->units
             << "/" << live_time << " y";
      plots.push_back(SpectralPlot(2, o->lower, o->upper,
                      o->yrange[0], o->yrange[1],
                      o->logscale, "", o->title, ytitle.str().c_str()));
    }
    all_plots[dataset] = plots;
    all_totals[dataset] = totals;
  }

  // Write best-fit histograms to a file
  std::ostringstream pdf_output_path;
  pdf_output_path << output_path << "fit_pdfs.root";
  TFile fpdfs(pdf_output_path.str().c_str(), "recreate");

  // Extract best-fit parameter values
  std::cout << "plot_fit: Best fit" << std::endl;
  std::vector<float> params(best_fit.size());

  for (size_t i=0; i<sources.size(); i++) {
    std::string name = sources[i].name;
    float bf = best_fit[name].point_estimate;
    params[i] = bf;
    std::cout << name << " " << bf << std::endl;
  }

  size_t idx = sources.size();
  for (size_t i=0; i<systematics.size(); i++) {
    for (size_t j=0; j<systematics[i].npars; j++) {
      std::ostringstream oss;
      oss << systematics[i].name << "_" << j;
      float bf = best_fit[oss.str()].point_estimate;
      params[idx++] = bf;
      std::cout << oss.str() << " " << bf << std::endl;
    }
  }

  hemi::Array<unsigned> norms_buffer(signals.size(), true);
  norms_buffer.writeOnlyHostPtr();

  hemi::Array<double> param_buffer(params.size(), true);
  for (size_t i=0; i<params.size(); i++) {
    param_buffer.writeOnlyHostPtr()[i] = params[i];
  }

  for (size_t i=0; i<signals.size(); i++) {
    pdfz::EvalHist* phist = \
      dynamic_cast<pdfz::EvalHist*>(signals[i].histogram);

    phist->SetParameterBuffer(&param_buffer, sources.size());
    phist->SetNormalizationBuffer(&norms_buffer, i);

    phist->EvalAsync(false);
    phist->EvalFinished();

    double eff = 1.0 * norms_buffer.readOnlyHostPtr()[i] / signals[i].n_mc;
    double nexp = signals[i].nexpected * eff * params[signals[i].source.index];

    TH1* hpdf_nd = phist->CreateHistogram();
    hpdf_nd->Scale(nexp / hpdf_nd->Integral());

    std::vector<TH1D*> hpdf(observables.size(), NULL);
    if (hpdf_nd->IsA() == TH1D::Class()) {
      hpdf[0] = dynamic_cast<TH1D*>(hpdf_nd);
    }
    else if (hpdf_nd->IsA() == TH2D::Class()) {
      hpdf[0] = dynamic_cast<TH2D*>(hpdf_nd)->ProjectionX("hpdf_x");
      hpdf[1] = dynamic_cast<TH2D*>(hpdf_nd)->ProjectionY("hpdf_y");
    }
    else if (hpdf_nd->IsA() == TH3D::Class()) {
      hpdf[0] = dynamic_cast<TH3D*>(hpdf_nd)->ProjectionX("hpdf_x");
      hpdf[1] = dynamic_cast<TH3D*>(hpdf_nd)->ProjectionY("hpdf_y");
      hpdf[2] = dynamic_cast<TH3D*>(hpdf_nd)->ProjectionZ("hpdf_z");
    }
    else {
      std::cerr << "SpectralPlot::plot_fit: Can't create projection in >3 "
                << "observable dimensions." << std::endl;
      assert(false);  
    }

    unsigned ds = signals[i].dataset;

    unsigned ns = signals.size() / datasets.size();
    unsigned ii = i % ns;
    int color = colors[ii % ncolors];
    int style = styles[ii % ncolors];

    if (all_totals_nd[ds] == NULL) {
      std::ostringstream ss;
      ss << "htotal_" << ds;
      all_totals_nd[ds] = (TH1*) hpdf_nd->Clone(ss.str().c_str());
    }
    else {
      all_totals_nd[ds]->Add(hpdf_nd);
    }

    for (size_t j=0; j<observables.size(); j++) {
      hpdf[j]->SetLineColor(color);
      hpdf[j]->SetLineStyle(style);

      all_plots[ds][j].add(hpdf[j], signals[i].name, signals[i].title, "hist");

      if (all_totals[ds][j] == NULL) {
        std::ostringstream ss;
        ss << "htotal_" << ds << observables[j].name;
        all_totals[ds][j] = (TH1F*) hpdf[j]->Clone(ss.str().c_str());
        all_totals[ds][j]->SetLineColor(kMagenta);
        all_totals[ds][j]->SetLineStyle(1);
      }
      else if (hpdf[j] && hpdf[j]->Integral() > 0) {
        all_totals[ds][j]->Add(hpdf[j]);
      }
    }
  }

  for (std::set<unsigned>::iterator it=datasets.begin();
       it!=datasets.end(); ++it) {
    unsigned ds = *it;

    for (size_t i=0; i<observables.size(); i++) {
      TH1D* hdata = (TH1D*) SpectralPlot::make_like(
          all_plots[ds][i].histograms[0], "hdata");

      hdata->SetMarkerStyle(20);
      hdata->SetMarkerSize(0.7);
      hdata->SetLineColor(kBlack);
      hdata->SetLineStyle(1);

      for (size_t idata=0; idata<data.size()/(observables.size()+1); idata++) {
        unsigned dds = data[idata * (observables.size() + 1) + observables.size()];
        if (dds == ds) {
          hdata->Fill(data[idata * (observables.size() + 1) + i]);
        }
      }

      all_plots[ds][i].add(all_totals[ds][i], "fit", "Fit", "hist");
      all_plots[ds][i].add(hdata, "data", "Data");

      std::ostringstream output;
      output << output_path << observables[i].name << "_" << ds;
      all_plots[ds][i].save(output.str());

      all_totals_nd[ds]->Write();
    }
  }

  fpdfs.Close();
}

