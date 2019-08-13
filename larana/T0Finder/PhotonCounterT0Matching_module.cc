/////////////////////////////////////////////////////////////////////////////
/// Class:       PhotonCounterT0Matching
/// Module Type: producer
/// File:        PhotonCounterT0Matching_module.cc
///
/// Author:         Thomas Karl Warburton
/// E-mail address: k.warburton@sheffield.ac.uk
///
/// Generated at Wed Mar 25 13:54:28 2015 by Thomas Warburton using artmod
/// from cetpkgsupport v1_08_04.
///
/// This module tries to match a reconstructed track with a reconstructed
/// flash with the purpose of making an anab::T0 data product.
/// It does this by looping through the reconstructed tracks and for each
/// track seeing if any of the reconstructed flashes could be associated with
/// it (if it is within 1 drift window). If a flash can be matched with the
/// track then some matching criteria are calculated;
///     A PE vs X relationship
///     The separation of the flash centre to a track space point in YZ
/// The flash which has the smallest summed square of these quantities is then
/// attributed to this track.
/// It is possible for a flash to be attributed to multiple tracks, but only
/// one flash is attributed to each track.
/// If there are no flashes within one drift window of the track, then no flash
/// is assigned. Therefore it is important to look at other methods of using
/// other T0 finders in addition to this one.
///
/// The module takes a reconstructed track as input.
/// The module outputs an anab::T0 object
/////////////////////////////////////////////////////////////////////////////

// Framework includes
#include "art/Framework/Core/EDProducer.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Principal/Handle.h"
#include "art_root_io/TFileService.h"
#include "canvas/Persistency/Common/FindManyP.h"
#include "canvas/Persistency/Common/Ptr.h"
#include "fhiclcpp/ParameterSet.h"

#include <iostream>
#include <memory>

// LArSoft
#include "larcore/Geometry/Geometry.h"
#include "lardata/DetectorInfoServices/DetectorClocksService.h"
#include "lardata/DetectorInfoServices/DetectorPropertiesService.h"
#include "lardata/Utilities/AssociationUtil.h"
#include "lardataobj/AnalysisBase/T0.h"
#include "lardataobj/RecoBase/Hit.h"
#include "lardataobj/RecoBase/OpFlash.h"
#include "lardataobj/RecoBase/Shower.h"
#include "lardataobj/RecoBase/Track.h"

// ROOT
#include "TTree.h"
#include "TH1D.h"
#include "TH2D.h"

namespace lbne {
  class PhotonCounterT0Matching;
}

class lbne::PhotonCounterT0Matching : public art::EDProducer {
public:
  explicit PhotonCounterT0Matching(fhicl::ParameterSet const & p);
  // The destructor generated by the compiler is fine for classes
  // without bare pointers or other resource use.

  // Plugins should not be copied or assigned.
  PhotonCounterT0Matching(PhotonCounterT0Matching const &) = delete;
  PhotonCounterT0Matching(PhotonCounterT0Matching &&) = delete;
  PhotonCounterT0Matching & operator = (PhotonCounterT0Matching const &) = delete;
 PhotonCounterT0Matching & operator = (PhotonCounterT0Matching &&) = delete;

  // Required functions.
  void produce(art::Event & e) override;

  // Selected optional functions.
  void beginJob() override;


private:
  // Internal functions.....
  void TrackProp ( double TrackStart_X, double TrackEnd_X, double &TrackLength_X, double &TrackCentre_X,
		   double TrackStart_Y, double TrackEnd_Y, double &TrackLength_Y, double &TrackCentre_Y,
		   double TrackStart_Z, double TrackEnd_Z, double &TrackLength_Z, double &TrackCentre_Z,
		   double trkTimeStart, double trkTimeEnd, double &trkTimeLengh , double &trkTimeCentre,
		   double &TrackLength);
  double DistFromPoint ( double StartY, double EndY, double StartZ, double EndZ, double PointY, double PointZ );

  // Params got from fcl file.......
  std::string fTrackModuleLabel;
  std::string fShowerModuleLabel;
  std::string fHitsModuleLabel;
  std::string fFlashModuleLabel;
  std::string fTruthT0ModuleLabel;
  double fPredictedXConstant;
  double fPredictedXPower = 1;
  double fPredictedExpConstant;
  double fPredictedExpGradient;
  double fDriftWindowSize;
  double fWeightOfDeltaYZ;
  double fMatchCriteria;
  double fPEThreshold;
  bool   fVerbosity;

