/** @file
  
  Declares global variables used across files and functions. This file contains the constructors
  used to initialize the classes defined in \ref global.h, and does little more than initilize the
  default values of various parameters.
*/
#include "global.h"

MessPass::MessPass(){
  typeSendNewGrid=NULL; 
  typeRecvOldGrid=NULL;
  typeSendNewVar=NULL;
  typeRecvNewVar=NULL;
  requestSend=NULL;
  requestRecv=NULL;
  statusSend=NULL;
  statusRecv=NULL;
}
Grid::Grid(){
  nGlobalGridDims=NULL;
  nVariables=NULL;
  nLocalGridDims=NULL;
  dLocalGridNew=NULL;
  dLocalGridOld=NULL;
  nStartUpdateExplicit=NULL;
  nEndUpdateExplicit=NULL;
  nStartGhostUpdateExplicit=NULL;
  nEndGhostUpdateExplicit=NULL;
  nStartUpdateImplicit=NULL;
  nEndUpdateImplicit=NULL;
  nStartGhostUpdateImplicit=NULL;
  nEndGhostUpdateImplicit=NULL;
  nCenIntOffset=NULL;
  nM=-1;
  nTheta=-1;
  nPhi=-1;
  nDM=-1;
  nR=-1;
  nD=-1;
  nU=-1;
  nU0=-1;
  nV=-1;
  nW=-1;
  nT=-1;
  nE=-1;
  nP=-1;
  nKappa=-1;
  nGamma=-1;
  nDenAve=-1;
  nQ0=-1;
  nQ1=-1;
  nQ2=-1;
  nDTheta=-1;
  nDPhi=-1;
  nSinThetaIJK=-1;
  nSinThetaIJp1halfK=-1;
  nCotThetaIJp1halfK=-1;
  nCotThetaIJK=-1;
  nDCosThetaIJK=-1;
  nEddyVisc=-1;
  nDonorCellFrac=-1;
}
Parameters::Parameters(){
  dPi=3.1415926535897932384626433832795;
  dG=6.67259E-8;
  dA=0.0;
  dAVThreshold=0.0;
  dAlpha=0.2;
  dAlphaExtra=0.0;
  dSigma=5.67040040E-5;
  dEddyViscosity=10.0;
  dMaxConvectiveVelocity=6.69041282767684e-02;
  dMaxConvectiveVelocity_c=0.0;
  dPrt=0.7;
  dDonorCellMin=0.05;
  dT_cut=2.35e4;
  bDEDM_cut_set=false;
  #if DEBUG_EQUATIONS==1
  bSetThisCall=false;
  #endif
}
Output::Output(){
  nDumpFrequencyStep=1;
  bDump=false;
  sBaseOutputFileName="out";
  ofWatchZoneFiles=NULL;
  nNumTimeStepsSinceLastDump=-1;
  nNumTimeStepsSinceLastPrint=-1;
}
Performance::Performance(){
  dStartTimer=0.0;
  dEndTimer=0.0;
}
Functions::Functions(){
  fpCalculateNewVelocities=NULL;
  fpCalculateNewGridVelocities=NULL;
  fpCalculateNewRadii=NULL;
  fpCalculateNewDensities=NULL;
  fpCalculateNewEnergies=NULL;
  fpCalculateDeltat=NULL;
  fpCalculateAveDensities=NULL;
  fpCalculateNewEOSVars=NULL;
  fpCalculateNewAV=NULL;
}
Implicit::Implicit(){
  nNumImplicitZones=0;
  nMaxNumIterations=15;
  dTolerance=5.0e-15;
  nNumRowsALocal=0;
  nNumRowsALocalSB=0;
  nNumDerPerRow=NULL;
  nTypeDer=NULL;
  nLocDer=NULL;
  nLocFun=NULL;
  dDerivativeStepFraction=0.1;
  dCurrentRelTError=0;
  nCurrentNumIterations=0;
  nMaxNumSolverIterations=0;
  dMaxErrorInRHS=0.0;
}
Global::Global(){
}
