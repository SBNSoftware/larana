////////////////////////////////////////////////////////////////////////
//
//  AlgoSlidingWindow source
//
////////////////////////////////////////////////////////////////////////

#include "fhiclcpp/ParameterSet.h"

#include "AlgoSlidingWindow.h"

namespace pmtana{

  //*********************************************************************
  AlgoSlidingWindow::AlgoSlidingWindow(const std::string name)
    : PMTPulseRecoBase(name)
  //*********************************************************************
  {}

  //*********************************************************************
  AlgoSlidingWindow::AlgoSlidingWindow(const fhicl::ParameterSet &pset,
  //AlgoSlidingWindow::AlgoSlidingWindow(const ::fcllite::PSet &pset,
				       const std::string name)
    : PMTPulseRecoBase(name)
  //*********************************************************************
  {
    _positive = pset.get<bool>("PositivePolarity",true);

    _adc_thres = pset.get<float>("ADCThreshold");

    _end_adc_thres = pset.get<float>("EndADCThreshold");

    _nsigma = pset.get<float>("NSigmaThreshold");

    _end_nsigma = pset.get<float>("EndNSigmaThreshold");

    _verbose = pset.get<bool>("Verbosity");

    _num_presample = pset.get<size_t>("NumPreSample");

    Reset();

  }

  //***************************************************************
  void AlgoSlidingWindow::Reset()
  //***************************************************************
  {
    PMTPulseRecoBase::Reset();
  }

  //***************************************************************
  bool AlgoSlidingWindow::RecoPulse(const pmtana::Waveform_t& wf,
				    const pmtana::PedestalMean_t& mean_v,
				    const pmtana::PedestalSigma_t& sigma_v)
  //***************************************************************
  {

    bool fire = false;

    bool in_tail = false;

    double pulse_start_threshold=0;
    double pulse_tail_threshold =0;

    double pulse_start_baseline =0;

    //double threshold = ( _adc_thres > (_nsigma * _ped_rms) ? _adc_thres : (_nsigma * _ped_rms) );

    //threshold += _ped_mean;

    Reset();

    for(size_t i=0; i<wf.size(); ++i) {

      double value = 0.;
      if(_positive) value = ((double)(wf[i])) -  mean_v.at(i);
      else value = mean_v.at(i) - ((double)(wf[i]));

      float start_threshold = 0.;

      if(sigma_v.at(i) * _nsigma < _adc_thres) start_threshold = _adc_thres;
      else start_threshold = sigma_v.at(i) * _nsigma;

      // End pulse if significantly high peak found (new pulse)
      if( (!fire || in_tail) && ((double)value > start_threshold) ) {

	// If there's a pulse, end it
	if(in_tail) {
	  _pulse.t_end = i - 1;

	  _pulse_v.push_back(_pulse);

	  _pulse.reset_param();

	  if(_verbose)
	    std::cout << "\033[93mPulse End\033[00m: "
		      << "baseline: " << mean_v[i] << " ... " << " ... adc above: " << value << " T=" << i << std::endl;
	}

	//
	// Found a new pulse ... try to get a few samples prior to this
	//

	pulse_start_threshold = start_threshold;
	pulse_start_baseline  = mean_v.at(i);

	pulse_tail_threshold = 0.;
	if(sigma_v.at(i) * _end_nsigma < _end_adc_thres) pulse_tail_threshold = _end_adc_thres;
	else pulse_tail_threshold = sigma_v.at(i) * _end_nsigma;

	int last_pulse_end_index = 0;
	if(_pulse_v.size()) last_pulse_end_index = _pulse_v.back().t_end;
	int buffer_num_index = (int)i - last_pulse_end_index;
	if(buffer_num_index > (int)_num_presample) buffer_num_index = _num_presample;

	if(buffer_num_index<0) {
	  std::cerr << "\033[95m[ERROR]\033[00m Logic error! Negative buffer_num_index..." << std::endl;
	  throw std::exception();
	}

	_pulse.t_start   = i - buffer_num_index;
	_pulse.ped_mean  = pulse_start_baseline;
	_pulse.ped_sigma = sigma_v.at(i);

	for(size_t pre_index=_pulse.t_start; pre_index<i; ++pre_index) {

	  double pre_adc = wf[pre_index];
	  if(_positive) pre_adc -= pulse_start_baseline;
	  else pre_adc = pulse_start_baseline - pre_adc;

	  if(pre_adc > 0.) _pulse.area += pre_adc;
	}

	if(_verbose)
	  std::cout << "\033[93mPulse Start\033[00m: "
		    << "baseline: " << mean_v[i]
		    << " ... threshold: " << start_threshold
		    << " ... adc above baseline: " << value
		    << " ... pre-adc sum: " << _pulse.area
		    << " T=" << i << std::endl;

	fire = true;
	in_tail = false;
      }

      if( (fire || in_tail) && _verbose ) {
	std::cout << (fire ? "\033[93mPulsing\033[00m: " : "\033[93mPulse ending\033[00m: ")
		  << "baseline: " << mean_v[i]
		  << " std: " << sigma_v[i]
		  << " ... adc above baseline " << value
		  << " T=" << i << std::endl;

      }

      if( fire && value < pulse_start_threshold ) {
	fire = false;
	in_tail = true;
      }

      if( (fire || in_tail) && value < pulse_tail_threshold ){

	// Found the end of a pulse
	_pulse.t_end = i - 1;

	_pulse_v.push_back(_pulse);

	if(_verbose)
	  std::cout << "\033[93mPulse End\033[00m: "
		    << "baseline: " << mean_v[i] << " ... adc: " << value << " T=" << i << " ... area sum " << _pulse.area << std::endl;

	_pulse.reset_param();

	fire = false;
	in_tail = false;

      }

      if(fire || in_tail){

	//_pulse.area += ((double)value - (double)mean_v.at(i));
	_pulse.area += value;

	if(_pulse.peak < value) {

	  // Found a new maximum
	  _pulse.peak = value;

	  _pulse.t_max = i;

	}

      }

    }

    if(fire || in_tail){

      // Take care of a pulse that did not finish within the readout window.

      fire = false;
      in_tail = false;

      _pulse.t_end = wf.size() - 1;

      _pulse_v.push_back(_pulse);

      _pulse.reset_param();

    }

    return true;

  }

}
