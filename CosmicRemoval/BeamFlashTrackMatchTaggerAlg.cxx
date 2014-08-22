/*!
 * Title:   Beam Flash<-->Track Match Algorithim Class
 * Author:  Wes Ketchum (wketchum@lanl.gov), based on code from Ben Jones
 *
 * Description: Algorithm that compares all tracks to the flash during the 
 *              beam gate, and determines if that track is consistent with
 *              having produced that flash.
 * Input:       recob::OpFlash, recob::Track
 * Output:      anab::CosmicTag (and Assn<anab::CosmicTag,recob::Track>) 
*/

#include "BeamFlashTrackMatchTaggerAlg.h"
#include <limits>
#include "TVector3.h"

cosmic::BeamFlashTrackMatchTaggerAlg::BeamFlashTrackMatchTaggerAlg(fhicl::ParameterSet const& p) 
  : COSMIC_TYPE(10),
    DEBUG_FLAG(p.get<bool>("RunDebugMode",false))
{
  this->reconfigure(p);
}

void cosmic::BeamFlashTrackMatchTaggerAlg::reconfigure(fhicl::ParameterSet const& p){
  fMIPYield   = p.get<float>("MIPYield",24000);
  fQE         = p.get<float>("QE",0.01);
  fMIPdQdx    = p.get<float>("MIPdQdx",2.1);
  fPromptFrac = p.get<float>("PromptFrac",0.25);

  fSingleChannelCut           = p.get<float>("SingleChannelCut");
  fCumulativeChannelThreshold = p.get<float>("CumulativeChannelThreshold");
  fCumulativeChannelCut       = p.get<unsigned int>("CumulativeChannelCut");
  fIntegralCut                = p.get<float>("IntegralCut");
}

void cosmic::BeamFlashTrackMatchTaggerAlg::RunCompatibilityCheck(std::vector<recob::OpFlash> const& flashVector,
								 std::vector<recob::Track> const& trackVector,
								 std::vector<anab::CosmicTag>& cosmicTagVector,
								 std::vector<size_t>& assnTrackTagVector,
								 geo::Geometry const& geom,
								 phot::PhotonVisibilityService const& pvs){

  std::vector< const recob::OpFlash* > flashesOnBeamTime;
  for(auto const& flash : flashVector){
    if(!flash.OnBeamTime()) continue;
    flashesOnBeamTime.push_back(&flash);
  }

  for(size_t track_i=0; track_i<trackVector.size(); track_i++){

    recob::Track const& track(trackVector[track_i]);

    TVector3 const& pt_begin = track.LocationAtPoint(0);
    TVector3 const& pt_end = track.LocationAtPoint(track.NumberTrajectoryPoints()-1);
    if(!InDriftWindow(pt_begin.x(),pt_end.x(),geom)) continue;

    std::vector<float> lightHypothesis = GetMIPHypotheses(track,geom,pvs);

    bool compatible=false;
    for(const recob::OpFlash* flashPointer : flashesOnBeamTime){
      CompatibilityResultType result = CheckCompatibility(lightHypothesis,flashPointer);
      if(result==CompatibilityResultType::kCompatible) compatible=true;
      if(DEBUG_FLAG){
	PrintTrackProperties(track);
	PrintFlashProperties(*flashPointer);
	PrintHypothesisFlashComparison(lightHypothesis,flashPointer,result);
      }
    }

    float cosmicScore=1.;
    if(compatible) cosmicScore=0.;
    std::vector<float> xyz_begin = { (float)pt_begin.x(), (float)pt_begin.y(), (float)pt_begin.z()};
    std::vector<float> xyz_end = {(float)pt_end.x(), (float)pt_end.y(), (float)pt_end.z()};

    cosmicTagVector.emplace_back(xyz_begin,xyz_end,cosmicScore,COSMIC_TYPE);
    assnTrackTagVector.push_back(track_i);
  }

}

bool cosmic::BeamFlashTrackMatchTaggerAlg::InDriftWindow(double start_x, double end_x, geo::Geometry const& geom){
  if(start_x < 0. || end_x < 0.) return false;
  if(start_x > 2*geom.DetHalfWidth() || end_x > 2*geom.DetHalfWidth()) return false;
  return true;
}

// Get a hypothesis for the light collected for a bezier track
std::vector<float> cosmic::BeamFlashTrackMatchTaggerAlg::GetMIPHypotheses(recob::Track const& track, 
									  geo::Geometry const& geom,
									  phot::PhotonVisibilityService const& pvs,
									  float XOffset)
{
  std::vector<float> lightHypothesis(geom.NOpDet(),0);  
  const float PromptMIPScintYield = fMIPYield * fQE * fMIPdQdx * fPromptFrac;


  float length_segment=0;
  double xyz_segment[3];
  for(size_t pt=1; pt<track.NumberTrajectoryPoints(); pt++){
    
    //get the segment we're interested in
    TVector3 const& pt1 = track.LocationAtPoint(pt-1);
    TVector3 const& pt2 = track.LocationAtPoint(pt);    
    xyz_segment[0] = 0.5*(pt2.x()+pt1.x()) + XOffset;
    xyz_segment[1] = 0.5*(pt2.y()+pt1.y());
    xyz_segment[2] = 0.5*(pt2.z()+pt1.z());
    length_segment = (pt2-pt1).Mag();

    //get the amount of light
    float LightAmount = PromptMIPScintYield*length_segment;

    //get the visibility vector
    const std::vector<float>* PointVisibility = pvs.GetAllVisibilities(xyz_segment);
    
    //check vector size, as it may be zero if given a y/z outside some range
    if(PointVisibility->size()!=geom.NOpDet()) continue;

    for(size_t opdet_i=0; opdet_i<geom.NOpDet(); opdet_i++)
      lightHypothesis[opdet_i] += PointVisibility->at(opdet_i)*LightAmount;

  }

  return lightHypothesis;

}//end GetMIPHypotheses