  // Variables used in module.......
  double TrackLength_X, TrackCentre_X, BestTrackCentre_X;
  double TrackLength_Y, TrackCentre_Y /* , BestTrackCentre_Y */;
  double TrackLength_Z, TrackCentre_Z /* , BestTrackCentre_Z */;
  double trkTimeStart, trkTimeEnd, trkTimeLengh;

  double trkTimeCentre, BesttrkTimeCentre;
  double TrackLength, BestTrackLength;
  double PredictedX, BestPredictedX;
  double TimeSepPredX, BestTimeSepPredX;
  double DeltaPredX, BestDeltaPredX;
  double minYZSep, BestminYZSep;
  double FitParam, BestFitParam;
  double FlashTime, BestFlashTime;
  double TimeSep, BestTimeSep;
  int BestFlash;
  int FlashTriggerType=1;

  double YZSep, MCTruthT0;
  // Histograms in TFS branches
  TTree* fTree;
  TH2D* hPredX_T;
  TH2D* hPredX_PE;
  TH2D* hPredX_T_PE;
  TH2D* hdeltaX_deltaYZ;
  TH2D* hdeltaYZ_Length;
  TH2D* hFitParam_Length;
  TH2D* hPhotonT0_MCT0;
  TH1D* hT0_diff_full;
  TH1D* hT0_diff_zoom;
};


lbne::PhotonCounterT0Matching::PhotonCounterT0Matching(fhicl::ParameterSet const & p)
  : EDProducer{p}
{
  // Call appropriate produces<>() functions here.
  produces< std::vector<anab::T0>               >();
  produces< art::Assns<recob::Track , anab::T0> >();
  produces< art::Assns<recob::Shower, anab::T0> > ();

  fTrackModuleLabel   = (p.get< std::string > ("TrackModuleLabel" ) );
  fShowerModuleLabel  = (p.get< std::string > ("ShowerModuleLabel") );
  fHitsModuleLabel    = (p.get< std::string > ("HitsModuleLabel"  ) );
  fFlashModuleLabel   = (p.get< std::string > ("FlashModuleLabel" ) );
  fTruthT0ModuleLabel = (p.get< std::string > ("TruthT0ModuleLabel"));

  fPredictedXConstant   = (p.get< double > ("PredictedXConstant"   ) );
  fPredictedExpConstant = (p.get< double > ("PredictedExpConstant" ) );
  fPredictedExpGradient = (p.get< double > ("PredictedExpGradient" ) );

  fDriftWindowSize = (p.get< double > ("DriftWindowSize"   ) );
  fWeightOfDeltaYZ = (p.get< double > ("WeightOfDeltaYZ"   ) );
  fMatchCriteria   = (p.get< double > ("MatchCriteria"     ) );
  fPEThreshold     = (p.get< double > ("PEThreshold"       ) );

  fVerbosity = (p.get< bool > ("Verbose",false) );
}

void lbne::PhotonCounterT0Matching::beginJob()
{
  // Implementation of optional member function here.
  art::ServiceHandle<art::TFileService const> tfs;
  fTree = tfs->make<TTree>("PhotonCounterT0Matching","PhotonCounterT0");
  fTree->Branch("TrackCentre_X",&BestTrackCentre_X,"TrackCentre_X/D");
  fTree->Branch("PredictedX"   ,&BestPredictedX   ,"PredictedX/D");
  fTree->Branch("TrackTimeCent",&BesttrkTimeCentre,"TimeSepPredX/D");
  fTree->Branch("FlashTime"    ,&BestFlashTime    ,"FlashTime/D");
  fTree->Branch("TimeSep"      ,&BestTimeSep      ,"TimeSep/D");
  fTree->Branch("TimeSepPredX" ,&BestTimeSepPredX ,"TimeSepPredX/D");
  fTree->Branch("minYZSep"     ,&BestminYZSep     ,"minYZSep/D");
  fTree->Branch("FitParam"     ,&BestFitParam     ,"FitParam/D");
  fTree->Branch("MCTruthT0"    ,&MCTruthT0        ,"MCTruthT0/D");

  hPredX_T  = tfs->make<TH2D>("hPredX_T" ,"Predicted X from timing information against reconstructed X; Reconstructed X (cm); Predicted X (cm)", 30, 0, 300, 30, 0, 300 );
  hPredX_PE = tfs->make<TH2D>("hPredX_PE","Predicted X from PE information against reconstructed X; Reconstructed X (cm); Predicted X (cm)"    , 30, 0, 300, 30, 0, 300 );
  hPredX_T_PE = tfs->make<TH2D>("hPredX_T_PE",
				"Predicted X position from time and PE information; Predicted X from timing information (cm); Predicted X from PE information",
				60, 0, 300, 60, 0, 300);
  hdeltaX_deltaYZ = tfs->make<TH2D>("hdeltaX_deltaYZ",
				    "Difference between X predicted from PE's and T agaisnt distance of flash from track in YZ; Difference in X predicted from PE's and T (cm); Distance of flash from track in YZ (cm)",
				    40, 0, 200, 40, 0, 100);
  hdeltaYZ_Length = tfs->make<TH2D>("hdeltaYZ_Length",
				    "Distance of flash from track against track length; Distance from flash to track (cm); Track length (cm)",
				    20, 0, 100, 60, 0, 300);
  hFitParam_Length = tfs->make<TH2D>("hFitParam_Length", "How fit correlates with track length; Fit correlation; Track Length (cm)", 50, 0, 250, 30, 0, 300);
  hPhotonT0_MCT0   = tfs->make<TH2D>("hPhotonT0_MCT0"  , "Comparing Photon Counter reconstructed T0 against MCTruth T0; Photon Counter T0 (us); MCTruthT0 T0 (us)", 1760, -1600, 16000, 1760, -1600, 16000);
  hT0_diff_full    = tfs->make<TH1D>("hT0_diff_full"   , "Difference between MCTruth T0 and photon detector T0; Time difference (us); Number", 500, -2500, 2500);
  hT0_diff_zoom    = tfs->make<TH1D>("hT0_diff_zoom"   , "Difference between MCTruth T0 and photon detector T0; Time difference (us); Number", 320, -1.6, 1.6);
}