//---------------------------------------
//  Check whether a hypothesis can be accomodated in a flash
//   Flashes fail if 1 bin is far in excess of the observed signal 
//   or if the whole flash intensity is much too large for the hypothesis.
//  MIP dEdx is assumed for now.  Accounting for real dQdx will 
//   improve performance of this algorithm.
//---------------------------------------
cosmic::BeamFlashTrackMatchTaggerAlg::CompatibilityResultType 
cosmic::BeamFlashTrackMatchTaggerAlg::CheckCompatibility(std::vector<float> const& lightHypothesis, 
							 const recob::OpFlash* flashPointer)
{
  float hypothesis_integral=0;
  float flash_integral=0;
  unsigned int cumulativeChannels=0;
  for(size_t pmt_i=0; pmt_i<lightHypothesis.size(); pmt_i++){

    flash_integral += flashPointer->PE(pmt_i);

    if(lightHypothesis[pmt_i] < std::numeric_limits<float>::epsilon() ) continue;

    float diff_scaled = (lightHypothesis[pmt_i] - flashPointer->PE(pmt_i))/std::sqrt(lightHypothesis[pmt_i]);

    if( diff_scaled > fSingleChannelCut ) return CompatibilityResultType::kSingleChannelCut;

    if( diff_scaled > fCumulativeChannelThreshold ) cumulativeChannels++;
    if(cumulativeChannels >= fCumulativeChannelCut) return CompatibilityResultType::kCumulativeChannelCut;

    hypothesis_integral += lightHypothesis[pmt_i];
  }

  if( (hypothesis_integral - flash_integral)/std::sqrt(hypothesis_integral) 
      > fIntegralCut) return CompatibilityResultType::kIntegralCut;

  return CompatibilityResultType::kCompatible;
}

void cosmic::BeamFlashTrackMatchTaggerAlg::PrintTrackProperties(recob::Track const& track, std::ostream* output)
{
  *output << "----------------------------------------------" << std::endl;
  *output << "Track properties: ";
  *output << "\n\tLength=" << track.Length();

  TVector3 const& pt_begin = track.LocationAtPoint(0);
  *output << "\n\tBegin Location (x,y,z)=(" << pt_begin.x() << "," << pt_begin.y() << "," << pt_begin.z() << ")";

  TVector3 const& pt_end = track.LocationAtPoint(track.NumberTrajectoryPoints()-1);
  *output << "\n\tEnd Location (x,y,z)=(" << pt_end.x() << "," << pt_end.y() << "," << pt_end.z() << ")";

  *output << "\n\tTrajectoryPoints=" << track.NumberTrajectoryPoints();
  *output << std::endl;
  *output << "----------------------------------------------" << std::endl;
}

void cosmic::BeamFlashTrackMatchTaggerAlg::PrintFlashProperties(recob::OpFlash const& flash, std::ostream* output)
{
  *output << "----------------------------------------------" << std::endl;
  *output << "Flash properties: ";

  *output << "\n\tTime=" << flash.Time();
  *output << "\n\tOnBeamTime=" << flash.OnBeamTime();
  *output << "\n\ty position (center,width)=(" << flash.YCenter() << "," << flash.YWidth() << ")";
  *output << "\n\tz position (center,width)=(" << flash.ZCenter() << "," << flash.ZWidth() << ")";
  *output << "\n\tTotal PE=" << flash.TotalPE();

  *output << std::endl;
  *output << "----------------------------------------------" << std::endl;

}

void cosmic::BeamFlashTrackMatchTaggerAlg::PrintHypothesisFlashComparison(std::vector<float> const& lightHypothesis,
									  const recob::OpFlash* flashPointer,
									  CompatibilityResultType result,
									  std::ostream* output)
{

  *output << "----------------------------------------------" << std::endl;
  *output << "Hypothesis-flash comparison: ";

  float hypothesis_integral=0;
  float flash_integral=0;
  for(size_t pmt_i=0; pmt_i<lightHypothesis.size(); pmt_i++){

    flash_integral += flashPointer->PE(pmt_i);

    *output << "\n\t pmt_i=" << pmt_i << ", (hypothesis,flash)=(" 
	   << lightHypothesis[pmt_i] << "," << flashPointer->PE(pmt_i) << ")";

    if(lightHypothesis[pmt_i] < std::numeric_limits<float>::epsilon() ) continue;

    *output << "  difference=" << (lightHypothesis[pmt_i] - flashPointer->PE(pmt_i))/std::sqrt(lightHypothesis[pmt_i]);

    hypothesis_integral += lightHypothesis[pmt_i];
  }

  *output << "\n\t TOTAL (hypothesis,flash)=(" 
	 << hypothesis_integral << "," << flash_integral << ")"
	 << "  difference=" << (hypothesis_integral - flash_integral)/std::sqrt(hypothesis_integral);

  *output << std::endl;
  *output << "End result=" << result << std::endl;
  *output << "----------------------------------------------" << std::endl;

}