void lbne::PhotonCounterT0Matching::produce(art::Event & evt)
{
  // Access art services...
  art::ServiceHandle<geo::Geometry const> geom;
  auto const* detprop = lar::providerFrom<detinfo::DetectorPropertiesService>();
  auto const* timeservice = lar::providerFrom<detinfo::DetectorClocksService>();
  //Commenting this out since it doesn't appear to actually be used.
//  art::ServiceHandle<cheat::BackTrackerService const> bt_serv;

  //TrackList handle
  art::Handle< std::vector<recob::Track> > trackListHandle;
  std::vector<art::Ptr<recob::Track> > tracklist;
  if (evt.getByLabel(fTrackModuleLabel,trackListHandle))
    art::fill_ptr_vector(tracklist, trackListHandle);

  //ShowerList handle
  art::Handle< std::vector<recob::Shower> > showerListHandle;
  std::vector<art::Ptr<recob::Shower> > showerlist;
  if (evt.getByLabel(fShowerModuleLabel,showerListHandle))
    art::fill_ptr_vector(showerlist, showerListHandle);

  //HitList Handle
  art::Handle< std::vector<recob::Hit> > hitListHandle;
  std::vector<art::Ptr<recob::Hit> > hitlist;
  if (evt.getByLabel(fHitsModuleLabel,hitListHandle))
      art::fill_ptr_vector(hitlist, hitListHandle);

  //FlashList Handle
  art::Handle< std::vector<recob::OpFlash> > flashListHandle;
  std::vector<art::Ptr<recob::OpFlash> > flashlist;
  if (evt.getByLabel(fFlashModuleLabel, flashListHandle))
    art::fill_ptr_vector(flashlist, flashListHandle);

  // Create anab::T0 objects and make association with recob::Track

  std::unique_ptr< std::vector<anab::T0> > T0col( new std::vector<anab::T0>);
  std::unique_ptr< art::Assns<recob::Track, anab::T0> > Trackassn( new art::Assns<recob::Track, anab::T0>);
  std::unique_ptr< art::Assns<recob::Shower, anab::T0> > Showerassn( new art::Assns<recob::Shower, anab::T0>);

  if (trackListHandle.isValid() && flashListHandle.isValid() ){
    //Access tracks and hits
    art::FindManyP<recob::Hit> fmtht(trackListHandle, evt, fTrackModuleLabel);
    art::FindMany<anab::T0>    fmtruth(trackListHandle, evt, fTruthT0ModuleLabel);

    size_t NTracks  = tracklist.size();
    size_t NFlashes = flashlist.size();

    if (fVerbosity)
      std::cout << "There were " << NTracks << " tracks and " << NFlashes << " flashes in this event." << std::endl;

    // Now to access PhotonCounter for each track...
    for(size_t iTrk=0; iTrk < NTracks; ++iTrk) {
      if (fVerbosity) std::cout << "\n New Track " << (int)iTrk << std::endl;
      // Reset Variables.
      BestFlashTime = BestFitParam = BestTrackCentre_X = BestTrackLength = 9999;
      BestTimeSepPredX = BestPredictedX = BestDeltaPredX = BestminYZSep = MCTruthT0 = 9999;
      bool ValidTrack = false;

      // Work out Properties of the track.
      recob::Track::Point_t trackStart, trackEnd;
      std::tie(trackStart, trackEnd) = tracklist[iTrk]->Extent();
      std::vector< art::Ptr<recob::Hit> > allHits = fmtht.at(iTrk);
      size_t nHits = allHits.size();
      trkTimeStart = allHits[nHits-1]->PeakTime() / timeservice->TPCClock().Frequency(); //Got in ticks, now in us!
      trkTimeEnd   = allHits[0]->PeakTime() / timeservice->TPCClock().Frequency(); //Got in ticks, now in us!
      TrackProp ( trackStart.X(), trackEnd.X(), TrackLength_X, TrackCentre_X,
		  trackStart.Y(), trackEnd.Y(), TrackLength_Y, TrackCentre_Y,
		  trackStart.Z(), trackEnd.Z(), TrackLength_Z, TrackCentre_Z,
		  trkTimeStart , trkTimeEnd , trkTimeLengh , trkTimeCentre, // times in us!
		  TrackLength);

      // Some cout statement about track properties.
      if (fVerbosity) {
	std::cout << trackStart.X() << " " << trackEnd.X() << " " << TrackLength_X << " " << TrackCentre_X
		  << "\n" << trackStart.Y() << " " << trackEnd.Y() << " " << TrackLength_Y << " " << TrackCentre_Y
		  << "\n" << trackStart.Z() << " " << trackEnd.Z() << " " << TrackLength_Z << " " << TrackCentre_Z
		  << "\n" << trkTimeStart  << " " << trkTimeEnd  << " " << trkTimeLengh  << " " << trkTimeCentre
		  << std::endl;
      }
      // ----- Loop over flashes ------
      for ( size_t iFlash=0; iFlash < NFlashes; ++iFlash ) {
	//Reset some flash specific quantities
	YZSep = minYZSep = 9999;
	FlashTime = TimeSep = 9999;
	PredictedX = TimeSepPredX = DeltaPredX = FitParam = 9999;
	// Check flash could be caused by track...
	FlashTime = flashlist[iFlash]->Time(); // Got in us!
	TimeSep = trkTimeCentre - FlashTime; // Time in us!
	if ( TimeSep < 0 || TimeSep > (fDriftWindowSize/timeservice->TPCClock().Frequency() ) ) continue; // Times compared in us!

	// Check flash has enough PE's to satisfy our threshold
	if ( flashlist[iFlash]->TotalPE() < fPEThreshold ) continue;

	// Work out some quantities for this flash...
	// PredictedX = ( A / x^n ) + exp ( B + Cx )
	PredictedX   = (fPredictedXConstant / pow ( flashlist[iFlash]->TotalPE(), fPredictedXPower ) ) + ( exp ( fPredictedExpConstant + ( fPredictedExpGradient * flashlist[iFlash]->TotalPE() ) ) );
	TimeSepPredX = TimeSep * detprop->DriftVelocity(); // us * cm/us = cm!
	DeltaPredX   = fabs(TimeSepPredX-PredictedX);
	// Dependant on each point...
	for ( size_t Point = 1; Point < tracklist[iTrk]->NumberTrajectoryPoints(); ++Point ) {
	  auto NewPoint  = tracklist[iTrk]->LocationAtPoint(Point);
	  auto PrevPoint = tracklist[iTrk]->LocationAtPoint(Point-1);
	  YZSep = DistFromPoint ( NewPoint.Y(), PrevPoint.Y(), NewPoint.Z(), PrevPoint.Z(),
				  flashlist[iFlash]->YCenter(), flashlist[iFlash]->ZCenter());
	  if ( Point == 1 ) minYZSep = YZSep;
	  if ( YZSep < minYZSep ) minYZSep = YZSep;
	}

	// Determine how well matched this track is......
	if (fMatchCriteria == 0) FitParam = pow( ((DeltaPredX*DeltaPredX)+(minYZSep*minYZSep*fWeightOfDeltaYZ)), 0.5);
	else if (fMatchCriteria == 1) FitParam = minYZSep;
	else if (fMatchCriteria == 2) FitParam = DeltaPredX;

	//----FLASH INFO-----
	if (fVerbosity) {
	  std::cout << "\nFlash " << (int)iFlash << " " << TrackCentre_X << ", " << TimeSepPredX << " - " << PredictedX << " = "
		    << DeltaPredX << ", " << minYZSep << " -> " << FitParam << std::endl;
	}
	//----Select best flash------
	//double YFitRegion = (-1 * DeltaPredX ) + 80;
	//if ( minYZSep > YFitRegion ) continue;
	if ( FitParam < BestFitParam ) {
	  ValidTrack        = true;
	  BestFlash         = (int)iFlash;
	  BestFitParam      = FitParam;
	  BestTrackCentre_X = TrackCentre_X;
	  BestTrackLength   = TrackLength;
	  BesttrkTimeCentre = trkTimeCentre;
	  BestTimeSepPredX  = TimeSepPredX;
	  BestPredictedX    = PredictedX;
	  BestDeltaPredX    = DeltaPredX;
	  BestminYZSep      = minYZSep;
	  BestFlashTime     = FlashTime;
	  BestTimeSep       = TimeSep;
	} // Find best Flash
      } // Loop over Flashes

      // ---- Now Make association and fill TTree/Histos with the best matched flash.....
      if (ValidTrack) {

	// -- Fill Histos --
	hPredX_T         ->Fill( BestTrackCentre_X, BestTimeSepPredX );
	hPredX_PE        ->Fill( BestTrackCentre_X, BestPredictedX );
	hPredX_T_PE      ->Fill( BestTimeSepPredX , BestPredictedX );
	hdeltaX_deltaYZ  ->Fill( BestDeltaPredX, BestminYZSep );
	hdeltaYZ_Length  ->Fill( BestminYZSep, BestTrackLength );
	hFitParam_Length ->Fill( BestFitParam, BestTrackLength );
	// ------ Compare Photon Matched to MCTruth Matched -------
	if ( fmtruth.isValid() ) {
	  std::vector<const anab::T0*> T0s = fmtruth.at((int)iTrk);
	  for ( size_t i=0; i<T0s.size(); ++i) {
	    MCTruthT0 = T0s[i]->Time() / 1e3; // Got in ns, now in us!!
	    hPhotonT0_MCT0 ->Fill( BestFlashTime, MCTruthT0 );
	    hT0_diff_full  -> Fill( MCTruthT0 - BestFlashTime );
	    hT0_diff_zoom  -> Fill( MCTruthT0 - BestFlashTime );
	    //std::cout << "Size " << T0s.size() << " " << MCTruthT0 << " " << BestFlashTime << std::endl;
	  }
	}
	// -- Fill TTree --
	fTree->Fill();
	//Make Association
	T0col->push_back(anab::T0(BestFlashTime * 1e3,
				  FlashTriggerType,
				  (int)BestFlash,
				  (*T0col).size(),
				  BestFitParam
				  ));
	util::CreateAssn(*this, evt, *T0col, tracklist[iTrk], *Trackassn);
      } // Valid Track
    } // Loop over tracks
  }
  /* // ------------------------------------------------- SHOWER STUFF -------------------------------------------------
  if (showerListHandle.isValid()){
    art::FindManyP<recob::Hit> fmsht(showerListHandle,evt, fShowerModuleLabel);
    // Now Loop over showers....
    size_t NShowers = showerlist.size();
    for (size_t Shower = 0; Shower < NShowers; ++Shower) {
      ShowerMatchID     = 0;
      ShowerID          = 0;
      ShowerT0          = 0;
      std::vector< art::Ptr<recob::Hit> > allHits = fmsht.at(Shower);

      std::map<int,double> showeride;
      for(size_t h = 0; h < allHits.size(); ++h){
	art::Ptr<recob::Hit> hit = allHits[h];
	std::vector<sim::IDE> ides;
	std::vector<sim::TrackIDE> TrackIDs = bt->HitToTrackID(hit);

	for(size_t e = 0; e < TrackIDs.size(); ++e){
	  showeride[TrackIDs[e].trackID] += TrackIDs[e].energy;
	}
      }
      // Work out which IDE despoited the most charge in the hit if there was more than one.
      double maxe = -1;
      double tote = 0;
      for (std::map<int,double>::iterator ii = showeride.begin(); ii!=showeride.end(); ++ii){
	tote += ii->second;
	if ((ii->second)>maxe){
	  maxe = ii->second;
	  ShowerID = ii->first;
	}
      }
      // Now have MCParticle trackID corresponding to shower, so get PdG code and T0 etc.
      const simb::MCParticle *particle = bt->TrackIDToParticle(ShowerID);
      ShowerT0 = particle->T();
      ShowerID = particle->TrackId();
      ShowerTriggerType = 1; // Using PhotonCounter as trigger, so tigger type is 1.

      T0col->push_back(anab::T0(ShowerT0,
				ShowerTriggerType,
				ShowerID,
				(*T0col).size()
				));
      util::CreateAssn(*this, evt, *T0col, showerlist[Shower], *Showerassn);
    }// Loop over showers
  }
  */
  evt.put(std::move(T0col));
  evt.put(std::move(Trackassn));
  evt.put(std::move(Showerassn));

} // Produce
// ----------------------------------------------------------------------------------------------------------------------------
void lbne::PhotonCounterT0Matching::TrackProp ( double TrackStart_X, double TrackEnd_X, double &TrackLength_X, double &TrackCentre_X,
						double TrackStart_Y, double TrackEnd_Y, double &TrackLength_Y, double &TrackCentre_Y,
						double TrackStart_Z, double TrackEnd_Z, double &TrackLength_Z, double &TrackCentre_Z,
						double trkTimeStart, double trkTimeEnd, double &trkTimeLengh , double &trkTimeCentre,
						double &TrackLength) {
  ///Calculate central values for track X, Y, Z and time, as well as lengths and overall track length.
  TrackLength_X = fabs ( TrackEnd_X - TrackStart_X );
  if ( TrackStart_X < TrackEnd_X ) TrackCentre_X = TrackStart_X + 0.5*TrackLength_X;
  else TrackCentre_X = TrackStart_X - 0.5*TrackLength_X;

  TrackLength_Y = fabs ( TrackEnd_Y - TrackStart_Y );
  if ( TrackStart_Y < TrackEnd_Y ) TrackCentre_Y = TrackStart_Y + 0.5*TrackLength_Y;
  else TrackCentre_Y = TrackStart_Y - 0.5*TrackLength_Y;

  TrackLength_Z = fabs ( TrackEnd_Z - TrackStart_Z );
  if ( TrackStart_Z < TrackEnd_Z ) TrackCentre_Z = TrackStart_Z + 0.5*TrackLength_Z;
  else TrackCentre_Z = TrackStart_Z - 0.5*TrackLength_Z;

  trkTimeLengh   = trkTimeEnd - trkTimeStart;
  trkTimeCentre = trkTimeStart + 0.5*trkTimeLengh;

  TrackLength =  pow( pow((TrackEnd_X-TrackStart_X), 2) + pow((TrackEnd_Y-TrackStart_Y), 2) + pow((TrackEnd_Z-TrackStart_Z), 2) , 0.5);

  return;
}
// ----------------------------------------------------------------------------------------------------------------------------
double lbne::PhotonCounterT0Matching::DistFromPoint ( double StartY, double EndY, double StartZ, double EndZ, double PointY, double PointZ ) {
  ///Calculate the distance between the centre of the flash and the centre of a line connecting two adjacent space points.
  double Length = hypot ( fabs( EndY - StartY), fabs ( EndZ - StartZ ) );
  //  double distance = (double)((point.x - line_start.x) * (line_end.y - line_start.y) - (point.y - line_start.y) * (line_end.x - line_start.x)) / normalLength;
  double distance = ( (PointZ - StartZ) * (EndY - StartY) - (PointY - StartY) * (EndZ - StartZ) ) / Length;
  return fabs(distance);
}
// ----------------------------------------------------------------------------------------------------------------------------
DEFINE_ART_MODULE(lbne::PhotonCounterT0Matching)
