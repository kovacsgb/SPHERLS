/** @file
  
  This file is used to specify the functions which contain physics. This includes conservation
  equations, equation of state, etc.. It also sets function pointers for these functions, so that 
  \ref main() will know which functions to call. This implementation also allows the functions
  called to calculate, for example new densities, to be different depending on the processor. This
  allows one processor to handle the 1D region and other processors to handle a 3D region.
*/

#include <cmath>
#include <sstream>
#include <signal.h>
#include "exception2.h"
#include "physEquations.h"
#include "dataManipulation.h"
#include "dataMonitoring.h"
#include "global.h"
#include <limits>

void setMainFunctions(Functions &functions,ProcTop &procTop,Parameters &parameters,Grid &grid
  ,Time &time,Implicit &implicit){
  
  //set some defaults
  functions.fpCalculateNewEddyVisc=&calNewEddyVisc_None;
  functions.fpImplicitSolve=&implicitSolve_None;
  functions.fpImplicitEnergyFunction=&dImplicitEnergyFunction_None;
  functions.fpImplicitEnergyFunction_SB=&dImplicitEnergyFunction_None;
  
  //rank 0 will be 1D, so always want to use 1D version of these equations
  if(procTop.nRank==0){// proc 1 always uses 1D
    
    functions.fpCalculateNewGridVelocities=&calNewU0_R;
    functions.fpCalculateNewRadii=&calNewR;
    functions.fpCalculateNewDensities=&calNewD_R;
    functions.fpCalculateAveDensities=&calNewDenave_R;
    functions.fpUpdateLocalBoundaryVelocitiesNewGrid=&updateLocalBoundaryVelocitiesNewGrid_R;
    if(parameters.bEOSGammaLaw){//use gamma law gas
      functions.fpCalculateDeltat=&calDelt_R_GL;
      functions.fpCalculateNewEOSVars=&calNewP_GL;
      functions.fpCalculateNewAV=&calNewQ0_R_GL;
      functions.fpModelWrite=&modelWrite_GL;
      functions.fpWriteWatchZones=&writeWatchZones_R_GL;
    }
    else{//use tabulated equation of state
      functions.fpCalculateDeltat=&calDelt_R_TEOS;
      functions.fpCalculateNewEOSVars=&calNewTPKappaGamma_TEOS;
      functions.fpCalculateNewAV=&calNewQ0_R_TEOS;
      functions.fpModelWrite=&modelWrite_TEOS;
      functions.fpWriteWatchZones=&writeWatchZones_R_TEOS;
    }
    
    //velocity equation
    functions.fpCalculateNewVelocities=&calNewVelocities_R;
    
    //energy equation, eddy viscosity
    if(parameters.bAdiabatic){//adiabatic
      functions.fpCalculateNewEnergies=&calNewE_R_AD;
    }
    else{//non-adaibatic
      if(!parameters.bEOSGammaLaw){//needs a tabulated equation of state
        functions.fpCalculateNewEnergies=&calNewE_R_NA;
        functions.fpCalculateNewEddyVisc=&calNewEddyVisc_None;
        if(implicit.nNumImplicitZones>0){//implicit, requires non-adiabatic
          functions.fpImplicitSolve=&implicitSolve_R;
          functions.fpImplicitEnergyFunction=&dImplicitEnergyFunction_R;
          functions.fpImplicitEnergyFunction_SB=&dImplicitEnergyFunction_R_SB;
        }
      }
      else{//can't do a non-adiabatic calculation, with a gamma-law gas
        std::stringstream ssTemp;
        ssTemp<<__FILE__<<":"<<__FUNCTION__<<":"<<__LINE__<<":"<<procTop.nRank
          <<": User selected to do a non-adiabatic calculation but starting model uses a gamma-law"
          <<" gas. Starting model must use a tabulated equation of state in order to perform a"
          <<" non-adiabatic calculation.\n";
        throw exception2(ssTemp.str(),CALCULATION);
      }
    }
    
    /*Processor 0 must update all the velocities that the other processors do, so that they don't
    get stuck on mpi::waitall calls in the update routines*/
    
    //set density average function, and grid update function
    if(grid.nNumDims==1){
      functions.fpCalculateAveDensities=&calNewDenave_None;
      functions.fpUpdateLocalBoundaryVelocitiesNewGrid=&updateLocalBoundaryVelocitiesNewGrid_R;
    }
    else if(grid.nNumDims==2){
      functions.fpUpdateLocalBoundaryVelocitiesNewGrid=&updateLocalBoundaryVelocitiesNewGrid_RT;
    }
    else if(grid.nNumDims==3){
      functions.fpUpdateLocalBoundaryVelocitiesNewGrid=&updateLocalBoundaryVelocitiesNewGrid_RTP;
    }
  }
  else{
    if(grid.nNumDims==3){//use 3D
      functions.fpCalculateNewGridVelocities=&calNewU0_RTP;
      functions.fpCalculateNewRadii=&calNewR;
      functions.fpCalculateNewDensities=&calNewD_RTP;
      functions.fpCalculateAveDensities=&calNewDenave_RTP;
      functions.fpUpdateLocalBoundaryVelocitiesNewGrid=&updateLocalBoundaryVelocitiesNewGrid_RTP;
      if(parameters.bEOSGammaLaw){//use gamma law gas
        functions.fpCalculateDeltat=&calDelt_RTP_GL;
        functions.fpCalculateNewEOSVars=&calNewP_GL;
        functions.fpCalculateNewAV=&calNewQ0Q1Q2_RTP_GL;
        functions.fpModelWrite=&modelWrite_GL;
        functions.fpWriteWatchZones=&writeWatchZones_RTP_GL;
      }
      else{//use tabulated equation of state
        functions.fpCalculateDeltat=&calDelt_RTP_TEOS;
        functions.fpCalculateNewEOSVars=&calNewTPKappaGamma_TEOS;
        functions.fpCalculateNewAV=&calNewQ0Q1Q2_RTP_TEOS;
        functions.fpModelWrite=&modelWrite_TEOS;
        functions.fpWriteWatchZones=&writeWatchZones_RTP_TEOS;
      }
      
      //velocity equation
      if(parameters.nTypeTurbulanceMod>0){
        functions.fpCalculateNewVelocities=&calNewVelocities_RTP_LES;
      }
      else{
        functions.fpCalculateNewVelocities=&calNewVelocities_RTP;
      }
      
      if(parameters.nTypeTurbulanceMod==1){
        functions.fpCalculateNewEddyVisc=&calNewEddyVisc_RTP_CN;
      }
      else if(parameters.nTypeTurbulanceMod==2){
        functions.fpCalculateNewEddyVisc=&calNewEddyVisc_RTP_SM;
      }
      
      //energy equation, eddy viscosity
      if(parameters.bAdiabatic){//adiabatic
        functions.fpCalculateNewEnergies=&calNewE_RTP_AD;
      }
      else{//non-adaibatic
        if(!parameters.bEOSGammaLaw){//needs a tabulated equation of state
          if(parameters.nTypeTurbulanceMod>0){//with turbulance model
            functions.fpCalculateNewEnergies=&calNewE_RTP_NA_LES;
          }
          else{
            functions.fpCalculateNewEnergies=&calNewE_RTP_NA;
          }
          if(implicit.nNumImplicitZones>0){//implicit, requires non-adiabatic
            functions.fpImplicitSolve=&implicitSolve_RTP;
            if(parameters.nTypeTurbulanceMod>0){
              functions.fpImplicitEnergyFunction=&dImplicitEnergyFunction_RTP_LES;
              functions.fpImplicitEnergyFunction_SB=&dImplicitEnergyFunction_RTP_LES_SB;
            }
            else{
              functions.fpImplicitEnergyFunction=&dImplicitEnergyFunction_RTP;
              functions.fpImplicitEnergyFunction_SB=&dImplicitEnergyFunction_RTP_SB;
            }
          }
        }
        else{//can't do a non-adiabatic calculation, with a gamma-law gas
          std::stringstream ssTemp;
          ssTemp<<__FILE__<<":"<<__FUNCTION__<<":"<<__LINE__<<":"<<procTop.nRank
            <<": User selected to do a non-adiabatic calculation but starting model uses a "
            <<"gamma-law gas. Starting model must use a tabulated equation of state in order to "
            <<"perform a non-adiabatic calculation.\n";
          throw exception2(ssTemp.str(),CALCULATION);
        }
      }
    }
    else if(grid.nNumDims==2){//use 2D
      functions.fpCalculateNewGridVelocities=&calNewU0_RT;
      functions.fpCalculateNewRadii=&calNewR;
      functions.fpCalculateNewDensities=&calNewD_RT;
      functions.fpCalculateAveDensities=&calNewDenave_RT;
      functions.fpUpdateLocalBoundaryVelocitiesNewGrid=&updateLocalBoundaryVelocitiesNewGrid_RT;
      if(parameters.bEOSGammaLaw){//use gamma law gas
        functions.fpCalculateDeltat=&calDelt_RT_GL;
        functions.fpCalculateNewEOSVars=&calNewP_GL;
        functions.fpCalculateNewAV=&calNewQ0Q1_RT_GL;
        functions.fpModelWrite=&modelWrite_GL;
        functions.fpWriteWatchZones=&writeWatchZones_RT_GL;
      }
      else{//use tabulated equation of state
        functions.fpCalculateDeltat=&calDelt_RT_TEOS;
        functions.fpCalculateNewEOSVars=&calNewTPKappaGamma_TEOS;
        functions.fpCalculateNewAV=&calNewQ0Q1_RT_TEOS;
        functions.fpModelWrite=&modelWrite_TEOS;
        functions.fpWriteWatchZones=&writeWatchZones_RT_TEOS;
      }
      
      //velocity equation
      if(parameters.nTypeTurbulanceMod>0){
        functions.fpCalculateNewVelocities=&calNewVelocities_RT_LES;
      }
      else{
        functions.fpCalculateNewVelocities=&calNewVelocities_RT;
      }
      
      if(parameters.nTypeTurbulanceMod==1){
        functions.fpCalculateNewEddyVisc=&calNewEddyVisc_RT_CN;
      }
      else if(parameters.nTypeTurbulanceMod==2){
        functions.fpCalculateNewEddyVisc=&calNewEddyVisc_RT_SM;
      }
      
      //energy equation, eddy viscosity
      if(parameters.bAdiabatic){//adiabatic
        functions.fpCalculateNewEnergies=&calNewE_RT_AD;
      }
      else{//non-adaibatic
        if(!parameters.bEOSGammaLaw){//needs a tabulated equation of state
          if(parameters.nTypeTurbulanceMod>0){//with turbulance model
            functions.fpCalculateNewEnergies=&calNewE_RT_NA_LES;
          }
          else{
            functions.fpCalculateNewEnergies=&calNewE_RT_NA;
          }
          if(implicit.nNumImplicitZones>0){//implicit, requires non-adiabatic
            functions.fpImplicitSolve=&implicitSolve_RT;
            if(parameters.nTypeTurbulanceMod>0){
              functions.fpImplicitEnergyFunction=&dImplicitEnergyFunction_RT_LES;
              functions.fpImplicitEnergyFunction_SB=&dImplicitEnergyFunction_RT_LES_SB;
            }
            else{
              functions.fpImplicitEnergyFunction=&dImplicitEnergyFunction_RT;
              functions.fpImplicitEnergyFunction_SB=&dImplicitEnergyFunction_RT_SB;
            }
          }
        }
        else{//can't do a non-adiabatic calculation, with a gamma-law gas
          std::stringstream ssTemp;
          ssTemp<<__FILE__<<":"<<__FUNCTION__<<":"<<__LINE__<<":"<<procTop.nRank
            <<": User selected to do a non-adiabatic calculation but starting model uses a "
            <<"gamma-law gas. Starting model must use a tabulated equation of state in order to "
            <<"perform a non-adiabatic calculation.\n";
          throw exception2(ssTemp.str(),CALCULATION);
        }
      }
    }
    else if(grid.nNumDims==1){//use 1D for all
      functions.fpCalculateNewGridVelocities=&calNewU0_R;
      functions.fpCalculateNewRadii=&calNewR;
      functions.fpCalculateNewDensities=&calNewD_R;
      functions.fpCalculateAveDensities=&calNewDenave_None;
      functions.fpUpdateLocalBoundaryVelocitiesNewGrid=&updateLocalBoundaryVelocitiesNewGrid_R;
      if(parameters.bEOSGammaLaw){//use gamma law gas
        functions.fpCalculateDeltat=&calDelt_R_GL;
        functions.fpCalculateNewEOSVars=&calNewP_GL;
        functions.fpCalculateNewAV=&calNewQ0_R_GL;
        functions.fpModelWrite=&modelWrite_GL;
        functions.fpWriteWatchZones=&writeWatchZones_R_GL;
      }
      else{//use tabulated equation of state
        functions.fpCalculateDeltat=&calDelt_R_TEOS;
        functions.fpCalculateNewEOSVars=&calNewTPKappaGamma_TEOS;
        functions.fpCalculateNewAV=&calNewQ0_R_TEOS;
        functions.fpModelWrite=&modelWrite_TEOS;
        functions.fpWriteWatchZones=&writeWatchZones_R_TEOS;
      }
      
      //velocity equation
      functions.fpCalculateNewVelocities=&calNewVelocities_R;
      
      //energy equation, eddy viscosity
      if(parameters.bAdiabatic){//adiabatic
        functions.fpCalculateNewEnergies=&calNewE_R_AD;
      }
      else{//non-adaibatic
        if(!parameters.bEOSGammaLaw){//needs a tabulated equation of state
          functions.fpCalculateNewEnergies=&calNewE_R_NA;
          functions.fpCalculateNewEddyVisc=&calNewEddyVisc_None;
          if(implicit.nNumImplicitZones>0){//implicit, requires non-adiabatic
            functions.fpImplicitSolve=&implicitSolve_R;
            functions.fpImplicitEnergyFunction=&dImplicitEnergyFunction_R;
            functions.fpImplicitEnergyFunction_SB=&dImplicitEnergyFunction_R_SB;
          }
        }
        else{//can't do a non-adiabatic calculation, with a gamma-law gas
          std::stringstream ssTemp;
          ssTemp<<__FILE__<<":"<<__FUNCTION__<<":"<<__LINE__<<":"<<procTop.nRank
            <<": User selected to do a non-adiabatic calculation but starting model uses a "
            <<"gamma-law gas. Starting model must use a tabulated equation of state in order to "
            <<"perform a non-adiabatic calculation.\n";
          throw exception2(ssTemp.str(),CALCULATION);
        }
      }
    }
  }
  if(!time.bVariableTimeStep){//if not a variable time step use a constant one
    functions.fpCalculateDeltat=&calDelt_CONST;
  }
}
void setInternalVarInf(Grid &grid, Parameters &parameters){
  
  //allocate space for internal variable infos
  for(int n=grid.nNumVars;n<grid.nNumVars+grid.nNumIntVars;n++){
    grid.nVariables[n]=new int[4];//+1 because of keeping track of time info
  }
  
  //VARIABLE INFOS.
  
  //P
  grid.nVariables[grid.nP][0]=0;//r centered
  grid.nVariables[grid.nP][1]=0;//centered in theta
  grid.nVariables[grid.nP][2]=0;//centered in phi
  grid.nVariables[grid.nP][3]=1;//updated with time
  
  
  //Q0
  grid.nVariables[grid.nQ0][0]=0;//r centered
  grid.nVariables[grid.nQ0][1]=0;//theta centered
  grid.nVariables[grid.nQ0][2]=0;//phi centered
  grid.nVariables[grid.nQ0][3]=1;//updated with time
  
  if(!parameters.bEOSGammaLaw){  //If using TEOS these are extra internal variables
    
    //E
    grid.nVariables[grid.nE][0]=0;//r centered
    grid.nVariables[grid.nE][1]=0;//theta centered
    grid.nVariables[grid.nE][2]=0;//phi centered
    grid.nVariables[grid.nE][3]=1;//updated with time
    
    //KAPPA
    grid.nVariables[grid.nKappa][0]=0;//r centered
    grid.nVariables[grid.nKappa][1]=0;//centered in theta
    grid.nVariables[grid.nKappa][2]=0;//centered in phi
    grid.nVariables[grid.nKappa][3]=1;//updated with time
  
    //GAMMA
    grid.nVariables[grid.nGamma][0]=0;//r centered
    grid.nVariables[grid.nGamma][1]=0;//centered in theta
    grid.nVariables[grid.nGamma][2]=0;//centered in phi
    grid.nVariables[grid.nGamma][3]=1;//updated with time
    
    //Eddy viscosity
    if(parameters.nTypeTurbulanceMod>0){
      grid.nVariables[grid.nEddyVisc][0]=0;//r centered
      grid.nVariables[grid.nEddyVisc][1]=0;//centered in theta
      grid.nVariables[grid.nEddyVisc][2]=0;//centered in phi
      grid.nVariables[grid.nEddyVisc][3]=1;//updated with time
    }
  }
  if(grid.nNumDims>1){//not defined for 1D
    
    //DENAVE
    grid.nVariables[grid.nDenAve][0]=0;//r centered
    grid.nVariables[grid.nDenAve][1]=-1;//not defined in theta
    grid.nVariables[grid.nDenAve][2]=-1;//not defined in phi
    grid.nVariables[grid.nDenAve][3]=1;//updated with time
    
    //DCOSTHETAIJK
    grid.nVariables[grid.nDCosThetaIJK][0]=-1;//not defined in r
    grid.nVariables[grid.nDCosThetaIJK][1]=0;//theta centered
    grid.nVariables[grid.nDCosThetaIJK][2]=-1;//not defined in phi
    grid.nVariables[grid.nDCosThetaIJK][3]=0;//not updated with time
    
    //Q1
    grid.nVariables[grid.nQ1][0]=0;//r centered
    grid.nVariables[grid.nQ1][1]=0;//theta centered
    grid.nVariables[grid.nQ1][2]=0;//phi centered
    grid.nVariables[grid.nQ1][3]=1;//updated with time
    
    //DTHETA
    grid.nVariables[grid.nDTheta][0]=-1;//not defined in r
    grid.nVariables[grid.nDTheta][1]=0;//theta centered
    grid.nVariables[grid.nDTheta][2]=-1;//not defined in phi
    grid.nVariables[grid.nDTheta][3]=0;//not updated with time
    
    //SINTHETAIJK
    grid.nVariables[grid.nSinThetaIJK][0]=-1;//not defined in r
    grid.nVariables[grid.nSinThetaIJK][1]=0;//theta centered
    grid.nVariables[grid.nSinThetaIJK][2]=-1;//not defined in phi
    grid.nVariables[grid.nSinThetaIJK][3]=0;//not updated with time
    
    //SINTHETAIJP1HALFK
    grid.nVariables[grid.nSinThetaIJp1halfK][0]=-1;//not defined in r
    grid.nVariables[grid.nSinThetaIJp1halfK][1]=1;//theta interface
    grid.nVariables[grid.nSinThetaIJp1halfK][2]=-1;//not defined in phi
    grid.nVariables[grid.nSinThetaIJp1halfK][3]=0;//not updated with time
  }
  if(grid.nNumDims>2){//not defined for 1D or 2D
    
    //DPHI
    grid.nVariables[grid.nDPhi][0]=-1;//not defined in r
    grid.nVariables[grid.nDPhi][1]=-1;//not defined in theta
    grid.nVariables[grid.nDPhi][2]=0;//phi centered
    grid.nVariables[grid.nDPhi][3]=0;//not updated with time
    
    //Q2
    grid.nVariables[grid.nQ2][0]=0;//r centered
    grid.nVariables[grid.nQ2][1]=0;//theta centered
    grid.nVariables[grid.nQ2][2]=0;//phi centered
    grid.nVariables[grid.nQ2][3]=1;//updated with time
  }
  if(grid.nNumDims>2||(grid.nNumDims>1&&parameters.nTypeTurbulanceMod>0)){/* Need these for 3D 
    calculations and 2D calculations that use a turbulance model*/
    
    //COTTHETAIJP1HALFK
    grid.nVariables[grid.nCotThetaIJp1halfK][0]=-1;//not defined in r
    grid.nVariables[grid.nCotThetaIJp1halfK][1]=1;//theta interface
    grid.nVariables[grid.nCotThetaIJp1halfK][2]=-1;//not defined in phi
    grid.nVariables[grid.nCotThetaIJp1halfK][3]=0;//not updated with time
    
    //COTTHETAIJK
    grid.nVariables[grid.nCotThetaIJK][0]=-1;//not defined in r
    grid.nVariables[grid.nCotThetaIJK][1]=0;//theta center
    grid.nVariables[grid.nCotThetaIJK][2]=-1;//not defined in phi
    grid.nVariables[grid.nCotThetaIJK][3]=0;//not updated with time
  }
  //adjust based on number of dimensions
  if(grid.nNumDims<3){
    for(int n=grid.nNumVars;n<grid.nNumVars+grid.nNumIntVars;n++){
      grid.nVariables[n][2]=-1;//not defined in phi direction if not 3D
    }
  }
  if(grid.nNumDims<2){
    for(int n=grid.nNumVars;n<grid.nNumVars+grid.nNumIntVars;n++){
      grid.nVariables[n][1]=-1;//not defined in theta direction if not 2D
    }
  }
}
void initInternalVars(Grid &grid, ProcTop &procTop, Parameters &parameters){
  /** \warning \f$\Delta \theta \f$, \f$\Delta \phi \f$, \f$\sin \theta_{i,j,k} \f$,
      \f$\Delta \cos\theta_{i,j,k} \f$, all don't have the first zone calculated. At the moment
      this is a ghost cell that doesn't matter, but it may become a problem if calculations require
      this quantity. This is an issue for quantities that aren't updated in time, as those that are
      will have boundary cells updated with periodic boundary conditions.
  */
  
  //set values from equation of state, they don't care if 1D, 2D or 3D
  if(parameters.bEOSGammaLaw){
    calOldP_GL(grid,parameters);//set pressure
  }
  else{
    //initialize P, E, Kappa and Gamma, suitable for both 1D and 3D regions
    calOldPEKappaGamma_TEOS(grid, parameters);//set pressure, energy, kappa, and gamma
  }
  
  if(procTop.nRank!=0){
    if(grid.nNumDims>1){//both 2D, and 3D
      
      //initialize DCOSTHETAIJK
      for(int j=1;j<grid.nLocalGridDims[procTop.nRank][grid.nDCosThetaIJK][grid.nTheta]
        +2*grid.nNumGhostCells;j++){
        
        //calculate j for interface centered quantities
        int nJInt=j+grid.nCenIntOffset[1];
        
        grid.dLocalGridOld[grid.nDCosThetaIJK][0][j][0]
          =cos(grid.dLocalGridOld[grid.nTheta][0][nJInt-1][0])
          -cos(grid.dLocalGridOld[grid.nTheta][0][nJInt][0]);
      }
      
      //initialize DTHETA
      for(int j=1;j<grid.nLocalGridDims[procTop.nRank][grid.nDTheta][grid.nTheta]
        +2*grid.nNumGhostCells;j++){
        
        //calculate j for interface centered quantities
        int nJInt=j+grid.nCenIntOffset[1];
        
        grid.dLocalGridOld[grid.nDTheta][0][j][0]=grid.dLocalGridOld[grid.nTheta][0][nJInt][0]
          -grid.dLocalGridOld[grid.nTheta][0][nJInt-1][0];
      }
      
      //initialize SINTHETAIJK
      for(int j=1;j<grid.nLocalGridDims[procTop.nRank][grid.nSinThetaIJK][grid.nTheta]
        +2*grid.nNumGhostCells;j++){
        
        //calculate j for interface centered quantities
        int nJInt=j+grid.nCenIntOffset[1];
        
        grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]
          =sin((grid.dLocalGridOld[grid.nTheta][0][nJInt][0]
          +grid.dLocalGridOld[grid.nTheta][0][nJInt-1][0])*0.5);
      }
    
      //initialize SINTHETAIJP1HALFK
      for(int j=0;j<grid.nLocalGridDims[procTop.nRank][grid.nSinThetaIJp1halfK][grid.nTheta]
        +2*grid.nNumGhostCells;j++){
        
        grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][j][0]
          =sin(grid.dLocalGridOld[grid.nTheta][0][j][0]);
      }
    }
    if(grid.nNumDims==2){//only 2D
      
      //initialize DENAVE
      calOldDenave_RT(grid);
    }
    if(grid.nNumDims==3){//only 3D
      
      //initialize DPHI
      for(int k=1;k<grid.nLocalGridDims[procTop.nRank][grid.nDPhi][grid.nPhi]
        +2*grid.nNumGhostCells;k++){
        
        //calculate k for interface centered quantities
        int nKInt=k+grid.nCenIntOffset[2];
        
        grid.dLocalGridOld[grid.nDPhi][0][0][k]=grid.dLocalGridOld[grid.nPhi][0][0][nKInt]
          -grid.dLocalGridOld[grid.nPhi][0][0][nKInt-1];
      }
      
      //initialize DENAVE
      calOldDenave_RTP(grid);
    }
    if(grid.nNumDims>2||(grid.nNumDims>1&&parameters.nTypeTurbulanceMod>0)){/* Need these for 3D and
      2D calculations that use a turbulance model*/
      
      //initialize COTTHETAIJP1HALFK
      for(int j=0;j<grid.nLocalGridDims[procTop.nRank][grid.nCotThetaIJp1halfK][grid.nTheta]
        +2*grid.nNumGhostCells;j++){
        
        grid.dLocalGridOld[grid.nCotThetaIJp1halfK][0][j][0]
          =1.0/tan(grid.dLocalGridOld[grid.nTheta][0][j][0]);
      }
      
      //initialize COTTHETAIJK
      for(int j=1;j<grid.nLocalGridDims[procTop.nRank][grid.nCotThetaIJK][grid.nTheta]
        +2*grid.nNumGhostCells;j++){
        
        //calculate j for interface centered quantities
        int nJInt=j+grid.nCenIntOffset[1];
        double dTheta_ijk=(grid.dLocalGridOld[grid.nTheta][0][nJInt][0]
          +grid.dLocalGridOld[grid.nTheta][0][nJInt-1][0])*0.5;
        grid.dLocalGridOld[grid.nCotThetaIJK][0][j][0]=1.0/tan(dTheta_ijk);
      }
    }
      
    //initialize Q (Artificial Viscosity), donor fraction, and maximum convective velocity
    if(parameters.bEOSGammaLaw){//Gamma law
      if(grid.nNumDims==1){
        calOldQ0_R_GL(grid,parameters);
        initDonorFracAndMaxConVel_R_GL(grid,parameters);
      }
      if(grid.nNumDims==2){
        calOldQ0Q1_RT_GL(grid,parameters);
        initDonorFracAndMaxConVel_RT_GL(grid,parameters);
      }
      if(grid.nNumDims==3){
        calOldQ0Q1Q2_RTP_GL(grid,parameters);
        initDonorFracAndMaxConVel_RTP_GL(grid,parameters);
      }
    }
    else{//tabulated equation of state
      if(grid.nNumDims==1){
        calOldQ0_R_TEOS(grid,parameters);
        initDonorFracAndMaxConVel_R_TEOS(grid,parameters);
      }
      if(grid.nNumDims==2){
        calOldQ0Q1_RT_TEOS(grid,parameters);
        initDonorFracAndMaxConVel_RT_TEOS(grid,parameters);
      }
      if(grid.nNumDims==3){
        calOldQ0Q1Q2_RTP_TEOS(grid,parameters);
        initDonorFracAndMaxConVel_RTP_TEOS(grid,parameters);
      }
    }
    
    //if using a turblance model, initilize the eddy viscosity
    if(parameters.nTypeTurbulanceMod==1){
      if(grid.nNumDims==1){
        calOldEddyVisc_R_CN(grid,parameters);
      }
      if(grid.nNumDims==2){
        calOldEddyVisc_RT_CN(grid,parameters);
      }
      if(grid.nNumDims==3){
        calOldEddyVisc_RTP_CN(grid,parameters);
      }
    }
    if(parameters.nTypeTurbulanceMod==2){
      if(grid.nNumDims==1){
        calOldEddyVisc_R_SM(grid,parameters);
      }
      if(grid.nNumDims==2){
        calOldEddyVisc_RT_SM(grid,parameters);
      }
      if(grid.nNumDims==3){
        calOldEddyVisc_RTP_SM(grid,parameters);
      }
    }
  }
  else{//processor 0, always 1D
    
    //initialize DENAVE only if number of dimensions greater than one, this will allow the grid in
    //the 1D region to be compatible with the grid in the 3D region for message passing perposes
    if(grid.nNumDims>1){
      calOldDenave_R(grid);
    }
    
    //initialize Q (Artificial Viscosity), donor fraction, and maximum convective velocity
    if(parameters.bEOSGammaLaw){
      calOldQ0_R_GL(grid,parameters);
      initDonorFracAndMaxConVel_R_GL(grid,parameters);
    }
    else{
      calOldQ0_R_TEOS(grid,parameters);
      initDonorFracAndMaxConVel_R_TEOS(grid,parameters);
    }
  }
}
void calNewVelocities_R(Grid &grid,Parameters &parameters,Time &time,ProcTop &procTop){
  
  //calcualte new radial velocities
  calNewU_R(grid,parameters,time,procTop);
}
void calNewVelocities_R_LES(Grid &grid,Parameters &parameters,Time &time,ProcTop &procTop){
  
  //calcualte new radial velocities
  calNewU_R_LES(grid,parameters,time,procTop);
}
void calNewVelocities_RT(Grid &grid,Parameters &parameters,Time &time,ProcTop &procTop){
  
  //calcualte new radial velocities
  calNewU_RT(grid,parameters,time,procTop);
  
  //calculate new theta velocities
  calNewV_RT(grid,parameters,time,procTop);
}
void calNewVelocities_RT_LES(Grid &grid,Parameters &parameters,Time &time,ProcTop &procTop){
  
  //calcualte new radial velocities
  calNewU_RT_LES(grid,parameters,time,procTop);
  
  //calculate new theta velocities
  calNewV_RT_LES(grid,parameters,time,procTop);
}
void calNewVelocities_RTP(Grid &grid,Parameters &parameters,Time &time,ProcTop &procTop){
  
  //calcualte new radial velocities
  calNewU_RTP(grid,parameters,time,procTop);
  
  //calculate new theta velocities
  calNewV_RTP(grid,parameters,time,procTop);
  
  //calculate new phi velocities
  calNewW_RTP(grid,parameters,time,procTop);
}
void calNewVelocities_RTP_LES(Grid &grid,Parameters &parameters,Time &time,ProcTop &procTop){
  
  //calcualte new radial velocities
  calNewU_RTP_LES(grid,parameters,time,procTop);
  
  //calculate new theta velocities
  calNewV_RTP_LES(grid,parameters,time,procTop);
  
  //calculate new phi velocities
  calNewW_RTP_LES(grid,parameters,time,procTop);
}
void calNewU_R(Grid &grid,Parameters &parameters,Time &time,ProcTop &procTop){
  int i;
  int j;
  int k;
  int nICen;
  double dRho_ip1halfjk_n;
  double dU_ip1jk_nm1half;
  double dU_ijk_nm1half;
  double dU_ip1halfjk_nm1half;
  double dU0_ip1half_nm1half;
  double dP_ip1jk_n;
  double dP_ijk_n;
  double dA1;
  double dS1;
  double dRSq_ip1half_n;
  double dS4;
  double dA1CenGrad;
  double dA1UpWindGrad;
  double dU_U0_Diff;
  for(i=grid.nStartUpdateExplicit[grid.nU][0];i<grid.nEndUpdateExplicit[grid.nU][0];i++){
    
    //calculate i of centered quantities
    nICen=i-grid.nCenIntOffset[0];
    dU0_ip1half_nm1half=grid.dLocalGridOld[grid.nU0][i][0][0];
    
    for(j=grid.nStartUpdateExplicit[grid.nU][1];j<grid.nEndUpdateExplicit[grid.nU][1];j++){
      for(k=grid.nStartUpdateExplicit[grid.nU][2];k<grid.nEndUpdateExplicit[grid.nU][2];k++){
        
        //calculate interpolated quantities
        dRho_ip1halfjk_n=(grid.dLocalGridOld[grid.nD][nICen+1][j][k]
          +grid.dLocalGridOld[grid.nD][nICen][j][k])*0.5;
        dU_ip1jk_nm1half=(grid.dLocalGridOld[grid.nU][i+1][j][k]
          +grid.dLocalGridOld[grid.nU][i][j][k])*0.5;
        dU_ijk_nm1half=(grid.dLocalGridOld[grid.nU][i+1][j][k]
          +grid.dLocalGridOld[grid.nU][i][j][k])*0.5;
        dU_ip1halfjk_nm1half=grid.dLocalGridOld[grid.nU][i][j][k];
        dP_ip1jk_n=grid.dLocalGridOld[grid.nP][nICen+1][j][k]
          +grid.dLocalGridOld[grid.nQ0][nICen+1][j][k];
        dP_ijk_n=grid.dLocalGridOld[grid.nP][nICen][j][k]
          +grid.dLocalGridOld[grid.nQ0][nICen][j][k];
        dRSq_ip1half_n=grid.dLocalGridOld[grid.nR][i][0][0]*grid.dLocalGridOld[grid.nR][i][0][0];
          
        //Calculate dA1
        dA1CenGrad=(dU_ip1jk_nm1half-dU_ijk_nm1half)
          /(grid.dLocalGridOld[grid.nDM][nICen+1][0][0]
          +grid.dLocalGridOld[grid.nDM][nICen][0][0])*2.0;
        dA1UpWindGrad=0.0;
        dU_U0_Diff=grid.dLocalGridOld[grid.nU][i][j][k]
          -grid.dLocalGridOld[grid.nU0][i][0][0];
        if(dU_U0_Diff<0.0){//moving from outside in
          dA1UpWindGrad=(grid.dLocalGridOld[grid.nU][i+1][j][k]
            -grid.dLocalGridOld[grid.nU][i][j][k])
            /grid.dLocalGridOld[grid.nDM][nICen+1][0][0];
        }
        else{//moving from inside out
          dA1UpWindGrad=(grid.dLocalGridOld[grid.nU][i][j][k]
            -grid.dLocalGridOld[grid.nU][i-1][j][k])
            /grid.dLocalGridOld[grid.nDM][nICen][0][0];
        }
        dA1=dU_U0_Diff*((1.0-parameters.dDonorFrac)*dA1CenGrad+parameters.dDonorFrac
          *dA1UpWindGrad);
        
        //calculate source terms in x1-direction
        dS1=(dP_ip1jk_n-dP_ijk_n)/(grid.dLocalGridOld[grid.nDM][nICen+1][0][0]
          +grid.dLocalGridOld[grid.nDM][nICen][0][0])*2.0/dRho_ip1halfjk_n;
        dS4=parameters.dG*grid.dLocalGridOld[grid.nM][i][0][0]/dRSq_ip1half_n;
        
        //calculate new velocity
        grid.dLocalGridNew[grid.nU][i][j][k]=grid.dLocalGridOld[grid.nU][i][j][k]
          -time.dDeltat_n*(4.0*parameters.dPi*dRho_ip1halfjk_n*dRSq_ip1half_n*(dA1+dS1)+dS4);
      }
    }
  }
  
  //ghost region 0, outter most ghost region in x1 direction
  for(i=grid.nStartGhostUpdateExplicit[grid.nU][0][0];i<grid.nEndGhostUpdateExplicit[grid.nU][0][0];
    i++){
    
    //calculate i of centered quantities
    nICen=i-grid.nCenIntOffset[0];
    
    for(j=grid.nStartGhostUpdateExplicit[grid.nU][0][1];
      j<grid.nEndGhostUpdateExplicit[grid.nU][0][1];j++){
      for(k=grid.nStartGhostUpdateExplicit[grid.nU][0][2];
        k<grid.nEndGhostUpdateExplicit[grid.nU][0][2];k++){
        
        //calculate interpolated quantities
        dRho_ip1halfjk_n=(0.0+grid.dLocalGridOld[grid.nD][nICen][j][k])*0.5;/**\BC Missing 
          grid.dLocalGridOld[grid.nD][nICen+1][j][k] in calculation of \f$\rho_{i+1/2}\f$, setting
           it to 0.0*/
        dU_ip1jk_nm1half=grid.dLocalGridOld[grid.nU][i][j][k];
        dU_ijk_nm1half=(grid.dLocalGridOld[grid.nU][i][j][k]+grid.dLocalGridOld[grid.nU][i-1][j][k])
          *0.5;
        
        //calculate advection term in x1-direction
        dP_ijk_n=grid.dLocalGridOld[grid.nP][nICen][j][k]+grid.dLocalGridOld[grid.nQ0][nICen][j][k];
        
        dP_ip1jk_n=-1.0*dP_ijk_n;/**\BC Missing grid.dLocalGridOld[grid.nP][nICen+1][j][k] in 
          calculation of \f$S_1\f$, setting it to -1.0*grid.dLocalGridOld[grid.nP][nICen][j][k].*/
        
        //Calculate dA1
        dA1CenGrad=(dU_ip1jk_nm1half-dU_ijk_nm1half)/grid.dLocalGridOld[grid.nDM][nICen][0][0]*2.0;
          /**\BC Missing grid.dLocalGridOld[grid.nDM][nICen+1][0][0] in calculation of centered 
          \f$A_1\f$ gradient, setting it to zero.*/
        dA1UpWindGrad=0.0;
        if(grid.dLocalGridOld[grid.nU][i][j][k]<0.0){//moving from outside in
          dA1UpWindGrad=dA1CenGrad;/**\BC Missing grid.dLocalGridOld[grid.nU][i+1][j][k] and 
            grid.dLocalGridOld[grid.nDM][nICen+1][0][0] in calculation of upwind gradient, when 
            moving inward. Using centered gradient instead.*/
        }
        else{//moving from inside out
          dA1UpWindGrad=(grid.dLocalGridOld[grid.nU][i][j][k]
            -grid.dLocalGridOld[grid.nU][i-1][j][k])/grid.dLocalGridOld[grid.nDM][nICen][0][0];
        }
        dA1=(grid.dLocalGridOld[grid.nU][i][j][k]-grid.dLocalGridOld[grid.nU0][i][0][0])
          *((1.0-parameters.dDonorFrac)*dA1CenGrad+parameters.dDonorFrac*dA1UpWindGrad);
        
        //calculate source terms in x1-direction
        dS1=(dP_ip1jk_n-dP_ijk_n)/(grid.dLocalGridOld[grid.nDM][nICen][0][0]*(0.5+parameters.dAlpha
        +parameters.dAlphaExtra))
          /dRho_ip1halfjk_n;
        dRSq_ip1half_n=grid.dLocalGridOld[grid.nR][i][0][0]*grid.dLocalGridOld[grid.nR][i][0][0];
        dS4=parameters.dG*grid.dLocalGridOld[grid.nM][i][0][0]/dRSq_ip1half_n;
        
        //calculate new velocity
        grid.dLocalGridNew[grid.nU][i][j][k]=grid.dLocalGridOld[grid.nU][i][j][k]
          -time.dDeltat_n*(4.0*parameters.dPi*dRho_ip1halfjk_n*dRSq_ip1half_n*(dA1+dS1)+dS4);
      }
    }
  }
  
  #if SEDOV==1
  //calculate gost region 1 inner most ghost region in x1 direction
  for(i=grid.nStartGhostUpdateExplicit[grid.nU][1][0];
    i<grid.nEndGhostUpdateExplicit[grid.nU][1][0];i++){
    
    //calculate i of centered quantities
    dU0_ip1half_nm1half=grid.dLocalGridOld[grid.nU0][i][0][0];
    nICen=i-grid.nCenIntOffset[0];
    
    for(j=grid.nStartGhostUpdateExplicit[grid.nU][1][1];
      j<grid.nEndGhostUpdateExplicit[grid.nU][1][1];j++){
      for(k=grid.nStartGhostUpdateExplicit[grid.nU][1][2];
        k<grid.nEndGhostUpdateExplicit[grid.nU][1][2];k++){
        
        //calculate interpolated quantities
        dRho_ip1halfjk_n=(grid.dLocalGridOld[grid.nD][nICen+1][j][k]
          +grid.dLocalGridOld[grid.nD][nICen][j][k])*0.5;
        dU_ip1jk_nm1half=(grid.dLocalGridOld[grid.nU][i+1][j][k]
          +grid.dLocalGridOld[grid.nU][i][j][k])*0.5;
        dU_ijk_nm1half=(grid.dLocalGridOld[grid.nU][i+1][j][k]
          +grid.dLocalGridOld[grid.nU][i][j][k])*0.5;
        dU_ip1halfjk_nm1half=grid.dLocalGridOld[grid.nU][i][j][k];
        
        //calculate advection term in x1-direction
        dP_ip1jk_n=grid.dLocalGridOld[grid.nP][nICen+1][j][k]
          +grid.dLocalGridOld[grid.nQ0][nICen+1][j][k];
        dP_ijk_n=grid.dLocalGridOld[grid.nP][nICen][j][k]
          +grid.dLocalGridOld[grid.nQ0][nICen][j][k];
        dA1=(dU_ip1halfjk_nm1half-dU0_ip1half_nm1half)*(dU_ip1jk_nm1half-dU_ijk_nm1half)/
          (grid.dLocalGridOld[grid.nDM][nICen+1][0][0]
          +grid.dLocalGridOld[grid.nDM][nICen][0][0])*2.0;
        
        //calculate source terms in x1-direction
        dS1=(dP_ip1jk_n-dP_ijk_n)/(grid.dLocalGridOld[grid.nDM][nICen+1][0][0]
          +grid.dLocalGridOld[grid.nDM][nICen][0][0])*2.0/dRho_ip1halfjk_n;
        dRSq_ip1half_n=grid.dLocalGridOld[grid.nR][i][0][0]*grid.dLocalGridOld[grid.nR][i][0][0];
        dS4=parameters.dG*grid.dLocalGridOld[grid.nM][i][0][0]/dRSq_ip1half_n;
        
        //calculate new velocity
        grid.dLocalGridNew[grid.nU][i][j][k]=grid.dLocalGridOld[grid.nU][i][j][k]
          -time.dDeltat_n*(4.0*parameters.dPi*dRho_ip1halfjk_n*dRSq_ip1half_n*(dA1+dS1)+dS4);
      }
    }
  }
  #endif
}
void calNewU_R_LES(Grid &grid,Parameters &parameters,Time &time,ProcTop &procTop){
  //calculate new u
  int i;
  int j;
  int k;
  int nICen;
  double dRho_ip1halfjk_n;
  double dP_ip1jk_n;
  double dP_ijk_n;
  double dA1;
  double dS1;
  double dS4;
  double dTA1;
  double dTS1;
  double dTS4;
  double dDivU_ijk_n;
  double dDivU_ip1jk_n;
  double dDivU_ip1halfjk_n;//used at surface boundary condition
  double dTau_rr_ip1jk_n;
  double dTau_rr_ip1halfjk_n;
  double dTau_rr_ijk_n;
  double dR_i_n;
  double dR_ip1_n;
  double dU_ip1jk_nm1half;
  double dU_ijk_nm1half;
  double dRSq_ip1half_n;
  double dRSq_ip1_n;
  double dRSq_i_n;
  double dRSq_ip3half_n;
  double dRSq_im1half_n;
  double dRSqU_ip3halfjk_n;
  double dRSqU_ip1halfjk_n;
  double dRSqU_im1halfjk_n;
  double dDM_ip1half;
  double dEddyVisc_ip1halfjk_n;
  double dA1CenGrad;
  double dA1UpWindGrad;
  double dU_U0_Diff;
  for(i=grid.nStartUpdateExplicit[grid.nU][0];i<grid.nEndUpdateExplicit[grid.nU][0];i++){
    
    //calculate i of centered quantities
    nICen=i-grid.nCenIntOffset[0];
    dR_ip1_n=(grid.dLocalGridOld[grid.nR][i+1][0][0]+grid.dLocalGridOld[grid.nR][i][0][0])*0.5;
    dR_i_n=(grid.dLocalGridOld[grid.nR][i][0][0]+grid.dLocalGridOld[grid.nR][i-1][0][0])*0.5;
    dRSq_ip1_n=dR_ip1_n*dR_ip1_n;
    dRSq_i_n=dR_i_n*dR_i_n;
    dRSq_ip1half_n=grid.dLocalGridOld[grid.nR][i][0][0]*grid.dLocalGridOld[grid.nR][i][0][0];
    dRSq_ip3half_n=grid.dLocalGridOld[grid.nR][i+1][0][0]*grid.dLocalGridOld[grid.nR][i+1][0][0];
    dRSq_im1half_n=grid.dLocalGridOld[grid.nR][i-1][0][0]*grid.dLocalGridOld[grid.nR][i-1][0][0];
    dDM_ip1half=(grid.dLocalGridOld[grid.nDM][nICen+1][0][0]
      +grid.dLocalGridOld[grid.nDM][nICen][0][0])*0.5;
    
    for(j=grid.nStartUpdateExplicit[grid.nU][1];j<grid.nEndUpdateExplicit[grid.nU][1];j++){
      for(k=grid.nStartUpdateExplicit[grid.nU][2];k<grid.nEndUpdateExplicit[grid.nU][2];k++){
        
        //calculate interpolated quantities
        dRho_ip1halfjk_n=(grid.dLocalGridOld[grid.nD][nICen+1][j][k]
          +grid.dLocalGridOld[grid.nD][nICen][j][k])*0.5;
        dU_ip1jk_nm1half=(grid.dLocalGridOld[grid.nU][i+1][j][k]
          +grid.dLocalGridOld[grid.nU][i][j][k])*0.5;
        dU_ijk_nm1half=(grid.dLocalGridOld[grid.nU][i+1][j][k]
          +grid.dLocalGridOld[grid.nU][i][j][k])*0.5;
        dEddyVisc_ip1halfjk_n=(grid.dLocalGridOld[grid.nEddyVisc][nICen][j][k]
          +grid.dLocalGridOld[grid.nEddyVisc][nICen+1][j][k])*0.5;
        dP_ip1jk_n=grid.dLocalGridOld[grid.nP][nICen+1][j][k]
          +grid.dLocalGridOld[grid.nQ0][nICen+1][j][k];
        dP_ijk_n=grid.dLocalGridOld[grid.nP][nICen][j][k]
          +grid.dLocalGridOld[grid.nQ0][nICen][j][k];
        
        //calculate derived quantities
        dRSqU_ip3halfjk_n=dRSq_ip3half_n*grid.dLocalGridOld[grid.nU][i+1][j][k];
        dRSqU_ip1halfjk_n=dRSq_ip1half_n*grid.dLocalGridOld[grid.nU][i][j][k];
        dRSqU_im1halfjk_n=dRSq_im1half_n*grid.dLocalGridOld[grid.nU][i-1][j][k];
        
        //cal DivU_ip1jk_n
        dDivU_ip1jk_n=4.0*parameters.dPi*grid.dLocalGridOld[grid.nD][nICen+1][0][0]
          *(dRSqU_ip3halfjk_n-dRSqU_ip1halfjk_n)/grid.dLocalGridOld[grid.nDM][nICen+1][0][0];
        
        //cal DivU_ijk_n
        dDivU_ijk_n=4.0*parameters.dPi*grid.dLocalGridOld[grid.nD][nICen][0][0]
          *(dRSqU_ip1halfjk_n-dRSqU_im1halfjk_n)/grid.dLocalGridOld[grid.nDM][nICen][0][0];
        
        //cal Tau_rr_ip1jk_n
        dTau_rr_ip1jk_n=2.0*grid.dLocalGridOld[grid.nEddyVisc][nICen+1][j][k]*(4.0*parameters.dPi
          *dRSq_ip1_n*grid.dLocalGridOld[grid.nD][nICen+1][0][0]
          *(grid.dLocalGridOld[grid.nU][i+1][j][k]-grid.dLocalGridOld[grid.nU][i][j][k])
          /grid.dLocalGridOld[grid.nDM][nICen+1][0][0]-0.3333333333333333*dDivU_ip1jk_n);
        
        //cal Tau_rr_ijk_n
        dTau_rr_ijk_n=2.0*grid.dLocalGridOld[grid.nEddyVisc][nICen][j][k]*(4.0*parameters.dPi
          *dRSq_i_n*grid.dLocalGridOld[grid.nD][nICen][0][0]*(grid.dLocalGridOld[grid.nU][i][j][k]
          -grid.dLocalGridOld[grid.nU][i-1][j][k])/grid.dLocalGridOld[grid.nDM][nICen][0][0]
          -0.3333333333333333*dDivU_ijk_n);
        
        //cal dTA1
        dTA1=1.0/dRho_ip1halfjk_n*(dTau_rr_ip1jk_n-dTau_rr_ijk_n)/dDM_ip1half;
        
        //cal dTS1
        dTS1=dEddyVisc_ip1halfjk_n/(dRho_ip1halfjk_n*grid.dLocalGridOld[grid.nR][i][0][0])*(4.0
          *(dU_ip1jk_nm1half-dU_ijk_nm1half)/dDM_ip1half);
        
        //cal dTS4
        dTS4=4.0*grid.dLocalGridOld[grid.nU][i][j][k]/grid.dLocalGridOld[grid.nR][i][0][0];
        
        //Calculate dA1
        dA1CenGrad=(dU_ip1jk_nm1half-dU_ijk_nm1half)
          /(grid.dLocalGridOld[grid.nDM][nICen+1][0][0]
          +grid.dLocalGridOld[grid.nDM][nICen][0][0])*2.0;
        dA1UpWindGrad=0.0;
        dU_U0_Diff=grid.dLocalGridOld[grid.nU][i][j][k]
          -grid.dLocalGridOld[grid.nU0][i][0][0];
        if(dU_U0_Diff<0.0){//moving from outside in
          dA1UpWindGrad=(grid.dLocalGridOld[grid.nU][i+1][j][k]
            -grid.dLocalGridOld[grid.nU][i][j][k])
            /grid.dLocalGridOld[grid.nDM][nICen+1][0][0];
        }
        else{//moving from inside out
          dA1UpWindGrad=(grid.dLocalGridOld[grid.nU][i][j][k]
            -grid.dLocalGridOld[grid.nU][i-1][j][k])
            /grid.dLocalGridOld[grid.nDM][nICen][0][0];
        }
        dA1=dU_U0_Diff*((1.0-parameters.dDonorFrac)*dA1CenGrad+parameters.dDonorFrac
          *dA1UpWindGrad);
        
        //calculate dS1
        dS1=(dP_ip1jk_n-dP_ijk_n)/(grid.dLocalGridOld[grid.nDM][nICen+1][0][0]
          +grid.dLocalGridOld[grid.nDM][nICen][0][0])*2.0/dRho_ip1halfjk_n;
        
        //calculate dS4
        dS4=parameters.dG*grid.dLocalGridOld[grid.nM][i][0][0]/dRSq_ip1half_n;
        
        //calculate new velocity
        grid.dLocalGridNew[grid.nU][i][j][k]=grid.dLocalGridOld[grid.nU][i][j][k]
          -time.dDeltat_n*(4.0*parameters.dPi*dRho_ip1halfjk_n*dRSq_ip1half_n*(dA1+dS1+dTA1+dTS1)
          +dS4+dEddyVisc_ip1halfjk_n/(dRho_ip1halfjk_n*grid.dLocalGridOld[grid.nR][i][0][0])
          *(dTS4));
      }
    }
  }
  
  //ghost region 0, outter most ghost region in x1 direction
  for(i=grid.nStartGhostUpdateExplicit[grid.nU][0][0];
    i<grid.nEndGhostUpdateExplicit[grid.nU][0][0];i++){
    
    //calculate i of centered quantities
    nICen=i-grid.nCenIntOffset[0];
    dR_i_n=(grid.dLocalGridOld[grid.nR][i][0][0]+grid.dLocalGridOld[grid.nR][i-1][0][0])*0.5;
    dRSq_i_n=dR_i_n*dR_i_n;
    dRSq_ip1half_n=grid.dLocalGridOld[grid.nR][i][0][0]*grid.dLocalGridOld[grid.nR][i][0][0];
    dRSq_im1half_n=grid.dLocalGridOld[grid.nR][i-1][0][0]*grid.dLocalGridOld[grid.nR][i-1][0][0];
    dDM_ip1half=(0.0+grid.dLocalGridOld[grid.nDM][nICen][0][0])*0.5;
    
    for(j=grid.nStartGhostUpdateExplicit[grid.nU][0][1];
      j<grid.nEndGhostUpdateExplicit[grid.nU][0][1];j++){
      for(k=grid.nStartGhostUpdateExplicit[grid.nU][0][2];
        k<grid.nEndGhostUpdateExplicit[grid.nU][0][2];k++){
        
        //calculate interpolated quantities
        dRho_ip1halfjk_n=(0.0+grid.dLocalGridOld[grid.nD][nICen][j][k])*0.5;/**\BC Missing 
          grid.dLocalGridOld[grid.nD][nICen+1][j][k] in calculation of \f$\rho_{i+1/2}\f$, setting
          it to 0.0*/
        dU_ip1jk_nm1half=grid.dLocalGridOld[grid.nU][i][j][k];/**\BC missing 
          grid.dLocalGridOld[grid.nU][i+1][j][k] using velocity at i*/
        dU_ijk_nm1half=(grid.dLocalGridOld[grid.nU][i][j][k]
          +grid.dLocalGridOld[grid.nU][i-1][j][k])*0.5;
        dEddyVisc_ip1halfjk_n=(grid.dLocalGridOld[grid.nEddyVisc][nICen][j][k])*0.5;/**\BC Assuming 
          eddy viscosity outside model is zero.*/
        
        //calculate derived quantities
        /*dRSqU_ip3halfjk_n=dRSq_ip3half_n*grid.dLocalGridOld[grid.nU][i][j][k]; missing outside 
          velocity */
        dRSqU_ip1halfjk_n=dRSq_ip1half_n*grid.dLocalGridOld[grid.nU][i][j][k];
        dRSqU_im1halfjk_n=dRSq_im1half_n*grid.dLocalGridOld[grid.nU][i-1][j][k];
        
        //calculate advection term in x1-direction
        dP_ijk_n=grid.dLocalGridOld[grid.nP][nICen][j][k]
          +grid.dLocalGridOld[grid.nQ0][nICen][j][k];
        dP_ip1jk_n=-1.0*dP_ijk_n;/**\BC Missing grid.dLocalGridOld[grid.nP][nICen+1][j][k] in 
          calculation of \f$S_1\f$, setting it to -1.0*grid.dLocalGridOld[grid.nP][nICen][j][k].*/
        
        //Calculate dA1
        dA1CenGrad=(dU_ip1jk_nm1half-dU_ijk_nm1half)/grid.dLocalGridOld[grid.nDM][nICen][0][0]*2.0;
          /**\BC Missing grid.dLocalGridOld[grid.nDM][nICen+1][0][0] in calculation of centered 
          \f$A_1\f$ gradient, setting it to zero.*/
        dA1UpWindGrad=0.0;
        if(grid.dLocalGridOld[grid.nU][i][j][k]<0.0){//moving from outside in
          dA1UpWindGrad=dA1CenGrad;/**\BC Missing grid.dLocalGridOld[grid.nU][i+1][j][k] and 
            grid.dLocalGridOld[grid.nDM][nICen+1][0][0] in calculation of upwind gradient, when 
            moving inward. Using centered gradient instead.*/
        }
        else{//moving from inside out
          dA1UpWindGrad=(grid.dLocalGridOld[grid.nU][i][j][k]
            -grid.dLocalGridOld[grid.nU][i-1][j][k])/grid.dLocalGridOld[grid.nDM][nICen][0][0];
        }
        dA1=(grid.dLocalGridOld[grid.nU][i][j][k]-grid.dLocalGridOld[grid.nU0][i][0][0])
          *((1.0-parameters.dDonorFrac)*dA1CenGrad+parameters.dDonorFrac*dA1UpWindGrad);
        
        //calculate source terms in x1-direction
        dS1=(dP_ip1jk_n-dP_ijk_n)/(grid.dLocalGridOld[grid.nDM][nICen][0][0]*(0.5+parameters.dAlpha
        +parameters.dAlphaExtra))
          /dRho_ip1halfjk_n;
        dS4=parameters.dG*grid.dLocalGridOld[grid.nM][i][0][0]/dRSq_ip1half_n;
        
        //cal DivU_ip1jk_n
        dDivU_ip1halfjk_n=4.0*parameters.dPi*dRho_ip1halfjk_n*(dRSqU_ip1halfjk_n-dRSqU_im1halfjk_n)
          /dDM_ip1half;
        
        //cal DivU_ijk_n
        dDivU_ijk_n=4.0*parameters.dPi*grid.dLocalGridOld[grid.nD][nICen][0][0]
          *(dRSqU_ip1halfjk_n-dRSqU_im1halfjk_n)/grid.dLocalGridOld[grid.nDM][nICen][0][0];
        
        //cal Tau_rr_ip1halfjk_n
        dTau_rr_ip1halfjk_n=2.0*dEddyVisc_ip1halfjk_n*(4.0*parameters.dPi*dRSq_ip1half_n
          *dRho_ip1halfjk_n*(grid.dLocalGridOld[grid.nU][i][j][k]-dU_ijk_nm1half)/dDM_ip1half
          -0.3333333333333333*dDivU_ip1halfjk_n);
        
        //cal dTS4
        dTS4=4.0*grid.dLocalGridOld[grid.nU][i][j][k]/grid.dLocalGridOld[grid.nR][i][0][0];
        
        //cal Tau_rr_ijk_n
        dTau_rr_ijk_n=2.0*grid.dLocalGridOld[grid.nEddyVisc][nICen][j][k]*(4.0*parameters.dPi
          *dRSq_i_n*grid.dLocalGridOld[grid.nD][nICen][0][0]*(grid.dLocalGridOld[grid.nU][i][j][k]
          -grid.dLocalGridOld[grid.nU][i-1][j][k])/grid.dLocalGridOld[grid.nDM][nICen][0][0]
          -0.3333333333333333*dDivU_ijk_n);
        
        //cal dTA1
        dTA1=1.0/dRho_ip1halfjk_n*(dTau_rr_ip1halfjk_n-dTau_rr_ijk_n)/dDM_ip1half;
        
        //cal dTS1
        dTS1=dEddyVisc_ip1halfjk_n/(dRho_ip1halfjk_n*grid.dLocalGridOld[grid.nR][i][0][0])*(4.0
          *(grid.dLocalGridOld[grid.nU][i][j][k]-dU_ijk_nm1half)/dDM_ip1half);
        
        //calculate new velocity
        grid.dLocalGridNew[grid.nU][i][j][k]=grid.dLocalGridOld[grid.nU][i][j][k]
          -time.dDeltat_n*(4.0*parameters.dPi*dRho_ip1halfjk_n*dRSq_ip1half_n*(dA1+dS1)+dS4);
      }
    }
  }
}
void calNewU_RT(Grid &grid,Parameters &parameters,Time &time,ProcTop &procTop){
  int i;
  int j;
  int k;
  int nICen;
  int nJInt;
  double dR_ip1half_n_Sq;
  double dU_ip1jk_nm1half;
  double dU_ijk_nm1half;
  double dU_ip1halfjp1halfk_nm1half;
  double dU_ip1halfjm1halfk_nm1half;
  double dUmU0_ijk_nm1half;
  double dV_ip1halfjk_nm1half;
  double dRho_ip1halfjk_n;
  double dRhoAve_ip1halfjk_n;
  double dP_ip1jk_n;
  double dP_ijk_n;
  double dA1CenGrad;
  double dA1UpWindGrad;
  double dA1;
  double dS1;
  double dS4;
  double dA2CenGrad;
  double dA2UpWindGrad;
  double dA2;
  double dS2;
  
  //calculate new u
  for(i=grid.nStartUpdateExplicit[grid.nU][0];i<grid.nEndUpdateExplicit[grid.nU][0];i++){
    
    //calculate i of centered quantities
    nICen=i-grid.nCenIntOffset[0];
    dR_ip1half_n_Sq=grid.dLocalGridOld[grid.nR][i][0][0]*grid.dLocalGridOld[grid.nR][i][0][0];
    dRhoAve_ip1halfjk_n=(grid.dLocalGridOld[grid.nDenAve][nICen+1][0][0]
      +grid.dLocalGridOld[grid.nDenAve][nICen][0][0])*0.5;
    
    for(j=grid.nStartUpdateExplicit[grid.nU][1];j<grid.nEndUpdateExplicit[grid.nU][1];j++){
      
      //calculate j of interface quantities
      nJInt=j+grid.nCenIntOffset[1];
      
      for(k=grid.nStartUpdateExplicit[grid.nU][2];k<grid.nEndUpdateExplicit[grid.nU][2];k++){
        
        //CALCULATE INTERPOLATED QUANTITIES
        dU_ip1jk_nm1half=(grid.dLocalGridOld[grid.nU][i+1][j][k]
          +grid.dLocalGridOld[grid.nU][i][j][k])*0.5;
        dU_ijk_nm1half=(grid.dLocalGridOld[grid.nU][i][j][k]+grid.dLocalGridOld[grid.nU][i-1][j][k])
          *0.5;
        dU_ip1halfjp1halfk_nm1half=0.5*(grid.dLocalGridOld[grid.nU][i][j+1][k]
          +grid.dLocalGridOld[grid.nU][i][j][k]);
        dU_ip1halfjm1halfk_nm1half=0.5*(grid.dLocalGridOld[grid.nU][i][j][k]
          +grid.dLocalGridOld[grid.nU][i][j-1][k]);
        dV_ip1halfjk_nm1half=0.25*(grid.dLocalGridOld[grid.nV][nICen+1][nJInt][k]
          +grid.dLocalGridOld[grid.nV][nICen+1][nJInt-1][k]
          +grid.dLocalGridOld[grid.nV][nICen][nJInt][k]
          +grid.dLocalGridOld[grid.nV][nICen][nJInt-1][k]);
        dRho_ip1halfjk_n=(grid.dLocalGridOld[grid.nD][nICen+1][j][k]
          +grid.dLocalGridOld[grid.nD][nICen][j][k])*0.5;
        dP_ip1jk_n=grid.dLocalGridOld[grid.nP][nICen+1][j][k]
          +grid.dLocalGridOld[grid.nQ0][nICen+1][j][k];
        dP_ijk_n=grid.dLocalGridOld[grid.nP][nICen][j][k]
          +grid.dLocalGridOld[grid.nQ0][nICen][j][k];
        
        //Calculate dA1
        dA1CenGrad=(dU_ip1jk_nm1half-dU_ijk_nm1half)
          /(grid.dLocalGridOld[grid.nDM][nICen+1][0][0]
          +grid.dLocalGridOld[grid.nDM][nICen][0][0])*2.0;
        dA1UpWindGrad=0.0;
        dUmU0_ijk_nm1half=grid.dLocalGridOld[grid.nU][i][j][k]
          -grid.dLocalGridOld[grid.nU0][i][0][0];
        if(dUmU0_ijk_nm1half<0.0){//moving from outside in
          dA1UpWindGrad=(grid.dLocalGridOld[grid.nU][i+1][j][k]
            -grid.dLocalGridOld[grid.nU][i][j][k])
            /grid.dLocalGridOld[grid.nDM][nICen+1][0][0];
        }
        else{//moving from inside out
          dA1UpWindGrad=(grid.dLocalGridOld[grid.nU][i][j][k]
            -grid.dLocalGridOld[grid.nU][i-1][j][k])
            /grid.dLocalGridOld[grid.nDM][nICen][0][0];
        }
        dA1=dUmU0_ijk_nm1half*((1.0-parameters.dDonorFrac)*dA1CenGrad+parameters.dDonorFrac
          *dA1UpWindGrad);
        
        //calculate dS1
        dS1=(dP_ip1jk_n-dP_ijk_n)/((grid.dLocalGridOld[grid.nDM][nICen+1][0][0]
          +grid.dLocalGridOld[grid.nDM][nICen][0][0])*dRho_ip1halfjk_n)*2.0;
        
        //Calculate dS4
        dS4=parameters.dG*grid.dLocalGridOld[grid.nM][i][0][0]/dR_ip1half_n_Sq;
        
        //Calculate dA2
        dA2CenGrad=(dU_ip1halfjp1halfk_nm1half-dU_ip1halfjm1halfk_nm1half)
          /grid.dLocalGridOld[grid.nDTheta][0][j][0];
        dA2UpWindGrad=0.0;
        if(dV_ip1halfjk_nm1half>0.0){//moving in positive direction
          dA2UpWindGrad=(grid.dLocalGridOld[grid.nU][i][j][k]
            -grid.dLocalGridOld[grid.nU][i][j-1][k])
            /(grid.dLocalGridOld[grid.nDTheta][0][j][0]
            +grid.dLocalGridOld[grid.nDTheta][0][j-1][0])*2.0;
        }
        else{//moving in negative direction
          dA2UpWindGrad=(grid.dLocalGridOld[grid.nU][i][j+1][k]
            -grid.dLocalGridOld[grid.nU][i][j][k])
            /(grid.dLocalGridOld[grid.nDTheta][0][j+1][0]
            +grid.dLocalGridOld[grid.nDTheta][0][j][0])*2.0;
        }
        dA2=dV_ip1halfjk_nm1half*((1.0-parameters.dDonorFrac)*dA2CenGrad
          +parameters.dDonorFrac*dA2UpWindGrad)/grid.dLocalGridOld[grid.nR][i][0][0];
        
        //Calculate dS2
        dS2=dV_ip1halfjk_nm1half*dV_ip1halfjk_nm1half
          /grid.dLocalGridOld[grid.nR][i][0][0];
        
        //calculate new velocity
        grid.dLocalGridNew[grid.nU][i][j][k]=grid.dLocalGridOld[grid.nU][i][j][k]
          -time.dDeltat_n*(4.0*parameters.dPi*dRhoAve_ip1halfjk_n*dR_ip1half_n_Sq*(dA1+dS1)+dA2-dS2
          +dS4);
      }
    }
  }
  
  //ghost region 0, outter most ghost region in x1 direction
  for(i=grid.nStartGhostUpdateExplicit[grid.nU][0][0];
    i<grid.nEndGhostUpdateExplicit[grid.nU][0][0];i++){
    
    //calculate i of centered quantities
    nICen=i-grid.nCenIntOffset[0];
    dR_ip1half_n_Sq=grid.dLocalGridOld[grid.nR][i][0][0]*grid.dLocalGridOld[grid.nR][i][0][0];
    dRhoAve_ip1halfjk_n=(grid.dLocalGridOld[grid.nDenAve][nICen][0][0])*0.5;
    
    for(j=grid.nStartGhostUpdateExplicit[grid.nU][0][1];
      j<grid.nEndGhostUpdateExplicit[grid.nU][0][1];j++){
      
      //calculate j of interface quantities
      nJInt=j+grid.nCenIntOffset[1];
      
      for(k=grid.nStartGhostUpdateExplicit[grid.nU][0][2];
        k<grid.nEndGhostUpdateExplicit[grid.nU][0][2];k++){
        
        //CALCULATE INTERPOLATED QUANTITIES
        dU_ip1jk_nm1half=grid.dLocalGridOld[grid.nU][i][j][k];
        dU_ijk_nm1half=(grid.dLocalGridOld[grid.nU][i][j][k]
          +grid.dLocalGridOld[grid.nU][i-1][j][k])*0.5;/**\BC Missing 
          grid.dLocalGridOld[grid.nD][nICen+1][j][k] in calculation of \f$\rho_{i+1/2,j,k}\f$,
          setting it to zero. */
        dU_ip1halfjp1halfk_nm1half=(grid.dLocalGridOld[grid.nU][i][j+1][k]
          +grid.dLocalGridOld[grid.nU][i][j][k])*0.5;
        dU_ip1halfjm1halfk_nm1half=0.5*(grid.dLocalGridOld[grid.nU][i][j][k]
          +grid.dLocalGridOld[grid.nU][i][j-1][k]);
        dV_ip1halfjk_nm1half=(grid.dLocalGridOld[grid.nV][nICen][nJInt][k]
          +grid.dLocalGridOld[grid.nV][nICen][nJInt-1][k])*0.5;/**\BC assuming theta velocity is
          constant across surface*/
        dRho_ip1halfjk_n=(grid.dLocalGridOld[grid.nD][nICen][j][k])*0.5;/**\BC Missing 
          grid.dLocalGridOld[grid.nDenAve][nICen+1][0][0] in calculation of 
          \f$\langle\rho\rangle_{i+1/2}\f$, setting it to zero.*/
        dP_ijk_n=grid.dLocalGridOld[grid.nP][nICen][j][k]+grid.dLocalGridOld[grid.nQ0][nICen][j][k];
        dP_ip1jk_n=-1.0*dP_ijk_n;/**\BC Missing grid.dLocalGridOld[grid.nP][nICen+1][j][k]
          in calculation of \f$S_1\f$, setting it to -1.0*dP_ijk_n.*/
        
        //Calculate dA1
        dA1CenGrad=(dU_ip1jk_nm1half-dU_ijk_nm1half)/grid.dLocalGridOld[grid.nDM][nICen][0][0]*2.0;
          /**\BC Missing grid.dLocalGridOld[grid.nDM][nICen+1][0][0] in calculation of centered 
          \f$A_1\f$ gradient, setting it to zero.*/
        dA1UpWindGrad=0.0;
        if(grid.dLocalGridOld[grid.nU][i][j][k]<0.0){//moving from outside in
          dA1UpWindGrad=dA1CenGrad;/**\BC Missing grid.dLocalGridOld[grid.nU][i+1][j][k] and 
            grid.dLocalGridOld[grid.nDM][nICen+1][0][0] in calculation of upwind gradient, when
            moving inward. Using centered gradient instead.*/
        }
        else{//moving from inside out
          dA1UpWindGrad=(grid.dLocalGridOld[grid.nU][i][j][k]
            -grid.dLocalGridOld[grid.nU][i-1][j][k])/grid.dLocalGridOld[grid.nDM][nICen][0][0];
        }
        dA1=(grid.dLocalGridOld[grid.nU][i][j][k]-grid.dLocalGridOld[grid.nU0][i][0][0])*((1.0
          -parameters.dDonorFrac)*dA1CenGrad+parameters.dDonorFrac*dA1UpWindGrad);
        
        //calculate dS1
        dS1=(dP_ip1jk_n-dP_ijk_n)/(grid.dLocalGridOld[grid.nDM][nICen][0][0]*(0.5
          +parameters.dAlpha+parameters.dAlphaExtra))/dRho_ip1halfjk_n;/**\BC Missing 
          grid.dLocalGridOld[grid.nDM][i+1][0][0] in calculation of \f$S_1\f$ using
          \ref Parameters.dAlpha *grid.dLocalGridOld[grid.nDM][nICen][0][0] instead.*/
        
        //Calculate dS4
        dS4=parameters.dG*grid.dLocalGridOld[grid.nM][i][0][0]/dR_ip1half_n_Sq;
        
        //Calculate dA2
        dA2CenGrad=(dU_ip1halfjp1halfk_nm1half-dU_ip1halfjm1halfk_nm1half)
          /grid.dLocalGridOld[grid.nDTheta][0][j][0];
        dA2UpWindGrad=0.0;
        if(dV_ip1halfjk_nm1half>0.0){//moving in positive direction
          dA2UpWindGrad=(grid.dLocalGridOld[grid.nU][i][j][k]
            -grid.dLocalGridOld[grid.nU][i][j-1][k])/(grid.dLocalGridOld[grid.nDTheta][0][j][0]
            +grid.dLocalGridOld[grid.nDTheta][0][j-1][0])*2.0;
        }
        else{//moving in negative direction
          dA2UpWindGrad=(grid.dLocalGridOld[grid.nU][i][j+1][k]
            -grid.dLocalGridOld[grid.nU][i][j][k])/(grid.dLocalGridOld[grid.nDTheta][0][j+1][0]
            +grid.dLocalGridOld[grid.nDTheta][0][j][0])*2.0;
        }
        dA2=dV_ip1halfjk_nm1half*((1.0-parameters.dDonorFrac)*dA2CenGrad
          +parameters.dDonorFrac*dA2UpWindGrad)/grid.dLocalGridOld[grid.nR][i][0][0];
        
        //Calculate dS2
        dS2=dV_ip1halfjk_nm1half*dV_ip1halfjk_nm1half/grid.dLocalGridOld[grid.nR][i][0][0];
        
        //calculate new velocity
        grid.dLocalGridNew[grid.nU][i][j][k]=grid.dLocalGridOld[grid.nU][i][j][k]
          -time.dDeltat_n*(4.0*parameters.dPi*dRhoAve_ip1halfjk_n*dR_ip1half_n_Sq*(dA1+dS1)+dA2-dS2
          +dS4);
      }
    }
  }

  #if SEDOV==1
    //calculate gost region 1 inner most ghost region in x1 direction
    for(i=grid.nStartGhostUpdateExplicit[grid.nU][1][0];
      i<grid.nEndGhostUpdateExplicit[grid.nU][1][0];i++){//nU0 needs to be 1D
      
      //calculate i of centered quantities
      nICen=i-grid.nCenIntOffset[0];
      dR_ip1half_n_Sq=grid.dLocalGridOld[grid.nR][i][0][0]*grid.dLocalGridOld[grid.nR][i][0][0];
      dRhoAve_ip1halfjk_n=(grid.dLocalGridOld[grid.nDenAve][nICen+1][0][0]
        +grid.dLocalGridOld[grid.nDenAve][nICen][0][0])*0.5;
      
      for(j=grid.nStartGhostUpdateExplicit[grid.nU][1][1];
        j<grid.nEndGhostUpdateExplicit[grid.nU][1][1];j++){
      
        //calculate j of interface quantities
        nJInt=j+grid.nCenIntOffset[1];
      
        for(k=grid.nStartGhostUpdateExplicit[grid.nU][1][2];
          k<grid.nEndGhostUpdateExplicit[grid.nU][1][2];k++){
          
          //CALCULATE INTERPOLATED QUANTITIES
          dU_ip1jk_nm1half=(grid.dLocalGridOld[grid.nU][i+1][j][k]
            +grid.dLocalGridOld[grid.nU][i][j][k])*0.5;
          dU_ijk_nm1half=(grid.dLocalGridOld[grid.nU][i][j][k]
            +grid.dLocalGridOld[grid.nU][i-1][j][k])*0.5;
          dU_ip1halfjp1halfk_nm1half=0.5*(grid.dLocalGridOld[grid.nU][i][j+1][k]
            +grid.dLocalGridOld[grid.nU][i][j][k]);
          dU_ip1halfjm1halfk_nm1half=0.5*(grid.dLocalGridOld[grid.nU][i][j][k]
            +grid.dLocalGridOld[grid.nU][i][j-1][k]);
          dV_ip1halfjk_nm1half=0.25*(grid.dLocalGridOld[grid.nV][nICen+1][nJInt][k]
            +grid.dLocalGridOld[grid.nV][nICen+1][nJInt-1][k]
            +grid.dLocalGridOld[grid.nV][nICen][nJInt][k]
            +grid.dLocalGridOld[grid.nV][nICen][nJInt-1][k]);
          dRho_ip1halfjk_n=(grid.dLocalGridOld[grid.nD][nICen+1][j][k]
            +grid.dLocalGridOld[grid.nD][nICen][j][k])*0.5;
          dP_ip1jk_n=grid.dLocalGridOld[grid.nP][nICen+1][j][k]
            +grid.dLocalGridOld[grid.nQ0][nICen+1][j][k];
          dP_ijk_n=grid.dLocalGridOld[grid.nP][nICen][j][k]
            +grid.dLocalGridOld[grid.nQ0][nICen][j][k];
          
          //Calculate dA1
          dA1CenGrad=(dU_ip1jk_nm1half-dU_ijk_nm1half)/(grid.dLocalGridOld[grid.nDM][nICen+1][0][0]
            +grid.dLocalGridOld[grid.nDM][nICen][0][0])*2.0;
          dA1UpWindGrad=0.0;
          dUmU0_ijk_nm1half=grid.dLocalGridOld[grid.nU][i][j][k]
            -grid.dLocalGridOld[grid.nU0][i][0][0];
          if(dUmU0_ijk_nm1half<0.0){//moving from outside in
            dA1UpWindGrad=(grid.dLocalGridOld[grid.nU][i+1][j][k]
              -grid.dLocalGridOld[grid.nU][i][j][k])
              /grid.dLocalGridOld[grid.nDM][nICen+1][0][0];
          }
          else{//moving from inside out
            dA1UpWindGrad=(grid.dLocalGridOld[grid.nU][i][j][k]
              -grid.dLocalGridOld[grid.nU][i-1][j][k])
              /grid.dLocalGridOld[grid.nDM][nICen][0][0];
          }
          dA1=dUmU0_ijk_nm1half*((1.0-parameters.dDonorFrac)*dA1CenGrad+parameters.dDonorFrac
            *dA1UpWindGrad);
          
          //calculate dS1
          dS1=(dP_ip1jk_n-dP_ijk_n)/((grid.dLocalGridOld[grid.nDM][nICen+1][0][0]
            +grid.dLocalGridOld[grid.nDM][nICen][0][0])*dRho_ip1halfjk_n)*2.0;
          
          //Calculate dS4
          dS4=parameters.dG*grid.dLocalGridOld[grid.nM][i][0][0]/dR_ip1half_n_Sq;
          
          //Calculate dA2
          dA2CenGrad=(dU_ip1halfjp1halfk_nm1half-dU_ip1halfjm1halfk_nm1half)
            /grid.dLocalGridOld[grid.nDTheta][0][j][0];
          dA2UpWindGrad=0.0;
          if(dV_ip1halfjk_nm1half>0.0){//moving in positive direction
            dA2UpWindGrad=(grid.dLocalGridOld[grid.nU][i][j][k]
              -grid.dLocalGridOld[grid.nU][i][j-1][k])
              /(grid.dLocalGridOld[grid.nDTheta][0][j][0]
              +grid.dLocalGridOld[grid.nDTheta][0][j-1][0])*2.0;
          }
          else{//moving in negative direction
            dA2UpWindGrad=(grid.dLocalGridOld[grid.nU][i][j+1][k]
              -grid.dLocalGridOld[grid.nU][i][j][k])
              /(grid.dLocalGridOld[grid.nDTheta][0][j+1][0]
              +grid.dLocalGridOld[grid.nDTheta][0][j][0])*2.0;
          }
          dA2=dV_ip1halfjk_nm1half*((1.0-parameters.dDonorFrac)*dA2CenGrad+parameters.dDonorFrac
            *dA2UpWindGrad)/grid.dLocalGridOld[grid.nR][i][0][0];
          
          //Calculate dS2
          dS2=-1.0*dV_ip1halfjk_nm1half*dV_ip1halfjk_nm1half/grid.dLocalGridOld[grid.nR][i][0][0];
          
          //calculate new velocity
          grid.dLocalGridNew[grid.nU][i][j][k]=grid.dLocalGridOld[grid.nU][i][j][k]
            -time.dDeltat_n*(4.0*parameters.dPi*dRhoAve_ip1halfjk_n*dR_ip1half_n_Sq*(dA1+dS1)+dA2
            +dS2+dS4);
        }
      }
    }
  #endif
}
void calNewU_RT_LES(Grid &grid,Parameters &parameters,Time &time,ProcTop &procTop){
  int i;
  int j;
  int k;
  int nICen;
  int nJInt;
  double dRho_ip1halfjk_n;
  double dP_ip1jk_n;
  double dP_ijk_n;
  double dA1CenGrad;
  double dA1UpWindGrad;
  double dA1;
  double dS1;
  double dA2CenGrad;
  double dA2UpWindGrad;
  double dA2;
  double dS2;
  double dS4;
  double dTA1;
  double dTS1;
  double dTA2;
  double dTS2;
  double dTS4;
  double dDivU_ijk_n;
  double dDivU_ip1jk_n;
  double dTau_rr_ip1jk_n;
  double dTau_rr_ijk_n;
  double dTau_rt_ip1halfjp1halfk_n;
  double dTau_rt_ip1halfjm1halfk_n;
  double dR_i_n;
  double dR_ip1_n;
  double dU_ip1jk_nm1half;
  double dU_ijk_nm1half;
  double dU_ip1halfjp1halfk_nm1half;
  double dU_ip1halfjm1halfk_nm1half;
  double dU0_ip1_nm1half;
  double dU0_i_nm1half;
  double dUmU0_ip1halfjk_nm1half;
  double dV_ip1halfjk_nm1half;
  double dV_ip1halfjp1halfk_nm1half;
  double dV_ip1halfjm1halfk_nm1half;
  double dV_ip1jk_nm1half;
  double dV_ijk_nm1half;
  double dV_R_ip1jk_n;
  double dV_R_ip1jp1halfk_n;
  double dV_R_ip1jm1halfk_n;
  double dV_R_ijp1halfk_n;
  double dV_R_ijm1halfk_n;
  double dV_R_ijk_n;
  double dRSq_ip1half_n;
  double dRSq_im1half_n;
  double dRSq_ip1_n;
  double dRSq_i_n;
  double dRSq_ip3half_n;
  double dRCu_ip1half_n;
  double dRSqUmU0_ip3halfjk_n;
  double dRSqUmU0_ip1halfjk_n;
  double dRSqUmU0_im1halfjk_n;
  double dRSqUmU0_ijk_n;//needed at surface boundary
  double dRhoR_ip1halfjk_n;
  double dDM_ip1half;
  double dDTheta_jp1half;
  double dDTheta_jm1half;
  double dEddyVisc_ip1halfjk_n;
  double dEddyVisc_ip1halfjp1halfk_n;
  double dEddyVisc_ip1halfjm1halfk_n;
  double dRhoAve_ip1half_n;
  double dEddyViscosityTerms;
  
  //calculate new u
  for(i=grid.nStartUpdateExplicit[grid.nU][0];i<grid.nEndUpdateExplicit[grid.nU][0];i++){
    
    //calculate i of centered quantities
    nICen=i-grid.nCenIntOffset[0];
    
    //calculate quantities that vary only with radius
    dR_ip1_n=(grid.dLocalGridOld[grid.nR][i+1][0][0]+grid.dLocalGridOld[grid.nR][i][0][0])*0.5;
    dR_i_n=(grid.dLocalGridOld[grid.nR][i][0][0]+grid.dLocalGridOld[grid.nR][i-1][0][0])*0.5;
    dRSq_ip1_n=dR_ip1_n*dR_ip1_n;
    dRSq_i_n=dR_i_n*dR_i_n;
    dRSq_ip1half_n=grid.dLocalGridOld[grid.nR][i][0][0]*grid.dLocalGridOld[grid.nR][i][0][0];
    dRSq_im1half_n=grid.dLocalGridOld[grid.nR][i-1][0][0]*grid.dLocalGridOld[grid.nR][i-1][0][0];
    dRSq_ip3half_n=grid.dLocalGridOld[grid.nR][i+1][0][0]*grid.dLocalGridOld[grid.nR][i+1][0][0];
    dRCu_ip1half_n=dRSq_ip1half_n*grid.dLocalGridOld[grid.nR][i][0][0];
    dDM_ip1half=(grid.dLocalGridOld[grid.nDM][nICen+1][0][0]
      +grid.dLocalGridOld[grid.nDM][nICen][0][0])*0.5;
    dRhoAve_ip1half_n=(grid.dLocalGridOld[grid.nDenAve][nICen+1][0][0]
      +grid.dLocalGridOld[grid.nDenAve][nICen][0][0])*0.5;
    dU0_ip1_nm1half=(grid.dLocalGridOld[grid.nU0][i+1][0][0]
      +grid.dLocalGridOld[grid.nU0][i][0][0])*0.5;
    dU0_i_nm1half=(grid.dLocalGridOld[grid.nU0][i][0][0]
      +grid.dLocalGridOld[grid.nU0][i-1][0][0])*0.5;
    
    for(j=grid.nStartUpdateExplicit[grid.nU][1];j<grid.nEndUpdateExplicit[grid.nU][1];j++){
      
      //calculate j of interface quantities
      nJInt=j+grid.nCenIntOffset[1];
      
      //calculating quantities that vary only with theta, and perhaps radius
      dDTheta_jp1half=(grid.dLocalGridOld[grid.nDTheta][0][j+1][0]
        +grid.dLocalGridOld[grid.nDTheta][0][j][0])*0.5;
      dDTheta_jm1half=(grid.dLocalGridOld[grid.nDTheta][0][j-1][0]
        +grid.dLocalGridOld[grid.nDTheta][0][j][0])*0.5;
      
      for(k=grid.nStartUpdateExplicit[grid.nU][2];k<grid.nEndUpdateExplicit[grid.nU][2];k++){
        
        //CALCULATE INTERPOLATED QUANTITIES
        dU_ip1jk_nm1half=(grid.dLocalGridOld[grid.nU][i+1][j][k]
          +grid.dLocalGridOld[grid.nU][i][j][k])*0.5;
        dU_ijk_nm1half=(grid.dLocalGridOld[grid.nU][i][j][k]
          +grid.dLocalGridOld[grid.nU][i-1][j][k])*0.5;
        dUmU0_ip1halfjk_nm1half=grid.dLocalGridOld[grid.nU][i][j][k]
          -grid.dLocalGridOld[grid.nU0][i][0][0];
        dU_ip1halfjp1halfk_nm1half=(grid.dLocalGridOld[grid.nU][i][j+1][k]
          +grid.dLocalGridOld[grid.nU][i][j][k])*0.5;
        dU_ip1halfjm1halfk_nm1half=(grid.dLocalGridOld[grid.nU][i][j-1][k]
          +grid.dLocalGridOld[grid.nU][i][j][k])*0.5;
        dRho_ip1halfjk_n=(grid.dLocalGridOld[grid.nD][nICen+1][j][k]
          +grid.dLocalGridOld[grid.nD][nICen][j][k])*0.5;
        dV_ip1halfjk_nm1half=0.25*(grid.dLocalGridOld[grid.nV][nICen+1][nJInt][k]
          +grid.dLocalGridOld[grid.nV][nICen+1][nJInt-1][k]
          +grid.dLocalGridOld[grid.nV][nICen][nJInt][k]
          +grid.dLocalGridOld[grid.nV][nICen][nJInt-1][k]);
        dV_ip1halfjp1halfk_nm1half=(grid.dLocalGridOld[grid.nV][nICen+1][nJInt][k]
          +grid.dLocalGridOld[grid.nV][nICen][nJInt][k])*0.5;
        dV_ip1halfjm1halfk_nm1half=(grid.dLocalGridOld[grid.nV][nICen+1][nJInt-1][k]
          +grid.dLocalGridOld[grid.nV][nICen][nJInt-1][k])*0.5;
        dV_ip1jk_nm1half=(grid.dLocalGridOld[grid.nV][nICen+1][nJInt][k]
          +grid.dLocalGridOld[grid.nV][nICen+1][nJInt-1][k])*0.5;
        dV_ijk_nm1half=(grid.dLocalGridOld[grid.nV][nICen][nJInt][k]
          +grid.dLocalGridOld[grid.nV][nICen][nJInt-1][k])*0.5;
        dP_ip1jk_n=grid.dLocalGridOld[grid.nP][nICen+1][j][k]
          +grid.dLocalGridOld[grid.nQ0][nICen+1][j][k]+grid.dLocalGridOld[grid.nQ1][nICen+1][j][k];
        dP_ijk_n=grid.dLocalGridOld[grid.nP][nICen][j][k]
          +grid.dLocalGridOld[grid.nQ0][nICen][j][k]+grid.dLocalGridOld[grid.nQ1][nICen][j][k];
        dEddyVisc_ip1halfjk_n=(grid.dLocalGridOld[grid.nEddyVisc][nICen][j][k]
          +grid.dLocalGridOld[grid.nEddyVisc][nICen+1][j][k])*0.5;
        dEddyVisc_ip1halfjp1halfk_n=(grid.dLocalGridOld[grid.nEddyVisc][nICen][j][k]
          +grid.dLocalGridOld[grid.nEddyVisc][nICen][j+1][k]
          +grid.dLocalGridOld[grid.nEddyVisc][nICen+1][j][k]
          +grid.dLocalGridOld[grid.nEddyVisc][nICen+1][j+1][k])*0.25;
        dEddyVisc_ip1halfjm1halfk_n=(grid.dLocalGridOld[grid.nEddyVisc][nICen][j][k]
          +grid.dLocalGridOld[grid.nEddyVisc][nICen][j-1][k]
          +grid.dLocalGridOld[grid.nEddyVisc][nICen+1][j][k]
          +grid.dLocalGridOld[grid.nEddyVisc][nICen+1][j-1][k])*0.25;
        
        //calculate derived quantities
        dRSqUmU0_ip3halfjk_n=dRSq_ip3half_n*(grid.dLocalGridOld[grid.nU][i+1][j][k]
          -grid.dLocalGridOld[grid.nU0][i+1][0][0]);
        dRSqUmU0_ip1halfjk_n=dRSq_ip1half_n*(grid.dLocalGridOld[grid.nU][i][j][k]
          -grid.dLocalGridOld[grid.nU0][i][0][0]);
        dRSqUmU0_im1halfjk_n=dRSq_im1half_n*(grid.dLocalGridOld[grid.nU][i-1][j][k]
          -grid.dLocalGridOld[grid.nU0][i-1][0][0]);
        dV_R_ip1jk_n=dV_ip1jk_nm1half/dR_ip1_n;
        dV_R_ip1jp1halfk_n=grid.dLocalGridOld[grid.nV][nICen+1][nJInt][k]/dR_ip1_n;
        dV_R_ip1jm1halfk_n=grid.dLocalGridOld[grid.nV][nICen+1][nJInt-1][k]/dR_ip1_n;
        dV_R_ijp1halfk_n=grid.dLocalGridOld[grid.nV][nICen][nJInt][k]/dR_i_n;
        dV_R_ijm1halfk_n=grid.dLocalGridOld[grid.nV][nICen][nJInt-1][k]/dR_i_n;
        dV_R_ijk_n=dV_ijk_nm1half/dR_i_n;
        dRhoR_ip1halfjk_n=dRho_ip1halfjk_n*grid.dLocalGridOld[grid.nR][i][0][0];
        
        //Calculate dA1
        dA1CenGrad=(dU_ip1jk_nm1half-dU_ijk_nm1half)
          /(grid.dLocalGridOld[grid.nDM][nICen+1][0][0]
          +grid.dLocalGridOld[grid.nDM][nICen][0][0])*2.0;
        dA1UpWindGrad=0.0;
        if(dUmU0_ip1halfjk_nm1half<0.0){//moving from outside in
          dA1UpWindGrad=(grid.dLocalGridOld[grid.nU][i+1][j][k]
            -grid.dLocalGridOld[grid.nU][i][j][k])
            /grid.dLocalGridOld[grid.nDM][nICen+1][0][0];
        }
        else{//moving from inside out
          dA1UpWindGrad=(grid.dLocalGridOld[grid.nU][i][j][k]
            -grid.dLocalGridOld[grid.nU][i-1][j][k])
            /grid.dLocalGridOld[grid.nDM][nICen][0][0];
        }
        dA1=dUmU0_ip1halfjk_nm1half*((1.0-parameters.dDonorFrac)*dA1CenGrad+parameters.dDonorFrac
          *dA1UpWindGrad);
        
        //calculate dS1
        dS1=(dP_ip1jk_n-dP_ijk_n)/(dDM_ip1half*dRho_ip1halfjk_n);
        
        //Calculate dS4
        dS4=parameters.dG*grid.dLocalGridOld[grid.nM][i][0][0]/dRSq_ip1half_n;
        
        //Calculate dA2
        dA2CenGrad=(dU_ip1halfjp1halfk_nm1half-dU_ip1halfjm1halfk_nm1half)
          /grid.dLocalGridOld[grid.nDTheta][0][j][0];
        dA2UpWindGrad=0.0;
        if(dV_ip1halfjk_nm1half>0.0){//moving in positive direction
          dA2UpWindGrad=(grid.dLocalGridOld[grid.nU][i][j][k]
            -grid.dLocalGridOld[grid.nU][i][j-1][k])
            /(grid.dLocalGridOld[grid.nDTheta][0][j][0]
            +grid.dLocalGridOld[grid.nDTheta][0][j-1][0])*2.0;
        }
        else{//moving in negative direction
          dA2UpWindGrad=(grid.dLocalGridOld[grid.nU][i][j+1][k]
            -grid.dLocalGridOld[grid.nU][i][j][k])
            /(grid.dLocalGridOld[grid.nDTheta][0][j+1][0]
            +grid.dLocalGridOld[grid.nDTheta][0][j][0])*2.0;
        }
        dA2=dV_ip1halfjk_nm1half*((1.0-parameters.dDonorFrac)*dA2CenGrad
          +parameters.dDonorFrac*dA2UpWindGrad)/grid.dLocalGridOld[grid.nR][i][0][0];
        
        //Calculate dS2
        dS2=dV_ip1halfjk_nm1half*dV_ip1halfjk_nm1half
          /grid.dLocalGridOld[grid.nR][i][0][0];
        
        //cal DivU_ip1jk_n
        dDivU_ip1jk_n=4.0*parameters.dPi*grid.dLocalGridOld[grid.nDenAve][nICen+1][0][0]
          *(dRSqUmU0_ip3halfjk_n-dRSqUmU0_ip1halfjk_n)/grid.dLocalGridOld[grid.nDM][nICen+1][0][0]
          +(grid.dLocalGridOld[grid.nV][nICen+1][nJInt][k]
          *grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt][0]
          -grid.dLocalGridOld[grid.nV][nICen+1][nJInt-1][k]
          *grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt-1][0])
          /(grid.dLocalGridOld[grid.nDTheta][0][j][0]*dR_ip1_n
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]);
        
        //cal DivU_ijk_n
        dDivU_ijk_n=4.0*parameters.dPi*grid.dLocalGridOld[grid.nDenAve][nICen][0][0]
          *(dRSqUmU0_ip1halfjk_n-dRSqUmU0_im1halfjk_n)/grid.dLocalGridOld[grid.nDM][nICen][0][0]
          +(grid.dLocalGridOld[grid.nV][nICen][nJInt][k]
          *grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt][0]
          -grid.dLocalGridOld[grid.nV][nICen][nJInt-1][k]
          *grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt-1][0])
          /(grid.dLocalGridOld[grid.nDTheta][0][j][0]*dR_i_n
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]);
        
        //cal Tau_rr_ip1jk_n
        dTau_rr_ip1jk_n=2.0*grid.dLocalGridOld[grid.nEddyVisc][nICen+1][j][k]*(4.0*parameters.dPi
          *dRSq_ip1_n*grid.dLocalGridOld[grid.nDenAve][nICen+1][0][0]
          *((grid.dLocalGridOld[grid.nU][i+1][j][k]-grid.dLocalGridOld[grid.nU0][i+1][0][0])
          -(grid.dLocalGridOld[grid.nU][i][j][k]-grid.dLocalGridOld[grid.nU0][i][0][0]))
          /grid.dLocalGridOld[grid.nDM][nICen+1][0][0]-0.3333333333333333*dDivU_ip1jk_n);
        
        //cal Tau_rr_ijk_n
        dTau_rr_ijk_n=2.0*grid.dLocalGridOld[grid.nEddyVisc][nICen][j][k]*(4.0*parameters.dPi
          *dRSq_i_n*grid.dLocalGridOld[grid.nDenAve][nICen][0][0]
          *((grid.dLocalGridOld[grid.nU][i][j][k]-grid.dLocalGridOld[grid.nU0][i][0][0])
          -(grid.dLocalGridOld[grid.nU][i-1][j][k]-grid.dLocalGridOld[grid.nU0][i-1][0][0]))
          /grid.dLocalGridOld[grid.nDM][nICen][0][0]-0.3333333333333333*dDivU_ijk_n);
        
        //calculate dTau_rt_ip1halfjp1halfk_n
        dTau_rt_ip1halfjp1halfk_n=dEddyVisc_ip1halfjp1halfk_n*(4.0*parameters.dPi*dRCu_ip1half_n
          *dRhoAve_ip1half_n*(dV_R_ip1jp1halfk_n-dV_R_ijp1halfk_n)/dDM_ip1half
          +((grid.dLocalGridOld[grid.nU][i][j+1][k]
          -grid.dLocalGridOld[grid.nU0][i][0][0])-(grid.dLocalGridOld[grid.nU][i][j][k]
          -grid.dLocalGridOld[grid.nU0][i][0][0]))/(dDTheta_jp1half
          *grid.dLocalGridOld[grid.nR][i][0][0]));
        
        //calculate dTau_rt_ip1halfjm1halfk_n
        dTau_rt_ip1halfjm1halfk_n=dEddyVisc_ip1halfjm1halfk_n*(4.0*parameters.dPi*dRCu_ip1half_n
          *dRhoAve_ip1half_n*(dV_R_ip1jm1halfk_n-dV_R_ijm1halfk_n)/dDM_ip1half
          +((grid.dLocalGridOld[grid.nU][i][j][k]
          -grid.dLocalGridOld[grid.nU0][i][0][0])-(grid.dLocalGridOld[grid.nU][i][j-1][k]
          -grid.dLocalGridOld[grid.nU0][i][0][0]))/(grid.dLocalGridOld[grid.nR][i][0][0]
          *dDTheta_jm1half));
        
        //cal dTA1
        dTA1=(dTau_rr_ip1jk_n-dTau_rr_ijk_n)/(dDM_ip1half*dRho_ip1halfjk_n);
        
        //cal dTS1
        dTS1=dEddyVisc_ip1halfjk_n/dRhoR_ip1halfjk_n*(4.0
          *((dU_ip1jk_nm1half-dU0_ip1_nm1half)-(dU_ijk_nm1half-dU0_i_nm1half))/dDM_ip1half
          +grid.dLocalGridOld[grid.nR][i][0][0]*grid.dLocalGridOld[grid.nCotThetaIJK][0][j][0]
          *(dV_R_ip1jk_n-dV_R_ijk_n)/dDM_ip1half);
        
        //calculate dTA2
        dTA2=(dTau_rt_ip1halfjp1halfk_n-dTau_rt_ip1halfjm1halfk_n)
          /(grid.dLocalGridOld[grid.nDTheta][0][j][0]*dRhoR_ip1halfjk_n);
        
        //calculate dTS2
        dTS2=(2.0*(dV_ip1halfjp1halfk_nm1half-dV_ip1halfjm1halfk_nm1half)
          -grid.dLocalGridOld[grid.nCotThetaIJK][0][j][0]*((dU_ip1halfjp1halfk_nm1half
          -grid.dLocalGridOld[grid.nU0][i][0][0])-(dU_ip1halfjm1halfk_nm1half
          -grid.dLocalGridOld[grid.nU0][i][0][0])))/(grid.dLocalGridOld[grid.nR][i][0][0]
          *grid.dLocalGridOld[grid.nDTheta][0][j][0]);
        
        //cal dTS4
        dTS4=(4.0*(grid.dLocalGridOld[grid.nU][i][j][k]-grid.dLocalGridOld[grid.nU0][i][0][0])
          +2.0*grid.dLocalGridOld[grid.nCotThetaIJK][0][j][0]*dV_ip1halfjk_nm1half)
          /grid.dLocalGridOld[grid.nR][i][0][0];
        
        dEddyViscosityTerms=-4.0*parameters.dPi*dRhoAve_ip1half_n*dRSq_ip1half_n*(dTA1+dTS1)-dTA2
          +dEddyVisc_ip1halfjk_n/dRhoR_ip1halfjk_n*(dTS2+dTS4);
        
        //calculate new velocity
        grid.dLocalGridNew[grid.nU][i][j][k]=grid.dLocalGridOld[grid.nU][i][j][k]
          -time.dDeltat_n*(4.0*parameters.dPi*dRhoAve_ip1half_n*dRSq_ip1half_n*(dA1+dS1)
          +dA2-dS2+dS4+dEddyViscosityTerms);
      }
    }
  }
  
  //ghost region 0, outter most ghost region in x1 direction
  for(i=grid.nStartGhostUpdateExplicit[grid.nU][0][0];
    i<grid.nEndGhostUpdateExplicit[grid.nU][0][0];i++){
    
    //calculate i of centered quantities
    nICen=i-grid.nCenIntOffset[0];
    
    //calculate quantities that vary only with radius
    dR_i_n=(grid.dLocalGridOld[grid.nR][i][0][0]+grid.dLocalGridOld[grid.nR][i-1][0][0])*0.5;
    dRSq_i_n=dR_i_n*dR_i_n;
    dRSq_ip1half_n=grid.dLocalGridOld[grid.nR][i][0][0]*grid.dLocalGridOld[grid.nR][i][0][0];
    dRCu_ip1half_n=grid.dLocalGridOld[grid.nR][i][0][0]*grid.dLocalGridOld[grid.nR][i][0][0]
      *grid.dLocalGridOld[grid.nR][i][0][0];
    dDM_ip1half=(grid.dLocalGridOld[grid.nDM][nICen][0][0])*(0.5+parameters.dAlpha
      +parameters.dAlphaExtra);/**\BC Missing grid.dLocalGridOld[grid.nDM][i+1][0][0] in calculation
      of \f$S_1\f$ using \ref Parameters.dAlpha *grid.dLocalGridOld[grid.nDM][nICen][0][0] instead.
      */
    dRhoAve_ip1half_n=(grid.dLocalGridOld[grid.nDenAve][nICen][0][0])*0.5;/**\BC Missing density
      outside of surface, setting it to zero.*/
    dU0_i_nm1half=(grid.dLocalGridOld[grid.nU0][i][0][0]+grid.dLocalGridOld[grid.nU0][i-1][0][0])
      *0.5;
    dR_ip1_n=grid.dLocalGridOld[grid.nR][i][0][0];
    
    for(j=grid.nStartGhostUpdateExplicit[grid.nU][0][1];
      j<grid.nEndGhostUpdateExplicit[grid.nU][0][1];j++){
      
      //calculate j of interface quantities
      nJInt=j+grid.nCenIntOffset[1];
      
      //calculating quantities that vary only with theta, and perhaps radius
      dDTheta_jp1half=(grid.dLocalGridOld[grid.nDTheta][0][j+1][0]
        +grid.dLocalGridOld[grid.nDTheta][0][j][0])*0.5;
      dDTheta_jm1half=(grid.dLocalGridOld[grid.nDTheta][0][j-1][0]
        +grid.dLocalGridOld[grid.nDTheta][0][j][0])*0.5;
      
      for(k=grid.nStartGhostUpdateExplicit[grid.nU][0][2];
        k<grid.nEndGhostUpdateExplicit[grid.nU][0][2];k++){
        
        //CALCULATE INTERPOLATED QUANTITIES
        dU_ip1jk_nm1half=grid.dLocalGridOld[grid.nU][i][j][k];
        dU_ijk_nm1half=(grid.dLocalGridOld[grid.nU][i][j][k]
          +grid.dLocalGridOld[grid.nU][i-1][j][k])*0.5;
        dUmU0_ip1halfjk_nm1half=grid.dLocalGridOld[grid.nU][i][j][k]
          -grid.dLocalGridOld[grid.nU0][i][0][0];
        dU_ip1halfjp1halfk_nm1half=(grid.dLocalGridOld[grid.nU][i][j+1][k]
          +grid.dLocalGridOld[grid.nU][i][j][k])*0.5;
        dU_ip1halfjm1halfk_nm1half=(grid.dLocalGridOld[grid.nU][i][j-1][k]
          +grid.dLocalGridOld[grid.nU][i][j][k])*0.5;
        dRho_ip1halfjk_n=(grid.dLocalGridOld[grid.nD][nICen][j][k])*0.5;/**\BC Missing density 
          outside model, setting it to zero. */
        dV_ip1halfjk_nm1half=0.5*(grid.dLocalGridOld[grid.nV][nICen][nJInt][k]
          +grid.dLocalGridOld[grid.nV][nICen][nJInt-1][k]);/**\BC assuming theta and phi velocity 
          same outside star as inside.*/
        dV_ip1halfjp1halfk_nm1half=grid.dLocalGridOld[grid.nV][nICen][nJInt][k];/**\BC Assuming 
          theta velocities are constant across surface.*/
        dV_ip1halfjm1halfk_nm1half=grid.dLocalGridOld[grid.nV][nICen][nJInt-1][k];
        dV_ip1jk_nm1half=(grid.dLocalGridOld[grid.nV][nICen][nJInt][k]
          +grid.dLocalGridOld[grid.nV][nICen][nJInt-1][k])*0.5;/**\BC assuming that $V$ at
          $i+1$ is equal to $v$ at $i$.*/
        dV_ijk_nm1half=(grid.dLocalGridOld[grid.nV][nICen][nJInt][k]
          +grid.dLocalGridOld[grid.nV][nICen][nJInt-1][k])*0.5;
        dP_ijk_n=grid.dLocalGridOld[grid.nP][nICen][j][k]
          +grid.dLocalGridOld[grid.nQ0][nICen][j][k]+grid.dLocalGridOld[grid.nQ1][nICen][j][k];
        dP_ip1jk_n=-1.0*dP_ijk_n;/**\BC Missing pressure outside surface setting it equal to 
          negative pressure in the center of the first cell so that it will be zero at surface.*/
        dEddyVisc_ip1halfjk_n=grid.dLocalGridOld[grid.nEddyVisc][nICen][j][k]*0.5;/**\BC assume 
          viscosity is zero outside the star.*/
        dEddyVisc_ip1halfjp1halfk_n=(grid.dLocalGridOld[grid.nEddyVisc][nICen][j][k]
          +grid.dLocalGridOld[grid.nEddyVisc][nICen][j+1][k])*0.25;
        dEddyVisc_ip1halfjm1halfk_n=(grid.dLocalGridOld[grid.nEddyVisc][nICen][j][k]
          +grid.dLocalGridOld[grid.nEddyVisc][nICen][j-1][k])*0.25;
        
        //calculate derived quantities
        dRSqUmU0_ijk_n=dRSq_i_n*(dU_ijk_nm1half-dU0_i_nm1half);
        dRSqUmU0_ip1halfjk_n=dRSq_ip1half_n*(grid.dLocalGridOld[grid.nU][i][j][k]
          -grid.dLocalGridOld[grid.nU0][i][0][0]);
        dRSqUmU0_im1halfjk_n=dRSq_im1half_n*(grid.dLocalGridOld[grid.nU][i-1][j][k]
          -grid.dLocalGridOld[grid.nU0][i-1][0][0]);
        dV_R_ip1jk_n=dV_ip1jk_nm1half/dR_ip1_n;
        dV_R_ip1jp1halfk_n=grid.dLocalGridOld[grid.nV][nICen][nJInt][k]/dR_ip1_n;
        dV_R_ip1jm1halfk_n=grid.dLocalGridOld[grid.nV][nICen][nJInt-1][k]/dR_ip1_n;
        dV_R_ijp1halfk_n=grid.dLocalGridOld[grid.nV][nICen][nJInt][k]/dR_i_n;
        dV_R_ijm1halfk_n=grid.dLocalGridOld[grid.nV][nICen][nJInt-1][k]/dR_i_n;
        dV_R_ijk_n=dV_ijk_nm1half/dR_i_n;
        dRhoR_ip1halfjk_n=dRho_ip1halfjk_n*grid.dLocalGridOld[grid.nR][i][0][0];
        
        //Calculate dA1
        dA1CenGrad=(dU_ip1jk_nm1half-dU_ijk_nm1half)
          /(grid.dLocalGridOld[grid.nDM][nICen][0][0]*0.5);/**\BC Missing mass outside model,
          setting it to zero.*/
        dA1UpWindGrad=0.0;
        if(dUmU0_ip1halfjk_nm1half<0.0){//moving from outside in
          dA1UpWindGrad=dA1CenGrad;/**\BC Missing grid.dLocalGridOld[grid.nU][i+1][j][k] and 
            grid.dLocalGridOld[grid.nDM][nICen+1][0][0] in calculation of upwind gradient, when 
            moving inward. Using centered gradient instead.*/
        }
        else{//moving from inside out
          dA1UpWindGrad=(grid.dLocalGridOld[grid.nU][i][j][k]
            -grid.dLocalGridOld[grid.nU][i-1][j][k])/grid.dLocalGridOld[grid.nDM][nICen][0][0];
        }
        dA1=dUmU0_ip1halfjk_nm1half*((1.0-parameters.dDonorFrac)*dA1CenGrad+parameters.dDonorFrac
          *dA1UpWindGrad);
        
        //calculate dS1
        dS1=(dP_ip1jk_n-dP_ijk_n)/(dDM_ip1half*dRho_ip1halfjk_n);
        
        //Calculate dS4
        dS4=parameters.dG*grid.dLocalGridOld[grid.nM][i][0][0]/dRSq_ip1half_n;
        
        //Calculate dA2
        dA2CenGrad=(dU_ip1halfjp1halfk_nm1half-dU_ip1halfjm1halfk_nm1half)
          /grid.dLocalGridOld[grid.nDTheta][0][j][0];
        dA2UpWindGrad=0.0;
        if(dV_ip1halfjk_nm1half>0.0){//moving in positive direction
          dA2UpWindGrad=(grid.dLocalGridOld[grid.nU][i][j][k]
            -grid.dLocalGridOld[grid.nU][i][j-1][k])/(grid.dLocalGridOld[grid.nDTheta][0][j][0]
            +grid.dLocalGridOld[grid.nDTheta][0][j-1][0])*2.0;
        }
        else{//moving in negative direction
          dA2UpWindGrad=(grid.dLocalGridOld[grid.nU][i][j+1][k]
            -grid.dLocalGridOld[grid.nU][i][j][k])/(grid.dLocalGridOld[grid.nDTheta][0][j+1][0]
            +grid.dLocalGridOld[grid.nDTheta][0][j][0])*2.0;
        }
        dA2=dV_ip1halfjk_nm1half*((1.0-parameters.dDonorFrac)*dA2CenGrad
          +parameters.dDonorFrac*dA2UpWindGrad)/grid.dLocalGridOld[grid.nR][i][0][0];
        
        //Calculate dS2
        dS2=dV_ip1halfjk_nm1half*dV_ip1halfjk_nm1half/grid.dLocalGridOld[grid.nR][i][0][0];
        
        //cal DivU_ip1jk_n
        dDivU_ip1jk_n=4.0*parameters.dPi*dRhoAve_ip1half_n
          *(dRSqUmU0_ip1halfjk_n-dRSqUmU0_ijk_n)/grid.dLocalGridOld[grid.nDM][nICen][0][0]*2.0
          +(grid.dLocalGridOld[grid.nV][nICen][nJInt][k]
          *grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt][0]
          -grid.dLocalGridOld[grid.nV][nICen][nJInt-1][k]
          *grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt-1][0])
          /(grid.dLocalGridOld[grid.nDTheta][0][j][0]*dR_ip1_n
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]);
        
        //cal DivU_ijk_n
        dDivU_ijk_n=4.0*parameters.dPi*grid.dLocalGridOld[grid.nDenAve][nICen][0][0]
          *(dRSqUmU0_ip1halfjk_n-dRSqUmU0_im1halfjk_n)/grid.dLocalGridOld[grid.nDM][nICen][0][0]
          +(grid.dLocalGridOld[grid.nV][nICen][nJInt][k]
          *grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt][0]
          -grid.dLocalGridOld[grid.nV][nICen][nJInt-1][k]
          *grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt-1][0])
          /(grid.dLocalGridOld[grid.nDTheta][0][j][0]*dR_i_n
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]);
        
        //cal Tau_rr_ip1jk_n
        dTau_rr_ip1jk_n=2.0*dEddyVisc_ip1halfjk_n*(4.0*parameters.dPi*dRSq_ip1half_n
          *dRhoAve_ip1half_n*((grid.dLocalGridOld[grid.nU][i][j][k]
          -grid.dLocalGridOld[grid.nU0][i][0][0])-(dU_ijk_nm1half-dU0_i_nm1half))
          /grid.dLocalGridOld[grid.nDM][nICen][0][0]*2.0-0.3333333333333333*dDivU_ip1jk_n);
        
        //cal Tau_rr_ijk_n
        dTau_rr_ijk_n=2.0*grid.dLocalGridOld[grid.nEddyVisc][nICen][j][k]*(4.0*parameters.dPi
          *dRSq_i_n*grid.dLocalGridOld[grid.nDenAve][nICen][0][0]
          *((grid.dLocalGridOld[grid.nU][i][j][k]-grid.dLocalGridOld[grid.nU0][i][0][0])
          -(grid.dLocalGridOld[grid.nU][i-1][j][k]-grid.dLocalGridOld[grid.nU0][i-1][0][0]))
          /grid.dLocalGridOld[grid.nDM][nICen][0][0]-0.3333333333333333*dDivU_ijk_n);
        
        //calculate dTau_rt_ip1halfjp1halfk_n
        dTau_rt_ip1halfjp1halfk_n=dEddyVisc_ip1halfjp1halfk_n*(4.0*parameters.dPi*dRCu_ip1half_n
          *dRhoAve_ip1half_n*(dV_R_ip1jp1halfk_n-dV_R_ijp1halfk_n)/dDM_ip1half
          +((grid.dLocalGridOld[grid.nU][i][j+1][k]
          -grid.dLocalGridOld[grid.nU0][i][0][0])-(grid.dLocalGridOld[grid.nU][i][j][k]
          -grid.dLocalGridOld[grid.nU0][i][0][0]))/(dDTheta_jp1half
          *grid.dLocalGridOld[grid.nR][i][0][0]));
        
        //calculate dTau_rt_ip1halfjm1halfk_n
        dTau_rt_ip1halfjm1halfk_n=dEddyVisc_ip1halfjm1halfk_n*(4.0*parameters.dPi*dRCu_ip1half_n
          *dRhoAve_ip1half_n*(dV_R_ip1jm1halfk_n-dV_R_ijm1halfk_n)/dDM_ip1half
          +((grid.dLocalGridOld[grid.nU][i][j][k]
          -grid.dLocalGridOld[grid.nU0][i][0][0])-(grid.dLocalGridOld[grid.nU][i][j-1][k]
          -grid.dLocalGridOld[grid.nU0][i][0][0]))/(grid.dLocalGridOld[grid.nR][i][0][0]
          *dDTheta_jm1half));
        
        //cal dTA1
        dTA1=(dTau_rr_ip1jk_n-dTau_rr_ijk_n)/(dDM_ip1half*dRho_ip1halfjk_n);
        
        //cal dTS1
        dTS1=dEddyVisc_ip1halfjk_n/dRhoR_ip1halfjk_n*(4.0
          *((dU_ip1jk_nm1half-dU0_ip1_nm1half)-(dU_ijk_nm1half-dU0_i_nm1half))/dDM_ip1half
          +grid.dLocalGridOld[grid.nR][i][0][0]*grid.dLocalGridOld[grid.nCotThetaIJK][0][j][0]
          *(dV_R_ip1jk_n-dV_R_ijk_n)/dDM_ip1half);
        
        //calculate dTA2
        dTA2=(dTau_rt_ip1halfjp1halfk_n-dTau_rt_ip1halfjm1halfk_n)
          /(grid.dLocalGridOld[grid.nDTheta][0][j][0]*dRhoR_ip1halfjk_n);
        
        //calculate dTS2
        dTS2=(2.0*(dV_ip1halfjp1halfk_nm1half-dV_ip1halfjm1halfk_nm1half)
          -grid.dLocalGridOld[grid.nCotThetaIJK][0][j][0]*((dU_ip1halfjp1halfk_nm1half
          -grid.dLocalGridOld[grid.nU0][i][0][0])-(dU_ip1halfjm1halfk_nm1half
          -grid.dLocalGridOld[grid.nU0][i][0][0])))/(grid.dLocalGridOld[grid.nR][i][0][0]
          *grid.dLocalGridOld[grid.nDTheta][0][j][0]);
        
        //cal dTS4
        dTS4=(4.0*(grid.dLocalGridOld[grid.nU][i][j][k]-grid.dLocalGridOld[grid.nU0][i][0][0])
          +2.0*grid.dLocalGridOld[grid.nCotThetaIJK][0][j][0]*dV_ip1halfjk_nm1half)
          /grid.dLocalGridOld[grid.nR][i][0][0];
        
        dEddyViscosityTerms=-4.0*parameters.dPi*dRhoAve_ip1half_n*dRSq_ip1half_n*(dTA1+dTS1)-dTA2
          +dEddyVisc_ip1halfjk_n/dRhoR_ip1halfjk_n*(dTS2+dTS4);
        
        //calculate new velocity
        grid.dLocalGridNew[grid.nU][i][j][k]=grid.dLocalGridOld[grid.nU][i][j][k]
          -time.dDeltat_n*(4.0*parameters.dPi*dRhoAve_ip1half_n*dRSq_ip1half_n*(dA1+dS1)
          +dA2-dS2+dS4+dEddyViscosityTerms);
      }
    }
  }
}
void calNewU_RTP(Grid &grid,Parameters &parameters,Time &time,ProcTop &procTop){
  int i;
  int j;
  int k;
  int nICen;
  int nJInt;
  int nKInt;
  double dRSq_ip1half_n;
  double dU_ip1jk_nm1half;
  double dU_ijk_nm1half;
  double dU_ip1halfjp1halfk_nm1half;
  double dU_ip1halfjm1halfk_nm1half;
  double dU_ip1halfjkp1half_nm1half;
  double dU_ip1halfjkm1half_nm1half;
  double dV_ip1halfjk_nm1half;
  double dW_ip1halfjk_nm1half;
  double dRho_ip1halfjk_n;
  double dRhoAve_ip1halfjk_n;
  double dP_ip1jk_n;
  double dP_ijk_n;
  double dUmU0_ijk_nm1half;
  double dA1CenGrad;
  double dA1UpWindGrad;
  double dA1;
  double dS1;
  double dA2CenGrad;
  double dA2UpWindGrad;
  double dA2;
  double dS2;
  double dA3CenGrad;
  double dA3UpWindGrad;
  double dA3;
  double dS3;
  double dS4;
  
  //calculate new u
  for(i=grid.nStartUpdateExplicit[grid.nU][0];i<grid.nEndUpdateExplicit[grid.nU][0];i++){
    
    //calculate i of centered quantities
    nICen=i-grid.nCenIntOffset[0];
    dRSq_ip1half_n=grid.dLocalGridOld[grid.nR][i][0][0]*grid.dLocalGridOld[grid.nR][i][0][0];
    dRhoAve_ip1halfjk_n=(grid.dLocalGridOld[grid.nDenAve][nICen+1][0][0]
      +grid.dLocalGridOld[grid.nDenAve][nICen][0][0])*0.5;
    
    for(j=grid.nStartUpdateExplicit[grid.nU][1];j<grid.nEndUpdateExplicit[grid.nU][1];j++){
      
      //calculate j of interface quantities
      nJInt=j+grid.nCenIntOffset[1];
      
      for(k=grid.nStartUpdateExplicit[grid.nU][2];k<grid.nEndUpdateExplicit[grid.nU][2];k++){
        
        //calculate k of interface quantities
        nKInt=k+grid.nCenIntOffset[2];
        
        //CALCULATE INTERPOLATED QUANTITIES
        dU_ip1jk_nm1half=(grid.dLocalGridOld[grid.nU][i+1][j][k]
          +grid.dLocalGridOld[grid.nU][i][j][k])*0.5;
        dU_ijk_nm1half=(grid.dLocalGridOld[grid.nU][i][j][k]
          +grid.dLocalGridOld[grid.nU][i-1][j][k])*0.5;
        dU_ip1halfjp1halfk_nm1half=(grid.dLocalGridOld[grid.nU][i][j+1][k]
          +grid.dLocalGridOld[grid.nU][i][j][k])*0.5;
        dU_ip1halfjm1halfk_nm1half=0.5*(grid.dLocalGridOld[grid.nU][i][j][k]
          +grid.dLocalGridOld[grid.nU][i][j-1][k]);
        dU_ip1halfjkp1half_nm1half=0.5*(grid.dLocalGridOld[grid.nU][i][j][k]
          +grid.dLocalGridOld[grid.nU][i][j][k+1]);
        dU_ip1halfjkm1half_nm1half=0.5*(grid.dLocalGridOld[grid.nU][i][j][k]
          +grid.dLocalGridOld[grid.nU][i][j][k-1]);
        dV_ip1halfjk_nm1half=(grid.dLocalGridOld[grid.nV][nICen+1][nJInt][k]
          +grid.dLocalGridOld[grid.nV][nICen+1][nJInt-1][k]
          +grid.dLocalGridOld[grid.nV][nICen][nJInt][k]
          +grid.dLocalGridOld[grid.nV][nICen][nJInt-1][k])*0.25;
        dW_ip1halfjk_nm1half=(grid.dLocalGridOld[grid.nW][nICen+1][j][nKInt]
          +grid.dLocalGridOld[grid.nW][nICen+1][j][nKInt-1]
          +grid.dLocalGridOld[grid.nW][nICen][j][nKInt]
          +grid.dLocalGridOld[grid.nW][nICen][j][nKInt-1])*0.25;
        dRho_ip1halfjk_n=(grid.dLocalGridOld[grid.nD][nICen+1][j][k]
          +grid.dLocalGridOld[grid.nD][nICen][j][k])*0.5;
        dP_ip1jk_n=grid.dLocalGridOld[grid.nP][nICen+1][j][k]
          +grid.dLocalGridOld[grid.nQ0][nICen+1][j][k];
        dP_ijk_n=grid.dLocalGridOld[grid.nP][nICen][j][k]+grid.dLocalGridOld[grid.nQ0][nICen][j][k];
        
        //Calculate derived quantities
        dUmU0_ijk_nm1half=grid.dLocalGridOld[grid.nU][i][j][k]
          -grid.dLocalGridOld[grid.nU0][i][0][0];
        
        //Calculate dA1
        dA1CenGrad=(dU_ip1jk_nm1half-dU_ijk_nm1half)/(grid.dLocalGridOld[grid.nDM][nICen+1][0][0]
          +grid.dLocalGridOld[grid.nDM][nICen][0][0])*2.0;
        dA1UpWindGrad=0.0;
        if(dUmU0_ijk_nm1half<0.0){//moving from outside in
          dA1UpWindGrad=(grid.dLocalGridOld[grid.nU][i+1][j][k]
            -grid.dLocalGridOld[grid.nU][i][j][k])/grid.dLocalGridOld[grid.nDM][nICen+1][0][0];
        }
        else{//moving from inside out
          dA1UpWindGrad=(grid.dLocalGridOld[grid.nU][i][j][k]
            -grid.dLocalGridOld[grid.nU][i-1][j][k])/grid.dLocalGridOld[grid.nDM][nICen][0][0];
        }
        dA1=dUmU0_ijk_nm1half*((1.0-parameters.dDonorFrac)*dA1CenGrad+parameters.dDonorFrac
          *dA1UpWindGrad);
        
        //calculate dS1
        dS1=(dP_ip1jk_n-dP_ijk_n)/((grid.dLocalGridOld[grid.nDM][nICen+1][0][0]
          +grid.dLocalGridOld[grid.nDM][nICen][0][0])*dRho_ip1halfjk_n)*2.0;
        
        //Calculate dS4
        dS4=parameters.dG*grid.dLocalGridOld[grid.nM][i][0][0]/dRSq_ip1half_n;
        
        //Calculate dA2
        dA2CenGrad=(dU_ip1halfjp1halfk_nm1half-dU_ip1halfjm1halfk_nm1half)
          /grid.dLocalGridOld[grid.nDTheta][0][j][0];
        dA2UpWindGrad=0.0;
        if(dV_ip1halfjk_nm1half>0.0){//moving in positive direction
          dA2UpWindGrad=(grid.dLocalGridOld[grid.nU][i][j][k]
            -grid.dLocalGridOld[grid.nU][i][j-1][k])/(grid.dLocalGridOld[grid.nDTheta][0][j][0]
            +grid.dLocalGridOld[grid.nDTheta][0][j-1][0])*2.0;
        }
        else{//moving in negative direction
          dA2UpWindGrad=(grid.dLocalGridOld[grid.nU][i][j+1][k]
            -grid.dLocalGridOld[grid.nU][i][j][k])/(grid.dLocalGridOld[grid.nDTheta][0][j+1][0]
            +grid.dLocalGridOld[grid.nDTheta][0][j][0])*2.0;
        }
        dA2=dV_ip1halfjk_nm1half*((1.0-parameters.dDonorFrac)*dA2CenGrad
          +parameters.dDonorFrac*dA2UpWindGrad)/grid.dLocalGridOld[grid.nR][i][0][0];
        
        //Calculate dS2
        dS2=-1.0*dV_ip1halfjk_nm1half*dV_ip1halfjk_nm1half/grid.dLocalGridOld[grid.nR][i][0][0];
        
        //Calculate dA3
        dA3CenGrad=(dU_ip1halfjkp1half_nm1half-dU_ip1halfjkm1half_nm1half)
          /grid.dLocalGridOld[grid.nDPhi][0][0][k];
        dA3UpWindGrad=0.0;
        if(dW_ip1halfjk_nm1half>0.0){//moving in positive direction
          dA3UpWindGrad=(grid.dLocalGridOld[grid.nU][i][j][k]
            -grid.dLocalGridOld[grid.nU][i][j][k-1])
            /(grid.dLocalGridOld[grid.nDPhi][0][0][k]
            +grid.dLocalGridOld[grid.nDPhi][0][0][k-1])*2.0;
        }
        else{//moving in negative direction
          dA3UpWindGrad=(grid.dLocalGridOld[grid.nU][i][j][k+1]
            -grid.dLocalGridOld[grid.nU][i][j][k])
            /(grid.dLocalGridOld[grid.nDPhi][0][0][k+1]
            +grid.dLocalGridOld[grid.nDPhi][0][0][k])*2.0;
        }
        dA3=dW_ip1halfjk_nm1half*((1.0-parameters.dDonorFrac)*dA3CenGrad
          +parameters.dDonorFrac*dA3UpWindGrad)/(grid.dLocalGridOld[grid.nR][i][0][0]
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]);
        
        //Calculate dS3
        dS3=-1.0*dW_ip1halfjk_nm1half*dW_ip1halfjk_nm1half
          /grid.dLocalGridOld[grid.nR][i][0][0];
        
        //calculate new velocity
        grid.dLocalGridNew[grid.nU][i][j][k]=grid.dLocalGridOld[grid.nU][i][j][k]
          -time.dDeltat_n*(4.0*parameters.dPi*dRhoAve_ip1halfjk_n*dRSq_ip1half_n*(dA1+dS1)+dA2+dS2
          +dA3+dS3+dS4);
      }
    }
  }
  
  //ghost region 0, outter most ghost region in x1 direction
  for(i=grid.nStartGhostUpdateExplicit[grid.nU][0][0];
    i<grid.nEndGhostUpdateExplicit[grid.nU][0][0];i++){
    
    //calculate i of centered quantities
    nICen=i-grid.nCenIntOffset[0];
    dRSq_ip1half_n=grid.dLocalGridOld[grid.nR][i][0][0]*grid.dLocalGridOld[grid.nR][i][0][0];
    dRhoAve_ip1halfjk_n=(grid.dLocalGridOld[grid.nDenAve][nICen][0][0])*0.5;
    
    for(j=grid.nStartGhostUpdateExplicit[grid.nU][0][1];
      j<grid.nEndGhostUpdateExplicit[grid.nU][0][1];j++){
      
      //calculate j of interface quantities
      nJInt=j+grid.nCenIntOffset[1];
      
      for(k=grid.nStartGhostUpdateExplicit[grid.nU][0][2];
        k<grid.nEndGhostUpdateExplicit[grid.nU][0][2];k++){
        
        //calculate k of interface quantities
        nKInt=k+grid.nCenIntOffset[2];
        
        //CALCULATE INTERPOLATED QUANTITIES
        dU_ip1jk_nm1half=grid.dLocalGridOld[grid.nU][i][j][k];/**\BC
          missing grid.dLocalGridOld[grid.nU][i+1][j][k] in calculation of \f$u_{i+1,j,k}\f$ setting
          \f$u_{i+1,j,k}\f$=\f$u_{i+1/2,j,k}\f$.*/
        dU_ijk_nm1half=(grid.dLocalGridOld[grid.nU][i][j][k]+grid.dLocalGridOld[grid.nU][i-1][j][k])
          *0.5;/**\BC Missing grid.dLocalGridOld[grid.nD][i+1][j][k] in calculation of 
          \f$\rho_{i+1/2,j,k}\f$, setting it to zero.*/
        dU_ip1halfjp1halfk_nm1half=0.5*(grid.dLocalGridOld[grid.nU][i][j+1][k]
          +grid.dLocalGridOld[grid.nU][i][j][k]);
        dU_ip1halfjm1halfk_nm1half=0.5*(grid.dLocalGridOld[grid.nU][i][j][k]
          +grid.dLocalGridOld[grid.nU][i][j-1][k]);
        dU_ip1halfjkp1half_nm1half=0.5*(grid.dLocalGridOld[grid.nU][i][j][k]
          +grid.dLocalGridOld[grid.nU][i][j][k+1]);
        dU_ip1halfjkm1half_nm1half=0.5*(grid.dLocalGridOld[grid.nU][i][j][k]
          +grid.dLocalGridOld[grid.nU][i][j][k-1]);
        dV_ip1halfjk_nm1half=0.5*(grid.dLocalGridOld[grid.nV][nICen][nJInt][k]
          +grid.dLocalGridOld[grid.nV][nICen][nJInt-1][k]);/**\BC assuming theta velocity is 
          constant across the surface.*/
        dW_ip1halfjk_nm1half=(grid.dLocalGridOld[grid.nW][nICen][j][nKInt]
          +grid.dLocalGridOld[grid.nW][nICen][j][nKInt-1])*0.5;/**\BC assuming phi velocity is
          constant across the surface.*/
        dRho_ip1halfjk_n=(grid.dLocalGridOld[grid.nD][nICen][j][k])*0.5;/**\BC Missing 
          grid.dLocalGridOld[grid.nDenAve][nICen+1][0][0] in calculation of 
          \f$\langle\rho\rangle_{i+1/2}\f$ setting it to zero.*/
        dP_ijk_n=grid.dLocalGridOld[grid.nP][nICen][j][k]+grid.dLocalGridOld[grid.nQ0][nICen][j][k];
        dP_ip1jk_n=-1.0*dP_ijk_n;
        
        //Calculate dA1
        dA1CenGrad=(dU_ip1jk_nm1half-dU_ijk_nm1half)/((0.5+parameters.dAlpha
          +parameters.dAlphaExtra)*grid.dLocalGridOld[grid.nDM][nICen][0][0]);/**\BC Missing
          grid.dLocalGridOld[grid.nDM][nICen+1][0][0] in calculation of centered \f$A_1\f$
          gradient, setting it equal to \ref Parameters.dAlpha
          *grid.dLocalGridOld[grid.nDM][nICen][0][0].*/
        dA1UpWindGrad=0.0;
        if(grid.dLocalGridOld[grid.nU][i][j][k]<0.0){//moving from outside in
          dA1UpWindGrad=dA1CenGrad;/**\BC Missing grid.dLocalGridOld[grid.nU][i+1][j][k] and
            grid.dLocalGridOld[grid.nDM][nICen+1][0][0] in calculation of upwind gradient, when 
            moving inward. Using centered gradient instead.*/
        }
        else{//moving from inside out
          dA1UpWindGrad=(grid.dLocalGridOld[grid.nU][i][j][k]
            -grid.dLocalGridOld[grid.nU][i-1][j][k])/grid.dLocalGridOld[grid.nDM][nICen][0][0];
        }
        dA1=(grid.dLocalGridOld[grid.nU][i][j][k]-grid.dLocalGridOld[grid.nU0][i][0][0])
          *((1.0-parameters.dDonorFrac)*dA1CenGrad+parameters.dDonorFrac*dA1UpWindGrad);
        
        //calculate dS1
        dS1=(dP_ip1jk_n-dP_ijk_n)/(grid.dLocalGridOld[grid.nDM][nICen][0][0]*(0.5+parameters.dAlpha
          +parameters.dAlphaExtra))/dRho_ip1halfjk_n;/**\BC Missing 
          grid.dLocalGridOld[grid.nDM][i+1][0][0] in calculation of \f$S_1\f$ using
          \ref Parameters.dAlpha *grid.dLocalGridOld[grid.nDM][nICen][0][0] instead.*/
        
        //Calculate dS4
        dS4=parameters.dG*grid.dLocalGridOld[grid.nM][i][0][0]/dRSq_ip1half_n;
        
        //Calculate dA2
        dA2CenGrad=(dU_ip1halfjp1halfk_nm1half-dU_ip1halfjm1halfk_nm1half)
          /grid.dLocalGridOld[grid.nDTheta][0][j][0];
        dA2UpWindGrad=0.0;
        if(dV_ip1halfjk_nm1half>0.0){//moving in positive direction
          dA2UpWindGrad=(grid.dLocalGridOld[grid.nU][i][j][k]
            -grid.dLocalGridOld[grid.nU][i][j-1][k])/(grid.dLocalGridOld[grid.nDTheta][0][j][0]
            +grid.dLocalGridOld[grid.nDTheta][0][j-1][0])*2.0;
        }
        else{//moving in negative direction
          dA2UpWindGrad=(grid.dLocalGridOld[grid.nU][i][j+1][k]
            -grid.dLocalGridOld[grid.nU][i][j][k])/(grid.dLocalGridOld[grid.nDTheta][0][j+1][0]
            +grid.dLocalGridOld[grid.nDTheta][0][j][0])*2.0;
        }
        dA2=dV_ip1halfjk_nm1half*((1.0-parameters.dDonorFrac)*dA2CenGrad+parameters.dDonorFrac
          *dA2UpWindGrad)/grid.dLocalGridOld[grid.nR][i][0][0];
        
        //Calculate dS2
        dS2=-1.0*dV_ip1halfjk_nm1half*dV_ip1halfjk_nm1half/grid.dLocalGridOld[grid.nR][i][0][0];
        
        //Calculate dA3
        dA3CenGrad=(dU_ip1halfjkp1half_nm1half-dU_ip1halfjkm1half_nm1half)
          /grid.dLocalGridOld[grid.nDPhi][0][0][k];
        dA3UpWindGrad=0.0;
        if(dW_ip1halfjk_nm1half>0.0){//moving in positive direction
          dA3UpWindGrad=(grid.dLocalGridOld[grid.nU][i][j][k]
            -grid.dLocalGridOld[grid.nU][i][j][k-1])/(grid.dLocalGridOld[grid.nDPhi][0][0][k]
            +grid.dLocalGridOld[grid.nDPhi][0][0][k-1])*2.0;
        }
        else{//moving in negative direction
          dA3UpWindGrad=(grid.dLocalGridOld[grid.nU][i][j][k+1]
            -grid.dLocalGridOld[grid.nU][i][j][k])/(grid.dLocalGridOld[grid.nDPhi][0][0][k+1]
            +grid.dLocalGridOld[grid.nDPhi][0][0][k])*2.0;
        }
        dA3=dW_ip1halfjk_nm1half*((1.0-parameters.dDonorFrac)*dA3CenGrad+parameters.dDonorFrac
          *dA3UpWindGrad)/(grid.dLocalGridOld[grid.nR][i][0][0]
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]);
        
        //Calculate dS3
        dS3=-1.0*dW_ip1halfjk_nm1half*dW_ip1halfjk_nm1half/grid.dLocalGridOld[grid.nR][i][0][0];
        
        //calculate new velocity
        grid.dLocalGridNew[grid.nU][i][j][k]=grid.dLocalGridOld[grid.nU][i][j][k]
          -time.dDeltat_n*(4.0*parameters.dPi*dRhoAve_ip1halfjk_n*dRSq_ip1half_n*(dA1+dS1)+dA2+dS2
          +dA3+dS3+dS4);
      }
    }
  }

  #if SEDOV==1
    //calculate gost region 1 inner most ghost region in x1 direction
    for(i=grid.nStartGhostUpdateExplicit[grid.nU][1][0];
      i<grid.nEndGhostUpdateExplicit[grid.nU][1][0];i++){
      
      //calculate i of centered quantities
      nICen=i-grid.nCenIntOffset[0];
      dRSq_ip1half_n=grid.dLocalGridOld[grid.nR][i][0][0]*grid.dLocalGridOld[grid.nR][i][0][0];
      dRhoAve_ip1halfjk_n=(grid.dLocalGridOld[grid.nDenAve][nICen+1][0][0]
        +grid.dLocalGridOld[grid.nDenAve][nICen][0][0])*0.5;
      
      for(j=grid.nStartGhostUpdateExplicit[grid.nU][1][1];
        j<grid.nEndGhostUpdateExplicit[grid.nU][1][1];j++){
        
        //calculate j of interface quantities
        nJInt=j+grid.nCenIntOffset[1];
        
        for(k=grid.nStartGhostUpdateExplicit[grid.nU][1][2];
          k<grid.nEndGhostUpdateExplicit[grid.nU][1][2];k++){
            
          //calculate k of interface quantities
          nKInt=k+grid.nCenIntOffset[2];
          
          //CALCULATE INTERPOLATED QUANTITIES
          dU_ip1jk_nm1half=(grid.dLocalGridOld[grid.nU][i+1][j][k]
            +grid.dLocalGridOld[grid.nU][i][j][k])*0.5;
          dU_ijk_nm1half=(grid.dLocalGridOld[grid.nU][i][j][k]
            +grid.dLocalGridOld[grid.nU][i-1][j][k])*0.5;
          dU_ip1halfjp1halfk_nm1half=(grid.dLocalGridOld[grid.nU][i][j+1][k]
            +grid.dLocalGridOld[grid.nU][i][j][k])*0.5;
          dU_ip1halfjm1halfk_nm1half=(grid.dLocalGridOld[grid.nU][i][j][k]
            +grid.dLocalGridOld[grid.nU][i][j-1][k])*0.5;
          dU_ip1halfjkp1half_nm1half=(grid.dLocalGridOld[grid.nU][i][j][k]
            +grid.dLocalGridOld[grid.nU][i][j][k+1])*0.5;
          dU_ip1halfjkm1half_nm1half=(grid.dLocalGridOld[grid.nU][i][j][k]
            +grid.dLocalGridOld[grid.nU][i][j][k-1])*0.5;
          dV_ip1halfjk_nm1half=(grid.dLocalGridOld[grid.nV][nICen+1][nJInt][k]
            +grid.dLocalGridOld[grid.nV][nICen+1][nJInt-1][k]
            +grid.dLocalGridOld[grid.nV][nICen][nJInt][k]
            +grid.dLocalGridOld[grid.nV][nICen][nJInt-1][k])*0.25;
          dW_ip1halfjk_nm1half=(grid.dLocalGridOld[grid.nW][nICen+1][j][nKInt]
            +grid.dLocalGridOld[grid.nW][nICen+1][j][nKInt-1]
            +grid.dLocalGridOld[grid.nW][nICen][j][nKInt]
            +grid.dLocalGridOld[grid.nW][nICen][j][nKInt-1])*0.25;
          dRho_ip1halfjk_n=(grid.dLocalGridOld[grid.nD][nICen+1][j][k]
            +grid.dLocalGridOld[grid.nD][nICen][j][k])*0.5;
          dP_ip1jk_n=grid.dLocalGridOld[grid.nP][nICen+1][j][k]
            +grid.dLocalGridOld[grid.nQ0][nICen+1][j][k];
          dP_ijk_n=grid.dLocalGridOld[grid.nP][nICen][j][k]
            +grid.dLocalGridOld[grid.nQ0][nICen][j][k];
          
          //Calculate dA1
          dA1CenGrad=(dU_ip1jk_nm1half-dU_ijk_nm1half)/(grid.dLocalGridOld[grid.nDM][nICen+1][0][0]
            +grid.dLocalGridOld[grid.nDM][nICen][0][0])*2.0;
          dA1UpWindGrad=0.0;
          dUmU0_ijk_nm1half=grid.dLocalGridOld[grid.nU][i][j][k]
            -grid.dLocalGridOld[grid.nU0][i][0][0];
          if(dUmU0_ijk_nm1half<0.0){//moving from outside in
            dA1UpWindGrad=(grid.dLocalGridOld[grid.nU][i+1][j][k]
              -grid.dLocalGridOld[grid.nU][i][j][k])
              /grid.dLocalGridOld[grid.nDM][nICen+1][0][0];
          }
          else{//moving from inside out
            dA1UpWindGrad=(grid.dLocalGridOld[grid.nU][i][j][k]
              -grid.dLocalGridOld[grid.nU][i-1][j][k])
              /grid.dLocalGridOld[grid.nDM][nICen][0][0];
          }
          dA1=dUmU0_ijk_nm1half*((1.0-parameters.dDonorFrac)*dA1CenGrad+parameters.dDonorFrac
            *dA1UpWindGrad);
          
          //calculate dS1
          dS1=(dP_ip1jk_n-dP_ijk_n)/((grid.dLocalGridOld[grid.nDM][nICen+1][0][0]
            +grid.dLocalGridOld[grid.nDM][nICen][0][0])*dRho_ip1halfjk_n)*2.0;
          
          //Calculate dS4
          dS4=parameters.dG*grid.dLocalGridOld[grid.nM][i][0][0]/dRSq_ip1half_n;
          
          //Calculate dA2
          dA2CenGrad=(dU_ip1halfjp1halfk_nm1half-dU_ip1halfjm1halfk_nm1half)
            /grid.dLocalGridOld[grid.nDTheta][0][j][0];
          dA2UpWindGrad=0.0;
          if(dV_ip1halfjk_nm1half>0.0){//moving in positive direction
            dA2UpWindGrad=(grid.dLocalGridOld[grid.nU][i][j][k]
              -grid.dLocalGridOld[grid.nU][i][j-1][k])/(grid.dLocalGridOld[grid.nDTheta][0][j][0]
              +grid.dLocalGridOld[grid.nDTheta][0][j-1][0])*2.0;
          }
          else{//moving in negative direction
            dA2UpWindGrad=(grid.dLocalGridOld[grid.nU][i][j+1][k]
              -grid.dLocalGridOld[grid.nU][i][j][k])/(grid.dLocalGridOld[grid.nDTheta][0][j+1][0]
              +grid.dLocalGridOld[grid.nDTheta][0][j][0])*2.0;
          }
          dA2=dV_ip1halfjk_nm1half*((1.0-parameters.dDonorFrac)*dA2CenGrad+parameters.dDonorFrac
            *dA2UpWindGrad)/grid.dLocalGridOld[grid.nR][i][0][0];
          
          //Calculate dS2
          dS2=-1.0*dV_ip1halfjk_nm1half*dV_ip1halfjk_nm1half/grid.dLocalGridOld[grid.nR][i][0][0];
          
          //Calculate dA3
          dA3CenGrad=(dU_ip1halfjkp1half_nm1half-dU_ip1halfjkm1half_nm1half)
            /grid.dLocalGridOld[grid.nDPhi][0][0][k];
          dA3UpWindGrad=0.0;
          if(dW_ip1halfjk_nm1half>0.0){//moving in positive direction
            dA3UpWindGrad=(grid.dLocalGridOld[grid.nU][i][j][k]
              -grid.dLocalGridOld[grid.nU][i][j][k-1])/(grid.dLocalGridOld[grid.nDPhi][0][0][k]
              +grid.dLocalGridOld[grid.nDPhi][0][0][k-1])*2.0;
          }
          else{//moving in negative direction
            dA3UpWindGrad=(grid.dLocalGridOld[grid.nU][i][j][k+1]
              -grid.dLocalGridOld[grid.nU][i][j][k])/(grid.dLocalGridOld[grid.nDPhi][0][0][k+1]
              +grid.dLocalGridOld[grid.nDPhi][0][0][k])*2.0;
          }
          dA3=dW_ip1halfjk_nm1half*((1.0-parameters.dDonorFrac)*dA3CenGrad
            +parameters.dDonorFrac*dA3UpWindGrad)/(grid.dLocalGridOld[grid.nR][i][0][0]
            *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]);
          
          //Calculate dS3
          dS3=-1.0*dW_ip1halfjk_nm1half*dW_ip1halfjk_nm1half
            /grid.dLocalGridOld[grid.nR][i][0][0];
          
          //calculate new velocity
          grid.dLocalGridNew[grid.nU][i][j][k]=grid.dLocalGridOld[grid.nU][i][j][k]
            -time.dDeltat_n*(4.0*parameters.dPi*dRhoAve_ip1halfjk_n*dRSq_ip1half_n*(dA1+dS1)+dA2
             +dS2+dA3+dS3+dS4);
        }
      }
    }
  #endif

}
void calNewU_RTP_LES(Grid &grid,Parameters &parameters,Time &time,ProcTop &procTop){
  int i;
  int j;
  int k;
  int nICen;
  int nJInt;
  int nKInt;
  double dRho_ip1halfjk_n;
  double dP_ip1jk_n;
  double dP_ijk_n;
  double dA1CenGrad;
  double dA1UpWindGrad;
  double dA1;
  double dS1;
  double dA2CenGrad;
  double dA2UpWindGrad;
  double dA3CenGrad;
  double dA3UpWindGrad;
  double dA2;
  double dS2;
  double dA3;
  double dS3;
  double dS4;
  double dTA1;
  double dTS1;
  double dTA2;
  double dTS2;
  double dTA3;
  double dTS3;
  double dTS4;
  double dDivU_ijk_n;
  double dDivU_ip1jk_n;
  double dTau_rr_ip1jk_n;
  double dTau_rr_ijk_n;
  double dTau_rt_ip1halfjp1halfk_n;
  double dTau_rt_ip1halfjm1halfk_n;
  double dTau_rp_ip1halfjkp1half_n;
  double dTau_rp_ip1halfjkm1half_n;
  double dR_i_n;
  double dR_ip1_n;
  double dU_ip1jk_nm1half;
  double dU_ijk_nm1half;
  double dU_ip1halfjp1halfk_nm1half;
  double dU_ip1halfjm1halfk_nm1half;
  double dU_ip1halfjkp1half_nm1half;
  double dU_ip1halfjkm1half_nm1half;
  double dU0_ip1_nm1half;
  double dU0_i_nm1half;
  double dUmU0_ip1halfjk_nm1half;
  double dV_ip1halfjk_nm1half;
  double dV_ip1halfjp1halfk_nm1half;
  double dV_ip1halfjm1halfk_nm1half;
  double dV_ip1jk_nm1half;
  double dV_ijk_nm1half;
  double dW_ip1halfjk_nm1half;
  double dW_ip1halfjkp1half_nm1half;
  double dW_ip1halfjkm1half_nm1half;
  double dV_R_ip1jk_n;
  double dV_R_ip1jp1halfk_n;
  double dV_R_ip1jm1halfk_n;
  double dV_R_ijp1halfk_n;
  double dV_R_ijm1halfk_n;
  double dV_R_ijk_n;
  double dW_R_ip1jkp1half_n;
  double dW_R_ijkp1half_n;
  double dW_R_ip1jkm1half_n;
  double dW_R_ijkm1half_n;
  double dRSq_ip1half_n;
  double dRSq_im1half_n;
  double dRSq_ip1_n;
  double dRSq_i_n;
  double dRSq_ip3half_n;
  double dRCu_ip1half_n;
  double dRSqUmU0_ip3halfjk_n;
  double dRSqUmU0_ip1halfjk_n;
  double dRSqUmU0_im1halfjk_n;
  double dRSqUmU0_ijk_n;//needed at surface boundary
  double dRhoR_ip1halfjk_n;
  double dDM_ip1half;
  double dDTheta_jp1half;
  double dDTheta_jm1half;
  double dDPhi_kp1half;
  double dDPhi_km1half;
  double dEddyVisc_ip1halfjk_n;
  double dEddyVisc_ip1halfjp1halfk_n;
  double dEddyVisc_ip1halfjm1halfk_n;
  double dEddyVisc_ip1halfjkp1half_n;
  double dEddyVisc_ip1halfjkm1half_n;
  double dRhoAve_ip1half_n;
  double dEddyViscosityTerms;
  
  //calculate new u
  for(i=grid.nStartUpdateExplicit[grid.nU][0];i<grid.nEndUpdateExplicit[grid.nU][0];i++){
    
    //calculate i of centered quantities
    nICen=i-grid.nCenIntOffset[0];
    
    //calculate quantities that vary only with radius
    dR_ip1_n=(grid.dLocalGridOld[grid.nR][i+1][0][0]+grid.dLocalGridOld[grid.nR][i][0][0])*0.5;
    dR_i_n=(grid.dLocalGridOld[grid.nR][i][0][0]+grid.dLocalGridOld[grid.nR][i-1][0][0])*0.5;
    dRSq_ip1_n=dR_ip1_n*dR_ip1_n;
    dRSq_i_n=dR_i_n*dR_i_n;
    dRSq_ip1half_n=grid.dLocalGridOld[grid.nR][i][0][0]*grid.dLocalGridOld[grid.nR][i][0][0];
    dRSq_im1half_n=grid.dLocalGridOld[grid.nR][i-1][0][0]*grid.dLocalGridOld[grid.nR][i-1][0][0];
    dRSq_ip3half_n=grid.dLocalGridOld[grid.nR][i+1][0][0]*grid.dLocalGridOld[grid.nR][i+1][0][0];
    dRCu_ip1half_n=dRSq_ip1half_n*grid.dLocalGridOld[grid.nR][i][0][0];
    dDM_ip1half=(grid.dLocalGridOld[grid.nDM][nICen+1][0][0]
      +grid.dLocalGridOld[grid.nDM][nICen][0][0])*0.5;
    dRhoAve_ip1half_n=(grid.dLocalGridOld[grid.nDenAve][nICen+1][0][0]
      +grid.dLocalGridOld[grid.nDenAve][nICen][0][0])*0.5;
    dU0_ip1_nm1half=(grid.dLocalGridOld[grid.nU0][i+1][0][0]
      +grid.dLocalGridOld[grid.nU0][i][0][0])*0.5;
    dU0_i_nm1half=(grid.dLocalGridOld[grid.nU0][i][0][0]
      +grid.dLocalGridOld[grid.nU0][i-1][0][0])*0.5;
    
    for(j=grid.nStartUpdateExplicit[grid.nU][1];j<grid.nEndUpdateExplicit[grid.nU][1];j++){
      
      //calculate j of interface quantities
      nJInt=j+grid.nCenIntOffset[1];
      
      //calculating quantities that vary only with theta, and perhaps radius
      dDTheta_jp1half=(grid.dLocalGridOld[grid.nDTheta][0][j+1][0]
        +grid.dLocalGridOld[grid.nDTheta][0][j][0])*0.5;
      dDTheta_jm1half=(grid.dLocalGridOld[grid.nDTheta][0][j-1][0]
        +grid.dLocalGridOld[grid.nDTheta][0][j][0])*0.5;
      
      for(k=grid.nStartUpdateExplicit[grid.nU][2];k<grid.nEndUpdateExplicit[grid.nU][2];k++){
        
        //calculate k of interface quantities
        nKInt=k+grid.nCenIntOffset[2];
        dDPhi_kp1half=(grid.dLocalGridOld[grid.nDPhi][0][0][k+1]
        +grid.dLocalGridOld[grid.nDPhi][0][0][k])*0.5;
        dDPhi_km1half=(grid.dLocalGridOld[grid.nDPhi][0][0][k]
        +grid.dLocalGridOld[grid.nDPhi][0][0][k-1])*0.5;
        
        //CALCULATE INTERPOLATED QUANTITIES
        dU_ip1jk_nm1half=(grid.dLocalGridOld[grid.nU][i+1][j][k]
          +grid.dLocalGridOld[grid.nU][i][j][k])*0.5;
        dU_ijk_nm1half=(grid.dLocalGridOld[grid.nU][i][j][k]
          +grid.dLocalGridOld[grid.nU][i-1][j][k])*0.5;
        dUmU0_ip1halfjk_nm1half=grid.dLocalGridOld[grid.nU][i][j][k]
          -grid.dLocalGridOld[grid.nU0][i][0][0];
        dU_ip1halfjp1halfk_nm1half=(grid.dLocalGridOld[grid.nU][i][j+1][k]
          +grid.dLocalGridOld[grid.nU][i][j][k])*0.5;
        dU_ip1halfjm1halfk_nm1half=(grid.dLocalGridOld[grid.nU][i][j-1][k]
          +grid.dLocalGridOld[grid.nU][i][j][k])*0.5;
        dU_ip1halfjkp1half_nm1half=(grid.dLocalGridOld[grid.nU][i][j][k]
          +grid.dLocalGridOld[grid.nU][i][j][k+1])*0.5;
        dU_ip1halfjkm1half_nm1half=(grid.dLocalGridOld[grid.nU][i][j][k]
          +grid.dLocalGridOld[grid.nU][i][j][k-1])*0.5;
        dRho_ip1halfjk_n=(grid.dLocalGridOld[grid.nD][nICen+1][j][k]
          +grid.dLocalGridOld[grid.nD][nICen][j][k])*0.5;
        dV_ip1halfjk_nm1half=0.25*(grid.dLocalGridOld[grid.nV][nICen+1][nJInt][k]
          +grid.dLocalGridOld[grid.nV][nICen+1][nJInt-1][k]
          +grid.dLocalGridOld[grid.nV][nICen][nJInt][k]
          +grid.dLocalGridOld[grid.nV][nICen][nJInt-1][k]);
        dV_ip1halfjp1halfk_nm1half=(grid.dLocalGridOld[grid.nV][nICen+1][nJInt][k]
          +grid.dLocalGridOld[grid.nV][nICen][nJInt][k])*0.5;
        dV_ip1halfjm1halfk_nm1half=(grid.dLocalGridOld[grid.nV][nICen+1][nJInt-1][k]
          +grid.dLocalGridOld[grid.nV][nICen][nJInt-1][k])*0.5;
        dV_ip1jk_nm1half=(grid.dLocalGridOld[grid.nV][nICen+1][nJInt][k]
          +grid.dLocalGridOld[grid.nV][nICen+1][nJInt-1][k])*0.5;
        dV_ijk_nm1half=(grid.dLocalGridOld[grid.nV][nICen][nJInt][k]
          +grid.dLocalGridOld[grid.nV][nICen][nJInt-1][k])*0.5;
        dW_ip1halfjk_nm1half=(grid.dLocalGridOld[grid.nW][nICen+1][j][nKInt]
          +grid.dLocalGridOld[grid.nW][nICen+1][j][nKInt-1]
          +grid.dLocalGridOld[grid.nW][nICen][j][nKInt]
          +grid.dLocalGridOld[grid.nW][nICen][j][nKInt-1])*0.25;
        dW_ip1halfjkp1half_nm1half=(grid.dLocalGridOld[grid.nW][nICen+1][j][nKInt]
          +grid.dLocalGridOld[grid.nW][nICen][j][nKInt])*0.5;
        dW_ip1halfjkm1half_nm1half=(grid.dLocalGridOld[grid.nW][nICen+1][j][nKInt-1]
          +grid.dLocalGridOld[grid.nW][nICen][j][nKInt-1])*0.5;
        dP_ip1jk_n=grid.dLocalGridOld[grid.nP][nICen+1][j][k]
          +grid.dLocalGridOld[grid.nQ0][nICen+1][j][k]+grid.dLocalGridOld[grid.nQ1][nICen+1][j][k]
          +grid.dLocalGridOld[grid.nQ2][nICen+1][j][k];
        dP_ijk_n=grid.dLocalGridOld[grid.nP][nICen][j][k]
          +grid.dLocalGridOld[grid.nQ0][nICen][j][k]+grid.dLocalGridOld[grid.nQ1][nICen][j][k]
          +grid.dLocalGridOld[grid.nQ2][nICen][j][k];
        dEddyVisc_ip1halfjk_n=(grid.dLocalGridOld[grid.nEddyVisc][nICen][j][k]
          +grid.dLocalGridOld[grid.nEddyVisc][nICen+1][j][k])*0.5;
        dEddyVisc_ip1halfjp1halfk_n=(grid.dLocalGridOld[grid.nEddyVisc][nICen][j][k]
          +grid.dLocalGridOld[grid.nEddyVisc][nICen][j+1][k]
          +grid.dLocalGridOld[grid.nEddyVisc][nICen+1][j][k]
          +grid.dLocalGridOld[grid.nEddyVisc][nICen+1][j+1][k])*0.25;
        dEddyVisc_ip1halfjm1halfk_n=(grid.dLocalGridOld[grid.nEddyVisc][nICen][j][k]
          +grid.dLocalGridOld[grid.nEddyVisc][nICen][j-1][k]
          +grid.dLocalGridOld[grid.nEddyVisc][nICen+1][j][k]
          +grid.dLocalGridOld[grid.nEddyVisc][nICen+1][j-1][k])*0.25;
        dEddyVisc_ip1halfjkp1half_n=(grid.dLocalGridOld[grid.nEddyVisc][nICen][j][k+1]
          +grid.dLocalGridOld[grid.nEddyVisc][nICen][j][k]
          +grid.dLocalGridOld[grid.nEddyVisc][nICen+1][j][k+1]
          +grid.dLocalGridOld[grid.nEddyVisc][nICen+1][j][k])*0.25;
        dEddyVisc_ip1halfjkm1half_n=(grid.dLocalGridOld[grid.nEddyVisc][nICen][j][k]
          +grid.dLocalGridOld[grid.nEddyVisc][nICen][j][k-1]
          +grid.dLocalGridOld[grid.nEddyVisc][nICen+1][j][k]
          +grid.dLocalGridOld[grid.nEddyVisc][nICen+1][j][k-1])*0.25;
        
        //calculate derived quantities
        dRSqUmU0_ip3halfjk_n=dRSq_ip3half_n*(grid.dLocalGridOld[grid.nU][i+1][j][k]
          -grid.dLocalGridOld[grid.nU0][i+1][0][0]);
        dRSqUmU0_ip1halfjk_n=dRSq_ip1half_n*(grid.dLocalGridOld[grid.nU][i][j][k]
          -grid.dLocalGridOld[grid.nU0][i][0][0]);
        dRSqUmU0_im1halfjk_n=dRSq_im1half_n*(grid.dLocalGridOld[grid.nU][i-1][j][k]
          -grid.dLocalGridOld[grid.nU0][i-1][0][0]);
        dV_R_ip1jk_n=dV_ip1jk_nm1half/dR_ip1_n;
        dV_R_ip1jp1halfk_n=grid.dLocalGridOld[grid.nV][nICen+1][nJInt][k]/dR_ip1_n;
        dV_R_ip1jm1halfk_n=grid.dLocalGridOld[grid.nV][nICen+1][nJInt-1][k]/dR_ip1_n;
        dV_R_ijp1halfk_n=grid.dLocalGridOld[grid.nV][nICen][nJInt][k]/dR_i_n;
        dV_R_ijm1halfk_n=grid.dLocalGridOld[grid.nV][nICen][nJInt-1][k]/dR_i_n;
        dV_R_ijk_n=dV_ijk_nm1half/dR_i_n;
        dW_R_ip1jkp1half_n=grid.dLocalGridOld[grid.nW][nICen+1][j][nKInt]/dR_ip1_n;
        dW_R_ijkp1half_n=grid.dLocalGridOld[grid.nW][nICen][j][nKInt]/dR_i_n;
        dW_R_ip1jkm1half_n=grid.dLocalGridOld[grid.nW][nICen+1][j][nKInt-1]/dR_ip1_n;
        dW_R_ijkm1half_n=grid.dLocalGridOld[grid.nW][nICen][j][nKInt-1]/dR_i_n;
        dRhoR_ip1halfjk_n=dRho_ip1halfjk_n*grid.dLocalGridOld[grid.nR][i][0][0];
        
        //Calculate dA1
        dA1CenGrad=(dU_ip1jk_nm1half-dU_ijk_nm1half)
          /(grid.dLocalGridOld[grid.nDM][nICen+1][0][0]
          +grid.dLocalGridOld[grid.nDM][nICen][0][0])*2.0;
        dA1UpWindGrad=0.0;
        if(dUmU0_ip1halfjk_nm1half<0.0){//moving from outside in
          dA1UpWindGrad=(grid.dLocalGridOld[grid.nU][i+1][j][k]
            -grid.dLocalGridOld[grid.nU][i][j][k])
            /grid.dLocalGridOld[grid.nDM][nICen+1][0][0];
        }
        else{//moving from inside out
          dA1UpWindGrad=(grid.dLocalGridOld[grid.nU][i][j][k]
            -grid.dLocalGridOld[grid.nU][i-1][j][k])
            /grid.dLocalGridOld[grid.nDM][nICen][0][0];
        }
        dA1=dUmU0_ip1halfjk_nm1half*((1.0-parameters.dDonorFrac)*dA1CenGrad+parameters.dDonorFrac
          *dA1UpWindGrad);
        
        //calculate dS1
        dS1=(dP_ip1jk_n-dP_ijk_n)/(dDM_ip1half*dRho_ip1halfjk_n);
        
        //Calculate dS4
        dS4=parameters.dG*grid.dLocalGridOld[grid.nM][i][0][0]/dRSq_ip1half_n;
        
        //Calculate dA2
        dA2CenGrad=(dU_ip1halfjp1halfk_nm1half-dU_ip1halfjm1halfk_nm1half)
          /grid.dLocalGridOld[grid.nDTheta][0][j][0];
        dA2UpWindGrad=0.0;
        if(dV_ip1halfjk_nm1half>0.0){//moving in positive direction
          dA2UpWindGrad=(grid.dLocalGridOld[grid.nU][i][j][k]
            -grid.dLocalGridOld[grid.nU][i][j-1][k])
            /(grid.dLocalGridOld[grid.nDTheta][0][j][0]
            +grid.dLocalGridOld[grid.nDTheta][0][j-1][0])*2.0;
        }
        else{//moving in negative direction
          dA2UpWindGrad=(grid.dLocalGridOld[grid.nU][i][j+1][k]
            -grid.dLocalGridOld[grid.nU][i][j][k])
            /(grid.dLocalGridOld[grid.nDTheta][0][j+1][0]
            +grid.dLocalGridOld[grid.nDTheta][0][j][0])*2.0;
        }
        dA2=dV_ip1halfjk_nm1half*((1.0-parameters.dDonorFrac)*dA2CenGrad
          +parameters.dDonorFrac*dA2UpWindGrad)/grid.dLocalGridOld[grid.nR][i][0][0];
        
        //Calculate dS2
        dS2=dV_ip1halfjk_nm1half*dV_ip1halfjk_nm1half
          /grid.dLocalGridOld[grid.nR][i][0][0];
        
        //Calculate dA3
        dA3CenGrad=(dU_ip1halfjkp1half_nm1half-dU_ip1halfjkm1half_nm1half)
          /grid.dLocalGridOld[grid.nDPhi][0][0][k];
        dA3UpWindGrad=0.0;
        if(dW_ip1halfjk_nm1half>0.0){//moving in positive direction
          dA3UpWindGrad=(grid.dLocalGridOld[grid.nU][i][j][k]
            -grid.dLocalGridOld[grid.nU][i][j][k-1])
            /(grid.dLocalGridOld[grid.nDPhi][0][0][k]
            +grid.dLocalGridOld[grid.nDPhi][0][0][k-1])*2.0;
        }
        else{//moving in negative direction
          dA3UpWindGrad=(grid.dLocalGridOld[grid.nU][i][j][k+1]
            -grid.dLocalGridOld[grid.nU][i][j][k])
            /(grid.dLocalGridOld[grid.nDPhi][0][0][k+1]
            +grid.dLocalGridOld[grid.nDPhi][0][0][k])*2.0;
        }
        dA3=dW_ip1halfjk_nm1half*((1.0-parameters.dDonorFrac)*dA3CenGrad+parameters.dDonorFrac
          *dA3UpWindGrad)/(grid.dLocalGridOld[grid.nR][i][0][0]
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]);
        
        //Calculate dS3
        dS3=dW_ip1halfjk_nm1half*dW_ip1halfjk_nm1half/grid.dLocalGridOld[grid.nR][i][0][0];
        
        //cal DivU_ip1jk_n
        dDivU_ip1jk_n=4.0*parameters.dPi*grid.dLocalGridOld[grid.nDenAve][nICen+1][0][0]
          *(dRSqUmU0_ip3halfjk_n-dRSqUmU0_ip1halfjk_n)/grid.dLocalGridOld[grid.nDM][nICen+1][0][0]
          +(grid.dLocalGridOld[grid.nV][nICen+1][nJInt][k]
          *grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt][0]
          -grid.dLocalGridOld[grid.nV][nICen+1][nJInt-1][k]
          *grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt-1][0])
          /(grid.dLocalGridOld[grid.nDTheta][0][j][0]*dR_ip1_n
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0])
          +(grid.dLocalGridOld[grid.nW][nICen+1][j][nKInt]
          -grid.dLocalGridOld[grid.nW][nICen+1][j][nKInt-1])
          /(grid.dLocalGridOld[grid.nDPhi][0][0][k]*dR_ip1_n
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]);
        
        //cal DivU_ijk_n
        dDivU_ijk_n=4.0*parameters.dPi*grid.dLocalGridOld[grid.nDenAve][nICen][0][0]
          *(dRSqUmU0_ip1halfjk_n-dRSqUmU0_im1halfjk_n)/grid.dLocalGridOld[grid.nDM][nICen][0][0]
          +(grid.dLocalGridOld[grid.nV][nICen][nJInt][k]
          *grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt][0]
          -grid.dLocalGridOld[grid.nV][nICen][nJInt-1][k]
          *grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt-1][0])
          /(grid.dLocalGridOld[grid.nDTheta][0][j][0]*dR_i_n
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0])
          +(grid.dLocalGridOld[grid.nW][nICen][j][nKInt]
          -grid.dLocalGridOld[grid.nW][nICen][j][nKInt-1])
          /(grid.dLocalGridOld[grid.nDPhi][0][0][k]*dR_i_n
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]);
        
        //cal Tau_rr_ip1jk_n
        dTau_rr_ip1jk_n=2.0*grid.dLocalGridOld[grid.nEddyVisc][nICen+1][j][k]*(4.0*parameters.dPi
          *dRSq_ip1_n*grid.dLocalGridOld[grid.nDenAve][nICen+1][0][0]
          *((grid.dLocalGridOld[grid.nU][i+1][j][k]-grid.dLocalGridOld[grid.nU0][i+1][0][0])
          -(grid.dLocalGridOld[grid.nU][i][j][k]-grid.dLocalGridOld[grid.nU0][i][0][0]))
          /grid.dLocalGridOld[grid.nDM][nICen+1][0][0]-0.3333333333333333*dDivU_ip1jk_n);
        
        //cal Tau_rr_ijk_n
        dTau_rr_ijk_n=2.0*grid.dLocalGridOld[grid.nEddyVisc][nICen][j][k]*(4.0*parameters.dPi
          *dRSq_i_n*grid.dLocalGridOld[grid.nDenAve][nICen][0][0]
          *((grid.dLocalGridOld[grid.nU][i][j][k]-grid.dLocalGridOld[grid.nU0][i][0][0])
          -(grid.dLocalGridOld[grid.nU][i-1][j][k]-grid.dLocalGridOld[grid.nU0][i-1][0][0]))
          /grid.dLocalGridOld[grid.nDM][nICen][0][0]-0.3333333333333333*dDivU_ijk_n);
        
        //calculate dTau_rt_ip1halfjp1halfk_n
        dTau_rt_ip1halfjp1halfk_n=dEddyVisc_ip1halfjp1halfk_n*(4.0*parameters.dPi*dRCu_ip1half_n
          *dRhoAve_ip1half_n*(dV_R_ip1jp1halfk_n-dV_R_ijp1halfk_n)/dDM_ip1half
          +((grid.dLocalGridOld[grid.nU][i][j+1][k]
          -grid.dLocalGridOld[grid.nU0][i][0][0])-(grid.dLocalGridOld[grid.nU][i][j][k]
          -grid.dLocalGridOld[grid.nU0][i][0][0]))/(dDTheta_jp1half
          *grid.dLocalGridOld[grid.nR][i][0][0]));
        
        //calculate dTau_rt_ip1halfjm1halfk_n
        dTau_rt_ip1halfjm1halfk_n=dEddyVisc_ip1halfjm1halfk_n*(4.0*parameters.dPi*dRCu_ip1half_n
          *dRhoAve_ip1half_n*(dV_R_ip1jm1halfk_n-dV_R_ijm1halfk_n)/dDM_ip1half
          +((grid.dLocalGridOld[grid.nU][i][j][k]
          -grid.dLocalGridOld[grid.nU0][i][0][0])-(grid.dLocalGridOld[grid.nU][i][j-1][k]
          -grid.dLocalGridOld[grid.nU0][i][0][0]))/(grid.dLocalGridOld[grid.nR][i][0][0]
          *dDTheta_jm1half));
        
        //calculate dTau_rp_ip1halfjkp1half_n
        dTau_rp_ip1halfjkp1half_n=dEddyVisc_ip1halfjkp1half_n*(4.0*parameters.dPi*dRCu_ip1half_n
          *dRhoAve_ip1half_n*(dW_R_ip1jkp1half_n-dW_R_ijkp1half_n)/dDM_ip1half
          +((grid.dLocalGridOld[grid.nU][i][j][k+1]-grid.dLocalGridOld[grid.nU0][i][0][0])
          -(grid.dLocalGridOld[grid.nU][i][j][k]-grid.dLocalGridOld[grid.nU0][i][0][0]))
          /(grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]*grid.dLocalGridOld[grid.nR][i][0][0]
          *dDPhi_kp1half));
          
        //calculate dTau_rp_im1halfjkm1half_n
        dTau_rp_ip1halfjkm1half_n=dEddyVisc_ip1halfjkm1half_n*(4.0*parameters.dPi*dRCu_ip1half_n
          *dRhoAve_ip1half_n*(dW_R_ip1jkm1half_n-dW_R_ijkm1half_n)/dDM_ip1half
          +((grid.dLocalGridOld[grid.nU][i][j][k]-grid.dLocalGridOld[grid.nU0][i][0][0])
          -(grid.dLocalGridOld[grid.nU][i][j][k-1]-grid.dLocalGridOld[grid.nU0][i][0][0]))
          /(grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]*grid.dLocalGridOld[grid.nR][i][0][0]
          *dDPhi_km1half));
        
        //cal dTA1
        dTA1=(dTau_rr_ip1jk_n-dTau_rr_ijk_n)/(dDM_ip1half*dRho_ip1halfjk_n);
        
        //cal dTS1
        dTS1=dEddyVisc_ip1halfjk_n/dRhoR_ip1halfjk_n*(4.0
          *((dU_ip1jk_nm1half-dU0_ip1_nm1half)-(dU_ijk_nm1half-dU0_i_nm1half))/dDM_ip1half
          +grid.dLocalGridOld[grid.nR][i][0][0]*grid.dLocalGridOld[grid.nCotThetaIJK][0][j][0]
          *(dV_R_ip1jk_n-dV_R_ijk_n)/dDM_ip1half);
        
        //calculate dTA2
        dTA2=(dTau_rt_ip1halfjp1halfk_n-dTau_rt_ip1halfjm1halfk_n)
          /(grid.dLocalGridOld[grid.nDTheta][0][j][0]*dRhoR_ip1halfjk_n);
        
        //calculate dTS2
        dTS2=(2.0*(dV_ip1halfjp1halfk_nm1half-dV_ip1halfjm1halfk_nm1half)
          -grid.dLocalGridOld[grid.nCotThetaIJK][0][j][0]*((dU_ip1halfjp1halfk_nm1half
          -grid.dLocalGridOld[grid.nU0][i][0][0])-(dU_ip1halfjm1halfk_nm1half
          -grid.dLocalGridOld[grid.nU0][i][0][0])))/(grid.dLocalGridOld[grid.nR][i][0][0]
          *grid.dLocalGridOld[grid.nDTheta][0][j][0]);
        
        //calculate dTA3
        dTA3=(dTau_rp_ip1halfjkp1half_n-dTau_rp_ip1halfjkm1half_n)/(dRho_ip1halfjk_n
          *grid.dLocalGridOld[grid.nR][i][0][0]*grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]
          *grid.dLocalGridOld[grid.nDPhi][0][0][k]);
        
        //calculate dTS3
        dTS3=2.0*(dW_ip1halfjkp1half_nm1half-dW_ip1halfjkm1half_nm1half)
          /(grid.dLocalGridOld[grid.nR][i][0][0]*grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]
          *grid.dLocalGridOld[grid.nDPhi][0][0][k]);
        
        //cal dTS4
        dTS4=(4.0*(grid.dLocalGridOld[grid.nU][i][j][k]-grid.dLocalGridOld[grid.nU0][i][0][0])
          +2.0*grid.dLocalGridOld[grid.nCotThetaIJK][0][j][0]*dV_ip1halfjk_nm1half)
          /grid.dLocalGridOld[grid.nR][i][0][0];
        
        dEddyViscosityTerms=-4.0*parameters.dPi*dRhoAve_ip1half_n*dRSq_ip1half_n*(dTA1+dTS1)-dTA2
          -dTA3+dEddyVisc_ip1halfjk_n/dRhoR_ip1halfjk_n*(dTS2+dTS3+dTS4);
        
        //calculate new velocity
        grid.dLocalGridNew[grid.nU][i][j][k]=grid.dLocalGridOld[grid.nU][i][j][k]
          -time.dDeltat_n*(4.0*parameters.dPi*dRhoAve_ip1half_n*dRSq_ip1half_n*(dA1+dS1)
          +dA2-dS2+dA3-dS3+dS4+dEddyViscosityTerms);
      }
    }
  }
  
  //ghost region 0, outter most ghost region in x1 direction
  for(i=grid.nStartGhostUpdateExplicit[grid.nU][0][0];
    i<grid.nEndGhostUpdateExplicit[grid.nU][0][0];i++){
    
    //calculate i of centered quantities
    nICen=i-grid.nCenIntOffset[0];
    
    //calculate quantities that vary only with radius
    dR_i_n=(grid.dLocalGridOld[grid.nR][i][0][0]+grid.dLocalGridOld[grid.nR][i-1][0][0])*0.5;
    dRSq_i_n=dR_i_n*dR_i_n;
    dRSq_ip1half_n=grid.dLocalGridOld[grid.nR][i][0][0]*grid.dLocalGridOld[grid.nR][i][0][0];
    dRCu_ip1half_n=grid.dLocalGridOld[grid.nR][i][0][0]*grid.dLocalGridOld[grid.nR][i][0][0]
      *grid.dLocalGridOld[grid.nR][i][0][0];
    dDM_ip1half=(grid.dLocalGridOld[grid.nDM][nICen][0][0])*(0.5+parameters.dAlpha
      +parameters.dAlphaExtra);/**\BC Missing grid.dLocalGridOld[grid.nDM][i+1][0][0] in calculation
      of \f$S_1\f$ using \ref Parameters.dAlpha *grid.dLocalGridOld[grid.nDM][nICen][0][0] instead.
      */
    dRhoAve_ip1half_n=(grid.dLocalGridOld[grid.nDenAve][nICen][0][0])*0.5;/**\BC Missing density
      outside of surface, setting it to zero.*/
    dU0_i_nm1half=(grid.dLocalGridOld[grid.nU0][i][0][0]+grid.dLocalGridOld[grid.nU0][i-1][0][0])
      *0.5;
    dR_ip1_n=grid.dLocalGridOld[grid.nR][i][0][0];
    
    for(j=grid.nStartGhostUpdateExplicit[grid.nU][0][1];
      j<grid.nEndGhostUpdateExplicit[grid.nU][0][1];j++){
      
      //calculate j of interface quantities
      nJInt=j+grid.nCenIntOffset[1];
      
      //calculating quantities that vary only with theta, and perhaps radius
      dDTheta_jp1half=(grid.dLocalGridOld[grid.nDTheta][0][j+1][0]
        +grid.dLocalGridOld[grid.nDTheta][0][j][0])*0.5;
      dDTheta_jm1half=(grid.dLocalGridOld[grid.nDTheta][0][j-1][0]
        +grid.dLocalGridOld[grid.nDTheta][0][j][0])*0.5;
      
      for(k=grid.nStartGhostUpdateExplicit[grid.nU][0][2];
        k<grid.nEndGhostUpdateExplicit[grid.nU][0][2];k++){
        
        //calculate k of interface quantities
        nKInt=k+grid.nCenIntOffset[2];
        dDPhi_kp1half=(grid.dLocalGridOld[grid.nDPhi][0][0][k+1]
        +grid.dLocalGridOld[grid.nDPhi][0][0][k])*0.5;
        dDPhi_km1half=(grid.dLocalGridOld[grid.nDPhi][0][0][k]
        +grid.dLocalGridOld[grid.nDPhi][0][0][k-1])*0.5;
        
        //CALCULATE INTERPOLATED QUANTITIES
        dU_ip1jk_nm1half=grid.dLocalGridOld[grid.nU][i][j][k];
        dU_ijk_nm1half=(grid.dLocalGridOld[grid.nU][i][j][k]
          +grid.dLocalGridOld[grid.nU][i-1][j][k])*0.5;
        dUmU0_ip1halfjk_nm1half=grid.dLocalGridOld[grid.nU][i][j][k]
          -grid.dLocalGridOld[grid.nU0][i][0][0];
        dU_ip1halfjp1halfk_nm1half=(grid.dLocalGridOld[grid.nU][i][j+1][k]
          +grid.dLocalGridOld[grid.nU][i][j][k])*0.5;
        dU_ip1halfjm1halfk_nm1half=(grid.dLocalGridOld[grid.nU][i][j-1][k]
          +grid.dLocalGridOld[grid.nU][i][j][k])*0.5;
        dU_ip1halfjkp1half_nm1half=(grid.dLocalGridOld[grid.nU][i][j][k]
          +grid.dLocalGridOld[grid.nU][i][j][k+1])*0.5;
        dU_ip1halfjkm1half_nm1half=(grid.dLocalGridOld[grid.nU][i][j][k]
          +grid.dLocalGridOld[grid.nU][i][j][k-1])*0.5;
        dRho_ip1halfjk_n=(grid.dLocalGridOld[grid.nD][nICen][j][k])*0.5;/**\BC Missing density 
          outside model, setting it to zero. */
        dV_ip1halfjk_nm1half=0.5*(grid.dLocalGridOld[grid.nV][nICen][nJInt][k]
          +grid.dLocalGridOld[grid.nV][nICen][nJInt-1][k]);/**\BC assuming theta and phi velocity 
          same outside star as inside.*/
        dV_ip1halfjp1halfk_nm1half=grid.dLocalGridOld[grid.nV][nICen][nJInt][k];/**\BC Assuming 
          theta velocities are constant across surface.*/
        dV_ip1halfjm1halfk_nm1half=grid.dLocalGridOld[grid.nV][nICen][nJInt-1][k];
        dV_ip1jk_nm1half=(grid.dLocalGridOld[grid.nV][nICen][nJInt][k]
          +grid.dLocalGridOld[grid.nV][nICen][nJInt-1][k])*0.5;/**\BC assuming that $V$ at
          $i+1$ is equal to $v$ at $i$.*/
        dV_ijk_nm1half=(grid.dLocalGridOld[grid.nV][nICen][nJInt][k]
          +grid.dLocalGridOld[grid.nV][nICen][nJInt-1][k])*0.5;
        dW_ip1halfjk_nm1half=(grid.dLocalGridOld[grid.nW][nICen][j][nKInt]
          +grid.dLocalGridOld[grid.nW][nICen][j][nKInt-1])*0.5;
        dW_ip1halfjkp1half_nm1half=grid.dLocalGridOld[grid.nW][nICen][j][nKInt];
        dW_ip1halfjkm1half_nm1half=grid.dLocalGridOld[grid.nW][nICen][j][nKInt-1];
        dP_ijk_n=grid.dLocalGridOld[grid.nP][nICen][j][k]
          +grid.dLocalGridOld[grid.nQ0][nICen][j][k]+grid.dLocalGridOld[grid.nQ1][nICen][j][k];
        dP_ip1jk_n=-1.0*dP_ijk_n;/**\BC Missing pressure outside surface setting it equal to 
          negative pressure in the center of the first cell so that it will be zero at surface.*/
        dEddyVisc_ip1halfjk_n=grid.dLocalGridOld[grid.nEddyVisc][nICen][j][k]*0.5;/**\BC assume 
          viscosity is zero outside the star.*/
        dEddyVisc_ip1halfjp1halfk_n=(grid.dLocalGridOld[grid.nEddyVisc][nICen][j][k]
          +grid.dLocalGridOld[grid.nEddyVisc][nICen][j+1][k])*0.25;
        dEddyVisc_ip1halfjm1halfk_n=(grid.dLocalGridOld[grid.nEddyVisc][nICen][j][k]
          +grid.dLocalGridOld[grid.nEddyVisc][nICen][j-1][k])*0.25;
        dEddyVisc_ip1halfjkp1half_n=(grid.dLocalGridOld[grid.nEddyVisc][nICen][j][k+1]
          +grid.dLocalGridOld[grid.nEddyVisc][nICen][j][k])*0.25;
        dEddyVisc_ip1halfjkm1half_n=(grid.dLocalGridOld[grid.nEddyVisc][nICen][j][k]
          +grid.dLocalGridOld[grid.nEddyVisc][nICen][j][k-1])*0.25;
        
        //calculate derived quantities
        dRSqUmU0_ijk_n=dRSq_i_n*(dU_ijk_nm1half-dU0_i_nm1half);
        dRSqUmU0_ip1halfjk_n=dRSq_ip1half_n*(grid.dLocalGridOld[grid.nU][i][j][k]
          -grid.dLocalGridOld[grid.nU0][i][0][0]);
        dRSqUmU0_im1halfjk_n=dRSq_im1half_n*(grid.dLocalGridOld[grid.nU][i-1][j][k]
          -grid.dLocalGridOld[grid.nU0][i-1][0][0]);
        dV_R_ip1jk_n=dV_ip1jk_nm1half/dR_ip1_n;
        dV_R_ip1jp1halfk_n=grid.dLocalGridOld[grid.nV][nICen][nJInt][k]/dR_ip1_n;
        dV_R_ip1jm1halfk_n=grid.dLocalGridOld[grid.nV][nICen][nJInt-1][k]/dR_ip1_n;
        dV_R_ijp1halfk_n=grid.dLocalGridOld[grid.nV][nICen][nJInt][k]/dR_i_n;
        dV_R_ijm1halfk_n=grid.dLocalGridOld[grid.nV][nICen][nJInt-1][k]/dR_i_n;
        dV_R_ijk_n=dV_ijk_nm1half/dR_i_n;
        dW_R_ip1jkp1half_n=grid.dLocalGridOld[grid.nW][nICen][j][nKInt]/dR_ip1_n;
        dW_R_ijkp1half_n=grid.dLocalGridOld[grid.nW][nICen][j][nKInt]/dR_i_n;
        dW_R_ip1jkm1half_n=grid.dLocalGridOld[grid.nW][nICen][j][nKInt-1]/dR_ip1_n;
        dW_R_ijkm1half_n=grid.dLocalGridOld[grid.nW][nICen][j][nKInt-1]/dR_i_n;
        dRhoR_ip1halfjk_n=dRho_ip1halfjk_n*grid.dLocalGridOld[grid.nR][i][0][0];
        
        //Calculate dA1
        dA1CenGrad=(dU_ip1jk_nm1half-dU_ijk_nm1half)
          /(grid.dLocalGridOld[grid.nDM][nICen][0][0]*0.5);/**\BC Missing mass outside model,
          setting it to zero.*/
        dA1UpWindGrad=0.0;
        if(dUmU0_ip1halfjk_nm1half<0.0){//moving from outside in
          dA1UpWindGrad=dA1CenGrad;/**\BC Missing grid.dLocalGridOld[grid.nU][i+1][j][k] and 
            grid.dLocalGridOld[grid.nDM][nICen+1][0][0] in calculation of upwind gradient, when 
            moving inward. Using centered gradient instead.*/
        }
        else{//moving from inside out
          dA1UpWindGrad=(grid.dLocalGridOld[grid.nU][i][j][k]
            -grid.dLocalGridOld[grid.nU][i-1][j][k])/grid.dLocalGridOld[grid.nDM][nICen][0][0];
        }
        dA1=dUmU0_ip1halfjk_nm1half*((1.0-parameters.dDonorFrac)*dA1CenGrad+parameters.dDonorFrac
          *dA1UpWindGrad);
        
        //calculate dS1
        dS1=(dP_ip1jk_n-dP_ijk_n)/(dDM_ip1half*dRho_ip1halfjk_n);
        
        //Calculate dS4
        dS4=parameters.dG*grid.dLocalGridOld[grid.nM][i][0][0]/dRSq_ip1half_n;
        
        //Calculate dA2
        dA2CenGrad=(dU_ip1halfjp1halfk_nm1half-dU_ip1halfjm1halfk_nm1half)
          /grid.dLocalGridOld[grid.nDTheta][0][j][0];
        dA2UpWindGrad=0.0;
        if(dV_ip1halfjk_nm1half>0.0){//moving in positive direction
          dA2UpWindGrad=(grid.dLocalGridOld[grid.nU][i][j][k]
            -grid.dLocalGridOld[grid.nU][i][j-1][k])/(grid.dLocalGridOld[grid.nDTheta][0][j][0]
            +grid.dLocalGridOld[grid.nDTheta][0][j-1][0])*2.0;
        }
        else{//moving in negative direction
          dA2UpWindGrad=(grid.dLocalGridOld[grid.nU][i][j+1][k]
            -grid.dLocalGridOld[grid.nU][i][j][k])/(grid.dLocalGridOld[grid.nDTheta][0][j+1][0]
            +grid.dLocalGridOld[grid.nDTheta][0][j][0])*2.0;
        }
        dA2=dV_ip1halfjk_nm1half*((1.0-parameters.dDonorFrac)*dA2CenGrad
          +parameters.dDonorFrac*dA2UpWindGrad)/grid.dLocalGridOld[grid.nR][i][0][0];
        
        //Calculate dS2
        dS2=dV_ip1halfjk_nm1half*dV_ip1halfjk_nm1half/grid.dLocalGridOld[grid.nR][i][0][0];
        
        //Calculate dA3
        dA3CenGrad=(dU_ip1halfjkp1half_nm1half-dU_ip1halfjkm1half_nm1half)
          /grid.dLocalGridOld[grid.nDPhi][0][0][k];
        dA3UpWindGrad=0.0;
        if(dW_ip1halfjk_nm1half>0.0){//moving in positive direction
          dA3UpWindGrad=(grid.dLocalGridOld[grid.nU][i][j][k]
            -grid.dLocalGridOld[grid.nU][i][j][k-1])
            /(grid.dLocalGridOld[grid.nDPhi][0][0][k]
            +grid.dLocalGridOld[grid.nDPhi][0][0][k-1])*2.0;
        }
        else{//moving in negative direction
          dA3UpWindGrad=(grid.dLocalGridOld[grid.nU][i][j][k+1]
            -grid.dLocalGridOld[grid.nU][i][j][k])
            /(grid.dLocalGridOld[grid.nDPhi][0][0][k+1]
            +grid.dLocalGridOld[grid.nDPhi][0][0][k])*2.0;
        }
        dA3=dW_ip1halfjk_nm1half*((1.0-parameters.dDonorFrac)*dA3CenGrad+parameters.dDonorFrac
          *dA3UpWindGrad)/(grid.dLocalGridOld[grid.nR][i][0][0]
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]);
        
        //Calculate dS3
        dS3=dW_ip1halfjk_nm1half*dW_ip1halfjk_nm1half/grid.dLocalGridOld[grid.nR][i][0][0];
        
        //cal DivU_ip1jk_n
        dDivU_ip1jk_n=4.0*parameters.dPi*dRhoAve_ip1half_n
          *(dRSqUmU0_ip1halfjk_n-dRSqUmU0_ijk_n)/grid.dLocalGridOld[grid.nDM][nICen][0][0]*2.0
          +(grid.dLocalGridOld[grid.nV][nICen][nJInt][k]
          *grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt][0]
          -grid.dLocalGridOld[grid.nV][nICen][nJInt-1][k]
          *grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt-1][0])
          /(grid.dLocalGridOld[grid.nDTheta][0][j][0]*dR_ip1_n
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0])
          +(grid.dLocalGridOld[grid.nW][nICen][j][nKInt]
          -grid.dLocalGridOld[grid.nW][nICen][j][nKInt-1])
          /(grid.dLocalGridOld[grid.nDPhi][0][0][k]*dR_ip1_n
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]);
        
        //cal DivU_ijk_n
        dDivU_ijk_n=4.0*parameters.dPi*grid.dLocalGridOld[grid.nDenAve][nICen][0][0]
          *(dRSqUmU0_ip1halfjk_n-dRSqUmU0_im1halfjk_n)/grid.dLocalGridOld[grid.nDM][nICen][0][0]
          +(grid.dLocalGridOld[grid.nV][nICen][nJInt][k]
          *grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt][0]
          -grid.dLocalGridOld[grid.nV][nICen][nJInt-1][k]
          *grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt-1][0])
          /(grid.dLocalGridOld[grid.nDTheta][0][j][0]*dR_i_n
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0])
          +(grid.dLocalGridOld[grid.nW][nICen][j][nKInt]
          -grid.dLocalGridOld[grid.nW][nICen][j][nKInt-1])
          /(grid.dLocalGridOld[grid.nDPhi][0][0][k]*dR_i_n
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]);
        
        //cal Tau_rr_ip1jk_n
        dTau_rr_ip1jk_n=2.0*dEddyVisc_ip1halfjk_n*(4.0*parameters.dPi*dRSq_ip1half_n
          *dRhoAve_ip1half_n*((grid.dLocalGridOld[grid.nU][i][j][k]
          -grid.dLocalGridOld[grid.nU0][i][0][0])-(dU_ijk_nm1half-dU0_i_nm1half))
          /grid.dLocalGridOld[grid.nDM][nICen][0][0]*2.0-0.3333333333333333*dDivU_ip1jk_n);
        
        //cal Tau_rr_ijk_n
        dTau_rr_ijk_n=2.0*grid.dLocalGridOld[grid.nEddyVisc][nICen][j][k]*(4.0*parameters.dPi
          *dRSq_i_n*grid.dLocalGridOld[grid.nDenAve][nICen][0][0]
          *((grid.dLocalGridOld[grid.nU][i][j][k]-grid.dLocalGridOld[grid.nU0][i][0][0])
          -(grid.dLocalGridOld[grid.nU][i-1][j][k]-grid.dLocalGridOld[grid.nU0][i-1][0][0]))
          /grid.dLocalGridOld[grid.nDM][nICen][0][0]-0.3333333333333333*dDivU_ijk_n);
        
        //calculate dTau_rt_ip1halfjp1halfk_n
        dTau_rt_ip1halfjp1halfk_n=dEddyVisc_ip1halfjp1halfk_n*(4.0*parameters.dPi*dRCu_ip1half_n
          *dRhoAve_ip1half_n*(dV_R_ip1jp1halfk_n-dV_R_ijp1halfk_n)/dDM_ip1half
          +((grid.dLocalGridOld[grid.nU][i][j+1][k]
          -grid.dLocalGridOld[grid.nU0][i][0][0])-(grid.dLocalGridOld[grid.nU][i][j][k]
          -grid.dLocalGridOld[grid.nU0][i][0][0]))/(dDTheta_jp1half
          *grid.dLocalGridOld[grid.nR][i][0][0]));
        
        //calculate dTau_rt_ip1halfjm1halfk_n
        dTau_rt_ip1halfjm1halfk_n=dEddyVisc_ip1halfjm1halfk_n*(4.0*parameters.dPi*dRCu_ip1half_n
          *dRhoAve_ip1half_n*(dV_R_ip1jm1halfk_n-dV_R_ijm1halfk_n)/dDM_ip1half
          +((grid.dLocalGridOld[grid.nU][i][j][k]
          -grid.dLocalGridOld[grid.nU0][i][0][0])-(grid.dLocalGridOld[grid.nU][i][j-1][k]
          -grid.dLocalGridOld[grid.nU0][i][0][0]))/(grid.dLocalGridOld[grid.nR][i][0][0]
          *dDTheta_jm1half));
        
        //calculate dTau_rp_ip1halfjkp1half_n
        dTau_rp_ip1halfjkp1half_n=dEddyVisc_ip1halfjkp1half_n*(4.0*parameters.dPi*dRCu_ip1half_n
          *dRhoAve_ip1half_n*(dW_R_ip1jkp1half_n-dW_R_ijkp1half_n)/dDM_ip1half
          +((grid.dLocalGridOld[grid.nU][i][j][k+1]-grid.dLocalGridOld[grid.nU0][i][0][0])
          -(grid.dLocalGridOld[grid.nU][i][j][k]-grid.dLocalGridOld[grid.nU0][i][0][0]))
          /(grid.dLocalGridOld[grid.nR][i][0][0]*grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]
          *dDPhi_kp1half));
          
        //calculate dTau_rp_im1halfjkm1half_n
        dTau_rp_ip1halfjkm1half_n=dEddyVisc_ip1halfjkm1half_n*(4.0*parameters.dPi*dRCu_ip1half_n
          *dRhoAve_ip1half_n*(dW_R_ip1jkm1half_n-dW_R_ijkm1half_n)/dDM_ip1half
          +((grid.dLocalGridOld[grid.nU][i][j][k]-grid.dLocalGridOld[grid.nU0][i][0][0])
          -(grid.dLocalGridOld[grid.nU][i][j][k-1]-grid.dLocalGridOld[grid.nU0][i][0][0]))
          /(grid.dLocalGridOld[grid.nR][i][0][0]*grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]
          *dDPhi_km1half));
        
        //cal dTA1
        dTA1=(dTau_rr_ip1jk_n-dTau_rr_ijk_n)/(dDM_ip1half*dRho_ip1halfjk_n);
        
        //cal dTS1
        dTS1=dEddyVisc_ip1halfjk_n/dRhoR_ip1halfjk_n*(4.0
          *((dU_ip1jk_nm1half-dU0_ip1_nm1half)-(dU_ijk_nm1half-dU0_i_nm1half))/dDM_ip1half
          +grid.dLocalGridOld[grid.nR][i][0][0]*grid.dLocalGridOld[grid.nCotThetaIJK][0][j][0]
          *(dV_R_ip1jk_n-dV_R_ijk_n)/dDM_ip1half);
        
        //calculate dTA2
        dTA2=(dTau_rt_ip1halfjp1halfk_n-dTau_rt_ip1halfjm1halfk_n)
          /(grid.dLocalGridOld[grid.nDTheta][0][j][0]*dRhoR_ip1halfjk_n);
        
        //calculate dTS2
        dTS2=(2.0*(dV_ip1halfjp1halfk_nm1half-dV_ip1halfjm1halfk_nm1half)
          -grid.dLocalGridOld[grid.nCotThetaIJK][0][j][0]*((dU_ip1halfjp1halfk_nm1half
          -grid.dLocalGridOld[grid.nU0][i][0][0])-(dU_ip1halfjm1halfk_nm1half
          -grid.dLocalGridOld[grid.nU0][i][0][0])))/(grid.dLocalGridOld[grid.nR][i][0][0]
          *grid.dLocalGridOld[grid.nDTheta][0][j][0]);
        
        //cal dTS4
        dTS4=(4.0*(grid.dLocalGridOld[grid.nU][i][j][k]-grid.dLocalGridOld[grid.nU0][i][0][0])
          +2.0*grid.dLocalGridOld[grid.nCotThetaIJK][0][j][0]*dV_ip1halfjk_nm1half)
          /grid.dLocalGridOld[grid.nR][i][0][0];
        
        dEddyViscosityTerms=-4.0*parameters.dPi*dRhoAve_ip1half_n*dRSq_ip1half_n*(dTA1+dTS1)-dTA2
          -dTA3+dEddyVisc_ip1halfjk_n/dRhoR_ip1halfjk_n*(dTS2+dTS3+dTS4);
        //dEddyViscosityTerms=0.0;
        
        //calculate new velocity
        grid.dLocalGridNew[grid.nU][i][j][k]=grid.dLocalGridOld[grid.nU][i][j][k]
          -time.dDeltat_n*(4.0*parameters.dPi*dRhoAve_ip1half_n*dRSq_ip1half_n*(dA1+dS1)
          +dA2-dS2+dA3-dS3+dS4+dEddyViscosityTerms);
      }
    }
  }
}
void calNewV_RT(Grid &grid,Parameters &parameters,Time &time,ProcTop &procTop){
  
  int i;
  int j;
  int k;
  int nIInt;
  int nJCen;
  double dR_i_n;
  double dU0i_n;
  double dU_ijp1halfk_n;
  double dV_ip1halfjp1halfk_n;
  double dV_im1halfjp1halfk_n;
  double dV_ijp1halfk_n;
  double dV_ijp1k_n;
  double dV_ijk_n;
  double dDeltaTheta_jp1half;
  double dRho_ijp1halfk_n;
  double dA1CenGrad;
  double dA1UpWindGrad;
  double dU_U0_Diff;
  double dA1;
  double dS1;
  double dA2CenGrad;
  double dA2UpWindGrad;
  double dA2;
  double dP_ijp1k_n;
  double dP_ijk_n;
  double dS2;
  
  //calculate new v
  for(i=grid.nStartUpdateExplicit[grid.nV][0];i<grid.nEndUpdateExplicit[grid.nV][0];i++){
    
    //calculate j of interface quantities
    nIInt=i+grid.nCenIntOffset[0];
    dR_i_n=(grid.dLocalGridOld[grid.nR][nIInt][0][0]
      +grid.dLocalGridOld[grid.nR][nIInt-1][0][0])*0.5;
    dU0i_n=0.5*(grid.dLocalGridOld[grid.nU0][nIInt][0][0]
      +grid.dLocalGridOld[grid.nU0][nIInt-1][0][0]);
    for(j=grid.nStartUpdateExplicit[grid.nV][1];j<grid.nEndUpdateExplicit[grid.nV][1];j++){
      
      //calculate j of centered quantities
      nJCen=j-grid.nCenIntOffset[1];
      
      for(k=grid.nStartUpdateExplicit[grid.nV][2];k<grid.nEndUpdateExplicit[grid.nV][2];k++){
        
        //Calculate interpolated quantities
        dU_ijp1halfk_n=0.25*(grid.dLocalGridOld[grid.nU][nIInt][nJCen][k]
          +grid.dLocalGridOld[grid.nU][nIInt][nJCen+1][k]
          +grid.dLocalGridOld[grid.nU][nIInt-1][nJCen][k]
          +grid.dLocalGridOld[grid.nU][nIInt-1][nJCen+1][k]);
        dV_ip1halfjp1halfk_n=0.5*(grid.dLocalGridOld[grid.nV][i+1][j][k]
          +grid.dLocalGridOld[grid.nV][i][j][k]);
        dV_im1halfjp1halfk_n=0.5*(grid.dLocalGridOld[grid.nV][i][j][k]
          +grid.dLocalGridOld[grid.nV][i-1][j][k]);
        dV_ijp1halfk_n=grid.dLocalGridOld[grid.nV][i][j][k];
        dV_ijp1k_n=(grid.dLocalGridOld[grid.nV][i][j+1][k]
          +grid.dLocalGridOld[grid.nV][i][j][k])*0.5;
        dV_ijk_n=(grid.dLocalGridOld[grid.nV][i][j][k]
          +grid.dLocalGridOld[grid.nV][i][j-1][k])*0.5;
        dDeltaTheta_jp1half=(grid.dLocalGridOld[grid.nDTheta][0][nJCen+1][0]
          +grid.dLocalGridOld[grid.nDTheta][0][nJCen][0])*0.5;
        dRho_ijp1halfk_n=(grid.dLocalGridOld[grid.nD][i][nJCen][k]
          +grid.dLocalGridOld[grid.nD][i][nJCen+1][k])*0.5;
        dP_ijp1k_n=grid.dLocalGridOld[grid.nP][i][nJCen+1][k]
          +grid.dLocalGridOld[grid.nQ0][i][nJCen+1][k]+grid.dLocalGridOld[grid.nQ1][i][nJCen+1][k];
        dP_ijk_n=grid.dLocalGridOld[grid.nP][i][nJCen][k]+grid.dLocalGridOld[grid.nQ0][i][nJCen][k]
          +grid.dLocalGridOld[grid.nQ1][i][nJCen][k];
        
        //calculate derived quantities
        dU_U0_Diff=dU_ijp1halfk_n-dU0i_n;
        
        //calculate A1
        dA1CenGrad=(dV_ip1halfjp1halfk_n-dV_im1halfjp1halfk_n)
          /grid.dLocalGridOld[grid.nDM][i][0][0];
        dA1UpWindGrad=0.0;
        if(dU_U0_Diff<0.0){//moving in a negative direction
          dA1UpWindGrad=(grid.dLocalGridOld[grid.nV][i+1][j][k]
            -grid.dLocalGridOld[grid.nV][i][j][k])/(grid.dLocalGridOld[grid.nDM][i][0][0]
            +grid.dLocalGridOld[grid.nDM][i+1][0][0])*2.0;
        }
        else{//moving in a positive direction
          dA1UpWindGrad=(grid.dLocalGridOld[grid.nV][i][j][k]
            -grid.dLocalGridOld[grid.nV][i-1][j][k])/(grid.dLocalGridOld[grid.nDM][i][0][0]
            +grid.dLocalGridOld[grid.nDM][i-1][0][0])*2.0;
        }
        dA1=4.0*parameters.dPi*dR_i_n*dR_i_n*grid.dLocalGridOld[grid.nDenAve][i][0][0]
          *dU_U0_Diff*((1.0-parameters.dDonorFrac)*dA1CenGrad+parameters.dDonorFrac*dA1UpWindGrad);
        
        //calculate S1
        dS1=dU_ijp1halfk_n*dV_ijp1halfk_n/dR_i_n;
        
        //calculate dA2
        dA2CenGrad=(dV_ijp1k_n-dV_ijk_n)/dDeltaTheta_jp1half;
        dA2UpWindGrad=0.0;
        if(dV_ijp1halfk_n<0.0){//moning in a negative direction
          dA2UpWindGrad=(grid.dLocalGridOld[grid.nV][i][j+1][k]
            -grid.dLocalGridOld[grid.nV][i][j][k])/grid.dLocalGridOld[grid.nDTheta][0][j+1][0];
        }
        else{//moving in a positive direction
          dA2UpWindGrad=(grid.dLocalGridOld[grid.nV][i][j][k]
            -grid.dLocalGridOld[grid.nV][i][j-1][k])/grid.dLocalGridOld[grid.nDTheta][0][j][0];
        }
        dA2=dV_ijp1halfk_n/dR_i_n*((1.0-parameters.dDonorFrac)*dA2CenGrad
          +parameters.dDonorFrac*dA2UpWindGrad);
        
        //calculate S2
        dS2=(dP_ijp1k_n-dP_ijk_n)/dDeltaTheta_jp1half/dRho_ijp1halfk_n/dR_i_n;
        
        //calculate new velocity
        grid.dLocalGridNew[grid.nV][i][j][k]=grid.dLocalGridOld[grid.nV][i][j][k]
          -time.dDeltat_n*(dA1+dS1+dA2+dS2);
      }
    }
  }
  
  //ghost region 0, outter most ghost region in x1 direction
  for(i=grid.nStartGhostUpdateExplicit[grid.nV][0][0];
    i<grid.nEndGhostUpdateExplicit[grid.nV][0][0];i++){
    
    //calculate j of interface quantities
    nIInt=i+grid.nCenIntOffset[0];
    dR_i_n=(grid.dLocalGridOld[grid.nR][nIInt][0][0]
      +grid.dLocalGridOld[grid.nR][nIInt-1][0][0])*0.5;
    dU0i_n=0.5*(grid.dLocalGridOld[grid.nU0][nIInt][0][0]
      +grid.dLocalGridOld[grid.nU0][nIInt-1][0][0]);
    
    for(j=grid.nStartGhostUpdateExplicit[grid.nV][0][1];
      j<grid.nEndGhostUpdateExplicit[grid.nV][0][1];j++){
      
      //calculate j of centered quantities
      nJCen=j-grid.nCenIntOffset[1];
      dDeltaTheta_jp1half=(grid.dLocalGridOld[grid.nDTheta][0][nJCen+1][0]
        +grid.dLocalGridOld[grid.nDTheta][0][nJCen][0])*0.5;
      
      for(k=grid.nStartGhostUpdateExplicit[grid.nV][0][2];
        k<grid.nEndGhostUpdateExplicit[grid.nV][0][2];k++){
        
        //Calculate interpolated quantities
        dU_ijp1halfk_n=0.25*(grid.dLocalGridOld[grid.nU][nIInt][nJCen][k]
          +grid.dLocalGridOld[grid.nU][nIInt][nJCen+1][k]
          +grid.dLocalGridOld[grid.nU][nIInt-1][nJCen][k]
          +grid.dLocalGridOld[grid.nU][nIInt-1][nJCen+1][k]);
        dV_ip1halfjp1halfk_n=grid.dLocalGridOld[grid.nV][i][j][k];/**\BC 
          grid.dLocalGridOld[grid.nV][i+1][j+1][k] is missing*/
        dV_im1halfjp1halfk_n=0.5*(grid.dLocalGridOld[grid.nV][i][j][k]
          +grid.dLocalGridOld[grid.nV][i-1][j][k]);
        dV_ijp1halfk_n=grid.dLocalGridOld[grid.nV][i][j][k];
        dV_ijp1k_n=(grid.dLocalGridOld[grid.nV][i][j+1][k]
          +grid.dLocalGridOld[grid.nV][i][j][k])*0.5;
         dV_ijk_n=(grid.dLocalGridOld[grid.nV][i][j][k]
          +grid.dLocalGridOld[grid.nV][i][j-1][k])*0.5;
        dRho_ijp1halfk_n=(grid.dLocalGridOld[grid.nD][i][nJCen][k]
          +grid.dLocalGridOld[grid.nD][i][nJCen+1][k])*0.5;
        dP_ijp1k_n=grid.dLocalGridOld[grid.nP][i][nJCen+1][k]
          +grid.dLocalGridOld[grid.nQ0][i][nJCen+1][k]+grid.dLocalGridOld[grid.nQ1][i][nJCen+1][k];
        dP_ijk_n=grid.dLocalGridOld[grid.nP][i][nJCen][k]+grid.dLocalGridOld[grid.nQ0][i][nJCen][k]
          +grid.dLocalGridOld[grid.nQ1][i][nJCen][k];
        
        //calculate derived quantities
        dU_U0_Diff=dU_ijp1halfk_n-dU0i_n;
        
        //calculate A1
        dA1CenGrad=(dV_ip1halfjp1halfk_n-dV_im1halfjp1halfk_n)
          /grid.dLocalGridOld[grid.nDM][i][0][0];
        dA1UpWindGrad=0.0;
        
        if(dU_U0_Diff<0.0){//moving in a negative direction
          dA1UpWindGrad=dA1CenGrad;/**\BC missing upwind gradient, using centred gradient instead*/
        }
        else{//moving in a positive direction*/
          dA1UpWindGrad=(grid.dLocalGridOld[grid.nV][i][j][k]
            -grid.dLocalGridOld[grid.nV][i-1][j][k])/(grid.dLocalGridOld[grid.nDM][i][0][0]
            +grid.dLocalGridOld[grid.nDM][i-1][0][0])*2.0;
        }
        dA1=4.0*parameters.dPi*dR_i_n*dR_i_n*grid.dLocalGridOld[grid.nDenAve][i][0][0]*dU_U0_Diff
          *((1.0-parameters.dDonorFrac)*dA1CenGrad+parameters.dDonorFrac*dA1UpWindGrad);
        
        //calculate S1
        dS1=dU_ijp1halfk_n*dV_ijp1halfk_n/dR_i_n;
        
        //calculate dA2
        dA2CenGrad=(dV_ijp1k_n-dV_ijk_n)/dDeltaTheta_jp1half;
        dA2UpWindGrad=0.0;
        if(dV_ijp1halfk_n<0.0){//moning in a negative direction
          dA2UpWindGrad=(grid.dLocalGridOld[grid.nV][i][j+1][k]
            -grid.dLocalGridOld[grid.nV][i][j][k])/grid.dLocalGridOld[grid.nDTheta][0][j+1][0];
        }
        else{//moving in a positive direction
          dA2UpWindGrad=(grid.dLocalGridOld[grid.nV][i][j][k]
            -grid.dLocalGridOld[grid.nV][i][j-1][k])/grid.dLocalGridOld[grid.nDTheta][0][j][0];
        }
        dA2=dV_ijp1halfk_n/dR_i_n*((1.0-parameters.dDonorFrac)*dA2CenGrad+parameters.dDonorFrac
          *dA2UpWindGrad);
        
        //calculate S2
        dS2=(dP_ijp1k_n-dP_ijk_n)/dDeltaTheta_jp1half/dRho_ijp1halfk_n/dR_i_n;
        
        //calculate new velocity
        grid.dLocalGridNew[grid.nV][i][j][k]=grid.dLocalGridOld[grid.nV][i][j][k]
          -time.dDeltat_n*(dA1+dS1+dA2+dS2);
      }
    }
  }
#if SEDOV==1
    
    //calculate gost region 1 inner most ghost region in x1 direction
    for(i=grid.nStartGhostUpdateExplicit[grid.nV][1][0];
      i<grid.nEndGhostUpdateExplicit[grid.nV][1][0];i++){//nU0 needs to be 1D
      
      //calculate j of interface quantities
      nIInt=i+grid.nCenIntOffset[0];
      dR_i_n=(grid.dLocalGridOld[grid.nR][nIInt][0][0]+grid.dLocalGridOld[grid.nR][nIInt-1][0][0])
        *0.5;
      dU0i_n=0.5*(grid.dLocalGridOld[grid.nU0][nIInt][0][0]
        +grid.dLocalGridOld[grid.nU0][nIInt-1][0][0]);
      
      for(j=grid.nStartGhostUpdateExplicit[grid.nV][1][1];
        j<grid.nEndGhostUpdateExplicit[grid.nV][1][1];j++){
        
        //calculate j of centered quantities
        nJCen=j-grid.nCenIntOffset[1];
        dDeltaTheta_jp1half=(grid.dLocalGridOld[grid.nDTheta][0][nJCen+1][0]
          +grid.dLocalGridOld[grid.nDTheta][0][nJCen][0])*0.5;
            
        for(k=grid.nStartGhostUpdateExplicit[grid.nV][1][2];
          k<grid.nEndGhostUpdateExplicit[grid.nV][1][2];k++){
          
          //Calculate interpolated quantities
          dU_ijp1halfk_n=0.25*(grid.dLocalGridOld[grid.nU][nIInt][nJCen][k]
            +grid.dLocalGridOld[grid.nU][nIInt][nJCen+1][k]
            +grid.dLocalGridOld[grid.nU][nIInt-1][nJCen][k]
            +grid.dLocalGridOld[grid.nU][nIInt-1][nJCen+1][k]);
          dV_ip1halfjp1halfk_n=0.5*(grid.dLocalGridOld[grid.nV][i+1][j][k]
            +grid.dLocalGridOld[grid.nV][i][j][k]);
          dV_im1halfjp1halfk_n=0.5*(grid.dLocalGridOld[grid.nV][i][j][k]
            +grid.dLocalGridOld[grid.nV][i-1][j][k]);
          dV_ijp1halfk_n=grid.dLocalGridOld[grid.nV][i][j][k];
          dV_ijp1k_n=(grid.dLocalGridOld[grid.nV][i][j+1][k]+grid.dLocalGridOld[grid.nV][i][j][k])
            *0.5;
          dV_ijk_n=(grid.dLocalGridOld[grid.nV][i][j][k]+grid.dLocalGridOld[grid.nV][i][j-1][k])
            *0.5;
          dRho_ijp1halfk_n=(grid.dLocalGridOld[grid.nD][i][nJCen][k]
            +grid.dLocalGridOld[grid.nD][i][nJCen+1][k])*0.5;
          dP_ijp1k_n=grid.dLocalGridOld[grid.nP][i][nJCen+1][k]
            +grid.dLocalGridOld[grid.nQ0][i][nJCen+1][k]
            +grid.dLocalGridOld[grid.nQ1][i][nJCen+1][k];
          dP_ijk_n=grid.dLocalGridOld[grid.nP][i][nJCen][k]
            +grid.dLocalGridOld[grid.nQ0][i][nJCen][k]
            +grid.dLocalGridOld[grid.nQ1][i][nJCen][k];
          
          //calculate derived quantities
          dU_U0_Diff=dU_ijp1halfk_n-dU0i_n;
          
          //calculate A1
          dA1CenGrad=(dV_ip1halfjp1halfk_n-dV_im1halfjp1halfk_n)
            /grid.dLocalGridOld[grid.nDM][i][0][0];
          dA1UpWindGrad=0.0;
          if(dU_U0_Diff<0.0){//moving in a negative direction
            dA1UpWindGrad=(grid.dLocalGridOld[grid.nV][i+1][j][k]
              -grid.dLocalGridOld[grid.nV][i][j][k])/(grid.dLocalGridOld[grid.nDM][i][0][0]
              +grid.dLocalGridOld[grid.nDM][i+1][0][0])*2.0;
          }
          else{//moving in a positive direction
            dA1UpWindGrad=(grid.dLocalGridOld[grid.nV][i][j][k]
              -grid.dLocalGridOld[grid.nV][i-1][j][k])/(grid.dLocalGridOld[grid.nDM][i][0][0]
              +grid.dLocalGridOld[grid.nDM][i-1][0][0])*2.0;
          }
          dA1=4.0*parameters.dPi*dR_i_n*dR_i_n*grid.dLocalGridOld[grid.nDenAve][i][0][0]
            *dU_U0_Diff*((1.0-parameters.dDonorFrac)*dA1CenGrad
            +parameters.dDonorFrac*dA1UpWindGrad);
          
          //calculate S1
          dS1=dU_ijp1halfk_n*dV_ijp1halfk_n/dR_i_n;
          
          //calculate dA2
          dA2CenGrad=(dV_ijp1k_n-dV_ijk_n)/dDeltaTheta_jp1half;
          dA2UpWindGrad=0.0;
          if(dV_ijp1halfk_n<0.0){//moning in a negative direction
            dA2UpWindGrad=(grid.dLocalGridOld[grid.nV][i][j+1][k]
              -grid.dLocalGridOld[grid.nV][i][j][k])
              /grid.dLocalGridOld[grid.nDTheta][0][nJCen+1][0];
          }
          else{//moving in a positive direction
            dA2UpWindGrad=(grid.dLocalGridOld[grid.nV][i][j][k]
              -grid.dLocalGridOld[grid.nV][i][j-1][k])
              /grid.dLocalGridOld[grid.nDTheta][0][nJCen][0];
          }
          dA2=dV_ijp1halfk_n/dR_i_n*((1.0-parameters.dDonorFrac)*dA2CenGrad
            +parameters.dDonorFrac*dA2UpWindGrad);
          
          //calculate S2
          dS2=(dP_ijp1k_n-dP_ijk_n)/dDeltaTheta_jp1half/dRho_ijp1halfk_n
            /dR_i_n;
          
          //calculate new velocity
          grid.dLocalGridNew[grid.nV][i][j][k]=grid.dLocalGridOld[grid.nV][i][j][k]
            -time.dDeltat_n*(dA1+dS1+dA2+dS2);
        }
      }
    }
#endif
}
void calNewV_RT_LES(Grid &grid,Parameters &parameters,Time &time,ProcTop &procTop){
  int i;
  int j;
  int k;
  int nIInt;
  int nJCen;
  double dR_i_n;
  double dR_ip1_n;
  double dR_im1_n;
  double dRCu_ip1half;
  double dRCu_im1half;
  double dU_ijp1halfk_nm1half;
  double dU_ijp1k_nm1half;
  double dU_im1halfjp1halfk_nm1half;
  double dU_im1jp1halfk_nm1half;
  double dU_ijk_nm1half;
  double dU0_i_nm1half;
  double dV_ip1halfjp1halfk_nm1half;
  double dV_ijp1k_nm1half;
  double dV_ijk_nm1half;
  double dV_im1halfjp1halfk_nm1half;
  double dDTheta_jp1half;
  double dRho_ijp1halfk_n;
  double dRhoAve_ip1half_n;
  double dRhoAve_im1half_n;
  double dP_ijp1k_n;
  double dP_ijk_n;
  double dDM_ip1half;
  double dDM_im1half;
  double dEddyVisc_ip1halfjp1halfk_n;
  double dEddyVisc_im1halfjp1halfk_n;
  double dEddyVisc_ijp1halfk_n;
  double dRSq_ip1half_n;
  double dRSq_im1half_n;
  double dRSq_i_n;
  double dRSqUmU0_ip1halfjp1k_n;
  double dRSqUmU0_im1halfjp1k_n;
  double dRSqUmU0_ip1halfjk_n;
  double dRSqUmU0_im1halfjk_n;
  double dV_R_ip1jp1halfk_n;
  double dV_R_ijp1halfk_n;
  double dV_R_im1jp1halfk_n;
  double dV_R_ip1halfjp1halfk_n;
  double dV_R_im1halfjp1halfk_n;
  double dU_U0_Diff_ijp1halfk_nm1half;
  double dA1CenGrad;
  double dA1UpWindGrad;
  double dA1;
  double dS1;
  double dA2CenGrad;
  double dA2UpWindGrad;
  double dA2;
  double dS2;
  double dTau_rt_ip1halfjp1halfk_n;
  double dTau_rt_im1halfjp1halfk_n;
  double dDivU_ijp1k_n;
  double dDivU_ijk_n;
  double dTau_tt_ijp1k_n;
  double dTau_tt_ijk_n;
  double dTA1;
  double dTS1;
  double dTA2;
  double dTS2;
  double dTS4;
  double dEddyViscosityTerms;
  
  //calculate new v
  for(i=grid.nStartUpdateExplicit[grid.nV][0];i<grid.nEndUpdateExplicit[grid.nV][0];i++){
    
    //calculate i of interface quantities
    nIInt=i+grid.nCenIntOffset[0];
    
    //calculate quantities that only vary radially
    dR_i_n=(grid.dLocalGridOld[grid.nR][nIInt][0][0]+grid.dLocalGridOld[grid.nR][nIInt-1][0][0])
      *0.5;
    dR_ip1_n=(grid.dLocalGridOld[grid.nR][nIInt+1][0][0]+grid.dLocalGridOld[grid.nR][nIInt][0][0])
      *0.5;
    dR_im1_n=(grid.dLocalGridOld[grid.nR][nIInt-1][0][0]+grid.dLocalGridOld[grid.nR][nIInt-2][0][0])
      *0.5;
    dRSq_i_n=dR_i_n*dR_i_n;
    dRSq_ip1half_n=grid.dLocalGridOld[grid.nR][nIInt][0][0]
      *grid.dLocalGridOld[grid.nR][nIInt][0][0];
    dRSq_im1half_n=grid.dLocalGridOld[grid.nR][nIInt-1][0][0]
      *grid.dLocalGridOld[grid.nR][nIInt-1][0][0];
    dRCu_ip1half=dRSq_ip1half_n*grid.dLocalGridOld[grid.nR][nIInt][0][0];
    dRCu_im1half=dRSq_im1half_n*grid.dLocalGridOld[grid.nR][nIInt-1][0][0];
    dU0_i_nm1half=(grid.dLocalGridOld[grid.nU0][nIInt][0][0]
      +grid.dLocalGridOld[grid.nU0][nIInt-1][0][0])*0.5;
    dRhoAve_ip1half_n=(grid.dLocalGridOld[grid.nDenAve][i+1][0][0]
      +grid.dLocalGridOld[grid.nDenAve][i][0][0])*0.5;
    dRhoAve_im1half_n=(grid.dLocalGridOld[grid.nDenAve][i][0][0]
      +grid.dLocalGridOld[grid.nDenAve][i-1][0][0])*0.5;
    dDM_ip1half=(grid.dLocalGridOld[grid.nDM][i+1][0][0]+grid.dLocalGridOld[grid.nDM][i][0][0])*0.5;
    dDM_im1half=(grid.dLocalGridOld[grid.nDM][i-1][0][0]+grid.dLocalGridOld[grid.nDM][i][0][0])*0.5;
    
    for(j=grid.nStartUpdateExplicit[grid.nV][1];j<grid.nEndUpdateExplicit[grid.nV][1];j++){
      
      //calculate j of centered quantities
      nJCen=j-grid.nCenIntOffset[1];
      
      //calculate quantities that only vary with theta and or radius
      dDTheta_jp1half=(grid.dLocalGridOld[grid.nDTheta][0][nJCen+1][0]
        +grid.dLocalGridOld[grid.nDTheta][0][nJCen][0])*0.5;
      
      for(k=grid.nStartUpdateExplicit[grid.nV][2];k<grid.nEndUpdateExplicit[grid.nV][2];k++){
        
        //Calculate interpolated quantities
        dU_ijp1halfk_nm1half=0.25*(grid.dLocalGridOld[grid.nU][nIInt][nJCen][k]
          +grid.dLocalGridOld[grid.nU][nIInt][nJCen+1][k]
          +grid.dLocalGridOld[grid.nU][nIInt-1][nJCen][k]
          +grid.dLocalGridOld[grid.nU][nIInt-1][nJCen+1][k]);
        dU_ijp1k_nm1half=(grid.dLocalGridOld[grid.nU][nIInt][nJCen+1][k]
          +grid.dLocalGridOld[grid.nU][nIInt-1][nJCen+1][k])*0.5;
        dU_im1halfjp1halfk_nm1half=(grid.dLocalGridOld[grid.nU][nIInt-1][nJCen+1][k]
          +grid.dLocalGridOld[grid.nU][nIInt-1][nJCen][k])*0.5;
        dU_im1jp1halfk_nm1half=0.25*(grid.dLocalGridOld[grid.nU][nIInt-1][nJCen][k]
          +grid.dLocalGridOld[grid.nU][nIInt-1][nJCen+1][k]
          +grid.dLocalGridOld[grid.nU][nIInt-2][nJCen][k]
          +grid.dLocalGridOld[grid.nU][nIInt-2][nJCen+1][k]);
        dU_ijk_nm1half=(grid.dLocalGridOld[grid.nU][nIInt][nJCen][k]
          +grid.dLocalGridOld[grid.nU][nIInt-1][nJCen][k])*0.5;
        dV_ip1halfjp1halfk_nm1half=0.5*(grid.dLocalGridOld[grid.nV][i+1][j][k]
          +grid.dLocalGridOld[grid.nV][i][j][k]);
        dV_im1halfjp1halfk_nm1half=0.5*(grid.dLocalGridOld[grid.nV][i][j][k]
          +grid.dLocalGridOld[grid.nV][i-1][j][k]);
        dV_ijp1k_nm1half=(grid.dLocalGridOld[grid.nV][i][j+1][k]
          +grid.dLocalGridOld[grid.nV][i][j][k])*0.5;
        dV_ijk_nm1half=(grid.dLocalGridOld[grid.nV][i][j][k]
          +grid.dLocalGridOld[grid.nV][i][j-1][k])*0.5;
        dRho_ijp1halfk_n=(grid.dLocalGridOld[grid.nD][i][nJCen][k]
          +grid.dLocalGridOld[grid.nD][i][nJCen+1][k])*0.5;
        dP_ijp1k_n=grid.dLocalGridOld[grid.nP][i][nJCen+1][k]
          +grid.dLocalGridOld[grid.nQ0][i][nJCen+1][k]+grid.dLocalGridOld[grid.nQ1][i][nJCen+1][k];
        dP_ijk_n=grid.dLocalGridOld[grid.nP][i][nJCen][k]+grid.dLocalGridOld[grid.nQ0][i][nJCen][k]
          +grid.dLocalGridOld[grid.nQ1][i][nJCen][k];
        dEddyVisc_ip1halfjp1halfk_n=(grid.dLocalGridOld[grid.nEddyVisc][i][nJCen][k]
          +grid.dLocalGridOld[grid.nEddyVisc][i+1][nJCen][k]
          +grid.dLocalGridOld[grid.nEddyVisc][i+1][nJCen+1][k]
          +grid.dLocalGridOld[grid.nEddyVisc][i][nJCen+1][k])*0.25;
        dEddyVisc_im1halfjp1halfk_n=(grid.dLocalGridOld[grid.nEddyVisc][i][nJCen][k]
          +grid.dLocalGridOld[grid.nEddyVisc][i-1][nJCen][k]
          +grid.dLocalGridOld[grid.nEddyVisc][i-1][nJCen+1][k]
          +grid.dLocalGridOld[grid.nEddyVisc][i][nJCen+1][k])*0.25;
        dEddyVisc_ijp1halfk_n=(grid.dLocalGridOld[grid.nEddyVisc][i][nJCen][k]
          +grid.dLocalGridOld[grid.nEddyVisc][i][nJCen+1][k])*0.5;
        
        //calculate derived quantities
        dU_U0_Diff_ijp1halfk_nm1half=dU_ijp1halfk_nm1half-dU0_i_nm1half;
        dRSqUmU0_ip1halfjp1k_n=dRSq_ip1half_n*(grid.dLocalGridOld[grid.nU][nIInt][nJCen+1][k]
          -grid.dLocalGridOld[grid.nU0][nIInt][0][0]);
        dRSqUmU0_im1halfjp1k_n=dRSq_im1half_n*(grid.dLocalGridOld[grid.nU][nIInt-1][nJCen+1][k]
          -grid.dLocalGridOld[grid.nU0][nIInt-1][0][0]);
        dRSqUmU0_ip1halfjk_n=dRSq_ip1half_n*(grid.dLocalGridOld[grid.nU][nIInt][nJCen][k]
          -grid.dLocalGridOld[grid.nU0][nIInt][0][0]);
        dRSqUmU0_im1halfjk_n=dRSq_im1half_n*(grid.dLocalGridOld[grid.nU][nIInt-1][nJCen][k]
          -grid.dLocalGridOld[grid.nU0][nIInt-1][0][0]);
        dU_U0_Diff_ijp1halfk_nm1half=dU_ijp1halfk_nm1half-dU0_i_nm1half;
        dV_R_ip1jp1halfk_n=grid.dLocalGridOld[grid.nV][i+1][j][k]/dR_ip1_n;
        dV_R_ijp1halfk_n=grid.dLocalGridOld[grid.nV][i][j][k]/dR_i_n;
        dV_R_im1jp1halfk_n=grid.dLocalGridOld[grid.nV][i-1][j][k]/dR_im1_n;
        dV_R_ip1halfjp1halfk_n=dV_ip1halfjp1halfk_nm1half/grid.dLocalGridOld[grid.nR][nIInt][0][0];
        dV_R_im1halfjp1halfk_n=dV_im1halfjp1halfk_nm1half
          /grid.dLocalGridOld[grid.nR][nIInt-1][0][0];
  
        //calculate A1
        dA1CenGrad=(dV_ip1halfjp1halfk_nm1half-dV_im1halfjp1halfk_nm1half)
          /grid.dLocalGridOld[grid.nDM][i][0][0];
        dA1UpWindGrad=0.0;
        if(dU_U0_Diff_ijp1halfk_nm1half<0.0){//moving in a negative direction
          dA1UpWindGrad=(grid.dLocalGridOld[grid.nV][i+1][j][k]
            -grid.dLocalGridOld[grid.nV][i][j][k])/(grid.dLocalGridOld[grid.nDM][i][0][0]
            +grid.dLocalGridOld[grid.nDM][i+1][0][0])*2.0;
        }
        else{//moving in a positive direction
          dA1UpWindGrad=(grid.dLocalGridOld[grid.nV][i][j][k]
            -grid.dLocalGridOld[grid.nV][i-1][j][k])/(grid.dLocalGridOld[grid.nDM][i][0][0]
            +grid.dLocalGridOld[grid.nDM][i-1][0][0])*2.0;
        }
        dA1=dU_U0_Diff_ijp1halfk_nm1half*((1.0-parameters.dDonorFrac)*dA1CenGrad
          +parameters.dDonorFrac*dA1UpWindGrad);
        
        //calculate S1
        dS1=dU_ijp1halfk_nm1half*grid.dLocalGridOld[grid.nV][i][j][k]/dR_i_n;
        
        //calculate dA2
        dA2CenGrad=(dV_ijp1k_nm1half-dV_ijk_nm1half)/dDTheta_jp1half;
        dA2UpWindGrad=0.0;
        if(grid.dLocalGridOld[grid.nV][i][j][k]<0.0){//moning in a negative direction
          dA2UpWindGrad=(grid.dLocalGridOld[grid.nV][i][j+1][k]
            -grid.dLocalGridOld[grid.nV][i][j][k])/grid.dLocalGridOld[grid.nDTheta][0][nJCen+1][0];
        }
        else{//moving in a positive direction
          dA2UpWindGrad=(grid.dLocalGridOld[grid.nV][i][j][k]
            -grid.dLocalGridOld[grid.nV][i][j-1][k])/grid.dLocalGridOld[grid.nDTheta][0][nJCen][0];
        }
        dA2=grid.dLocalGridOld[grid.nV][i][j][k]/dR_i_n*((1.0-parameters.dDonorFrac)*dA2CenGrad
          +parameters.dDonorFrac*dA2UpWindGrad);
        
        //calculate S2
        dS2=(dP_ijp1k_n-dP_ijk_n)/(dDTheta_jp1half*dRho_ijp1halfk_n*dR_i_n);
        
        //calculate Tau_rt_ip1halfjp1halfk_n
        dTau_rt_ip1halfjp1halfk_n=dEddyVisc_ip1halfjp1halfk_n*(4.0*parameters.dPi*dRCu_ip1half
          *dRhoAve_ip1half_n*(dV_R_ip1jp1halfk_n-dV_R_ijp1halfk_n)/dDM_ip1half+1.0
          /grid.dLocalGridOld[grid.nR][nIInt][0][0]*((grid.dLocalGridOld[grid.nU][nIInt][nJCen+1][k]
          -grid.dLocalGridOld[grid.nU0][nIInt][0][0])-(grid.dLocalGridOld[grid.nU][nIInt][nJCen][k]
          -grid.dLocalGridOld[grid.nU0][nIInt][0][0]))/dDTheta_jp1half);
        
        //calculate Tau_rt_im1halfjp1halfk_n
        dTau_rt_im1halfjp1halfk_n=dEddyVisc_im1halfjp1halfk_n*(4.0*parameters.dPi*dRCu_im1half
          *dRhoAve_im1half_n*(dV_R_ijp1halfk_n-dV_R_im1jp1halfk_n)/dDM_im1half+1.0
          /grid.dLocalGridOld[grid.nR][nIInt-1][0][0]
          *((grid.dLocalGridOld[grid.nU][nIInt-1][nJCen+1][k]
          -grid.dLocalGridOld[grid.nU0][nIInt-1][0][0])
          -(grid.dLocalGridOld[grid.nU][nIInt-1][nJCen][k]
          -grid.dLocalGridOld[grid.nU0][nIInt-1][0][0]))/dDTheta_jp1half);
        
        //calculate DivU_ijp1k_n
        dDivU_ijp1k_n=4.0*parameters.dPi*grid.dLocalGridOld[grid.nDenAve][i][0][0]
          *(dRSqUmU0_ip1halfjp1k_n-dRSqUmU0_im1halfjp1k_n)/grid.dLocalGridOld[grid.nDM][i][0][0]
          +(grid.dLocalGridOld[grid.nV][i][j+1][k]
          *grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][j+1][0]
          -grid.dLocalGridOld[grid.nV][i][j][k]
          *grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][j][0])
          /(grid.dLocalGridOld[grid.nDTheta][0][nJCen+1][0]
          *dR_i_n*grid.dLocalGridOld[grid.nSinThetaIJK][0][nJCen+1][0]);
        
        //calculate DivU_ijk_n
        dDivU_ijk_n=4.0*parameters.dPi*grid.dLocalGridOld[grid.nDenAve][i][0][0]
          *(dRSqUmU0_ip1halfjk_n-dRSqUmU0_im1halfjk_n)/grid.dLocalGridOld[grid.nDM][i][0][0]
          +(grid.dLocalGridOld[grid.nV][i][j][k]
          *grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][j][0]
          -grid.dLocalGridOld[grid.nV][i][j-1][k]
          *grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][j-1][0])
          /(grid.dLocalGridOld[grid.nDTheta][0][nJCen][0]*dR_i_n
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][nJCen][0]);
        
        //calculate Tau_tt_ijp1k_n
        dTau_tt_ijp1k_n=2.0*grid.dLocalGridOld[grid.nEddyVisc][i][nJCen+1][k]
          *((grid.dLocalGridOld[grid.nV][i][j+1][k]-grid.dLocalGridOld[grid.nV][i][j][k])
          /(dR_i_n*grid.dLocalGridOld[grid.nDTheta][0][nJCen+1][0])
          +(dU_ijp1k_nm1half-dU0_i_nm1half)/dR_i_n-0.333333333333333*dDivU_ijp1k_n);
        
        //calculate Tau_tt_ijk_n
        dTau_tt_ijk_n=2.0*grid.dLocalGridOld[grid.nEddyVisc][i][nJCen][k]
          *((grid.dLocalGridOld[grid.nV][i][j][k]-grid.dLocalGridOld[grid.nV][i][j-1][k])
          /(grid.dLocalGridOld[grid.nDTheta][0][nJCen][0]*dR_i_n)
          +(dU_ijk_nm1half-dU0_i_nm1half)/dR_i_n-0.333333333333333*dDivU_ijk_n);
        
        //calculate TA1
        dTA1=(dTau_rt_ip1halfjp1halfk_n-dTau_rt_im1halfjp1halfk_n)
          /(grid.dLocalGridOld[grid.nDM][i][0][0]*dRho_ijp1halfk_n);
        
        //calculate TS1
        dTS1=3.0*dEddyVisc_ijp1halfk_n*(dV_R_ip1halfjp1halfk_n-dV_R_im1halfjp1halfk_n)
          /(grid.dLocalGridOld[grid.nDM][i][0][0]*dRho_ijp1halfk_n);
        
        //calculate TA2
        dTA2=(dTau_tt_ijp1k_n-dTau_tt_ijk_n)/(dRho_ijp1halfk_n*dR_i_n*dDTheta_jp1half);
        
        //calculate TS2
        dTS2=(2.0*grid.dLocalGridOld[grid.nCotThetaIJp1halfK][0][j][0]*(dV_ijp1k_nm1half
          -dV_ijk_nm1half)+3.0*((dU_ijp1k_nm1half-dU0_i_nm1half)
          -(dU_ijk_nm1half-dU0_i_nm1half)))/(dR_i_n*dDTheta_jp1half);
        
        //calculate TS4
        dTS4=2.0*grid.dLocalGridOld[grid.nV][i][j][k]
          *grid.dLocalGridOld[grid.nCotThetaIJp1halfK][0][j][0]
          *grid.dLocalGridOld[grid.nCotThetaIJp1halfK][0][j][0]/dR_i_n;
        
        dEddyViscosityTerms=-4.0*dRSq_i_n*grid.dLocalGridOld[grid.nDenAve][i][0][0]*(dTA1+dTS1)
          -dTA2-dEddyVisc_ijp1halfk_n/(dRho_ijp1halfk_n*dR_i_n)*(dTS2-dTS4);
        
        //calculate new velocity
        grid.dLocalGridNew[grid.nV][i][j][k]=grid.dLocalGridOld[grid.nV][i][j][k]
          -time.dDeltat_n*(4.0*dRSq_i_n*grid.dLocalGridOld[grid.nDenAve][i][0][0]*(dA1)
          +dS1+dA2+dS2+dEddyViscosityTerms);
      }
    }
  }
  
  //ghost region 0, outter most ghost region in x1 direction
  for(i=grid.nStartGhostUpdateExplicit[grid.nV][0][0];
    i<grid.nEndGhostUpdateExplicit[grid.nV][0][0];i++){
    
    //calculate i of interface quantities
    nIInt=i+grid.nCenIntOffset[0];
    
    //calculate quantities that only vary radially
    dR_i_n=(grid.dLocalGridOld[grid.nR][nIInt][0][0]+grid.dLocalGridOld[grid.nR][nIInt-1][0][0])
      *0.5;
    dR_ip1_n=grid.dLocalGridOld[grid.nR][nIInt][0][0];
    dR_im1_n=(grid.dLocalGridOld[grid.nR][nIInt-1][0][0]+grid.dLocalGridOld[grid.nR][nIInt-2][0][0])
      *0.5;
    dRSq_i_n=dR_i_n*dR_i_n;
    dRSq_ip1half_n=grid.dLocalGridOld[grid.nR][nIInt][0][0]
      *grid.dLocalGridOld[grid.nR][nIInt][0][0];
    dRSq_im1half_n=grid.dLocalGridOld[grid.nR][nIInt-1][0][0]
      *grid.dLocalGridOld[grid.nR][nIInt-1][0][0];
    dRCu_ip1half=dRSq_ip1half_n*grid.dLocalGridOld[grid.nR][nIInt][0][0];
    dRCu_im1half=dRSq_im1half_n*grid.dLocalGridOld[grid.nR][nIInt-1][0][0];
    dU0_i_nm1half=(grid.dLocalGridOld[grid.nU0][nIInt][0][0]
      +grid.dLocalGridOld[grid.nU0][nIInt-1][0][0])*0.5;
    dRhoAve_ip1half_n=grid.dLocalGridOld[grid.nDenAve][i][0][0]*0.5;/**\BC Assuming density outside
      star is zero*/
    dRhoAve_im1half_n=(grid.dLocalGridOld[grid.nDenAve][i][0][0]
      +grid.dLocalGridOld[grid.nDenAve][i-1][0][0])*0.5;
    dDM_ip1half=grid.dLocalGridOld[grid.nDM][i][0][0]*0.5;
    dDM_im1half=(grid.dLocalGridOld[grid.nDM][i-1][0][0]+grid.dLocalGridOld[grid.nDM][i][0][0])*0.5;
    
    for(j=grid.nStartGhostUpdateExplicit[grid.nV][0][1];
      j<grid.nEndGhostUpdateExplicit[grid.nV][0][1];j++){
      
      //calculate j of centered quantities
      nJCen=j-grid.nCenIntOffset[1];
      
      //calculate quantities that only vary with theta and or radius
      dDTheta_jp1half=(grid.dLocalGridOld[grid.nDTheta][0][nJCen+1][0]
        +grid.dLocalGridOld[grid.nDTheta][0][nJCen][0])*0.5;
      
      for(k=grid.nStartGhostUpdateExplicit[grid.nV][0][2];
        k<grid.nEndGhostUpdateExplicit[grid.nV][0][2];k++){
        
        //Calculate interpolated quantities
        dU_ijp1halfk_nm1half=0.25*(grid.dLocalGridOld[grid.nU][nIInt][nJCen][k]
          +grid.dLocalGridOld[grid.nU][nIInt][nJCen+1][k]
          +grid.dLocalGridOld[grid.nU][nIInt-1][nJCen][k]
          +grid.dLocalGridOld[grid.nU][nIInt-1][nJCen+1][k]);
        dU_ijp1k_nm1half=(grid.dLocalGridOld[grid.nU][nIInt][nJCen+1][k]
          +grid.dLocalGridOld[grid.nU][nIInt-1][nJCen+1][k])*0.5;
        dU_im1halfjp1halfk_nm1half=(grid.dLocalGridOld[grid.nU][nIInt-1][nJCen+1][k]
          +grid.dLocalGridOld[grid.nU][nIInt-1][nJCen][k])*0.5;
        dU_im1jp1halfk_nm1half=0.25*(grid.dLocalGridOld[grid.nU][nIInt-1][nJCen][k]
          +grid.dLocalGridOld[grid.nU][nIInt-1][nJCen+1][k]
          +grid.dLocalGridOld[grid.nU][nIInt-2][nJCen][k]
          +grid.dLocalGridOld[grid.nU][nIInt-2][nJCen+1][k]);
        dU_ijk_nm1half=(grid.dLocalGridOld[grid.nU][nIInt][nJCen][k]
          +grid.dLocalGridOld[grid.nU][nIInt-1][nJCen][k])*0.5;
        dV_ip1halfjp1halfk_nm1half=grid.dLocalGridOld[grid.nV][i][j][k];/**\BC Assuming theta 
          velocity is constant across surface.*/
        dV_im1halfjp1halfk_nm1half=0.5*(grid.dLocalGridOld[grid.nV][i][j][k]
          +grid.dLocalGridOld[grid.nV][i-1][j][k]);
        dV_ijp1k_nm1half=(grid.dLocalGridOld[grid.nV][i][j+1][k]
          +grid.dLocalGridOld[grid.nV][i][j][k])*0.5;
        dV_ijk_nm1half=(grid.dLocalGridOld[grid.nV][i][j][k]
          +grid.dLocalGridOld[grid.nV][i][j-1][k])*0.5;
        dRho_ijp1halfk_n=(grid.dLocalGridOld[grid.nD][i][nJCen][k]
          +grid.dLocalGridOld[grid.nD][i][nJCen+1][k])*0.5;
        dP_ijp1k_n=grid.dLocalGridOld[grid.nP][i][nJCen+1][k]
          +grid.dLocalGridOld[grid.nQ0][i][nJCen+1][k]+grid.dLocalGridOld[grid.nQ1][i][nJCen+1][k];
        dP_ijk_n=grid.dLocalGridOld[grid.nP][i][nJCen][k]+grid.dLocalGridOld[grid.nQ0][i][nJCen][k]
          +grid.dLocalGridOld[grid.nQ1][i][nJCen][k];
        dEddyVisc_ip1halfjp1halfk_n=(grid.dLocalGridOld[grid.nEddyVisc][i][nJCen][k]
          +grid.dLocalGridOld[grid.nEddyVisc][i][nJCen+1][k])*0.25;/**\BC Assuming eddy viscosity is
          zero at surface.*/
        dEddyVisc_im1halfjp1halfk_n=(grid.dLocalGridOld[grid.nEddyVisc][i][nJCen][k]
          +grid.dLocalGridOld[grid.nEddyVisc][i-1][nJCen][k]
          +grid.dLocalGridOld[grid.nEddyVisc][i-1][nJCen+1][k]
          +grid.dLocalGridOld[grid.nEddyVisc][i][nJCen+1][k])*0.25;
        dEddyVisc_ijp1halfk_n=(grid.dLocalGridOld[grid.nEddyVisc][i][nJCen][k]
          +grid.dLocalGridOld[grid.nEddyVisc][i][nJCen+1][k])*0.5;
        
        //calculate derived quantities
        dU_U0_Diff_ijp1halfk_nm1half=dU_ijp1halfk_nm1half-dU0_i_nm1half;
        dRSqUmU0_ip1halfjp1k_n=dRSq_ip1half_n*(grid.dLocalGridOld[grid.nU][nIInt][nJCen+1][k]
          -grid.dLocalGridOld[grid.nU0][nIInt][0][0]);
        dRSqUmU0_im1halfjp1k_n=dRSq_im1half_n*(grid.dLocalGridOld[grid.nU][nIInt-1][nJCen+1][k]
          -grid.dLocalGridOld[grid.nU0][nIInt-1][0][0]);
        dRSqUmU0_ip1halfjk_n=dRSq_ip1half_n*(grid.dLocalGridOld[grid.nU][nIInt][nJCen][k]
          -grid.dLocalGridOld[grid.nU0][nIInt][0][0]);
        dRSqUmU0_im1halfjk_n=dRSq_im1half_n*(grid.dLocalGridOld[grid.nU][nIInt-1][nJCen][k]
          -grid.dLocalGridOld[grid.nU0][nIInt-1][0][0]);
        dU_U0_Diff_ijp1halfk_nm1half=dU_ijp1halfk_nm1half-dU0_i_nm1half;
        dV_R_ip1jp1halfk_n=grid.dLocalGridOld[grid.nV][i][j][k]/dR_ip1_n;
        dV_R_ijp1halfk_n=grid.dLocalGridOld[grid.nV][i][j][k]/dR_i_n;
        dV_R_im1jp1halfk_n=grid.dLocalGridOld[grid.nV][i-1][j][k]/dR_im1_n;
        dV_R_ip1halfjp1halfk_n=dV_ip1halfjp1halfk_nm1half/grid.dLocalGridOld[grid.nR][nIInt][0][0];
        dV_R_im1halfjp1halfk_n=dV_im1halfjp1halfk_nm1half
          /grid.dLocalGridOld[grid.nR][nIInt-1][0][0];
        
        //calculate A1
        dA1CenGrad=(dV_ip1halfjp1halfk_nm1half-dV_im1halfjp1halfk_nm1half)
          /grid.dLocalGridOld[grid.nDM][i][0][0];
        dA1UpWindGrad=0.0;
        if(dU_U0_Diff_ijp1halfk_nm1half<0.0){//moving in a negative direction
          dA1UpWindGrad=dA1CenGrad;
        }
        else{//moving in a positive direction
          dA1UpWindGrad=(grid.dLocalGridOld[grid.nV][i][j][k]
            -grid.dLocalGridOld[grid.nV][i-1][j][k])/(grid.dLocalGridOld[grid.nDM][i][0][0]
            +grid.dLocalGridOld[grid.nDM][i-1][0][0])*2.0;
        }
        dA1=dU_U0_Diff_ijp1halfk_nm1half*((1.0-parameters.dDonorFrac)*dA1CenGrad
          +parameters.dDonorFrac*dA1UpWindGrad);
        
        //calculate S1
        dS1=dU_ijp1halfk_nm1half*grid.dLocalGridOld[grid.nV][i][j][k]/dR_i_n;
        
        //calculate dA2
        dA2CenGrad=(dV_ijp1k_nm1half-dV_ijk_nm1half)/dDTheta_jp1half;
        dA2UpWindGrad=0.0;
        if(grid.dLocalGridOld[grid.nV][i][j][k]<0.0){//moving in a negative direction
          dA2UpWindGrad=(grid.dLocalGridOld[grid.nV][i][j+1][k]
            -grid.dLocalGridOld[grid.nV][i][j][k])/grid.dLocalGridOld[grid.nDTheta][0][nJCen+1][0];
        }
        else{//moving in a positive direction
          dA2UpWindGrad=(grid.dLocalGridOld[grid.nV][i][j][k]
            -grid.dLocalGridOld[grid.nV][i][j-1][k])/grid.dLocalGridOld[grid.nDTheta][0][nJCen][0];
        }
        dA2=grid.dLocalGridOld[grid.nV][i][j][k]/dR_i_n*((1.0-parameters.dDonorFrac)*dA2CenGrad
          +parameters.dDonorFrac*dA2UpWindGrad);
        
        //calculate S2
        dS2=(dP_ijp1k_n-dP_ijk_n)/(dDTheta_jp1half*dRho_ijp1halfk_n*dR_i_n);
        
        //calculate Tau_rt_ip1halfjp1halfk_n
        dTau_rt_ip1halfjp1halfk_n=dEddyVisc_ip1halfjp1halfk_n*(4.0*parameters.dPi*dRCu_ip1half
          *dRhoAve_ip1half_n*(dV_R_ip1jp1halfk_n-dV_R_ijp1halfk_n)/dDM_ip1half+1.0
          /grid.dLocalGridOld[grid.nR][nIInt][0][0]*((grid.dLocalGridOld[grid.nU][nIInt][nJCen+1][k]
          -grid.dLocalGridOld[grid.nU0][nIInt][0][0])-(grid.dLocalGridOld[grid.nU][nIInt][nJCen][k]
          -grid.dLocalGridOld[grid.nU0][nIInt][0][0]))/dDTheta_jp1half);
        
        //calculate Tau_rt_im1halfjp1halfk_n
        dTau_rt_im1halfjp1halfk_n=dEddyVisc_im1halfjp1halfk_n*(4.0*parameters.dPi*dRCu_im1half
          *dRhoAve_im1half_n*(dV_R_ijp1halfk_n-dV_R_im1jp1halfk_n)/dDM_im1half+1.0
          /grid.dLocalGridOld[grid.nR][nIInt-1][0][0]
          *((grid.dLocalGridOld[grid.nU][nIInt-1][nJCen+1][k]
          -grid.dLocalGridOld[grid.nU0][nIInt-1][0][0])
          -(grid.dLocalGridOld[grid.nU][nIInt-1][nJCen][k]
          -grid.dLocalGridOld[grid.nU0][nIInt-1][0][0]))/dDTheta_jp1half);
        
        //calculate DivU_ijp1k_n
        dDivU_ijp1k_n=4.0*parameters.dPi*grid.dLocalGridOld[grid.nDenAve][i][0][0]
          *(dRSqUmU0_ip1halfjp1k_n-dRSqUmU0_im1halfjp1k_n)/grid.dLocalGridOld[grid.nDM][i][0][0]
          +(grid.dLocalGridOld[grid.nV][i][j+1][k]
          *grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][j+1][0]
          -grid.dLocalGridOld[grid.nV][i][j][k]
          *grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][j][0])
          /(dR_i_n*grid.dLocalGridOld[grid.nSinThetaIJK][0][nJCen+1][0]
          *grid.dLocalGridOld[grid.nDTheta][0][nJCen+1][0]);
        
        //calculate DivU_ijk_n
        dDivU_ijk_n=4.0*parameters.dPi*grid.dLocalGridOld[grid.nDenAve][i][0][0]
          *(dRSqUmU0_ip1halfjk_n-dRSqUmU0_im1halfjk_n)/grid.dLocalGridOld[grid.nDM][i][0][0]
          +(grid.dLocalGridOld[grid.nV][i][j][k]
          *grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][j][0]
          -grid.dLocalGridOld[grid.nV][i][j-1][k]
          *grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][j-1][0])
          /(dR_i_n*grid.dLocalGridOld[grid.nSinThetaIJK][0][nJCen][0]
          *grid.dLocalGridOld[grid.nDTheta][0][nJCen][0]);
        
        //calculate Tau_tt_ijp1k_n
        dTau_tt_ijp1k_n=2.0*grid.dLocalGridOld[grid.nEddyVisc][i][nJCen+1][k]
          *((grid.dLocalGridOld[grid.nV][i][j+1][k]-grid.dLocalGridOld[grid.nV][i][j][k])
          /(dR_i_n*grid.dLocalGridOld[grid.nDTheta][0][nJCen+1][0])
          +(dU_ijp1k_nm1half-dU0_i_nm1half)/dR_i_n-0.333333333333333*dDivU_ijp1k_n);
        
        //calculate Tau_tt_ijk_n
        dTau_tt_ijk_n=2.0*grid.dLocalGridOld[grid.nEddyVisc][i][nJCen][k]
          *((grid.dLocalGridOld[grid.nV][i][j][k]-grid.dLocalGridOld[grid.nV][i][j-1][k])
          /(grid.dLocalGridOld[grid.nDTheta][0][nJCen][0]*dR_i_n)
          +(dU_ijk_nm1half-dU0_i_nm1half)/dR_i_n-0.333333333333333*dDivU_ijk_n);
        
        //calculate TA1
        dTA1=(dTau_rt_ip1halfjp1halfk_n-dTau_rt_im1halfjp1halfk_n)
          /(grid.dLocalGridOld[grid.nDM][i][0][0]*dRho_ijp1halfk_n);
        
        //calculate TS1
        dTS1=3.0*dEddyVisc_ijp1halfk_n*(dV_R_ip1halfjp1halfk_n-dV_R_im1halfjp1halfk_n)
          /(grid.dLocalGridOld[grid.nDM][i][0][0]*dRho_ijp1halfk_n);
        
        //calculate TA2
        dTA2=(dTau_tt_ijp1k_n-dTau_tt_ijk_n)/(dRho_ijp1halfk_n*dR_i_n*dDTheta_jp1half);
        
        //calculate TS2
        dTS2=(2.0*grid.dLocalGridOld[grid.nCotThetaIJp1halfK][0][j][0]*(dV_ijp1k_nm1half
          -dV_ijk_nm1half)+3.0*((dU_ijp1k_nm1half-dU0_i_nm1half)
          -(dU_ijk_nm1half-dU0_i_nm1half)))/(dR_i_n*dDTheta_jp1half);
        
        //calculate TS4
        dTS4=2.0*grid.dLocalGridOld[grid.nV][i][j][k]
          *grid.dLocalGridOld[grid.nCotThetaIJp1halfK][0][j][0]
          *grid.dLocalGridOld[grid.nCotThetaIJp1halfK][0][j][0]/dR_i_n;
        
        dEddyViscosityTerms=-4.0*dRSq_i_n*grid.dLocalGridOld[grid.nDenAve][i][0][0]*(dTA1+dTS1)
          -dTA2-dEddyVisc_ijp1halfk_n/(dRho_ijp1halfk_n*dR_i_n)*(dTS2-dTS4);
        
        //calculate new velocity
        grid.dLocalGridNew[grid.nV][i][j][k]=grid.dLocalGridOld[grid.nV][i][j][k]
          -time.dDeltat_n*(4.0*dRSq_i_n*grid.dLocalGridOld[grid.nDenAve][i][0][0]*(dA1)
          +dS1+dA2+dS2+dEddyViscosityTerms);
      }
    }
  }
}
void calNewV_RTP(Grid &grid,Parameters &parameters,Time &time,ProcTop &procTop){
  
  int i;
  int j;
  int k;
  int nIInt;
  int nJCen;
  int nKInt;
  double dR_i_n;
  double dU_ijp1halfk_nm1half;
  double dU0i_nm1half;
  double dV_ip1halfjp1halfk_nm1half;
  double dV_im1halfjp1halfk_nm1half;
  double dV_ijp1halfk_nm1half;
  double dV_ijp1k_nm1half;
  double dV_ijk_nm1half;
  double dDeltaTheta_jp1half;
  double dRho_ijp1halfk_n;
  double dV_ijp1halfkm1half_nm1half;
  double dV_ijp1halfkp1half_nm1half;
  double dW_ijp1halfk_nm1half;
  double dP_ijp1k_n;
  double dP_ijk_n;
  double dA1CenGrad;
  double dA1UpWindGrad;
  double dU_U0_Diff;
  double dA1;
  double dS1;
  double dA2CenGrad;
  double dA2UpWindGrad;
  double dA2;
  double dS2;
  double dA3CenGrad;
  double dA3UpWindGrad;
  double dA3;
  double dS3;
  
  //calculate new v
  for(i=grid.nStartUpdateExplicit[grid.nV][0];i<grid.nEndUpdateExplicit[grid.nV][0];i++){
    
    //calculate j of interface quantities
    nIInt=i+grid.nCenIntOffset[0];
    dR_i_n=(grid.dLocalGridOld[grid.nR][nIInt][0][0]+grid.dLocalGridOld[grid.nR][nIInt-1][0][0])
      *0.5;
    dU0i_nm1half=0.5*(grid.dLocalGridOld[grid.nU0][nIInt][0][0]
      +grid.dLocalGridOld[grid.nU0][nIInt-1][0][0]);
    
    for(j=grid.nStartUpdateExplicit[grid.nV][1];j<grid.nEndUpdateExplicit[grid.nV][1];j++){
      
      //calculate j of centered quantities
      nJCen=j-grid.nCenIntOffset[1];
      dDeltaTheta_jp1half=(grid.dLocalGridOld[grid.nDTheta][0][nJCen+1][0]
        +grid.dLocalGridOld[grid.nDTheta][0][nJCen][0])*0.5;
      
      for(k=grid.nStartUpdateExplicit[grid.nV][2];k<grid.nEndUpdateExplicit[grid.nV][2];k++){
        
        //calculate k of interface quantities
        nKInt=k+grid.nCenIntOffset[2];
        
        //Calculate interpolated quantities
        dU_ijp1halfk_nm1half=0.25*(grid.dLocalGridOld[grid.nU][nIInt][nJCen][k]
          +grid.dLocalGridOld[grid.nU][nIInt][nJCen+1][k]
          +grid.dLocalGridOld[grid.nU][nIInt-1][nJCen][k]
          +grid.dLocalGridOld[grid.nU][nIInt-1][nJCen+1][k]);
        dV_ip1halfjp1halfk_nm1half=0.5*(grid.dLocalGridOld[grid.nV][i+1][j][k]
          +grid.dLocalGridOld[grid.nV][i][j][k]);
        dV_im1halfjp1halfk_nm1half=0.5*(grid.dLocalGridOld[grid.nV][i][j][k]
          +grid.dLocalGridOld[grid.nV][i-1][j][k]);
        dV_ijp1halfk_nm1half=grid.dLocalGridOld[grid.nV][i][j][k];
        dV_ijp1k_nm1half=(grid.dLocalGridOld[grid.nV][i][j+1][k]
          +grid.dLocalGridOld[grid.nV][i][j][k])*0.5;
        dV_ijk_nm1half=(grid.dLocalGridOld[grid.nV][i][j][k]+grid.dLocalGridOld[grid.nV][i][j-1][k])
          *0.5;
        dRho_ijp1halfk_n=(grid.dLocalGridOld[grid.nD][i][nJCen][k]
          +grid.dLocalGridOld[grid.nD][i][nJCen+1][k])*0.5;
        dV_ijp1halfkp1half_nm1half=(grid.dLocalGridOld[grid.nV][i][j][k+1]
          +grid.dLocalGridOld[grid.nV][i][j][k])*0.5;
        dV_ijp1halfkm1half_nm1half=(grid.dLocalGridOld[grid.nV][i][j][k]
          +grid.dLocalGridOld[grid.nV][i][j][k-1])*0.5;
        dW_ijp1halfk_nm1half=0.25*(grid.dLocalGridOld[grid.nW][i][nJCen+1][nKInt]
          +grid.dLocalGridOld[grid.nW][i][nJCen+1][nKInt-1]
          +grid.dLocalGridOld[grid.nW][i][nJCen][nKInt]
          +grid.dLocalGridOld[grid.nW][i][nJCen][nKInt-1]);
        dP_ijp1k_n=grid.dLocalGridOld[grid.nP][i][nJCen+1][k]
          +grid.dLocalGridOld[grid.nQ0][i][nJCen+1][k]+grid.dLocalGridOld[grid.nQ1][i][nJCen+1][k]
          +grid.dLocalGridOld[grid.nQ2][i][nJCen+1][k];
        dP_ijk_n=grid.dLocalGridOld[grid.nP][i][nJCen][k]+grid.dLocalGridOld[grid.nQ0][i][nJCen][k]
          +grid.dLocalGridOld[grid.nQ1][i][nJCen][k]+grid.dLocalGridOld[grid.nQ2][i][nJCen][k];
        
        //calculate dreived quantities
        dU_U0_Diff=dU_ijp1halfk_nm1half-dU0i_nm1half;
        
        //calculate A1
        dA1CenGrad=(dV_ip1halfjp1halfk_nm1half-dV_im1halfjp1halfk_nm1half)
          /grid.dLocalGridOld[grid.nDM][i][0][0];
        dA1UpWindGrad=0.0;
        if(dU_U0_Diff<0.0){//moving in a negative direction
          dA1UpWindGrad=(grid.dLocalGridOld[grid.nV][i+1][j][k]
            -grid.dLocalGridOld[grid.nV][i][j][k])/(grid.dLocalGridOld[grid.nDM][i][0][0]
            +grid.dLocalGridOld[grid.nDM][i+1][0][0])*2.0;
        }
        else{//moving in a positive direction
          dA1UpWindGrad=(grid.dLocalGridOld[grid.nV][i][j][k]
            -grid.dLocalGridOld[grid.nV][i-1][j][k])/(grid.dLocalGridOld[grid.nDM][i][0][0]
            +grid.dLocalGridOld[grid.nDM][i-1][0][0])*2.0;
        }
        dA1=4.0*parameters.dPi*dR_i_n*dR_i_n*grid.dLocalGridOld[grid.nDenAve][i][0][0]
          *dU_U0_Diff*((1.0-parameters.dDonorFrac)*dA1CenGrad+parameters.dDonorFrac*dA1UpWindGrad);
        
        //calculate S1
        dS1=dU_ijp1halfk_nm1half*dV_ijp1halfk_nm1half/dR_i_n;
        
        //calculate dA2
        dA2CenGrad=(dV_ijp1k_nm1half-dV_ijk_nm1half)/dDeltaTheta_jp1half;
        dA2UpWindGrad=0.0;
        if(dV_ijp1halfk_nm1half<0.0){//moning in a negative direction
          dA2UpWindGrad=(grid.dLocalGridOld[grid.nV][i][j+1][k]
            -grid.dLocalGridOld[grid.nV][i][j][k])/grid.dLocalGridOld[grid.nDTheta][0][nJCen+1][0];
        }
        else{//moving in a positive direction
          dA2UpWindGrad=(grid.dLocalGridOld[grid.nV][i][j][k]
            -grid.dLocalGridOld[grid.nV][i][j-1][k])/grid.dLocalGridOld[grid.nDTheta][0][nJCen][0];
        }
        dA2=dV_ijp1halfk_nm1half/dR_i_n*((1.0-parameters.dDonorFrac)*dA2CenGrad
          +parameters.dDonorFrac*dA2UpWindGrad);
        
        //calculate S2
        dS2=(dP_ijp1k_n-dP_ijk_n)/(dDeltaTheta_jp1half*dRho_ijp1halfk_n*dR_i_n);
        
        //calculate A3
        dA3CenGrad=(dV_ijp1halfkp1half_nm1half-dV_ijp1halfkm1half_nm1half)
          /grid.dLocalGridOld[grid.nDPhi][0][0][k];
        dA3UpWindGrad=0.0;
        if(dW_ijp1halfk_nm1half<0.0){//moving in a negative direction
          dA3UpWindGrad=(grid.dLocalGridOld[grid.nV][i][j][k+1]
            -grid.dLocalGridOld[grid.nV][i][j][k])/(grid.dLocalGridOld[grid.nDPhi][0][0][k+1]
            +grid.dLocalGridOld[grid.nDPhi][0][0][k])*2.0;
        }
        else{//moving in a positive direction
          dA3UpWindGrad=(grid.dLocalGridOld[grid.nV][i][j][k]
            -grid.dLocalGridOld[grid.nV][i][j][k-1])/(grid.dLocalGridOld[grid.nDPhi][0][0][k]
            +grid.dLocalGridOld[grid.nDPhi][0][0][k-1])*2.0;
        }
        dA3=dW_ijp1halfk_nm1half*((1.0-parameters.dDonorFrac)*dA3CenGrad+parameters.dDonorFrac
          *dA3UpWindGrad)/(dR_i_n*grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][j][0]);
        
        //calculate S3
        dS3=-1.0*dW_ijp1halfk_nm1half*dW_ijp1halfk_nm1half
          *grid.dLocalGridOld[grid.nCotThetaIJp1halfK][0][j][0]/dR_i_n;
        
        //calculate new velocity
        grid.dLocalGridNew[grid.nV][i][j][k]=grid.dLocalGridOld[grid.nV][i][j][k]-time.dDeltat_n
          *(dA1+dS1+dA2+dS2+dA3+dS3);
      }
    }
  }
  
  //ghost region 0, outter most ghost region in x1 direction
  for(i=grid.nStartGhostUpdateExplicit[grid.nV][0][0];i<grid.nEndGhostUpdateExplicit[grid.nV][0][0];
    i++){
    
    //calculate j of interface quantities
    nIInt=i+grid.nCenIntOffset[0];
    dR_i_n=(grid.dLocalGridOld[grid.nR][nIInt][0][0]+grid.dLocalGridOld[grid.nR][nIInt-1][0][0])
      *0.5;
    dU0i_nm1half=0.5*(grid.dLocalGridOld[grid.nU0][nIInt][0][0]
      +grid.dLocalGridOld[grid.nU0][nIInt-1][0][0]);
          
    for(j=grid.nStartGhostUpdateExplicit[grid.nV][0][1];
      j<grid.nEndGhostUpdateExplicit[grid.nV][0][1];j++){
      
      //calculate j of centered quantities
      nJCen=j-grid.nCenIntOffset[1];
      dDeltaTheta_jp1half=(grid.dLocalGridOld[grid.nDTheta][0][nJCen+1][0]
        +grid.dLocalGridOld[grid.nDTheta][0][nJCen][0])*0.5;
        
      for(k=grid.nStartGhostUpdateExplicit[grid.nV][0][2];
        k<grid.nEndGhostUpdateExplicit[grid.nV][0][2];k++){
        
        //calculate k of interface quantities
        nKInt=k+grid.nCenIntOffset[2];
        
        //Calculate interpolated quantities
        dU_ijp1halfk_nm1half=0.25*(grid.dLocalGridOld[grid.nU][nIInt][nJCen][k]
          +grid.dLocalGridOld[grid.nU][nIInt][nJCen+1][k]
          +grid.dLocalGridOld[grid.nU][nIInt-1][nJCen][k]
          +grid.dLocalGridOld[grid.nU][nIInt-1][nJCen+1][k]);
        dV_ip1halfjp1halfk_nm1half=grid.dLocalGridOld[grid.nV][i][j][k];/**\BC Assuming theta
          and phi velocities are the same at the surface of the star as just inside the star.*/
        dV_im1halfjp1halfk_nm1half=0.5*(grid.dLocalGridOld[grid.nV][i][j][k]
          +grid.dLocalGridOld[grid.nV][i-1][j][k]);
        dV_ijp1halfk_nm1half=grid.dLocalGridOld[grid.nV][i][j][k];
        dV_ijp1k_nm1half=(grid.dLocalGridOld[grid.nV][i][j+1][k]
          +grid.dLocalGridOld[grid.nV][i][j][k])*0.5;
        dV_ijk_nm1half=(grid.dLocalGridOld[grid.nV][i][j][k]+grid.dLocalGridOld[grid.nV][i][j-1][k])
          *0.5;
        dRho_ijp1halfk_n=(grid.dLocalGridOld[grid.nD][i][nJCen][k]
          +grid.dLocalGridOld[grid.nD][i][nJCen+1][k])*0.5;
        dP_ijp1k_n=grid.dLocalGridOld[grid.nP][i][nJCen+1][k]
          +grid.dLocalGridOld[grid.nQ0][i][nJCen+1][k]+grid.dLocalGridOld[grid.nQ1][i][nJCen+1][k]
          +grid.dLocalGridOld[grid.nQ2][i][nJCen+1][k];
        dP_ijk_n=grid.dLocalGridOld[grid.nP][i][nJCen][k]+grid.dLocalGridOld[grid.nQ0][i][nJCen][k]
          +grid.dLocalGridOld[grid.nQ1][i][nJCen][k]+grid.dLocalGridOld[grid.nQ2][i][nJCen][k];
        dV_ijp1halfkp1half_nm1half=(grid.dLocalGridOld[grid.nV][i][j][k+1]
          +grid.dLocalGridOld[grid.nV][i][j][k])*0.5;
        dV_ijp1halfkm1half_nm1half=(grid.dLocalGridOld[grid.nV][i][j][k]
          +grid.dLocalGridOld[grid.nV][i][j][k-1])*0.5;
        dW_ijp1halfk_nm1half=0.25*(grid.dLocalGridOld[grid.nW][i][nJCen+1][nKInt]
          +grid.dLocalGridOld[grid.nW][i][nJCen+1][nKInt-1]
          +grid.dLocalGridOld[grid.nW][i][nJCen][nKInt]
          +grid.dLocalGridOld[grid.nW][i][nJCen][nKInt-1]);
        
        //calculate derived quantities
        dU_U0_Diff=dU_ijp1halfk_nm1half-dU0i_nm1half;
        
        //calculate A1
        dA1CenGrad=(dV_ip1halfjp1halfk_nm1half-dV_im1halfjp1halfk_nm1half)
          /grid.dLocalGridOld[grid.nDM][i][0][0];
        dA1UpWindGrad=0.0;
        if(dU_U0_Diff<0.0){//moving in a negative direction
          dA1UpWindGrad=dA1CenGrad;/**\BC ussing cetnered gradient for upwind gradient outside star
            at surface.*/
        }
        else{//moving in a positive direction*/
          dA1UpWindGrad=(grid.dLocalGridOld[grid.nV][i][j][k]
            -grid.dLocalGridOld[grid.nV][i-1][j][k])/(grid.dLocalGridOld[grid.nDM][i][0][0]
            +grid.dLocalGridOld[grid.nDM][i-1][0][0])*2.0;
        }
        dA1=4.0*parameters.dPi*dR_i_n*dR_i_n*grid.dLocalGridOld[grid.nDenAve][i][0][0]
          *dU_U0_Diff*((1.0-parameters.dDonorFrac)*dA1CenGrad+parameters.dDonorFrac*dA1UpWindGrad);
        
        //calculate S1
        dS1=dU_ijp1halfk_nm1half*dV_ijp1halfk_nm1half/dR_i_n;
        
        //calculate dA2
        dA2CenGrad=(dV_ijp1k_nm1half-dV_ijk_nm1half)/dDeltaTheta_jp1half;
        dA2UpWindGrad=0.0;
        if(dV_ijp1halfk_nm1half<0.0){//moning in a negative direction
          dA2UpWindGrad=(grid.dLocalGridOld[grid.nV][i][j+1][k]
            -grid.dLocalGridOld[grid.nV][i][j][k])/grid.dLocalGridOld[grid.nDTheta][0][nJCen+1][0];
        }
        else{//moving in a positive direction
          dA2UpWindGrad=(grid.dLocalGridOld[grid.nV][i][j][k]
            -grid.dLocalGridOld[grid.nV][i][j-1][k])/grid.dLocalGridOld[grid.nDTheta][0][nJCen][0];
        }
        dA2=dV_ijp1halfk_nm1half/dR_i_n*((1.0-parameters.dDonorFrac)*dA2CenGrad
          +parameters.dDonorFrac*dA2UpWindGrad);
        
        //calculate S2
        dS2=(dP_ijp1k_n-dP_ijk_n)/dDeltaTheta_jp1half/dRho_ijp1halfk_n/dR_i_n;
        
        //calculate A3
        dA3CenGrad=(dV_ijp1halfkp1half_nm1half-dV_ijp1halfkm1half_nm1half)
          /grid.dLocalGridOld[grid.nDPhi][0][0][k];
        dA3UpWindGrad=0.0;
        if(dW_ijp1halfk_nm1half<0.0){//moving in a negative direction
          dA3UpWindGrad=(grid.dLocalGridOld[grid.nV][i][j][k+1]
            -grid.dLocalGridOld[grid.nV][i][j][k])/(grid.dLocalGridOld[grid.nDPhi][0][0][k+1]
            +grid.dLocalGridOld[grid.nDPhi][0][0][k])*2.0;
        }
        else{//moving in a positive direction
          dA3UpWindGrad=(grid.dLocalGridOld[grid.nV][i][j][k]
            -grid.dLocalGridOld[grid.nV][i][j][k-1])/(grid.dLocalGridOld[grid.nDPhi][0][0][k]
            +grid.dLocalGridOld[grid.nDPhi][0][0][k-1])*2.0;
        }
        dA3=dW_ijp1halfk_nm1half*((1.0-parameters.dDonorFrac)*dA3CenGrad
          +parameters.dDonorFrac*dA3UpWindGrad)/(dR_i_n
          *grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][j][0]);
        
        //calculate S3
        dS3=-1.0*dW_ijp1halfk_nm1half*dW_ijp1halfk_nm1half
          *grid.dLocalGridOld[grid.nCotThetaIJp1halfK][0][j][0]/dR_i_n;
        
        //calculate new velocity
        grid.dLocalGridNew[grid.nV][i][j][k]=grid.dLocalGridOld[grid.nV][i][j][k]
          -time.dDeltat_n*(dA1+dS1+dA2+dS2+dA3+dS3);
      }
    }
  }
  
  #if SEDOV==1
    //ghost region 1, innner most ghost region in x1 direction
    for(i=grid.nStartGhostUpdateExplicit[grid.nV][1][0];
      i<grid.nEndGhostUpdateExplicit[grid.nV][1][0];i++){
      
      //calculate j of interface quantities
      nIInt=i+grid.nCenIntOffset[0];
      dR_i_n=(grid.dLocalGridOld[grid.nR][nIInt][0][0]+grid.dLocalGridOld[grid.nR][nIInt-1][0][0])
        *0.5;
          
      for(j=grid.nStartGhostUpdateExplicit[grid.nV][1][1];
        j<grid.nEndGhostUpdateExplicit[grid.nV][1][1];j++){
        
        //calculate j of centered quantities
        nJCen=j-grid.nCenIntOffset[1];
        
        for(k=grid.nStartGhostUpdateExplicit[grid.nV][1][2];
          k<grid.nEndGhostUpdateExplicit[grid.nV][1][2];k++){
          
          //calculate k of interface quantities
          nKInt=k+grid.nCenIntOffset[2];
          
          //Calculate interpolated quantities
          dU_ijp1halfk_nm1half=0.25*(grid.dLocalGridOld[grid.nU][nIInt][nJCen][k]
            +grid.dLocalGridOld[grid.nU][nIInt][nJCen+1][k]
            +grid.dLocalGridOld[grid.nU][nIInt-1][nJCen][k]
            +grid.dLocalGridOld[grid.nU][nIInt-1][nJCen+1][k]);
          dU0i_nm1half=0.5*(grid.dLocalGridOld[grid.nU0][nIInt][0][0]
            +grid.dLocalGridOld[grid.nU0][nIInt-1][0][0]);
          dV_ip1halfjp1halfk_nm1half=0.5*(grid.dLocalGridOld[grid.nV][i+1][j][k]
            +grid.dLocalGridOld[grid.nV][i][j][k]);
          dV_im1halfjp1halfk_nm1half=0.5*(grid.dLocalGridOld[grid.nV][i][j][k]
            +grid.dLocalGridOld[grid.nV][i-1][j][k]);
          dV_ijp1halfk_nm1half=grid.dLocalGridOld[grid.nV][i][j][k];
          dV_ijp1k_nm1half=(grid.dLocalGridOld[grid.nV][i][j+1][k]
            +grid.dLocalGridOld[grid.nV][i][j][k])*0.5;
          dV_ijk_nm1half=(grid.dLocalGridOld[grid.nV][i][j][k]
            +grid.dLocalGridOld[grid.nV][i][j-1][k])*0.5;
          dDeltaTheta_jp1half=(grid.dLocalGridOld[grid.nDTheta][0][nJCen+1][0]
            +grid.dLocalGridOld[grid.nDTheta][0][nJCen][0])*0.5;
          dRho_ijp1halfk_n=(grid.dLocalGridOld[grid.nD][i][nJCen][k]
            +grid.dLocalGridOld[grid.nD][i][nJCen+1][k])*0.5;
          dV_ijp1halfkp1half_nm1half=(grid.dLocalGridOld[grid.nV][i][j][k+1]
            +grid.dLocalGridOld[grid.nV][i][j][k])*0.5;
          dV_ijp1halfkm1half_nm1half=(grid.dLocalGridOld[grid.nV][i][j][k]
            +grid.dLocalGridOld[grid.nV][i][j][k-1])*0.5;
          dW_ijp1halfk_nm1half=0.25*(grid.dLocalGridOld[grid.nW][i][nJCen+1][nKInt]
            +grid.dLocalGridOld[grid.nW][i][nJCen+1][nKInt-1]
            +grid.dLocalGridOld[grid.nW][i][nJCen][nKInt]
            +grid.dLocalGridOld[grid.nW][i][nJCen][nKInt-1]);
          dP_ijp1k_n=grid.dLocalGridOld[grid.nP][i][nJCen+1][k]
            +grid.dLocalGridOld[grid.nQ1][i][nJCen+1][k];
          dP_ijk_n=grid.dLocalGridOld[grid.nP][i][nJCen][k]
            +grid.dLocalGridOld[grid.nQ1][i][nJCen][k];
          
          //calculate derived quantities
          dU_U0_Diff=dU_ijp1halfk_nm1half-dU0i_nm1half;
          
          //calculate A1
          dA1CenGrad=(dV_ip1halfjp1halfk_nm1half-dV_im1halfjp1halfk_nm1half)
            /grid.dLocalGridOld[grid.nDM][i][0][0];
          dA1UpWindGrad=0.0;
          if(dU_U0_Diff<0.0){//moving in a negative direction
            dA1UpWindGrad=(grid.dLocalGridOld[grid.nV][i+1][j][k]
              -grid.dLocalGridOld[grid.nV][i][j][k])/(grid.dLocalGridOld[grid.nDM][i][0][0]
              +grid.dLocalGridOld[grid.nDM][i+1][0][0])*2.0;
          }
          else{//moving in a positive direction
            dA1UpWindGrad=(grid.dLocalGridOld[grid.nV][i][j][k]
              -grid.dLocalGridOld[grid.nV][i-1][j][k])/(grid.dLocalGridOld[grid.nDM][i][0][0]
              +grid.dLocalGridOld[grid.nDM][i-1][0][0])*2.0;
          }
          dA1=4.0*parameters.dPi*dR_i_n*dR_i_n*grid.dLocalGridOld[grid.nDenAve][i][0][0]*dU_U0_Diff
            *((1.0-parameters.dDonorFrac)*dA1CenGrad+parameters.dDonorFrac*dA1UpWindGrad);
          
          //calculate S1
          dS1=dU_ijp1halfk_nm1half*dV_ijp1halfk_nm1half/dR_i_n;
          
          //calculate dA2
          dA2CenGrad=(dV_ijp1k_nm1half-dV_ijk_nm1half)/dDeltaTheta_jp1half;
          dA2UpWindGrad=0.0;
          if(dV_ijp1halfk_nm1half<0.0){//moning in a negative direction
            dA2UpWindGrad=(grid.dLocalGridOld[grid.nV][i][j+1][k]
              -grid.dLocalGridOld[grid.nV][i][j][k])
              /grid.dLocalGridOld[grid.nDTheta][0][nJCen+1][0];
          }
          else{//moving in a positive direction
            dA2UpWindGrad=(grid.dLocalGridOld[grid.nV][i][j][k]
              -grid.dLocalGridOld[grid.nV][i][j-1][k])
              /grid.dLocalGridOld[grid.nDTheta][0][nJCen][0];
          }
          dA2=dV_ijp1halfk_nm1half/dR_i_n*((1.0-parameters.dDonorFrac)*dA2CenGrad
            +parameters.dDonorFrac*dA2UpWindGrad);
          
          //calculate S2
          dS2=(dP_ijp1k_n-dP_ijk_n)/(dDeltaTheta_jp1half*dRho_ijp1halfk_n*dR_i_n);
          
          //calculate A3
          dA3CenGrad=(dV_ijp1halfkp1half_nm1half-dV_ijp1halfkm1half_nm1half)
            /grid.dLocalGridOld[grid.nDPhi][0][0][k];
          dA3UpWindGrad=0.0;
          if(dW_ijp1halfk_nm1half<0.0){//moving in a negative direction
            dA3UpWindGrad=(grid.dLocalGridOld[grid.nV][i][j][k+1]
              -grid.dLocalGridOld[grid.nV][i][j][k])/(grid.dLocalGridOld[grid.nDPhi][0][0][k+1]
              +grid.dLocalGridOld[grid.nDPhi][0][0][k])*2.0;
          }
          else{//moving in a positive direction
            dA3UpWindGrad=(grid.dLocalGridOld[grid.nV][i][j][k]
              -grid.dLocalGridOld[grid.nV][i][j][k-1])/(grid.dLocalGridOld[grid.nDPhi][0][0][k]
              +grid.dLocalGridOld[grid.nDPhi][0][0][k-1])*2.0;
          }
          dA3=dW_ijp1halfk_nm1half*((1.0-parameters.dDonorFrac)*dA3CenGrad
            +parameters.dDonorFrac*dA3UpWindGrad)/(dR_i_n
            *grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][j][0]);
          
          //calculate S3
          dS3=-1.0*dW_ijp1halfk_nm1half*dW_ijp1halfk_nm1half
            *grid.dLocalGridOld[grid.nCotThetaIJp1halfK][0][j][0]/dR_i_n;
          
          //calculate new velocity
          grid.dLocalGridNew[grid.nV][i][j][k]=grid.dLocalGridOld[grid.nV][i][j][k]
            -time.dDeltat_n*(dA1+dS1+dA2+dS2+dA3+dS3);
        }
      }
    }
  #endif
}
void calNewV_RTP_LES(Grid &grid,Parameters &parameters,Time &time,ProcTop &procTop){
  int i;
  int j;
  int k;
  int nIInt;
  int nJCen;
  int nKInt;
  double dR_i_n;
  double dR_ip1_n;
  double dR_im1_n;
  double dRCu_ip1half;
  double dRCu_im1half;
  double dU_ijp1halfk_nm1half;
  double dU_ijp1k_nm1half;
  double dU_im1halfjp1halfk_nm1half;
  double dU_im1jp1halfk_nm1half;
  double dU_ijk_nm1half;
  double dU0_i_nm1half;
  double dV_ip1halfjp1halfk_nm1half;
  double dV_ijp1k_nm1half;
  double dV_ijk_nm1half;
  double dV_im1halfjp1halfk_nm1half;
  double dV_ijp1halfkp1half_nm1half;
  double dV_ijp1halfkm1half_nm1half;
  double dW_ijp1halfk_nm1half;
  double dW_ijp1halfkp1half_nm1half;
  double dW_ijp1halfkm1half_nm1half;
  double dDTheta_jp1half;
  double dDPhi_kp1half;
  double dDPhi_km1half;
  double dRho_ijp1halfk_n;
  double dRhoAve_ip1half_n;
  double dRhoAve_im1half_n;
  double dP_ijp1k_n;
  double dP_ijk_n;
  double dDM_ip1half;
  double dDM_im1half;
  double dEddyVisc_ip1halfjp1halfk_n;
  double dEddyVisc_im1halfjp1halfk_n;
  double dEddyVisc_ijp1halfk_n;
  double dEddyVisc_ijp1halfkp1half_n;
  double dEddyVisc_ijp1halfkm1half_n;
  double dRSq_ip1half_n;
  double dRSq_im1half_n;
  double dRSq_i_n;
  double dRSqUmU0_ip1halfjp1k_n;
  double dRSqUmU0_im1halfjp1k_n;
  double dRSqUmU0_ip1halfjk_n;
  double dRSqUmU0_im1halfjk_n;
  double dV_R_ip1jp1halfk_n;
  double dV_R_ijp1halfk_n;
  double dV_R_im1jp1halfk_n;
  double dV_R_ip1halfjp1halfk_n;
  double dV_R_im1halfjp1halfk_n;
  double dW_SinTheta_ijp1kp1half_n;
  double dW_SinTheta_ijkp1half_n;
  double dW_SinTheta_ijp1km1half_n;
  double dW_SinTheta_ijkm1half_n;
  double dU_U0_Diff_ijp1halfk_nm1half;
  double dA1CenGrad;
  double dA1UpWindGrad;
  double dA1;
  double dS1;
  double dA2CenGrad;
  double dA2UpWindGrad;
  double dA2;
  double dS2;
  double dA3CenGrad;
  double dA3UpWindGrad;
  double dA3;
  double dS3;
  double dTau_rt_ip1halfjp1halfk_n;
  double dTau_rt_im1halfjp1halfk_n;
  double dDivU_ijp1k_n;
  double dDivU_ijk_n;
  double dTau_tt_ijp1k_n;
  double dTau_tt_ijk_n;
  double dTau_tp_ijp1halfkp1half_n;
  double dTau_tp_ijp1halfkm1half_n;
  double dTA1;
  double dTS1;
  double dTA2;
  double dTS2;
  double dTA3;
  double dTS3;
  double dTS4;
  double dEddyViscosityTerms;
  
  //calculate new v
  for(i=grid.nStartUpdateExplicit[grid.nV][0];i<grid.nEndUpdateExplicit[grid.nV][0];i++){
    
    //calculate i of interface quantities
    nIInt=i+grid.nCenIntOffset[0];
    
    //calculate quantities that only vary radially
    dR_i_n=(grid.dLocalGridOld[grid.nR][nIInt][0][0]+grid.dLocalGridOld[grid.nR][nIInt-1][0][0])
      *0.5;
    dR_ip1_n=(grid.dLocalGridOld[grid.nR][nIInt+1][0][0]+grid.dLocalGridOld[grid.nR][nIInt][0][0])
      *0.5;
    dR_im1_n=(grid.dLocalGridOld[grid.nR][nIInt-1][0][0]+grid.dLocalGridOld[grid.nR][nIInt-2][0][0])
      *0.5;
    dRSq_i_n=dR_i_n*dR_i_n;
    dRSq_ip1half_n=grid.dLocalGridOld[grid.nR][nIInt][0][0]
      *grid.dLocalGridOld[grid.nR][nIInt][0][0];
    dRSq_im1half_n=grid.dLocalGridOld[grid.nR][nIInt-1][0][0]
      *grid.dLocalGridOld[grid.nR][nIInt-1][0][0];
    dRCu_ip1half=dRSq_ip1half_n*grid.dLocalGridOld[grid.nR][nIInt][0][0];
    dRCu_im1half=dRSq_im1half_n*grid.dLocalGridOld[grid.nR][nIInt-1][0][0];
    dU0_i_nm1half=(grid.dLocalGridOld[grid.nU0][nIInt][0][0]
      +grid.dLocalGridOld[grid.nU0][nIInt-1][0][0])*0.5;
    dRhoAve_ip1half_n=(grid.dLocalGridOld[grid.nDenAve][i+1][0][0]
      +grid.dLocalGridOld[grid.nDenAve][i][0][0])*0.5;
    dRhoAve_im1half_n=(grid.dLocalGridOld[grid.nDenAve][i][0][0]
      +grid.dLocalGridOld[grid.nDenAve][i-1][0][0])*0.5;
    dDM_ip1half=(grid.dLocalGridOld[grid.nDM][i+1][0][0]+grid.dLocalGridOld[grid.nDM][i][0][0])*0.5;
    dDM_im1half=(grid.dLocalGridOld[grid.nDM][i-1][0][0]+grid.dLocalGridOld[grid.nDM][i][0][0])*0.5;
    
    for(j=grid.nStartUpdateExplicit[grid.nV][1];j<grid.nEndUpdateExplicit[grid.nV][1];j++){
      
      //calculate j of centered quantities
      nJCen=j-grid.nCenIntOffset[1];
      
      //calculate quantities that only vary with theta and or radius
      dDTheta_jp1half=(grid.dLocalGridOld[grid.nDTheta][0][nJCen+1][0]
        +grid.dLocalGridOld[grid.nDTheta][0][nJCen][0])*0.5;
      
      for(k=grid.nStartUpdateExplicit[grid.nV][2];k<grid.nEndUpdateExplicit[grid.nV][2];k++){
        
        //calculate k of interface quantities
        nKInt=k+grid.nCenIntOffset[2];
        
        dDPhi_kp1half=(grid.dLocalGridOld[grid.nDPhi][0][0][k+1]
          +grid.dLocalGridOld[grid.nDPhi][0][0][k])*0.5;
        dDPhi_km1half=(grid.dLocalGridOld[grid.nDPhi][0][0][k]
          +grid.dLocalGridOld[grid.nDPhi][0][0][k-1])*0.5;
        
        //Calculate interpolated quantities
        dU_ijp1halfk_nm1half=0.25*(grid.dLocalGridOld[grid.nU][nIInt][nJCen][k]
          +grid.dLocalGridOld[grid.nU][nIInt][nJCen+1][k]
          +grid.dLocalGridOld[grid.nU][nIInt-1][nJCen][k]
          +grid.dLocalGridOld[grid.nU][nIInt-1][nJCen+1][k]);
        dU_ijp1k_nm1half=(grid.dLocalGridOld[grid.nU][nIInt][nJCen+1][k]
          +grid.dLocalGridOld[grid.nU][nIInt-1][nJCen+1][k])*0.5;
        dU_im1halfjp1halfk_nm1half=(grid.dLocalGridOld[grid.nU][nIInt-1][nJCen+1][k]
          +grid.dLocalGridOld[grid.nU][nIInt-1][nJCen][k])*0.5;
        dU_im1jp1halfk_nm1half=0.25*(grid.dLocalGridOld[grid.nU][nIInt-1][nJCen][k]
          +grid.dLocalGridOld[grid.nU][nIInt-1][nJCen+1][k]
          +grid.dLocalGridOld[grid.nU][nIInt-2][nJCen][k]
          +grid.dLocalGridOld[grid.nU][nIInt-2][nJCen+1][k]);
        dU_ijk_nm1half=(grid.dLocalGridOld[grid.nU][nIInt][nJCen][k]
          +grid.dLocalGridOld[grid.nU][nIInt-1][nJCen][k])*0.5;
        dV_ip1halfjp1halfk_nm1half=0.5*(grid.dLocalGridOld[grid.nV][i+1][j][k]
          +grid.dLocalGridOld[grid.nV][i][j][k]);
        dV_im1halfjp1halfk_nm1half=0.5*(grid.dLocalGridOld[grid.nV][i][j][k]
          +grid.dLocalGridOld[grid.nV][i-1][j][k]);
        dV_ijp1k_nm1half=(grid.dLocalGridOld[grid.nV][i][j+1][k]
          +grid.dLocalGridOld[grid.nV][i][j][k])*0.5;
        dV_ijk_nm1half=(grid.dLocalGridOld[grid.nV][i][j][k]
          +grid.dLocalGridOld[grid.nV][i][j-1][k])*0.5;
        dV_ijp1halfkp1half_nm1half=(grid.dLocalGridOld[grid.nV][i][j][k+1]
          +grid.dLocalGridOld[grid.nV][i][j][k])*0.5;
        dV_ijp1halfkm1half_nm1half=(grid.dLocalGridOld[grid.nV][i][j][k]
          +grid.dLocalGridOld[grid.nV][i][j][k-1])*0.5;
        dW_ijp1halfk_nm1half=0.25*(grid.dLocalGridOld[grid.nW][i][nJCen+1][nKInt]
          +grid.dLocalGridOld[grid.nW][i][nJCen+1][nKInt-1]
          +grid.dLocalGridOld[grid.nW][i][nJCen][nKInt]
          +grid.dLocalGridOld[grid.nW][i][nJCen][nKInt-1]);
        dW_ijp1halfkp1half_nm1half=(grid.dLocalGridOld[grid.nW][i][nJCen+1][nKInt]
          +grid.dLocalGridOld[grid.nW][i][nJCen][nKInt])*0.5;
        dW_ijp1halfkm1half_nm1half=(grid.dLocalGridOld[grid.nW][i][nJCen+1][nKInt-1]
          +grid.dLocalGridOld[grid.nW][i][nJCen][nKInt-1])*0.5;
        dRho_ijp1halfk_n=(grid.dLocalGridOld[grid.nD][i][nJCen][k]
          +grid.dLocalGridOld[grid.nD][i][nJCen+1][k])*0.5;
        dP_ijp1k_n=grid.dLocalGridOld[grid.nP][i][nJCen+1][k]
          +grid.dLocalGridOld[grid.nQ0][i][nJCen+1][k]+grid.dLocalGridOld[grid.nQ1][i][nJCen+1][k]
          +grid.dLocalGridOld[grid.nQ2][i][nJCen+1][k];
        dP_ijk_n=grid.dLocalGridOld[grid.nP][i][nJCen][k]+grid.dLocalGridOld[grid.nQ0][i][nJCen][k]
          +grid.dLocalGridOld[grid.nQ1][i][nJCen][k]+grid.dLocalGridOld[grid.nQ2][i][nJCen][k];
        dEddyVisc_ip1halfjp1halfk_n=(grid.dLocalGridOld[grid.nEddyVisc][i][nJCen][k]
          +grid.dLocalGridOld[grid.nEddyVisc][i+1][nJCen][k]
          +grid.dLocalGridOld[grid.nEddyVisc][i+1][nJCen+1][k]
          +grid.dLocalGridOld[grid.nEddyVisc][i][nJCen+1][k])*0.25;
        dEddyVisc_im1halfjp1halfk_n=(grid.dLocalGridOld[grid.nEddyVisc][i][nJCen][k]
          +grid.dLocalGridOld[grid.nEddyVisc][i-1][nJCen][k]
          +grid.dLocalGridOld[grid.nEddyVisc][i-1][nJCen+1][k]
          +grid.dLocalGridOld[grid.nEddyVisc][i][nJCen+1][k])*0.25;
        dEddyVisc_ijp1halfk_n=(grid.dLocalGridOld[grid.nEddyVisc][i][nJCen][k]
          +grid.dLocalGridOld[grid.nEddyVisc][i][nJCen+1][k])*0.5;
        dEddyVisc_ijp1halfkp1half_n=(grid.dLocalGridOld[grid.nEddyVisc][i][nJCen+1][k+1]
          +grid.dLocalGridOld[grid.nEddyVisc][i][nJCen+1][k]
          +grid.dLocalGridOld[grid.nEddyVisc][i][nJCen][k+1]
          +grid.dLocalGridOld[grid.nEddyVisc][i][nJCen][k])*0.25;
        dEddyVisc_ijp1halfkm1half_n=(grid.dLocalGridOld[grid.nEddyVisc][i][nJCen+1][k]
          +grid.dLocalGridOld[grid.nEddyVisc][i][nJCen+1][k-1]
          +grid.dLocalGridOld[grid.nEddyVisc][i][nJCen][k]
          +grid.dLocalGridOld[grid.nEddyVisc][i][nJCen][k-1])*0.25;
        
        //calculate derived quantities
        dU_U0_Diff_ijp1halfk_nm1half=dU_ijp1halfk_nm1half-dU0_i_nm1half;
        dRSqUmU0_ip1halfjp1k_n=dRSq_ip1half_n*(grid.dLocalGridOld[grid.nU][nIInt][nJCen+1][k]
          -grid.dLocalGridOld[grid.nU0][nIInt][0][0]);
        dRSqUmU0_im1halfjp1k_n=dRSq_im1half_n*(grid.dLocalGridOld[grid.nU][nIInt-1][nJCen+1][k]
          -grid.dLocalGridOld[grid.nU0][nIInt-1][0][0]);
        dRSqUmU0_ip1halfjk_n=dRSq_ip1half_n*(grid.dLocalGridOld[grid.nU][nIInt][nJCen][k]
          -grid.dLocalGridOld[grid.nU0][nIInt][0][0]);
        dRSqUmU0_im1halfjk_n=dRSq_im1half_n*(grid.dLocalGridOld[grid.nU][nIInt-1][nJCen][k]
          -grid.dLocalGridOld[grid.nU0][nIInt-1][0][0]);
        dU_U0_Diff_ijp1halfk_nm1half=dU_ijp1halfk_nm1half-dU0_i_nm1half;
        dV_R_ip1jp1halfk_n=grid.dLocalGridOld[grid.nV][i+1][j][k]/dR_ip1_n;
        dV_R_ijp1halfk_n=grid.dLocalGridOld[grid.nV][i][j][k]/dR_i_n;
        dV_R_im1jp1halfk_n=grid.dLocalGridOld[grid.nV][i-1][j][k]/dR_im1_n;
        dV_R_ip1halfjp1halfk_n=dV_ip1halfjp1halfk_nm1half/grid.dLocalGridOld[grid.nR][nIInt][0][0];
        dV_R_im1halfjp1halfk_n=dV_im1halfjp1halfk_nm1half
          /grid.dLocalGridOld[grid.nR][nIInt-1][0][0];
        dW_SinTheta_ijp1kp1half_n=grid.dLocalGridOld[grid.nW][i][nJCen+1][nKInt]
          /grid.dLocalGridOld[grid.nSinThetaIJK][0][nJCen+1][0];
        dW_SinTheta_ijkp1half_n=grid.dLocalGridOld[grid.nW][i][nJCen][nKInt]
          /grid.dLocalGridOld[grid.nSinThetaIJK][0][nJCen][0];
        dW_SinTheta_ijp1km1half_n=grid.dLocalGridOld[grid.nW][i][nJCen+1][nKInt-1]
          /grid.dLocalGridOld[grid.nSinThetaIJK][0][nJCen+1][0];
        dW_SinTheta_ijkm1half_n=grid.dLocalGridOld[grid.nW][i][nJCen][nKInt-1]
          /grid.dLocalGridOld[grid.nSinThetaIJK][0][nJCen][0];
        
        //calculate A1
        dA1CenGrad=(dV_ip1halfjp1halfk_nm1half-dV_im1halfjp1halfk_nm1half)
          /grid.dLocalGridOld[grid.nDM][i][0][0];
        dA1UpWindGrad=0.0;
        if(dU_U0_Diff_ijp1halfk_nm1half<0.0){//moving in a negative direction
          dA1UpWindGrad=(grid.dLocalGridOld[grid.nV][i+1][j][k]
            -grid.dLocalGridOld[grid.nV][i][j][k])/(grid.dLocalGridOld[grid.nDM][i][0][0]
            +grid.dLocalGridOld[grid.nDM][i+1][0][0])*2.0;
        }
        else{//moving in a positive direction
          dA1UpWindGrad=(grid.dLocalGridOld[grid.nV][i][j][k]
            -grid.dLocalGridOld[grid.nV][i-1][j][k])/(grid.dLocalGridOld[grid.nDM][i][0][0]
            +grid.dLocalGridOld[grid.nDM][i-1][0][0])*2.0;
        }
        dA1=dU_U0_Diff_ijp1halfk_nm1half*((1.0-parameters.dDonorFrac)*dA1CenGrad
          +parameters.dDonorFrac*dA1UpWindGrad);
        
        //calculate S1
        dS1=dU_ijp1halfk_nm1half*grid.dLocalGridOld[grid.nV][i][j][k]/dR_i_n;
        
        //calculate dA2
        dA2CenGrad=(dV_ijp1k_nm1half-dV_ijk_nm1half)/dDTheta_jp1half;
        dA2UpWindGrad=0.0;
        if(grid.dLocalGridOld[grid.nV][i][j][k]<0.0){//moning in a negative direction
          dA2UpWindGrad=(grid.dLocalGridOld[grid.nV][i][j+1][k]
            -grid.dLocalGridOld[grid.nV][i][j][k])/grid.dLocalGridOld[grid.nDTheta][0][nJCen+1][0];
        }
        else{//moving in a positive direction
          dA2UpWindGrad=(grid.dLocalGridOld[grid.nV][i][j][k]
            -grid.dLocalGridOld[grid.nV][i][j-1][k])/grid.dLocalGridOld[grid.nDTheta][0][nJCen][0];
        }
        dA2=grid.dLocalGridOld[grid.nV][i][j][k]/dR_i_n*((1.0-parameters.dDonorFrac)*dA2CenGrad
          +parameters.dDonorFrac*dA2UpWindGrad);
        
        //calculate S2
        dS2=(dP_ijp1k_n-dP_ijk_n)/(dDTheta_jp1half*dRho_ijp1halfk_n*dR_i_n);
        
        //calculate A3
        dA3CenGrad=(dV_ijp1halfkp1half_nm1half-dV_ijp1halfkm1half_nm1half)
          /grid.dLocalGridOld[grid.nDPhi][0][0][k];
        dA3UpWindGrad=0.0;
        if(dW_ijp1halfk_nm1half<0.0){//moving in a negative direction
          dA3UpWindGrad=(grid.dLocalGridOld[grid.nV][i][j][k+1]
            -grid.dLocalGridOld[grid.nV][i][j][k])/(grid.dLocalGridOld[grid.nDPhi][0][0][k+1]
            +grid.dLocalGridOld[grid.nDPhi][0][0][k])*2.0;
        }
        else{//moving in a positive direction
          dA3UpWindGrad=(grid.dLocalGridOld[grid.nV][i][j][k]
            -grid.dLocalGridOld[grid.nV][i][j][k-1])/(grid.dLocalGridOld[grid.nDPhi][0][0][k]
            +grid.dLocalGridOld[grid.nDPhi][0][0][k-1])*2.0;
        }
        dA3=dW_ijp1halfk_nm1half*((1.0-parameters.dDonorFrac)*dA3CenGrad
          +parameters.dDonorFrac*dA3UpWindGrad)/(dR_i_n
          *grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][j][0]);
        
        //calculate S3
        dS3=-1.0*dW_ijp1halfk_nm1half*dW_ijp1halfk_nm1half
          *grid.dLocalGridOld[grid.nCotThetaIJp1halfK][0][j][0]/dR_i_n;
        
        //calculate Tau_rt_ip1halfjp1halfk_n
        dTau_rt_ip1halfjp1halfk_n=dEddyVisc_ip1halfjp1halfk_n*(4.0*parameters.dPi*dRCu_ip1half
          *dRhoAve_ip1half_n*(dV_R_ip1jp1halfk_n-dV_R_ijp1halfk_n)/dDM_ip1half+1.0
          /grid.dLocalGridOld[grid.nR][nIInt][0][0]*((grid.dLocalGridOld[grid.nU][nIInt][nJCen+1][k]
          -grid.dLocalGridOld[grid.nU0][nIInt][0][0])-(grid.dLocalGridOld[grid.nU][nIInt][nJCen][k]
          -grid.dLocalGridOld[grid.nU0][nIInt][0][0]))/dDTheta_jp1half);
        
        //calculate Tau_rt_im1halfjp1halfk_n
        dTau_rt_im1halfjp1halfk_n=dEddyVisc_im1halfjp1halfk_n*(4.0*parameters.dPi*dRCu_im1half
          *dRhoAve_im1half_n*(dV_R_ijp1halfk_n-dV_R_im1jp1halfk_n)/dDM_im1half+1.0
          /grid.dLocalGridOld[grid.nR][nIInt-1][0][0]
          *((grid.dLocalGridOld[grid.nU][nIInt-1][nJCen+1][k]
          -grid.dLocalGridOld[grid.nU0][nIInt-1][0][0])
          -(grid.dLocalGridOld[grid.nU][nIInt-1][nJCen][k]
          -grid.dLocalGridOld[grid.nU0][nIInt-1][0][0]))/dDTheta_jp1half);
        
        //calculate DivU_ijp1k_n
        dDivU_ijp1k_n=4.0*parameters.dPi*grid.dLocalGridOld[grid.nDenAve][i][0][0]
          *(dRSqUmU0_ip1halfjp1k_n-dRSqUmU0_im1halfjp1k_n)/grid.dLocalGridOld[grid.nDM][i][0][0]
          +((grid.dLocalGridOld[grid.nV][i][j+1][k]
          *grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][j+1][0]
          -grid.dLocalGridOld[grid.nV][i][j][k]
          *grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][j][0])
          /grid.dLocalGridOld[grid.nDTheta][0][nJCen+1][0]
          +(grid.dLocalGridOld[grid.nW][i][nJCen+1][nKInt]
          -grid.dLocalGridOld[grid.nW][i][nJCen+1][nKInt-1])
          /(grid.dLocalGridOld[grid.nDPhi][0][0][k]))
          /(dR_i_n*grid.dLocalGridOld[grid.nSinThetaIJK][0][nJCen+1][0]);
        
        //calculate DivU_ijk_n
        dDivU_ijk_n=4.0*parameters.dPi*grid.dLocalGridOld[grid.nDenAve][i][0][0]
          *(dRSqUmU0_ip1halfjk_n-dRSqUmU0_im1halfjk_n)/grid.dLocalGridOld[grid.nDM][i][0][0]
          +((grid.dLocalGridOld[grid.nV][i][j][k]
          *grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][j][0]
          -grid.dLocalGridOld[grid.nV][i][j-1][k]
          *grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][j-1][0])
          /grid.dLocalGridOld[grid.nDTheta][0][nJCen][0]
          +(grid.dLocalGridOld[grid.nW][i][nJCen][nKInt]
          -grid.dLocalGridOld[grid.nW][i][nJCen][nKInt-1])
          /(grid.dLocalGridOld[grid.nDPhi][0][0][k]))
          /(dR_i_n*grid.dLocalGridOld[grid.nSinThetaIJK][0][nJCen][0]);
        
        //calculate Tau_tt_ijp1k_n
        dTau_tt_ijp1k_n=2.0*grid.dLocalGridOld[grid.nEddyVisc][i][nJCen+1][k]
          *((grid.dLocalGridOld[grid.nV][i][j+1][k]-grid.dLocalGridOld[grid.nV][i][j][k])
          /(dR_i_n*grid.dLocalGridOld[grid.nDTheta][0][nJCen+1][0])
          +(dU_ijp1k_nm1half-dU0_i_nm1half)/dR_i_n-0.333333333333333*dDivU_ijp1k_n);
        
        //calculate Tau_tt_ijk_n
        dTau_tt_ijk_n=2.0*grid.dLocalGridOld[grid.nEddyVisc][i][nJCen][k]
          *((grid.dLocalGridOld[grid.nV][i][j][k]-grid.dLocalGridOld[grid.nV][i][j-1][k])
          /(grid.dLocalGridOld[grid.nDTheta][0][nJCen][0]*dR_i_n)
          +(dU_ijk_nm1half-dU0_i_nm1half)/dR_i_n-0.333333333333333*dDivU_ijk_n);
        
        //calculate dTau_tp_ijp1halfkp1half_n
        dTau_tp_ijp1halfkp1half_n=dEddyVisc_ijp1halfkp1half_n
          *(grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][j][0]*(dW_SinTheta_ijp1kp1half_n
          -dW_SinTheta_ijkp1half_n)/(dR_i_n*dDTheta_jp1half)+(grid.dLocalGridOld[grid.nV][i][j][k+1]
          -grid.dLocalGridOld[grid.nV][i][j][k])/(dR_i_n
          *grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][j][0]*dDPhi_kp1half));
        
        //calculate dTau_tp_ijp1halfmp1half_n
        dTau_tp_ijp1halfkm1half_n=dEddyVisc_ijp1halfkm1half_n
          *(grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][j][0]*(dW_SinTheta_ijp1km1half_n
          -dW_SinTheta_ijkm1half_n)/(dR_i_n*dDTheta_jp1half)+(grid.dLocalGridOld[grid.nV][i][j][k]
          -grid.dLocalGridOld[grid.nV][i][j][k-1])/(dR_i_n
          *grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][j][0]*dDPhi_km1half));
        
        //calculate TA1
        dTA1=(dTau_rt_ip1halfjp1halfk_n-dTau_rt_im1halfjp1halfk_n)
          /(grid.dLocalGridOld[grid.nDM][i][0][0]*dRho_ijp1halfk_n);
        
        //calculate TS1
        dTS1=3.0*dEddyVisc_ijp1halfk_n*(dV_R_ip1halfjp1halfk_n-dV_R_im1halfjp1halfk_n)
          /(grid.dLocalGridOld[grid.nDM][i][0][0]*dRho_ijp1halfk_n);
        
        //calculate TA2
        dTA2=(dTau_tt_ijp1k_n-dTau_tt_ijk_n)/(dRho_ijp1halfk_n*dR_i_n*dDTheta_jp1half);
        
        //calculate TS2
        dTS2=(2.0*grid.dLocalGridOld[grid.nCotThetaIJp1halfK][0][j][0]*(dV_ijp1k_nm1half
          -dV_ijk_nm1half)+3.0*((dU_ijp1k_nm1half-dU0_i_nm1half)
          -(dU_ijk_nm1half-dU0_i_nm1half)))/(dR_i_n*dDTheta_jp1half);
        
        //calculate dTA3
        dTA3=(dTau_tp_ijp1halfkp1half_n-dTau_tp_ijp1halfkm1half_n)/(dRho_ijp1halfk_n*dR_i_n
          *grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][j][0]
          *grid.dLocalGridOld[grid.nDPhi][0][0][k]);
        
        //calculate dTS3
        dTS3=2.0*grid.dLocalGridOld[grid.nCotThetaIJp1halfK][0][j][0]*(dW_ijp1halfkp1half_nm1half
          -dW_ijp1halfkm1half_nm1half)/(dR_i_n*grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][j][0]
          *grid.dLocalGridOld[grid.nDPhi][0][0][k]);
        
        //calculate TS4
        dTS4=2.0*grid.dLocalGridOld[grid.nV][i][j][k]
          *grid.dLocalGridOld[grid.nCotThetaIJp1halfK][0][j][0]
          *grid.dLocalGridOld[grid.nCotThetaIJp1halfK][0][j][0]/dR_i_n;
        
        dEddyViscosityTerms=-4.0*dRSq_i_n*grid.dLocalGridOld[grid.nDenAve][i][0][0]*(dTA1+dTS1)
          -dTA2-dTA3-dEddyVisc_ijp1halfk_n/(dRho_ijp1halfk_n*dR_i_n)*(dTS2-dTS3-dTS4);
        
        //calculate new velocity
        grid.dLocalGridNew[grid.nV][i][j][k]=grid.dLocalGridOld[grid.nV][i][j][k]
          -time.dDeltat_n*(4.0*dRSq_i_n*grid.dLocalGridOld[grid.nDenAve][i][0][0]*(dA1)
          +dS1+dA2+dS2+dA3+dS3+dEddyViscosityTerms);
      }
    }
  }
  
  //ghost region 0, outter most ghost region in x1 direction
  for(i=grid.nStartGhostUpdateExplicit[grid.nV][0][0];
    i<grid.nEndGhostUpdateExplicit[grid.nV][0][0];i++){
    
    //calculate i of interface quantities
    nIInt=i+grid.nCenIntOffset[0];
    
    //calculate quantities that only vary radially
    dR_i_n=(grid.dLocalGridOld[grid.nR][nIInt][0][0]+grid.dLocalGridOld[grid.nR][nIInt-1][0][0])
      *0.5;
    dR_ip1_n=grid.dLocalGridOld[grid.nR][nIInt][0][0];
    dR_im1_n=(grid.dLocalGridOld[grid.nR][nIInt-1][0][0]+grid.dLocalGridOld[grid.nR][nIInt-2][0][0])
      *0.5;
    dRSq_i_n=dR_i_n*dR_i_n;
    dRSq_ip1half_n=grid.dLocalGridOld[grid.nR][nIInt][0][0]
      *grid.dLocalGridOld[grid.nR][nIInt][0][0];
    dRSq_im1half_n=grid.dLocalGridOld[grid.nR][nIInt-1][0][0]
      *grid.dLocalGridOld[grid.nR][nIInt-1][0][0];
    dRCu_ip1half=dRSq_ip1half_n*grid.dLocalGridOld[grid.nR][nIInt][0][0];
    dRCu_im1half=dRSq_im1half_n*grid.dLocalGridOld[grid.nR][nIInt-1][0][0];
    dU0_i_nm1half=(grid.dLocalGridOld[grid.nU0][nIInt][0][0]
      +grid.dLocalGridOld[grid.nU0][nIInt-1][0][0])*0.5;
    dRhoAve_ip1half_n=grid.dLocalGridOld[grid.nDenAve][i][0][0]*0.5;/**\BC Assuming density outside
      star is zero*/
    dRhoAve_im1half_n=(grid.dLocalGridOld[grid.nDenAve][i][0][0]
      +grid.dLocalGridOld[grid.nDenAve][i-1][0][0])*0.5;
    dDM_ip1half=grid.dLocalGridOld[grid.nDM][i][0][0]*0.5;
    dDM_im1half=(grid.dLocalGridOld[grid.nDM][i-1][0][0]+grid.dLocalGridOld[grid.nDM][i][0][0])*0.5;
    
    for(j=grid.nStartGhostUpdateExplicit[grid.nV][0][1];
      j<grid.nEndGhostUpdateExplicit[grid.nV][0][1];j++){
      
      //calculate j of centered quantities
      nJCen=j-grid.nCenIntOffset[1];
      
      //calculate quantities that only vary with theta and or radius
      dDTheta_jp1half=(grid.dLocalGridOld[grid.nDTheta][0][nJCen+1][0]
        +grid.dLocalGridOld[grid.nDTheta][0][nJCen][0])*0.5;
      
      for(k=grid.nStartGhostUpdateExplicit[grid.nV][0][2];
        k<grid.nEndGhostUpdateExplicit[grid.nV][0][2];k++){
        
        //calculate k of interface quantities
        nKInt=k+grid.nCenIntOffset[2];
        dDPhi_kp1half=(grid.dLocalGridOld[grid.nDPhi][0][0][k+1]
          +grid.dLocalGridOld[grid.nDPhi][0][0][k])*0.5;
        dDPhi_km1half=(grid.dLocalGridOld[grid.nDPhi][0][0][k]
          +grid.dLocalGridOld[grid.nDPhi][0][0][k-1])*0.5;
        
        //Calculate interpolated quantities
        dU_ijp1halfk_nm1half=0.25*(grid.dLocalGridOld[grid.nU][nIInt][nJCen][k]
          +grid.dLocalGridOld[grid.nU][nIInt][nJCen+1][k]
          +grid.dLocalGridOld[grid.nU][nIInt-1][nJCen][k]
          +grid.dLocalGridOld[grid.nU][nIInt-1][nJCen+1][k]);
        dU_ijp1k_nm1half=(grid.dLocalGridOld[grid.nU][nIInt][nJCen+1][k]
          +grid.dLocalGridOld[grid.nU][nIInt-1][nJCen+1][k])*0.5;
        dU_im1halfjp1halfk_nm1half=(grid.dLocalGridOld[grid.nU][nIInt-1][nJCen+1][k]
          +grid.dLocalGridOld[grid.nU][nIInt-1][nJCen][k])*0.5;
        dU_im1jp1halfk_nm1half=0.25*(grid.dLocalGridOld[grid.nU][nIInt-1][nJCen][k]
          +grid.dLocalGridOld[grid.nU][nIInt-1][nJCen+1][k]
          +grid.dLocalGridOld[grid.nU][nIInt-2][nJCen][k]
          +grid.dLocalGridOld[grid.nU][nIInt-2][nJCen+1][k]);
        dU_ijk_nm1half=(grid.dLocalGridOld[grid.nU][nIInt][nJCen][k]
          +grid.dLocalGridOld[grid.nU][nIInt-1][nJCen][k])*0.5;
        dV_ip1halfjp1halfk_nm1half=grid.dLocalGridOld[grid.nV][i][j][k];/**\BC Assuming theta 
          velocity is constant across surface.*/
        dV_im1halfjp1halfk_nm1half=0.5*(grid.dLocalGridOld[grid.nV][i][j][k]
          +grid.dLocalGridOld[grid.nV][i-1][j][k]);
        dV_ijp1k_nm1half=(grid.dLocalGridOld[grid.nV][i][j+1][k]
          +grid.dLocalGridOld[grid.nV][i][j][k])*0.5;
        dV_ijk_nm1half=(grid.dLocalGridOld[grid.nV][i][j][k]
          +grid.dLocalGridOld[grid.nV][i][j-1][k])*0.5;
        dV_ijp1halfkp1half_nm1half=(grid.dLocalGridOld[grid.nV][i][j][k+1]
          +grid.dLocalGridOld[grid.nV][i][j][k])*0.5;
        dV_ijp1halfkm1half_nm1half=(grid.dLocalGridOld[grid.nV][i][j][k]
          +grid.dLocalGridOld[grid.nV][i][j][k-1])*0.5;
        dW_ijp1halfk_nm1half=0.25*(grid.dLocalGridOld[grid.nW][i][nJCen+1][nKInt]
          +grid.dLocalGridOld[grid.nW][i][nJCen+1][nKInt-1]
          +grid.dLocalGridOld[grid.nW][i][nJCen][nKInt]
          +grid.dLocalGridOld[grid.nW][i][nJCen][nKInt-1]);
        dW_ijp1halfkp1half_nm1half=(grid.dLocalGridOld[grid.nW][i][nJCen+1][nKInt]
          +grid.dLocalGridOld[grid.nW][i][nJCen][nKInt])*0.5;
        dW_ijp1halfkm1half_nm1half=(grid.dLocalGridOld[grid.nW][i][nJCen+1][nKInt-1]
          +grid.dLocalGridOld[grid.nW][i][nJCen][nKInt-1])*0.5;
        dRho_ijp1halfk_n=(grid.dLocalGridOld[grid.nD][i][nJCen][k]
          +grid.dLocalGridOld[grid.nD][i][nJCen+1][k])*0.5;
        dP_ijp1k_n=grid.dLocalGridOld[grid.nP][i][nJCen+1][k]
          +grid.dLocalGridOld[grid.nQ0][i][nJCen+1][k]+grid.dLocalGridOld[grid.nQ1][i][nJCen+1][k]
          +grid.dLocalGridOld[grid.nQ2][i][nJCen+1][k];
        dP_ijk_n=grid.dLocalGridOld[grid.nP][i][nJCen][k]+grid.dLocalGridOld[grid.nQ0][i][nJCen][k]
          +grid.dLocalGridOld[grid.nQ1][i][nJCen][k]+grid.dLocalGridOld[grid.nQ2][i][nJCen][k];
        dEddyVisc_ip1halfjp1halfk_n=(grid.dLocalGridOld[grid.nEddyVisc][i][nJCen][k]
          +grid.dLocalGridOld[grid.nEddyVisc][i][nJCen+1][k])*0.25;/**\BC Assuming eddy viscosity is
          zero at surface.*/
        dEddyVisc_im1halfjp1halfk_n=(grid.dLocalGridOld[grid.nEddyVisc][i][nJCen][k]
          +grid.dLocalGridOld[grid.nEddyVisc][i-1][nJCen][k]
          +grid.dLocalGridOld[grid.nEddyVisc][i-1][nJCen+1][k]
          +grid.dLocalGridOld[grid.nEddyVisc][i][nJCen+1][k])*0.25;
        dEddyVisc_ijp1halfk_n=(grid.dLocalGridOld[grid.nEddyVisc][i][nJCen][k]
          +grid.dLocalGridOld[grid.nEddyVisc][i][nJCen+1][k])*0.5;
        dEddyVisc_ijp1halfkp1half_n=(grid.dLocalGridOld[grid.nEddyVisc][i][nJCen+1][k+1]
          +grid.dLocalGridOld[grid.nEddyVisc][i][nJCen+1][k]
          +grid.dLocalGridOld[grid.nEddyVisc][i][nJCen][k+1]
          +grid.dLocalGridOld[grid.nEddyVisc][i][nJCen][k])*0.25;
        dEddyVisc_ijp1halfkm1half_n=(grid.dLocalGridOld[grid.nEddyVisc][i][nJCen+1][k]
          +grid.dLocalGridOld[grid.nEddyVisc][i][nJCen+1][k-1]
          +grid.dLocalGridOld[grid.nEddyVisc][i][nJCen][k]
          +grid.dLocalGridOld[grid.nEddyVisc][i][nJCen][k-1])*0.25;
        
        //calculate derived quantities
        dU_U0_Diff_ijp1halfk_nm1half=dU_ijp1halfk_nm1half-dU0_i_nm1half;
        dRSqUmU0_ip1halfjp1k_n=dRSq_ip1half_n*(grid.dLocalGridOld[grid.nU][nIInt][nJCen+1][k]
          -grid.dLocalGridOld[grid.nU0][nIInt][0][0]);
        dRSqUmU0_im1halfjp1k_n=dRSq_im1half_n*(grid.dLocalGridOld[grid.nU][nIInt-1][nJCen+1][k]
          -grid.dLocalGridOld[grid.nU0][nIInt-1][0][0]);
        dRSqUmU0_ip1halfjk_n=dRSq_ip1half_n*(grid.dLocalGridOld[grid.nU][nIInt][nJCen][k]
          -grid.dLocalGridOld[grid.nU0][nIInt][0][0]);
        dRSqUmU0_im1halfjk_n=dRSq_im1half_n*(grid.dLocalGridOld[grid.nU][nIInt-1][nJCen][k]
          -grid.dLocalGridOld[grid.nU0][nIInt-1][0][0]);
        dU_U0_Diff_ijp1halfk_nm1half=dU_ijp1halfk_nm1half-dU0_i_nm1half;
        dV_R_ip1jp1halfk_n=grid.dLocalGridOld[grid.nV][i][j][k]/dR_ip1_n;
        dV_R_ijp1halfk_n=grid.dLocalGridOld[grid.nV][i][j][k]/dR_i_n;
        dV_R_im1jp1halfk_n=grid.dLocalGridOld[grid.nV][i-1][j][k]/dR_im1_n;
        dV_R_ip1halfjp1halfk_n=dV_ip1halfjp1halfk_nm1half/grid.dLocalGridOld[grid.nR][nIInt][0][0];
        dV_R_im1halfjp1halfk_n=dV_im1halfjp1halfk_nm1half
          /grid.dLocalGridOld[grid.nR][nIInt-1][0][0];
        dW_SinTheta_ijp1kp1half_n=grid.dLocalGridOld[grid.nW][i][nJCen+1][nKInt]
          /grid.dLocalGridOld[grid.nSinThetaIJK][0][nJCen+1][0];
        dW_SinTheta_ijkp1half_n=grid.dLocalGridOld[grid.nW][i][nJCen][nKInt]
          /grid.dLocalGridOld[grid.nSinThetaIJK][0][nJCen][0];
        dW_SinTheta_ijp1km1half_n=grid.dLocalGridOld[grid.nW][i][nJCen+1][nKInt-1]
          /grid.dLocalGridOld[grid.nSinThetaIJK][0][nJCen+1][0];
        dW_SinTheta_ijkm1half_n=grid.dLocalGridOld[grid.nW][i][nJCen][nKInt-1]
          /grid.dLocalGridOld[grid.nSinThetaIJK][0][nJCen][0];
        
        //calculate A1
        dA1CenGrad=(dV_ip1halfjp1halfk_nm1half-dV_im1halfjp1halfk_nm1half)
          /grid.dLocalGridOld[grid.nDM][i][0][0];
        dA1UpWindGrad=0.0;
        if(dU_U0_Diff_ijp1halfk_nm1half<0.0){//moving in a negative direction
          dA1UpWindGrad=dA1CenGrad;
        }
        else{//moving in a positive direction
          dA1UpWindGrad=(grid.dLocalGridOld[grid.nV][i][j][k]
            -grid.dLocalGridOld[grid.nV][i-1][j][k])/(grid.dLocalGridOld[grid.nDM][i][0][0]
            +grid.dLocalGridOld[grid.nDM][i-1][0][0])*2.0;
        }
        dA1=dU_U0_Diff_ijp1halfk_nm1half*((1.0-parameters.dDonorFrac)*dA1CenGrad
          +parameters.dDonorFrac*dA1UpWindGrad);
        
        //calculate S1
        dS1=dU_ijp1halfk_nm1half*grid.dLocalGridOld[grid.nV][i][j][k]/dR_i_n;
        
        //calculate dA2
        dA2CenGrad=(dV_ijp1k_nm1half-dV_ijk_nm1half)/dDTheta_jp1half;
        dA2UpWindGrad=0.0;
        if(grid.dLocalGridOld[grid.nV][i][j][k]<0.0){//moving in a negative direction
          dA2UpWindGrad=(grid.dLocalGridOld[grid.nV][i][j+1][k]
            -grid.dLocalGridOld[grid.nV][i][j][k])/grid.dLocalGridOld[grid.nDTheta][0][nJCen+1][0];
        }
        else{//moving in a positive direction
          dA2UpWindGrad=(grid.dLocalGridOld[grid.nV][i][j][k]
            -grid.dLocalGridOld[grid.nV][i][j-1][k])/grid.dLocalGridOld[grid.nDTheta][0][nJCen][0];
        }
        dA2=grid.dLocalGridOld[grid.nV][i][j][k]/dR_i_n*((1.0-parameters.dDonorFrac)*dA2CenGrad
          +parameters.dDonorFrac*dA2UpWindGrad);
        
        //calculate S2
        dS2=(dP_ijp1k_n-dP_ijk_n)/(dDTheta_jp1half*dRho_ijp1halfk_n*dR_i_n);
        
        //calculate A3
        dA3CenGrad=(dV_ijp1halfkp1half_nm1half-dV_ijp1halfkm1half_nm1half)
          /grid.dLocalGridOld[grid.nDPhi][0][0][k];
        dA3UpWindGrad=0.0;
        if(dW_ijp1halfk_nm1half<0.0){//moving in a negative direction
          dA3UpWindGrad=(grid.dLocalGridOld[grid.nV][i][j][k+1]
            -grid.dLocalGridOld[grid.nV][i][j][k])/(grid.dLocalGridOld[grid.nDPhi][0][0][k+1]
            +grid.dLocalGridOld[grid.nDPhi][0][0][k])*2.0;
        }
        else{//moving in a positive direction
          dA3UpWindGrad=(grid.dLocalGridOld[grid.nV][i][j][k]
            -grid.dLocalGridOld[grid.nV][i][j][k-1])/(grid.dLocalGridOld[grid.nDPhi][0][0][k]
            +grid.dLocalGridOld[grid.nDPhi][0][0][k-1])*2.0;
        }
        dA3=dW_ijp1halfk_nm1half*((1.0-parameters.dDonorFrac)*dA3CenGrad
          +parameters.dDonorFrac*dA3UpWindGrad)/(dR_i_n
          *grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][j][0]);
        
        //calculate S3
        dS3=-1.0*dW_ijp1halfk_nm1half*dW_ijp1halfk_nm1half
          *grid.dLocalGridOld[grid.nCotThetaIJp1halfK][0][j][0]/dR_i_n;
        
        //calculate Tau_rt_ip1halfjp1halfk_n
        dTau_rt_ip1halfjp1halfk_n=dEddyVisc_ip1halfjp1halfk_n*(4.0*parameters.dPi*dRCu_ip1half
          *dRhoAve_ip1half_n*(dV_R_ip1jp1halfk_n-dV_R_ijp1halfk_n)/dDM_ip1half+1.0
          /grid.dLocalGridOld[grid.nR][nIInt][0][0]*((grid.dLocalGridOld[grid.nU][nIInt][nJCen+1][k]
          -grid.dLocalGridOld[grid.nU0][nIInt][0][0])-(grid.dLocalGridOld[grid.nU][nIInt][nJCen][k]
          -grid.dLocalGridOld[grid.nU0][nIInt][0][0]))/dDTheta_jp1half);
        
        //calculate Tau_rt_im1halfjp1halfk_n
        dTau_rt_im1halfjp1halfk_n=dEddyVisc_im1halfjp1halfk_n*(4.0*parameters.dPi*dRCu_im1half
          *dRhoAve_im1half_n*(dV_R_ijp1halfk_n-dV_R_im1jp1halfk_n)/dDM_im1half+1.0
          /grid.dLocalGridOld[grid.nR][nIInt-1][0][0]
          *((grid.dLocalGridOld[grid.nU][nIInt-1][nJCen+1][k]
          -grid.dLocalGridOld[grid.nU0][nIInt-1][0][0])
          -(grid.dLocalGridOld[grid.nU][nIInt-1][nJCen][k]
          -grid.dLocalGridOld[grid.nU0][nIInt-1][0][0]))/dDTheta_jp1half);
        
        //calculate DivU_ijp1k_n
        dDivU_ijp1k_n=4.0*parameters.dPi*grid.dLocalGridOld[grid.nDenAve][i][0][0]
          *(dRSqUmU0_ip1halfjp1k_n-dRSqUmU0_im1halfjp1k_n)/grid.dLocalGridOld[grid.nDM][i][0][0]
          +((grid.dLocalGridOld[grid.nV][i][j+1][k]
          *grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][j+1][0]
          -grid.dLocalGridOld[grid.nV][i][j][k]
          *grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][j][0])
          /grid.dLocalGridOld[grid.nDTheta][0][nJCen+1][0]
          +(grid.dLocalGridOld[grid.nW][i][nJCen+1][nKInt]
          -grid.dLocalGridOld[grid.nW][i][nJCen+1][nKInt-1])
          /(grid.dLocalGridOld[grid.nDPhi][0][0][k]))
          /(dR_i_n*grid.dLocalGridOld[grid.nSinThetaIJK][0][nJCen+1][0]);
        
        //calculate DivU_ijk_n
        dDivU_ijk_n=4.0*parameters.dPi*grid.dLocalGridOld[grid.nDenAve][i][0][0]
          *(dRSqUmU0_ip1halfjk_n-dRSqUmU0_im1halfjk_n)/grid.dLocalGridOld[grid.nDM][i][0][0]
          +((grid.dLocalGridOld[grid.nV][i][j][k]
          *grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][j][0]
          -grid.dLocalGridOld[grid.nV][i][j-1][k]
          *grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][j-1][0])
          /grid.dLocalGridOld[grid.nDTheta][0][nJCen][0]
          +(grid.dLocalGridOld[grid.nW][i][nJCen][nKInt]
          -grid.dLocalGridOld[grid.nW][i][nJCen][nKInt-1])
          /(grid.dLocalGridOld[grid.nDPhi][0][0][k]))
          /(dR_i_n*grid.dLocalGridOld[grid.nSinThetaIJK][0][nJCen][0]);
        
        //calculate Tau_tt_ijp1k_n
        dTau_tt_ijp1k_n=2.0*grid.dLocalGridOld[grid.nEddyVisc][i][nJCen+1][k]
          *((grid.dLocalGridOld[grid.nV][i][j+1][k]-grid.dLocalGridOld[grid.nV][i][j][k])
          /(dR_i_n*grid.dLocalGridOld[grid.nDTheta][0][nJCen+1][0])
          +(dU_ijp1k_nm1half-dU0_i_nm1half)/dR_i_n-0.333333333333333*dDivU_ijp1k_n);
        
        //calculate Tau_tt_ijk_n
        dTau_tt_ijk_n=2.0*grid.dLocalGridOld[grid.nEddyVisc][i][nJCen][k]
          *((grid.dLocalGridOld[grid.nV][i][j][k]-grid.dLocalGridOld[grid.nV][i][j-1][k])
          /(grid.dLocalGridOld[grid.nDTheta][0][nJCen][0]*dR_i_n)
          +(dU_ijk_nm1half-dU0_i_nm1half)/dR_i_n-0.333333333333333*dDivU_ijk_n);
        
        //calculate dTau_tp_ijp1halfkp1half_n
        dTau_tp_ijp1halfkp1half_n=dEddyVisc_ijp1halfkp1half_n
          *(grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][j][0]*(dW_SinTheta_ijp1kp1half_n
          -dW_SinTheta_ijkp1half_n)/(dR_i_n*dDTheta_jp1half)+(grid.dLocalGridOld[grid.nV][i][j][k+1]
          -grid.dLocalGridOld[grid.nV][i][j][k])/(dR_i_n
          *grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][j][0]*dDPhi_kp1half));
        
        //calculate dTau_tp_ijp1halfmp1half_n
        dTau_tp_ijp1halfkm1half_n=dEddyVisc_ijp1halfkm1half_n
          *(grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][j][0]*(dW_SinTheta_ijp1km1half_n
          -dW_SinTheta_ijkm1half_n)/(dR_i_n*dDTheta_jp1half)+(grid.dLocalGridOld[grid.nV][i][j][k]
          -grid.dLocalGridOld[grid.nV][i][j][k-1])/(dR_i_n
          *grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][j][0]*dDPhi_km1half));
        
        //calculate TA1
        dTA1=(dTau_rt_ip1halfjp1halfk_n-dTau_rt_im1halfjp1halfk_n)
          /(grid.dLocalGridOld[grid.nDM][i][0][0]*dRho_ijp1halfk_n);
        
        //calculate TS1
        dTS1=3.0*dEddyVisc_ijp1halfk_n*(dV_R_ip1halfjp1halfk_n-dV_R_im1halfjp1halfk_n)
          /(grid.dLocalGridOld[grid.nDM][i][0][0]*dRho_ijp1halfk_n);
        
        //calculate TA2
        dTA2=(dTau_tt_ijp1k_n-dTau_tt_ijk_n)/(dRho_ijp1halfk_n*dR_i_n*dDTheta_jp1half);
        
        //calculate TS2
        dTS2=(2.0*grid.dLocalGridOld[grid.nCotThetaIJp1halfK][0][j][0]*(dV_ijp1k_nm1half
          -dV_ijk_nm1half)+3.0*((dU_ijp1k_nm1half-dU0_i_nm1half)
          -(dU_ijk_nm1half-dU0_i_nm1half)))/(dR_i_n*dDTheta_jp1half);
        
        //calculate dTA3
        dTA3=(dTau_tp_ijp1halfkp1half_n-dTau_tp_ijp1halfkm1half_n)/(dRho_ijp1halfk_n*dR_i_n
          *grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][j][0]
          *grid.dLocalGridOld[grid.nDPhi][0][0][k]);
        
        //calculate dTS3
        dTS3=2.0*grid.dLocalGridOld[grid.nCotThetaIJp1halfK][0][j][0]*(dW_ijp1halfkp1half_nm1half
          -dW_ijp1halfkm1half_nm1half)/(dR_i_n*grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][j][0]
          *grid.dLocalGridOld[grid.nDPhi][0][0][k]);
        
        //calculate TS4
        dTS4=2.0*grid.dLocalGridOld[grid.nV][i][j][k]
          *grid.dLocalGridOld[grid.nCotThetaIJp1halfK][0][j][0]
          *grid.dLocalGridOld[grid.nCotThetaIJp1halfK][0][j][0]/dR_i_n;
        
        dEddyViscosityTerms=-4.0*dRSq_i_n*grid.dLocalGridOld[grid.nDenAve][i][0][0]*(dTA1+dTS1)
          -dTA2-dTA3-dEddyVisc_ijp1halfk_n/(dRho_ijp1halfk_n*dR_i_n)*(dTS2-dTS3-dTS4);
        //dEddyViscosityTerms=0.0;
        
        //calculate new velocity
        grid.dLocalGridNew[grid.nV][i][j][k]=grid.dLocalGridOld[grid.nV][i][j][k]
          -time.dDeltat_n*(4.0*dRSq_i_n*grid.dLocalGridOld[grid.nDenAve][i][0][0]*(dA1)
          +dS1+dA2+dS2+dA3+dS3+dEddyViscosityTerms);
      }
    }
  }
}
void calNewW_RTP(Grid &grid,Parameters &parameters,Time &time,ProcTop &procTop){
  int i;
  int j;
  int k;
  int nIInt;
  int nJInt;
  int nKCen;
  double dU_ijkp1half_nm1half;
  double dV_ijkp1half_nm1half;
  double dU0i_nm1half;
  double dR_i_n;
  double dW_ijkp1half_nm1half;
  double dW_ijp1halfkp1half_nm1half;
  double dW_ijm1halfkp1half_nm1half;
  double dW_ip1halfjkp1half_nm1half;
  double dW_im1halfjkp1half_nm1half;
  double dW_ijkp1_nm1half;
  double dW_ijk_nm1half;
  double dDeltaPhi_kp1half;
  double dRho_ijkp1half_n;
  double dA1CenGrad;
  double dA1UpWindGrad;
  double dUmU0_ijkp1half_nm1half;
  double dA1;
  double dS1;
  double dA2CenGrad;
  double dA2UpWindGrad;
  double dA2;
  double dS2;
  double dA3CenGrad;
  double dA3UpWindGrad;
  double dA3;
  double dP_ijkp1_n;
  double dP_ijk_n;
  double dS3;
  
  //calculate new w
  for(i=grid.nStartUpdateExplicit[grid.nW][0];i<grid.nEndUpdateExplicit[grid.nW][0];i++){
    
    //calculate j of interface quantities
    nIInt=i+grid.nCenIntOffset[0];
    dU0i_nm1half=(grid.dLocalGridOld[grid.nU0][nIInt][0][0]
      +grid.dLocalGridOld[grid.nU0][nIInt-1][0][0])*0.5;
    dR_i_n=(grid.dLocalGridOld[grid.nR][nIInt][0][0]+grid.dLocalGridOld[grid.nR][nIInt-1][0][0])
      *0.5;
    
    for(j=grid.nStartUpdateExplicit[grid.nW][1];j<grid.nEndUpdateExplicit[grid.nW][1];j++){
      
      //calculate j of centered quantities
      nJInt=j+grid.nCenIntOffset[1];
      
      for(k=grid.nStartUpdateExplicit[grid.nW][2];k<grid.nEndUpdateExplicit[grid.nW][2];k++){
        
        //calculate k of interface quantities
        nKCen=k-grid.nCenIntOffset[2];
        
        //Calculate interpolated quantities
        dU_ijkp1half_nm1half=(grid.dLocalGridOld[grid.nU][nIInt][j][nKCen+1]
          +grid.dLocalGridOld[grid.nU][nIInt-1][j][nKCen+1]
          +grid.dLocalGridOld[grid.nU][nIInt][j][nKCen]
          +grid.dLocalGridOld[grid.nU][nIInt-1][j][nKCen])*0.25;
        dV_ijkp1half_nm1half=(grid.dLocalGridOld[grid.nV][i][nJInt][nKCen+1]
          +grid.dLocalGridOld[grid.nV][i][nJInt][nKCen]
          +grid.dLocalGridOld[grid.nV][i][nJInt-1][nKCen+1]
          +grid.dLocalGridOld[grid.nV][i][nJInt-1][nKCen])*0.25;
        dW_ijkp1half_nm1half=grid.dLocalGridOld[grid.nW][i][j][k];
        dW_ijp1halfkp1half_nm1half=(grid.dLocalGridOld[grid.nW][i][j+1][k]
          +grid.dLocalGridOld[grid.nW][i][j][k])*0.5;
        dW_ijm1halfkp1half_nm1half=(grid.dLocalGridOld[grid.nW][i][j-1][k]
          +grid.dLocalGridOld[grid.nW][i][j][k])*0.5;
        dW_ip1halfjkp1half_nm1half=(grid.dLocalGridOld[grid.nW][i+1][j][k]
          +grid.dLocalGridOld[grid.nW][i][j][k])*0.5;
        dW_im1halfjkp1half_nm1half=(grid.dLocalGridOld[grid.nW][i-1][j][k]
          +grid.dLocalGridOld[grid.nW][i][j][k])*0.5;
        dW_ijkp1_nm1half=(grid.dLocalGridOld[grid.nW][i][j][k]
          +grid.dLocalGridOld[grid.nW][i][j][k+1])*0.5;
        dW_ijk_nm1half=(grid.dLocalGridOld[grid.nW][i][j][k]
          +grid.dLocalGridOld[grid.nW][i][j][k-1])*0.5;
        dDeltaPhi_kp1half=(grid.dLocalGridOld[grid.nDPhi][0][0][nKCen]
          +grid.dLocalGridOld[grid.nDPhi][0][0][nKCen+1])*0.5;
        dRho_ijkp1half_n=(grid.dLocalGridOld[grid.nD][i][j][nKCen]
          +grid.dLocalGridOld[grid.nD][i][j][nKCen+1])*0.5;
        dP_ijkp1_n=grid.dLocalGridOld[grid.nP][i][j][nKCen+1]
          +grid.dLocalGridOld[grid.nQ0][i][j][nKCen+1]+grid.dLocalGridOld[grid.nQ1][i][j][nKCen+1]
          +grid.dLocalGridOld[grid.nQ2][i][j][nKCen+1];
        dP_ijk_n=grid.dLocalGridOld[grid.nP][i][j][nKCen]+grid.dLocalGridOld[grid.nQ0][i][j][nKCen]
          +grid.dLocalGridOld[grid.nQ1][i][j][nKCen]+grid.dLocalGridOld[grid.nQ2][i][j][nKCen];
        
        //calculate A1
        dA1CenGrad=(dW_ip1halfjkp1half_nm1half-dW_im1halfjkp1half_nm1half)
          /grid.dLocalGridOld[grid.nDM][i][0][0];
        dA1UpWindGrad=0.0;
        dUmU0_ijkp1half_nm1half=dU_ijkp1half_nm1half-dU0i_nm1half;
        if(dUmU0_ijkp1half_nm1half<0.0){//moving in a negative direction
          dA1UpWindGrad=(grid.dLocalGridOld[grid.nW][i+1][j][k]
            -grid.dLocalGridOld[grid.nW][i][j][k])/(grid.dLocalGridOld[grid.nDM][i][0][0]
            +grid.dLocalGridOld[grid.nDM][i+1][0][0])*2.0;
        }
        else{//moving in a positive direction
          dA1UpWindGrad=(grid.dLocalGridOld[grid.nW][i][j][k]
            -grid.dLocalGridOld[grid.nW][i-1][j][k])/(grid.dLocalGridOld[grid.nDM][i][0][0]
            +grid.dLocalGridOld[grid.nDM][i-1][0][0])*2.0;
        }
        dA1=4.0*parameters.dPi*dR_i_n*dR_i_n*grid.dLocalGridOld[grid.nDenAve][i][0][0]
          *dUmU0_ijkp1half_nm1half*((1.0-parameters.dDonorFrac)*dA1CenGrad+parameters.dDonorFrac
          *dA1UpWindGrad);
        
        //calculate S1
        dS1=dU_ijkp1half_nm1half*dW_ijkp1half_nm1half/dR_i_n;
        
        //calculate dA2
        dA2CenGrad=(dW_ijp1halfkp1half_nm1half-dW_ijm1halfkp1half_nm1half)
          /grid.dLocalGridOld[grid.nDTheta][0][j][0];
        dA2UpWindGrad=0.0;
        if(dV_ijkp1half_nm1half<0.0){//moning in a negative direction
          dA2UpWindGrad=(grid.dLocalGridOld[grid.nW][i][j+1][nKCen]
            -grid.dLocalGridOld[grid.nW][i][j][k])/(grid.dLocalGridOld[grid.nDTheta][0][j+1][0]
            +grid.dLocalGridOld[grid.nDTheta][0][j][0])*2.0;
        }
        else{//moving in a positive direction
          dA2UpWindGrad=(grid.dLocalGridOld[grid.nW][i][j][k]
            -grid.dLocalGridOld[grid.nW][i][j-1][k])/(grid.dLocalGridOld[grid.nDTheta][0][j-1][0]
            +grid.dLocalGridOld[grid.nDTheta][0][j][0])*2.0;
        }
        dA2=dV_ijkp1half_nm1half/dR_i_n*((1.0-parameters.dDonorFrac)*dA2CenGrad
          +parameters.dDonorFrac*dA2UpWindGrad);
        
        //calculate S2
        dS2=dV_ijkp1half_nm1half*grid.dLocalGridOld[grid.nW][i][j][k]
          *grid.dLocalGridOld[grid.nCotThetaIJK][0][j][0]/dR_i_n;
        
        //calculate A3
        dA3CenGrad=(dW_ijkp1_nm1half-dW_ijk_nm1half)
          /dDeltaPhi_kp1half;
        dA3UpWindGrad=0.0;
        if(dW_ijkp1half_nm1half<0.0){//moving in a negative direction
          dA3UpWindGrad=(grid.dLocalGridOld[grid.nW][i][j][k+1]
            -grid.dLocalGridOld[grid.nW][i][j][k])/grid.dLocalGridOld[grid.nDPhi][0][0][nKCen+1];
        }
        else{//moving in a positive direction
          dA3UpWindGrad=(grid.dLocalGridOld[grid.nW][i][j][k]
            -grid.dLocalGridOld[grid.nW][i][j][k-1])/grid.dLocalGridOld[grid.nDPhi][0][0][nKCen];
        }
        dA3=dW_ijkp1half_nm1half*((1.0-parameters.dDonorFrac)*dA3CenGrad+parameters.dDonorFrac
          *dA3UpWindGrad)/(dR_i_n*grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]);
        
        //calculate S3
        dS3=(dP_ijkp1_n-dP_ijk_n)/(dRho_ijkp1half_n*dR_i_n
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]*dDeltaPhi_kp1half);
        
        //calculate new velocity
        grid.dLocalGridNew[grid.nW][i][j][k]=grid.dLocalGridOld[grid.nW][i][j][k]
          -time.dDeltat_n*(dA1+dS1+dA2+dS2+dA3+dS3);
      }
    }
  }
  
  //ghost region 0, outter most ghost region in x1 direction
  for(i=grid.nStartGhostUpdateExplicit[grid.nV][0][0];
    i<grid.nEndGhostUpdateExplicit[grid.nV][0][0];i++){
    
    //calculate j of interface quantities
    nIInt=i+grid.nCenIntOffset[0];
    dU0i_nm1half=(grid.dLocalGridOld[grid.nU0][nIInt][0][0]
      +grid.dLocalGridOld[grid.nU0][nIInt-1][0][0])*0.5;
    dR_i_n=(grid.dLocalGridOld[grid.nR][nIInt][0][0]+grid.dLocalGridOld[grid.nR][nIInt-1][0][0])
      *0.5;
    
    for(j=grid.nStartGhostUpdateExplicit[grid.nV][0][1];
      j<grid.nEndGhostUpdateExplicit[grid.nV][0][1];j++){
      
      //calculate j of centered quantities
      nJInt=j+grid.nCenIntOffset[1];
      
      for(k=grid.nStartGhostUpdateExplicit[grid.nV][0][2];
        k<grid.nEndGhostUpdateExplicit[grid.nV][0][2];k++){
        
        //calculate k of interface quantities
        nKCen=k-grid.nCenIntOffset[2];
        
        //Calculate interpolated quantities
        dU_ijkp1half_nm1half=(grid.dLocalGridOld[grid.nU][nIInt][j][nKCen+1]
          +grid.dLocalGridOld[grid.nU][nIInt-1][j][nKCen+1]
          +grid.dLocalGridOld[grid.nU][nIInt][j][nKCen]
          +grid.dLocalGridOld[grid.nU][nIInt-1][j][nKCen])*0.25;
        dV_ijkp1half_nm1half=(grid.dLocalGridOld[grid.nV][i][nJInt][nKCen+1]
          +grid.dLocalGridOld[grid.nV][i][nJInt][nKCen]
          +grid.dLocalGridOld[grid.nV][i][nJInt-1][nKCen+1]
          +grid.dLocalGridOld[grid.nV][i][nJInt-1][nKCen])*0.25;
        dW_ijkp1half_nm1half=grid.dLocalGridOld[grid.nW][i][j][k];
        dW_ijp1halfkp1half_nm1half=(grid.dLocalGridOld[grid.nW][i][j+1][k]
          +grid.dLocalGridOld[grid.nW][i][j][k])*0.5;
        dW_ijm1halfkp1half_nm1half=(grid.dLocalGridOld[grid.nW][i][j-1][k]
          +grid.dLocalGridOld[grid.nW][i][j][k])*0.5;
        dW_ip1halfjkp1half_nm1half=grid.dLocalGridOld[grid.nW][i][j][k];/**\BC missing 
          grid.dLocalGridOld[grid.nW][i+1][j][k] assuming that the phi velocity at the outter most 
          interface is the same as the phi velocity in the center of the zone.*/
        dW_im1halfjkp1half_nm1half=(grid.dLocalGridOld[grid.nW][i-1][j][k]
          +grid.dLocalGridOld[grid.nW][i][j][k])*0.5;
        dW_ijkp1_nm1half=(grid.dLocalGridOld[grid.nW][i][j][k]
          +grid.dLocalGridOld[grid.nW][i][j][k+1])*0.5;
        dW_ijk_nm1half=(grid.dLocalGridOld[grid.nW][i][j][k]
          +grid.dLocalGridOld[grid.nW][i][j][k-1])*0.5;
        dDeltaPhi_kp1half=(grid.dLocalGridOld[grid.nDPhi][0][0][nKCen]
          +grid.dLocalGridOld[grid.nDPhi][0][0][nKCen+1])*0.5;
        dRho_ijkp1half_n=(grid.dLocalGridOld[grid.nD][i][j][nKCen]
          +grid.dLocalGridOld[grid.nD][i][j][nKCen+1])*0.5;
        dP_ijkp1_n=grid.dLocalGridOld[grid.nP][i][j][nKCen+1]
          +grid.dLocalGridOld[grid.nQ0][i][j][nKCen+1]+grid.dLocalGridOld[grid.nQ1][i][j][nKCen+1]
          +grid.dLocalGridOld[grid.nQ2][i][j][nKCen+1];
        dP_ijk_n=grid.dLocalGridOld[grid.nP][i][j][nKCen]+grid.dLocalGridOld[grid.nQ0][i][j][nKCen]
          +grid.dLocalGridOld[grid.nQ1][i][j][nKCen]+grid.dLocalGridOld[grid.nQ2][i][j][nKCen];
          
        //calculate A1
        dA1CenGrad=(dW_ip1halfjkp1half_nm1half-dW_im1halfjkp1half_nm1half)
          /grid.dLocalGridOld[grid.nDM][i][0][0];
        dA1UpWindGrad=0.0;
        dUmU0_ijkp1half_nm1half=dU_ijkp1half_nm1half-dU0i_nm1half;
        if(dUmU0_ijkp1half_nm1half<0.0){//moving in a negative direction
          dA1UpWindGrad=dA1CenGrad;/**\BC missing grid.dLocalGridOld[grid.nW][i+1][j][k] in outter 
            most zone. This is needed to  calculate the upwind gradient for donnor cell. The 
            centered gradient is used instead when moving in the negative direction.*/
        }
        else{//moving in a positive direction
          dA1UpWindGrad=(grid.dLocalGridOld[grid.nW][i][j][k]
            -grid.dLocalGridOld[grid.nW][i-1][j][k])/(grid.dLocalGridOld[grid.nDM][i][0][0]
            +grid.dLocalGridOld[grid.nDM][i-1][0][0])*2.0;
        }
        dA1=4.0*parameters.dPi*dR_i_n*dR_i_n*grid.dLocalGridOld[grid.nDenAve][i][0][0]
          *dUmU0_ijkp1half_nm1half*((1.0-parameters.dDonorFrac)*dA1CenGrad+parameters.dDonorFrac
          *dA1UpWindGrad);
        
        //calculate S1
        dS1=dU_ijkp1half_nm1half*dW_ijkp1half_nm1half/dR_i_n;
        
        //calculate dA2
        dA2CenGrad=(dW_ijp1halfkp1half_nm1half-dW_ijm1halfkp1half_nm1half)
          /grid.dLocalGridOld[grid.nDTheta][0][j][0];
        dA2UpWindGrad=0.0;
        if(dV_ijkp1half_nm1half<0.0){//moning in a negative direction
          dA2UpWindGrad=(grid.dLocalGridOld[grid.nW][i][j+1][k]
            -grid.dLocalGridOld[grid.nW][i][j][k])/(grid.dLocalGridOld[grid.nDTheta][0][j+1][0]
            +grid.dLocalGridOld[grid.nDTheta][0][j][0])*2.0;
        }
        else{//moving in a positive direction
          dA2UpWindGrad=(grid.dLocalGridOld[grid.nW][i][j][k]
            -grid.dLocalGridOld[grid.nW][i][j-1][k])/(grid.dLocalGridOld[grid.nDTheta][0][j-1][0]
            +grid.dLocalGridOld[grid.nDTheta][0][j][0])*2.0;
        }
        dA2=dV_ijkp1half_nm1half/dR_i_n*((1.0-parameters.dDonorFrac)*dA2CenGrad
          +parameters.dDonorFrac*dA2UpWindGrad);
        
        //calculate S2
        dS2=dV_ijkp1half_nm1half*grid.dLocalGridOld[grid.nW][i][j][k]
          *grid.dLocalGridOld[grid.nCotThetaIJK][0][j][0]/dR_i_n;
        
        //calculate A3
        dA3CenGrad=(dW_ijkp1_nm1half-dW_ijk_nm1half)/dDeltaPhi_kp1half;
        dA3UpWindGrad=0.0;
        if(dW_ijkp1half_nm1half<0.0){//moving in a negative direction
          dA3UpWindGrad=(grid.dLocalGridOld[grid.nW][i][j][k+1]
            -grid.dLocalGridOld[grid.nW][i][j][k])/grid.dLocalGridOld[grid.nDPhi][0][0][nKCen+1];
        }
        else{//moving in a positive direction
          dA3UpWindGrad=(grid.dLocalGridOld[grid.nW][i][j][k]
            -grid.dLocalGridOld[grid.nW][i][j][k-1])/grid.dLocalGridOld[grid.nDPhi][0][0][nKCen];
        }
        dA3=dW_ijkp1half_nm1half*((1.0-parameters.dDonorFrac)*dA3CenGrad+parameters.dDonorFrac
          *dA3UpWindGrad)/(dR_i_n*grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]);
        
        //calculate S3
        dS3=(dP_ijkp1_n-dP_ijk_n)/(dRho_ijkp1half_n*dR_i_n
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]*dDeltaPhi_kp1half);
        
        //calculate new velocity
        grid.dLocalGridNew[grid.nW][i][j][k]=grid.dLocalGridOld[grid.nW][i][j][k]
          -time.dDeltat_n*(dA1+dS1+dA2+dS2+dA3+dS3);
      }
    }
  }
  
  #if SEDOV==1
  
    //ghost region 1, inner ghost region in x1 direction
    for(i=grid.nStartGhostUpdateExplicit[grid.nV][1][0];
      i<grid.nEndGhostUpdateExplicit[grid.nV][1][0];i++){
      
      //calculate j of interface quantities
      nIInt=i+grid.nCenIntOffset[0];
      dR_i_n=(grid.dLocalGridOld[grid.nR][nIInt][0][0]+grid.dLocalGridOld[grid.nR][nIInt-1][0][0])
        *0.5;
      dU0i_nm1half=(grid.dLocalGridOld[grid.nU0][nIInt][0][0]
        +grid.dLocalGridOld[grid.nU0][nIInt-1][0][0])*0.5;
      for(j=grid.nStartGhostUpdateExplicit[grid.nV][1][1];
        j<grid.nEndGhostUpdateExplicit[grid.nV][1][1];j++){
        
        //calculate j of centered quantities
        nJInt=j+grid.nCenIntOffset[1];
        
        for(k=grid.nStartGhostUpdateExplicit[grid.nV][1][2];
          k<grid.nEndGhostUpdateExplicit[grid.nV][1][2];k++){
        
        //calculate k of interface quantities
        nKCen=k-grid.nCenIntOffset[2];
        
        //Calculate interpolated quantities
        dU_ijkp1half_nm1half=(grid.dLocalGridOld[grid.nU][nIInt][j][nKCen+1]
          +grid.dLocalGridOld[grid.nU][nIInt-1][j][nKCen+1]
          +grid.dLocalGridOld[grid.nU][nIInt][j][nKCen]
          +grid.dLocalGridOld[grid.nU][nIInt-1][j][nKCen])*0.25;
        ddV_ijkp1half_nm1half=(grid.dLocalGridOld[grid.nV][i][nJInt][nKCen+1]
          +grid.dLocalGridOld[grid.nV][i][nJInt][nKCen]
          +grid.dLocalGridOld[grid.nV][i][nJInt-1][nKCen+1]
          +grid.dLocalGridOld[grid.nV][i][nJInt-1][nKCen])*0.25;
        dW_ijkp1half_nm1half=grid.dLocalGridOld[grid.nW][i][j][k];
        dW_ijp1halfkp1half_nm1half=(grid.dLocalGridOld[grid.nW][i][j+1][k]
          +grid.dLocalGridOld[grid.nW][i][j][k])*0.5;
        dW_ijm1halfkp1half_nm1half=(grid.dLocalGridOld[grid.nW][i][j-1][k]
          +grid.dLocalGridOld[grid.nW][i][j][k])*0.5;
        dW_ip1halfjkp1half_nm1half=(grid.dLocalGridOld[grid.nW][i+1][j][k]
          +grid.dLocalGridOld[grid.nW][i][j][k])*0.5;
        dW_im1halfjkp1half_nm1half=(grid.dLocalGridOld[grid.nW][i-1][j][k]
          +grid.dLocalGridOld[grid.nW][i][j][k])*0.5;
        dW_ijkp1_nm1half=(grid.dLocalGridOld[grid.nW][i][j][k]
          +grid.dLocalGridOld[grid.nW][i][j][k+1])*0.5;
        dW_ijk_nm1half=(grid.dLocalGridOld[grid.nW][i][j][k]
          +grid.dLocalGridOld[grid.nW][i][j][k-1])*0.5;
        dDeltaPhi_kp1half=(grid.dLocalGridOld[grid.nDPhi][0][0][nKCen]
          +grid.dLocalGridOld[grid.nDPhi][0][0][nKCen+1])*0.5;
        dRho_ijkp1half_n=(grid.dLocalGridOld[grid.nD][i][j][nKCen]
          +grid.dLocalGridOld[grid.nD][i][j][nKCen+1])*0.5;
        dP_ijkp1_n=grid.dLocalGridOld[grid.nP][i][j][nKCen+1]
          +grid.dLocalGridOld[grid.nQ0][i][j][nKCen+1]+grid.dLocalGridOld[grid.nQ1][i][j][nKCen+1]
          +grid.dLocalGridOld[grid.nQ2][i][j][nKCen+1];
        dP_ijk_n=grid.dLocalGridOld[grid.nP][i][j][nKCen]+grid.dLocalGridOld[grid.nQ0][i][j][nKCen]
          +grid.dLocalGridOld[grid.nQ1][i][j][nKCen]+grid.dLocalGridOld[grid.nQ2][i][j][nKCen];
        
        //calculate A1
        dA1CenGrad=(dW_ip1halfjkp1half_nm1half-dW_im1halfjkp1half_nm1half)
          /grid.dLocalGridOld[grid.nDM][i][0][0];
        dA1UpWindGrad=0.0;
        dUmU0_ijkp1half_nm1half=dU_ijkp1half_nm1half-dU0i_nm1half;
        if(dUmU0_ijkp1half_nm1half<0.0){//moving in a negative direction
          dA1UpWindGrad=(grid.dLocalGridOld[grid.nW][i+1][j][k]
            -grid.dLocalGridOld[grid.nW][i][j][k])/(grid.dLocalGridOld[grid.nDM][i][0][0]
            +grid.dLocalGridOld[grid.nDM][i+1][0][0])*2.0;
        }
        else{//moving in a positive direction
          dA1UpWindGrad=(grid.dLocalGridOld[grid.nW][i][j][k]
            -grid.dLocalGridOld[grid.nW][i-1][j][k])/(grid.dLocalGridOld[grid.nDM][i][0][0]
            +grid.dLocalGridOld[grid.nDM][i-1][0][0])*2.0;
        }
        dA1=4.0*parameters.dPi*dR_i_n*dR_i_n*grid.dLocalGridOld[grid.nDenAve][i][0][0]
          *dUmU0_ijkp1half_nm1half*((1.0-parameters.dDonorFrac)*dA1CenGrad+parameters.dDonorFrac
          *dA1UpWindGrad);
        
        //calculate S1
        dS1=dU_ijkp1half_nm1half*dW_ijkp1half_nm1half/dR_i_n;
        
        //calculate dA2
        dA2CenGrad=(dW_ijp1halfkp1half_nm1half-dW_ijm1halfkp1half_nm1half)
          /grid.dLocalGridOld[grid.nDTheta][0][j][0];
        dA2UpWindGrad=0.0;
        if(dV_ijkp1half_nm1half<0.0){//moning in a negative direction
          dA2UpWindGrad=(grid.dLocalGridOld[grid.nW][i][j+1][nKCen]
            -grid.dLocalGridOld[grid.nW][i][j][k])/(grid.dLocalGridOld[grid.nDTheta][0][j+1][0]
            +grid.dLocalGridOld[grid.nDTheta][0][j][0])*2.0;
        }
        else{//moving in a positive direction
          dA2UpWindGrad=(grid.dLocalGridOld[grid.nW][i][j][k]
            -grid.dLocalGridOld[grid.nW][i][j-1][k])/(grid.dLocalGridOld[grid.nDTheta][0][j-1][0]
            +grid.dLocalGridOld[grid.nDTheta][0][j][0])*2.0;
        }
        dA2=dV_ijkp1half_nm1half/dR_i_n*((1.0-parameters.dDonorFrac)*dA2CenGrad
          +parameters.dDonorFrac*dA2UpWindGrad);
        
        //calculate S2
        dS2=dV_ijkp1half_nm1half*grid.dLocalGridOld[grid.nW][i][j][k]
          *grid.dLocalGridOld[grid.nCotThetaIJK][0][j][0]/dR_i_n;
        
        //calculate A3
        dA3CenGrad=(dW_ijkp1_nm1half-dW_ijk_nm1half)/dDeltaPhi_kp1half;
        dA3UpWindGrad=0.0;
        if(dW_ijkp1half_nm1half<0.0){//moving in a negative direction
          dA3UpWindGrad=(grid.dLocalGridOld[grid.nW][i][j][k+1]-grid.dLocalGridOld[grid.nW][i][j][k])
            /grid.dLocalGridOld[grid.nDPhi][0][0][nKCen+1];
        }
        else{//moving in a positive direction
          dA3UpWindGrad=(grid.dLocalGridOld[grid.nW][i][j][k]-grid.dLocalGridOld[grid.nW][i][j][k-1])
            /grid.dLocalGridOld[grid.nDPhi][0][0][nKCen];
        }
        dA3=dW_ijkp1half_nm1half*((1.0-parameters.dDonorFrac)*dA3CenGrad+parameters.dDonorFrac
          *dA3UpWindGrad)/(dR_i_n*grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]);
        
        //calculate S3
        dS3=(dP_ijkp1_n-dP_ijk_n)/(dRho_ijkp1half_n*dR_i_n
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]*dDeltaPhi_kp1half);
        
        //calculate new velocity
        grid.dLocalGridNew[grid.nW][i][j][k]=grid.dLocalGridOld[grid.nW][i][j][k]
          -time.dDeltat_n*(dA1+dS1+dA2+dS2+dA3+dS3);
        }
      }
    }
  
  #endif
}
void calNewW_RTP_LES(Grid &grid,Parameters &parameters,Time &time,ProcTop &procTop){
  int i;
  int j;
  int k;
  int nIInt;
  int nJInt;
  int nKCen;
  double dR_i_n;
  double dR_ip1_n;
  double dR_im1_n;
  double dRSq_i_n;
  double dRSq_ip1half_n;
  double dRSq_im1half_n;
  double dR3_ip1half_n;
  double dR3_im1half_n;
  double dRhoAve_ip1half_n;
  double dRhoAve_im1half_n;
  double dDM_ip1half;
  double dDM_im1half;
  double dDTheta_jp1half;
  double dDTheta_jm1half;
  double dDPhi_kp1half;
  double dDPhi_km1half;
  double dU0_i_nm1half;
  double dU_ijkp1half_nm1half;
  double dU_ijkp1_nm1half;
  double dU_ijk_nm1half;
  double dV_ijk_nm1half;
  double dV_ijkp1_nm1half;
  double dV_ijkp1half_nm1half;
  double dV_ijm1halfkp1half_nm1half;
  double dV_ijm1halfkm1half_nm1half;
  double dW_ijkp1half_nm1half;
  double dW_ijp1halfkp1half_nm1half;
  double dW_ijm1halfkp1half_nm1half;
  double dW_ip1halfjkp1half_nm1half;
  double dW_im1halfjkp1half_nm1half;
  double dW_ijkp1_nm1half;
  double dW_ijk_nm1half;
  double dRho_ijkp1half_n;
  double dP_ijkp1_n;
  double dP_ijk_n;
  double dEddyVisc_ip1halfjkp1half_n;
  double dEddyVisc_im1halfjkp1half_n;
  double dEddyVisc_ijp1halfkp1half_n;
  double dEddyVisc_ijm1halfkp1half_n;
  double dEddyVisc_ijkp1half_n;
  double dUmU0_ijkp1half_nm1half;
  double d1_rhoDM_ijkp1half_n;
  double dRSq_UmU0_ip1halfjkp1_n;
  double dRSq_UmU0_im1halfjkp1_n;
  double dRSq_UmU0_ip1halfjk_n;
  double dRSq_UmU0_im1halfjk_n;
  double dV_SinTheta_ijp1halfkp1_n;
  double dV_SinTheta_ijm1halfkp1_n;
  double dV_SinTheta_ijp1halfk_n;
  double dV_SinTheta_ijm1halfk_n;
  double dW_R_ip1jkp1half_n;
  double dW_R_im1jkp1half_n;
  double dW_R_ijkp1half_n;
  double dW_R_ip1halfjkp1half_n;
  double dW_R_im1halfjkp1half_n;
  double dW_SinTheta_ijp1kp1half_n;
  double dW_SinTheta_ijm1kp1half_n;
  double dW_SinTheta_ijkp1half_n;
  double dW_SinTheta_ijp1halfkp1half_n;
  double dW_SinTheta_ijm1halfkp1half_n;
  double dRRho_ijkp1half_n;
  double dA1CenGrad;
  double dA1UpWindGrad;
  double dA1;
  double dS1;
  double dA2CenGrad;
  double dA2UpWindGrad;
  double dA2;
  double dS2;
  double dA3CenGrad;
  double dA3UpWindGrad;
  double dA3;
  double dS3;
  double dDivU_ijkp1_n;
  double dDivU_ijk_n;
  double dTau_rp_ip1halfjkp1half_n;
  double dTau_rp_im1halfjkp1half_n;
  double dTau_tp_ijp1halfkp1half_n;
  double dTau_tp_ijm1halfkp1half_n;
  double dTau_pp_ijkp1_n;
  double dTau_pp_ijk_n;
  double dTA1;
  double dTS1;
  double dTA2;
  double dTS2;
  double dTA3;
  double dTS3;
  double dEddyViscosityTerms;
  
  //calculate new w
  for(i=grid.nStartUpdateExplicit[grid.nW][0];i<grid.nEndUpdateExplicit[grid.nW][0];i++){
    
    //calculate j of interface quantities
    nIInt=i+grid.nCenIntOffset[0];
    dR_i_n=(grid.dLocalGridOld[grid.nR][nIInt][0][0]+grid.dLocalGridOld[grid.nR][nIInt-1][0][0])
      *0.5;
    dR_ip1_n=(grid.dLocalGridOld[grid.nR][nIInt+1][0][0]+grid.dLocalGridOld[grid.nR][nIInt][0][0])
      *0.5;
    dR_im1_n=(grid.dLocalGridOld[grid.nR][nIInt-1][0][0]+grid.dLocalGridOld[grid.nR][nIInt-2][0][0])
      *0.5;
    dRSq_i_n=dR_i_n*dR_i_n;
    dRSq_ip1half_n=grid.dLocalGridOld[grid.nR][nIInt][0][0]
      *grid.dLocalGridOld[grid.nR][nIInt][0][0];
    dRSq_im1half_n=grid.dLocalGridOld[grid.nR][nIInt-1][0][0]
      *grid.dLocalGridOld[grid.nR][nIInt-1][0][0];
    dR3_ip1half_n=dRSq_ip1half_n*grid.dLocalGridOld[grid.nR][nIInt][0][0];
    dR3_im1half_n=dRSq_im1half_n*grid.dLocalGridOld[grid.nR][nIInt-1][0][0];
    dU0_i_nm1half=(grid.dLocalGridOld[grid.nU0][nIInt][0][0]
      +grid.dLocalGridOld[grid.nU0][nIInt-1][0][0])*0.5;
    dRhoAve_ip1half_n=(grid.dLocalGridOld[grid.nDenAve][i+1][0][0]
      +grid.dLocalGridOld[grid.nDenAve][i][0][0])*0.5;
    dRhoAve_im1half_n=(grid.dLocalGridOld[grid.nDenAve][i][0][0]
      +grid.dLocalGridOld[grid.nDenAve][i-1][0][0])*0.5;
    dDM_ip1half=(grid.dLocalGridOld[grid.nDM][i+1][0][0]+grid.dLocalGridOld[grid.nDM][i][0][0])*0.5;
    dDM_im1half=(grid.dLocalGridOld[grid.nDM][i][0][0]+grid.dLocalGridOld[grid.nDM][i-1][0][0])*0.5;
    
    for(j=grid.nStartUpdateExplicit[grid.nW][1];j<grid.nEndUpdateExplicit[grid.nW][1];j++){
      
      //calculate j of centered quantities
      nJInt=j+grid.nCenIntOffset[1];
      dDTheta_jp1half=(grid.dLocalGridOld[grid.nDTheta][0][j+1][0]
        +grid.dLocalGridOld[grid.nDTheta][0][j][0])*0.5;
      dDTheta_jm1half=(grid.dLocalGridOld[grid.nDTheta][0][j][0]
        +grid.dLocalGridOld[grid.nDTheta][0][j-1][0])*0.5;
      
      for(k=grid.nStartUpdateExplicit[grid.nW][2];k<grid.nEndUpdateExplicit[grid.nW][2];k++){
        
        //calculate k of interface quantities
        nKCen=k-grid.nCenIntOffset[2];
        
        //Calculate interpolated quantities
        dDPhi_kp1half=(grid.dLocalGridOld[grid.nDPhi][0][0][nKCen]
          +grid.dLocalGridOld[grid.nDPhi][0][0][nKCen+1])*0.5;
        dDPhi_km1half=(grid.dLocalGridOld[grid.nDPhi][0][0][nKCen]
          +grid.dLocalGridOld[grid.nDPhi][0][0][nKCen-1])*0.5;
        dU_ijkp1half_nm1half=(grid.dLocalGridOld[grid.nU][nIInt][j][nKCen+1]
          +grid.dLocalGridOld[grid.nU][nIInt-1][j][nKCen+1]
          +grid.dLocalGridOld[grid.nU][nIInt][j][nKCen]
          +grid.dLocalGridOld[grid.nU][nIInt-1][j][nKCen])*0.25;
        dU_ijk_nm1half=(grid.dLocalGridOld[grid.nU][nIInt][j][nKCen]
          +grid.dLocalGridOld[grid.nU][nIInt-1][j][nKCen])*0.5;
        dU_ijkp1_nm1half=(grid.dLocalGridOld[grid.nU][nIInt][j][nKCen+1]
          +grid.dLocalGridOld[grid.nU][nIInt-1][j][nKCen+1])*0.5;
        dV_ijk_nm1half=(grid.dLocalGridOld[grid.nV][i][nJInt][nKCen]
          +grid.dLocalGridOld[grid.nV][i][nJInt-1][nKCen])*0.5;
        dV_ijkp1_nm1half=(grid.dLocalGridOld[grid.nV][i][nJInt][nKCen+1]
          +grid.dLocalGridOld[grid.nV][i][nJInt-1][nKCen+1])*0.5;
        dV_ijkp1half_nm1half=(grid.dLocalGridOld[grid.nV][i][nJInt][nKCen+1]
          +grid.dLocalGridOld[grid.nV][i][nJInt][nKCen]
          +grid.dLocalGridOld[grid.nV][i][nJInt-1][nKCen+1]
          +grid.dLocalGridOld[grid.nV][i][nJInt-1][nKCen])*0.25;
        dV_ijm1halfkp1half_nm1half=(grid.dLocalGridOld[grid.nV][i][nJInt-1][nKCen+1]
          +grid.dLocalGridOld[grid.nV][i][nJInt-1][nKCen])*0.5;
        dV_ijm1halfkm1half_nm1half=(grid.dLocalGridOld[grid.nV][i][nJInt-1][nKCen]
          +grid.dLocalGridOld[grid.nV][i][nJInt-1][nKCen-1])*0.5;
        dW_ijkp1half_nm1half=grid.dLocalGridOld[grid.nW][i][j][k];
        dW_ijp1halfkp1half_nm1half=(grid.dLocalGridOld[grid.nW][i][j+1][k]
          +grid.dLocalGridOld[grid.nW][i][j][k])*0.5;
        dW_ijm1halfkp1half_nm1half=(grid.dLocalGridOld[grid.nW][i][j-1][k]
          +grid.dLocalGridOld[grid.nW][i][j][k])*0.5;
        dW_ip1halfjkp1half_nm1half=(grid.dLocalGridOld[grid.nW][i+1][j][k]
          +grid.dLocalGridOld[grid.nW][i][j][k])*0.5;
        dW_im1halfjkp1half_nm1half=(grid.dLocalGridOld[grid.nW][i-1][j][k]
          +grid.dLocalGridOld[grid.nW][i][j][k])*0.5;
        dW_ijkp1_nm1half=(grid.dLocalGridOld[grid.nW][i][j][k]
          +grid.dLocalGridOld[grid.nW][i][j][k+1])*0.5;
        dW_ijk_nm1half=(grid.dLocalGridOld[grid.nW][i][j][k]
          +grid.dLocalGridOld[grid.nW][i][j][k-1])*0.5;
        dRho_ijkp1half_n=(grid.dLocalGridOld[grid.nD][i][j][nKCen]
          +grid.dLocalGridOld[grid.nD][i][j][nKCen+1])*0.5;
        dP_ijkp1_n=grid.dLocalGridOld[grid.nP][i][j][nKCen+1]
          +grid.dLocalGridOld[grid.nQ0][i][j][nKCen+1]+grid.dLocalGridOld[grid.nQ1][i][j][nKCen+1]
          +grid.dLocalGridOld[grid.nQ2][i][j][nKCen+1];
        dP_ijk_n=grid.dLocalGridOld[grid.nP][i][j][nKCen]+grid.dLocalGridOld[grid.nQ0][i][j][nKCen]
          +grid.dLocalGridOld[grid.nQ1][i][j][nKCen]
          +grid.dLocalGridOld[grid.nQ2][i][j][nKCen];
        dEddyVisc_ip1halfjkp1half_n=(grid.dLocalGridOld[grid.nEddyVisc][i+1][j][nKCen+1]
          +grid.dLocalGridOld[grid.nEddyVisc][i+1][j][nKCen]
          +grid.dLocalGridOld[grid.nEddyVisc][i][j][nKCen+1]
          +grid.dLocalGridOld[grid.nEddyVisc][i][j][nKCen])*0.25;
        dEddyVisc_im1halfjkp1half_n=(grid.dLocalGridOld[grid.nEddyVisc][i][j][nKCen+1]
          +grid.dLocalGridOld[grid.nEddyVisc][i][j][nKCen]
          +grid.dLocalGridOld[grid.nEddyVisc][i-1][j][nKCen+1]
          +grid.dLocalGridOld[grid.nEddyVisc][i-1][j][nKCen])*0.25;
        dEddyVisc_ijp1halfkp1half_n=(grid.dLocalGridOld[grid.nEddyVisc][i][j+1][nKCen+1]
          +grid.dLocalGridOld[grid.nEddyVisc][i][j+1][nKCen]
          +grid.dLocalGridOld[grid.nEddyVisc][i][j][nKCen+1]
          +grid.dLocalGridOld[grid.nEddyVisc][i][j][nKCen])*0.25;
        dEddyVisc_ijm1halfkp1half_n=(grid.dLocalGridOld[grid.nEddyVisc][i][j][nKCen+1]
          +grid.dLocalGridOld[grid.nEddyVisc][i][j][nKCen]
          +grid.dLocalGridOld[grid.nEddyVisc][i][j-1][nKCen+1]
          +grid.dLocalGridOld[grid.nEddyVisc][i][j-1][nKCen])*0.25;
        dEddyVisc_ijkp1half_n=(grid.dLocalGridOld[grid.nEddyVisc][i][j][nKCen+1]
          +grid.dLocalGridOld[grid.nEddyVisc][i][j][nKCen])*0.5;
        
        //calculate derived quantities
        dUmU0_ijkp1half_nm1half=dU_ijkp1half_nm1half-dU0_i_nm1half;
        d1_rhoDM_ijkp1half_n=1.0/(dRho_ijkp1half_n*grid.dLocalGridOld[grid.nDM][i][0][0]);
        dRRho_ijkp1half_n=dR_i_n*dRho_ijkp1half_n;
        dRSq_UmU0_ip1halfjkp1_n=dRSq_ip1half_n*(grid.dLocalGridOld[grid.nU][nIInt][j][nKCen+1]
          -grid.dLocalGridOld[grid.nU0][nIInt][0][0]);
        dRSq_UmU0_im1halfjkp1_n=dRSq_im1half_n*(grid.dLocalGridOld[grid.nU][nIInt-1][j][nKCen+1]
          -grid.dLocalGridOld[grid.nU0][nIInt-1][0][0]);
        dRSq_UmU0_ip1halfjk_n=dRSq_ip1half_n*(grid.dLocalGridOld[grid.nU][nIInt][j][nKCen]
          -grid.dLocalGridOld[grid.nU0][nIInt][0][0]);
        dRSq_UmU0_im1halfjk_n=dRSq_im1half_n*(grid.dLocalGridOld[grid.nU][nIInt-1][j][nKCen]
          -grid.dLocalGridOld[grid.nU0][nIInt-1][0][0]);
        dV_SinTheta_ijp1halfkp1_n=grid.dLocalGridOld[grid.nV][i][nJInt][nKCen+1]
          *grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt][0];
        dV_SinTheta_ijm1halfkp1_n=grid.dLocalGridOld[grid.nV][i][nJInt-1][nKCen+1]
          *grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt-1][0];
        dV_SinTheta_ijp1halfk_n=grid.dLocalGridOld[grid.nV][i][nJInt][nKCen]
          *grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt][0];
        dV_SinTheta_ijm1halfk_n=grid.dLocalGridOld[grid.nV][i][nJInt-1][nKCen]
          *grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt-1][0];
        dW_R_ip1jkp1half_n=grid.dLocalGridOld[grid.nW][i+1][j][k]/dR_ip1_n;
        dW_R_im1jkp1half_n=grid.dLocalGridOld[grid.nW][i-1][j][k]/dR_im1_n;
        dW_R_ijkp1half_n=grid.dLocalGridOld[grid.nW][i][j][k]/dR_i_n;
        dW_R_ip1halfjkp1half_n=dW_ip1halfjkp1half_nm1half/grid.dLocalGridOld[grid.nR][nIInt][0][0];
        dW_R_im1halfjkp1half_n=dW_im1halfjkp1half_nm1half
          /grid.dLocalGridOld[grid.nR][nIInt-1][0][0];
        dW_SinTheta_ijp1kp1half_n=grid.dLocalGridOld[grid.nW][i][j+1][k]
          /grid.dLocalGridOld[grid.nSinThetaIJK][0][j+1][0];
        dW_SinTheta_ijm1kp1half_n=grid.dLocalGridOld[grid.nW][i][j-1][k]
          /grid.dLocalGridOld[grid.nSinThetaIJK][0][j-1][0];
        dW_SinTheta_ijkp1half_n=grid.dLocalGridOld[grid.nW][i][j][k]
          /grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0];
        dW_SinTheta_ijp1halfkp1half_n=dW_ijp1halfkp1half_nm1half
          /grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt][0];
        dW_SinTheta_ijm1halfkp1half_n=dW_ijm1halfkp1half_nm1half
          /grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt-1][0];
        
        //calculate A1
        dA1CenGrad=(dW_ip1halfjkp1half_nm1half-dW_im1halfjkp1half_nm1half)
          /grid.dLocalGridOld[grid.nDM][i][0][0];
        if(dUmU0_ijkp1half_nm1half<0.0){//moving in a negative direction
          dA1UpWindGrad=(grid.dLocalGridOld[grid.nW][i+1][j][k]
            -grid.dLocalGridOld[grid.nW][i][j][k])/(grid.dLocalGridOld[grid.nDM][i][0][0]
            +grid.dLocalGridOld[grid.nDM][i+1][0][0])*2.0;
        }
        else{//moving in a positive direction
          dA1UpWindGrad=(grid.dLocalGridOld[grid.nW][i][j][k]
            -grid.dLocalGridOld[grid.nW][i-1][j][k])/(grid.dLocalGridOld[grid.nDM][i][0][0]
            +grid.dLocalGridOld[grid.nDM][i-1][0][0])*2.0;
        }
        dA1=dUmU0_ijkp1half_nm1half*((1.0-parameters.dDonorFrac)*dA1CenGrad+parameters.dDonorFrac
          *dA1UpWindGrad);
        
        //calculate S1
        dS1=dU_ijkp1half_nm1half*dW_ijkp1half_nm1half/dR_i_n;
        
        //calculate dA2
        dA2CenGrad=(dW_ijp1halfkp1half_nm1half-dW_ijm1halfkp1half_nm1half)
          /grid.dLocalGridOld[grid.nDTheta][0][j][0];
        if(dV_ijkp1half_nm1half<0.0){//moving in a negative direction
          dA2UpWindGrad=(grid.dLocalGridOld[grid.nW][i][j+1][k]
            -grid.dLocalGridOld[grid.nW][i][j][k])/(grid.dLocalGridOld[grid.nDTheta][0][j+1][0]
            +grid.dLocalGridOld[grid.nDTheta][0][j][0])*2.0;
        }
        else{//moving in a positive direction
          dA2UpWindGrad=(grid.dLocalGridOld[grid.nW][i][j][k]
            -grid.dLocalGridOld[grid.nW][i][j-1][k])/(grid.dLocalGridOld[grid.nDTheta][0][j-1][0]
            +grid.dLocalGridOld[grid.nDTheta][0][j][0])*2.0;
        }
        dA2=dV_ijkp1half_nm1half/dR_i_n*((1.0-parameters.dDonorFrac)*dA2CenGrad
          +parameters.dDonorFrac*dA2UpWindGrad);
        
        //calculate S2
        dS2=dV_ijkp1half_nm1half*grid.dLocalGridOld[grid.nW][i][j][k]
          *grid.dLocalGridOld[grid.nCotThetaIJK][0][j][0]/dR_i_n;
        
        //calculate A3
        dA3CenGrad=(dW_ijkp1_nm1half-dW_ijk_nm1half)/dDPhi_kp1half;
        if(dW_ijkp1half_nm1half<0.0){//moving in a negative direction
          dA3UpWindGrad=(grid.dLocalGridOld[grid.nW][i][j][k+1]
            -grid.dLocalGridOld[grid.nW][i][j][k])/grid.dLocalGridOld[grid.nDPhi][0][0][nKCen+1];
        }
        else{//moving in a positive direction
          dA3UpWindGrad=(grid.dLocalGridOld[grid.nW][i][j][k]
            -grid.dLocalGridOld[grid.nW][i][j][k-1])/grid.dLocalGridOld[grid.nDPhi][0][0][nKCen];
        }
        dA3=dW_ijkp1half_nm1half*((1.0-parameters.dDonorFrac)*dA3CenGrad
          +parameters.dDonorFrac*dA3UpWindGrad)/(dR_i_n
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]);
        
        //calculate S3
        dS3=(dP_ijkp1_n-dP_ijk_n)/(dRho_ijkp1half_n*dR_i_n
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]*dDPhi_kp1half);
        
        //calculate dDivU_ijkp1_n
        dDivU_ijkp1_n=4.0*parameters.dPi*grid.dLocalGridOld[grid.nDenAve][i][0][0]
          *(dRSq_UmU0_ip1halfjkp1_n-dRSq_UmU0_im1halfjkp1_n)/grid.dLocalGridOld[grid.nDM][i][0][0]
          +(dV_SinTheta_ijp1halfkp1_n-dV_SinTheta_ijm1halfkp1_n)/(dR_i_n
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]*grid.dLocalGridOld[grid.nDTheta][0][j][0])
          +(grid.dLocalGridOld[grid.nW][i][j][k+1]-grid.dLocalGridOld[grid.nW][i][j][k])/(dR_i_n
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]
          *grid.dLocalGridOld[grid.nDPhi][0][0][nKCen+1]);
        
        //calculate dDivU_ijk_n
        dDivU_ijk_n=4.0*parameters.dPi*grid.dLocalGridOld[grid.nDenAve][i][0][0]
          *(dRSq_UmU0_ip1halfjk_n-dRSq_UmU0_im1halfjk_n)/grid.dLocalGridOld[grid.nDM][i][0][0]
          +(dV_SinTheta_ijp1halfk_n-dV_SinTheta_ijm1halfk_n)/(dR_i_n
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]*grid.dLocalGridOld[grid.nDTheta][0][j][0])
          +(grid.dLocalGridOld[grid.nW][i][j][k]-grid.dLocalGridOld[grid.nW][i][j][k-1])/(dR_i_n
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]
          *grid.dLocalGridOld[grid.nDPhi][0][0][nKCen]);
        
        //calculate dTau_rp_ip1halfjkp1half_n
        dTau_rp_ip1halfjkp1half_n=dEddyVisc_ip1halfjkp1half_n*(4.0*parameters.dPi*dR3_ip1half_n
          *dRhoAve_ip1half_n*(dW_R_ip1jkp1half_n-dW_R_ijkp1half_n)/dDM_ip1half
          +(grid.dLocalGridOld[grid.nU][nIInt][j][nKCen+1]
          -grid.dLocalGridOld[grid.nU][nIInt][j][nKCen])
          /(dDPhi_kp1half*grid.dLocalGridOld[grid.nR][nIInt][0][0]
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]));
        
        //calculate dTau_rp_im1halfjkp1half_n
        dTau_rp_im1halfjkp1half_n=dEddyVisc_im1halfjkp1half_n*(4.0*parameters.dPi*dR3_im1half_n
          *dRhoAve_im1half_n*(dW_R_ijkp1half_n-dW_R_im1jkp1half_n)/dDM_im1half
          +(grid.dLocalGridOld[grid.nU][nIInt-1][j][nKCen+1]
          -grid.dLocalGridOld[grid.nU][nIInt-1][j][nKCen])
          /(dDPhi_kp1half*grid.dLocalGridOld[grid.nR][nIInt-1][0][0]
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]));
        
        //calculate dTau_tp_ijp1halfkp1half_n
        dTau_tp_ijp1halfkp1half_n=dEddyVisc_ijp1halfkp1half_n
          *(grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt][0]*(dW_SinTheta_ijp1kp1half_n
          -dW_SinTheta_ijkp1half_n)/(dR_i_n*dDTheta_jp1half)
          +(grid.dLocalGridOld[grid.nV][i][nJInt][nKCen+1]
          -grid.dLocalGridOld[grid.nV][i][nJInt][nKCen])/(dR_i_n
          *grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt][0]*dDPhi_kp1half));
        
        //calculate dTau_tp_ijm1halfkp1half_n
        dTau_tp_ijm1halfkp1half_n=dEddyVisc_ijm1halfkp1half_n
          *(grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt-1][0]*(dW_SinTheta_ijkp1half_n
          -dW_SinTheta_ijm1kp1half_n)/(dR_i_n*dDTheta_jm1half)
          +(grid.dLocalGridOld[grid.nV][i][nJInt-1][nKCen+1]
          -grid.dLocalGridOld[grid.nV][i][nJInt-1][nKCen])/(dR_i_n
          *grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt-1][0]*dDPhi_kp1half));
        
        //calculate dTau_pp_ijkp1half_n
        dTau_pp_ijkp1_n=2.0*grid.dLocalGridOld[grid.nEddyVisc][i][j][nKCen+1]
          *((grid.dLocalGridOld[grid.nW][i][j][k+1]-grid.dLocalGridOld[grid.nW][i][j][k])/(dR_i_n
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]
          *grid.dLocalGridOld[grid.nDPhi][0][0][nKCen+1])+(dU_ijkp1_nm1half-dU0_i_nm1half)
          /dR_i_n+dV_ijkp1_nm1half*grid.dLocalGridOld[grid.nCotThetaIJK][0][j][0]/dR_i_n
          -0.333333333333333*dDivU_ijkp1_n);
        
        //calculate dTau_pp_ijk_n
        dTau_pp_ijk_n=2.0*grid.dLocalGridOld[grid.nEddyVisc][i][j][nKCen]
          *((grid.dLocalGridOld[grid.nW][i][j][k]-grid.dLocalGridOld[grid.nW][i][j][k-1])/(dR_i_n
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]
          *grid.dLocalGridOld[grid.nDPhi][0][0][nKCen])+(dU_ijk_nm1half-dU0_i_nm1half)
          /dR_i_n+dV_ijk_nm1half*grid.dLocalGridOld[grid.nCotThetaIJK][0][j][0]/dR_i_n
          -0.333333333333333*dDivU_ijk_n);
        
        //calculate dTA1
        dTA1=(dTau_rp_ip1halfjkp1half_n-dTau_rp_im1halfjkp1half_n)*d1_rhoDM_ijkp1half_n;
        
        //calculate dTS1
        dTS1=3.0*dEddyVisc_ijkp1half_n*(dW_R_ip1halfjkp1half_n-dW_R_im1halfjkp1half_n)
          *d1_rhoDM_ijkp1half_n;
        
        //calculate dTA2
        dTA2=(dTau_tp_ijp1halfkp1half_n-dTau_tp_ijm1halfkp1half_n)/(dRRho_ijkp1half_n
          *grid.dLocalGridOld[grid.nDTheta][0][j][0]);
        
        //calculate dTS2
        dTS2=2.0*grid.dLocalGridOld[grid.nCotThetaIJK][0][j][0]
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]*(dW_SinTheta_ijp1halfkp1half_n
          -dW_SinTheta_ijm1halfkp1half_n)/(dR_i_n*grid.dLocalGridOld[grid.nDTheta][0][j][0]);
        
        //calculate dTA3
        dTA3=(dTau_pp_ijkp1_n-dTau_pp_ijk_n)/(dRRho_ijkp1half_n
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]
          *dDPhi_kp1half);
        //dTA3=0.0;
        
        //calculate dTS3
        dTS3=(3.0*(dU_ijkp1_nm1half-dU_ijk_nm1half)+2.0
          *grid.dLocalGridOld[grid.nCotThetaIJK][0][j][0]*(dV_ijkp1_nm1half-dV_ijk_nm1half))/(dR_i_n
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]*dDPhi_kp1half);
        
        dEddyViscosityTerms=-4.0*parameters.dPi*dRSq_i_n*grid.dLocalGridOld[grid.nDenAve][i][0][0]
          *(dTA1+dTS1)-dTA2-dTA3-dEddyVisc_ijkp1half_n/(dRho_ijkp1half_n*dR_i_n)*(dTS2+dTS3);
        //dEddyViscosityTerms=0.0;
        
        //calculate new velocity
        grid.dLocalGridNew[grid.nW][i][j][k]=grid.dLocalGridOld[grid.nW][i][j][k]-time.dDeltat_n
          *(4.0*parameters.dPi*dRSq_i_n*grid.dLocalGridOld[grid.nDenAve][i][0][0]*(dA1)
          +dS1+dA2+dS2+dA3+dS3+dEddyViscosityTerms);
      }
    }
  }
  
  //ghost region 0, outter most ghost region in x1 direction
  for(i=grid.nStartGhostUpdateExplicit[grid.nV][0][0];
    i<grid.nEndGhostUpdateExplicit[grid.nV][0][0];i++){
    
    //calculate j of interface quantities
    nIInt=i+grid.nCenIntOffset[0];
    dR_i_n=(grid.dLocalGridOld[grid.nR][nIInt][0][0]+grid.dLocalGridOld[grid.nR][nIInt-1][0][0])
      *0.5;
    dR_ip1_n=grid.dLocalGridOld[grid.nR][nIInt][0][0];
    dR_im1_n=(grid.dLocalGridOld[grid.nR][nIInt-1][0][0]+grid.dLocalGridOld[grid.nR][nIInt-2][0][0])
      *0.5;
    dRSq_i_n=dR_i_n*dR_i_n;
    dRSq_ip1half_n=grid.dLocalGridOld[grid.nR][nIInt][0][0]
      *grid.dLocalGridOld[grid.nR][nIInt][0][0];
    dRSq_im1half_n=grid.dLocalGridOld[grid.nR][nIInt-1][0][0]
      *grid.dLocalGridOld[grid.nR][nIInt-1][0][0];
    dR3_ip1half_n=dRSq_ip1half_n*grid.dLocalGridOld[grid.nR][nIInt][0][0];
    dR3_im1half_n=dRSq_im1half_n*grid.dLocalGridOld[grid.nR][nIInt-1][0][0];
    dU0_i_nm1half=(grid.dLocalGridOld[grid.nU0][nIInt][0][0]
      +grid.dLocalGridOld[grid.nU0][nIInt-1][0][0])*0.5;
    dRhoAve_ip1half_n=(grid.dLocalGridOld[grid.nDenAve][i][0][0])*0.5;
    dRhoAve_im1half_n=(grid.dLocalGridOld[grid.nDenAve][i][0][0]
      +grid.dLocalGridOld[grid.nDenAve][i-1][0][0])*0.5;
    dDM_ip1half=(grid.dLocalGridOld[grid.nDM][i][0][0])*0.5;
    dDM_im1half=(grid.dLocalGridOld[grid.nDM][i][0][0]+grid.dLocalGridOld[grid.nDM][i-1][0][0])*0.5;
    
    for(j=grid.nStartGhostUpdateExplicit[grid.nV][0][1];
      j<grid.nEndGhostUpdateExplicit[grid.nV][0][1];j++){
      
      //calculate j of centered quantities
      nJInt=j+grid.nCenIntOffset[1];
      dDTheta_jp1half=(grid.dLocalGridOld[grid.nDTheta][0][j+1][0]
        +grid.dLocalGridOld[grid.nDTheta][0][j][0])*0.5;
      dDTheta_jm1half=(grid.dLocalGridOld[grid.nDTheta][0][j][0]
        +grid.dLocalGridOld[grid.nDTheta][0][j-1][0])*0.5;
      
      for(k=grid.nStartGhostUpdateExplicit[grid.nV][0][2];
        k<grid.nEndGhostUpdateExplicit[grid.nV][0][2];k++){
        
        //calculate k of interface quantities
        nKCen=k-grid.nCenIntOffset[2];
        
        //Calculate interpolated quantities
        dDPhi_kp1half=(grid.dLocalGridOld[grid.nDPhi][0][0][nKCen]
          +grid.dLocalGridOld[grid.nDPhi][0][0][nKCen+1])*0.5;
        dDPhi_km1half=(grid.dLocalGridOld[grid.nDPhi][0][0][nKCen]
          +grid.dLocalGridOld[grid.nDPhi][0][0][nKCen-1])*0.5;
        dU_ijkp1half_nm1half=(grid.dLocalGridOld[grid.nU][nIInt][j][nKCen+1]
          +grid.dLocalGridOld[grid.nU][nIInt-1][j][nKCen+1]
          +grid.dLocalGridOld[grid.nU][nIInt][j][nKCen]
          +grid.dLocalGridOld[grid.nU][nIInt-1][j][nKCen])*0.25;
        dU_ijk_nm1half=(grid.dLocalGridOld[grid.nU][nIInt][j][nKCen]
          +grid.dLocalGridOld[grid.nU][nIInt-1][j][nKCen])*0.5;
        dU_ijkp1_nm1half=(grid.dLocalGridOld[grid.nU][nIInt][j][nKCen+1]
          +grid.dLocalGridOld[grid.nU][nIInt-1][j][nKCen+1])*0.5;
        dV_ijk_nm1half=(grid.dLocalGridOld[grid.nV][i][nJInt][nKCen]
          +grid.dLocalGridOld[grid.nV][i][nJInt-1][nKCen])*0.5;
        dV_ijkp1_nm1half=(grid.dLocalGridOld[grid.nV][i][nJInt][nKCen+1]
          +grid.dLocalGridOld[grid.nV][i][nJInt-1][nKCen+1])*0.5;
        dV_ijkp1half_nm1half=(grid.dLocalGridOld[grid.nV][i][nJInt][nKCen+1]
          +grid.dLocalGridOld[grid.nV][i][nJInt][nKCen]
          +grid.dLocalGridOld[grid.nV][i][nJInt-1][nKCen+1]
          +grid.dLocalGridOld[grid.nV][i][nJInt-1][nKCen])*0.25;
        dV_ijm1halfkp1half_nm1half=(grid.dLocalGridOld[grid.nV][i][nJInt-1][nKCen+1]
          +grid.dLocalGridOld[grid.nV][i][nJInt-1][nKCen])*0.5;
        dV_ijm1halfkm1half_nm1half=(grid.dLocalGridOld[grid.nV][i][nJInt-1][nKCen]
          +grid.dLocalGridOld[grid.nV][i][nJInt-1][nKCen-1])*0.5;
        dW_ijkp1half_nm1half=grid.dLocalGridOld[grid.nW][i][j][k];
        dW_ijp1halfkp1half_nm1half=(grid.dLocalGridOld[grid.nW][i][j+1][k]
          +grid.dLocalGridOld[grid.nW][i][j][k])*0.5;
        dW_ijm1halfkp1half_nm1half=(grid.dLocalGridOld[grid.nW][i][j-1][k]
          +grid.dLocalGridOld[grid.nW][i][j][k])*0.5;
        dW_ip1halfjkp1half_nm1half=grid.dLocalGridOld[grid.nW][i][j][k];/**\BC assume theta and
          phi velocities are constant across surface*/
        dW_im1halfjkp1half_nm1half=(grid.dLocalGridOld[grid.nW][i-1][j][k]
          +grid.dLocalGridOld[grid.nW][i][j][k])*0.5;
        dW_ijkp1_nm1half=(grid.dLocalGridOld[grid.nW][i][j][k]
          +grid.dLocalGridOld[grid.nW][i][j][k+1])*0.5;
        dW_ijk_nm1half=(grid.dLocalGridOld[grid.nW][i][j][k]
          +grid.dLocalGridOld[grid.nW][i][j][k-1])*0.5;
        dRho_ijkp1half_n=(grid.dLocalGridOld[grid.nD][i][j][nKCen]
          +grid.dLocalGridOld[grid.nD][i][j][nKCen+1])*0.5;
        dP_ijkp1_n=grid.dLocalGridOld[grid.nP][i][j][nKCen+1]
          +grid.dLocalGridOld[grid.nQ0][i][j][nKCen+1]
          +grid.dLocalGridOld[grid.nQ1][i][j][nKCen+1]
          +grid.dLocalGridOld[grid.nQ2][i][j][nKCen+1];
        dP_ijk_n=grid.dLocalGridOld[grid.nP][i][j][nKCen]+grid.dLocalGridOld[grid.nQ0][i][j][nKCen]
          +grid.dLocalGridOld[grid.nQ1][i][j][nKCen]+grid.dLocalGridOld[grid.nQ2][i][j][nKCen];
        dEddyVisc_ip1halfjkp1half_n=(grid.dLocalGridOld[grid.nEddyVisc][i][j][nKCen+1]
          +grid.dLocalGridOld[grid.nEddyVisc][i][j][nKCen])*0.25;/** \BC assume eddy viscosity is 
          zero at surface*/
        dEddyVisc_im1halfjkp1half_n=(grid.dLocalGridOld[grid.nEddyVisc][i][j][nKCen+1]
          +grid.dLocalGridOld[grid.nEddyVisc][i][j][nKCen]
          +grid.dLocalGridOld[grid.nEddyVisc][i-1][j][nKCen+1]
          +grid.dLocalGridOld[grid.nEddyVisc][i-1][j][nKCen])*0.25;
        dEddyVisc_ijp1halfkp1half_n=(grid.dLocalGridOld[grid.nEddyVisc][i][j+1][nKCen+1]
          +grid.dLocalGridOld[grid.nEddyVisc][i][j+1][nKCen]
          +grid.dLocalGridOld[grid.nEddyVisc][i][j][nKCen+1]
          +grid.dLocalGridOld[grid.nEddyVisc][i][j][nKCen])*0.25;
        dEddyVisc_ijm1halfkp1half_n=(grid.dLocalGridOld[grid.nEddyVisc][i][j][nKCen+1]
          +grid.dLocalGridOld[grid.nEddyVisc][i][j][nKCen]
          +grid.dLocalGridOld[grid.nEddyVisc][i][j-1][nKCen+1]
          +grid.dLocalGridOld[grid.nEddyVisc][i][j-1][nKCen])*0.25;
        dEddyVisc_ijkp1half_n=(grid.dLocalGridOld[grid.nEddyVisc][i][j][nKCen+1]
          +grid.dLocalGridOld[grid.nEddyVisc][i][j][nKCen])*0.5;
        
        //calculate derived quantities
        dUmU0_ijkp1half_nm1half=dU_ijkp1half_nm1half-dU0_i_nm1half;
        d1_rhoDM_ijkp1half_n=1.0/(dRho_ijkp1half_n*grid.dLocalGridOld[grid.nDM][i][0][0]);
        dRRho_ijkp1half_n=dR_i_n*dRho_ijkp1half_n;
        dRSq_UmU0_ip1halfjkp1_n=dRSq_ip1half_n*(grid.dLocalGridOld[grid.nU][nIInt][j][nKCen+1]
          -grid.dLocalGridOld[grid.nU0][nIInt][0][0]);
        dRSq_UmU0_im1halfjkp1_n=dRSq_im1half_n*(grid.dLocalGridOld[grid.nU][nIInt-1][j][nKCen+1]
          -grid.dLocalGridOld[grid.nU0][nIInt-1][0][0]);
        dRSq_UmU0_ip1halfjk_n=dRSq_ip1half_n*(grid.dLocalGridOld[grid.nU][nIInt][j][nKCen]
          -grid.dLocalGridOld[grid.nU0][nIInt][0][0]);
        dRSq_UmU0_im1halfjk_n=dRSq_im1half_n*(grid.dLocalGridOld[grid.nU][nIInt-1][j][nKCen]
          -grid.dLocalGridOld[grid.nU0][nIInt-1][0][0]);
        dV_SinTheta_ijp1halfkp1_n=grid.dLocalGridOld[grid.nV][i][nJInt][nKCen+1]
          *grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt][0];
        dV_SinTheta_ijm1halfkp1_n=grid.dLocalGridOld[grid.nV][i][nJInt-1][nKCen+1]
          *grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt-1][0];
        dV_SinTheta_ijp1halfk_n=grid.dLocalGridOld[grid.nV][i][nJInt][nKCen]
          *grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt][0];
        dV_SinTheta_ijm1halfk_n=grid.dLocalGridOld[grid.nV][i][nJInt-1][nKCen]
          *grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt-1][0];
        dW_R_ip1jkp1half_n=grid.dLocalGridOld[grid.nW][i][j][k]/dR_ip1_n;
        dW_R_im1jkp1half_n=grid.dLocalGridOld[grid.nW][i-1][j][k]/dR_im1_n;
        dW_R_ijkp1half_n=grid.dLocalGridOld[grid.nW][i][j][k]/dR_i_n;
        dW_R_ip1halfjkp1half_n=dW_ip1halfjkp1half_nm1half/grid.dLocalGridOld[grid.nR][nIInt][0][0];
        dW_R_im1halfjkp1half_n=dW_im1halfjkp1half_nm1half
          /grid.dLocalGridOld[grid.nR][nIInt-1][0][0];
        dW_SinTheta_ijp1kp1half_n=grid.dLocalGridOld[grid.nW][i][j+1][k]
          /grid.dLocalGridOld[grid.nSinThetaIJK][0][j+1][0];
        dW_SinTheta_ijm1kp1half_n=grid.dLocalGridOld[grid.nW][i][j-1][k]
          /grid.dLocalGridOld[grid.nSinThetaIJK][0][j-1][0];
        dW_SinTheta_ijkp1half_n=grid.dLocalGridOld[grid.nW][i][j][k]
          /grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0];
        dW_SinTheta_ijp1halfkp1half_n=dW_ijp1halfkp1half_nm1half
          /grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt][0];
        dW_SinTheta_ijm1halfkp1half_n=dW_ijm1halfkp1half_nm1half
          /grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt-1][0];
        
        //calculate A1
        dA1CenGrad=(dW_ip1halfjkp1half_nm1half-dW_im1halfjkp1half_nm1half)
          /grid.dLocalGridOld[grid.nDM][i][0][0];
        dA1UpWindGrad=0.0;
        
        if(dUmU0_ijkp1half_nm1half<0.0){//moving in a negative direction
          dA1UpWindGrad=dA1CenGrad;/**\BC assume upwind gradient is the same as centered gradient
            across surface*/
        }
        else{//moving in a positive direction
          dA1UpWindGrad=(grid.dLocalGridOld[grid.nW][i][j][k]
            -grid.dLocalGridOld[grid.nW][i-1][j][k])/(grid.dLocalGridOld[grid.nDM][i][0][0]
            +grid.dLocalGridOld[grid.nDM][i-1][0][0])*2.0;
        }
        dA1=dUmU0_ijkp1half_nm1half*((1.0-parameters.dDonorFrac)*dA1CenGrad+parameters.dDonorFrac
          *dA1UpWindGrad);
        
        //calculate S1
        dS1=dU_ijkp1half_nm1half*dW_ijkp1half_nm1half/dR_i_n;
        
        //calculate dA2
        dA2CenGrad=(dW_ijp1halfkp1half_nm1half-dW_ijm1halfkp1half_nm1half)
          /grid.dLocalGridOld[grid.nDTheta][0][j][0];
        dA2UpWindGrad=0.0;
        if(dV_ijkp1half_nm1half<0.0){//moning in a negative direction
          dA2UpWindGrad=(grid.dLocalGridOld[grid.nW][i][j+1][k]
            -grid.dLocalGridOld[grid.nW][i][j][k])/(grid.dLocalGridOld[grid.nDTheta][0][j+1][0]
            +grid.dLocalGridOld[grid.nDTheta][0][j][0])*2.0;
        }
        else{//moving in a positive direction
          dA2UpWindGrad=(grid.dLocalGridOld[grid.nW][i][j][k]
            -grid.dLocalGridOld[grid.nW][i][j-1][k])/(grid.dLocalGridOld[grid.nDTheta][0][j-1][0]
            +grid.dLocalGridOld[grid.nDTheta][0][j][0])*2.0;
        }
        dA2=dV_ijkp1half_nm1half/dR_i_n*((1.0-parameters.dDonorFrac)*dA2CenGrad
          +parameters.dDonorFrac*dA2UpWindGrad);
        
        //calculate S2
        dS2=dV_ijkp1half_nm1half*grid.dLocalGridOld[grid.nW][i][j][k]
          *grid.dLocalGridOld[grid.nCotThetaIJK][0][j][0]/dR_i_n;
        
        //calculate A3
        dA3CenGrad=(dW_ijkp1_nm1half-dW_ijk_nm1half)/dDPhi_kp1half;
        dA3UpWindGrad=0.0;
        if(dW_ijkp1half_nm1half<0.0){//moving in a negative direction
          dA3UpWindGrad=(grid.dLocalGridOld[grid.nW][i][j][k+1]
            -grid.dLocalGridOld[grid.nW][i][j][k])/grid.dLocalGridOld[grid.nDPhi][0][0][nKCen+1];
        }
        else{//moving in a positive direction
          dA3UpWindGrad=(grid.dLocalGridOld[grid.nW][i][j][k]
            -grid.dLocalGridOld[grid.nW][i][j][k-1])/grid.dLocalGridOld[grid.nDPhi][0][0][nKCen];
        }
        dA3=dW_ijkp1half_nm1half*((1.0-parameters.dDonorFrac)*dA3CenGrad
          +parameters.dDonorFrac*dA3UpWindGrad)/(dR_i_n
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]);
        
        //calculate S3
        dS3=(dP_ijkp1_n-dP_ijk_n)/(dRho_ijkp1half_n*dR_i_n
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]*dDPhi_kp1half);
        
        //calculate dDivU_ijkp1_n
        dDivU_ijkp1_n=4.0*parameters.dPi*grid.dLocalGridOld[grid.nDenAve][i][0][0]
          *(dRSq_UmU0_ip1halfjkp1_n-dRSq_UmU0_im1halfjkp1_n)/grid.dLocalGridOld[grid.nDM][i][0][0]
          +(dV_SinTheta_ijp1halfkp1_n-dV_SinTheta_ijm1halfkp1_n)/(dR_i_n
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]*grid.dLocalGridOld[grid.nDTheta][0][j][0])
          +(grid.dLocalGridOld[grid.nW][i][j][k+1]-grid.dLocalGridOld[grid.nW][i][j][k])/(dR_i_n
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]
          *grid.dLocalGridOld[grid.nDPhi][0][0][nKCen+1]);
        
        //calculate dDivU_ijk_n
        dDivU_ijk_n=4.0*parameters.dPi*grid.dLocalGridOld[grid.nDenAve][i][0][0]
          *(dRSq_UmU0_ip1halfjk_n-dRSq_UmU0_im1halfjk_n)/grid.dLocalGridOld[grid.nDM][i][0][0]
          +(dV_SinTheta_ijp1halfk_n-dV_SinTheta_ijm1halfk_n)/(dR_i_n
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]*grid.dLocalGridOld[grid.nDTheta][0][j][0])
          +(grid.dLocalGridOld[grid.nW][i][j][k]-grid.dLocalGridOld[grid.nW][i][j][k-1])/(dR_i_n
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]
          *grid.dLocalGridOld[grid.nDPhi][0][0][nKCen]);
        
        //calculate dTau_rp_ip1halfjkp1half_n
        dTau_rp_ip1halfjkp1half_n=dEddyVisc_ip1halfjkp1half_n*(4.0*parameters.dPi*dR3_ip1half_n
          *dRhoAve_ip1half_n*(dW_R_ip1jkp1half_n-dW_R_ijkp1half_n)/dDM_ip1half
          +(grid.dLocalGridOld[grid.nU][nIInt][j][nKCen+1]
          -grid.dLocalGridOld[grid.nU0][nIInt][0][0])-(grid.dLocalGridOld[grid.nU][nIInt][j][nKCen]
          -grid.dLocalGridOld[grid.nU0][nIInt][0][0])
          /(dDPhi_kp1half*grid.dLocalGridOld[grid.nR][nIInt][0][0]
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]));
        
        //calculate dTau_rp_im1halfjkp1half_n
        dTau_rp_im1halfjkp1half_n=dEddyVisc_im1halfjkp1half_n*(4.0*parameters.dPi*dR3_im1half_n
          *dRhoAve_im1half_n*(dW_R_ijkp1half_n-dW_R_im1jkp1half_n)/dDM_im1half
          +((grid.dLocalGridOld[grid.nU][nIInt-1][j][nKCen+1]
          -grid.dLocalGridOld[grid.nU0][nIInt-1][0][0])
          -(grid.dLocalGridOld[grid.nU][nIInt-1][j][nKCen]
          -grid.dLocalGridOld[grid.nU0][nIInt-1][0][0]))
          /(dDPhi_kp1half*grid.dLocalGridOld[grid.nR][nIInt-1][0][0]
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]));
        
        //calculate dTau_tp_ijp1halfkp1half_n
        dTau_tp_ijp1halfkp1half_n=dEddyVisc_ijp1halfkp1half_n
          *(grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt][0]*(dW_SinTheta_ijp1kp1half_n
          -dW_SinTheta_ijkp1half_n)/(dR_i_n*dDTheta_jp1half)
          +(grid.dLocalGridOld[grid.nV][i][nJInt][nKCen+1]
          -grid.dLocalGridOld[grid.nV][i][nJInt][nKCen])/(dR_i_n
          *grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt][0]*dDPhi_kp1half));
        
        //calculate dTau_tp_ijm1halfkp1half_n
        dTau_tp_ijm1halfkp1half_n=dEddyVisc_ijm1halfkp1half_n
          *(grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt-1][0]*(dW_SinTheta_ijkp1half_n
          -dW_SinTheta_ijm1kp1half_n)/(dR_i_n*dDTheta_jm1half)
          +(grid.dLocalGridOld[grid.nV][i][nJInt-1][nKCen+1]
          -grid.dLocalGridOld[grid.nV][i][nJInt-1][nKCen])/(dR_i_n
          *grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt-1][0]*dDPhi_kp1half));
        
        //calculate dTau_pp_ijkp1half_n
        dTau_pp_ijkp1_n=2.0*grid.dLocalGridOld[grid.nEddyVisc][i][j][nKCen+1]
          *((grid.dLocalGridOld[grid.nW][i][j][k+1]-grid.dLocalGridOld[grid.nW][i][j][k])/(dR_i_n
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]
          *grid.dLocalGridOld[grid.nDPhi][0][0][nKCen+1])+(dU_ijkp1_nm1half-dU0_i_nm1half)
          /dR_i_n+dV_ijkp1_nm1half*grid.dLocalGridOld[grid.nCotThetaIJK][0][j][0]/dR_i_n
          -0.333333333333333*dDivU_ijkp1_n);
        
        dTau_pp_ijk_n=2.0*grid.dLocalGridOld[grid.nEddyVisc][i][j][nKCen]
          *((grid.dLocalGridOld[grid.nW][i][j][k]-grid.dLocalGridOld[grid.nW][i][j][k-1])/(dR_i_n
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]
          *grid.dLocalGridOld[grid.nDPhi][0][0][nKCen])
          +(dU_ijk_nm1half-dU0_i_nm1half)/dR_i_n+dV_ijk_nm1half
          *grid.dLocalGridOld[grid.nCotThetaIJK][0][j][0]/dR_i_n-0.333333333333333*dDivU_ijk_n);
        
        //calculate dTA1
        dTA1=(dTau_rp_ip1halfjkp1half_n-dTau_rp_im1halfjkp1half_n)*d1_rhoDM_ijkp1half_n;
        
        //calculate dTS1
        dTS1=3.0*dEddyVisc_ijkp1half_n*(dW_R_ip1halfjkp1half_n-dW_R_im1halfjkp1half_n)
          *d1_rhoDM_ijkp1half_n;
        
        //calculate dTA2
        dTA2=(dTau_tp_ijp1halfkp1half_n-dTau_tp_ijm1halfkp1half_n)/(dRRho_ijkp1half_n
          *grid.dLocalGridOld[grid.nDTheta][0][j][0]);
        
        //calculate dTS2
        dTS2=2.0*grid.dLocalGridOld[grid.nCotThetaIJK][0][j][0]
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]*(dW_SinTheta_ijp1halfkp1half_n
          -dW_SinTheta_ijm1halfkp1half_n)/(dR_i_n*grid.dLocalGridOld[grid.nDTheta][0][j][0]);
        
        //calculate dTA3
        dTA3=(dTau_pp_ijkp1_n-dTau_pp_ijk_n)/(dRRho_ijkp1half_n
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]
          *grid.dLocalGridOld[grid.nDPhi][0][0][nKCen]);
        //dTA3=0.0;
        
        //calculate dTS3
        dTS3=3.0*((dU_ijkp1_nm1half-dU0_i_nm1half)-(dU_ijk_nm1half-dU0_i_nm1half)+2.0
          *grid.dLocalGridOld[grid.nCotThetaIJK][0][j][0]*(dV_ijkp1_nm1half-dV_ijk_nm1half))/(dR_i_n
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]
          *grid.dLocalGridOld[grid.nDPhi][0][0][nKCen]);
        
        dEddyViscosityTerms=-4.0*parameters.dPi*dRSq_i_n*grid.dLocalGridOld[grid.nDenAve][i][0][0]
          *(dTA1+dTS1)-dTA2-dTA3-dEddyVisc_ijkp1half_n/(dRho_ijkp1half_n*dR_i_n)*(dTS2+dTS3);
        //dEddyViscosityTerms=0.0;
        
        //calculate new velocity
        grid.dLocalGridNew[grid.nW][i][j][k]=grid.dLocalGridOld[grid.nW][i][j][k]-time.dDeltat_n
          *(4.0*parameters.dPi*dRSq_i_n*grid.dLocalGridOld[grid.nDenAve][i][0][0]*(dA1)
          +dS1+dA2+dS2+dA3+dS3+dEddyViscosityTerms);
      }
    }
  }
}
void calNewU0_R(Grid &grid,Parameters &parameters,Time &time,ProcTop &procTop,MessPass &messPass){
  
  /**\todo
    At some point I will likely want to make this funciton compatiable with a 3D domain 
    decomposition instead of a purely radial domain decomposition.
  */
  
  int i;
  //post a blocking recieve from inner radial neighbour
  for(i=0;i<procTop.nNumRadialNeighbors;i++){
    if(procTop.nCoords[procTop.nRank][0]>procTop.nCoords[procTop.nRadialNeighborRanks[i]][0]){/*if
      current processor has a radial neighbor at inside post a recieve*/
      MPI::COMM_WORLD.Recv(grid.dLocalGridNew,1
        ,messPass.typeRecvNewVar[procTop.nRadialNeighborNeighborIDs[i]][grid.nU0]
        ,procTop.nRadialNeighborRanks[i],2);
    }
  }
  
  #if SEDOV==1
    //calculate grid velocities for local grid at inner most ghost region
    for(i=grid.nStartGhostUpdateExplicit[grid.nU0][1][0];
      i<grid.nEndGhostUpdateExplicit[grid.nU0][1][0];i++){//nU0 needs to be 1D
      grid.dLocalGridNew[grid.nU0][i][0][0]=grid.dLocalGridNew[grid.nU][i][0][0];
    }
  #endif
  
  //calculate grid velocities for local grid
  int nICen;
  double dARatio;
  double dRhoim1half;
  double dRhoip1half;
  for(i=grid.nStartUpdateExplicit[grid.nU0][0];i<grid.nEndUpdateExplicit[grid.nU0][0];i++){/*nU0
    needs to be 1D*/
    
    //calculate i of centered quantities
    nICen=i-grid.nCenIntOffset[0];
    
    //calculate ratio of area at i-1/2 to area at i+1/2
    dARatio=grid.dLocalGridOld[grid.nR][i-1][0][0]*grid.dLocalGridOld[grid.nR][i-1][0][0]
      /(grid.dLocalGridOld[grid.nR][i][0][0]*grid.dLocalGridOld[grid.nR][i][0][0]);
    
    //calculate density at i-1/2
    dRhoim1half=(grid.dLocalGridOld[grid.nD][nICen][0][0]
      +grid.dLocalGridOld[grid.nD][nICen-1][0][0])*0.5;
    
    //calculate density at i+1/2
    dRhoip1half=(grid.dLocalGridOld[grid.nD][nICen+1][0][0]
      +grid.dLocalGridOld[grid.nD][nICen][0][0])*0.5;
    
    //calculate new grid velocity
    grid.dLocalGridNew[grid.nU0][i][0][0]=(grid.dLocalGridNew[grid.nU0][i-1][0][0]
      -grid.dLocalGridNew[grid.nU][i-1][0][0])
      *dARatio*dRhoim1half/dRhoip1half+grid.dLocalGridNew[grid.nU][i][0][0];
    
  }
  
  //post a blocking send to outer radial neighbour
  int nNumRecieves=0;
  for(i=0;i<procTop.nNumRadialNeighbors;i++){
    if(procTop.nCoords[procTop.nRank][0]<procTop.nCoords[procTop.nRadialNeighborRanks[i]][0]){/*if
      current processor has a radial neighbor at inside post a recieve*/
      
      MPI::COMM_WORLD.Send(grid.dLocalGridNew,1
        ,messPass.typeSendNewVar[procTop.nRadialNeighborNeighborIDs[i]][grid.nU0]
        ,procTop.nRadialNeighborRanks[i],2);
      nNumRecieves++;
    }
  }
  
  //post a non-blocking recieve for outer radial neighbour
  MPI::Request *requestTempRecv=new MPI::Request[nNumRecieves];
  int nCount=0;
  for(i=0;i<procTop.nNumRadialNeighbors;i++){
    if(procTop.nCoords[procTop.nRank][0]<procTop.nCoords[procTop.nRadialNeighborRanks[i]][0]){/*if
      current processor has a radial neighbor at inside post a recieve*/
      
      requestTempRecv[nCount]=MPI::COMM_WORLD.Irecv(grid.dLocalGridNew,1
        ,messPass.typeRecvNewVar[procTop.nRadialNeighborNeighborIDs[i]][grid.nU0]
        ,procTop.nRadialNeighborRanks[i],2);
      nCount++;
    }
  }
  
  //post a blocking recieve for inner radial neighbour
  for(i=0;i<procTop.nNumRadialNeighbors;i++){
    if(procTop.nCoords[procTop.nRank][0]>procTop.nCoords[procTop.nRadialNeighborRanks[i]][0]){/*if
      current processor has a radial neighbor at inside post a recieve*/
      
      MPI::COMM_WORLD.Send(grid.dLocalGridNew,1
        ,messPass.typeSendNewVar[procTop.nRadialNeighborNeighborIDs[i]][grid.nU0]
        ,procTop.nRadialNeighborRanks[i],2);
    }
  }
  
  //calculate outermost grid velocity
  for(i=grid.nStartGhostUpdateExplicit[grid.nU][0][0];i<grid.nEndGhostUpdateExplicit[grid.nU][0][0]
    ;i++){
    
    //calculate i of centered quantities
    nICen=i-grid.nCenIntOffset[0];
    
    //calculate ratio of area at i-1/2 to area at i+1/2
    dARatio=grid.dLocalGridOld[grid.nR][i-1][0][0]*grid.dLocalGridOld[grid.nR][i-1][0][0]
      /(grid.dLocalGridOld[grid.nR][i][0][0]*grid.dLocalGridOld[grid.nR][i][0][0]);
    
    //calculate density at i-1/2
    dRhoim1half=(grid.dLocalGridOld[grid.nD][nICen][0][0]
      +grid.dLocalGridOld[grid.nD][nICen-1][0][0])/2.0;
    
    //calculate density at i+1/2
    dRhoip1half=(0.0+grid.dLocalGridOld[grid.nD][nICen][0][0])/2.0;
    
    grid.dLocalGridNew[grid.nU0][i][0][0]=(grid.dLocalGridNew[grid.nU0][i-1][0][0]
      -grid.dLocalGridNew[grid.nU][i-1][0][0])*dARatio*dRhoim1half/dRhoip1half
      +grid.dLocalGridNew[grid.nU][i][0][0];
    
  }
  
  //wait for all recieves to complete
  MPI::Status *statusTempRecv=new MPI::Status[nNumRecieves];
  MPI::Request::Waitall(nNumRecieves,requestTempRecv,statusTempRecv);
  delete [] requestTempRecv;
  delete [] statusTempRecv;
}
void calNewU0_RT(Grid &grid,Parameters &parameters,Time &time,ProcTop &procTop,MessPass &messPass){
  
  /**\todo
    At some point I will likely want to make this funciton compatiable with a 3D domain 
    decomposition instead of a purely radial domain decomposition.
  */
  
  //post a blocking recieve from inner radial neighbour
  int i;
  for(i=0;i<procTop.nNumRadialNeighbors;i++){
    if(procTop.nCoords[procTop.nRank][0]>procTop.nCoords[procTop.nRadialNeighborRanks[i]][0]){/*if
      current processor has a radial neighbor at inside post a recieve*/
      
      MPI::COMM_WORLD.Recv(grid.dLocalGridNew,1
        ,messPass.typeRecvNewVar[procTop.nRadialNeighborNeighborIDs[i]][grid.nU0]
        ,procTop.nRadialNeighborRanks[i],2);
    }
  }
  
  #if SEDOV==1
    //calculate grid velocities for local grid at inner most ghost region
    for(i=grid.nStartGhostUpdateExplicit[grid.nU0][1][0];
      i<grid.nEndGhostUpdateExplicit[grid.nU0][1][0];i++){//nU0 needs to be 1D
      grid.dLocalGridNew[grid.nU0][i][0][0]=grid.dLocalGridNew[grid.nU][i][0][0];
    }
  #endif
  
  //calculate grid velocities for local grid
  double dCSum;
  double dARhoSum;
  int nICen;
  int j;
  int nJInt;
  int k;
  double dR_im1half_np1half;
  double dR_ip1half_np1half;
  double dRSq_im1half_np1half;
  double dRSq_ip1half_np1half;
  double dA_im1halfjk_np1half;
  double dA_ip1halfjk_np1half;
  double d1half_RSq_ip1half_m_RSq_im1half;
  double dA_ijm1halfk_np1half;
  double dA_ijp1halfk_np1half;
  double dRho_im1halfjk_np1half;
  double dRho_ip1halfjk_np1half;
  double dRho_ijm1halfk_np1half;
  double dRho_ijp1halfk_np1half;
  for(i=grid.nStartUpdateExplicit[grid.nU0][0];i<grid.nEndUpdateExplicit[grid.nU0][0];i++){/*nU0
    needs to be 1D*/
    dCSum=0.0;
    dARhoSum=0.0;
    
    //calculate i of centered quantities
    nICen=i-grid.nCenIntOffset[0];
    dR_im1half_np1half=grid.dLocalGridOld[grid.nR][i-1][0][0];
    dR_ip1half_np1half=grid.dLocalGridOld[grid.nR][i][0][0];
    dRSq_im1half_np1half=dR_im1half_np1half*dR_im1half_np1half;
    dRSq_ip1half_np1half=dR_ip1half_np1half*dR_ip1half_np1half;
    d1half_RSq_ip1half_m_RSq_im1half=0.5*(dRSq_ip1half_np1half-dRSq_im1half_np1half);
    for(j=grid.nStartUpdateExplicit[grid.nU][1];j<grid.nEndUpdateExplicit[grid.nU][1];j++){
      
      //calculate j of interface quantities
      nJInt=j+grid.nCenIntOffset[1];
      
      //calculate areas only dependent on i and j
      dA_im1halfjk_np1half=dRSq_im1half_np1half*grid.dLocalGridOld[grid.nDCosThetaIJK][0][j][0];
      dA_ip1halfjk_np1half=dRSq_ip1half_np1half*grid.dLocalGridOld[grid.nDCosThetaIJK][0][j][0];
      dA_ijm1halfk_np1half=d1half_RSq_ip1half_m_RSq_im1half
        *grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt-1][0];
      dA_ijp1halfk_np1half=d1half_RSq_ip1half_m_RSq_im1half
        *grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt][0];
      
      for(k=grid.nStartUpdateExplicit[grid.nU][2];k<grid.nEndUpdateExplicit[grid.nU][2];k++){
        
        //calculate interpolated quantities
        dRho_im1halfjk_np1half=(grid.dLocalGridOld[grid.nD][nICen][j][k]
          +grid.dLocalGridOld[grid.nD][nICen-1][j][k])*0.5;
        dRho_ip1halfjk_np1half=(grid.dLocalGridOld[grid.nD][nICen][j][k]
          +grid.dLocalGridOld[grid.nD][nICen+1][j][k])*0.5;
        dRho_ijm1halfk_np1half=(grid.dLocalGridOld[grid.nD][nICen][j-1][k]
          +grid.dLocalGridOld[grid.nD][nICen][j][k])*0.5;
        dRho_ijp1halfk_np1half=(grid.dLocalGridOld[grid.nD][nICen][j+1][k]
          +grid.dLocalGridOld[grid.nD][nICen][j][k])*0.5;
        
        //calculate sum
        dCSum+=(grid.dLocalGridNew[grid.nU][i-1][j][k]-grid.dLocalGridNew[grid.nU0][i-1][0][0])
          *dA_im1halfjk_np1half*dRho_im1halfjk_np1half
          -grid.dLocalGridNew[grid.nU][i][j][k]*dA_ip1halfjk_np1half*dRho_ip1halfjk_np1half
          +grid.dLocalGridNew[grid.nV][nICen][nJInt-1][k]*dA_ijm1halfk_np1half
          *dRho_ijm1halfk_np1half
          -grid.dLocalGridNew[grid.nV][nICen][nJInt][k]*dA_ijp1halfk_np1half
          *dRho_ijp1halfk_np1half;
        dARhoSum+=dA_ip1halfjk_np1half*dRho_ip1halfjk_np1half;
      }
    }
    grid.dLocalGridNew[grid.nU0][i][0][0]=-1.0*dCSum/dARhoSum;
  }
  
  //post a blocking send to outer radial neighbour
  int nNumRecieves=0;
  for(i=0;i<procTop.nNumRadialNeighbors;i++){
    if(procTop.nCoords[procTop.nRank][0]<procTop.nCoords[procTop.nRadialNeighborRanks[i]][0]){/*if
      current processor has a radial neighbor at inside post a recieve*/
      MPI::COMM_WORLD.Send(grid.dLocalGridNew,1
        ,messPass.typeSendNewVar[procTop.nRadialNeighborNeighborIDs[i]][grid.nU0]
        ,procTop.nRadialNeighborRanks[i],2);
      nNumRecieves++;
    }
  }
  
  //post a non-blocking recieve for outer radial neighbour
  MPI::Request *requestTempRecv=new MPI::Request[nNumRecieves];
  int nCount=0;
  for(i=0;i<procTop.nNumRadialNeighbors;i++){
    if(procTop.nCoords[procTop.nRank][0]<procTop.nCoords[procTop.nRadialNeighborRanks[i]][0]){/*if
      current processor has a radial neighbor at inside post a recieve*/
      requestTempRecv[nCount]=MPI::COMM_WORLD.Irecv(grid.dLocalGridNew,1
        ,messPass.typeRecvNewVar[procTop.nRadialNeighborNeighborIDs[i]][grid.nU0]
        ,procTop.nRadialNeighborRanks[i],2);
      nCount++;
    }
  }
  
  //post a blocking recieve for inner radial neighbour
  for(i=0;i<procTop.nNumRadialNeighbors;i++){
    if(procTop.nCoords[procTop.nRank][0]>procTop.nCoords[procTop.nRadialNeighborRanks[i]][0]){/*if
      current processor has a radial neighbor at inside post a recieve*/
      MPI::COMM_WORLD.Send(grid.dLocalGridNew,1
        ,messPass.typeSendNewVar[procTop.nRadialNeighborNeighborIDs[i]][grid.nU0]
        ,procTop.nRadialNeighborRanks[i],2);
    }
  }
  
  //calculate outermost grid velocity
  for(i=grid.nStartGhostUpdateExplicit[grid.nU0][0][0];i<grid.nEndGhostUpdateExplicit[grid.nU0][0][0];i++){
    dCSum=0.0;
    dARhoSum=0.0;
    
    //calculate i of centered quantities
    nICen=i-grid.nCenIntOffset[0];
    dR_im1half_np1half=grid.dLocalGridOld[grid.nR][i-1][0][0];
    dR_ip1half_np1half=grid.dLocalGridOld[grid.nR][i][0][0];
    dRSq_im1half_np1half=dR_im1half_np1half*dR_im1half_np1half;
    dRSq_ip1half_np1half=dR_ip1half_np1half*dR_ip1half_np1half;
    d1half_RSq_ip1half_m_RSq_im1half=0.5*(dRSq_ip1half_np1half-dRSq_im1half_np1half);
    
    for(j=grid.nStartUpdateExplicit[grid.nU][1];j<grid.nEndUpdateExplicit[grid.nU][1];j++){
      
      //calculate j of interface quantities
      nJInt=j+grid.nCenIntOffset[1];
      
      dA_im1halfjk_np1half=dRSq_im1half_np1half*grid.dLocalGridOld[grid.nDCosThetaIJK][0][j][0];
      dA_ip1halfjk_np1half=dRSq_ip1half_np1half*grid.dLocalGridOld[grid.nDCosThetaIJK][0][j][0];
      dA_ijm1halfk_np1half=d1half_RSq_ip1half_m_RSq_im1half
        *grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][j-1][0];
      dA_ijp1halfk_np1half=d1half_RSq_ip1half_m_RSq_im1half
        *grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][j][0];
      
      for(k=grid.nStartUpdateExplicit[grid.nU][2];k<grid.nEndUpdateExplicit[grid.nU][2];k++){
        dRho_im1halfjk_np1half=(grid.dLocalGridOld[grid.nD][nICen][j][k]
          +grid.dLocalGridOld[grid.nD][nICen-1][j][k])*0.5;
        /**\BC grid.dLocalGridOld[grid.nD][i+1][j][k] is missing*/
        dRho_ip1halfjk_np1half=(grid.dLocalGridOld[grid.nD][nICen][j][k])*0.5;
        dRho_ijm1halfk_np1half=(grid.dLocalGridOld[grid.nD][nICen][j-1][k]
          +grid.dLocalGridOld[grid.nD][nICen][j][k])*0.5;
        dRho_ijp1halfk_np1half=(grid.dLocalGridOld[grid.nD][nICen][j+1][k]
          +grid.dLocalGridOld[grid.nD][nICen][j][k])*0.5;
        
        dCSum+=(grid.dLocalGridNew[grid.nU][i-1][j][k]-grid.dLocalGridNew[grid.nU0][i-1][0][0])
          *dA_im1halfjk_np1half*dRho_im1halfjk_np1half
          -grid.dLocalGridNew[grid.nU][i][j][k]*dA_ip1halfjk_np1half*dRho_ip1halfjk_np1half
          +grid.dLocalGridNew[grid.nV][nICen][nJInt-1][k]*dA_ijm1halfk_np1half
          *dRho_ijm1halfk_np1half
          -grid.dLocalGridNew[grid.nV][nICen][nJInt][k]*dA_ijp1halfk_np1half
          *dRho_ijp1halfk_np1half;
        dARhoSum+=dA_ip1halfjk_np1half*dRho_ip1halfjk_np1half;
      }
    }
    grid.dLocalGridNew[grid.nU0][i][0][0]=-1.0*dCSum/dARhoSum;
    
    //set U equal to U0 at surface
   for(j=0;j<grid.nLocalGridDims[procTop.nRank][grid.nU][1]+2*grid.nNumGhostCells;j++){
      for(k=0;k<grid.nLocalGridDims[procTop.nRank][grid.nU][2];k++){
        grid.dLocalGridNew[grid.nU][i][j][k]=grid.dLocalGridNew[grid.nU0][i][0][0];
      }
    }
  }
  
  //wait for all recieves to complete
  MPI::Status *statusTempRecv=new MPI::Status[nNumRecieves];
  MPI::Request::Waitall(nNumRecieves,requestTempRecv,statusTempRecv);
  delete [] requestTempRecv;
  delete [] statusTempRecv;
}
void calNewU0_RTP(Grid &grid,Parameters &parameters,Time &time,ProcTop &procTop,MessPass &messPass){//resume, moving decalerations
  
  /**\todo
    At some point I will likely want to make this funciton compatiable with a 3D domain 
    decomposition instead of a purely radial domain decomposition.
  */
  
  //post a blocking recieve from inner radial neighbour
  int i;
  for(i=0;i<procTop.nNumRadialNeighbors;i++){
    if(procTop.nCoords[procTop.nRank][0]>procTop.nCoords[procTop.nRadialNeighborRanks[i]][0]){/*
      if current processor has a radial neighbor at inside post a recieve*/
      MPI::COMM_WORLD.Recv(grid.dLocalGridNew,1
        ,messPass.typeRecvNewVar[procTop.nRadialNeighborNeighborIDs[i]][grid.nU0]
        ,procTop.nRadialNeighborRanks[i],2);
    }
  }
  
  #if SEDOV==1
    //calculate grid velocities for local grid at inner most ghost region
    for(i=grid.nStartGhostUpdateExplicit[grid.nU0][1][0];
      i<grid.nEndGhostUpdateExplicit[grid.nU0][1][0];i++){//nU0 needs to be 1D
      grid.dLocalGridNew[grid.nU0][i][0][0]=grid.dLocalGridNew[grid.nU][i][0][0];
    }
  #endif
  
  //calculate grid velocities for local grid
  double dCSum;
  double dARhoSum;
  int nICen;
  int j;
  int k;
  int nJInt;
  double dR_im1half_np1half;
  double dR_ip1half_np1half;
  double dRSq_im1half_np1half;
  double dRSq_ip1half_np1half;
  double d1half_RSq_ip1half_m_RSq_im1half;
  double dTemp;
  double dA_ip1halfjk_np1half;
  double dA_im1halfjk_np1half;
  double dA_ijm1halfk_np1half;
  double dA_ijp1halfk_np1half;
  double dA_ijkm1half_np1half;
  double dA_ijkp1half_np1half;
  double dRho_im1halfjk_np1half;
  
  for(i=grid.nStartUpdateExplicit[grid.nU0][0];i<grid.nEndUpdateExplicit[grid.nU0][0];i++){/*nU0
    needs to be 1D*/
    dCSum=0.0;
    dARhoSum=0.0;
    
    //calculate i of centered quantities
    nICen=i-grid.nCenIntOffset[0];
    
    dR_im1half_np1half=grid.dLocalGridOld[grid.nR][i-1][0][0];
    dR_ip1half_np1half=grid.dLocalGridOld[grid.nR][i][0][0];
    dRSq_im1half_np1half=dR_im1half_np1half*dR_im1half_np1half;
    dRSq_ip1half_np1half=dR_ip1half_np1half*dR_ip1half_np1half;
    d1half_RSq_ip1half_m_RSq_im1half=0.5*(dRSq_ip1half_np1half-dRSq_im1half_np1half);
    for(j=grid.nStartUpdateExplicit[grid.nU][1];j<grid.nEndUpdateExplicit[grid.nU][1];j++){
      
      //calculate j of interface quantities
      nJInt=j+grid.nCenIntOffset[1];
      
      for(k=grid.nStartUpdateExplicit[grid.nU][2];k<grid.nEndUpdateExplicit[grid.nU][2];k++){
        
          
        dTemp=grid.dLocalGridOld[grid.nDCosThetaIJK][0][j][0]
          *grid.dLocalGridOld[grid.nDPhi][0][0][k];
        dA_im1halfjk_np1half=dRSq_im1half_np1half*dTemp;
        dA_ip1halfjk_np1half=dRSq_ip1half_np1half*dTemp;
        dA_ijm1halfk_np1half=d1half_RSq_ip1half_m_RSq_im1half
          *grid.dLocalGridOld[grid.nDPhi][0][0][k]
          *grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt-1][0];
         dA_ijp1halfk_np1half=d1half_RSq_ip1half_m_RSq_im1half
          *grid.dLocalGridOld[grid.nDPhi][0][0][k]
          *grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt][0];
        dA_ijkm1half_np1half=d1half_RSq_ip1half_m_RSq_im1half
          *grid.dLocalGridOld[grid.nDTheta][0][j][0];
        dA_ijkp1half_np1half=dA_ijkm1half_np1half;
        dRho_im1halfjk_np1half=(grid.dLocalGridOld[grid.nD][nICen][j][k]
          +grid.dLocalGridOld[grid.nD][nICen-1][j][k])*0.5;
        double dRho_ip1halfjk_np1half=(grid.dLocalGridOld[grid.nD][nICen][j][k]
          +grid.dLocalGridOld[grid.nD][nICen+1][j][k])*0.5;
        double dRho_ijm1halfk_np1half=(grid.dLocalGridOld[grid.nD][nICen][j-1][k]
          +grid.dLocalGridOld[grid.nD][nICen][j][k])*0.5;
        double dRho_ijp1halfk_np1half=(grid.dLocalGridOld[grid.nD][nICen][j+1][k]
          +grid.dLocalGridOld[grid.nD][nICen][j][k])*0.5;
        double dRho_ijkm1half_np1half=(grid.dLocalGridOld[grid.nD][nICen][j][k-1]
          +grid.dLocalGridOld[grid.nD][nICen][j][k])*0.5;
        double dRho_ijkp1half_np1half=(grid.dLocalGridOld[grid.nD][nICen][j][k+1]
          +grid.dLocalGridOld[grid.nD][nICen][j][k])*0.5;
        
        dCSum+=(grid.dLocalGridNew[grid.nU][i-1][j][k]-grid.dLocalGridNew[grid.nU0][i-1][0][0])
          *dA_im1halfjk_np1half*dRho_im1halfjk_np1half
          -grid.dLocalGridNew[grid.nU][i][j][k]*dA_ip1halfjk_np1half*dRho_ip1halfjk_np1half
          +grid.dLocalGridNew[grid.nV][nICen][nJInt-1][k]*dA_ijm1halfk_np1half
          *dRho_ijm1halfk_np1half
          -grid.dLocalGridNew[grid.nV][nICen][nJInt][k]*dA_ijp1halfk_np1half
          *dRho_ijp1halfk_np1half
          +grid.dLocalGridNew[grid.nW][nICen][j][k-1]*dA_ijkm1half_np1half
          *dRho_ijkm1half_np1half-grid.dLocalGridNew[grid.nW][nICen][j][k]*dA_ijkp1half_np1half
          *dRho_ijkp1half_np1half;
        dARhoSum+=dA_ip1halfjk_np1half*dRho_ip1halfjk_np1half;
      }
    }
    grid.dLocalGridNew[grid.nU0][i][0][0]=-1.0*dCSum/dARhoSum;
  }
  
  //post a blocking send to outer radial neighbour
  int nNumRecieves=0;
  for(int i=0;i<procTop.nNumRadialNeighbors;i++){
    if(procTop.nCoords[procTop.nRank][0]<procTop.nCoords[procTop.nRadialNeighborRanks[i]][0]){/*
      if current processor has a radial neighbor at inside post a recieve*/
      MPI::COMM_WORLD.Send(grid.dLocalGridNew,1
        ,messPass.typeSendNewVar[procTop.nRadialNeighborNeighborIDs[i]][grid.nU0]
        ,procTop.nRadialNeighborRanks[i],2);
      nNumRecieves++;
    }
  }
  
  //post a non-blocking recieve for outer radial neighbour
  MPI::Request *requestTempRecv=new MPI::Request[nNumRecieves];
  int nCount=0;
  for(int i=0;i<procTop.nNumRadialNeighbors;i++){
    if(procTop.nCoords[procTop.nRank][0]<procTop.nCoords[procTop.nRadialNeighborRanks[i]][0]){/*
      if current processor has a radial neighbor at inside post a recieve*/
      requestTempRecv[nCount]=MPI::COMM_WORLD.Irecv(grid.dLocalGridNew,1
        ,messPass.typeRecvNewVar[procTop.nRadialNeighborNeighborIDs[i]][grid.nU0]
        ,procTop.nRadialNeighborRanks[i],2);
      nCount++;
    }
  }
  
  //post a blocking recieve for inner radial neighbour
  for(int i=0;i<procTop.nNumRadialNeighbors;i++){
    if(procTop.nCoords[procTop.nRank][0]>procTop.nCoords[procTop.nRadialNeighborRanks[i]][0]){/*
      if current processor has a radial neighbor at inside post a recieve*/
      MPI::COMM_WORLD.Send(grid.dLocalGridNew,1
        ,messPass.typeSendNewVar[procTop.nRadialNeighborNeighborIDs[i]][grid.nU0]
        ,procTop.nRadialNeighborRanks[i],2);
    }
  }
  
  //calculate outermost grid velocity
  for(int i=grid.nStartGhostUpdateExplicit[grid.nU0][0][0];i<grid.nEndGhostUpdateExplicit[grid.nU0][0][0];i++){
    double dCSum=0.0;
    double dARhoSum=0.0;
    
    //calculate i of centered quantities
    int nICen=i-grid.nCenIntOffset[0];
    
    for(int j=grid.nStartUpdateExplicit[grid.nU][1];j<grid.nEndUpdateExplicit[grid.nU][1];j++){
      
      //calculate j of interface quantities
      int nJInt=j+grid.nCenIntOffset[1];
      
      for(int k=grid.nStartUpdateExplicit[grid.nU][2];k<grid.nEndUpdateExplicit[grid.nU][2];k++){
        double dR_im1half_np1half=grid.dLocalGridOld[grid.nR][i-1][0][0];
        double dR_ip1half_np1half=grid.dLocalGridOld[grid.nR][i][0][0];
        double dRSq_im1half_np1half=dR_im1half_np1half*dR_im1half_np1half;
        double dRSq_ip1half_np1half=dR_ip1half_np1half*dR_ip1half_np1half;
        double dTemp=grid.dLocalGridOld[grid.nDCosThetaIJK][0][j][0]
          *grid.dLocalGridOld[grid.nDPhi][0][0][k];
        double dA_im1halfjk_np1half=dRSq_im1half_np1half*dTemp;
        double dA_ip1halfjk_np1half=dRSq_ip1half_np1half*dTemp;
        dTemp=0.5*(dRSq_ip1half_np1half-dRSq_im1half_np1half)
          *grid.dLocalGridOld[grid.nDPhi][0][0][k];
        double dA_ijm1halfk_np1half=dTemp*grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][j-1][0];
        double dA_ijp1halfk_np1half=dTemp*grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][j][0];
        double dA_ijkm1half_np1half=0.5*(dRSq_ip1half_np1half-dRSq_im1half_np1half)
          *grid.dLocalGridOld[grid.nDTheta][0][j][0];
        double dA_ijkp1half_np1half=dA_ijkm1half_np1half;
        double dRho_im1halfjk_np1half=(grid.dLocalGridOld[grid.nD][nICen][j][k]
          +grid.dLocalGridOld[grid.nD][nICen-1][j][k])*0.5;
        /**\BC grid.dLocalGridOld[grid.nD][i+1][j][k] is missing*/
        double dRho_ip1halfjk_np1half=(grid.dLocalGridOld[grid.nD][nICen][j][k])*0.5;
        double dRho_ijm1halfk_np1half=(grid.dLocalGridOld[grid.nD][nICen][j-1][k]
          +grid.dLocalGridOld[grid.nD][nICen][j][k])*0.5;
        double dRho_ijp1halfk_np1half=(grid.dLocalGridOld[grid.nD][nICen][j+1][k]
          +grid.dLocalGridOld[grid.nD][nICen][j][k])*0.5;
        double dRho_ijkm1half_np1half=(grid.dLocalGridOld[grid.nD][nICen][j][k-1]
          +grid.dLocalGridOld[grid.nD][nICen][j][k])*0.5;
        double dRho_ijkp1half_np1half=(grid.dLocalGridOld[grid.nD][nICen][j][k+1]
          +grid.dLocalGridOld[grid.nD][nICen][j][k])*0.5;
        
        dCSum+=(grid.dLocalGridNew[grid.nU][i-1][j][k]-grid.dLocalGridNew[grid.nU0][i-1][0][0])
          *dA_im1halfjk_np1half*dRho_im1halfjk_np1half
          -grid.dLocalGridNew[grid.nU][i][j][k]*dA_ip1halfjk_np1half*dRho_ip1halfjk_np1half
          +grid.dLocalGridNew[grid.nV][nICen][nJInt-1][k]*dA_ijm1halfk_np1half
          *dRho_ijm1halfk_np1half
          -grid.dLocalGridNew[grid.nV][nICen][nJInt][k]*dA_ijp1halfk_np1half
          *dRho_ijp1halfk_np1half
          +grid.dLocalGridNew[grid.nW][nICen][j][k-1]*dA_ijkm1half_np1half
          *dRho_ijkm1half_np1half
          -grid.dLocalGridNew[grid.nW][nICen][j][k]*dA_ijkp1half_np1half
          *dRho_ijkp1half_np1half;
        dARhoSum+=dA_ip1halfjk_np1half*dRho_ip1halfjk_np1half;
      }
    }
    grid.dLocalGridNew[grid.nU0][i][0][0]=-1.0*dCSum/dARhoSum;
    
    //set U equal to U0
    for(int j=0;j<grid.nLocalGridDims[procTop.nRank][grid.nU][1]+2*grid.nNumGhostCells;j++){
      for(int k=0;k<grid.nLocalGridDims[procTop.nRank][grid.nU][2]+2*grid.nNumGhostCells;k++){
        grid.dLocalGridNew[grid.nU][i][j][k]=grid.dLocalGridNew[grid.nU0][i][0][0];
      }
    }
  }
  
  //wait for all recieves to complete
  MPI::Status *statusTempRecv=new MPI::Status[nNumRecieves];
  MPI::Request::Waitall(nNumRecieves,requestTempRecv,statusTempRecv);
  delete [] requestTempRecv;
  delete [] statusTempRecv;
}
void calNewR(Grid &grid, Time &time){
  
  for(int i=grid.nStartUpdateExplicit[grid.nR][0];i<grid.nEndUpdateExplicit[grid.nR][0];i++){//nR needs to be 1D
    grid.dLocalGridNew[grid.nR][i][0][0]=grid.dLocalGridOld[grid.nR][i][0][0]
      +time.dDeltat_np1half*grid.dLocalGridNew[grid.nU0][i][0][0];
  }
  for(int l=0;l<6;l++){
    for(int i=grid.nStartGhostUpdateExplicit[grid.nR][l][0];i<grid.nEndGhostUpdateExplicit[grid.nR][l][0];i++){
      grid.dLocalGridNew[grid.nR][i][0][0]=grid.dLocalGridOld[grid.nR][i][0][0]
        +time.dDeltat_np1half*grid.dLocalGridNew[grid.nU0][i][0][0];
    }
  }
}
void calNewD_R(Grid &grid, Parameters &parameters, Time &time,ProcTop &procTop){
  int i;
  int j;
  int k;
  int nIInt;
  int nJInt;
  int nKInt;
  double dDelRCu_i_n;
  double dDelRCu_i_np1;
  double dVRatio;
  double dR_ip1half_np1half;
  double dR_im1half_np1half;
  double dRSq_ip1half_np1half;
  double dRSq_im1half_np1half;
  double dDelRSq_i_np1half;
  double dV_np1;
  double dA_im1half;
  double dA_ip1half;
  double dRho_im1half;
  double dRho_ip1half;
  double dDeltaRhoR;
  double dVr_np1;
  double d1Thrid=0.333333333333333333333333333333;
  for(i=grid.nStartUpdateExplicit[grid.nD][0];i<grid.nEndUpdateExplicit[grid.nD][0];i++){
    
    //calculate i for interface centered quantities
    nIInt=i+grid.nCenIntOffset[0];
    dDelRCu_i_n=(pow(grid.dLocalGridOld[grid.nR][nIInt][0][0],3.0)
          -pow(grid.dLocalGridOld[grid.nR][nIInt-1][0][0],3.0));
    dDelRCu_i_np1=(pow(grid.dLocalGridNew[grid.nR][nIInt][0][0],3.0)
          -pow(grid.dLocalGridNew[grid.nR][nIInt-1][0][0],3.0));
    dR_ip1half_np1half=grid.dLocalGridOld[grid.nR][nIInt][0][0];//could be time centered
    dR_im1half_np1half=grid.dLocalGridOld[grid.nR][nIInt-1][0][0];//could be time centered
    dRSq_ip1half_np1half=dR_ip1half_np1half*dR_ip1half_np1half;
    dRSq_im1half_np1half=dR_im1half_np1half*dR_im1half_np1half;
    dDelRSq_i_np1half=dRSq_ip1half_np1half-dRSq_im1half_np1half;
    dVRatio=dDelRCu_i_n/dDelRCu_i_np1;//calculate ratio of volume at n to volume at n+1
    
    for(j=grid.nStartUpdateExplicit[grid.nD][1];j<grid.nEndUpdateExplicit[grid.nD][1];j++){
      
      for(k=grid.nStartUpdateExplicit[grid.nD][2];k<grid.nEndUpdateExplicit[grid.nD][2];k++){
        
        dV_np1=d1Thrid*dDelRCu_i_np1;
          
        //CALCULATE RATE OF CHANGE IN RHO IN RADIAL DIRECTION
        
        //calculate area at i-1/2
        dA_im1half=dRSq_im1half_np1half;
        
        //calculate area at i+1/2
        dA_ip1half=dRSq_ip1half_np1half;
        
        //calculate rho at i-1/2, not time centered
        dRho_im1half=(grid.dLocalGridOld[grid.nD][i][j][k]+grid.dLocalGridOld[grid.nD][i-1][j][k])
          *0.5;
        
        //calculate rho at i+1/2, not time centered
        dRho_ip1half=(grid.dLocalGridOld[grid.nD][i][j][k]+grid.dLocalGridOld[grid.nD][i+1][j][k])
          *0.5;
        
        //calculate radial term
        dDeltaRhoR=(grid.dLocalGridNew[grid.nU][nIInt-1][j][k]
          -grid.dLocalGridNew[grid.nU0][nIInt-1][0][0])*dRho_im1half*dA_im1half
          -(grid.dLocalGridNew[grid.nU][nIInt][j][k]-grid.dLocalGridNew[grid.nU0][nIInt][0][0])
          *dRho_ip1half*dA_ip1half;
        
        //calculate new density
        grid.dLocalGridNew[grid.nD][i][j][k]=dVRatio*grid.dLocalGridOld[grid.nD][i][j][k]
          +time.dDeltat_np1half*(dDeltaRhoR)/dV_np1;
        
        if(grid.dLocalGridNew[grid.nD][i][j][k]<0.0){
          
          #if SIGNEGDEN==1
          raise(SIGINT);
          #endif
          
          std::stringstream ssTemp;
          ssTemp<<__FILE__<<":"<<__FUNCTION__<<":"<<__LINE__<<":"<<procTop.nRank
            <<": negative density calculated in , ("<<i<<","<<j<<","<<k<<")\n";
          throw exception2(ssTemp.str(),CALCULATION);
        }
      }
    }
  }
  
  //ghost region 0, outter most ghost region in x1 direction
  for(i=grid.nStartGhostUpdateExplicit[grid.nD][0][0]
    ;i<grid.nEndGhostUpdateExplicit[grid.nD][0][0];i++){
    
    //calculate i for interface centered quantities
    nIInt=i+grid.nCenIntOffset[0];
    dDelRCu_i_n=(pow(grid.dLocalGridOld[grid.nR][nIInt][0][0],3.0)
          -pow(grid.dLocalGridOld[grid.nR][nIInt-1][0][0],3.0));
    dDelRCu_i_np1=(pow(grid.dLocalGridNew[grid.nR][nIInt][0][0],3.0)
          -pow(grid.dLocalGridNew[grid.nR][nIInt-1][0][0],3.0));
    dR_ip1half_np1half=grid.dLocalGridOld[grid.nR][nIInt][0][0];//could be time centered
    dR_im1half_np1half=grid.dLocalGridOld[grid.nR][nIInt-1][0][0];//could be time centered
    dRSq_ip1half_np1half=dR_ip1half_np1half*dR_ip1half_np1half;
    dRSq_im1half_np1half=dR_im1half_np1half*dR_im1half_np1half;
    dDelRSq_i_np1half=dRSq_ip1half_np1half-dRSq_im1half_np1half;
    dVRatio=dDelRCu_i_n/dDelRCu_i_np1;//calculate ratio of volume at n to volume at n+1
    
    for(j=grid.nStartUpdateExplicit[grid.nD][1];j<grid.nEndUpdateExplicit[grid.nD][1];j++){
      
      for(k=grid.nStartUpdateExplicit[grid.nD][2];k<grid.nEndUpdateExplicit[grid.nD][2];k++){
        
        dV_np1=d1Thrid*dDelRCu_i_np1;
          
        //CALCULATE RATE OF CHANGE IN RHO IN RADIAL DIRECTION
        
        //calculate area at i-1/2
        dA_im1half=dRSq_im1half_np1half;
        
        //calculate rho at i-1/2, not time centered
        dRho_im1half=(grid.dLocalGridOld[grid.nD][i][j][k]+grid.dLocalGridOld[grid.nD][i-1][j][k])
          *0.5;
        
        //calculate radial term
        dDeltaRhoR=(grid.dLocalGridNew[grid.nU][nIInt-1][j][k]
          -grid.dLocalGridNew[grid.nU0][nIInt-1][0][0])*dRho_im1half*dA_im1half;
        
        //calculate new density
        /**\BC doesn't allow mass flux through outter interface*/
        grid.dLocalGridNew[grid.nD][i][j][k]=dVRatio*grid.dLocalGridOld[grid.nD][i][j][k]
          +time.dDeltat_np1half*(dDeltaRhoR)/dV_np1;
        
        if(grid.dLocalGridNew[grid.nD][i][j][k]<0.0){
          
          #if SIGNEGDEN==1
          raise(SIGINT);
          #endif
          
          std::stringstream ssTemp;
          ssTemp<<__FILE__<<":"<<__FUNCTION__<<":"<<__LINE__<<":"<<procTop.nRank
            <<": negative density calculated in , ("<<i<<","<<j<<","<<k<<")\n";
          throw exception2(ssTemp.str(),CALCULATION);
        }
      }
    }
  }

  #if SEDOV==1
    
    //ghost region 1, inner most ghost region in x1 direction
    for(i=grid.nStartGhostUpdateExplicit[grid.nD][1][0];
      i<grid.nEndGhostUpdateExplicit[grid.nD][1][0];i++){
      
      //calculate i for interface centered quantities
      nIInt=i+grid.nCenIntOffset[0];
      dDelRCu_i_n=(pow(grid.dLocalGridOld[grid.nR][nIInt][0][0],3.0)
            -pow(grid.dLocalGridOld[grid.nR][nIInt-1][0][0],3.0));
      dDelRCu_i_np1=(pow(grid.dLocalGridNew[grid.nR][nIInt][0][0],3.0)
            -pow(grid.dLocalGridNew[grid.nR][nIInt-1][0][0],3.0));
      dR_ip1half_np1half=grid.dLocalGridOld[grid.nR][nIInt][0][0];//could be time centered
      dR_im1half_np1half=grid.dLocalGridOld[grid.nR][nIInt-1][0][0];//could be time centered
      dRSq_ip1half_np1half=dR_ip1half_np1half*dR_ip1half_np1half;
      dRSq_im1half_np1half=dR_im1half_np1half*dR_im1half_np1half;
      dDelRSq_i_np1half=dRSq_ip1half_np1half-dRSq_im1half_np1half;
      dVRatio=dDelRCu_i_n/dDelRCu_i_np1;//calculate ratio of volume at n to volume at n+1
      
      for(j=grid.nStartGhostUpdateExplicit[grid.nD][1][1];
        j<grid.nEndGhostUpdateExplicit[grid.nD][1][1];j++){
        
        for(k=grid.nStartGhostUpdateExplicit[grid.nD][1][2];
          k<grid.nEndGhostUpdateExplicit[grid.nD][1][2];k++){
          
          dV_np1=d1Thrid*dDelRCu_i_np1;
            
          //CALCULATE RATE OF CHANGE IN RHO IN RADIAL DIRECTION
          
          //calculate area at i+1/2
          dA_ip1half=dRSq_ip1half_np1half;
          
          //calculate rho at i+1/2, not time centered
          dRho_ip1half=(grid.dLocalGridOld[grid.nD][i][j][k]+grid.dLocalGridOld[grid.nD][i+1][j][k])
            *0.5;
          
          //calculate radial term
          dDeltaRhoR=-(grid.dLocalGridNew[grid.nU][nIInt][j][k]
            -grid.dLocalGridNew[grid.nU0][nIInt][0][0])*dRho_ip1half*dA_ip1half;
          
          //calculate new density
          grid.dLocalGridNew[grid.nD][i][j][k]=dVRatio*grid.dLocalGridOld[grid.nD][i][j][k]
            +time.dDeltat_np1half*(dDeltaRhoR)/dV_np1;
          
          if(grid.dLocalGridNew[grid.nD][i][j][k]<0.0){
            
            #if SIGNEGDEN==1
            raise(SIGINT);
            #endif
            
            std::stringstream ssTemp;
            ssTemp<<__FILE__<<":"<<__FUNCTION__<<":"<<__LINE__<<":"<<procTop.nRank
              <<": negative density calculated in , ("<<i<<","<<j<<","<<k<<")\n";
            throw exception2(ssTemp.str(),CALCULATION);
          }
        }
      }
    }
  #endif
}
void calNewD_RT(Grid &grid, Parameters &parameters, Time &time,ProcTop &procTop){
  int i;
  int j;
  int k;
  int nIInt;
  int nJInt;
  int nKInt;
  double dDelRCu_i_n;
  double dDelRCu_i_np1;
  double dVRatio;
  double dR_ip1half_np1half;
  double dR_im1half_np1half;
  double dRSq_ip1half_np1half;
  double dRSq_im1half_np1half;
  double dDelRSq_i_np1half;
  double dDelCosTheta;
  double dV_np1;
  double dA_im1half;
  double dA_ip1half;
  double dRho_im1half;
  double dRho_ip1half;
  double dDeltaRhoR;
  double dA_jm1half;
  double dA_jp1half;
  double dRho_jm1half;
  double dRho_jp1half;
  double dDeltaRhoTheta;
  double dVr_np1;
  double d1Thrid=0.333333333333333333333333333333;
  for(i=grid.nStartUpdateExplicit[grid.nD][0];i<grid.nEndUpdateExplicit[grid.nD][0];i++){
    
    //calculate i for interface centered quantities
    nIInt=i+grid.nCenIntOffset[0];
    dDelRCu_i_n=(pow(grid.dLocalGridOld[grid.nR][nIInt][0][0],3.0)
          -pow(grid.dLocalGridOld[grid.nR][nIInt-1][0][0],3.0));
    dDelRCu_i_np1=(pow(grid.dLocalGridNew[grid.nR][nIInt][0][0],3.0)
          -pow(grid.dLocalGridNew[grid.nR][nIInt-1][0][0],3.0));
    dR_ip1half_np1half=grid.dLocalGridOld[grid.nR][nIInt][0][0];//could be time centered
    dR_im1half_np1half=grid.dLocalGridOld[grid.nR][nIInt-1][0][0];//could be time centered
    dRSq_ip1half_np1half=dR_ip1half_np1half*dR_ip1half_np1half;
    dRSq_im1half_np1half=dR_im1half_np1half*dR_im1half_np1half;
    dDelRSq_i_np1half=dRSq_ip1half_np1half-dRSq_im1half_np1half;
    dVRatio=dDelRCu_i_n/dDelRCu_i_np1;//calculate ratio of volume at n to volume at n+1
    
    for(j=grid.nStartUpdateExplicit[grid.nD][1];j<grid.nEndUpdateExplicit[grid.nD][1];j++){
      
      //calculate j for interface centered quantities
      nJInt=j+grid.nCenIntOffset[1];
      dDelCosTheta=grid.dLocalGridOld[grid.nDCosThetaIJK][0][j][0];
      
      for(k=grid.nStartUpdateExplicit[grid.nD][2];k<grid.nEndUpdateExplicit[grid.nD][2];k++){
        
        dV_np1=d1Thrid*dDelRCu_i_np1*dDelCosTheta;
          
        //CALCULATE RATE OF CHANGE IN RHO IN RADIAL DIRECTION
        
        //calculate area at i-1/2
        dA_im1half=dRSq_im1half_np1half*dDelCosTheta;
        
        //calculate area at i+1/2
        dA_ip1half=dRSq_ip1half_np1half*dDelCosTheta;
        
        //calculate rho at i-1/2, not time centered
        dRho_im1half=(grid.dLocalGridOld[grid.nD][i][j][k]+grid.dLocalGridOld[grid.nD][i-1][j][k])
          *0.5;
        
        //calculate rho at i+1/2, not time centered
        dRho_ip1half=(grid.dLocalGridOld[grid.nD][i][j][k]+grid.dLocalGridOld[grid.nD][i+1][j][k])
          *0.5;
        
        //calculate radial term
        dDeltaRhoR=(grid.dLocalGridNew[grid.nU][nIInt-1][j][k]
          -grid.dLocalGridNew[grid.nU0][nIInt-1][0][0])*dRho_im1half*dA_im1half
          -(grid.dLocalGridNew[grid.nU][nIInt][j][k]-grid.dLocalGridNew[grid.nU0][nIInt][0][0])
          *dRho_ip1half*dA_ip1half;
        
        
        //CALCULATE RATE OF CHANGE IN RHO IN THE THETA DIRECTION
        
        //calculate ratio of area at j-1/2,n to volume at i,j,k, n+1
        dA_jm1half=0.5*dDelRSq_i_np1half
          *grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt-1][0];
        
        //calculate ratio of area at j+1/2,n to volume at i,j,k, n+1
        dA_jp1half=0.5*dDelRSq_i_np1half
          *grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt][0];
        
        //calculte rho at j-1/2
        dRho_jm1half=(grid.dLocalGridOld[grid.nD][i][j-1][k]+grid.dLocalGridOld[grid.nD][i][j][k])
          *0.5;
        
        //calculte rho at j+1/2
        dRho_jp1half=(grid.dLocalGridOld[grid.nD][i][j+1][k]+grid.dLocalGridOld[grid.nD][i][j][k])
          *0.5;
        
        //calculate theta term
        dDeltaRhoTheta=grid.dLocalGridNew[grid.nV][i][nJInt-1][k]*dRho_jm1half*dA_jm1half
          -grid.dLocalGridNew[grid.nV][i][nJInt][k]*dRho_jp1half*dA_jp1half;
        
        //calculate new density
        grid.dLocalGridNew[grid.nD][i][j][k]=dVRatio*grid.dLocalGridOld[grid.nD][i][j][k]
          +time.dDeltat_np1half*(dDeltaRhoR+dDeltaRhoTheta)/dV_np1;
        
        if(grid.dLocalGridNew[grid.nD][i][j][k]<0.0){
          
          #if SIGNEGDEN==1
          raise(SIGINT);
          #endif
          
          std::stringstream ssTemp;
          ssTemp<<__FILE__<<":"<<__FUNCTION__<<":"<<__LINE__<<":"<<procTop.nRank
            <<": negative density calculated in , ("<<i<<","<<j<<","<<k<<")\n";
          throw exception2(ssTemp.str(),CALCULATION);
        }
      }
    }
  }
  
  //ghost region 0, outter most ghost region in x1 direction
  for(i=grid.nStartGhostUpdateExplicit[grid.nD][0][0]
    ;i<grid.nEndGhostUpdateExplicit[grid.nD][0][0];i++){
    
    //calculate i for interface centered quantities
    nIInt=i+grid.nCenIntOffset[0];
    dDelRCu_i_n=(pow(grid.dLocalGridOld[grid.nR][nIInt][0][0],3.0)
          -pow(grid.dLocalGridOld[grid.nR][nIInt-1][0][0],3.0));
    dDelRCu_i_np1=(pow(grid.dLocalGridNew[grid.nR][nIInt][0][0],3.0)
          -pow(grid.dLocalGridNew[grid.nR][nIInt-1][0][0],3.0));
    dR_ip1half_np1half=grid.dLocalGridOld[grid.nR][nIInt][0][0];//could be time centered
    dR_im1half_np1half=grid.dLocalGridOld[grid.nR][nIInt-1][0][0];//could be time centered
    dRSq_ip1half_np1half=dR_ip1half_np1half*dR_ip1half_np1half;
    dRSq_im1half_np1half=dR_im1half_np1half*dR_im1half_np1half;
    dDelRSq_i_np1half=dRSq_ip1half_np1half-dRSq_im1half_np1half;
    dVRatio=dDelRCu_i_n/dDelRCu_i_np1;//calculate ratio of volume at n to volume at n+1
    
    for(j=grid.nStartUpdateExplicit[grid.nD][1];j<grid.nEndUpdateExplicit[grid.nD][1];j++){
      
      //calculate j for interface centered quantities
      nJInt=j+grid.nCenIntOffset[1];
      
      for(k=grid.nStartUpdateExplicit[grid.nD][2];k<grid.nEndUpdateExplicit[grid.nD][2];k++){
        
        dDelCosTheta=grid.dLocalGridOld[grid.nDCosThetaIJK][0][j][0];
        dV_np1=d1Thrid*dDelRCu_i_np1*dDelCosTheta;
          
        //CALCULATE RATE OF CHANGE IN RHO IN RADIAL DIRECTION
        
        //calculate area at i-1/2
        dA_im1half=dRSq_im1half_np1half*dDelCosTheta;
        
        //calculate rho at i-1/2, not time centered
        dRho_im1half=(grid.dLocalGridOld[grid.nD][i][j][k]+grid.dLocalGridOld[grid.nD][i-1][j][k])
          *0.5;
        
        //calculate radial term
        dDeltaRhoR=(grid.dLocalGridNew[grid.nU][nIInt-1][j][k]
          -grid.dLocalGridNew[grid.nU0][nIInt-1][0][0])*dRho_im1half*dA_im1half;
        
        
        //CALCULATE RATE OF CHANGE IN RHO IN THE THETA DIRECTION
        
        //calculate ratio of area at j-1/2,n to volume at i,j,k, n+1
        dA_jm1half=0.5*dDelRSq_i_np1half
          *grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt-1][0];
        
        //calculate ratio of area at j+1/2,n to volume at i,j,k, n+1
        dA_jp1half=0.5*dDelRSq_i_np1half
          *grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt][0];
        
        //calculte rho at j-1/2
        dRho_jm1half=(grid.dLocalGridOld[grid.nD][i][j-1][k]+grid.dLocalGridOld[grid.nD][i][j][k])
          *0.5;
        
        //calculte rho at j+1/2
        dRho_jp1half=(grid.dLocalGridOld[grid.nD][i][j+1][k]+grid.dLocalGridOld[grid.nD][i][j][k])
          *0.5;
        
        //calculate theta term
        dDeltaRhoTheta=grid.dLocalGridNew[grid.nV][i][nJInt-1][k]*dRho_jm1half*dA_jm1half
          -grid.dLocalGridNew[grid.nV][i][nJInt][k]*dRho_jp1half*dA_jp1half;
        
        //calculate new density
        /**\BC doesn't allow mass flux through outter interface*/
        grid.dLocalGridNew[grid.nD][i][j][k]=dVRatio*grid.dLocalGridOld[grid.nD][i][j][k]
          +time.dDeltat_np1half*(dDeltaRhoR+dDeltaRhoTheta)/dV_np1;
        
        if(grid.dLocalGridNew[grid.nD][i][j][k]<0.0){
          
          #if SIGNEGDEN==1
          raise(SIGINT);
          #endif
          
          std::stringstream ssTemp;
          ssTemp<<__FILE__<<":"<<__FUNCTION__<<":"<<__LINE__<<":"<<procTop.nRank
            <<": negative density calculated in , ("<<i<<","<<j<<","<<k<<")\n";
          throw exception2(ssTemp.str(),CALCULATION);
        }
      }
    }
  }

  #if SEDOV==1
    
    //ghost region 1, inner most ghost region in x1 direction
    for(i=grid.nStartGhostUpdateExplicit[grid.nD][1][0];
      i<grid.nEndGhostUpdateExplicit[grid.nD][1][0];i++){
      
      //calculate i for interface centered quantities
      nIInt=i+grid.nCenIntOffset[0];
      dDelRCu_i_n=(pow(grid.dLocalGridOld[grid.nR][nIInt][0][0],3.0)
            -pow(grid.dLocalGridOld[grid.nR][nIInt-1][0][0],3.0));
      dDelRCu_i_np1=(pow(grid.dLocalGridNew[grid.nR][nIInt][0][0],3.0)
            -pow(grid.dLocalGridNew[grid.nR][nIInt-1][0][0],3.0));
      dR_ip1half_np1half=grid.dLocalGridOld[grid.nR][nIInt][0][0];//could be time centered
      dR_im1half_np1half=grid.dLocalGridOld[grid.nR][nIInt-1][0][0];//could be time centered
      dRSq_ip1half_np1half=dR_ip1half_np1half*dR_ip1half_np1half;
      dRSq_im1half_np1half=dR_im1half_np1half*dR_im1half_np1half;
      dDelRSq_i_np1half=dRSq_ip1half_np1half-dRSq_im1half_np1half;
      dVRatio=dDelRCu_i_n/dDelRCu_i_np1;//calculate ratio of volume at n to volume at n+1
      
      for(j=grid.nStartGhostUpdateExplicit[grid.nD][1][1];
        j<grid.nEndGhostUpdateExplicit[grid.nD][1][1];j++){
        
        //calculate j for interface centered quantities
        nJInt=j+grid.nCenIntOffset[1];
        
        for(k=grid.nStartGhostUpdateExplicit[grid.nD][1][2];
          k<grid.nEndGhostUpdateExplicit[grid.nD][1][2];k++){
          
          
          dDelCosTheta=grid.dLocalGridOld[grid.nDCosThetaIJK][0][j][0];
          dV_np1=d1Thrid*dDelRCu_i_np1*dDelCosTheta;
            
          //CALCULATE RATE OF CHANGE IN RHO IN RADIAL DIRECTION
          
          //calculate area at i+1/2
          dA_ip1half=dRSq_ip1half_np1half*dDelCosTheta;
          
          //calculate rho at i+1/2, not time centered
          dRho_ip1half=(grid.dLocalGridOld[grid.nD][i][j][k]+grid.dLocalGridOld[grid.nD][i+1][j][k])
            *0.5;
          
          //calculate radial term
          dDeltaRhoR=-(grid.dLocalGridNew[grid.nU][nIInt][j][k]
            -grid.dLocalGridNew[grid.nU0][nIInt][0][0])*dRho_ip1half*dA_ip1half;
          
          
          //CALCULATE RATE OF CHANGE IN RHO IN THE THETA DIRECTION
          
          //calculate ratio of area at j-1/2,n to volume at i,j,k, n+1
          dA_jm1half=0.5*dDelRSq_i_np1half
            *grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt-1][0];
          
          //calculate ratio of area at j+1/2,n to volume at i,j,k, n+1
          dA_jp1half=0.5*dDelRSq_i_np1half
            *grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt][0];
          
          //calculte rho at j-1/2
          dRho_jm1half=(grid.dLocalGridOld[grid.nD][i][j-1][k]+grid.dLocalGridOld[grid.nD][i][j][k])
            *0.5;
          
          //calculte rho at j+1/2
          dRho_jp1half=(grid.dLocalGridOld[grid.nD][i][j+1][k]+grid.dLocalGridOld[grid.nD][i][j][k])
            *0.5;
          
          //calculate theta term
          dDeltaRhoTheta=grid.dLocalGridNew[grid.nV][i][nJInt-1][k]*dRho_jm1half*dA_jm1half
            -grid.dLocalGridNew[grid.nV][i][nJInt][k]*dRho_jp1half*dA_jp1half;
          
          //calculate new density
          grid.dLocalGridNew[grid.nD][i][j][k]=dVRatio*grid.dLocalGridOld[grid.nD][i][j][k]
            +time.dDeltat_np1half*(dDeltaRhoR+dDeltaRhoTheta)/dV_np1;
          
          if(grid.dLocalGridNew[grid.nD][i][j][k]<0.0){
            
            #if SIGNEGDEN==1
            raise(SIGINT);
            #endif
            
            std::stringstream ssTemp;
            ssTemp<<__FILE__<<":"<<__FUNCTION__<<":"<<__LINE__<<":"<<procTop.nRank
              <<": negative density calculated in , ("<<i<<","<<j<<","<<k<<")\n";
            throw exception2(ssTemp.str(),CALCULATION);
          }
        }
      }
    }
  #endif
}
void calNewD_RTP(Grid &grid, Parameters &parameters, Time &time,ProcTop &procTop){
  int i;
  int j;
  int k;
  int nIInt;
  int nJInt;
  int nKInt;
  double dDelRCu_i_n;
  double dDelRCu_i_np1;
  double dVRatio;
  double dR_ip1half_np1half;
  double dR_im1half_np1half;
  double dRSq_ip1half_np1half;
  double dRSq_im1half_np1half;
  double dDelRSq_i_np1half;
  double dDelCosThetaDelPhi;
  double dV_np1;
  double dA_im1half;
  double dA_ip1half;
  double dRho_im1half;
  double dRho_ip1half;
  double dDeltaRhoR;
  double dA_jm1half;
  double dA_jp1half;
  double dRho_jm1half;
  double dRho_jp1half;
  double dDeltaRhoTheta;
  double dA_km1half;
  double dA_kp1half;
  double dRho_km1half;
  double dRho_kp1half;
  double dDeltaRhoPhi;
  double dVr_np1;
  double d1Thrid=0.333333333333333333333333333333;
  for(i=grid.nStartUpdateExplicit[grid.nD][0];i<grid.nEndUpdateExplicit[grid.nD][0];i++){
    
    //calculate i for interface centered quantities
    nIInt=i+grid.nCenIntOffset[0];
    dDelRCu_i_n=(pow(grid.dLocalGridOld[grid.nR][nIInt][0][0],3.0)
          -pow(grid.dLocalGridOld[grid.nR][nIInt-1][0][0],3.0));
    dDelRCu_i_np1=(pow(grid.dLocalGridNew[grid.nR][nIInt][0][0],3.0)
          -pow(grid.dLocalGridNew[grid.nR][nIInt-1][0][0],3.0));
    dR_ip1half_np1half=grid.dLocalGridOld[grid.nR][nIInt][0][0];//could be time centered
    dR_im1half_np1half=grid.dLocalGridOld[grid.nR][nIInt-1][0][0];//could be time centered
    dRSq_ip1half_np1half=dR_ip1half_np1half*dR_ip1half_np1half;
    dRSq_im1half_np1half=dR_im1half_np1half*dR_im1half_np1half;
    dDelRSq_i_np1half=dRSq_ip1half_np1half-dRSq_im1half_np1half;
    dVRatio=dDelRCu_i_n/dDelRCu_i_np1;//calculate ratio of volume at n to volume at n+1
    
    for(j=grid.nStartUpdateExplicit[grid.nD][1];j<grid.nEndUpdateExplicit[grid.nD][1];j++){
      
      //calculate j for interface centered quantities
      nJInt=j+grid.nCenIntOffset[1];
      
      for(k=grid.nStartUpdateExplicit[grid.nD][2];k<grid.nEndUpdateExplicit[grid.nD][2];k++){
        
        nKInt=k+grid.nCenIntOffset[2];
        dDelCosThetaDelPhi=grid.dLocalGridOld[grid.nDCosThetaIJK][0][j][0]
          *grid.dLocalGridOld[grid.nDPhi][0][0][k];
        dV_np1=d1Thrid*dDelRCu_i_np1*dDelCosThetaDelPhi;
          
        //CALCULATE RATE OF CHANGE IN RHO IN RADIAL DIRECTION
        
        //calculate area at i-1/2
        dA_im1half=dRSq_im1half_np1half*dDelCosThetaDelPhi;
        
        //calculate area at i+1/2
        dA_ip1half=dRSq_ip1half_np1half*dDelCosThetaDelPhi;
        
        //calculate rho at i-1/2, not time centered
        dRho_im1half=(grid.dLocalGridOld[grid.nD][i][j][k]+grid.dLocalGridOld[grid.nD][i-1][j][k])
          *0.5;
        
        //calculate rho at i+1/2, not time centered
        dRho_ip1half=(grid.dLocalGridOld[grid.nD][i][j][k]+grid.dLocalGridOld[grid.nD][i+1][j][k])
          *0.5;
        
        //calculate radial term
        dDeltaRhoR=(grid.dLocalGridNew[grid.nU][nIInt-1][j][k]
          -grid.dLocalGridNew[grid.nU0][nIInt-1][0][0])*dRho_im1half*dA_im1half
          -(grid.dLocalGridNew[grid.nU][nIInt][j][k]-grid.dLocalGridNew[grid.nU0][nIInt][0][0])
          *dRho_ip1half*dA_ip1half;
        
        
        //CALCULATE RATE OF CHANGE IN RHO IN THE THETA DIRECTION
        
        //calculate ratio of area at j-1/2,n to volume at i,j,k, n+1
        dA_jm1half=0.5*dDelRSq_i_np1half
          *grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt-1][0]
          *grid.dLocalGridOld[grid.nDPhi][0][0][k];
        
        //calculate ratio of area at j+1/2,n to volume at i,j,k, n+1
        dA_jp1half=0.5*dDelRSq_i_np1half
          *grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt][0]
          *grid.dLocalGridOld[grid.nDPhi][0][0][k];
        
        //calculte rho at j-1/2
        dRho_jm1half=(grid.dLocalGridOld[grid.nD][i][j-1][k]+grid.dLocalGridOld[grid.nD][i][j][k])
          *0.5;
        
        //calculte rho at j+1/2
        dRho_jp1half=(grid.dLocalGridOld[grid.nD][i][j+1][k]+grid.dLocalGridOld[grid.nD][i][j][k])
          *0.5;
        
        //calculate theta term
        dDeltaRhoTheta=grid.dLocalGridNew[grid.nV][i][nJInt-1][k]*dRho_jm1half*dA_jm1half
          -grid.dLocalGridNew[grid.nV][i][nJInt][k]*dRho_jp1half*dA_jp1half;
        
        
        //CALCULATE RATE OF CHANGE IN RHO IN THE PHI DIRECTION
        
        //calculate ratio of area at k-1/2,n to volume at i,j,k, n+1
        dA_km1half=0.5*dDelRSq_i_np1half*grid.dLocalGridOld[grid.nDTheta][0][j][0];
        
        //calculate ratio of area at j+1/2,n to volume at i,j,k, n+1
        dA_kp1half=dA_km1half;
        
        //calculte rho at j-1/2
        dRho_km1half=(grid.dLocalGridOld[grid.nD][i][j][k-1]+grid.dLocalGridOld[grid.nD][i][j][k])
          *0.5;
        
        //calculte rho at j+1/2
        dRho_kp1half=(grid.dLocalGridOld[grid.nD][i][j][k+1]+grid.dLocalGridOld[grid.nD][i][j][k])
          *0.5;
        
        //calculate theta term
        dDeltaRhoPhi=grid.dLocalGridNew[grid.nW][i][j][nKInt-1]*dRho_km1half*dA_km1half
          -grid.dLocalGridNew[grid.nW][i][j][nKInt]*dRho_kp1half*dA_kp1half;
        
        //calculate new density
        grid.dLocalGridNew[grid.nD][i][j][k]=dVRatio*grid.dLocalGridOld[grid.nD][i][j][k]
          +time.dDeltat_np1half*(dDeltaRhoR+dDeltaRhoTheta+dDeltaRhoPhi)/dV_np1;
        
        if(grid.dLocalGridNew[grid.nD][i][j][k]<0.0){
          
          #if SIGNEGDEN==1
          raise(SIGINT);
          #endif
          
          std::stringstream ssTemp;
          ssTemp<<__FILE__<<":"<<__FUNCTION__<<":"<<__LINE__<<":"<<procTop.nRank
            <<": negative density calculated in , ("<<i<<","<<j<<","<<k<<")\n";
          throw exception2(ssTemp.str(),CALCULATION);
        }
      }
    }
  }
  
  //ghost region 0, outter most ghost region in x1 direction
  for(i=grid.nStartGhostUpdateExplicit[grid.nD][0][0]
    ;i<grid.nEndGhostUpdateExplicit[grid.nD][0][0];i++){
    
    //calculate i for interface centered quantities
    nIInt=i+grid.nCenIntOffset[0];
    dDelRCu_i_n=(pow(grid.dLocalGridOld[grid.nR][nIInt][0][0],3.0)
          -pow(grid.dLocalGridOld[grid.nR][nIInt-1][0][0],3.0));
    dDelRCu_i_np1=(pow(grid.dLocalGridNew[grid.nR][nIInt][0][0],3.0)
          -pow(grid.dLocalGridNew[grid.nR][nIInt-1][0][0],3.0));
    dR_ip1half_np1half=grid.dLocalGridOld[grid.nR][nIInt][0][0];//could be time centered
    dR_im1half_np1half=grid.dLocalGridOld[grid.nR][nIInt-1][0][0];//could be time centered
    dRSq_ip1half_np1half=dR_ip1half_np1half*dR_ip1half_np1half;
    dRSq_im1half_np1half=dR_im1half_np1half*dR_im1half_np1half;
    dDelRSq_i_np1half=dRSq_ip1half_np1half-dRSq_im1half_np1half;
    dVRatio=dDelRCu_i_n/dDelRCu_i_np1;//calculate ratio of volume at n to volume at n+1
    
    for(j=grid.nStartUpdateExplicit[grid.nD][1];j<grid.nEndUpdateExplicit[grid.nD][1];j++){
      
      //calculate j for interface centered quantities
      nJInt=j+grid.nCenIntOffset[1];
      
      for(k=grid.nStartUpdateExplicit[grid.nD][2];k<grid.nEndUpdateExplicit[grid.nD][2];k++){
        nKInt=k+grid.nCenIntOffset[2];
        dDelCosThetaDelPhi=grid.dLocalGridOld[grid.nDCosThetaIJK][0][j][0]
          *grid.dLocalGridOld[grid.nDPhi][0][0][k];
        dV_np1=d1Thrid*dDelRCu_i_np1*dDelCosThetaDelPhi;
          
        //CALCULATE RATE OF CHANGE IN RHO IN RADIAL DIRECTION
        
        //calculate area at i-1/2
        dA_im1half=dRSq_im1half_np1half*dDelCosThetaDelPhi;
        
        //calculate rho at i-1/2, not time centered
        dRho_im1half=(grid.dLocalGridOld[grid.nD][i][j][k]+grid.dLocalGridOld[grid.nD][i-1][j][k])
          *0.5;
        
        //calculate radial term
        dDeltaRhoR=(grid.dLocalGridNew[grid.nU][nIInt-1][j][k]
          -grid.dLocalGridNew[grid.nU0][nIInt-1][0][0])*dRho_im1half*dA_im1half;
        
        
        //CALCULATE RATE OF CHANGE IN RHO IN THE THETA DIRECTION
        
        //calculate ratio of area at j-1/2,n to volume at i,j,k, n+1
        dA_jm1half=0.5*dDelRSq_i_np1half
          *grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt-1][0]
          *grid.dLocalGridOld[grid.nDPhi][0][0][k];
        
        //calculate ratio of area at j+1/2,n to volume at i,j,k, n+1
        dA_jp1half=0.5*dDelRSq_i_np1half
          *grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt][0]
          *grid.dLocalGridOld[grid.nDPhi][0][0][k];
        
        //calculte rho at j-1/2
        dRho_jm1half=(grid.dLocalGridOld[grid.nD][i][j-1][k]+grid.dLocalGridOld[grid.nD][i][j][k])
          *0.5;
        
        //calculte rho at j+1/2
        dRho_jp1half=(grid.dLocalGridOld[grid.nD][i][j+1][k]+grid.dLocalGridOld[grid.nD][i][j][k])
          *0.5;
        
        //calculate theta term
        dDeltaRhoTheta=grid.dLocalGridNew[grid.nV][i][nJInt-1][k]*dRho_jm1half*dA_jm1half
          -grid.dLocalGridNew[grid.nV][i][nJInt][k]*dRho_jp1half*dA_jp1half;
        
        
        //CALCULATE RATE OF CHANGE IN RHO IN THE PHI DIRECTION
        
        //calculate ratio of area at k-1/2,n to volume at i,j,k, n+1
        dA_km1half=0.5*dDelRSq_i_np1half*grid.dLocalGridOld[grid.nDTheta][0][j][0];
        
        //calculate ratio of area at j+1/2,n to volume at i,j,k, n+1
        dA_kp1half=dA_km1half;
        
        //calculte rho at j-1/2
        dRho_km1half=(grid.dLocalGridOld[grid.nD][i][j][k-1]+grid.dLocalGridOld[grid.nD][i][j][k])
          *0.5;
        
        //calculte rho at j+1/2
        dRho_kp1half=(grid.dLocalGridOld[grid.nD][i][j][k+1]+grid.dLocalGridOld[grid.nD][i][j][k])
          *0.5;
        
        //calculate theta term
        dDeltaRhoPhi=grid.dLocalGridNew[grid.nW][i][j][nKInt-1]*dRho_km1half*dA_km1half
          -grid.dLocalGridNew[grid.nW][i][j][nKInt]*dRho_kp1half*dA_kp1half;
        
        //calculate new density
        /**\BC doesn't allow mass flux through outter interface*/
        grid.dLocalGridNew[grid.nD][i][j][k]=dVRatio*grid.dLocalGridOld[grid.nD][i][j][k]
          +time.dDeltat_np1half*(dDeltaRhoR+dDeltaRhoTheta+dDeltaRhoPhi)/dV_np1;
        
        if(grid.dLocalGridNew[grid.nD][i][j][k]<0.0){
          
          #if SIGNEGDEN==1
          raise(SIGINT);
          #endif
          
          std::stringstream ssTemp;
          ssTemp<<__FILE__<<":"<<__FUNCTION__<<":"<<__LINE__<<":"<<procTop.nRank
            <<": negative density calculated in , ("<<i<<","<<j<<","<<k<<")\n";
          throw exception2(ssTemp.str(),CALCULATION);
        }
      }
    }
  }

  #if SEDOV==1
    
    //ghost region 1, inner most ghost region in x1 direction
    for(i=grid.nStartGhostUpdateExplicit[grid.nD][1][0];
      i<grid.nEndGhostUpdateExplicit[grid.nD][1][0];i++){
      
      //calculate i for interface centered quantities
      nIInt=i+grid.nCenIntOffset[0];
      dDelRCu_i_n=(pow(grid.dLocalGridOld[grid.nR][nIInt][0][0],3.0)
            -pow(grid.dLocalGridOld[grid.nR][nIInt-1][0][0],3.0));
      dDelRCu_i_np1=(pow(grid.dLocalGridNew[grid.nR][nIInt][0][0],3.0)
            -pow(grid.dLocalGridNew[grid.nR][nIInt-1][0][0],3.0));
      dR_ip1half_np1half=grid.dLocalGridOld[grid.nR][nIInt][0][0];//could be time centered
      dR_im1half_np1half=grid.dLocalGridOld[grid.nR][nIInt-1][0][0];//could be time centered
      dRSq_ip1half_np1half=dR_ip1half_np1half*dR_ip1half_np1half;
      dRSq_im1half_np1half=dR_im1half_np1half*dR_im1half_np1half;
      dDelRSq_i_np1half=dRSq_ip1half_np1half-dRSq_im1half_np1half;
      dVRatio=dDelRCu_i_n/dDelRCu_i_np1;//calculate ratio of volume at n to volume at n+1
      
      for(j=grid.nStartGhostUpdateExplicit[grid.nD][1][1];
        j<grid.nEndGhostUpdateExplicit[grid.nD][1][1];j++){
        
        //calculate j for interface centered quantities
        nJInt=j+grid.nCenIntOffset[1];
        
        for(k=grid.nStartGhostUpdateExplicit[grid.nD][1][2];
          k<grid.nEndGhostUpdateExplicit[grid.nD][1][2];k++){
          
          nKInt=k+grid.nCenIntOffset[2];
          dDelCosThetaDelPhi=grid.dLocalGridOld[grid.nDCosThetaIJK][0][j][0]
            *grid.dLocalGridOld[grid.nDPhi][0][0][k];
          dV_np1=d1Thrid*dDelRCu_i_np1*dDelCosThetaDelPhi;
            
          //CALCULATE RATE OF CHANGE IN RHO IN RADIAL DIRECTION
          
          //calculate area at i+1/2
          dA_ip1half=dRSq_ip1half_np1half*dDelCosThetaDelPhi;
          
          //calculate rho at i+1/2, not time centered
          dRho_ip1half=(grid.dLocalGridOld[grid.nD][i][j][k]+grid.dLocalGridOld[grid.nD][i+1][j][k])
            *0.5;
          
          //calculate radial term
          dDeltaRhoR=-(grid.dLocalGridNew[grid.nU][nIInt][j][k]
            -grid.dLocalGridNew[grid.nU0][nIInt][0][0])*dRho_ip1half*dA_ip1half;
          
          
          //CALCULATE RATE OF CHANGE IN RHO IN THE THETA DIRECTION
          
          //calculate ratio of area at j-1/2,n to volume at i,j,k, n+1
          dA_jm1half=0.5*dDelRSq_i_np1half
            *grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt-1][0]
            *grid.dLocalGridOld[grid.nDPhi][0][0][k];
          
          //calculate ratio of area at j+1/2,n to volume at i,j,k, n+1
          dA_jp1half=0.5*dDelRSq_i_np1half
            *grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt][0]
            *grid.dLocalGridOld[grid.nDPhi][0][0][k];
          
          //calculte rho at j-1/2
          dRho_jm1half=(grid.dLocalGridOld[grid.nD][i][j-1][k]+grid.dLocalGridOld[grid.nD][i][j][k])
            *0.5;
          
          //calculte rho at j+1/2
          dRho_jp1half=(grid.dLocalGridOld[grid.nD][i][j+1][k]+grid.dLocalGridOld[grid.nD][i][j][k])
            *0.5;
          
          //calculate theta term
          dDeltaRhoTheta=grid.dLocalGridNew[grid.nV][i][nJInt-1][k]*dRho_jm1half*dA_jm1half
            -grid.dLocalGridNew[grid.nV][i][nJInt][k]*dRho_jp1half*dA_jp1half;
          
          
          //CALCULATE RATE OF CHANGE IN RHO IN THE PHI DIRECTION
          
          //calculate ratio of area at k-1/2,n to volume at i,j,k, n+1
          dA_km1half=0.5*dDelRSq_i_np1half*grid.dLocalGridOld[grid.nDTheta][0][j][0];
          
          //calculate ratio of area at j+1/2,n to volume at i,j,k, n+1
          dA_kp1half=dA_km1half;
          
          //calculte rho at j-1/2
          dRho_km1half=(grid.dLocalGridOld[grid.nD][i][j][k-1]+grid.dLocalGridOld[grid.nD][i][j][k])
            *0.5;
          
          //calculte rho at j+1/2
          dRho_kp1half=(grid.dLocalGridOld[grid.nD][i][j][k+1]+grid.dLocalGridOld[grid.nD][i][j][k])
            *0.5;
          
          //calculate theta term
          dDeltaRhoPhi=grid.dLocalGridNew[grid.nW][i][j][nKInt-1]*dRho_km1half*dA_km1half
            -grid.dLocalGridNew[grid.nW][i][j][nKInt]*dRho_kp1half*dA_kp1half;
          
          //calculate new density
          grid.dLocalGridNew[grid.nD][i][j][k]=dVRatio*grid.dLocalGridOld[grid.nD][i][j][k]
            +time.dDeltat_np1half*(dDeltaRhoR+dDeltaRhoTheta+dDeltaRhoPhi)/dV_np1;
          
          if(grid.dLocalGridNew[grid.nD][i][j][k]<0.0){
            
            #if SIGNEGDEN==1
            raise(SIGINT);
            #endif
            
            std::stringstream ssTemp;
            ssTemp<<__FILE__<<":"<<__FUNCTION__<<":"<<__LINE__<<":"<<procTop.nRank
              <<": negative density calculated in , ("<<i<<","<<j<<","<<k<<")\n";
            throw exception2(ssTemp.str(),CALCULATION);
          }
        }
      }
    }
  #endif
}
void calNewE_R_AD(Grid &grid, Parameters &parameters, Time &time, ProcTop &procTop){
  for(int i=grid.nStartUpdateExplicit[grid.nE][0];i<grid.nEndUpdateExplicit[grid.nE][0];i++){
    
    //calculate i for interface centered quantities
    int nIInt=i+grid.nCenIntOffset[0];
    
    for(int j=grid.nStartUpdateExplicit[grid.nE][1];j<grid.nEndUpdateExplicit[grid.nE][1];j++){
      for(int k=grid.nStartUpdateExplicit[grid.nE][2];k<grid.nEndUpdateExplicit[grid.nE][2];k++){
        
        //calculate interpolated quantities
        double dUCen=(grid.dLocalGridNew[grid.nU][nIInt][j][k]+grid.dLocalGridNew[grid.nU][nIInt-1][j][k])*0.5;
        double dU0Cen=(grid.dLocalGridNew[grid.nU0][nIInt][0][0]+grid.dLocalGridNew[grid.nU0][nIInt-1][0][0])
          *0.5;
        double dE_ip1half=(grid.dLocalGridOld[grid.nE][i+1][j][k]+grid.dLocalGridOld[grid.nE][i][j][k])*0.5;
        double dE_im1half=(grid.dLocalGridOld[grid.nE][i][j][k]+grid.dLocalGridOld[grid.nE][i-1][j][k])*0.5;
        double dRCen=(grid.dLocalGridOld[grid.nR][nIInt][0][0]+grid.dLocalGridOld[grid.nR][nIInt-1][0][0])*0.5;
        double dR_im1half_np1half=grid.dLocalGridOld[grid.nR][nIInt-1][0][0];
        double dR_ip1half_np1half=grid.dLocalGridOld[grid.nR][nIInt][0][0];
        
        //advection term in x-direction
        double dRCenSq=dRCen*dRCen;
        double dA1=(dUCen-dU0Cen)*dRCenSq*(dE_ip1half-dE_im1half)
          /grid.dLocalGridOld[grid.nDM][i][0][0];
        
        //source term in x-direction
        double dUR2_im1half=grid.dLocalGridNew[grid.nU][nIInt-1][j][k]*dR_im1half_np1half
          *dR_im1half_np1half;
        double dUR2_ip1half=grid.dLocalGridNew[grid.nU][nIInt][j][k]*dR_ip1half_np1half
          *dR_ip1half_np1half;
        double dP=grid.dLocalGridOld[grid.nP][i][j][k];
        #if VISCOUS_ENERGY_EQ==1
          dP+=grid.dLocalGridOld[grid.nQ0][i][j][k];
        #endif
        
        double dS1=dP/grid.dLocalGridOld[grid.nD][i][j][k]*(dUR2_ip1half-dUR2_im1half)
          /grid.dLocalGridOld[grid.nDM][i][0][0];
        
        //calculate new energy
        grid.dLocalGridNew[grid.nE][i][j][k]=grid.dLocalGridOld[grid.nE][i][j][k]
        -time.dDeltat_n*4.0*parameters.dPi*grid.dLocalGridOld[grid.nD][i][j][k]*(dA1+dS1);
        
        if(grid.dLocalGridNew[grid.nE][i][j][k]<0.0){
          
          #if SIGNEGENG==1
          raise(SIGINT);
          #endif
          
          std::stringstream ssTemp;
          ssTemp<<__FILE__<<":"<<__FUNCTION__<<":"<<__LINE__<<":"<<procTop.nRank
            <<": negative energy calculated in , ("<<i<<","<<j<<","<<k<<")\n";
          throw exception2(ssTemp.str(),CALCULATION);
          
        }
      }
    }
  }
  
  //ghost region 0, outter most ghost region in x1 direction
  for(int i=grid.nStartGhostUpdateExplicit[grid.nE][0][0];i<grid.nEndGhostUpdateExplicit[grid.nE][0][0];i++){
    
    //calculate i for interface centered quantities
    int nIInt=i+grid.nCenIntOffset[0];
    
    for(int j=grid.nStartGhostUpdateExplicit[grid.nE][0][1];j<grid.nEndGhostUpdateExplicit[grid.nE][0][1];j++){
      for(int k=grid.nStartGhostUpdateExplicit[grid.nE][0][2];k<grid.nEndGhostUpdateExplicit[grid.nE][0][2];k++){
        
        //calculate interpolated quantities
        double dUCen=(grid.dLocalGridNew[grid.nU][nIInt][j][k]+grid.dLocalGridNew[grid.nU][nIInt-1][j][k])*0.5;
        double dU0Cen=(grid.dLocalGridNew[grid.nU0][nIInt][0][0]+grid.dLocalGridNew[grid.nU0][nIInt-1][0][0])
          *0.5;
        double dE_ip1half=grid.dLocalGridOld[grid.nE][i][j][k]*0.5;
        double dE_im1half=(grid.dLocalGridOld[grid.nE][i][j][k]+grid.dLocalGridOld[grid.nE][i-1][j][k])*0.5;
        double dRCen=(grid.dLocalGridOld[grid.nR][nIInt][0][0]+grid.dLocalGridOld[grid.nR][nIInt-1][0][0])*0.5;
        double dR_im1half_np1half=grid.dLocalGridOld[grid.nR][nIInt-1][0][0];
        double dR_ip1half_np1half=grid.dLocalGridOld[grid.nR][nIInt][0][0];
        
        //advection term in x-direction
        double dRCenSq=dRCen*dRCen;
        double dA1=(dUCen-dU0Cen)*dRCenSq*(dE_ip1half-dE_im1half)
          /grid.dLocalGridOld[grid.nDM][i][0][0];
        
        //source term in x-direction
        double dUR2_im1half=grid.dLocalGridNew[grid.nU][nIInt-1][j][k]*dR_im1half_np1half
          *dR_im1half_np1half;
        double dUR2_ip1half=grid.dLocalGridNew[grid.nU][nIInt][j][k]*dR_ip1half_np1half
          *dR_ip1half_np1half;
          
        double dP=grid.dLocalGridOld[grid.nP][i][j][k];
        #if VISCOUS_ENERGY_EQ==1
          dP+=grid.dLocalGridOld[grid.nQ0][i][j][k];
        #endif
        
        double dS1=dP/grid.dLocalGridOld[grid.nD][i][j][k]*(dUR2_ip1half-dUR2_im1half)
          /grid.dLocalGridOld[grid.nDM][i][0][0];
        
        //calculate new energy
        grid.dLocalGridNew[grid.nE][i][j][k]=grid.dLocalGridOld[grid.nE][i][j][k]
        -time.dDeltat_n*4.0*parameters.dPi*grid.dLocalGridOld[grid.nD][i][j][k]*(dA1+dS1);
        
        if(grid.dLocalGridNew[grid.nE][i][j][k]<0.0){
          
          #if SIGNEGENG==1
          raise(SIGINT);
          #endif
          
          std::stringstream ssTemp;
          ssTemp<<__FILE__<<":"<<__FUNCTION__<<":"<<__LINE__<<":"<<procTop.nRank
            <<": negative energy calculated in , ("<<i<<","<<j<<","<<k<<")\n";
          throw exception2(ssTemp.str(),CALCULATION);
          
        }
      }
    }
  }

  #if SEDOV==1 //use zero P, E, and rho gradients
    for(int i=grid.nStartGhostUpdateExplicit[grid.nE][1][0];i<grid.nEndGhostUpdateExplicit[grid.nE][1][0];i++){
      
      for(int j=grid.nStartGhostUpdateExplicit[grid.nE][1][1];j<grid.nEndGhostUpdateExplicit[grid.nE][1][1];j++){
        for(int k=grid.nStartGhostUpdateExplicit[grid.nE][1][2];k<grid.nEndGhostUpdateExplicit[grid.nE][1][2];k++){
          grid.dLocalGridNew[grid.nE][i][j][k]=grid.dLocalGridNew[grid.nE][i+1][j][k];
        }
      }
    }
  #endif
}
void calNewE_R_NA(Grid &grid, Parameters &parameters, Time &time, ProcTop &procTop){
  for(int i=grid.nStartUpdateExplicit[grid.nE][0];i<grid.nEndUpdateExplicit[grid.nE][0];i++){
    
    //calculate i for interface centered quantities
    int nIInt=i+grid.nCenIntOffset[0];
    
    for(int j=grid.nStartUpdateExplicit[grid.nE][1];j<grid.nEndUpdateExplicit[grid.nE][1];j++){
      
      for(int k=grid.nStartUpdateExplicit[grid.nE][2];k<grid.nEndUpdateExplicit[grid.nE][2];k++){
        
        //Calculate interpolated quantities
        double dU_ijk_np1half=(grid.dLocalGridNew[grid.nU][nIInt][j][k]
          +grid.dLocalGridNew[grid.nU][nIInt-1][j][k])*0.5;
        double dU0_i_np1half=(grid.dLocalGridNew[grid.nU0][nIInt][0][0]
          +grid.dLocalGridNew[grid.nU0][nIInt-1][0][0])*0.5;
        double dE_ip1halfjk_n=(grid.dLocalGridOld[grid.nE][i+1][j][k]
          +grid.dLocalGridOld[grid.nE][i][j][k])*0.5;
        double dE_im1halfjk_n=(grid.dLocalGridOld[grid.nE][i][j][k]
          +grid.dLocalGridOld[grid.nE][i-1][j][k])*0.5;
        double dR_i_n=(grid.dLocalGridOld[grid.nR][nIInt][0][0]
          +grid.dLocalGridOld[grid.nR][nIInt-1][0][0])*0.5;
        double dR_im1half_n=grid.dLocalGridOld[grid.nR][nIInt-1][0][0];
        double dR_ip1half_n=grid.dLocalGridOld[grid.nR][nIInt][0][0];
        double dRSq_i_n=dR_i_n*dR_i_n;
        double dRSq_ip1half=dR_ip1half_n*dR_ip1half_n;
        double dR4_ip1half=dRSq_ip1half*dRSq_ip1half;
        double dR_im1half_sq=dR_im1half_n*dR_im1half_n;
        double dR_im1half_4=dR_im1half_sq*dR_im1half_sq;
        double dRhoAve_ip1half=(grid.dLocalGridOld[grid.nD][i+1][0][0]
          +grid.dLocalGridOld[grid.nD][i][0][0])*0.5;
        double dRhoAve_im1half=(grid.dLocalGridOld[grid.nD][i][0][0]
          +grid.dLocalGridOld[grid.nD][i-1][0][0])*0.5;
        double dRho_ip1halfjk=(grid.dLocalGridOld[grid.nD][i+1][j][k]
          +grid.dLocalGridOld[grid.nD][i][j][k])*0.5;
        double dRho_im1halfjk=(grid.dLocalGridOld[grid.nD][i][j][k]
          +grid.dLocalGridOld[grid.nD][i-1][j][k])*0.5;
        double dTSq_ip1jk_n=grid.dLocalGridOld[grid.nT][i+1][j][k]
          *grid.dLocalGridOld[grid.nT][i+1][j][k];
        double dT4_ip1jk_n=dTSq_ip1jk_n*dTSq_ip1jk_n;
        double dTSq_ijk_n=grid.dLocalGridOld[grid.nT][i][j][k]
          *grid.dLocalGridOld[grid.nT][i][j][k];
        double dT4_ijk_n=dTSq_ijk_n*dTSq_ijk_n;
        double dTSq_im1jk_n=grid.dLocalGridOld[grid.nT][i-1][j][k]
          *grid.dLocalGridOld[grid.nT][i-1][j][k];
        double dT4_im1jk_n=dTSq_im1jk_n*dTSq_im1jk_n;
        double dKappa_ip1halfjk_n=(dT4_ip1jk_n+dT4_ijk_n)
          /(dT4_ijk_n/grid.dLocalGridOld[grid.nKappa][i][j][k]
          +dT4_ip1jk_n/grid.dLocalGridOld[grid.nKappa][i+1][j][k]);
        double dKappa_im1halfjk_n=(dT4_im1jk_n+dT4_ijk_n)
          /(dT4_ijk_n/grid.dLocalGridOld[grid.nKappa][i][j][k]
          +dT4_im1jk_n/grid.dLocalGridOld[grid.nKappa][i-1][j][k]);
          
        //Calcuate dA1
        double dA1CenGrad=(dE_ip1halfjk_n-dE_im1halfjk_n)/grid.dLocalGridOld[grid.nDM][i][0][0];
        double dA1UpWindGrad=0.0;
        double dU_U0_Diff=(dU_ijk_np1half-dU0_i_np1half);
        if(dU_U0_Diff<0.0){//moving in the negative direction
          dA1UpWindGrad=(grid.dLocalGridOld[grid.nE][i+1][j][k]
            -grid.dLocalGridOld[grid.nE][i][j][k])/(grid.dLocalGridOld[grid.nDM][i+1][0][0]
            +grid.dLocalGridOld[grid.nDM][i][0][0])*2.0;
        }
        else{//moving in the postive direction
          dA1UpWindGrad=(grid.dLocalGridOld[grid.nE][i][j][k]
            -grid.dLocalGridOld[grid.nE][i-1][j][k])/(grid.dLocalGridOld[grid.nDM][i][0][0]
            +grid.dLocalGridOld[grid.nDM][i-1][0][0])*2.0;
        }
        double dA1=dU_U0_Diff*dRSq_i_n*((1.0-parameters.dDonorFrac)*dA1CenGrad
          +parameters.dDonorFrac*dA1UpWindGrad);
        
        //calculate dS1
        double dUR2_im1half_np1half=grid.dLocalGridNew[grid.nU][nIInt-1][j][k]*dR_im1half_n
          *dR_im1half_n;
        double dUR2_ip1half_np1half=grid.dLocalGridNew[grid.nU][nIInt][j][k]*dR_ip1half_n
          *dR_ip1half_n;
        double dPi_ijk_n=grid.dLocalGridOld[grid.nP][i][j][k];
        #if VISCOUS_ENERGY_EQ==1
          dPi_ijk_n+=grid.dLocalGridOld[grid.nQ0][i][j][k];
        #endif
        double dS1=dPi_ijk_n/grid.dLocalGridOld[grid.nD][i][j][k]
          *(dUR2_ip1half_np1half-dUR2_im1half_np1half)/grid.dLocalGridOld[grid.nDM][i][0][0];
        
        //Calculate dS4
        double dTGrad_ip1half=(dT4_ip1jk_n-dT4_ijk_n)/(grid.dLocalGridOld[grid.nDM][i+1][0][0]
          +grid.dLocalGridOld[grid.nDM][i][0][0])*2.0;
        double dTGrad_im1half=(dT4_ijk_n-dT4_im1jk_n)/(grid.dLocalGridOld[grid.nDM][i][0][0]
          +grid.dLocalGridOld[grid.nDM][i-1][0][0])*2.0;
        double dGrad_ip1half=dRhoAve_ip1half*dR4_ip1half/(dKappa_ip1halfjk_n*dRho_ip1halfjk)
          *dTGrad_ip1half;
        double dGrad_im1half=dRhoAve_im1half*dR_im1half_4/(dKappa_im1halfjk_n*dRho_im1halfjk)
          *dTGrad_im1half;
        double dS4=16.0*parameters.dPi*parameters.dPi*grid.dLocalGridOld[grid.nD][i][0][0]
          *(dGrad_ip1half-dGrad_im1half)/grid.dLocalGridOld[grid.nDM][i][0][0];
        
        //calculate new energy
        grid.dLocalGridNew[grid.nE][i][j][k]=grid.dLocalGridOld[grid.nE][i][j][k]
          -time.dDeltat_n*(4.0*parameters.dPi*grid.dLocalGridOld[grid.nD][i][0][0]*(dA1+dS1)
          -4.0*parameters.dSigma/(3.0*grid.dLocalGridOld[grid.nD][i][j][k])*(dS4));
        
        if(grid.dLocalGridNew[grid.nE][i][j][k]<0.0){
          
          #if SIGNEGENG==1
          raise(SIGINT);
          #endif
          
          std::stringstream ssTemp;
          ssTemp<<__FILE__<<":"<<__FUNCTION__<<":"<<__LINE__<<":"<<procTop.nRank
            <<": negative energy calculated in , ("<<i<<","<<j<<","<<k<<")\n";
          throw exception2(ssTemp.str(),CALCULATION);
          
        }
      }
    }
  }
  
  //ghost region 0, outter most ghost region in x1 direction
  for(int i=grid.nStartGhostUpdateExplicit[grid.nE][0][0];i<grid.nEndGhostUpdateExplicit[grid.nE][0][0];i++){
    
    //calculate i for interface centered quantities
    int nIInt=i+grid.nCenIntOffset[0];
    
    for(int j=grid.nStartGhostUpdateExplicit[grid.nE][0][1];j<grid.nEndGhostUpdateExplicit[grid.nE][0][1];j++){
      
      for(int k=grid.nStartGhostUpdateExplicit[grid.nE][0][2];k<grid.nEndGhostUpdateExplicit[grid.nE][0][2];k++){
        
        //Calculate interpolated quantities
        double dU_ijk_np1half=(grid.dLocalGridNew[grid.nU][nIInt][j][k]
          +grid.dLocalGridNew[grid.nU][nIInt-1][j][k])*0.5;
        double dU0_i_np1half=(grid.dLocalGridNew[grid.nU0][nIInt][0][0]
          +grid.dLocalGridNew[grid.nU0][nIInt-1][0][0])*0.5;
        double dE_ip1halfjk_n=(grid.dLocalGridOld[grid.nE][i][j][k]);/**\BC Missing
          grid.dLocalGridOld[grid.nE][i+1][j][k] in calculation of \f$E_{i+1/2,j,k}\f$ 
          setting it equal to value at i.*/
        double dE_im1halfjk_n=(grid.dLocalGridOld[grid.nE][i][j][k]
          +grid.dLocalGridOld[grid.nE][i-1][j][k])*0.5;
        double dR_i_n=(grid.dLocalGridOld[grid.nR][nIInt][0][0]
          +grid.dLocalGridOld[grid.nR][nIInt-1][0][0])*0.5;
        double dR_im1half_n=grid.dLocalGridOld[grid.nR][nIInt-1][0][0];
        double dR_ip1half_n=grid.dLocalGridOld[grid.nR][nIInt][0][0];
        double dRSq_i_n=dR_i_n*dR_i_n;
        double dRSq_ip1half=dR_ip1half_n*dR_ip1half_n;
        double dR_im1half_sq=dR_im1half_n*dR_im1half_n;
        double dR_im1half_4=dR_im1half_sq*dR_im1half_sq;
        double dRhoAve_im1half=(grid.dLocalGridOld[grid.nD][i][0][0]
          +grid.dLocalGridOld[grid.nD][i-1][0][0])*0.5;
        double dRho_im1halfjk=(grid.dLocalGridOld[grid.nD][i][j][k]
          +grid.dLocalGridOld[grid.nD][i-1][j][k])*0.5;
        double dTSq_ijk_n=grid.dLocalGridOld[grid.nT][i][j][k]
          *grid.dLocalGridOld[grid.nT][i][j][k];
        double dT4_ijk_n=dTSq_ijk_n*dTSq_ijk_n;
        double dTSq_im1jk_n=grid.dLocalGridOld[grid.nT][i-1][j][k]
          *grid.dLocalGridOld[grid.nT][i-1][j][k];
        double dT4_im1jk_n=dTSq_im1jk_n*dTSq_im1jk_n;
        double dKappa_im1halfjk_n=(dT4_im1jk_n+dT4_ijk_n)
          /(dT4_ijk_n/grid.dLocalGridOld[grid.nKappa][i][j][k]
          +dT4_im1jk_n/grid.dLocalGridOld[grid.nKappa][i-1][j][k]);
        
        //Calcuate dA1
        double dA1CenGrad=(dE_ip1halfjk_n-dE_im1halfjk_n)/grid.dLocalGridOld[grid.nDM][i][0][0];
        double dA1UpWindGrad=0.0;
        double dU_U0_Diff=(dU_ijk_np1half-dU0_i_np1half);
        if(dU_U0_Diff<0.0){//moving in the negative direction
          dA1UpWindGrad=dA1CenGrad;/**\BC grid.dLocalGridOld[grid.nDM][i+1][0][0] and 
            grid.dLocalGridOld[grid.nE][i+1][j][k] missing in the calculation of upwind gradient in 
            dA1. Using the centered gradient instead.*/
        }
        else{//moving in the postive direction*/
          dA1UpWindGrad=(grid.dLocalGridOld[grid.nE][i][j][k]
            -grid.dLocalGridOld[grid.nE][i-1][j][k])/(grid.dLocalGridOld[grid.nDM][i][0][0]
            +grid.dLocalGridOld[grid.nDM][i-1][0][0])*2.0;
        }
        double dA1=dU_U0_Diff*dRSq_i_n*((1.0-parameters.dDonorFrac)*dA1CenGrad
          +parameters.dDonorFrac*dA1UpWindGrad);
        
        //calculate dS1
        double dUR2_im1half_np1half=grid.dLocalGridNew[grid.nU][nIInt-1][j][k]*dR_im1half_n
          *dR_im1half_n;
        double dUR2_ip1half_np1half=grid.dLocalGridNew[grid.nU][nIInt][j][k]*dR_ip1half_n
          *dR_ip1half_n;
        double dP_ijk_n=grid.dLocalGridOld[grid.nP][i][j][k];
        #if VISCOUS_ENERGY_EQ==1
          dP_ijk_n+=grid.dLocalGridOld[grid.nQ0][i][j][k];
        #endif
        double dS1=dP_ijk_n/grid.dLocalGridOld[grid.nD][i][j][k]
          *(dUR2_ip1half_np1half-dUR2_im1half_np1half)/grid.dLocalGridOld[grid.nDM][i][0][0];
        
        //Calculate dS4
        double dTGrad_im1half=(dT4_ijk_n-dT4_im1jk_n)/(grid.dLocalGridOld[grid.nDM][i][0][0]
          +grid.dLocalGridOld[grid.nDM][i-1][0][0])*2.0;
        double dGrad_ip1half=-3.0*dRSq_ip1half*dT4_ijk_n/(8.0*parameters.dPi);/**\BC
          Missing grid.dLocalGridOld[grid.nT][i+1][0][0]*/
        double dGrad_im1half=dRhoAve_im1half*dR_im1half_4/(dKappa_im1halfjk_n*dRho_im1halfjk)
          *dTGrad_im1half;
        double dS4=16.0*parameters.dPi*parameters.dPi*grid.dLocalGridOld[grid.nD][i][0][0]
          *(dGrad_ip1half-dGrad_im1half)/grid.dLocalGridOld[grid.nDM][i][0][0];
        
        //calculate new energy
        grid.dLocalGridNew[grid.nE][i][j][k]=grid.dLocalGridOld[grid.nE][i][j][k]
          -time.dDeltat_n*(4.0*parameters.dPi*grid.dLocalGridOld[grid.nD][i][0][0]*(dA1+dS1)
          -4.0*parameters.dSigma/(3.0*grid.dLocalGridOld[grid.nD][i][j][k])*(dS4));
        
        if(grid.dLocalGridNew[grid.nE][i][j][k]<0.0){
          
          #if SIGNEGENG==1
          raise(SIGINT);
          #endif
          
          std::stringstream ssTemp;
          ssTemp<<__FILE__<<":"<<__FUNCTION__<<":"<<__LINE__<<":"<<procTop.nRank
            <<": negative energy calculated in , ("<<i<<","<<j<<","<<k<<")\n";
          throw exception2(ssTemp.str(),CALCULATION);
          
        }
      }
    }
  }
}
void calNewE_R_NA_LES(Grid &grid, Parameters &parameters, Time &time, ProcTop &procTop){
  int nIInt;
  double dU_ijk_np1half;
  double dU_ip1jk_np1half;
  double dU_im1jk_np1half;
  double dU0_i_np1half;
  double dE_ip1halfjk_n;
  double dE_im1halfjk_n;
  double dR_ip1_np1half;
  double dRSq_ip1_np1half;
  double dR_i_np1half;
  double dR_im1half_np1half;
  double dR_ip1half_np1half;
  double dRSq_i_np1half;
  double dR_im1_np1half;
  double dRSq_im1_np1half;
  double dRSq_ip1half_np1half;
  double dR4_ip1half_np1half;
  double dRSq_im1half_np1half;
  double dR4_im1half_np1half;
  double dRhoAve_i_np1half;
  double dRhoAve_ip1half_np1half;
  double dRhoAve_im1_np1half;
  double dRhoAve_im1half_np1half;
  double dRhoAve_ip1_np1half;
  double dRho_ijk_np1half;
  double dRho_ip1jk_np1half;
  double dRho_im1jk_np1half;
  double dRho_ip1halfjk_np1half;
  double dRho_im1halfjk_np1half;
  double dTSq_ip1jk_n;
  double dT4_ip1jk_n;
  double dTSq_ijk_n;
  double dT4_ijk_n;
  double dTSq_im1jk_n;
  double dT4_im1jk_n;
  double dKappa_ip1halfjk_n;
  double dKappa_im1halfjk_n;
  double dEddyVisc_ip1halfjk_np1half;
  double dEddyVisc_im1halfjk_np1half;
  double dDM_ip1half;
  double dDM_im1half;
  double dUR2_ip1jk_np1half;
  double dUR2_ijk_np1half;
  double dUR2_im1jk_np1half;
  double dUR2_im1halfjk_np1half;
  double dUR2_ip1halfjk_np1half;
  double dA1CenGrad;
  double dA1UpWindGrad;
  double dUmU0_ijk_np1half;
  double dA1;
  double dPi_ijk_n;
  double dS1;
  double dTGrad_ip1half;
  double dTGrad_im1half;
  double dGrad_ip1half;
  double dGrad_im1half;
  double dS4;
  double dDivU_ip1halfjk_np1half;
  double dDivU_im1halfjk_np1half;
  double dTau_rr_ip1halfjk_np1half;
  double dTau_rr_im1halfjk_np1half;
  double dTauVR2_ip1halfjk_np1half;
  double dTauVR2_im1halfjk_np1half;
  double dT1;
  
  for(int i=grid.nStartUpdateExplicit[grid.nE][0];i<grid.nEndUpdateExplicit[grid.nE][0];i++){
    
    //calculate i for interface centered quantities
    nIInt=i+grid.nCenIntOffset[0];
    dR_ip1half_np1half=(grid.dLocalGridOld[grid.nR][nIInt][0][0]+grid.dLocalGridNew[grid.nR][nIInt][0][0])*0.5;
    dR_im1half_np1half=(grid.dLocalGridOld[grid.nR][nIInt-1][0][0]+grid.dLocalGridNew[grid.nR][nIInt-1][0][0])*0.5;
    dR_ip1_np1half=(grid.dLocalGridOld[grid.nR][nIInt+1][0][0]
      +grid.dLocalGridOld[grid.nR][nIInt][0][0]+grid.dLocalGridNew[grid.nR][nIInt+1][0][0]
      +grid.dLocalGridNew[grid.nR][nIInt][0][0])*0.25;
    dRSq_ip1_np1half=dR_ip1_np1half*dR_ip1_np1half;
    dR_im1_np1half=(grid.dLocalGridOld[grid.nR][nIInt-1][0][0]
      +grid.dLocalGridOld[grid.nR][nIInt-2][0][0]+grid.dLocalGridNew[grid.nR][nIInt-1][0][0]
      +grid.dLocalGridNew[grid.nR][nIInt-2][0][0])*0.25;
    dRSq_im1_np1half=dR_im1_np1half*dR_im1_np1half;
    dR_i_np1half=(dR_ip1half_np1half+dR_im1half_np1half)*0.5;
    dRSq_i_np1half=dR_i_np1half*dR_i_np1half;
    dRSq_ip1half_np1half=dR_ip1half_np1half*dR_ip1half_np1half;
    dR4_ip1half_np1half=dRSq_ip1half_np1half*dRSq_ip1half_np1half;
    dRSq_im1half_np1half=dR_im1half_np1half*dR_im1half_np1half;
    dR4_im1half_np1half=dRSq_im1half_np1half*dRSq_im1half_np1half;
    dRhoAve_i_np1half=grid.dLocalGridOld[grid.nD][i][0][0];
    dRhoAve_ip1_np1half=grid.dLocalGridOld[grid.nD][i+1][0][0];
    dRhoAve_im1_np1half=grid.dLocalGridOld[grid.nD][i-1][0][0];
    dRhoAve_ip1half_np1half=(dRhoAve_i_np1half+dRhoAve_ip1_np1half)*0.5;
    dRhoAve_im1half_np1half=(dRhoAve_i_np1half+dRhoAve_im1_np1half)*0.5;
    dU0_i_np1half=(grid.dLocalGridNew[grid.nU0][nIInt][0][0]
      +grid.dLocalGridNew[grid.nU0][nIInt-1][0][0])*0.5;
    dDM_ip1half=(grid.dLocalGridOld[grid.nDM][i+1][0][0]+grid.dLocalGridOld[grid.nDM][i][0][0])*0.5;
    dDM_im1half=(grid.dLocalGridOld[grid.nDM][i][0][0]+grid.dLocalGridOld[grid.nDM][i-1][0][0])*0.5;
    
    for(int j=grid.nStartUpdateExplicit[grid.nE][1];j<grid.nEndUpdateExplicit[grid.nE][1];j++){
      
      for(int k=grid.nStartUpdateExplicit[grid.nE][2];k<grid.nEndUpdateExplicit[grid.nE][2];k++){
        
        //Calculate interpolated quantities
        dU_ijk_np1half=(grid.dLocalGridNew[grid.nU][nIInt][j][k]
          +grid.dLocalGridNew[grid.nU][nIInt-1][j][k])*0.5;
        dU_ip1jk_np1half=(grid.dLocalGridNew[grid.nU][nIInt+1][j][k]
          +grid.dLocalGridNew[grid.nU][nIInt][j][k])*0.5;
        dU_im1jk_np1half=(grid.dLocalGridNew[grid.nU][nIInt-1][j][k]
          +grid.dLocalGridNew[grid.nU][nIInt-2][j][k])*0.5;
        dE_ip1halfjk_n=(grid.dLocalGridOld[grid.nE][i+1][j][k]
          +grid.dLocalGridOld[grid.nE][i][j][k])*0.5;
        dE_im1halfjk_n=(grid.dLocalGridOld[grid.nE][i][j][k]
          +grid.dLocalGridOld[grid.nE][i-1][j][k])*0.5;
        dRho_ijk_np1half=grid.dLocalGridOld[grid.nD][i][j][k];
        dRho_ip1jk_np1half=grid.dLocalGridOld[grid.nD][i+1][j][k];
        dRho_im1jk_np1half=grid.dLocalGridOld[grid.nD][i-1][j][k];
        dRho_ip1halfjk_np1half=(dRho_ip1jk_np1half+dRho_ijk_np1half)*0.5;
        dRho_im1halfjk_np1half=(dRho_ijk_np1half+dRho_im1jk_np1half)*0.5;
        dTSq_ip1jk_n=grid.dLocalGridOld[grid.nT][i+1][j][k]
          *grid.dLocalGridOld[grid.nT][i+1][j][k];
        dT4_ip1jk_n=dTSq_ip1jk_n*dTSq_ip1jk_n;
        dTSq_ijk_n=grid.dLocalGridOld[grid.nT][i][j][k]
          *grid.dLocalGridOld[grid.nT][i][j][k];
        dT4_ijk_n=dTSq_ijk_n*dTSq_ijk_n;
        dTSq_im1jk_n=grid.dLocalGridOld[grid.nT][i-1][j][k]
          *grid.dLocalGridOld[grid.nT][i-1][j][k];
        dT4_im1jk_n=dTSq_im1jk_n*dTSq_im1jk_n;
        dKappa_ip1halfjk_n=(dT4_ip1jk_n+dT4_ijk_n)
          /(dT4_ijk_n/grid.dLocalGridOld[grid.nKappa][i][j][k]
          +dT4_ip1jk_n/grid.dLocalGridOld[grid.nKappa][i+1][j][k]);
        dKappa_im1halfjk_n=(dT4_im1jk_n+dT4_ijk_n)
          /(dT4_ijk_n/grid.dLocalGridOld[grid.nKappa][i][j][k]
          +dT4_im1jk_n/grid.dLocalGridOld[grid.nKappa][i-1][j][k]);
        dEddyVisc_ip1halfjk_np1half=(grid.dLocalGridNew[grid.nEddyVisc][i][j][k]
          +grid.dLocalGridNew[grid.nEddyVisc][i+1][j][k])*0.5;
        dEddyVisc_im1halfjk_np1half=(grid.dLocalGridNew[grid.nEddyVisc][i][j][k]
          +grid.dLocalGridNew[grid.nEddyVisc][i-1][j][k])*0.5;
        dPi_ijk_n=grid.dLocalGridOld[grid.nP][i][j][k];
        #if VISCOUS_ENERGY_EQ==1
          dPi_ijk_n+=grid.dLocalGridOld[grid.nQ0][i][j][k];
        #endif
        
        //calculate derinved quantities
        dUR2_im1halfjk_np1half=grid.dLocalGridNew[grid.nU][nIInt-1][j][k]*dRSq_im1half_np1half;
        dUR2_ip1halfjk_np1half=grid.dLocalGridNew[grid.nU][nIInt][j][k]*dRSq_ip1half_np1half;
        dUR2_ip1jk_np1half=dU_ip1jk_np1half*dRSq_ip1_np1half;
        dUR2_ijk_np1half=dU_ijk_np1half*dRSq_i_np1half;
        dUR2_im1jk_np1half=dU_im1jk_np1half*dRSq_im1_np1half;
        
        //Calcuate dA1
        dA1CenGrad=(dE_ip1halfjk_n-dE_im1halfjk_n)/grid.dLocalGridOld[grid.nDM][i][0][0];
        dA1UpWindGrad=0.0;
        dUmU0_ijk_np1half=(dU_ijk_np1half-dU0_i_np1half);
        if(dUmU0_ijk_np1half<0.0){//moving in the negative direction
          dA1UpWindGrad=(grid.dLocalGridOld[grid.nE][i+1][j][k]
            -grid.dLocalGridOld[grid.nE][i][j][k])/(grid.dLocalGridOld[grid.nDM][i+1][0][0]
            +grid.dLocalGridOld[grid.nDM][i][0][0])*2.0;
        }
        else{//moving in the postive direction
          dA1UpWindGrad=(grid.dLocalGridOld[grid.nE][i][j][k]
            -grid.dLocalGridOld[grid.nE][i-1][j][k])/(grid.dLocalGridOld[grid.nDM][i][0][0]
            +grid.dLocalGridOld[grid.nDM][i-1][0][0])*2.0;
        }
        dA1=dUmU0_ijk_np1half*dRSq_i_np1half*((1.0-parameters.dDonorFrac)*dA1CenGrad
          +parameters.dDonorFrac*dA1UpWindGrad);
        
        //calculate dS1
        dS1=dPi_ijk_n/grid.dLocalGridOld[grid.nD][i][j][k]
          *(dUR2_ip1halfjk_np1half-dUR2_im1halfjk_np1half)/grid.dLocalGridOld[grid.nDM][i][0][0];
        
        //Calculate dS4
        dTGrad_ip1half=(dT4_ip1jk_n-dT4_ijk_n)/(grid.dLocalGridOld[grid.nDM][i+1][0][0]
          +grid.dLocalGridOld[grid.nDM][i][0][0])*2.0;
        dTGrad_im1half=(dT4_ijk_n-dT4_im1jk_n)/(grid.dLocalGridOld[grid.nDM][i][0][0]
          +grid.dLocalGridOld[grid.nDM][i-1][0][0])*2.0;
        dGrad_ip1half=dRhoAve_ip1half_np1half*dR4_ip1half_np1half/(dKappa_ip1halfjk_n*dRho_ip1halfjk_np1half)
          *dTGrad_ip1half;
        dGrad_im1half=dRhoAve_im1half_np1half*dR4_im1half_np1half/(dKappa_im1halfjk_n*dRho_im1halfjk_np1half)
          *dTGrad_im1half;
        dS4=16.0*parameters.dPi*parameters.dPi*grid.dLocalGridOld[grid.nD][i][0][0]
          *(dGrad_ip1half-dGrad_im1half)/grid.dLocalGridOld[grid.nDM][i][0][0];
        
        //calculate dDivU_ip1halfjk_np1half
        dDivU_ip1halfjk_np1half=4.0*parameters.dPi*dRhoAve_ip1half_np1half*(dUR2_ip1jk_np1half
          -dUR2_ijk_np1half)/dDM_ip1half;
        
        //calculate dDivU_im1halfjk_np1half
        dDivU_im1halfjk_np1half=4.0*parameters.dPi*dRhoAve_im1half_np1half*(dUR2_ijk_np1half
          -dUR2_im1jk_np1half)/dDM_im1half;
        
        //calculate dTau_rr_ip1halfjk_np1half
        dTau_rr_ip1halfjk_np1half=2.0*dEddyVisc_ip1halfjk_np1half*(4.0*parameters.dPi
          *dRSq_ip1half_np1half*dRhoAve_ip1half_np1half*(dU_ip1jk_np1half-dU_ijk_np1half)
          /dDM_ip1half-0.333333333333333*dDivU_ip1halfjk_np1half);
        
        //calculate dTau_rr_im1halfjk_np1half
        dTau_rr_im1halfjk_np1half=2.0*dEddyVisc_im1halfjk_np1half*(4.0*parameters.dPi
          *dRSq_im1half_np1half*dRhoAve_im1half_np1half*(dU_ijk_np1half-dU_im1jk_np1half)
          /dDM_im1half-0.333333333333333*dDivU_im1halfjk_np1half);
        
        //calculate dTauVR2_ip1halfjk_np1half
        dTauVR2_ip1halfjk_np1half=dRSq_ip1half_np1half*dTau_rr_ip1halfjk_np1half
          *grid.dLocalGridNew[grid.nU][nIInt][j][k];
        
        //calculate dTauVR2_im1halfjk_np1half
        dTauVR2_im1halfjk_np1half=dRSq_im1half_np1half*dTau_rr_im1halfjk_np1half
          *grid.dLocalGridNew[grid.nU][nIInt-1][j][k];
        
        //calculate dT1
        dT1=(dTauVR2_ip1halfjk_np1half-dTauVR2_im1halfjk_np1half)
          /(grid.dLocalGridOld[grid.nDM][i][0][0]*grid.dLocalGridOld[grid.nD][i][j][k]);
        
        //calculate new energy
        grid.dLocalGridNew[grid.nE][i][j][k]=grid.dLocalGridOld[grid.nE][i][j][k]
          -time.dDeltat_n*(4.0*parameters.dPi*grid.dLocalGridOld[grid.nD][i][0][0]*(dA1+dS1-dT1)
          -4.0*parameters.dSigma/(3.0*grid.dLocalGridOld[grid.nD][i][j][k])*(dS4));
        
        if(grid.dLocalGridNew[grid.nE][i][j][k]<0.0){
          
          #if SIGNEGENG==1
          raise(SIGINT);
          #endif
          
          std::stringstream ssTemp;
          ssTemp<<__FILE__<<":"<<__FUNCTION__<<":"<<__LINE__<<":"<<procTop.nRank
            <<": negative energy calculated in , ("<<i<<","<<j<<","<<k<<")\n";
          throw exception2(ssTemp.str(),CALCULATION);
          
        }
      }
    }
  }
  
  //ghost region 0, outter most ghost region in x1 direction
  for(int i=grid.nStartGhostUpdateExplicit[grid.nE][0][0];i<grid.nEndGhostUpdateExplicit[grid.nE][0][0];i++){
    
    //calculate i for interface centered quantities
    nIInt=i+grid.nCenIntOffset[0];
    dR_ip1half_np1half=(grid.dLocalGridOld[grid.nR][nIInt][0][0]
      +grid.dLocalGridNew[grid.nR][nIInt][0][0])*0.5;
    dR_im1half_np1half=(grid.dLocalGridOld[grid.nR][nIInt-1][0][0]
      +grid.dLocalGridNew[grid.nR][nIInt-1][0][0])*0.5;
    dR_im1_np1half=(grid.dLocalGridOld[grid.nR][nIInt-1][0][0]
      +grid.dLocalGridOld[grid.nR][nIInt-2][0][0]+grid.dLocalGridNew[grid.nR][nIInt-1][0][0]
      +grid.dLocalGridNew[grid.nR][nIInt-2][0][0])*0.25;
    dRSq_im1_np1half=dR_im1_np1half*dR_im1_np1half;
    dRSq_im1half_np1half=dR_im1half_np1half*dR_im1half_np1half;
    dR4_im1half_np1half=dRSq_im1half_np1half*dRSq_im1half_np1half;
    dR_i_np1half=(dR_ip1half_np1half+dR_im1half_np1half)*0.5;
    dRSq_i_np1half=dR_i_np1half*dR_i_np1half;
    dRSq_ip1half_np1half=dR_ip1half_np1half*dR_ip1half_np1half;
    dR4_ip1half_np1half=dRSq_ip1half_np1half*dRSq_ip1half_np1half;
    dRSq_im1half_np1half=dR_im1half_np1half*dR_im1half_np1half;
    dR4_im1half_np1half=dRSq_im1half_np1half*dRSq_im1half_np1half;
    dRhoAve_i_np1half=grid.dLocalGridOld[grid.nD][i][0][0];
    dRhoAve_ip1_np1half=0.0;
    dRhoAve_im1_np1half=grid.dLocalGridOld[grid.nD][i-1][0][0];
    dRhoAve_ip1half_np1half=(dRhoAve_i_np1half+dRhoAve_ip1_np1half)*0.5;
    dRhoAve_im1half_np1half=(dRhoAve_i_np1half+dRhoAve_im1_np1half)*0.5;
    dU0_i_np1half=(grid.dLocalGridNew[grid.nU0][nIInt][0][0]
      +grid.dLocalGridNew[grid.nU0][nIInt-1][0][0])*0.5;
    dDM_ip1half=(0.0+grid.dLocalGridOld[grid.nDM][i][0][0])*0.5;
    dDM_im1half=(grid.dLocalGridOld[grid.nDM][i][0][0]+grid.dLocalGridOld[grid.nDM][i-1][0][0])*0.5;
    
    for(int j=grid.nStartGhostUpdateExplicit[grid.nE][0][1];j<grid.nEndGhostUpdateExplicit[grid.nE][0][1];j++){
      
      for(int k=grid.nStartGhostUpdateExplicit[grid.nE][0][2];k<grid.nEndGhostUpdateExplicit[grid.nE][0][2];k++){
        
        //Calculate interpolated quantities
        dU_ijk_np1half=(grid.dLocalGridNew[grid.nU][nIInt][j][k]
          +grid.dLocalGridNew[grid.nU][nIInt-1][j][k])*0.5;
        dU0_i_np1half=(grid.dLocalGridNew[grid.nU0][nIInt][0][0]
          +grid.dLocalGridNew[grid.nU0][nIInt-1][0][0])*0.5;
        dU_im1jk_np1half=(grid.dLocalGridNew[grid.nU][nIInt-2][j][k]
          +grid.dLocalGridNew[grid.nU][nIInt-1][j][k])*0.5;
        dE_ip1halfjk_n=(grid.dLocalGridOld[grid.nE][i][j][k]);/**\BC Missing
          grid.dLocalGridOld[grid.nE][i+1][j][k] in calculation of \f$E_{i+1/2,j,k}\f$ 
          setting it equal to value at i.*/
        dE_im1halfjk_n=(grid.dLocalGridOld[grid.nE][i][j][k]
          +grid.dLocalGridOld[grid.nE][i-1][j][k])*0.5;
        dTSq_ijk_n=grid.dLocalGridOld[grid.nT][i][j][k]
          *grid.dLocalGridOld[grid.nT][i][j][k];
        dT4_ijk_n=dTSq_ijk_n*dTSq_ijk_n;
        dTSq_im1jk_n=grid.dLocalGridOld[grid.nT][i-1][j][k]
          *grid.dLocalGridOld[grid.nT][i-1][j][k];
        dT4_im1jk_n=dTSq_im1jk_n*dTSq_im1jk_n;
        dKappa_im1halfjk_n=(dT4_im1jk_n+dT4_ijk_n)
          /(dT4_ijk_n/grid.dLocalGridOld[grid.nKappa][i][j][k]
          +dT4_im1jk_n/grid.dLocalGridOld[grid.nKappa][i-1][j][k]);
        dRho_ijk_np1half=grid.dLocalGridOld[grid.nD][i][j][k];
        dRho_im1jk_np1half=grid.dLocalGridOld[grid.nD][i-1][j][k];
        dRho_im1halfjk_np1half=(dRho_ijk_np1half+dRho_im1jk_np1half)*0.5;
        
        dEddyVisc_ip1halfjk_np1half=(grid.dLocalGridNew[grid.nEddyVisc][i][j][k]
          +grid.dLocalGridNew[grid.nEddyVisc][i+1][j][k])*0.5;
        dEddyVisc_im1halfjk_np1half=(grid.dLocalGridNew[grid.nEddyVisc][i][j][k]
          +grid.dLocalGridNew[grid.nEddyVisc][i-1][j][k])*0.5;
        
        //Calcuate dA1
        dA1CenGrad=(dE_ip1halfjk_n-dE_im1halfjk_n)/grid.dLocalGridOld[grid.nDM][i][0][0];
        dA1UpWindGrad=0.0;
        dUmU0_ijk_np1half=(dU_ijk_np1half-dU0_i_np1half);
        if(dUmU0_ijk_np1half<0.0){//moving in the negative direction
          dA1UpWindGrad=dA1CenGrad;/**\BC grid.dLocalGridOld[grid.nDM][i+1][0][0] and 
            grid.dLocalGridOld[grid.nE][i+1][j][k] missing in the calculation of upwind gradient in 
            dA1. Using the centered gradient instead.*/
        }
        else{//moving in the postive direction*/
          dA1UpWindGrad=(grid.dLocalGridOld[grid.nE][i][j][k]
            -grid.dLocalGridOld[grid.nE][i-1][j][k])/(grid.dLocalGridOld[grid.nDM][i][0][0]
            +grid.dLocalGridOld[grid.nDM][i-1][0][0])*2.0;
        }
        dA1=dUmU0_ijk_np1half*dRSq_i_np1half*((1.0-parameters.dDonorFrac)*dA1CenGrad
          +parameters.dDonorFrac*dA1UpWindGrad);
        
        //calculate dUR2_im1half_np1half
        dUR2_im1halfjk_np1half=grid.dLocalGridNew[grid.nU][nIInt-1][j][k]*dRSq_im1half_np1half;
        
        //calculate dR2_ip1half_np1half
        dUR2_ip1halfjk_np1half=grid.dLocalGridNew[grid.nU][nIInt][j][k]*dRSq_ip1half_np1half;
        
        //calculate dURsq_ijk_np1half
        dUR2_ijk_np1half=dU_ijk_np1half*dRSq_i_np1half;
        
        //calculate dURsq_im1jk_np1half
        dUR2_im1jk_np1half=dU_im1jk_np1half*dRSq_im1_np1half;
        
        //calculate dS1
        dPi_ijk_n=grid.dLocalGridOld[grid.nP][i][j][k];
        #if VISCOUS_ENERGY_EQ==1
          dPi_ijk_n+=grid.dLocalGridOld[grid.nQ0][i][j][k];
        #endif
        dS1=dPi_ijk_n/grid.dLocalGridOld[grid.nD][i][j][k]
          *(dUR2_ip1halfjk_np1half-dUR2_im1halfjk_np1half)/grid.dLocalGridOld[grid.nDM][i][0][0];
        
        //Calculate dS4
        dTGrad_im1half=(dT4_ijk_n-dT4_im1jk_n)/(grid.dLocalGridOld[grid.nDM][i][0][0]
          +grid.dLocalGridOld[grid.nDM][i-1][0][0])*2.0;
        dGrad_ip1half=-3.0*dRSq_ip1half_np1half*dT4_ijk_n/(8.0*parameters.dPi);/**\BC
          Missing grid.dLocalGridOld[grid.nT][i+1][0][0]*/
        dGrad_im1half=dRhoAve_im1half_np1half*dR4_im1half_np1half/(dKappa_im1halfjk_n
          *dRho_im1halfjk_np1half)*dTGrad_im1half;
        dS4=16.0*parameters.dPi*parameters.dPi*grid.dLocalGridOld[grid.nD][i][0][0]
          *(dGrad_ip1half-dGrad_im1half)/grid.dLocalGridOld[grid.nDM][i][0][0];
       
        //calculate dDivU_ip1halfjk_np1half
        dDivU_ip1halfjk_np1half=4.0*parameters.dPi*dRhoAve_ip1half_np1half*(dUR2_ip1halfjk_np1half
          -dUR2_ijk_np1half)/dDM_ip1half;
        
        //calculate dDivU_im1halfjk_np1half
        dDivU_im1halfjk_np1half=4.0*parameters.dPi*dRhoAve_im1half_np1half*(dUR2_ijk_np1half
          -dUR2_im1jk_np1half)/dDM_im1half;
        
        //calculate dTau_rr_ip1halfjk_np1half
        dTau_rr_ip1halfjk_np1half=2.0*dEddyVisc_ip1halfjk_np1half*(4.0*parameters.dPi
          *dRSq_ip1half_np1half*dRhoAve_ip1half_np1half*(grid.dLocalGridNew[grid.nU][nIInt][j][k]
          -dU_ijk_np1half)/dDM_ip1half-0.333333333333333*dDivU_ip1halfjk_np1half);
        
        //calculate dTau_rr_im1halfjk_np1half
        dTau_rr_im1halfjk_np1half=2.0*dEddyVisc_im1halfjk_np1half*(4.0*parameters.dPi
          *dRSq_im1half_np1half*dRhoAve_im1half_np1half*(dU_ijk_np1half-dU_im1jk_np1half)
          /dDM_im1half-0.333333333333333*dDivU_im1halfjk_np1half);
        
        //calculate dTauVR2_ip1halfjk_np1half
        dTauVR2_ip1halfjk_np1half=dRSq_ip1half_np1half*dTau_rr_ip1halfjk_np1half
          *grid.dLocalGridNew[grid.nU][nIInt][j][k];
        
        //calculate dTauVR2_im1halfjk_np1half
        dTauVR2_im1halfjk_np1half=dRSq_im1half_np1half*dTau_rr_im1halfjk_np1half
          *grid.dLocalGridNew[grid.nU][nIInt-1][j][k];
        
        //calculate dT1
        dT1=(dTauVR2_ip1halfjk_np1half-dTauVR2_im1halfjk_np1half)
          /(grid.dLocalGridOld[grid.nDM][i][0][0]*grid.dLocalGridOld[grid.nD][i][j][k]);
        
        //calculate new energy
        grid.dLocalGridNew[grid.nE][i][j][k]=grid.dLocalGridOld[grid.nE][i][j][k]
          -time.dDeltat_n*(4.0*parameters.dPi*grid.dLocalGridOld[grid.nD][i][0][0]*(dA1+dS1-dT1)
          -4.0*parameters.dSigma/(3.0*grid.dLocalGridOld[grid.nD][i][j][k])*(dS4));
        
        if(grid.dLocalGridNew[grid.nE][i][j][k]<0.0){
          
          #if SIGNEGENG==1
          raise(SIGINT);
          #endif
          
          std::stringstream ssTemp;
          ssTemp<<__FILE__<<":"<<__FUNCTION__<<":"<<__LINE__<<":"<<procTop.nRank
            <<": negative energy calculated in , ("<<i<<","<<j<<","<<k<<")\n";
          throw exception2(ssTemp.str(),CALCULATION);
          
        }
      }
    }
  }
}
void calNewE_RT_AD(Grid &grid, Parameters &parameters, Time &time, ProcTop &procTop){
  
  for(int i=grid.nStartUpdateExplicit[grid.nE][0];i<grid.nEndUpdateExplicit[grid.nE][0];i++){
    
    //calculate i for interface centered quantities
    int nIInt=i+grid.nCenIntOffset[0];
    
    for(int j=grid.nStartUpdateExplicit[grid.nE][1];j<grid.nEndUpdateExplicit[grid.nE][1];j++){
      
      //calculate i for interface centered quantities
      int nJInt=j+grid.nCenIntOffset[1];
      
      for(int k=grid.nStartUpdateExplicit[grid.nE][2];k<grid.nEndUpdateExplicit[grid.nE][2];k++){
        
        //Calculate interpolated quantities
        double dU_ijk_np1half=(grid.dLocalGridNew[grid.nU][nIInt][j][k]
          +grid.dLocalGridNew[grid.nU][nIInt-1][j][k])*0.5;
        double dU0_i_np1half=(grid.dLocalGridNew[grid.nU0][nIInt][0][0]
          +grid.dLocalGridNew[grid.nU0][nIInt-1][0][0])*0.5;
        double dE_ip1halfjk_n=(grid.dLocalGridOld[grid.nE][i+1][j][k]+grid.dLocalGridOld[grid.nE][i][j][k])*0.5;
        double dE_im1halfjk_n=(grid.dLocalGridOld[grid.nE][i][j][k]+grid.dLocalGridOld[grid.nE][i-1][j][k])*0.5;
        double dR_i_n=(grid.dLocalGridOld[grid.nR][nIInt][0][0]+grid.dLocalGridOld[grid.nR][nIInt-1][0][0])*0.5;
        double dR_im1half_n=grid.dLocalGridOld[grid.nR][nIInt-1][0][0];
        double dR_ip1half_n=grid.dLocalGridOld[grid.nR][nIInt][0][0];
        double dRSq_i_n=dR_i_n*dR_i_n;
        double dV_ijk_np1half=(grid.dLocalGridNew[grid.nV][i][nJInt][k]
          +grid.dLocalGridNew[grid.nV][i][nJInt-1][k])*0.5;
        double dE_ijp1halfk_n=(grid.dLocalGridOld[grid.nE][i][j+1][k]+grid.dLocalGridOld[grid.nE][i][j][k])*0.5;
        double dE_ijm1halfk_n=(grid.dLocalGridOld[grid.nE][i][j][k]+grid.dLocalGridOld[grid.nE][i][j-1][k])*0.5;
        double dVSinTheta_ijp1halfk_np1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt][0]
          *grid.dLocalGridNew[grid.nV][i][nJInt][k];
        double dVSinTheta_ijm1halfk_np1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt-1][0]
          *grid.dLocalGridNew[grid.nV][i][nJInt-1][k];
        
        //Calcuate dA1
        double dA1CenGrad=(dE_ip1halfjk_n-dE_im1halfjk_n)/grid.dLocalGridOld[grid.nDM][i][0][0];
        double dA1UpWindGrad=0.0;
        double dU_U0_Diff=(dU_ijk_np1half-dU0_i_np1half);
        if(dU_U0_Diff<0.0){//moving in the negative direction
          dA1UpWindGrad=(grid.dLocalGridOld[grid.nE][i+1][j][k]-grid.dLocalGridOld[grid.nE][i][j][k])
            /(grid.dLocalGridOld[grid.nDM][i+1][0][0]+grid.dLocalGridOld[grid.nDM][i][0][0])*2.0;
        }
        else{//moving in the postive direction
          dA1UpWindGrad=(grid.dLocalGridOld[grid.nE][i][j][k]-grid.dLocalGridOld[grid.nE][i-1][j][k])
            /(grid.dLocalGridOld[grid.nDM][i][0][0]+grid.dLocalGridOld[grid.nDM][i-1][0][0])*2.0;
        }
        double dA1=dU_U0_Diff*dRSq_i_n*((1.0-parameters.dDonorFrac)*dA1CenGrad
          +parameters.dDonorFrac*dA1UpWindGrad);
        
        //calculate dS1
        double dUR2_im1half_np1half=grid.dLocalGridNew[grid.nU][nIInt-1][j][k]*dR_im1half_n*dR_im1half_n;
        double dUR2_ip1half_np1half=grid.dLocalGridNew[grid.nU][nIInt][j][k]*dR_ip1half_n*dR_ip1half_n;
        double dP=grid.dLocalGridOld[grid.nP][i][j][k];
        #if VISCOUS_ENERGY_EQ==1
          dP+=grid.dLocalGridOld[grid.nQ0][i][j][k];
        #endif
        double dS1=dP/grid.dLocalGridOld[grid.nD][i][j][k]*(dUR2_ip1half_np1half-dUR2_im1half_np1half)
          /grid.dLocalGridOld[grid.nDM][i][0][0];
        
        //Calcualte dA2
        double dA2CenGrad=(dE_ijp1halfk_n-dE_ijm1halfk_n)/grid.dLocalGridOld[grid.nDTheta][0][j][0];
        double dA2UpWindGrad=0.0;
        if(dV_ijk_np1half<0.0){//moving in the negative direction
          dA2UpWindGrad=(grid.dLocalGridOld[grid.nE][i][j+1][k]-grid.dLocalGridOld[grid.nE][i][j][k])/
            (grid.dLocalGridOld[grid.nDTheta][0][j+1][0]+grid.dLocalGridOld[grid.nDTheta][0][j][0])*2.0;
        }
        else{//moving in the positive direction
          dA2UpWindGrad=(grid.dLocalGridOld[grid.nE][i][j][k]-grid.dLocalGridOld[grid.nE][i][j-1][k])/
            (grid.dLocalGridOld[grid.nDTheta][0][j][0]+grid.dLocalGridOld[grid.nDTheta][0][j-1][0])*2.0;
        }
        double dA2=dV_ijk_np1half/dR_i_n*((1.0-parameters.dDonorFrac)*dA2CenGrad
          +parameters.dDonorFrac*dA2UpWindGrad);
          
        //Calcualte dS2
        dP=grid.dLocalGridOld[grid.nP][i][j][k];
        #if VISCOUS_ENERGY_EQ==1
          dP+=grid.dLocalGridOld[grid.nQ1][i][j][k];
        #endif
        double dS2=dP/(grid.dLocalGridOld[grid.nD][i][j][k]*dR_i_n
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]*grid.dLocalGridOld[grid.nDTheta][0][j][0])
          *(dVSinTheta_ijp1halfk_np1half-dVSinTheta_ijm1halfk_np1half);
        
        //calculate new energy
        grid.dLocalGridNew[grid.nE][i][j][k]=grid.dLocalGridOld[grid.nE][i][j][k]
        -time.dDeltat_n*(4.0*parameters.dPi*grid.dLocalGridOld[grid.nDenAve][i][0][0]*(dA1+dS1)+dA2+dS2);
        
        if(grid.dLocalGridNew[grid.nE][i][j][k]<0.0){
          
          #if SIGNEGENG==1
          raise(SIGINT);
          #endif
          
          std::stringstream ssTemp;
          ssTemp<<__FILE__<<":"<<__FUNCTION__<<":"<<__LINE__<<":"<<procTop.nRank
            <<": negative energy calculated in , ("<<i<<","<<j<<","<<k<<")\n";
          throw exception2(ssTemp.str(),CALCULATION);
          
        }
      }
    }
  }
  
  //ghost region 0, outter most ghost region in x1 direction
  for(int i=grid.nStartGhostUpdateExplicit[grid.nE][0][0];i<grid.nEndGhostUpdateExplicit[grid.nE][0][0];i++){
    
    //calculate i for interface centered quantities
    int nIInt=i+grid.nCenIntOffset[0];
    
    for(int j=grid.nStartGhostUpdateExplicit[grid.nE][0][1];j<grid.nEndGhostUpdateExplicit[grid.nE][0][1];j++){
      
      //calculate i for interface centered quantities
      int nJInt=j+grid.nCenIntOffset[1];
      
      for(int k=grid.nStartGhostUpdateExplicit[grid.nE][0][2];k<grid.nEndGhostUpdateExplicit[grid.nE][0][2];k++){
        
        
        //Calculate interpolated quantities
        double dU_ijk_np1half=(grid.dLocalGridNew[grid.nU][nIInt][j][k]
          +grid.dLocalGridNew[grid.nU][nIInt-1][j][k])*0.5;
        double dU0_i_np1half=(grid.dLocalGridNew[grid.nU0][nIInt][0][0]
          +grid.dLocalGridNew[grid.nU0][nIInt-1][0][0])
          *0.5;
        double dE_ip1halfjk_n=0.0;/**\BC grid.dLocalGridOld[grid.nE][i+1][j][k] is missing*/
        double dE_im1halfjk_n=(grid.dLocalGridOld[grid.nE][i][j][k]+grid.dLocalGridOld[grid.nE][i-1][j][k])*0.5;
        double dR_i_n=(grid.dLocalGridOld[grid.nR][nIInt][0][0]+grid.dLocalGridOld[grid.nR][nIInt-1][0][0])*0.5;
        double dR_im1half_n=grid.dLocalGridOld[grid.nR][nIInt-1][0][0];
        double dR_ip1half_n=grid.dLocalGridOld[grid.nR][nIInt][0][0];
        double dRSq_i_n=dR_i_n*dR_i_n;
        double dV_ijk_np1half=(grid.dLocalGridNew[grid.nV][i][nJInt][k]
          +grid.dLocalGridNew[grid.nV][i][nJInt-1][k])*0.5;
        double dE_ijp1halfk_n=(grid.dLocalGridOld[grid.nE][i][j+1][k]+grid.dLocalGridOld[grid.nE][i][j][k])*0.5;
        double dE_ijm1halfk_n=(grid.dLocalGridOld[grid.nE][i][j][k]+grid.dLocalGridOld[grid.nE][i][j-1][k])*0.5;
        double dVSinTheta_ijp1halfk_np1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt][0]
          *grid.dLocalGridNew[grid.nV][i][nJInt][k];
        double dVSinTheta_ijm1halfk_np1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt-1][0]
          *grid.dLocalGridNew[grid.nV][i][nJInt-1][k];
        
        //Calcuate dA1
        double dA1CenGrad=(dE_ip1halfjk_n-dE_im1halfjk_n)
          /grid.dLocalGridOld[grid.nDM][i][0][0];
        double dA1UpWindGrad=0.0;
        double dU_U0_Diff=(dU_ijk_np1half-dU0_i_np1half);
        /**\BC grid.dLocalGridOld[grid.nDM][i+1][0][0] and grid.dLocalGridOld[grid.nE][i+1][j][k] missing 
        using inner gradient for both*/
        dA1UpWindGrad=(grid.dLocalGridOld[grid.nE][i][j][k]-grid.dLocalGridOld[grid.nE][i-1][j][k])
          /(grid.dLocalGridOld[grid.nDM][i][0][0]+grid.dLocalGridOld[grid.nDM][i-1][0][0])*2.0;
        double dA1=dU_U0_Diff*dRSq_i_n*((1.0-parameters.dDonorFrac)*dA1CenGrad
          +parameters.dDonorFrac*dA1UpWindGrad);
        
        //calculate dS1
        double dUR2_im1half_np1half=grid.dLocalGridNew[grid.nU][nIInt-1][j][k]*dR_im1half_n*dR_im1half_n;
        double dUR2_ip1half_np1half=grid.dLocalGridNew[grid.nU][nIInt][j][k]*dR_ip1half_n*dR_ip1half_n;
        double dP=grid.dLocalGridOld[grid.nP][i][j][k];
        #if VISCOUS_ENERGY_EQ==1
          dP+=grid.dLocalGridOld[grid.nQ0][i][j][k];
        #endif
        double dS1=dP/grid.dLocalGridOld[grid.nD][i][j][k]*(dUR2_ip1half_np1half-dUR2_im1half_np1half)
          /grid.dLocalGridOld[grid.nDM][i][0][0];
        
        //Calcualte dA2
        double dA2CenGrad=(dE_ijp1halfk_n-dE_ijm1halfk_n)/grid.dLocalGridOld[grid.nDTheta][0][j][0];
        double dA2UpWindGrad=0.0;
        if(dV_ijk_np1half<0.0){//moving in the negative direction
          dA2UpWindGrad=(grid.dLocalGridOld[grid.nE][i][j+1][k]-grid.dLocalGridOld[grid.nE][i][j][k])/
            (grid.dLocalGridOld[grid.nDTheta][0][j+1][0]+grid.dLocalGridOld[grid.nDTheta][0][j][0])*2.0;
        }
        else{//moving in the positive direction
          dA2UpWindGrad=(grid.dLocalGridOld[grid.nE][i][j][k]-grid.dLocalGridOld[grid.nE][i][j-1][k])/
            (grid.dLocalGridOld[grid.nDTheta][0][j][0]+grid.dLocalGridOld[grid.nDTheta][0][j-1][0])*2.0;
        }
        double dA2=dV_ijk_np1half/dR_i_n*((1.0-parameters.dDonorFrac)*dA2CenGrad
          +parameters.dDonorFrac*dA2UpWindGrad);
          
        //Calcualte dS2
        dP=grid.dLocalGridOld[grid.nP][i][j][k];
        #if VISCOUS_ENERGY_EQ==1
          dP+=grid.dLocalGridOld[grid.nQ1][i][j][k];
        #endif
        double dS2=dP/(grid.dLocalGridOld[grid.nD][i][j][k]*dR_i_n
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]*grid.dLocalGridOld[grid.nDTheta][0][j][0])
          *(dVSinTheta_ijp1halfk_np1half-dVSinTheta_ijm1halfk_np1half);
        
        //calculate new energy
        grid.dLocalGridNew[grid.nE][i][j][k]=grid.dLocalGridOld[grid.nE][i][j][k]
        -time.dDeltat_n*(4.0*parameters.dPi*grid.dLocalGridOld[grid.nDenAve][i][0][0]*(dA1+dS1)+dA2+dS2);
        
        if(grid.dLocalGridNew[grid.nE][i][j][k]<0.0){
          
          #if SIGNEGENG==1
          raise(SIGINT);
          #endif
          
          std::stringstream ssTemp;
          ssTemp<<__FILE__<<":"<<__FUNCTION__<<":"<<__LINE__<<":"<<procTop.nRank
            <<": negative energy calculated in , ("<<i<<","<<j<<","<<k<<")\n";
          throw exception2(ssTemp.str(),CALCULATION);
          
        }
      }
    }
  }
  
  #if SEDOV==1 //use zero P, E, and rho gradients
    for(int i=grid.nStartGhostUpdateExplicit[grid.nE][1][0];i<grid.nEndGhostUpdateExplicit[grid.nE][1][0];i++){
      
      for(int j=grid.nStartGhostUpdateExplicit[grid.nE][1][1];j<grid.nEndGhostUpdateExplicit[grid.nE][1][1];j++){
        for(int k=grid.nStartGhostUpdateExplicit[grid.nE][1][2];k<grid.nEndGhostUpdateExplicit[grid.nE][1][2];k++){
          grid.dLocalGridNew[grid.nE][i][j][k]=grid.dLocalGridNew[grid.nE][i+1][j][k];
        }
      }
    }
  #endif
}
void calNewE_RT_NA(Grid &grid, Parameters &parameters, Time &time, ProcTop &procTop){
  for(int i=grid.nStartUpdateExplicit[grid.nE][0];i<grid.nEndUpdateExplicit[grid.nE][0];i++){
    
    //calculate i for interface centered quantities
    int nIInt=i+grid.nCenIntOffset[0];
    
    for(int j=grid.nStartUpdateExplicit[grid.nE][1];j<grid.nEndUpdateExplicit[grid.nE][1];j++){
      
      //calculate i for interface centered quantities
      int nJInt=j+grid.nCenIntOffset[1];
      
      for(int k=grid.nStartUpdateExplicit[grid.nE][2];k<grid.nEndUpdateExplicit[grid.nE][2];k++){
        
        //Calculate interpolated quantities
        double dU_ijk_np1half=(grid.dLocalGridNew[grid.nU][nIInt][j][k]+grid.dLocalGridNew[grid.nU][nIInt-1][j][k])*0.5;
        double dU0_i_np1half=(grid.dLocalGridNew[grid.nU0][nIInt][0][0]+grid.dLocalGridNew[grid.nU0][nIInt-1][0][0])*0.5;
        double dE_ip1halfjk_n=(grid.dLocalGridOld[grid.nE][i+1][j][k]+grid.dLocalGridOld[grid.nE][i][j][k])*0.5;
        double dE_im1halfjk_n=(grid.dLocalGridOld[grid.nE][i][j][k]+grid.dLocalGridOld[grid.nE][i-1][j][k])*0.5;
        double dR_i_n=(grid.dLocalGridOld[grid.nR][nIInt][0][0]+grid.dLocalGridOld[grid.nR][nIInt-1][0][0])*0.5;
        double dR_im1half_n=grid.dLocalGridOld[grid.nR][nIInt-1][0][0];
        double dR_ip1half_n=grid.dLocalGridOld[grid.nR][nIInt][0][0];
        double dRSq_i_n=dR_i_n*dR_i_n;
        double dRSq_ip1half=dR_ip1half_n*dR_ip1half_n;
        double dR4_ip1half=dRSq_ip1half*dRSq_ip1half;
        double dR_im1half_sq=dR_im1half_n*dR_im1half_n;
        double dR_im1half_4=dR_im1half_sq*dR_im1half_sq;
        double dV_ijk_np1half=(grid.dLocalGridNew[grid.nV][i][nJInt][k]+grid.dLocalGridNew[grid.nV][i][nJInt-1][k])*0.5;
        double dE_ijp1halfk_n=(grid.dLocalGridOld[grid.nE][i][j+1][k]+grid.dLocalGridOld[grid.nE][i][j][k])*0.5;
        double dE_ijm1halfk_n=(grid.dLocalGridOld[grid.nE][i][j][k]+grid.dLocalGridOld[grid.nE][i][j-1][k])*0.5;
        double dVSinTheta_ijp1halfk_np1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt][0]*grid.dLocalGridNew[grid.nV][i][nJInt][k];
        double dVSinTheta_ijm1halfk_np1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt-1][0]*grid.dLocalGridNew[grid.nV][i][nJInt-1][k];
        double dRhoAve_ip1half=(grid.dLocalGridOld[grid.nDenAve][i+1][0][0]+grid.dLocalGridOld[grid.nDenAve][i][0][0])*0.5;
        double dRhoAve_im1half=(grid.dLocalGridOld[grid.nDenAve][i][0][0]+grid.dLocalGridOld[grid.nDenAve][i-1][0][0])*0.5;
        double dRho_ip1halfjk=(grid.dLocalGridOld[grid.nD][i+1][j][k]+grid.dLocalGridOld[grid.nD][i][j][k])*0.5;
        double dRho_im1halfjk=(grid.dLocalGridOld[grid.nD][i][j][k]+grid.dLocalGridOld[grid.nD][i-1][j][k])*0.5;
        double dRho_ijp1halfk=(grid.dLocalGridOld[grid.nD][i][j+1][k]+grid.dLocalGridOld[grid.nD][i][j][k])*0.5;
        double dRho_ijm1halfk=(grid.dLocalGridOld[grid.nD][i][j][k]+grid.dLocalGridOld[grid.nD][i][j-1][k])*0.5;
        double dTSq_ip1jk_n=grid.dLocalGridOld[grid.nT][i+1][j][k]*grid.dLocalGridOld[grid.nT][i+1][j][k];
        double dT4_ip1jk_n=dTSq_ip1jk_n*dTSq_ip1jk_n;
        double dTSq_ijk_n=grid.dLocalGridOld[grid.nT][i][j][k]*grid.dLocalGridOld[grid.nT][i][j][k];
        double dT4_ijk_n=dTSq_ijk_n*dTSq_ijk_n;
        double dTSq_im1jk_n=grid.dLocalGridOld[grid.nT][i-1][j][k]*grid.dLocalGridOld[grid.nT][i-1][j][k];
        double dT4_im1jk_n=dTSq_im1jk_n*dTSq_im1jk_n;
        double dTSq_ijp1k_n=grid.dLocalGridOld[grid.nT][i][j+1][k]*grid.dLocalGridOld[grid.nT][i][j+1][k];
        double dT4_ijp1k_n=dTSq_ijp1k_n*dTSq_ijp1k_n;
        double dTSq_ijm1k_n=grid.dLocalGridOld[grid.nT][i][j-1][k]*grid.dLocalGridOld[grid.nT][i][j-1][k];
        double dT4_ijm1k_n=dTSq_ijm1k_n*dTSq_ijm1k_n;
        double dKappa_ip1halfjk_n=(dT4_ip1jk_n+dT4_ijk_n)/(dT4_ijk_n/grid.dLocalGridOld[grid.nKappa][i][j][k]+dT4_ip1jk_n/grid.dLocalGridOld[grid.nKappa][i+1][j][k]);
        double dKappa_im1halfjk_n=(dT4_im1jk_n+dT4_ijk_n)/(dT4_ijk_n/grid.dLocalGridOld[grid.nKappa][i][j][k]+dT4_im1jk_n/grid.dLocalGridOld[grid.nKappa][i-1][j][k]);
        double dKappa_ijp1halfk_n=(dT4_ijp1k_n+dT4_ijk_n)/(dT4_ijk_n/grid.dLocalGridOld[grid.nKappa][i][j][k]+dT4_ijp1k_n/grid.dLocalGridOld[grid.nKappa][i][j+1][k]);
        double dKappa_ijm1halfk_n=(dT4_ijm1k_n+dT4_ijk_n)/(dT4_ijk_n/grid.dLocalGridOld[grid.nKappa][i][j][k]+dT4_ijm1k_n/grid.dLocalGridOld[grid.nKappa][i][j-1][k]);
        
        //Calcuate dA1
        double dA1CenGrad=(dE_ip1halfjk_n-dE_im1halfjk_n)/grid.dLocalGridOld[grid.nDM][i][0][0];
        double dA1UpWindGrad=0.0;
        double dU_U0_Diff=(dU_ijk_np1half-dU0_i_np1half);
        if(dU_U0_Diff<0.0){//moving in the negative direction
          dA1UpWindGrad=(grid.dLocalGridOld[grid.nE][i+1][j][k]
            -grid.dLocalGridOld[grid.nE][i][j][k])/(grid.dLocalGridOld[grid.nDM][i+1][0][0]
            +grid.dLocalGridOld[grid.nDM][i][0][0])*2.0;
        }
        else{//moving in the postive direction
          dA1UpWindGrad=(grid.dLocalGridOld[grid.nE][i][j][k]
            -grid.dLocalGridOld[grid.nE][i-1][j][k])/(grid.dLocalGridOld[grid.nDM][i][0][0]
            +grid.dLocalGridOld[grid.nDM][i-1][0][0])*2.0;
        }
        double dA1=dU_U0_Diff*dRSq_i_n*((1.0-parameters.dDonorFrac)*dA1CenGrad
          +parameters.dDonorFrac*dA1UpWindGrad);
        
        //calculate dS1
        double dUR2_im1half_np1half=grid.dLocalGridNew[grid.nU][nIInt-1][j][k]*dR_im1half_n
          *dR_im1half_n;
        double dUR2_ip1half_np1half=grid.dLocalGridNew[grid.nU][nIInt][j][k]*dR_ip1half_n
          *dR_ip1half_n;
        double dPi_ijk_n=grid.dLocalGridOld[grid.nP][i][j][k];
        #if VISCOUS_ENERGY_EQ==1
          dPi_ijk_n+=grid.dLocalGridOld[grid.nQ0][i][j][k];
        #endif
        double dS1=dPi_ijk_n/grid.dLocalGridOld[grid.nD][i][j][k]
          *(dUR2_ip1half_np1half-dUR2_im1half_np1half)/grid.dLocalGridOld[grid.nDM][i][0][0];
        
        //Calcualte dA2
        double dA2CenGrad=(dE_ijp1halfk_n-dE_ijm1halfk_n)/grid.dLocalGridOld[grid.nDTheta][0][j][0];
        double dA2UpWindGrad=0.0;
        if(dV_ijk_np1half<0.0){//moving in the negative direction
          dA2UpWindGrad=(grid.dLocalGridOld[grid.nE][i][j+1][k]
            -grid.dLocalGridOld[grid.nE][i][j][k])/(grid.dLocalGridOld[grid.nDTheta][0][j+1][0]
            +grid.dLocalGridOld[grid.nDTheta][0][j][0])*2.0;
        }
        else{//moving in the positive direction
          dA2UpWindGrad=(grid.dLocalGridOld[grid.nE][i][j][k]
            -grid.dLocalGridOld[grid.nE][i][j-1][k])/(grid.dLocalGridOld[grid.nDTheta][0][j][0]
            +grid.dLocalGridOld[grid.nDTheta][0][j-1][0])*2.0;
        }
        double dA2=dV_ijk_np1half/dR_i_n*((1.0-parameters.dDonorFrac)*dA2CenGrad
          +parameters.dDonorFrac*dA2UpWindGrad);
          
        //Calcualte dS2
        double dPj_ijk_n=grid.dLocalGridOld[grid.nP][i][j][k];
        #if VISCOUS_ENERGY_EQ==1
          dPj_ijk_n+=grid.dLocalGridOld[grid.nQ1][i][j][k];
        #endif
        double dS2=dPj_ijk_n/(grid.dLocalGridOld[grid.nD][i][j][k]*dR_i_n
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]*grid.dLocalGridOld[grid.nDTheta][0][j][0])
          *(dVSinTheta_ijp1halfk_np1half-dVSinTheta_ijm1halfk_np1half);
        
        //Calculate dS4
        double dTGrad_ip1half=(dT4_ip1jk_n-dT4_ijk_n)/(grid.dLocalGridOld[grid.nDM][i+1][0][0]
          +grid.dLocalGridOld[grid.nDM][i][0][0])*2.0;
        double dTGrad_im1half=(dT4_ijk_n-dT4_im1jk_n)/(grid.dLocalGridOld[grid.nDM][i][0][0]
          +grid.dLocalGridOld[grid.nDM][i-1][0][0])*2.0;
        double dGrad_ip1half=dRhoAve_ip1half*dR4_ip1half/(dKappa_ip1halfjk_n*dRho_ip1halfjk)
          *dTGrad_ip1half;
        double dGrad_im1half=dRhoAve_im1half*dR_im1half_4/(dKappa_im1halfjk_n*dRho_im1halfjk)
          *dTGrad_im1half;
        double dS4=16.0*parameters.dPi*parameters.dPi*grid.dLocalGridOld[grid.nDenAve][i][0][0]
          *(dGrad_ip1half-dGrad_im1half)/grid.dLocalGridOld[grid.nDM][i][0][0];
        
        //Calculate dS5
        double dTGrad_jp1half=(dT4_ijp1k_n-dT4_ijk_n)/(grid.dLocalGridOld[grid.nDTheta][0][j+1][0]
          +grid.dLocalGridOld[grid.nDTheta][0][j][0])*2.0;
        double dTGrad_jm1half=(dT4_ijk_n-dT4_ijm1k_n)/(grid.dLocalGridOld[grid.nDTheta][0][j][0]
          +grid.dLocalGridOld[grid.nDTheta][0][j-1][0])*2.0;;
        double dGrad_jp1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt][0]
          /(dKappa_ijp1halfk_n*dRho_ijp1halfk*dR_i_n)*dTGrad_jp1half;
        double dGrad_jm1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt-1][0]
          /(dKappa_ijm1halfk_n*dRho_ijm1halfk*dR_i_n)*dTGrad_jm1half;;
        double dS5=(dGrad_jp1half-dGrad_jm1half)/(grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]
          *dR_i_n*grid.dLocalGridOld[grid.nDTheta][0][j][0]);
        
        //calculate new energy
        grid.dLocalGridNew[grid.nE][i][j][k]=grid.dLocalGridOld[grid.nE][i][j][k]
          -time.dDeltat_n*(4.0*parameters.dPi*grid.dLocalGridOld[grid.nDenAve][i][0][0]*(dA1+dS1)
          +dA2+dS2-4.0*parameters.dSigma/(3.0*grid.dLocalGridOld[grid.nD][i][j][k])*(dS4+dS5));
        
        if(grid.dLocalGridNew[grid.nE][i][j][k]<0.0){
          
          #if SIGNEGENG==1
          raise(SIGINT);
          #endif
          
          std::stringstream ssTemp;
          ssTemp<<__FILE__<<":"<<__FUNCTION__<<":"<<__LINE__<<":"<<procTop.nRank
            <<": negative energy calculated in , ("<<i<<","<<j<<","<<k<<")\n";
          throw exception2(ssTemp.str(),CALCULATION);
          
        }
      }
    }
  }
  
  //ghost region 0, outter most ghost region in x1 direction
  for(int i=grid.nStartGhostUpdateExplicit[grid.nE][0][0];i<grid.nEndGhostUpdateExplicit[grid.nE][0][0];i++){
    
    //calculate i for interface centered quantities
    int nIInt=i+grid.nCenIntOffset[0];
    
    for(int j=grid.nStartGhostUpdateExplicit[grid.nE][0][1];j<grid.nEndGhostUpdateExplicit[grid.nE][0][1];j++){
      
      //calculate i for interface centered quantities
      int nJInt=j+grid.nCenIntOffset[1];
      
      for(int k=grid.nStartGhostUpdateExplicit[grid.nE][0][2];k<grid.nEndGhostUpdateExplicit[grid.nE][0][2];k++){
        
        //Calculate interpolated quantities
        double dU_ijk_np1half=(grid.dLocalGridNew[grid.nU][nIInt][j][k]
          +grid.dLocalGridNew[grid.nU][nIInt-1][j][k])*0.5;
        double dU0_i_np1half=(grid.dLocalGridNew[grid.nU0][nIInt][0][0]
          +grid.dLocalGridNew[grid.nU0][nIInt-1][0][0])*0.5;
        double dE_ip1halfjk_n=(grid.dLocalGridOld[grid.nE][i][j][k]);/**\BC Missing
          grid.dLocalGridOld[grid.nE][i+1][j][k] in calculation of \f$E_{i+1/2,j,k}\f$ 
          setting it equal to value at i.*/
        double dE_im1halfjk_n=(grid.dLocalGridOld[grid.nE][i][j][k]
          +grid.dLocalGridOld[grid.nE][i-1][j][k])*0.5;
        double dR_i_n=(grid.dLocalGridOld[grid.nR][nIInt][0][0]
          +grid.dLocalGridOld[grid.nR][nIInt-1][0][0])*0.5;
        double dR_im1half_n=grid.dLocalGridOld[grid.nR][nIInt-1][0][0];
        double dR_ip1half_n=grid.dLocalGridOld[grid.nR][nIInt][0][0];
        double dRSq_i_n=dR_i_n*dR_i_n;
        double dRSq_ip1half=dR_ip1half_n*dR_ip1half_n;
        double dR_im1half_sq=dR_im1half_n*dR_im1half_n;
        double dR_im1half_4=dR_im1half_sq*dR_im1half_sq;
        double dV_ijk_np1half=(grid.dLocalGridNew[grid.nV][i][nJInt][k]
          +grid.dLocalGridNew[grid.nV][i][nJInt-1][k])*0.5;
        double dE_ijp1halfk_n=(grid.dLocalGridOld[grid.nE][i][j+1][k]
          +grid.dLocalGridOld[grid.nE][i][j][k])*0.5;
        double dE_ijm1halfk_n=(grid.dLocalGridOld[grid.nE][i][j][k]
          +grid.dLocalGridOld[grid.nE][i][j-1][k])*0.5;
        double dVSinTheta_ijp1halfk_np1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt][0]
          *grid.dLocalGridNew[grid.nV][i][nJInt][k];
        double dVSinTheta_ijm1halfk_np1half=
          grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt-1][0]
          *grid.dLocalGridNew[grid.nV][i][nJInt-1][k];
        double dRhoAve_im1half=(grid.dLocalGridOld[grid.nDenAve][i][0][0]
          +grid.dLocalGridOld[grid.nDenAve][i-1][0][0])*0.5;
        double dRho_im1halfjk=(grid.dLocalGridOld[grid.nD][i][j][k]
          +grid.dLocalGridOld[grid.nD][i-1][j][k])*0.5;
        double dRho_ijp1halfk=(grid.dLocalGridOld[grid.nD][i][j+1][k]
          +grid.dLocalGridOld[grid.nD][i][j][k])*0.5;
        double dRho_ijm1halfk=(grid.dLocalGridOld[grid.nD][i][j][k]
          +grid.dLocalGridOld[grid.nD][i][j-1][k])*0.5;
        double dTSq_ijk_n=grid.dLocalGridOld[grid.nT][i][j][k]
          *grid.dLocalGridOld[grid.nT][i][j][k];
        double dT4_ijk_n=dTSq_ijk_n*dTSq_ijk_n;
        double dTSq_im1jk_n=grid.dLocalGridOld[grid.nT][i-1][j][k]
          *grid.dLocalGridOld[grid.nT][i-1][j][k];
        double dT4_im1jk_n=dTSq_im1jk_n*dTSq_im1jk_n;
        double dTSq_ijp1k=grid.dLocalGridOld[grid.nT][i][j+1][k]
          *grid.dLocalGridOld[grid.nT][i][j+1][k];
        double dT4_ijp1k=dTSq_ijp1k*dTSq_ijp1k;
        double dTSq_ijm1k=grid.dLocalGridOld[grid.nT][i][j-1][k]
          *grid.dLocalGridOld[grid.nT][i][j-1][k];
        double dT4_ijm1k=dTSq_ijm1k*dTSq_ijm1k;
        double dKappa_im1halfjk_n=(dT4_im1jk_n+dT4_ijk_n)/(dT4_ijk_n/grid.dLocalGridOld[grid.nKappa][i][j][k]+dT4_im1jk_n/grid.dLocalGridOld[grid.nKappa][i-1][j][k]);
        double dKappa_ijp1halfk_n=(dT4_ijp1k+dT4_ijk_n)/(dT4_ijk_n/grid.dLocalGridOld[grid.nKappa][i][j][k]+dT4_ijp1k/grid.dLocalGridOld[grid.nKappa][i][j+1][k]);
        double dKappa_ijm1halfk_n=(dT4_ijm1k+dT4_ijk_n)/(dT4_ijk_n/grid.dLocalGridOld[grid.nKappa][i][j][k]+dT4_ijm1k/grid.dLocalGridOld[grid.nKappa][i][j-1][k]);
        
        //Calcuate dA1
        double dA1CenGrad=(dE_ip1halfjk_n-dE_im1halfjk_n)/grid.dLocalGridOld[grid.nDM][i][0][0];
        double dA1UpWindGrad=0.0;
        double dU_U0_Diff=(dU_ijk_np1half-dU0_i_np1half);
        if(dU_U0_Diff<0.0){//moving in the negative direction
          dA1UpWindGrad=dA1CenGrad;/**\BC grid.dLocalGridOld[grid.nDM][i+1][0][0] and 
            grid.dLocalGridOld[grid.nE][i+1][j][k] missing in the calculation of upwind gradient in 
            dA1. Using the centered gradient instead.*/
        }
        else{//moving in the postive direction*/
          dA1UpWindGrad=(grid.dLocalGridOld[grid.nE][i][j][k]
            -grid.dLocalGridOld[grid.nE][i-1][j][k])/(grid.dLocalGridOld[grid.nDM][i][0][0]
            +grid.dLocalGridOld[grid.nDM][i-1][0][0])*2.0;
        }
        double dA1=dU_U0_Diff*dRSq_i_n*((1.0-parameters.dDonorFrac)*dA1CenGrad
          +parameters.dDonorFrac*dA1UpWindGrad);
        
        //calculate dS1
        double dUR2_im1half_np1half=grid.dLocalGridNew[grid.nU][nIInt-1][j][k]*dR_im1half_n
          *dR_im1half_n;
        double dUR2_ip1half_np1half=grid.dLocalGridNew[grid.nU][nIInt][j][k]*dR_ip1half_n
          *dR_ip1half_n;
        double dP_ijk_n=grid.dLocalGridOld[grid.nP][i][j][k];
        #if VISCOUS_ENERGY_EQ==1
          dP_ijk_n+=grid.dLocalGridOld[grid.nQ0][i][j][k];
        #endif
        double dS1=dP_ijk_n/grid.dLocalGridOld[grid.nD][i][j][k]
          *(dUR2_ip1half_np1half-dUR2_im1half_np1half)/grid.dLocalGridOld[grid.nDM][i][0][0];
        
        //Calcualte dA2
        double dA2CenGrad=(dE_ijp1halfk_n-dE_ijm1halfk_n)/grid.dLocalGridOld[grid.nDTheta][0][j][0];
        double dA2UpWindGrad=0.0;
        if(dV_ijk_np1half<0.0){//moving in the negative direction
          dA2UpWindGrad=(grid.dLocalGridOld[grid.nE][i][j+1][k]
            -grid.dLocalGridOld[grid.nE][i][j][k])/(grid.dLocalGridOld[grid.nDTheta][0][j+1][0]
            +grid.dLocalGridOld[grid.nDTheta][0][j][0])*2.0;
        }
        else{//moving in the positive direction
          dA2UpWindGrad=(grid.dLocalGridOld[grid.nE][i][j][k]
            -grid.dLocalGridOld[grid.nE][i][j-1][k])/(grid.dLocalGridOld[grid.nDTheta][0][j][0]
            +grid.dLocalGridOld[grid.nDTheta][0][j-1][0])*2.0;
        }
        double dA2=dV_ijk_np1half/dR_i_n*((1.0-parameters.dDonorFrac)*dA2CenGrad
          +parameters.dDonorFrac*dA2UpWindGrad);
          
        //Calcualte dS2
        dP_ijk_n=grid.dLocalGridOld[grid.nP][i][j][k];
        #if VISCOUS_ENERGY_EQ==1
          dP_ijk_n+=grid.dLocalGridOld[grid.nQ1][i][j][k];
        #endif
        double dS2=dP_ijk_n/(grid.dLocalGridOld[grid.nD][i][j][k]*dR_i_n
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]*grid.dLocalGridOld[grid.nDTheta][0][j][0])
          *(dVSinTheta_ijp1halfk_np1half-dVSinTheta_ijm1halfk_np1half);
        
        //Calculate dS4
        double dTGrad_im1half=(dT4_ijk_n-dT4_im1jk_n)/(grid.dLocalGridOld[grid.nDM][i][0][0]
          +grid.dLocalGridOld[grid.nDM][i-1][0][0])*2.0;
        double dGrad_ip1half=-3.0*dRSq_ip1half*dT4_ijk_n/(8.0*parameters.dPi);/**\BC
          Missing grid.dLocalGridOld[grid.nT][i+1][0][0] using flux equals \f$2\sigma T^4\f$ at 
          surface.*/
        double dGrad_im1half=dRhoAve_im1half*dR_im1half_4/(dKappa_im1halfjk_n*dRho_im1halfjk)
          *dTGrad_im1half;
        double dS4=16.0*parameters.dPi*parameters.dPi*grid.dLocalGridOld[grid.nDenAve][i][0][0]
          *(dGrad_ip1half-dGrad_im1half)/grid.dLocalGridOld[grid.nDM][i][0][0];
        
        //Calculate dS5
        double dTGrad_jp1half=(dT4_ijp1k-dT4_ijk_n)/(grid.dLocalGridOld[grid.nDTheta][0][j+1][0]
          +grid.dLocalGridOld[grid.nDTheta][0][j][0])*2.0;
        double dTGrad_jm1half=(dT4_ijk_n-dT4_ijm1k)/(grid.dLocalGridOld[grid.nDTheta][0][j][0]
          +grid.dLocalGridOld[grid.nDTheta][0][j-1][0])*2.0;;
        double dGrad_jp1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt][0]
          /(dKappa_ijp1halfk_n*dRho_ijp1halfk*dR_i_n)*dTGrad_jp1half;
        double dGrad_jm1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt-1][0]
          /(dKappa_ijm1halfk_n*dRho_ijm1halfk*dR_i_n)*dTGrad_jm1half;;
        double dS5=(dGrad_jp1half-dGrad_jm1half)/(grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]
          *dR_i_n*grid.dLocalGridOld[grid.nDTheta][0][j][0]);
        
        //calculate new energy
        grid.dLocalGridNew[grid.nE][i][j][k]=grid.dLocalGridOld[grid.nE][i][j][k]
          -time.dDeltat_n*(4.0*parameters.dPi*grid.dLocalGridOld[grid.nDenAve][i][0][0]*(dA1+dS1)
          +dA2+dS2-4.0*parameters.dSigma/(3.0*grid.dLocalGridOld[grid.nD][i][j][k])*(dS4+dS5));
        
        if(grid.dLocalGridNew[grid.nE][i][j][k]<0.0){
          
          #if SIGNEGENG==1
            raise(SIGINT);
          #endif
          
          std::stringstream ssTemp;
          ssTemp<<__FILE__<<":"<<__FUNCTION__<<":"<<__LINE__<<":"<<procTop.nRank
            <<": negative energy calculated in , ("<<i<<","<<j<<","<<k<<")\n";
          throw exception2(ssTemp.str(),CALCULATION);
        }
      }
    }
  }
}
void calNewE_RT_NA_LES(Grid &grid, Parameters &parameters, Time &time, ProcTop &procTop){
  int i;
  int j;
  int k;
  int nIInt;
  int nJInt;
  double dDM_ip1half;
  double dDM_im1half;
  double dDelTheta_jp1half;
  double dDelTheta_jm1half;
  double dU_ijk_np1half;
  double dU_ijp1halfk_np1half;
  double dU_ijm1halfk_np1half;
  double dU0_i_np1half;
  double dV_ijk_np1half;
  double dV_ip1halfjk_np1half;
  double dV_im1halfjk_np1half;
  double dE_ip1halfjk_n;
  double dE_im1halfjk_n;
  double dE_ijp1halfk_n;
  double dE_ijm1halfk_n;
  double dR_ip1_np1half;
  double dRSq_ip1_np1half;
  double dR_i_np1half;
  double dR_im1half_np1half;
  double dR_ip1half_np1half;
  double dRSq_i_np1half;
  double dR_im1_np1half;
  double dRSq_im1_np1half;
  double dRSq_ip1half_np1half;
  double dR4_ip1half_np1half;
  double dRSq_im1half_np1half;
  double dR4_im1half_np1half;
  double dUmU0_ijk_np1half;
  double dVSinTheta_ijp1halfk_np1half;
  double dVSinTheta_ijm1halfk_np1half;
  double dRhoAve_ip1half_n;
  double dRhoAve_im1half_n;
  double dRho_ip1halfjk_n;
  double dRho_im1halfjk_n;
  double dRho_ijp1halfk_n;
  double dRho_ijm1halfk_n;
  double dTSq_ijp1k_n;
  double dTSq_ijm1k_n;
  double dTSq_ip1jk_n;
  double dTSq_ijk_n;
  double dTSq_im1jk_n;
  double dT4_ip1jk_n;
  double dT4_ijk_n;
  double dT4_im1jk_n;
  double dT4_ijp1k_n;
  double dT4_ijm1k_n;
  double dKappa_ip1halfjk_n;
  double dKappa_im1halfjk_n;
  double dKappa_ijp1halfk_n;
  double dKappa_ijm1halfk_n;
  double dUR2_im1halfjk_np1half;
  double dUR2_ip1halfjk_np1half;
  double dA1CenGrad;
  double dA1UpWindGrad;
  double dA2CenGrad;
  double dA2UpWindGrad;
  double dA2;
  double dA1;
  double dP_ijk_n;
  double dS1;
  double dS2;
  double dTGrad_ip1half;
  double dTGrad_im1half;
  double dTGrad_jp1half;
  double dTGrad_jm1half;
  double dGrad_ip1half;
  double dGrad_im1half;
  double dGrad_jp1half;
  double dGrad_jm1half;
  double dS4;
  double dS5;
  double dEddyViscosityTerms;
  /*double dDivU_ijk_np1half;
  double dDUDM_ijk_np1half;
  double dDVDM_ijk_np1half;
  double dDUDTheta_ijk_np1half;
  double dDVDTheta_ijk_np1half;
  double dTau_rr_ijk_np1half;
  double dTau_tt_ijk_np1half;
  double dTau_rt_ijk_np1half;*/
  double dT1;
  double dT2;
  double dEGrad_ip1halfjk_np1half;
  double dEGrad_im1halfjk_np1half;
  double dEGrad_ijp1halfk_np1half;
  double dEGrad_ijm1halfk_np1half;
  double dE_ip1jk_np1half;
  double dE_ijk_np1half;
  double dE_im1jk_np1half;
  double dEddyVisc_ip1halfjk_n;
  double dEddyVisc_im1halfjk_n;
  double dEddyVisc_ijp1halfk_n;
  double dEddyVisc_ijm1halfk_n;
  double dPiSq=parameters.dPi*parameters.dPi;
  for(i=grid.nStartUpdateExplicit[grid.nE][0];i<grid.nEndUpdateExplicit[grid.nE][0];i++){
    
    //calculate i for interface centered quantities
    nIInt=i+grid.nCenIntOffset[0];
    dR_ip1half_np1half=(grid.dLocalGridOld[grid.nR][nIInt][0][0]
      +grid.dLocalGridNew[grid.nR][nIInt][0][0])*0.5;
    dR_im1half_np1half=(grid.dLocalGridOld[grid.nR][nIInt-1][0][0]
      +grid.dLocalGridNew[grid.nR][nIInt-1][0][0])*0.5;
    dR_ip1_np1half=(grid.dLocalGridOld[grid.nR][nIInt+1][0][0]
      +grid.dLocalGridOld[grid.nR][nIInt][0][0]+grid.dLocalGridNew[grid.nR][nIInt+1][0][0]
      +grid.dLocalGridNew[grid.nR][nIInt][0][0])*0.25;
    dRSq_ip1_np1half=dR_ip1_np1half*dR_ip1_np1half;
    dR_im1_np1half=(grid.dLocalGridOld[grid.nR][nIInt-1][0][0]
      +grid.dLocalGridOld[grid.nR][nIInt-2][0][0]+grid.dLocalGridNew[grid.nR][nIInt-1][0][0]
      +grid.dLocalGridNew[grid.nR][nIInt-2][0][0])*0.25;
    dRSq_im1_np1half=dR_im1_np1half*dR_im1_np1half;
    dR_i_np1half=(dR_ip1half_np1half+dR_im1half_np1half)*0.5;
    dRSq_i_np1half=dR_i_np1half*dR_i_np1half;
    dRSq_ip1half_np1half=dR_ip1half_np1half*dR_ip1half_np1half;
    dR4_ip1half_np1half=dRSq_ip1half_np1half*dRSq_ip1half_np1half;
    dRSq_im1half_np1half=dR_im1half_np1half*dR_im1half_np1half;
    dR4_im1half_np1half=dRSq_im1half_np1half*dRSq_im1half_np1half;
    dRhoAve_ip1half_n=(grid.dLocalGridOld[grid.nDenAve][i][0][0]
      +grid.dLocalGridOld[grid.nDenAve][i+1][0][0])*0.5;
    dRhoAve_im1half_n=(grid.dLocalGridOld[grid.nDenAve][i][0][0]
      +grid.dLocalGridOld[grid.nDenAve][i-1][0][0])*0.5;
    dU0_i_np1half=(grid.dLocalGridNew[grid.nU0][nIInt][0][0]
      +grid.dLocalGridNew[grid.nU0][nIInt-1][0][0])*0.5;
    dDM_ip1half=(grid.dLocalGridOld[grid.nDM][i][0][0]+grid.dLocalGridOld[grid.nDM][i+1][0][0])*0.5;
    dDM_im1half=(grid.dLocalGridOld[grid.nDM][i][0][0]+grid.dLocalGridOld[grid.nDM][i-1][0][0])*0.5;
    
    for(j=grid.nStartUpdateExplicit[grid.nE][1];j<grid.nEndUpdateExplicit[grid.nE][1];j++){
      
      //calculate j for interface centered quantities
      nJInt=j+grid.nCenIntOffset[1];
      dDelTheta_jp1half=(grid.dLocalGridOld[grid.nDTheta][0][j][0]
        +grid.dLocalGridOld[grid.nDTheta][0][j+1][0])*0.5;
      dDelTheta_jm1half=(grid.dLocalGridOld[grid.nDTheta][0][j][0]
        +grid.dLocalGridOld[grid.nDTheta][0][j-1][0])*0.5;
      
      for(k=grid.nStartUpdateExplicit[grid.nE][2];k<grid.nEndUpdateExplicit[grid.nE][2];k++){
        
        //Calculate interpolated quantities
        dU_ijk_np1half=(grid.dLocalGridNew[grid.nU][nIInt][j][k]
          +grid.dLocalGridNew[grid.nU][nIInt-1][j][k])*0.5;
        dU_ijp1halfk_np1half=(grid.dLocalGridNew[grid.nU][nIInt][j+1][k]
          +grid.dLocalGridNew[grid.nU][nIInt-1][j+1][k]+grid.dLocalGridNew[grid.nU][nIInt][j][k]
          +grid.dLocalGridNew[grid.nU][nIInt-1][j][k])*0.25;
        dU_ijm1halfk_np1half=(grid.dLocalGridNew[grid.nU][nIInt][j-1][k]
          +grid.dLocalGridNew[grid.nU][nIInt-1][j-1][k]+grid.dLocalGridNew[grid.nU][nIInt][j][k]
          +grid.dLocalGridNew[grid.nU][nIInt-1][j][k])*0.25;
        dV_ijk_np1half=(grid.dLocalGridNew[grid.nV][i][nJInt][k]
          +grid.dLocalGridNew[grid.nV][i][nJInt-1][k])*0.5;
        dV_ip1halfjk_np1half=(grid.dLocalGridNew[grid.nV][i+1][nJInt][k]
          +grid.dLocalGridNew[grid.nV][i+1][nJInt-1][k]+grid.dLocalGridNew[grid.nV][i][nJInt][k]
          +grid.dLocalGridNew[grid.nV][i][nJInt-1][k])*0.25;
        dV_im1halfjk_np1half=(grid.dLocalGridNew[grid.nV][i][nJInt][k]
          +grid.dLocalGridNew[grid.nV][i][nJInt-1][k]+grid.dLocalGridNew[grid.nV][i-1][nJInt][k]
          +grid.dLocalGridNew[grid.nV][i-1][nJInt-1][k])*0.25;
        dE_ip1halfjk_n=(grid.dLocalGridOld[grid.nE][i+1][j][k]
          +grid.dLocalGridOld[grid.nE][i][j][k])*0.5;
        dE_im1halfjk_n=(grid.dLocalGridOld[grid.nE][i][j][k]
          +grid.dLocalGridOld[grid.nE][i-1][j][k])*0.5;
        dE_ijp1halfk_n=(grid.dLocalGridOld[grid.nE][i][j+1][k]
          +grid.dLocalGridOld[grid.nE][i][j][k])*0.5;
        dE_ijm1halfk_n=(grid.dLocalGridOld[grid.nE][i][j][k]
          +grid.dLocalGridOld[grid.nE][i][j-1][k])*0.5;
        dRho_ip1halfjk_n=(grid.dLocalGridOld[grid.nD][i+1][j][k]
          +grid.dLocalGridOld[grid.nD][i][j][k])*0.5;
        dRho_im1halfjk_n=(grid.dLocalGridOld[grid.nD][i][j][k]
          +grid.dLocalGridOld[grid.nD][i-1][j][k])*0.5;
        dRho_ijp1halfk_n=(grid.dLocalGridOld[grid.nD][i][j+1][k]
          +grid.dLocalGridOld[grid.nD][i][j][k])*0.5;
        dRho_ijm1halfk_n=(grid.dLocalGridOld[grid.nD][i][j][k]
          +grid.dLocalGridOld[grid.nD][i][j-1][k])*0.5;
        dEddyVisc_ip1halfjk_n=(grid.dLocalGridOld[grid.nEddyVisc][i+1][j][k]
          +grid.dLocalGridOld[grid.nEddyVisc][i][j][k])*0.5;
        dEddyVisc_im1halfjk_n=(grid.dLocalGridOld[grid.nEddyVisc][i-1][j][k]
          +grid.dLocalGridOld[grid.nEddyVisc][i][j][k])*0.5;
        dEddyVisc_ijp1halfk_n=(grid.dLocalGridOld[grid.nEddyVisc][i][j+1][k]
        +grid.dLocalGridOld[grid.nEddyVisc][i][j][k])*0.5;
        dEddyVisc_ijm1halfk_n=(grid.dLocalGridOld[grid.nEddyVisc][i][j-1][k]
          +grid.dLocalGridOld[grid.nEddyVisc][i][j][k])*0.5;
          
        dTSq_ip1jk_n=grid.dLocalGridOld[grid.nT][i+1][j][k]*grid.dLocalGridOld[grid.nT][i+1][j][k];
        dT4_ip1jk_n=dTSq_ip1jk_n*dTSq_ip1jk_n;
        dTSq_ijk_n=grid.dLocalGridOld[grid.nT][i][j][k]*grid.dLocalGridOld[grid.nT][i][j][k];
        dT4_ijk_n=dTSq_ijk_n*dTSq_ijk_n;
        dTSq_im1jk_n=grid.dLocalGridOld[grid.nT][i-1][j][k]*grid.dLocalGridOld[grid.nT][i-1][j][k];
        dT4_im1jk_n=dTSq_im1jk_n*dTSq_im1jk_n;
        dTSq_ijp1k_n=grid.dLocalGridOld[grid.nT][i][j+1][k]*grid.dLocalGridOld[grid.nT][i][j+1][k];
        dT4_ijp1k_n=dTSq_ijp1k_n*dTSq_ijp1k_n;
        dTSq_ijm1k_n=grid.dLocalGridOld[grid.nT][i][j-1][k]*grid.dLocalGridOld[grid.nT][i][j-1][k];
        dT4_ijm1k_n=dTSq_ijm1k_n*dTSq_ijm1k_n;
        dKappa_ip1halfjk_n=(dT4_ip1jk_n+dT4_ijk_n)/(dT4_ijk_n
          /grid.dLocalGridOld[grid.nKappa][i][j][k]+dT4_ip1jk_n
          /grid.dLocalGridOld[grid.nKappa][i+1][j][k]);
        dKappa_im1halfjk_n=(dT4_im1jk_n+dT4_ijk_n)/(dT4_ijk_n
          /grid.dLocalGridOld[grid.nKappa][i][j][k]+dT4_im1jk_n
          /grid.dLocalGridOld[grid.nKappa][i-1][j][k]);
        dKappa_ijp1halfk_n=(dT4_ijp1k_n+dT4_ijk_n)/(dT4_ijk_n
          /grid.dLocalGridOld[grid.nKappa][i][j][k]+dT4_ijp1k_n
          /grid.dLocalGridOld[grid.nKappa][i][j+1][k]);
        dKappa_ijm1halfk_n=(dT4_ijm1k_n+dT4_ijk_n)/(dT4_ijk_n
          /grid.dLocalGridOld[grid.nKappa][i][j][k]+dT4_ijm1k_n
          /grid.dLocalGridOld[grid.nKappa][i][j-1][k]);
        dP_ijk_n=grid.dLocalGridOld[grid.nP][i][j][k];
        #if VISCOUS_ENERGY_EQ==1
          dP_ijk_n=dP_ijk_n+grid.dLocalGridOld[grid.nQ0][i][j][k]
            +grid.dLocalGridOld[grid.nQ1][i][j][k];
        #endif
        
        //calculate derived quantities
        dVSinTheta_ijp1halfk_np1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt][0]
          *grid.dLocalGridNew[grid.nV][i][nJInt][k];
        dVSinTheta_ijm1halfk_np1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt-1][0]
          *grid.dLocalGridNew[grid.nV][i][nJInt-1][k];
        dUR2_im1halfjk_np1half=grid.dLocalGridNew[grid.nU][nIInt-1][j][k]*dRSq_im1half_np1half;
        dUR2_ip1halfjk_np1half=grid.dLocalGridNew[grid.nU][nIInt][j][k]*dRSq_ip1half_np1half;
        
        //Calcuate dA1
        dA1CenGrad=(dE_ip1halfjk_n-dE_im1halfjk_n)/grid.dLocalGridOld[grid.nDM][i][0][0];
        dA1UpWindGrad=0.0;
        dUmU0_ijk_np1half=(dU_ijk_np1half-dU0_i_np1half);
        if(dUmU0_ijk_np1half<0.0){//moving in the negative direction
          dA1UpWindGrad=(grid.dLocalGridOld[grid.nE][i+1][j][k]
            -grid.dLocalGridOld[grid.nE][i][j][k])/(grid.dLocalGridOld[grid.nDM][i+1][0][0]
            +grid.dLocalGridOld[grid.nDM][i][0][0])*2.0;
        }
        else{//moving in the postive direction
          dA1UpWindGrad=(grid.dLocalGridOld[grid.nE][i][j][k]
            -grid.dLocalGridOld[grid.nE][i-1][j][k])/(grid.dLocalGridOld[grid.nDM][i][0][0]
            +grid.dLocalGridOld[grid.nDM][i-1][0][0])*2.0;
        }
        dA1=dUmU0_ijk_np1half*dRSq_i_np1half*((1.0-parameters.dDonorFrac)*dA1CenGrad
          +parameters.dDonorFrac*dA1UpWindGrad);
        
        //calculate dS1
        dS1=dP_ijk_n/grid.dLocalGridOld[grid.nD][i][j][k]
          *(dUR2_ip1halfjk_np1half-dUR2_im1halfjk_np1half)/grid.dLocalGridOld[grid.nDM][i][0][0];
        
        //Calcualte dA2
        dA2CenGrad=(dE_ijp1halfk_n-dE_ijm1halfk_n)/grid.dLocalGridOld[grid.nDTheta][0][j][0];
        dA2UpWindGrad=0.0;
        if(dV_ijk_np1half<0.0){//moving in the negative direction
          dA2UpWindGrad=(grid.dLocalGridOld[grid.nE][i][j+1][k]
            -grid.dLocalGridOld[grid.nE][i][j][k])/(grid.dLocalGridOld[grid.nDTheta][0][j+1][0]
            +grid.dLocalGridOld[grid.nDTheta][0][j][0])*2.0;
        }
        else{//moving in the positive direction
          dA2UpWindGrad=(grid.dLocalGridOld[grid.nE][i][j][k]
            -grid.dLocalGridOld[grid.nE][i][j-1][k])/(grid.dLocalGridOld[grid.nDTheta][0][j][0]
            +grid.dLocalGridOld[grid.nDTheta][0][j-1][0])*2.0;
        }
        dA2=dV_ijk_np1half/dR_i_np1half*((1.0-parameters.dDonorFrac)*dA2CenGrad
          +parameters.dDonorFrac*dA2UpWindGrad);
          
        //Calcualte dS2
        dS2=dP_ijk_n/(grid.dLocalGridOld[grid.nD][i][j][k]*dR_i_np1half
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]*grid.dLocalGridOld[grid.nDTheta][0][j][0])
          *(dVSinTheta_ijp1halfk_np1half-dVSinTheta_ijm1halfk_np1half);
        
        //Calculate dS4
        dTGrad_ip1half=(dT4_ip1jk_n-dT4_ijk_n)/(grid.dLocalGridOld[grid.nDM][i+1][0][0]
          +grid.dLocalGridOld[grid.nDM][i][0][0])*2.0;
        dTGrad_im1half=(dT4_ijk_n-dT4_im1jk_n)/(grid.dLocalGridOld[grid.nDM][i][0][0]
          +grid.dLocalGridOld[grid.nDM][i-1][0][0])*2.0;
        dGrad_ip1half=dRhoAve_ip1half_n*dR4_ip1half_np1half/(dKappa_ip1halfjk_n
          *dRho_ip1halfjk_n)*dTGrad_ip1half;
        dGrad_im1half=dRhoAve_im1half_n*dR4_im1half_np1half/(dKappa_im1halfjk_n
          *dRho_im1halfjk_n)*dTGrad_im1half;
        dS4=16.0*dPiSq*grid.dLocalGridOld[grid.nDenAve][i][0][0]
          *(dGrad_ip1half-dGrad_im1half)/grid.dLocalGridOld[grid.nDM][i][0][0];
        
        //Calculate dS5
        dTGrad_jp1half=(dT4_ijp1k_n-dT4_ijk_n)/(grid.dLocalGridOld[grid.nDTheta][0][j+1][0]
          +grid.dLocalGridOld[grid.nDTheta][0][j][0])*2.0;
        dTGrad_jm1half=(dT4_ijk_n-dT4_ijm1k_n)/(grid.dLocalGridOld[grid.nDTheta][0][j][0]
          +grid.dLocalGridOld[grid.nDTheta][0][j-1][0])*2.0;;
        dGrad_jp1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt][0]
          /(dKappa_ijp1halfk_n*dRho_ijp1halfk_n)*dTGrad_jp1half;
        dGrad_jm1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt-1][0]
          /(dKappa_ijm1halfk_n*dRho_ijm1halfk_n)*dTGrad_jm1half;
        dS5=(dGrad_jp1half-dGrad_jm1half)/(grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]
          *dRSq_i_np1half*grid.dLocalGridOld[grid.nDTheta][0][j][0]);
        
        //calculate DUDM_ijk_np1half
        /*dDUDM_ijk_np1half=4.0*parameters.dPi*grid.dLocalGridOld[grid.nDenAve][i][0][0]
          *dRSq_i_np1half*((grid.dLocalGridNew[grid.nU][nIInt][j][k]
          -grid.dLocalGridNew[grid.nU0][nIInt][0][0])-(grid.dLocalGridNew[grid.nU][nIInt-1][j][k]
          -grid.dLocalGridNew[grid.nU0][nIInt-1][0][0]))/grid.dLocalGridOld[grid.nDM][i][0][0];
        
        //calculate DVDM_ijk_np1half
        dDVDM_ijk_np1half=4.0*parameters.dPi*grid.dLocalGridOld[grid.nDenAve][i][0][0]
          *dRSq_i_np1half*(dV_ip1halfjk_np1half-dV_im1halfjk_np1half)
          /grid.dLocalGridOld[grid.nDM][i][0][0];
        
        //calculate DUDTheta_ijk_np1half
        dDUDTheta_ijk_np1half=((dU_ijp1halfk_np1half-dU0_i_np1half)-(dU_ijm1halfk_np1half
          -dU0_i_np1half))/(dR_i_np1half*grid.dLocalGridOld[grid.nDTheta][0][j][0]);
        
        //calculate DVDTheta_ijk_np1half
        dDVDTheta_ijk_np1half=(grid.dLocalGridNew[grid.nV][i][nJInt][k]
          -grid.dLocalGridNew[grid.nV][i][nJInt-1][k])/(dR_i_np1half
          *grid.dLocalGridOld[grid.nDTheta][0][j][0]);
        
        //cal DivU_ijk_np1half
        dDivU_ijk_np1half=dDUDM_ijk_np1half+dDVDTheta_ijk_np1half;
        
        //cal Tau_rr_ijk_np1half
        dTau_rr_ijk_np1half=2.0*grid.dLocalGridNew[grid.nEddyVisc][i][j][k]*(dDUDM_ijk_np1half
          -0.3333333333333333*dDivU_ijk_np1half);
        
        //calculate Tau_tt_ijp1halfk_np1half
        dTau_tt_ijk_np1half=2.0*grid.dLocalGridNew[grid.nEddyVisc][i][j][k]*(dDVDTheta_ijk_np1half
          -0.333333333333333*dDivU_ijk_np1half);
        
        //calculate Tau_rt_ijk_np1half
        dTau_rt_ijk_np1half=grid.dLocalGridNew[grid.nEddyVisc][i][j][k]*(dDVDM_ijk_np1half
          +dDUDTheta_ijk_np1half);
        
        //eddy viscosity terms
        dEddyViscosityTerms=(dTau_rr_ijk_np1half*dDUDM_ijk_np1half+dTau_tt_ijk_np1half
          *dDVDTheta_ijk_np1half+dTau_rt_ijk_np1half*(dDUDTheta_ijk_np1half+dDVDM_ijk_np1half))
          /grid.dLocalGridOld[grid.nD][i][j][k];*/
        
        //calculate dT1
        dEGrad_ip1halfjk_np1half=dR4_ip1half_np1half*dEddyVisc_ip1halfjk_n*dRhoAve_ip1half_n
          *(grid.dLocalGridOld[grid.nE][i+1][j][k]-grid.dLocalGridOld[grid.nE][i][j][k])
          /(dRho_ip1halfjk_n*dDM_ip1half);
        dEGrad_im1halfjk_np1half=dR4_im1half_np1half*dEddyVisc_im1halfjk_n*dRhoAve_im1half_n
          *(grid.dLocalGridOld[grid.nE][i][j][k]-grid.dLocalGridOld[grid.nE][i-1][j][k])
          /(dRho_im1halfjk_n*dDM_im1half);
        dT1=16.0*dPiSq*grid.dLocalGridOld[grid.nDenAve][i][0][0]*(dEGrad_ip1halfjk_np1half
          -dEGrad_im1halfjk_np1half)/grid.dLocalGridOld[grid.nDM][i][0][0];
        
        //calculate dT2
        dEGrad_ijp1halfk_np1half=dEddyVisc_ijp1halfk_n
          *grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt][0]
          *(grid.dLocalGridOld[grid.nE][i][j+1][k]-grid.dLocalGridOld[grid.nE][i][j][k])
          /(dRho_ijp1halfk_n*dR_i_np1half*dDelTheta_jp1half);
        dEGrad_ijm1halfk_np1half=dEddyVisc_ijm1halfk_n
          *grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt-1][0]
          *(grid.dLocalGridOld[grid.nE][i][j][k]-grid.dLocalGridOld[grid.nE][i][j-1][k])
          /(dRho_ijm1halfk_n*dR_i_np1half*dDelTheta_jm1half);
        dT2=(dEGrad_ijp1halfk_np1half-dEGrad_ijm1halfk_np1half)/(dR_i_np1half
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]
          *grid.dLocalGridOld[grid.nDTheta][0][j][0]);
        
        //eddy viscosity terms
        dEddyViscosityTerms=(dT1+dT2)/parameters.dPrt;
        
        //calculate new energy
        grid.dLocalGridNew[grid.nE][i][j][k]=grid.dLocalGridOld[grid.nE][i][j][k]-time.dDeltat_n
          *(4.0*parameters.dPi*grid.dLocalGridOld[grid.nDenAve][i][0][0]*(dA1+dS1)+dA2+dS2
          -4.0*parameters.dSigma/(3.0*grid.dLocalGridOld[grid.nD][i][j][k])*(dS4+dS5)
          -dEddyViscosityTerms);
        
        if(grid.dLocalGridNew[grid.nE][i][j][k]<0.0){
          
          #if SIGNEGENG==1
          raise(SIGINT);
          #endif
          
          std::stringstream ssTemp;
          ssTemp<<__FILE__<<":"<<__FUNCTION__<<":"<<__LINE__<<":"<<procTop.nRank
            <<": negative energy calculated in , ("<<i<<","<<j<<","<<k<<")\n";
          throw exception2(ssTemp.str(),CALCULATION);
          
        }
      }
    }
  }
  
  //ghost region 0, outter most ghost region in x1 direction
  for(i=grid.nStartGhostUpdateExplicit[grid.nE][0][0];i<grid.nEndGhostUpdateExplicit[grid.nE][0][0]
    ;i++){
    
    //calculate i for interface centered quantities
    nIInt=i+grid.nCenIntOffset[0];
    dR_ip1half_np1half=(grid.dLocalGridOld[grid.nR][nIInt][0][0]
      +grid.dLocalGridNew[grid.nR][nIInt][0][0])*0.5;
    dR_im1half_np1half=(grid.dLocalGridOld[grid.nR][nIInt-1][0][0]
      +grid.dLocalGridNew[grid.nR][nIInt-1][0][0])*0.5;
    dR_ip1_np1half=dR_ip1half_np1half;
    dRSq_ip1_np1half=dR_ip1_np1half*dR_ip1_np1half;
    dR_im1_np1half=(grid.dLocalGridOld[grid.nR][nIInt-1][0][0]
      +grid.dLocalGridOld[grid.nR][nIInt-2][0][0]+grid.dLocalGridNew[grid.nR][nIInt-1][0][0]
      +grid.dLocalGridNew[grid.nR][nIInt-2][0][0])*0.25;
    dRSq_im1_np1half=dR_im1_np1half*dR_im1_np1half;
    dR_i_np1half=(dR_ip1half_np1half+dR_im1half_np1half)*0.5;
    dRSq_i_np1half=dR_i_np1half*dR_i_np1half;
    dRSq_ip1half_np1half=dR_ip1half_np1half*dR_ip1half_np1half;
    dR4_ip1half_np1half=dRSq_ip1half_np1half*dRSq_ip1half_np1half;
    dRSq_im1half_np1half=dR_im1half_np1half*dR_im1half_np1half;
    dR4_im1half_np1half=dRSq_im1half_np1half*dRSq_im1half_np1half;
    dRhoAve_ip1half_n=grid.dLocalGridOld[grid.nDenAve][i][0][0]*0.5;
    dRhoAve_im1half_n=(grid.dLocalGridOld[grid.nDenAve][i][0][0]
      +grid.dLocalGridOld[grid.nDenAve][i-1][0][0])*0.5;
    dU0_i_np1half=(grid.dLocalGridNew[grid.nU0][nIInt][0][0]
      +grid.dLocalGridNew[grid.nU0][nIInt-1][0][0])*0.5;
    dDM_ip1half=(grid.dLocalGridOld[grid.nDM][i][0][0])*(0.5+parameters.dAlpha
      +parameters.dAlphaExtra);/**\BC Missing \f$\Delta M_r\f$ outside model using 
      \ref Parameters.dAlpha times \f$\Delta M_r\f$ in the last zone instead.*/
    dDM_im1half=(grid.dLocalGridOld[grid.nDM][i][0][0]+grid.dLocalGridOld[grid.nDM][i-1][0][0])*0.5;
    for(j=grid.nStartGhostUpdateExplicit[grid.nE][0][1];
      j<grid.nEndGhostUpdateExplicit[grid.nE][0][1];j++){
      
      //calculate i for interface centered quantities
      nJInt=j+grid.nCenIntOffset[1];
      dDelTheta_jp1half=(grid.dLocalGridOld[grid.nDTheta][0][j][0]
        +grid.dLocalGridOld[grid.nDTheta][0][j+1][0])*0.5;
      dDelTheta_jm1half=(grid.dLocalGridOld[grid.nDTheta][0][j][0]
        +grid.dLocalGridOld[grid.nDTheta][0][j-1][0])*0.5;
      
      for(k=grid.nStartGhostUpdateExplicit[grid.nE][0][2];
        k<grid.nEndGhostUpdateExplicit[grid.nE][0][2];k++){
        
        //Calculate interpolated quantities
        dU_ijk_np1half=(grid.dLocalGridNew[grid.nU][nIInt][j][k]
          +grid.dLocalGridNew[grid.nU][nIInt-1][j][k])*0.5;
        dU_ijp1halfk_np1half=(grid.dLocalGridNew[grid.nU][nIInt][j+1][k]
          +grid.dLocalGridNew[grid.nU][nIInt-1][j+1][k]+grid.dLocalGridNew[grid.nU][nIInt][j][k]
          +grid.dLocalGridNew[grid.nU][nIInt-1][j][k])*0.25;
        dU_ijm1halfk_np1half=(grid.dLocalGridNew[grid.nU][nIInt][j-1][k]
          +grid.dLocalGridNew[grid.nU][nIInt-1][j-1][k]+grid.dLocalGridNew[grid.nU][nIInt][j][k]
          +grid.dLocalGridNew[grid.nU][nIInt-1][j][k])*0.25;
        dV_ijk_np1half=(grid.dLocalGridNew[grid.nV][i][nJInt][k]
          +grid.dLocalGridNew[grid.nV][i][nJInt-1][k])*0.5;
        dV_ip1halfjk_np1half=dV_ijk_np1half;
        dV_im1halfjk_np1half=(grid.dLocalGridNew[grid.nV][i][nJInt][k]
          +grid.dLocalGridNew[grid.nV][i][nJInt-1][k]+grid.dLocalGridNew[grid.nV][i-1][nJInt][k]
          +grid.dLocalGridNew[grid.nV][i-1][nJInt-1][k])*0.25;
        dE_ip1halfjk_n=grid.dLocalGridOld[grid.nE][i][j][k];/**\BC Setting energy at surface equal 
          to energy in last zone.*/
        dE_im1halfjk_n=(grid.dLocalGridOld[grid.nE][i][j][k]+grid.dLocalGridOld[grid.nE][i-1][j][k])
          *0.5;
        dE_ijp1halfk_n=(grid.dLocalGridOld[grid.nE][i][j+1][k]+grid.dLocalGridOld[grid.nE][i][j][k])
          *0.5;
        dE_ijm1halfk_n=(grid.dLocalGridOld[grid.nE][i][j][k]+grid.dLocalGridOld[grid.nE][i][j-1][k])
          *0.5;
        dRho_im1halfjk_n=(grid.dLocalGridOld[grid.nD][i][j][k]
          +grid.dLocalGridOld[grid.nD][i-1][j][k])*0.5;
        dRho_ijp1halfk_n=(grid.dLocalGridOld[grid.nD][i][j+1][k]
          +grid.dLocalGridOld[grid.nD][i][j][k])*0.5;
        dRho_ijm1halfk_n=(grid.dLocalGridOld[grid.nD][i][j][k]
          +grid.dLocalGridOld[grid.nD][i][j-1][k])*0.5;
        dEddyVisc_ip1halfjk_n=(grid.dLocalGridOld[grid.nEddyVisc][i][j][k])*0.5;/**\BC missing 
          eddy viscosity outside the model setting it to zero*/
        dEddyVisc_im1halfjk_n=(grid.dLocalGridOld[grid.nEddyVisc][i-1][j][k]
          +grid.dLocalGridOld[grid.nEddyVisc][i][j][k])*0.5;
        dEddyVisc_ijp1halfk_n=(grid.dLocalGridOld[grid.nEddyVisc][i][j+1][k]
        +grid.dLocalGridOld[grid.nEddyVisc][i][j][k])*0.5;
        dEddyVisc_ijm1halfk_n=(grid.dLocalGridOld[grid.nEddyVisc][i][j-1][k]
          +grid.dLocalGridOld[grid.nEddyVisc][i][j][k])*0.5;
        dTSq_ijk_n=grid.dLocalGridOld[grid.nT][i][j][k]*grid.dLocalGridOld[grid.nT][i][j][k];
        dT4_ijk_n=dTSq_ijk_n*dTSq_ijk_n;
        dTSq_im1jk_n=grid.dLocalGridOld[grid.nT][i-1][j][k]*grid.dLocalGridOld[grid.nT][i-1][j][k];
        dT4_im1jk_n=dTSq_im1jk_n*dTSq_im1jk_n;
        dTSq_ijp1k_n=grid.dLocalGridOld[grid.nT][i][j+1][k]*grid.dLocalGridOld[grid.nT][i][j+1][k];
        dT4_ijp1k_n=dTSq_ijp1k_n*dTSq_ijp1k_n;
        dTSq_ijm1k_n=grid.dLocalGridOld[grid.nT][i][j-1][k]*grid.dLocalGridOld[grid.nT][i][j-1][k];
        dT4_ijm1k_n=dTSq_ijm1k_n*dTSq_ijm1k_n;
        dKappa_im1halfjk_n=(dT4_im1jk_n+dT4_ijk_n)/(dT4_ijk_n
          /grid.dLocalGridOld[grid.nKappa][i][j][k]+dT4_im1jk_n
          /grid.dLocalGridOld[grid.nKappa][i-1][j][k]);
        dKappa_ijp1halfk_n=(dT4_ijp1k_n+dT4_ijk_n)/(dT4_ijk_n
          /grid.dLocalGridOld[grid.nKappa][i][j][k]+dT4_ijp1k_n
          /grid.dLocalGridOld[grid.nKappa][i][j+1][k]);
        dKappa_ijm1halfk_n=(dT4_ijm1k_n+dT4_ijk_n)/(dT4_ijk_n
          /grid.dLocalGridOld[grid.nKappa][i][j][k]+dT4_ijm1k_n
          /grid.dLocalGridOld[grid.nKappa][i][j-1][k]);
        dP_ijk_n=grid.dLocalGridOld[grid.nP][i][j][k];
        #if VISCOUS_ENERGY_EQ==1
          dP_ijk_n+=grid.dLocalGridOld[grid.nQ0][i][j][k];
          dP_ijk_n+=grid.dLocalGridOld[grid.nQ1][i][j][k];
        #endif
        
        //calculate derived quantities
        dVSinTheta_ijp1halfk_np1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt][0]
          *grid.dLocalGridNew[grid.nV][i][nJInt][k];
        dVSinTheta_ijm1halfk_np1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt-1][0]
          *grid.dLocalGridNew[grid.nV][i][nJInt-1][k];
        dUR2_im1halfjk_np1half=grid.dLocalGridNew[grid.nU][nIInt-1][j][k]*dRSq_im1half_np1half;
        dUR2_ip1halfjk_np1half=grid.dLocalGridNew[grid.nU][nIInt][j][k]*dRSq_ip1half_np1half;
        
        //Calcuate dA1
        dA1CenGrad=(dE_ip1halfjk_n-dE_im1halfjk_n)/grid.dLocalGridOld[grid.nDM][i][0][0];
        dA1UpWindGrad=0.0;
        dUmU0_ijk_np1half=(dU_ijk_np1half-dU0_i_np1half);
        if(dUmU0_ijk_np1half<0.0){//moving in the negative direction
          dA1UpWindGrad=dA1CenGrad;
        }
        else{//moving in the postive direction
          dA1UpWindGrad=(grid.dLocalGridOld[grid.nE][i][j][k]
            -grid.dLocalGridOld[grid.nE][i-1][j][k])/(grid.dLocalGridOld[grid.nDM][i][0][0]
            +grid.dLocalGridOld[grid.nDM][i-1][0][0])*2.0;
        }
        dA1=dUmU0_ijk_np1half*dRSq_i_np1half*((1.0-parameters.dDonorFrac)*dA1CenGrad
          +parameters.dDonorFrac*dA1UpWindGrad);
        
        //calculate dS1
        dS1=dP_ijk_n/grid.dLocalGridOld[grid.nD][i][j][k]
          *(dUR2_ip1halfjk_np1half-dUR2_im1halfjk_np1half)/grid.dLocalGridOld[grid.nDM][i][0][0];
        
        //Calcualte dA2
        dA2CenGrad=(dE_ijp1halfk_n-dE_ijm1halfk_n)/grid.dLocalGridOld[grid.nDTheta][0][j][0];
        dA2UpWindGrad=0.0;
        if(dV_ijk_np1half<0.0){//moving in the negative direction
          dA2UpWindGrad=(grid.dLocalGridOld[grid.nE][i][j+1][k]
            -grid.dLocalGridOld[grid.nE][i][j][k])/(grid.dLocalGridOld[grid.nDTheta][0][j+1][0]
            +grid.dLocalGridOld[grid.nDTheta][0][j][0])*2.0;
        }
        else{//moving in the positive direction
          dA2UpWindGrad=(grid.dLocalGridOld[grid.nE][i][j][k]
            -grid.dLocalGridOld[grid.nE][i][j-1][k])/(grid.dLocalGridOld[grid.nDTheta][0][j][0]
            +grid.dLocalGridOld[grid.nDTheta][0][j-1][0])*2.0;
        }
        dA2=dV_ijk_np1half/dR_i_np1half*((1.0-parameters.dDonorFrac)*dA2CenGrad
          +parameters.dDonorFrac*dA2UpWindGrad);
          
        //Calcualte dS2
        dS2=dP_ijk_n/(grid.dLocalGridOld[grid.nD][i][j][k]*dR_i_np1half
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]*grid.dLocalGridOld[grid.nDTheta][0][j][0])
          *(dVSinTheta_ijp1halfk_np1half-dVSinTheta_ijm1halfk_np1half);
        
        //Calculate dS4
        dTGrad_im1half=(dT4_ijk_n-dT4_im1jk_n)/(grid.dLocalGridOld[grid.nDM][i][0][0]
          +grid.dLocalGridOld[grid.nDM][i-1][0][0])*2.0;
        dGrad_ip1half=-3.0*dRSq_ip1half_np1half*dT4_ijk_n/(8.0*parameters.dPi);/**\BC
          Missing grid.dLocalGridOld[grid.nT][i+1][0][0] using flux equals \f$2\sigma T^4\f$ at 
          surface.*/
        dGrad_im1half=dRhoAve_im1half_n*dR4_im1half_np1half/(dKappa_im1halfjk_n
          *dRho_im1halfjk_n)*dTGrad_im1half;
        dS4=16.0*parameters.dPi*parameters.dPi*grid.dLocalGridOld[grid.nDenAve][i][0][0]
          *(dGrad_ip1half-dGrad_im1half)/grid.dLocalGridOld[grid.nDM][i][0][0];
        
        //Calculate dS5
        dTGrad_jp1half=(dT4_ijp1k_n-dT4_ijk_n)/(grid.dLocalGridOld[grid.nDTheta][0][j+1][0]
          +grid.dLocalGridOld[grid.nDTheta][0][j][0])*2.0;
        dTGrad_jm1half=(dT4_ijk_n-dT4_ijm1k_n)/(grid.dLocalGridOld[grid.nDTheta][0][j][0]
          +grid.dLocalGridOld[grid.nDTheta][0][j-1][0])*2.0;;
        dGrad_jp1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt][0]
          /(dKappa_ijp1halfk_n*dRho_ijp1halfk_n)*dTGrad_jp1half;
        dGrad_jm1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt-1][0]
          /(dKappa_ijm1halfk_n*dRho_ijm1halfk_n)*dTGrad_jm1half;
        dS5=(dGrad_jp1half-dGrad_jm1half)/(grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]
          *dRSq_i_np1half*grid.dLocalGridOld[grid.nDTheta][0][j][0]);
        
        /*
        //calculate DUDM_ijk_np1half
        dDUDM_ijk_np1half=4.0*parameters.dPi*grid.dLocalGridOld[grid.nDenAve][i][0][0]
          *dRSq_i_np1half*((grid.dLocalGridNew[grid.nU][nIInt][j][k]
          -grid.dLocalGridNew[grid.nU0][nIInt][0][0])-(grid.dLocalGridNew[grid.nU][nIInt-1][j][k]
          -grid.dLocalGridNew[grid.nU0][nIInt-1][0][0]))/grid.dLocalGridOld[grid.nDM][i][0][0];
        
        //calculate DVDM_ijk_np1half
        dDVDM_ijk_np1half=4.0*parameters.dPi*grid.dLocalGridOld[grid.nDenAve][i][0][0]
          *dRSq_i_np1half*(dV_ip1halfjk_np1half-dV_im1halfjk_np1half)
          /grid.dLocalGridOld[grid.nDM][i][0][0];
        
        //calculate DUDTheta_ijk_np1half
        dDUDTheta_ijk_np1half=((dU_ijp1halfk_np1half-dU0_i_np1half)-(dU_ijm1halfk_np1half
          -dU0_i_np1half))/(dR_i_np1half*grid.dLocalGridOld[grid.nDTheta][0][j][0]);
        
        //calculate DVDTheta_ijk_np1half
        dDVDTheta_ijk_np1half=(grid.dLocalGridNew[grid.nV][i][nJInt][k]
          -grid.dLocalGridNew[grid.nV][i][nJInt-1][k])/(dR_i_np1half
          *grid.dLocalGridOld[grid.nDTheta][0][j][0]);
        
        //cal DivU_ijk_np1half
        dDivU_ijk_np1half=dDUDM_ijk_np1half+dDVDTheta_ijk_np1half;
        
        //cal Tau_rr_ijk_np1half
        dTau_rr_ijk_np1half=2.0*grid.dLocalGridNew[grid.nEddyVisc][i][j][k]*(dDUDM_ijk_np1half
          -0.3333333333333333*dDivU_ijk_np1half);
        
        //calculate Tau_tt_ijp1halfk_np1half
        dTau_tt_ijk_np1half=2.0*grid.dLocalGridNew[grid.nEddyVisc][i][j][k]*(dDVDTheta_ijk_np1half
          -0.333333333333333*dDivU_ijk_np1half);
        
        dTau_rt_ijk_np1half=grid.dLocalGridNew[grid.nEddyVisc][i][j][k]*(dDVDM_ijk_np1half
          +dDUDTheta_ijk_np1half);
        
        //eddy viscosity terms
        dEddyViscosityTerms=(dTau_rr_ijk_np1half*dDUDM_ijk_np1half+dTau_tt_ijk_np1half
          *dDVDTheta_ijk_np1half+dTau_rt_ijk_np1half*(dDUDTheta_ijk_np1half+dDVDM_ijk_np1half))
          /grid.dLocalGridOld[grid.nD][i][j][k];*/
        
        //calculate dT1
        dEGrad_ip1halfjk_np1half=0.0;/**\BC missing energy outside the model, assuming it is the 
          same as that in the last zone. That causes this term to be zero.*/
        dEGrad_im1halfjk_np1half=dR4_im1half_np1half*dEddyVisc_im1halfjk_n*dRhoAve_im1half_n
          *(grid.dLocalGridOld[grid.nE][i][j][k]-grid.dLocalGridOld[grid.nE][i-1][j][k])
          /(dRho_im1halfjk_n*dDM_im1half);
        dT1=16.0*dPiSq*grid.dLocalGridOld[grid.nDenAve][i][0][0]*(dEGrad_ip1halfjk_np1half
          -dEGrad_im1halfjk_np1half)/grid.dLocalGridOld[grid.nDM][i][0][0];
        
        //calculate dT2
        dEGrad_ijp1halfk_np1half=dEddyVisc_ijp1halfk_n
          *grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt][0]
          *(grid.dLocalGridOld[grid.nE][i][j+1][k]-grid.dLocalGridOld[grid.nE][i][j][k])
          /(dRho_ijp1halfk_n*dR_i_np1half*dDelTheta_jp1half);
        dEGrad_ijm1halfk_np1half=dEddyVisc_ijm1halfk_n
          *grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt-1][0]
          *(grid.dLocalGridOld[grid.nE][i][j][k]-grid.dLocalGridOld[grid.nE][i][j-1][k])
          /(dRho_ijm1halfk_n*dR_i_np1half*dDelTheta_jm1half);
        dT2=(dEGrad_ijp1halfk_np1half-dEGrad_ijm1halfk_np1half)/(dR_i_np1half
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]
          *grid.dLocalGridOld[grid.nDTheta][0][j][0]);
        
        //eddy viscosity terms
        dEddyViscosityTerms=(dT1+dT2)/parameters.dPrt;
        
        //calculate new energy
        grid.dLocalGridNew[grid.nE][i][j][k]=grid.dLocalGridOld[grid.nE][i][j][k]-time.dDeltat_n
          *(4.0*parameters.dPi*grid.dLocalGridOld[grid.nDenAve][i][0][0]*(dA1+dS1)+dA2+dS2
          -4.0*parameters.dSigma/(3.0*grid.dLocalGridOld[grid.nD][i][j][k])*(dS4+dS5)
          -dEddyViscosityTerms);
        
        if(grid.dLocalGridNew[grid.nE][i][j][k]<0.0){
          
          #if SIGNEGENG==1
          raise(SIGINT);
          #endif
          
          std::stringstream ssTemp;
          ssTemp<<__FILE__<<":"<<__FUNCTION__<<":"<<__LINE__<<":"<<procTop.nRank
            <<": negative energy calculated in , ("<<i<<","<<j<<","<<k<<")\n";
          throw exception2(ssTemp.str(),CALCULATION);
          
        }
      }
    }
  }
}
void calNewE_RTP_AD(Grid &grid, Parameters &parameters, Time &time, ProcTop &procTop){
  for(int i=grid.nStartUpdateExplicit[grid.nE][0];i<grid.nEndUpdateExplicit[grid.nE][0];i++){
    
    //calculate i for interface centered quantities
    int nIInt=i+grid.nCenIntOffset[0];
    
    for(int j=grid.nStartUpdateExplicit[grid.nE][1];j<grid.nEndUpdateExplicit[grid.nE][1];j++){
      
      //calculate j for interface centered quantities
      int nJInt=j+grid.nCenIntOffset[1];
      
      for(int k=grid.nStartUpdateExplicit[grid.nE][2];k<grid.nEndUpdateExplicit[grid.nE][2];k++){
        
      //calculate k for interface centered quantities
      int nKInt=k+grid.nCenIntOffset[2];
        
        //Calculate interpolated quantities
        double dU_ijk_np1half=(grid.dLocalGridNew[grid.nU][nIInt][j][k]
          +grid.dLocalGridNew[grid.nU][nIInt-1][j][k])*0.5;
        double dU0_i_np1half=(grid.dLocalGridNew[grid.nU0][nIInt][0][0]
          +grid.dLocalGridNew[grid.nU0][nIInt-1][0][0])*0.5;
        double dE_ip1halfjk_n=(grid.dLocalGridOld[grid.nE][i+1][j][k]+grid.dLocalGridOld[grid.nE][i][j][k])*0.5;
        double dE_im1halfjk_n=(grid.dLocalGridOld[grid.nE][i][j][k]+grid.dLocalGridOld[grid.nE][i-1][j][k])*0.5;
        double dR_i_n=(grid.dLocalGridOld[grid.nR][nIInt][0][0]+grid.dLocalGridOld[grid.nR][nIInt-1][0][0])*0.5;
        double dR_im1half_n=grid.dLocalGridOld[grid.nR][nIInt-1][0][0];
        double dR_ip1half_n=grid.dLocalGridOld[grid.nR][nIInt][0][0];
        double dRSq_i_n=dR_i_n*dR_i_n;
        double dV_ijk_np1half=(grid.dLocalGridNew[grid.nV][i][nJInt][k]
          +grid.dLocalGridNew[grid.nV][i][nJInt-1][k])*0.5;
        double dE_ijp1halfk_n=(grid.dLocalGridOld[grid.nE][i][j+1][k]+grid.dLocalGridOld[grid.nE][i][j][k])*0.5;
        double dE_ijm1halfk_n=(grid.dLocalGridOld[grid.nE][i][j][k]+grid.dLocalGridOld[grid.nE][i][j-1][k])*0.5;
        double dVSinTheta_ijp1halfk_np1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt][0]
          *grid.dLocalGridNew[grid.nV][i][nJInt][k];
        double dVSinTheta_ijm1halfk_np1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt-1][0]
          *grid.dLocalGridNew[grid.nV][i][nJInt-1][k];
        double dE_ijkp1half_n=(grid.dLocalGridOld[grid.nE][i][j][k+1]+grid.dLocalGridOld[grid.nE][i][j][k])*0.5;
        double dE_ijkm1half_n=(grid.dLocalGridOld[grid.nE][i][j][k-1]+grid.dLocalGridOld[grid.nE][i][j][k])*0.5;
        double dW_ijk_np1half=(grid.dLocalGridNew[grid.nW][i][j][nKInt]
          +grid.dLocalGridNew[grid.nW][i][j][nKInt-1])*0.5;
        double dW_ijkp1half_np1half=(grid.dLocalGridNew[grid.nW][i][j][nKInt]);
        double dW_ijkm1half_np1half=(grid.dLocalGridNew[grid.nW][i][j][nKInt-1]);
        
        //Calcuate dA1
        double dA1CenGrad=(dE_ip1halfjk_n-dE_im1halfjk_n)/grid.dLocalGridOld[grid.nDM][i][0][0];
        double dA1UpWindGrad=0.0;
        double dU_U0_Diff=(dU_ijk_np1half-dU0_i_np1half);
        if(dU_U0_Diff<0.0){//moving in the negative direction
          dA1UpWindGrad=(grid.dLocalGridOld[grid.nE][i+1][j][k]-grid.dLocalGridOld[grid.nE][i][j][k])
            /(grid.dLocalGridOld[grid.nDM][i+1][0][0]+grid.dLocalGridOld[grid.nDM][i][0][0])*2.0;
        }
        else{//moving in the postive direction
          dA1UpWindGrad=(grid.dLocalGridOld[grid.nE][i][j][k]-grid.dLocalGridOld[grid.nE][i-1][j][k])
            /(grid.dLocalGridOld[grid.nDM][i][0][0]+grid.dLocalGridOld[grid.nDM][i-1][0][0])*2.0;
        }
        double dA1=dU_U0_Diff*dRSq_i_n*((1.0-parameters.dDonorFrac)*dA1CenGrad
          +parameters.dDonorFrac*dA1UpWindGrad);
        
        //calculate dS1
        double dUR2_im1half_np1half=grid.dLocalGridNew[grid.nU][nIInt-1][j][k]*dR_im1half_n*dR_im1half_n;
        double dUR2_ip1half_np1half=grid.dLocalGridNew[grid.nU][nIInt][j][k]*dR_ip1half_n*dR_ip1half_n;
        double dPi_ijk_n=grid.dLocalGridOld[grid.nP][i][j][k];
        #if VISCOUS_ENERGY_EQ==1
          dPi_ijk_n+=grid.dLocalGridOld[grid.nQ0][i][j][k];
        #endif
        double dS1=dPi_ijk_n/grid.dLocalGridOld[grid.nD][i][j][k]
          *(dUR2_ip1half_np1half-dUR2_im1half_np1half)/grid.dLocalGridOld[grid.nDM][i][0][0];
        
        //Calcualte dA2
        double dA2CenGrad=(dE_ijp1halfk_n-dE_ijm1halfk_n)/grid.dLocalGridOld[grid.nDTheta][0][j][0];
        double dA2UpWindGrad=0.0;
        if(dV_ijk_np1half<0.0){//moving in the negative direction
          dA2UpWindGrad=(grid.dLocalGridOld[grid.nE][i][j+1][k]-grid.dLocalGridOld[grid.nE][i][j][k])/
            (grid.dLocalGridOld[grid.nDTheta][0][j+1][0]+grid.dLocalGridOld[grid.nDTheta][0][j][0])*2.0;
        }
        else{//moving in the positive direction
          dA2UpWindGrad=(grid.dLocalGridOld[grid.nE][i][j][k]-grid.dLocalGridOld[grid.nE][i][j-1][k])/
            (grid.dLocalGridOld[grid.nDTheta][0][j][0]+grid.dLocalGridOld[grid.nDTheta][0][j-1][0])*2.0;
        }
        double dA2=dV_ijk_np1half/dR_i_n*((1.0-parameters.dDonorFrac)*dA2CenGrad
          +parameters.dDonorFrac*dA2UpWindGrad);
          
        //Calcualte dS2
        double dPj_ijk_n=grid.dLocalGridOld[grid.nP][i][j][k];//+grid.dLocalGridOld[grid.nQ1][i][j][k];
        #if VISCOUS_ENERGY_EQ==1
          dPj_ijk_n+=grid.dLocalGridOld[grid.nQ1][i][j][k];
        #endif
        double dS2=dPj_ijk_n/(grid.dLocalGridOld[grid.nD][i][j][k]*dR_i_n
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]*grid.dLocalGridOld[grid.nDTheta][0][j][0])
          *(dVSinTheta_ijp1halfk_np1half-dVSinTheta_ijm1halfk_np1half);
        
        //Calcualte dA3
        double dA3CenGrad=(dE_ijkp1half_n-dE_ijkm1half_n)/grid.dLocalGridOld[grid.nDPhi][0][0][k];
        double dA3UpWindGrad=0.0;
        if(dW_ijk_np1half<0.0){//moving in the negative direction
          dA3UpWindGrad=(grid.dLocalGridOld[grid.nE][i][j][k+1]-grid.dLocalGridOld[grid.nE][i][j][k])/
            (grid.dLocalGridOld[grid.nDPhi][0][0][k+1]+grid.dLocalGridOld[grid.nDPhi][0][0][k])*2.0;
        }
        else{//moving in the positive direction
          dA3UpWindGrad=(grid.dLocalGridOld[grid.nE][i][j][k]-grid.dLocalGridOld[grid.nE][i][j][k-1])/
            (grid.dLocalGridOld[grid.nDPhi][0][0][k]+grid.dLocalGridOld[grid.nDPhi][0][0][k-1])*2.0;
        }
        double dA3=dW_ijk_np1half/(dR_i_n*grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0])*
          ((1.0-parameters.dDonorFrac)*dA3CenGrad+parameters.dDonorFrac*dA3UpWindGrad);
        
        //Calcualte dS3
        double dPk_ijk_n=grid.dLocalGridOld[grid.nP][i][j][k];
        #if VISCOUS_ENERGY_EQ==1
          dPk_ijk_n+=grid.dLocalGridOld[grid.nQ2][i][j][k];
        #endif
        double dS3=dPk_ijk_n/(grid.dLocalGridOld[grid.nD][i][j][k]*dR_i_n
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]*grid.dLocalGridOld[grid.nDPhi][0][0][k])
          *(dW_ijkp1half_np1half-dW_ijkm1half_np1half);
          
        //calculate new energy
        grid.dLocalGridNew[grid.nE][i][j][k]=grid.dLocalGridOld[grid.nE][i][j][k]
        -time.dDeltat_n*(4.0*parameters.dPi*grid.dLocalGridOld[grid.nDenAve][i][0][0]*(dA1+dS1)+dA2+dS2
          +dA3+dS3);
        
        if(grid.dLocalGridNew[grid.nE][i][j][k]<0.0){
          
          #if SIGNEGENG==1
            raise(SIGINT);
          #endif
          
          std::stringstream ssTemp;
          ssTemp<<__FILE__<<":"<<__FUNCTION__<<":"<<__LINE__<<":"<<procTop.nRank
            <<": negative energy calculated in , ("<<i<<","<<j<<","<<k<<")\n";
          throw exception2(ssTemp.str(),CALCULATION);
          
        }
      }
    }
  }
  
  //ghost region 0, outter most ghost region in x1 direction
  for(int i=grid.nStartGhostUpdateExplicit[grid.nE][0][0];i<grid.nEndGhostUpdateExplicit[grid.nE][0][0];i++){
    
    //calculate i for interface centered quantities
    int nIInt=i+grid.nCenIntOffset[0];
    
    for(int j=grid.nStartGhostUpdateExplicit[grid.nE][0][1];j<grid.nEndGhostUpdateExplicit[grid.nE][0][1];j++){
      
      //calculate i for interface centered quantities
      int nJInt=j+grid.nCenIntOffset[1];
      
      for(int k=grid.nStartGhostUpdateExplicit[grid.nE][0][2];k<grid.nEndGhostUpdateExplicit[grid.nE][0][2];k++){
        
        int nKInt=k+grid.nCenIntOffset[2];
        
        //Calculate interpolated quantities
        double dU_ijk_np1half=(grid.dLocalGridNew[grid.nU][nIInt][j][k]
          +grid.dLocalGridNew[grid.nU][nIInt-1][j][k])*0.5;
        double dU0_i_np1half=(grid.dLocalGridNew[grid.nU0][nIInt][0][0]
          +grid.dLocalGridNew[grid.nU0][nIInt-1][0][0])*0.5;
        double dE_ip1halfjk_n=(grid.dLocalGridOld[grid.nE][i][j][k])*0.5;/**\BC Missing
          grid.dLocalGridOld[grid.nE][i+1][j][k] in calculation of \f$E_{i+1/2,j,k}\f$ setting it 
          equal to zero. */
        double dE_im1halfjk_n=(grid.dLocalGridOld[grid.nE][i][j][k]
          +grid.dLocalGridOld[grid.nE][i-1][j][k])*0.5;
        double dR_i_n=(grid.dLocalGridOld[grid.nR][nIInt][0][0]
          +grid.dLocalGridOld[grid.nR][nIInt-1][0][0])*0.5;
        double dR_im1half_n=grid.dLocalGridOld[grid.nR][nIInt-1][0][0];
        double dR_ip1half_n=grid.dLocalGridOld[grid.nR][nIInt][0][0];
        double dRSq_i_n=dR_i_n*dR_i_n;
        double dV_ijk_np1half=(grid.dLocalGridNew[grid.nV][i][nJInt][k]
          +grid.dLocalGridNew[grid.nV][i][nJInt-1][k])*0.5;
        double dE_ijp1halfk_n=(grid.dLocalGridOld[grid.nE][i][j+1][k]
          +grid.dLocalGridOld[grid.nE][i][j][k])*0.5;
        double dE_ijm1halfk_n=(grid.dLocalGridOld[grid.nE][i][j][k]
          +grid.dLocalGridOld[grid.nE][i][j-1][k])*0.5;
        double dVSinTheta_ijp1halfk_np1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt][0]
          *grid.dLocalGridNew[grid.nV][i][nJInt][k];
        double dVSinTheta_ijm1halfk_np1half=
          grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt-1][0]
          *grid.dLocalGridNew[grid.nV][i][nJInt-1][k];
        double dE_ijkp1half_n=(grid.dLocalGridOld[grid.nE][i][j][k+1]
          +grid.dLocalGridOld[grid.nE][i][j][k])*0.5;
        double dE_ijkm1half_n=(grid.dLocalGridOld[grid.nE][i][j][k-1]
          +grid.dLocalGridOld[grid.nE][i][j][k])*0.5;
        double dW_ijk_np1half=(grid.dLocalGridNew[grid.nW][i][j][nKInt]
          +grid.dLocalGridNew[grid.nW][i][j][nKInt-1])*0.5;
        double dW_ijkp1half_np1half=(grid.dLocalGridNew[grid.nW][i][j][nKInt]);
        double dW_ijkm1half_np1half=(grid.dLocalGridNew[grid.nW][i][j][nKInt-1]);
        
        //Calcuate dA1
        double dA1CenGrad=(dE_ip1halfjk_n-dE_im1halfjk_n)/grid.dLocalGridOld[grid.nDM][i][0][0];
        double dA1UpWindGrad=0.0;
        double dU_U0_Diff=(dU_ijk_np1half-dU0_i_np1half);
        if(dU_U0_Diff<0.0){//moving in the negative direction
          dA1UpWindGrad=dA1CenGrad;/**\BC grid.dLocalGridOld[grid.nDM][i+1][0][0] and
            grid.dLocalGridOld[grid.nE][i+1][j][k] missing in the calculation of upwind gradient
            in dA1.Using the centered gradient.*/
        }
        else{//moving in the postive direction
          dA1UpWindGrad=(grid.dLocalGridOld[grid.nE][i][j][k]
            -grid.dLocalGridOld[grid.nE][i-1][j][k])/(grid.dLocalGridOld[grid.nDM][i][0][0]
            +grid.dLocalGridOld[grid.nDM][i-1][0][0])*2.0;
        }
        double dA1=dU_U0_Diff*dRSq_i_n*((1.0-parameters.dDonorFrac)*dA1CenGrad
          +parameters.dDonorFrac*dA1UpWindGrad);
        
        //calculate dS1
        double dUR2_im1half_np1half=grid.dLocalGridNew[grid.nU][nIInt-1][j][k]*dR_im1half_n
          *dR_im1half_n;
        double dUR2_ip1half_np1half=grid.dLocalGridNew[grid.nU][nIInt][j][k]*dR_ip1half_n
          *dR_ip1half_n;
        double dP_ijk_n=grid.dLocalGridOld[grid.nP][i][j][k];
        #if VISCOUS_ENERGY_EQ==1
          dP_ijk_n+=grid.dLocalGridOld[grid.nQ0][i][j][k];
        #endif
        double dS1=dP_ijk_n/grid.dLocalGridOld[grid.nD][i][j][k]
          *(dUR2_ip1half_np1half-dUR2_im1half_np1half)/grid.dLocalGridOld[grid.nDM][i][0][0];
        
        //Calcualte dA2
        double dA2CenGrad=(dE_ijp1halfk_n-dE_ijm1halfk_n)/grid.dLocalGridOld[grid.nDTheta][0][j][0];
        double dA2UpWindGrad=0.0;
        if(dV_ijk_np1half<0.0){//moving in the negative direction
          dA2UpWindGrad=(grid.dLocalGridOld[grid.nE][i][j+1][k]
            -grid.dLocalGridOld[grid.nE][i][j][k])/(grid.dLocalGridOld[grid.nDTheta][0][j+1][0]
            +grid.dLocalGridOld[grid.nDTheta][0][j][0])*2.0;
        }
        else{//moving in the positive direction
          dA2UpWindGrad=(grid.dLocalGridOld[grid.nE][i][j][k]
            -grid.dLocalGridOld[grid.nE][i][j-1][k])/(grid.dLocalGridOld[grid.nDTheta][0][j][0]
            +grid.dLocalGridOld[grid.nDTheta][0][j-1][0])*2.0;
        }
        double dA2=dV_ijk_np1half/dR_i_n*((1.0-parameters.dDonorFrac)*dA2CenGrad
          +parameters.dDonorFrac*dA2UpWindGrad);
          
        //Calcualte dS2
        dP_ijk_n=grid.dLocalGridOld[grid.nP][i][j][k];
        #if VISCOUS_ENERGY_EQ==1
          dP_ijk_n+=grid.dLocalGridOld[grid.nQ1][i][j][k];
        #endif
        double dS2=dP_ijk_n/(grid.dLocalGridOld[grid.nD][i][j][k]*dR_i_n
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]*grid.dLocalGridOld[grid.nDTheta][0][j][0])
          *(dVSinTheta_ijp1halfk_np1half-dVSinTheta_ijm1halfk_np1half);
        
        //Calcualte dA3
        double dA3CenGrad=(dE_ijkp1half_n-dE_ijkm1half_n)/grid.dLocalGridOld[grid.nDPhi][0][0][k];
        double dA3UpWindGrad=0.0;
        if(dW_ijk_np1half<0.0){//moving in the negative direction
          dA3UpWindGrad=(grid.dLocalGridOld[grid.nE][i][j][k+1]
            -grid.dLocalGridOld[grid.nE][i][j][k])/(grid.dLocalGridOld[grid.nDPhi][0][0][k+1]
            +grid.dLocalGridOld[grid.nDPhi][0][0][k])*2.0;
        }
        else{//moving in the positive direction
          dA3UpWindGrad=(grid.dLocalGridOld[grid.nE][i][j][k]
            -grid.dLocalGridOld[grid.nE][i][j][k-1])/(grid.dLocalGridOld[grid.nDPhi][0][0][k]
            +grid.dLocalGridOld[grid.nDPhi][0][0][k-1])*2.0;
        }
        double dA3=dW_ijk_np1half/(dR_i_n*grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0])*
          ((1.0-parameters.dDonorFrac)*dA3CenGrad+parameters.dDonorFrac*dA3UpWindGrad);
        
        //Calcualte dS3
        dP_ijk_n=grid.dLocalGridOld[grid.nP][i][j][k];
        #if VISCOUS_ENERGY_EQ==1
          dP_ijk_n+=grid.dLocalGridOld[grid.nQ2][i][j][k];
        #endif
        double dS3=dP_ijk_n/(grid.dLocalGridOld[grid.nD][i][j][k]*dR_i_n
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]*grid.dLocalGridOld[grid.nDPhi][0][0][k])
          *(dW_ijkp1half_np1half-dW_ijkm1half_np1half);
          
        //calculate new energy
        grid.dLocalGridNew[grid.nE][i][j][k]=grid.dLocalGridOld[grid.nE][i][j][k]
        -time.dDeltat_n*(4.0*parameters.dPi*grid.dLocalGridOld[grid.nDenAve][i][0][0]*(dA1+dS1)
          +dA2+dS2+dA3+dS3);
        
        if(grid.dLocalGridNew[grid.nE][i][j][k]<0.0){
          
          #if SIGNEGENG==1
            raise(SIGINT);
          #endif
          
          std::stringstream ssTemp;
          ssTemp<<__FILE__<<":"<<__FUNCTION__<<":"<<__LINE__<<":"<<procTop.nRank
            <<": negative energy calculated in , ("<<i<<","<<j<<","<<k<<")\n";
          throw exception2(ssTemp.str(),CALCULATION);
          
        }
      }
    }
  }
  
  #if SEDOV==1 //use zero P, E, and rho gradients
    for(int i=grid.nStartGhostUpdateExplicit[grid.nE][1][0];i<grid.nEndGhostUpdateExplicit[grid.nE][1][0];i++){
      
      for(int j=grid.nStartGhostUpdateExplicit[grid.nE][1][1];j<grid.nEndGhostUpdateExplicit[grid.nE][1][1];j++){
        for(int k=grid.nStartGhostUpdateExplicit[grid.nE][1][2];k<grid.nEndGhostUpdateExplicit[grid.nE][1][2];k++){
          grid.dLocalGridNew[grid.nE][i][j][k]=grid.dLocalGridNew[grid.nE][i+1][j][k];
        }
      }
    }
  #endif
}
void calNewE_RTP_NA(Grid &grid, Parameters &parameters, Time &time, ProcTop &procTop){
  for(int i=grid.nStartUpdateExplicit[grid.nE][0];i<grid.nEndUpdateExplicit[grid.nE][0];i++){
    
    //calculate i for interface centered quantities
    int nIInt=i+grid.nCenIntOffset[0];
    
    for(int j=grid.nStartUpdateExplicit[grid.nE][1];j<grid.nEndUpdateExplicit[grid.nE][1];j++){
      
      //calculate j for interface centered quantities
      int nJInt=j+grid.nCenIntOffset[1];
      
      for(int k=grid.nStartUpdateExplicit[grid.nE][2];k<grid.nEndUpdateExplicit[grid.nE][2];k++){
        
        //calculate k for interface centered quantities
        int nKInt=k+grid.nCenIntOffset[2];
        
        //Calculate interpolated quantities
        double dU_ijk_np1half=(grid.dLocalGridNew[grid.nU][nIInt][j][k]
          +grid.dLocalGridNew[grid.nU][nIInt-1][j][k])*0.5;
        double dU0_i_np1half=(grid.dLocalGridNew[grid.nU0][nIInt][0][0]
          +grid.dLocalGridNew[grid.nU0][nIInt-1][0][0])*0.5;
        double dE_ip1halfjk_n=(grid.dLocalGridOld[grid.nE][i+1][j][k]
          +grid.dLocalGridOld[grid.nE][i][j][k])*0.5;
        double dE_im1halfjk_n=(grid.dLocalGridOld[grid.nE][i][j][k]
          +grid.dLocalGridOld[grid.nE][i-1][j][k])*0.5;
        double dR_i_n=(grid.dLocalGridOld[grid.nR][nIInt][0][0]
          +grid.dLocalGridOld[grid.nR][nIInt-1][0][0])*0.5;
        double dR_im1half_n=grid.dLocalGridOld[grid.nR][nIInt-1][0][0];
        double dR_ip1half_n=grid.dLocalGridOld[grid.nR][nIInt][0][0];
        double dRSq_i_n=dR_i_n*dR_i_n;
        double dRSq_ip1half=dR_ip1half_n*dR_ip1half_n;
        double dR4_ip1half=dRSq_ip1half*dRSq_ip1half;
        double dR_im1half_sq=dR_im1half_n*dR_im1half_n;
        double dR_im1half_4=dR_im1half_sq*dR_im1half_sq;
        double dV_ijk_np1half=(grid.dLocalGridNew[grid.nV][i][nJInt][k]
          +grid.dLocalGridNew[grid.nV][i][nJInt-1][k])*0.5;
        double dE_ijp1halfk_n=(grid.dLocalGridOld[grid.nE][i][j+1][k]
          +grid.dLocalGridOld[grid.nE][i][j][k])*0.5;
        double dE_ijm1halfk_n=(grid.dLocalGridOld[grid.nE][i][j][k]
          +grid.dLocalGridOld[grid.nE][i][j-1][k])*0.5;
        double dVSinTheta_ijp1halfk_np1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt][0]
          *grid.dLocalGridNew[grid.nV][i][nJInt][k];
        double dVSinTheta_ijm1halfk_np1half=
          grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt-1][0]
          *grid.dLocalGridNew[grid.nV][i][nJInt-1][k];
        double dE_ijkp1half_n=(grid.dLocalGridOld[grid.nE][i][j][k+1]
          +grid.dLocalGridOld[grid.nE][i][j][k])*0.5;
        double dE_ijkm1half_n=(grid.dLocalGridOld[grid.nE][i][j][k-1]
          +grid.dLocalGridOld[grid.nE][i][j][k])*0.5;
        double dW_ijk_np1half=(grid.dLocalGridNew[grid.nW][i][j][nKInt]
          +grid.dLocalGridNew[grid.nW][i][j][nKInt-1])*0.5;
        double dW_ijkp1half_np1half=(grid.dLocalGridNew[grid.nW][i][j][nKInt]);
        double dW_ijkm1half_np1half=(grid.dLocalGridNew[grid.nW][i][j][nKInt-1]);
        double dRhoAve_ip1half=(grid.dLocalGridOld[grid.nDenAve][i+1][0][0]
          +grid.dLocalGridOld[grid.nDenAve][i][0][0])*0.5;
        double dRhoAve_im1half=(grid.dLocalGridOld[grid.nDenAve][i][0][0]
          +grid.dLocalGridOld[grid.nDenAve][i-1][0][0])*0.5;
        double dRho_ip1halfjk=(grid.dLocalGridOld[grid.nD][i+1][j][k]
          +grid.dLocalGridOld[grid.nD][i][j][k])*0.5;
        double dRho_im1halfjk=(grid.dLocalGridOld[grid.nD][i][j][k]
          +grid.dLocalGridOld[grid.nD][i-1][j][k])*0.5;
        double dRho_ijp1halfk=(grid.dLocalGridOld[grid.nD][i][j+1][k]
          +grid.dLocalGridOld[grid.nD][i][j][k])*0.5;
        double dRho_ijm1halfk=(grid.dLocalGridOld[grid.nD][i][j][k]
          +grid.dLocalGridOld[grid.nD][i][j-1][k])*0.5;
        double dRho_ijkp1half=(grid.dLocalGridOld[grid.nD][i][j][k+1]
          +grid.dLocalGridOld[grid.nD][i][j][k])*0.5;
        double dRho_ijkm1half=(grid.dLocalGridOld[grid.nD][i][j][k]
          +grid.dLocalGridOld[grid.nD][i][j][k-1])*0.5;
        double dTSq_ip1jk_n=grid.dLocalGridOld[grid.nT][i+1][j][k]
          *grid.dLocalGridOld[grid.nT][i+1][j][k];
        double dT4_ip1jk_n=dTSq_ip1jk_n*dTSq_ip1jk_n;
        double dTSq_ijk_n=grid.dLocalGridOld[grid.nT][i][j][k]
          *grid.dLocalGridOld[grid.nT][i][j][k];
        double dT4_ijk_n=dTSq_ijk_n*dTSq_ijk_n;
        double dTSq_im1jk_n=grid.dLocalGridOld[grid.nT][i-1][j][k]
          *grid.dLocalGridOld[grid.nT][i-1][j][k];
        double dT4_im1jk_n=dTSq_im1jk_n*dTSq_im1jk_n;
        double dTSq_ijp1k=grid.dLocalGridOld[grid.nT][i][j+1][k]
          *grid.dLocalGridOld[grid.nT][i][j+1][k];
        double dT4_ijp1k=dTSq_ijp1k*dTSq_ijp1k;
        double dTSq_ijm1k=grid.dLocalGridOld[grid.nT][i][j-1][k]
          *grid.dLocalGridOld[grid.nT][i][j-1][k];
        double dT4_ijm1k=dTSq_ijm1k*dTSq_ijm1k;
        double dT_ijkp1_sq=grid.dLocalGridOld[grid.nT][i][j][k+1]
          *grid.dLocalGridOld[grid.nT][i][j][k+1];
        double dT_ijkp1_4=dT_ijkp1_sq*dT_ijkp1_sq;
        double dT_ijkm1_sq=grid.dLocalGridOld[grid.nT][i][j][k-1]
          *grid.dLocalGridOld[grid.nT][i][j][k-1];
        double dT_ijkm1_4=dT_ijkm1_sq*dT_ijkm1_sq;
        double dKappa_ip1halfjk_n=(dT4_ip1jk_n+dT4_ijk_n)
          /(dT4_ijk_n/grid.dLocalGridOld[grid.nKappa][i][j][k]
          +dT4_ip1jk_n/grid.dLocalGridOld[grid.nKappa][i+1][j][k]);
        double dKappa_im1halfjk_n=(dT4_im1jk_n+dT4_ijk_n)
          /(dT4_ijk_n/grid.dLocalGridOld[grid.nKappa][i][j][k]
          +dT4_im1jk_n/grid.dLocalGridOld[grid.nKappa][i-1][j][k]);
        double dKappa_ijp1halfk_n=(grid.dLocalGridOld[grid.nKappa][i][j+1][k]
          +grid.dLocalGridOld[grid.nKappa][i][j][k])*0.5;
        double dKappa_ijm1halfk_n=(grid.dLocalGridOld[grid.nKappa][i][j][k]
          +grid.dLocalGridOld[grid.nKappa][i][j-1][k])*0.5;
        double dKappa_ijkp1half=(grid.dLocalGridOld[grid.nKappa][i][j][k+1]
          +grid.dLocalGridOld[grid.nKappa][i][j][k])*0.5;
        double dKappa_ijkm1half=(grid.dLocalGridOld[grid.nKappa][i][j][k]
          +grid.dLocalGridOld[grid.nKappa][i][j][k-1])*0.5;
        
        //Calcuate dA1
        double dA1CenGrad=(dE_ip1halfjk_n-dE_im1halfjk_n)/grid.dLocalGridOld[grid.nDM][i][0][0];
        double dA1UpWindGrad=0.0;
        double dU_U0_Diff=(dU_ijk_np1half-dU0_i_np1half);
        if(dU_U0_Diff<0.0){//moving in the negative direction
          dA1UpWindGrad=(grid.dLocalGridOld[grid.nE][i+1][j][k]-grid.dLocalGridOld[grid.nE][i][j][k])
            /(grid.dLocalGridOld[grid.nDM][i+1][0][0]+grid.dLocalGridOld[grid.nDM][i][0][0])*2.0;
        }
        else{//moving in the postive direction
          dA1UpWindGrad=(grid.dLocalGridOld[grid.nE][i][j][k]-grid.dLocalGridOld[grid.nE][i-1][j][k])
            /(grid.dLocalGridOld[grid.nDM][i][0][0]+grid.dLocalGridOld[grid.nDM][i-1][0][0])*2.0;
        }
        double dA1=dU_U0_Diff*dRSq_i_n*((1.0-parameters.dDonorFrac)*dA1CenGrad
          +parameters.dDonorFrac*dA1UpWindGrad);
        
        //calculate dS1
        double dUR2_im1half_np1half=grid.dLocalGridNew[grid.nU][nIInt-1][j][k]
          *dR_im1half_n*dR_im1half_n;
        double dUR2_ip1half_np1half=grid.dLocalGridNew[grid.nU][nIInt][j][k]
          *dR_ip1half_n*dR_ip1half_n;
        double dPi_ijk_n=grid.dLocalGridOld[grid.nP][i][j][k];
        #if VISCOUS_ENERGY_EQ==1
          dPi_ijk_n+=grid.dLocalGridOld[grid.nQ0][i][j][k];
        #endif
        double dS1=dPi_ijk_n/grid.dLocalGridOld[grid.nD][i][j][k]
          *(dUR2_ip1half_np1half-dUR2_im1half_np1half)/grid.dLocalGridOld[grid.nDM][i][0][0];
        
        //Calcualte dA2
        double dA2CenGrad=(dE_ijp1halfk_n-dE_ijm1halfk_n)/grid.dLocalGridOld[grid.nDTheta][0][j][0];
        double dA2UpWindGrad=0.0;
        if(dV_ijk_np1half<0.0){//moving in the negative direction
          dA2UpWindGrad=(grid.dLocalGridOld[grid.nE][i][j+1][k]
            -grid.dLocalGridOld[grid.nE][i][j][k])/
            (grid.dLocalGridOld[grid.nDTheta][0][j+1][0]
            +grid.dLocalGridOld[grid.nDTheta][0][j][0])*2.0;
        }
        else{//moving in the positive direction
          dA2UpWindGrad=(grid.dLocalGridOld[grid.nE][i][j][k]
            -grid.dLocalGridOld[grid.nE][i][j-1][k])/
            (grid.dLocalGridOld[grid.nDTheta][0][j][0]
            +grid.dLocalGridOld[grid.nDTheta][0][j-1][0])*2.0;
        }
        double dA2=dV_ijk_np1half/dR_i_n*((1.0-parameters.dDonorFrac)*dA2CenGrad
          +parameters.dDonorFrac*dA2UpWindGrad);
          
        //Calcualte dS2
        double dPj_ijk_n=grid.dLocalGridOld[grid.nP][i][j][k];
        #if VISCOUS_ENERGY_EQ==1
          dPj_ijk_n+=grid.dLocalGridOld[grid.nQ1][i][j][k];
        #endif
        double dS2=dPj_ijk_n/(grid.dLocalGridOld[grid.nD][i][j][k]*dR_i_n
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]*grid.dLocalGridOld[grid.nDTheta][0][j][0])
          *(dVSinTheta_ijp1halfk_np1half-dVSinTheta_ijm1halfk_np1half);
        
        //Calcualte dA3
        double dA3CenGrad=(dE_ijkp1half_n-dE_ijkm1half_n)/grid.dLocalGridOld[grid.nDPhi][0][0][k];
        double dA3UpWindGrad=0.0;
        if(dW_ijk_np1half<0.0){//moving in the negative direction
          dA3UpWindGrad=(grid.dLocalGridOld[grid.nE][i][j][k+1]
            -grid.dLocalGridOld[grid.nE][i][j][k])/
            (grid.dLocalGridOld[grid.nDPhi][0][0][k+1]+grid.dLocalGridOld[grid.nDPhi][0][0][k])*2.0;
        }
        else{//moving in the positive direction
          dA3UpWindGrad=(grid.dLocalGridOld[grid.nE][i][j][k]
            -grid.dLocalGridOld[grid.nE][i][j][k-1])/
            (grid.dLocalGridOld[grid.nDPhi][0][0][k]+grid.dLocalGridOld[grid.nDPhi][0][0][k-1])*2.0;
        }
        double dA3=dW_ijk_np1half/(dR_i_n*grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0])*
          ((1.0-parameters.dDonorFrac)*dA3CenGrad+parameters.dDonorFrac*dA3UpWindGrad);
        
        //Calcualte dS3
        double dPk_ijk_n=grid.dLocalGridOld[grid.nP][i][j][k];
        #if VISCOUS_ENERGY_EQ==1
          dPk_ijk_n+=grid.dLocalGridOld[grid.nQ2][i][j][k];
        #endif
        double dS3=dPk_ijk_n/(grid.dLocalGridOld[grid.nD][i][j][k]*dR_i_n
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]*grid.dLocalGridOld[grid.nDPhi][0][0][k])
          *(dW_ijkp1half_np1half-dW_ijkm1half_np1half);
        
        //Calculate dS4
        double dTGrad_ip1half=(dT4_ip1jk_n-dT4_ijk_n)/(grid.dLocalGridOld[grid.nDM][i+1][0][0]
          +grid.dLocalGridOld[grid.nDM][i][0][0])*2.0;
        double dTGrad_im1half=(dT4_ijk_n-dT4_im1jk_n)/(grid.dLocalGridOld[grid.nDM][i][0][0]
          +grid.dLocalGridOld[grid.nDM][i-1][0][0])*2.0;
        double dGrad_ip1half=dRhoAve_ip1half*dR4_ip1half/(dKappa_ip1halfjk_n*dRho_ip1halfjk)
          *dTGrad_ip1half;
        double dGrad_im1half=dRhoAve_im1half*dR_im1half_4/(dKappa_im1halfjk_n*dRho_im1halfjk)
          *dTGrad_im1half;
        double dS4=16.0*parameters.dPi*parameters.dPi*grid.dLocalGridOld[grid.nDenAve][i][0][0]
          *(dGrad_ip1half-dGrad_im1half)/grid.dLocalGridOld[grid.nDM][i][0][0];
        
        //Calculate dS5
        double dTGrad_jp1half=(dT4_ijp1k-dT4_ijk_n)/(grid.dLocalGridOld[grid.nDTheta][0][j+1][0]
          +grid.dLocalGridOld[grid.nDTheta][0][j][0])*2.0;
        double dTGrad_jm1half=(dT4_ijk_n-dT4_ijm1k)/(grid.dLocalGridOld[grid.nDTheta][0][j][0]
          +grid.dLocalGridOld[grid.nDTheta][0][j-1][0])*2.0;;
        double dGrad_jp1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt][0]
          /(dKappa_ijp1halfk_n*dRho_ijp1halfk*dR_i_n)*dTGrad_jp1half;
        double dGrad_jm1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt-1][0]
          /(dKappa_ijm1halfk_n*dRho_ijm1halfk*dR_i_n)*dTGrad_jm1half;;
        double dS5=(dGrad_jp1half-dGrad_jm1half)/(grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]
          *dR_i_n*grid.dLocalGridOld[grid.nDTheta][0][j][0]);
        
        //Calculate dS6
        double dTGrad_kp1half=(dT_ijkp1_4-dT4_ijk_n)/(grid.dLocalGridOld[grid.nDPhi][0][0][k+1]
          +grid.dLocalGridOld[grid.nDPhi][0][0][k])*2.0;
        double dTGrad_km1half=(dT4_ijk_n-dT_ijkm1_4)/(grid.dLocalGridOld[grid.nDPhi][0][0][k]
          +grid.dLocalGridOld[grid.nDPhi][0][0][k-1])*2.0;;
        double dGrad_kp1half=dTGrad_kp1half/(dKappa_ijkp1half*dRho_ijkp1half*dR_i_n);
        double dGrad_km1half=dTGrad_km1half/(dKappa_ijkm1half*dRho_ijkm1half*dR_i_n);
        double dS6=(dGrad_kp1half-dGrad_km1half)/(dR_i_n
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]*grid.dLocalGridOld[grid.nDPhi][0][0][k]);
        
        //calculate new energy
        grid.dLocalGridNew[grid.nE][i][j][k]=grid.dLocalGridOld[grid.nE][i][j][k]
          -time.dDeltat_n*(4.0*parameters.dPi*grid.dLocalGridOld[grid.nDenAve][i][0][0]*(dA1+dS1)
          +dA2+dS2+dA3+dS3
          -4.0*parameters.dSigma/(3.0*grid.dLocalGridOld[grid.nD][i][j][k])*(dS4+dS5+dS6));
        
        if(grid.dLocalGridNew[grid.nE][i][j][k]<0.0){
          
          #if SIGNEGENG==1
            raise(SIGINT);
          #endif
          
          std::stringstream ssTemp;
          ssTemp<<__FILE__<<":"<<__FUNCTION__<<":"<<__LINE__<<":"<<procTop.nRank
            <<": negative energy calculated in , ("<<i<<","<<j<<","<<k<<")\n";
          throw exception2(ssTemp.str(),CALCULATION);
          
        }
      }
    }
  }
  
  //ghost region 0, outter most ghost region in x1 direction
  for(int i=grid.nStartGhostUpdateExplicit[grid.nE][0][0];i<grid.nEndGhostUpdateExplicit[grid.nE][0][0];i++){
    
    //calculate i for interface centered quantities
    int nIInt=i+grid.nCenIntOffset[0];
    
    for(int j=grid.nStartGhostUpdateExplicit[grid.nE][0][1];j<grid.nEndGhostUpdateExplicit[grid.nE][0][1];j++){
      
      //calculate i for interface centered quantities
      int nJInt=j+grid.nCenIntOffset[1];
      
      for(int k=grid.nStartGhostUpdateExplicit[grid.nE][0][2];k<grid.nEndGhostUpdateExplicit[grid.nE][0][2];k++){
        
        int nKInt=k+grid.nCenIntOffset[2];
        
        //Calculate interpolated quantities
        double dU_ijk_np1half=(grid.dLocalGridNew[grid.nU][nIInt][j][k]
          +grid.dLocalGridNew[grid.nU][nIInt-1][j][k])*0.5;
        double dU0_i_np1half=(grid.dLocalGridNew[grid.nU0][nIInt][0][0]
          +grid.dLocalGridNew[grid.nU0][nIInt-1][0][0])*0.5;
        double dE_ip1halfjk_n=(grid.dLocalGridOld[grid.nE][i][j][k]);/**\BC Missing
          grid.dLocalGridOld[grid.nE][i+1][j][k] in calculation of \f$E_{i+1/2,j,k}\f$ 
          setting it equal to the value at i.*/
        double dE_im1halfjk_n=(grid.dLocalGridOld[grid.nE][i][j][k]
          +grid.dLocalGridOld[grid.nE][i-1][j][k])*0.5;
        double dR_i_n=(grid.dLocalGridOld[grid.nR][nIInt][0][0]
          +grid.dLocalGridOld[grid.nR][nIInt-1][0][0])*0.5;
        double dR_im1half_n=grid.dLocalGridOld[grid.nR][nIInt-1][0][0];
        double dR_ip1half_n=grid.dLocalGridOld[grid.nR][nIInt][0][0];
        double dRSq_i_n=dR_i_n*dR_i_n;
        double dRSq_ip1half=dR_ip1half_n*dR_ip1half_n;
        double dR_im1half_sq=dR_im1half_n*dR_im1half_n;
        double dR_im1half_4=dR_im1half_sq*dR_im1half_sq;
        double dV_ijk_np1half=(grid.dLocalGridNew[grid.nV][i][nJInt][k]
          +grid.dLocalGridNew[grid.nV][i][nJInt-1][k])*0.5;
        double dE_ijp1halfk_n=(grid.dLocalGridOld[grid.nE][i][j+1][k]
          +grid.dLocalGridOld[grid.nE][i][j][k])*0.5;
        double dE_ijm1halfk_n=(grid.dLocalGridOld[grid.nE][i][j][k]
          +grid.dLocalGridOld[grid.nE][i][j-1][k])*0.5;
        double dVSinTheta_ijp1halfk_np1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt][0]
          *grid.dLocalGridNew[grid.nV][i][nJInt][k];
        double dVSinTheta_ijm1halfk_np1half=
          grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt-1][0]
          *grid.dLocalGridNew[grid.nV][i][nJInt-1][k];
        double dE_ijkp1half_n=(grid.dLocalGridOld[grid.nE][i][j][k+1]
          +grid.dLocalGridOld[grid.nE][i][j][k])*0.5;
        double dE_ijkm1half_n=(grid.dLocalGridOld[grid.nE][i][j][k-1]
          +grid.dLocalGridOld[grid.nE][i][j][k])*0.5;
        double dW_ijk_np1half=(grid.dLocalGridNew[grid.nW][i][j][nKInt]
          +grid.dLocalGridNew[grid.nW][i][j][nKInt-1])*0.5;
        double dW_ijkp1half_np1half=(grid.dLocalGridNew[grid.nW][i][j][nKInt]);
        double dW_ijkm1half_np1half=(grid.dLocalGridNew[grid.nW][i][j][nKInt-1]);
        double dRhoAve_im1half=(grid.dLocalGridOld[grid.nDenAve][i][0][0]
          +grid.dLocalGridOld[grid.nDenAve][i-1][0][0])*0.5;
        double dRho_im1halfjk=(grid.dLocalGridOld[grid.nD][i][j][k]
          +grid.dLocalGridOld[grid.nD][i-1][j][k])*0.5;
        double dRho_ijp1halfk=(grid.dLocalGridOld[grid.nD][i][j+1][k]
          +grid.dLocalGridOld[grid.nD][i][j][k])*0.5;
        double dRho_ijm1halfk=(grid.dLocalGridOld[grid.nD][i][j][k]
          +grid.dLocalGridOld[grid.nD][i][j-1][k])*0.5;
        double dRho_ijkp1half=(grid.dLocalGridOld[grid.nD][i][j][k+1]
          +grid.dLocalGridOld[grid.nD][i][j][k])*0.5;
        double dRho_ijkm1half=(grid.dLocalGridOld[grid.nD][i][j][k]
          +grid.dLocalGridOld[grid.nD][i][j][k-1])*0.5;
        double dTSq_ijk_n=grid.dLocalGridOld[grid.nT][i][j][k]
          *grid.dLocalGridOld[grid.nT][i][j][k];
        double dT4_ijk_n=dTSq_ijk_n*dTSq_ijk_n;
        double dTSq_im1jk_n=grid.dLocalGridOld[grid.nT][i-1][j][k]
          *grid.dLocalGridOld[grid.nT][i-1][j][k];
        double dT4_im1jk_n=dTSq_im1jk_n*dTSq_im1jk_n;
        double dTSq_ijp1k=grid.dLocalGridOld[grid.nT][i][j+1][k]
          *grid.dLocalGridOld[grid.nT][i][j+1][k];
        double dT4_ijp1k=dTSq_ijp1k*dTSq_ijp1k;
        double dTSq_ijm1k=grid.dLocalGridOld[grid.nT][i][j-1][k]
          *grid.dLocalGridOld[grid.nT][i][j-1][k];
        double dT4_ijm1k=dTSq_ijm1k*dTSq_ijm1k;
        double dT_ijkp1_sq=grid.dLocalGridOld[grid.nT][i][j][k+1]
          *grid.dLocalGridOld[grid.nT][i][j][k+1];
        double dT_ijkp1_4=dT_ijkp1_sq*dT_ijkp1_sq;
        double dT_ijkm1_sq=grid.dLocalGridOld[grid.nT][i][j][k-1]
          *grid.dLocalGridOld[grid.nT][i][j][k-1];
        double dT_ijkm1_4=dT_ijkm1_sq*dT_ijkm1_sq;
        double dKappa_im1halfjk_n=(dT4_im1jk_n+dT4_ijk_n)
          /(dT4_ijk_n/grid.dLocalGridOld[grid.nKappa][i][j][k]
          +dT4_im1jk_n/grid.dLocalGridOld[grid.nKappa][i-1][j][k]);
        double dKappa_ijp1halfk_n=(grid.dLocalGridOld[grid.nKappa][i][j+1][k]
          +grid.dLocalGridOld[grid.nKappa][i][j][k])*0.5;
        double dKappa_ijm1halfk_n=(grid.dLocalGridOld[grid.nKappa][i][j][k]
          +grid.dLocalGridOld[grid.nKappa][i][j-1][k])*0.5;
        double dKappa_ijkp1half=(grid.dLocalGridOld[grid.nKappa][i][j][k+1]
          +grid.dLocalGridOld[grid.nKappa][i][j][k])*0.5;
        double dKappa_ijkm1half=(grid.dLocalGridOld[grid.nKappa][i][j][k]
          +grid.dLocalGridOld[grid.nKappa][i][j][k-1])*0.5;
        
        //Calcuate dA1
        double dA1CenGrad=(dE_ip1halfjk_n-dE_im1halfjk_n)/grid.dLocalGridOld[grid.nDM][i][0][0];
        double dA1UpWindGrad=0.0;
        double dU_U0_Diff=(dU_ijk_np1half-dU0_i_np1half);
        if(dU_U0_Diff<0.0){//moving in the negative direction
          dA1UpWindGrad=dA1CenGrad;/**\BC grid.dLocalGridOld[grid.nDM][i+1][0][0] and 
            grid.dLocalGridOld[grid.nE][i+1][j][k] missing in the calculation of upwind gradient in 
            dA1. Using the centered gradient instead.*/
        }
        else{//moving in the postive direction*/
          dA1UpWindGrad=(grid.dLocalGridOld[grid.nE][i][j][k]
            -grid.dLocalGridOld[grid.nE][i-1][j][k])/(grid.dLocalGridOld[grid.nDM][i][0][0]
            +grid.dLocalGridOld[grid.nDM][i-1][0][0])*2.0;
        }
        double dA1=dU_U0_Diff*dRSq_i_n*((1.0-parameters.dDonorFrac)*dA1CenGrad
          +parameters.dDonorFrac*dA1UpWindGrad);
        
        //calculate dS1
        double dUR2_im1half_np1half=grid.dLocalGridNew[grid.nU][nIInt-1][j][k]*dR_im1half_n
          *dR_im1half_n;
        double dUR2_ip1half_np1half=grid.dLocalGridNew[grid.nU][nIInt][j][k]*dR_ip1half_n
          *dR_ip1half_n;
        double dP_ijk_n=grid.dLocalGridOld[grid.nP][i][j][k];
        #if VISCOUS_ENERGY_EQ==1
          dP_ijk_n+=grid.dLocalGridOld[grid.nQ0][i][j][k];
        #endif
        double dS1=dP_ijk_n/grid.dLocalGridOld[grid.nD][i][j][k]
          *(dUR2_ip1half_np1half-dUR2_im1half_np1half)/grid.dLocalGridOld[grid.nDM][i][0][0];
        
        //Calcualte dA2
        double dA2CenGrad=(dE_ijp1halfk_n-dE_ijm1halfk_n)/grid.dLocalGridOld[grid.nDTheta][0][j][0];
        double dA2UpWindGrad=0.0;
        if(dV_ijk_np1half<0.0){//moving in the negative direction
          dA2UpWindGrad=(grid.dLocalGridOld[grid.nE][i][j+1][k]
            -grid.dLocalGridOld[grid.nE][i][j][k])/(grid.dLocalGridOld[grid.nDTheta][0][j+1][0]
            +grid.dLocalGridOld[grid.nDTheta][0][j][0])*2.0;
        }
        else{//moving in the positive direction
          dA2UpWindGrad=(grid.dLocalGridOld[grid.nE][i][j][k]
            -grid.dLocalGridOld[grid.nE][i][j-1][k])/(grid.dLocalGridOld[grid.nDTheta][0][j][0]
            +grid.dLocalGridOld[grid.nDTheta][0][j-1][0])*2.0;
        }
        double dA2=dV_ijk_np1half/dR_i_n*((1.0-parameters.dDonorFrac)*dA2CenGrad
          +parameters.dDonorFrac*dA2UpWindGrad);
          
        //Calcualte dS2
        dP_ijk_n=grid.dLocalGridOld[grid.nP][i][j][k];
        #if VISCOUS_ENERGY_EQ==1
          dP_ijk_n+=grid.dLocalGridOld[grid.nQ1][i][j][k];
        #endif
        double dS2=dP_ijk_n/(grid.dLocalGridOld[grid.nD][i][j][k]*dR_i_n
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]*grid.dLocalGridOld[grid.nDTheta][0][j][0])
          *(dVSinTheta_ijp1halfk_np1half-dVSinTheta_ijm1halfk_np1half);
        
        //Calcualte dA3
        double dA3CenGrad=(dE_ijkp1half_n-dE_ijkm1half_n)/grid.dLocalGridOld[grid.nDPhi][0][0][k];
        double dA3UpWindGrad=0.0;
        if(dW_ijk_np1half<0.0){//moving in the negative direction
          dA3UpWindGrad=(grid.dLocalGridOld[grid.nE][i][j][k+1]
            -grid.dLocalGridOld[grid.nE][i][j][k])/(grid.dLocalGridOld[grid.nDPhi][0][0][k+1]
            +grid.dLocalGridOld[grid.nDPhi][0][0][k])*2.0;
        }
        else{//moving in the positive direction
          dA3UpWindGrad=(grid.dLocalGridOld[grid.nE][i][j][k]
            -grid.dLocalGridOld[grid.nE][i][j][k-1])/(grid.dLocalGridOld[grid.nDPhi][0][0][k]
            +grid.dLocalGridOld[grid.nDPhi][0][0][k-1])*2.0;
        }
        double dA3=dW_ijk_np1half/(dR_i_n*grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0])*
          ((1.0-parameters.dDonorFrac)*dA3CenGrad+parameters.dDonorFrac*dA3UpWindGrad);
        
        //Calcualte dS3
        dP_ijk_n=grid.dLocalGridOld[grid.nP][i][j][k];
        #if VISCOUS_ENERGY_EQ==1
          dP_ijk_n+=grid.dLocalGridOld[grid.nQ2][i][j][k];
        #endif
        double dS3=dP_ijk_n/(grid.dLocalGridOld[grid.nD][i][j][k]*dR_i_n
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]*grid.dLocalGridOld[grid.nDPhi][0][0][k])
          *(dW_ijkp1half_np1half-dW_ijkm1half_np1half);
        
        //Calculate dS4
        double dTGrad_im1half=(dT4_ijk_n-dT4_im1jk_n)/(grid.dLocalGridOld[grid.nDM][i][0][0]
          +grid.dLocalGridOld[grid.nDM][i-1][0][0])*2.0;
        double dGrad_ip1half=-3.0*dRSq_ip1half*dT4_ijk_n/(8.0*parameters.dPi);/**\BC
          Missing grid.dLocalGridOld[grid.nT][i+1][0][0]*/
        double dGrad_im1half=dRhoAve_im1half*dR_im1half_4/(dKappa_im1halfjk_n*dRho_im1halfjk)
          *dTGrad_im1half;
        double dS4=16.0*parameters.dPi*parameters.dPi*grid.dLocalGridOld[grid.nDenAve][i][0][0]
          *(dGrad_ip1half-dGrad_im1half)/grid.dLocalGridOld[grid.nDM][i][0][0];
        
        //Calculate dS5
        double dTGrad_jp1half=(dT4_ijp1k-dT4_ijk_n)/(grid.dLocalGridOld[grid.nDTheta][0][j+1][0]
          +grid.dLocalGridOld[grid.nDTheta][0][j][0])*2.0;
        double dTGrad_jm1half=(dT4_ijk_n-dT4_ijm1k)/(grid.dLocalGridOld[grid.nDTheta][0][j][0]
          +grid.dLocalGridOld[grid.nDTheta][0][j-1][0])*2.0;;
        double dGrad_jp1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt][0]
          /(dKappa_ijp1halfk_n*dRho_ijp1halfk*dR_i_n)*dTGrad_jp1half;
        double dGrad_jm1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt-1][0]
          /(dKappa_ijm1halfk_n*dRho_ijm1halfk*dR_i_n)*dTGrad_jm1half;;
        double dS5=(dGrad_jp1half-dGrad_jm1half)/(grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]
          *dR_i_n*grid.dLocalGridOld[grid.nDTheta][0][j][0]);
        
        //Calculate dS6
        double dTGrad_kp1half=(dT_ijkp1_4-dT4_ijk_n)/(grid.dLocalGridOld[grid.nDPhi][0][0][k+1]
          +grid.dLocalGridOld[grid.nDPhi][0][0][k])*2.0;
        double dTGrad_km1half=(dT4_ijk_n-dT_ijkm1_4)/(grid.dLocalGridOld[grid.nDPhi][0][0][k]
          +grid.dLocalGridOld[grid.nDPhi][0][0][k-1])*2.0;;
        double dGrad_kp1half=dTGrad_kp1half/(dKappa_ijkp1half*dRho_ijkp1half*dR_i_n);
        double dGrad_km1half=dTGrad_km1half/(dKappa_ijkm1half*dRho_ijkm1half*dR_i_n);
        double dS6=(dGrad_kp1half-dGrad_km1half)/(dR_i_n
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]*grid.dLocalGridOld[grid.nDPhi][0][0][k]);

        //calculate new energy
        grid.dLocalGridNew[grid.nE][i][j][k]=grid.dLocalGridOld[grid.nE][i][j][k]
          -time.dDeltat_n*(4.0*parameters.dPi*grid.dLocalGridOld[grid.nDenAve][i][0][0]*(dA1+dS1)
          +dA2+dS2+dA3+dS3
          -4.0*parameters.dSigma/(3.0*grid.dLocalGridOld[grid.nD][i][j][k])*(dS4+dS5+dS6));
        
        if(grid.dLocalGridNew[grid.nE][i][j][k]<0.0){
          
          #if SIGNEGENG==1
            raise(SIGINT);
          #endif
          
          std::stringstream ssTemp;
          ssTemp<<__FILE__<<":"<<__FUNCTION__<<":"<<__LINE__<<":"<<procTop.nRank
            <<": negative energy calculated in , ("<<i<<","<<j<<","<<k<<")\n";
          throw exception2(ssTemp.str(),CALCULATION);
          
        }
      }
    }
  }
}
void calNewE_RTP_NA_LES(Grid &grid, Parameters &parameters, Time &time, ProcTop &procTop){
  int i;
  int j;
  int k;
  int nIInt;
  int nJInt;
  int nKInt;
  double dDM_ip1half;
  double dDM_im1half;
  double dDelTheta_jp1half;
  double dDelTheta_jm1half;
  double dDelPhi_kp1half;
  double dDelPhi_km1half;
  double dU_ijk_np1half;
  double dU_ijp1halfk_np1half;
  double dU_ijm1halfk_np1half;
  double dU_ijkp1half_np1half;
  double dU_ijkm1half_np1half;
  double dV_ijk_np1half;
  double dV_ip1halfjk_np1half;
  double dV_im1halfjk_np1half;
  double dV_ijkp1half_np1half;
  double dV_ijkm1half_np1half;
  double dW_ijk_np1half;
  double dW_ijkp1half_np1half;
  double dW_ijkm1half_np1half;
  double dW_ip1halfjk_np1half;
  double dW_im1halfjk_np1half;
  double dW_ijp1halfk_np1half;
  double dW_ijm1halfk_np1half;
  double dU0_i_np1half;
  double dE_ip1halfjk_n;
  double dE_im1halfjk_n;
  double dR_ip1_np1half;
  double dRSq_ip1_np1half;
  double dR_i_np1half;
  double dR_im1half_np1half;
  double dR_ip1half_np1half;
  double dRSq_i_np1half;
  double dR_im1_np1half;
  double dRSq_im1_np1half;
  double dRSq_ip1half_np1half;
  double dR4_ip1half_np1half;
  double dRSq_im1half_np1half;
  double dR4_im1half_np1half;
  double dE_ijp1halfk_n;
  double dE_ijm1halfk_n;
  double dUmU0_ijk_np1half;
  double dVSinTheta_ijp1halfk_np1half;
  double dVSinTheta_ijm1halfk_np1half;
  double dE_ijkp1half_n;
  double dE_ijkm1half_n;
  double dRhoAve_ip1half_n;
  double dRhoAve_im1half_n;
  double dRho_ip1halfjk_n;
  double dRho_im1halfjk_n;
  double dRho_ijp1halfk_n;
  double dRho_ijm1halfk_n;
  double dRho_ijkp1half_n;
  double dRho_ijkm1half_n;
  double dTSq_ip1jk_n;
  double dT4_ip1jk_n;
  double dTSq_ijk_n;
  double dT4_ijk_n;
  double dTSq_im1jk_n;
  double dT4_im1jk_n;
  double dTSq_ijp1k_n;
  double dT4_ijp1k_n;
  double dTSq_ijm1k_n;
  double dT4_ijm1k_n;
  double dTSq_ijkp1_n;
  double dT4_ijkp1_n;
  double dTSq_ijkm1_n;
  double dT4_ijkm1_n;
  double dKappa_ip1halfjk_n;
  double dKappa_im1halfjk_n;
  double dKappa_ijp1halfk_n;
  double dKappa_ijm1halfk_n;
  double dKappa_ijkp1half;
  double dKappa_ijkm1half;
  double dA1CenGrad;
  double dA1UpWindGrad;
  double dU_U0_Diff;
  double dA1;
  double dUR2_im1halfjk_np1half;
  double dUR2_ip1halfjk_np1half;
  double dP_ijk_n;
  double dS1;
  double dA2CenGrad;
  double dA2UpWindGrad;
  double dA2;
  double dS2;
  double dA3CenGrad;
  double dA3UpWindGrad;
  double dA3;
  double dS3;
  double dTGrad_ip1half;
  double dTGrad_im1half;
  double dGrad_ip1half;
  double dGrad_im1half;
  double dS4;
  double dTGrad_jp1half;
  double dTGrad_jm1half;
  double dGrad_jp1half;
  double dGrad_jm1half;
  double dS5;
  double dTGrad_kp1half;
  double dTGrad_km1half;
  double dGrad_kp1half;
  double dGrad_km1half;
  double dS6;
  /*double dDivU_ijk_np1half;
  double dDUDM_ijk_np1half;
  double dDVDTheta_ijk_np1half;
  double dDUDPhi_ijk_np1half;
  double dDVDPhi_ijk_np1half;
  double dDWDM_ijk_np1half;
  double dDWDTheta_ijk_np1half;
  double dDWDPhi_ijk_np1half;
  double dTau_rr_ijk_np1half;
  double dTau_tt_ijk_np1half;
  double dTau_rt_ijk_np1half;
  double dTau_pp_ijk_np1half;
  double dTau_tp_ijk_np1half;
  double dTau_rp_ijk_np1half;
  double dDVDM_ijk_np1half;
  double dDUDTheta_ijk_np1half;*/
  double dT1;
  double dT2;
  double dT3;
  double dEGrad_ip1halfjk_np1half;
  double dEGrad_im1halfjk_np1half;
  double dEGrad_ijp1halfk_np1half;
  double dEGrad_ijm1halfk_np1half;
  double dEGrad_ijkp1half_np1half;
  double dEGrad_ijkm1half_np1half;
  double dE_ip1jk_np1half;
  double dE_ijk_np1half;
  double dE_im1jk_np1half;
  double dEddyVisc_ip1halfjk_n;
  double dEddyVisc_im1halfjk_n;
  double dEddyVisc_ijp1halfk_n;
  double dEddyVisc_ijm1halfk_n;
  double dEddyVisc_ijkm1half_n;
  double dEddyVisc_ijkp1half_n;
  double VSinTheta_ijp1halfk_np1half;
  double dEddyViscosityTerms;
  double dPiSq=parameters.dPi*parameters.dPi;
  for(i=grid.nStartUpdateExplicit[grid.nE][0];i<grid.nEndUpdateExplicit[grid.nE][0];i++){
    
    //calculate i for interface centered quantities
    nIInt=i+grid.nCenIntOffset[0];
    dR_ip1half_np1half=(grid.dLocalGridOld[grid.nR][nIInt][0][0]
      +grid.dLocalGridNew[grid.nR][nIInt][0][0])*0.5;
    dR_im1half_np1half=(grid.dLocalGridOld[grid.nR][nIInt-1][0][0]
      +grid.dLocalGridNew[grid.nR][nIInt-1][0][0])*0.5;
    dR_ip1_np1half=(grid.dLocalGridOld[grid.nR][nIInt+1][0][0]
      +grid.dLocalGridOld[grid.nR][nIInt][0][0]+grid.dLocalGridNew[grid.nR][nIInt+1][0][0]
      +grid.dLocalGridNew[grid.nR][nIInt][0][0])*0.25;
    dRSq_ip1_np1half=dR_ip1_np1half*dR_ip1_np1half;
    dR_im1_np1half=(grid.dLocalGridOld[grid.nR][nIInt-1][0][0]
      +grid.dLocalGridOld[grid.nR][nIInt-2][0][0]+grid.dLocalGridNew[grid.nR][nIInt-1][0][0]
      +grid.dLocalGridNew[grid.nR][nIInt-2][0][0])*0.25;
    dRSq_im1_np1half=dR_im1_np1half*dR_im1_np1half;
    dR_i_np1half=(dR_ip1half_np1half+dR_im1half_np1half)*0.5;
    dRSq_i_np1half=dR_i_np1half*dR_i_np1half;
    dRSq_ip1half_np1half=dR_ip1half_np1half*dR_ip1half_np1half;
    dR4_ip1half_np1half=dRSq_ip1half_np1half*dRSq_ip1half_np1half;
    dRSq_im1half_np1half=dR_im1half_np1half*dR_im1half_np1half;
    dR4_im1half_np1half=dRSq_im1half_np1half*dRSq_im1half_np1half;
    dRhoAve_ip1half_n=(grid.dLocalGridOld[grid.nDenAve][i][0][0]
      +grid.dLocalGridOld[grid.nDenAve][i+1][0][0])*0.5;
    dRhoAve_im1half_n=(grid.dLocalGridOld[grid.nDenAve][i][0][0]
      +grid.dLocalGridOld[grid.nDenAve][i-1][0][0])*0.5;
    dU0_i_np1half=(grid.dLocalGridNew[grid.nU0][nIInt][0][0]
      +grid.dLocalGridNew[grid.nU0][nIInt-1][0][0])*0.5;
    dDM_ip1half=(grid.dLocalGridOld[grid.nDM][i][0][0]+grid.dLocalGridOld[grid.nDM][i+1][0][0])*0.5;
    dDM_im1half=(grid.dLocalGridOld[grid.nDM][i][0][0]+grid.dLocalGridOld[grid.nDM][i-1][0][0])*0.5;
    
    for(j=grid.nStartUpdateExplicit[grid.nE][1];j<grid.nEndUpdateExplicit[grid.nE][1];j++){
      
      //calculate j for interface centered quantities
      nJInt=j+grid.nCenIntOffset[1];
      dDelTheta_jp1half=(grid.dLocalGridOld[grid.nDTheta][0][j][0]
        +grid.dLocalGridOld[grid.nDTheta][0][j+1][0])*0.5;
      dDelTheta_jm1half=(grid.dLocalGridOld[grid.nDTheta][0][j][0]
        +grid.dLocalGridOld[grid.nDTheta][0][j-1][0])*0.5;
      
      for(k=grid.nStartUpdateExplicit[grid.nE][2];k<grid.nEndUpdateExplicit[grid.nE][2];k++){
        
        //calculate k for interface centered quantities
        nKInt=k+grid.nCenIntOffset[2];
        dDelPhi_kp1half=(grid.dLocalGridOld[grid.nDPhi][0][0][k]
          +grid.dLocalGridOld[grid.nDPhi][0][0][k+1])*0.5;
        dDelPhi_km1half=(grid.dLocalGridOld[grid.nDPhi][0][0][k]
          +grid.dLocalGridOld[grid.nDPhi][0][0][k-1])*0.5;
        
        //Calculate interpolated quantities
        dU_ijk_np1half=(grid.dLocalGridNew[grid.nU][nIInt][j][k]
          +grid.dLocalGridNew[grid.nU][nIInt-1][j][k])*0.5;
        dU_ijp1halfk_np1half=(grid.dLocalGridNew[grid.nU][nIInt][j+1][k]
          +grid.dLocalGridNew[grid.nU][nIInt-1][j+1][k]+grid.dLocalGridNew[grid.nU][nIInt][j][k]
          +grid.dLocalGridNew[grid.nU][nIInt-1][j][k])*0.25;
        dU_ijm1halfk_np1half=(grid.dLocalGridNew[grid.nU][nIInt][j-1][k]
          +grid.dLocalGridNew[grid.nU][nIInt-1][j-1][k]+grid.dLocalGridNew[grid.nU][nIInt][j][k]
          +grid.dLocalGridNew[grid.nU][nIInt-1][j][k])*0.25;
        dU_ijkp1half_np1half=(grid.dLocalGridNew[grid.nU][nIInt][j][k]
          +grid.dLocalGridNew[grid.nU][nIInt][j][k+1]+grid.dLocalGridNew[grid.nU][nIInt-1][j][k]
          +grid.dLocalGridNew[grid.nU][nIInt-1][j][k+1])*0.25;
        dU_ijkm1half_np1half=(grid.dLocalGridNew[grid.nU][nIInt][j][k]
          +grid.dLocalGridNew[grid.nU][nIInt][j][k-1]+grid.dLocalGridNew[grid.nU][nIInt-1][j][k]
          +grid.dLocalGridNew[grid.nU][nIInt-1][j][k-1])*0.25;
        dV_ijk_np1half=(grid.dLocalGridNew[grid.nV][i][nJInt][k]
          +grid.dLocalGridNew[grid.nV][i][nJInt-1][k])*0.5;
        dV_ip1halfjk_np1half=(grid.dLocalGridNew[grid.nV][i+1][nJInt][k]
          +grid.dLocalGridNew[grid.nV][i+1][nJInt-1][k]+grid.dLocalGridNew[grid.nV][i][nJInt][k]
          +grid.dLocalGridNew[grid.nV][i][nJInt-1][k])*0.25;
        dV_im1halfjk_np1half=(grid.dLocalGridNew[grid.nV][i][nJInt][k]
          +grid.dLocalGridNew[grid.nV][i][nJInt-1][k]+grid.dLocalGridNew[grid.nV][i-1][nJInt][k]
          +grid.dLocalGridNew[grid.nV][i-1][nJInt-1][k])*0.25;
        dV_ijkp1half_np1half=(grid.dLocalGridNew[grid.nV][i][nJInt][k+1]
          +grid.dLocalGridNew[grid.nV][i][nJInt-1][k+1]+grid.dLocalGridNew[grid.nV][i][nJInt][k]
          +grid.dLocalGridNew[grid.nV][i][nJInt-1][k])*0.25;
        dV_ijkm1half_np1half=(grid.dLocalGridNew[grid.nV][i][nJInt][k]
          +grid.dLocalGridNew[grid.nV][i][nJInt-1][k]+grid.dLocalGridNew[grid.nV][i][nJInt][k-1]
          +grid.dLocalGridNew[grid.nV][i][nJInt-1][k-1])*0.25;
        dW_ijk_np1half=(grid.dLocalGridNew[grid.nW][i][j][nKInt]
          +grid.dLocalGridNew[grid.nW][i][j][nKInt-1])*0.5;
        dW_ijkp1half_np1half=(grid.dLocalGridNew[grid.nW][i][j][nKInt]);
        dW_ijkm1half_np1half=(grid.dLocalGridNew[grid.nW][i][j][nKInt-1]);
        dW_ip1halfjk_np1half=(grid.dLocalGridNew[grid.nW][i+1][j][nKInt]
          +grid.dLocalGridNew[grid.nW][i+1][j][nKInt-1]+grid.dLocalGridNew[grid.nW][i][j][nKInt]
          +grid.dLocalGridNew[grid.nW][i][j][nKInt-1])*0.25;
        dW_im1halfjk_np1half=(grid.dLocalGridNew[grid.nW][i][j][nKInt]
          +grid.dLocalGridNew[grid.nW][i][j][nKInt-1]+grid.dLocalGridNew[grid.nW][i-1][j][nKInt]
          +grid.dLocalGridNew[grid.nW][i-1][j][nKInt-1])*0.25;
        dW_ijp1halfk_np1half=(grid.dLocalGridNew[grid.nW][i][j+1][nKInt]
          +grid.dLocalGridNew[grid.nW][i][j+1][nKInt-1]+grid.dLocalGridNew[grid.nW][i][j][nKInt]
          +grid.dLocalGridNew[grid.nW][i][j][nKInt-1])*0.25;
        dW_ijm1halfk_np1half=(grid.dLocalGridNew[grid.nW][i][j][nKInt]
          +grid.dLocalGridNew[grid.nW][i][j][nKInt-1]+grid.dLocalGridNew[grid.nW][i][j-1][nKInt]
          +grid.dLocalGridNew[grid.nW][i][j-1][nKInt-1])*0.25;
        dE_ip1halfjk_n=(grid.dLocalGridOld[grid.nE][i+1][j][k]
          +grid.dLocalGridOld[grid.nE][i][j][k])*0.5;
        dE_im1halfjk_n=(grid.dLocalGridOld[grid.nE][i][j][k]
          +grid.dLocalGridOld[grid.nE][i-1][j][k])*0.5;
        dE_ijp1halfk_n=(grid.dLocalGridOld[grid.nE][i][j+1][k]
          +grid.dLocalGridOld[grid.nE][i][j][k])*0.5;
        dE_ijm1halfk_n=(grid.dLocalGridOld[grid.nE][i][j][k]
          +grid.dLocalGridOld[grid.nE][i][j-1][k])*0.5;
        dE_ijkp1half_n=(grid.dLocalGridOld[grid.nE][i][j][k+1]+grid.dLocalGridOld[grid.nE][i][j][k])
          *0.5;
        dE_ijkm1half_n=(grid.dLocalGridOld[grid.nE][i][j][k-1]+grid.dLocalGridOld[grid.nE][i][j][k])
          *0.5;
        dRho_ip1halfjk_n=(grid.dLocalGridOld[grid.nD][i+1][j][k]
          +grid.dLocalGridOld[grid.nD][i][j][k])*0.5;
        dRho_im1halfjk_n=(grid.dLocalGridOld[grid.nD][i][j][k]
          +grid.dLocalGridOld[grid.nD][i-1][j][k])*0.5;
        dRho_ijp1halfk_n=(grid.dLocalGridOld[grid.nD][i][j+1][k]
          +grid.dLocalGridOld[grid.nD][i][j][k])*0.5;
        dRho_ijm1halfk_n=(grid.dLocalGridOld[grid.nD][i][j][k]
          +grid.dLocalGridOld[grid.nD][i][j-1][k])*0.5;
        dRho_ijkp1half_n=(grid.dLocalGridOld[grid.nD][i][j][k+1]
          +grid.dLocalGridOld[grid.nD][i][j][k])*0.5;
        dRho_ijkm1half_n=(grid.dLocalGridOld[grid.nD][i][j][k]
          +grid.dLocalGridOld[grid.nD][i][j][k-1])*0.5;
        dEddyVisc_ip1halfjk_n=(grid.dLocalGridOld[grid.nEddyVisc][i+1][j][k]
          +grid.dLocalGridOld[grid.nEddyVisc][i][j][k])*0.5;
        dEddyVisc_im1halfjk_n=(grid.dLocalGridOld[grid.nEddyVisc][i-1][j][k]
          +grid.dLocalGridOld[grid.nEddyVisc][i][j][k])*0.5;
        dEddyVisc_ijp1halfk_n=(grid.dLocalGridOld[grid.nEddyVisc][i][j+1][k]
        +grid.dLocalGridOld[grid.nEddyVisc][i][j][k])*0.5;
        dEddyVisc_ijm1halfk_n=(grid.dLocalGridOld[grid.nEddyVisc][i][j-1][k]
          +grid.dLocalGridOld[grid.nEddyVisc][i][j][k])*0.5;
        dEddyVisc_ijkp1half_n=(grid.dLocalGridOld[grid.nEddyVisc][i][j][k+1]
          +grid.dLocalGridOld[grid.nEddyVisc][i][j][k])*0.5;
        dEddyVisc_ijkm1half_n=(grid.dLocalGridOld[grid.nEddyVisc][i][j][k-1]
          +grid.dLocalGridOld[grid.nEddyVisc][i][j][k])*0.5;
        
        //calculate derived quantities
        dVSinTheta_ijp1halfk_np1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt][0]
          *grid.dLocalGridNew[grid.nV][i][nJInt][k];
        dVSinTheta_ijm1halfk_np1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt-1][0]
          *grid.dLocalGridNew[grid.nV][i][nJInt-1][k];
        dUR2_im1halfjk_np1half=grid.dLocalGridNew[grid.nU][nIInt-1][j][k]*dRSq_im1half_np1half;
        dUR2_ip1halfjk_np1half=grid.dLocalGridNew[grid.nU][nIInt][j][k]*dRSq_ip1half_np1half;
        dTSq_ip1jk_n=grid.dLocalGridOld[grid.nT][i+1][j][k]*grid.dLocalGridOld[grid.nT][i+1][j][k];
        dT4_ip1jk_n=dTSq_ip1jk_n*dTSq_ip1jk_n;
        dTSq_ijk_n=grid.dLocalGridOld[grid.nT][i][j][k]*grid.dLocalGridOld[grid.nT][i][j][k];
        dT4_ijk_n=dTSq_ijk_n*dTSq_ijk_n;
        dTSq_im1jk_n=grid.dLocalGridOld[grid.nT][i-1][j][k]*grid.dLocalGridOld[grid.nT][i-1][j][k];
        dT4_im1jk_n=dTSq_im1jk_n*dTSq_im1jk_n;
        dTSq_ijp1k_n=grid.dLocalGridOld[grid.nT][i][j+1][k]*grid.dLocalGridOld[grid.nT][i][j+1][k];
        dT4_ijp1k_n=dTSq_ijp1k_n*dTSq_ijp1k_n;
        dTSq_ijm1k_n=grid.dLocalGridOld[grid.nT][i][j-1][k]*grid.dLocalGridOld[grid.nT][i][j-1][k];
        dT4_ijm1k_n=dTSq_ijm1k_n*dTSq_ijm1k_n;
        dTSq_ijkp1_n=grid.dLocalGridOld[grid.nT][i][j][k+1]*grid.dLocalGridOld[grid.nT][i][j][k+1];
        dT4_ijkp1_n=dTSq_ijkp1_n*dTSq_ijkp1_n;
        dTSq_ijkm1_n=grid.dLocalGridOld[grid.nT][i][j][k-1]*grid.dLocalGridOld[grid.nT][i][j][k-1];
        dT4_ijkm1_n=dTSq_ijkm1_n*dTSq_ijkm1_n;
        dKappa_ip1halfjk_n=(dT4_ip1jk_n+dT4_ijk_n)/(dT4_ijk_n
          /grid.dLocalGridOld[grid.nKappa][i][j][k]+dT4_ip1jk_n
          /grid.dLocalGridOld[grid.nKappa][i+1][j][k]);
        dKappa_im1halfjk_n=(dT4_im1jk_n+dT4_ijk_n)/(dT4_ijk_n
          /grid.dLocalGridOld[grid.nKappa][i][j][k]+dT4_im1jk_n
          /grid.dLocalGridOld[grid.nKappa][i-1][j][k]);
        dKappa_ijp1halfk_n=(dT4_ijp1k_n+dT4_ijk_n)/(dT4_ijk_n
          /grid.dLocalGridOld[grid.nKappa][i][j][k]+dT4_ijp1k_n
          /grid.dLocalGridOld[grid.nKappa][i][j+1][k]);
        dKappa_ijm1halfk_n=(dT4_ijm1k_n+dT4_ijk_n)/(dT4_ijk_n
          /grid.dLocalGridOld[grid.nKappa][i][j][k]+dT4_ijm1k_n
          /grid.dLocalGridOld[grid.nKappa][i][j-1][k]);
        dKappa_ijkp1half=(dT4_ijkp1_n+dT4_ijk_n)/(dT4_ijkp1_n
          /grid.dLocalGridOld[grid.nKappa][i][j][k+1]+dT4_ijk_n
          /grid.dLocalGridOld[grid.nKappa][i][j][k]);
        dKappa_ijkm1half=(dT4_ijkm1_n+dT4_ijk_n)/(dT4_ijkm1_n
          /grid.dLocalGridOld[grid.nKappa][i][j][k-1]+dT4_ijk_n
          /grid.dLocalGridOld[grid.nKappa][i][j][k]);
        dP_ijk_n=grid.dLocalGridOld[grid.nP][i][j][k];
        #if VISCOUS_ENERGY_EQ==1
          dP_ijk_n=dP_ijk_n+grid.dLocalGridOld[grid.nQ0][i][j][k]
            +grid.dLocalGridOld[grid.nQ1][i][j][k]+grid.dLocalGridOld[grid.nQ2][i][j][k];
        #endif
        
        //Calcuate dA1
        dA1CenGrad=(dE_ip1halfjk_n-dE_im1halfjk_n)/grid.dLocalGridOld[grid.nDM][i][0][0];
        dA1UpWindGrad=0.0;
        dUmU0_ijk_np1half=(dU_ijk_np1half-dU0_i_np1half);
        if(dUmU0_ijk_np1half<0.0){//moving in the negative direction
          dA1UpWindGrad=(grid.dLocalGridOld[grid.nE][i+1][j][k]
            -grid.dLocalGridOld[grid.nE][i][j][k])/(grid.dLocalGridOld[grid.nDM][i+1][0][0]
            +grid.dLocalGridOld[grid.nDM][i][0][0])*2.0;
        }
        else{//moving in the postive direction
          dA1UpWindGrad=(grid.dLocalGridOld[grid.nE][i][j][k]
            -grid.dLocalGridOld[grid.nE][i-1][j][k])/(grid.dLocalGridOld[grid.nDM][i][0][0]
            +grid.dLocalGridOld[grid.nDM][i-1][0][0])*2.0;
        }
        dA1=dUmU0_ijk_np1half*dRSq_i_np1half*((1.0-parameters.dDonorFrac)*dA1CenGrad
          +parameters.dDonorFrac*dA1UpWindGrad);
        
        //calculate dS1
        dS1=dP_ijk_n/grid.dLocalGridOld[grid.nD][i][j][k]
          *(dUR2_ip1halfjk_np1half-dUR2_im1halfjk_np1half)/grid.dLocalGridOld[grid.nDM][i][0][0];
        
        //Calcualte dA2
        dA2CenGrad=(dE_ijp1halfk_n-dE_ijm1halfk_n)/grid.dLocalGridOld[grid.nDTheta][0][j][0];
        dA2UpWindGrad=0.0;
        if(dV_ijk_np1half<0.0){//moving in the negative direction
          dA2UpWindGrad=(grid.dLocalGridOld[grid.nE][i][j+1][k]
            -grid.dLocalGridOld[grid.nE][i][j][k])/(grid.dLocalGridOld[grid.nDTheta][0][j+1][0]
            +grid.dLocalGridOld[grid.nDTheta][0][j][0])*2.0;
        }
        else{//moving in the positive direction
          dA2UpWindGrad=(grid.dLocalGridOld[grid.nE][i][j][k]
            -grid.dLocalGridOld[grid.nE][i][j-1][k])/(grid.dLocalGridOld[grid.nDTheta][0][j][0]
            +grid.dLocalGridOld[grid.nDTheta][0][j-1][0])*2.0;
        }
        dA2=dV_ijk_np1half/dR_i_np1half*((1.0-parameters.dDonorFrac)*dA2CenGrad
          +parameters.dDonorFrac*dA2UpWindGrad);
          
        //Calcualte dS2
        dS2=dP_ijk_n/(grid.dLocalGridOld[grid.nD][i][j][k]*dR_i_np1half
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]*grid.dLocalGridOld[grid.nDTheta][0][j][0])
          *(dVSinTheta_ijp1halfk_np1half-dVSinTheta_ijm1halfk_np1half);
        
        //Calcualte dA3
        dA3CenGrad=(dE_ijkp1half_n-dE_ijkm1half_n)/grid.dLocalGridOld[grid.nDPhi][0][0][k];
        dA3UpWindGrad=0.0;
        if(dW_ijk_np1half<0.0){//moving in the negative direction
          dA3UpWindGrad=(grid.dLocalGridOld[grid.nE][i][j][k+1]
            -grid.dLocalGridOld[grid.nE][i][j][k])/
            (grid.dLocalGridOld[grid.nDPhi][0][0][k+1]+grid.dLocalGridOld[grid.nDPhi][0][0][k])*2.0;
        }
        else{//moving in the positive direction
          dA3UpWindGrad=(grid.dLocalGridOld[grid.nE][i][j][k]
            -grid.dLocalGridOld[grid.nE][i][j][k-1])/
            (grid.dLocalGridOld[grid.nDPhi][0][0][k]+grid.dLocalGridOld[grid.nDPhi][0][0][k-1])*2.0;
        }
        dA3=dW_ijk_np1half/(dR_i_np1half*grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0])*
          ((1.0-parameters.dDonorFrac)*dA3CenGrad+parameters.dDonorFrac*dA3UpWindGrad);
        
        //Calcualte dS3
        dS3=dP_ijk_n/(grid.dLocalGridOld[grid.nD][i][j][k]*dR_i_np1half
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]*grid.dLocalGridOld[grid.nDPhi][0][0][k])
          *(dW_ijkp1half_np1half-dW_ijkm1half_np1half);
        
        //Calculate dS4
        dTGrad_ip1half=(dT4_ip1jk_n-dT4_ijk_n)/(grid.dLocalGridOld[grid.nDM][i+1][0][0]
          +grid.dLocalGridOld[grid.nDM][i][0][0])*2.0;
        dTGrad_im1half=(dT4_ijk_n-dT4_im1jk_n)/(grid.dLocalGridOld[grid.nDM][i][0][0]
          +grid.dLocalGridOld[grid.nDM][i-1][0][0])*2.0;
        dGrad_ip1half=dRhoAve_ip1half_n*dR4_ip1half_np1half/(dKappa_ip1halfjk_n
          *dRho_ip1halfjk_n)*dTGrad_ip1half;
        dGrad_im1half=dRhoAve_im1half_n*dR4_im1half_np1half/(dKappa_im1halfjk_n
          *dRho_im1halfjk_n)*dTGrad_im1half;
        dS4=16.0*dPiSq*grid.dLocalGridOld[grid.nDenAve][i][0][0]
          *(dGrad_ip1half-dGrad_im1half)/grid.dLocalGridOld[grid.nDM][i][0][0];
        
        //Calculate dS5
        dTGrad_jp1half=(dT4_ijp1k_n-dT4_ijk_n)/(grid.dLocalGridOld[grid.nDTheta][0][j+1][0]
          +grid.dLocalGridOld[grid.nDTheta][0][j][0])*2.0;
        dTGrad_jm1half=(dT4_ijk_n-dT4_ijm1k_n)/(grid.dLocalGridOld[grid.nDTheta][0][j][0]
          +grid.dLocalGridOld[grid.nDTheta][0][j-1][0])*2.0;;
        dGrad_jp1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt][0]
          /(dKappa_ijp1halfk_n*dRho_ijp1halfk_n)*dTGrad_jp1half;
        dGrad_jm1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt-1][0]
          /(dKappa_ijm1halfk_n*dRho_ijm1halfk_n)*dTGrad_jm1half;
        dS5=(dGrad_jp1half-dGrad_jm1half)/(grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]
          *dRSq_i_np1half*grid.dLocalGridOld[grid.nDTheta][0][j][0]);
        
        //Calculate dS6
        dTGrad_kp1half=(dT4_ijkp1_n-dT4_ijk_n)/(grid.dLocalGridOld[grid.nDPhi][0][0][k+1]
          +grid.dLocalGridOld[grid.nDPhi][0][0][k])*2.0;
        dTGrad_km1half=(dT4_ijk_n-dT4_ijkm1_n)/(grid.dLocalGridOld[grid.nDPhi][0][0][k]
          +grid.dLocalGridOld[grid.nDPhi][0][0][k-1])*2.0;;
        dGrad_kp1half=dTGrad_kp1half/(dKappa_ijkp1half*dRho_ijkp1half_n);
        dGrad_km1half=dTGrad_km1half/(dKappa_ijkm1half*dRho_ijkm1half_n);
        dS6=(dGrad_kp1half-dGrad_km1half)/(dRSq_i_np1half
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]*grid.dLocalGridOld[grid.nDPhi][0][0][k]);
        
        /*//calculate DUDM_ijk_np1half
        dDUDM_ijk_np1half=4.0*parameters.dPi*grid.dLocalGridOld[grid.nDenAve][i][0][0]
          *dRSq_i_np1half*((grid.dLocalGridNew[grid.nU][nIInt][j][k]
          -grid.dLocalGridNew[grid.nU0][nIInt][0][0])-(grid.dLocalGridNew[grid.nU][nIInt-1][j][k]
          -grid.dLocalGridNew[grid.nU0][nIInt-1][0][0]))/grid.dLocalGridOld[grid.nDM][i][0][0];
        
        //calculate DVDM_ijk_np1half
        dDVDM_ijk_np1half=4.0*parameters.dPi*grid.dLocalGridOld[grid.nDenAve][i][0][0]
          *dRSq_i_np1half*(dV_ip1halfjk_np1half-dV_im1halfjk_np1half)
          /grid.dLocalGridOld[grid.nDM][i][0][0];
        
        //calculate DUDTheta_ijk_np1half
        dDUDTheta_ijk_np1half=((dU_ijp1halfk_np1half-dU0_i_np1half)-(dU_ijm1halfk_np1half
          -dU0_i_np1half))/(dR_i_np1half*grid.dLocalGridOld[grid.nDTheta][0][j][0]);
        
        //calculate DVDTheta_ijk_np1half
        dDVDTheta_ijk_np1half=(grid.dLocalGridNew[grid.nV][i][nJInt][k]
          -grid.dLocalGridNew[grid.nV][i][nJInt-1][k])/(dR_i_np1half
          *grid.dLocalGridOld[grid.nDTheta][0][j][0]);
        
        //calculate DUDPhi_ijk_np1half
        dDUDPhi_ijk_np1half=((dU_ijkp1half_np1half-dU0_i_np1half)-(dU_ijkm1half_np1half
          -dU0_i_np1half))/(dR_i_np1half*grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]
          *grid.dLocalGridOld[grid.nDPhi][0][0][k]);
        
        //calculate DVDPhi_ijk_np1half
        dDVDPhi_ijk_np1half=(dV_ijkp1half_np1half-dV_ijkm1half_np1half)/(dR_i_np1half
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]*grid.dLocalGridOld[grid.nDPhi][0][0][k]);
        
        //calculate DWDM_ijk_np1half
        dDWDM_ijk_np1half=4.0*parameters.dPi*grid.dLocalGridOld[grid.nDenAve][i][0][0]
          *dRSq_i_np1half*(dW_ip1halfjk_np1half-dW_im1halfjk_np1half)
          /grid.dLocalGridOld[grid.nDM][i][0][0];
        
        //calculate DWDTheta_ijk_np1half
        dDWDTheta_ijk_np1half=(dW_ijp1halfk_np1half-dW_ijm1halfk_np1half)/(dR_i_np1half
          *grid.dLocalGridOld[grid.nDTheta][0][j][0]);
        
        //calculate DWDPhi_ijk_np1half
        dDWDPhi_ijk_np1half=(grid.dLocalGridNew[grid.nW][i][j][nKInt]
          -grid.dLocalGridNew[grid.nW][i][j][nKInt-1])/(dR_i_np1half
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]*grid.dLocalGridOld[grid.nDPhi][0][0][k]);
        
        //cal DivU_ijk_np1half
        dDivU_ijk_np1half=dDUDM_ijk_np1half+dDVDTheta_ijk_np1half+dDWDPhi_ijk_np1half;
        
        //cal Tau_rr_ijk_np1half
        dTau_rr_ijk_np1half=2.0*grid.dLocalGridNew[grid.nEddyVisc][i][j][k]*(dDUDM_ijk_np1half
          -0.3333333333333333*dDivU_ijk_np1half);
        
        //calculate Tau_tt_ijp1halfk_np1half
        dTau_tt_ijk_np1half=2.0*grid.dLocalGridNew[grid.nEddyVisc][i][j][k]*(dDVDTheta_ijk_np1half
          -0.333333333333333*dDivU_ijk_np1half);
        
        //calculate dTau_pp_ijk_np1half
        dTau_pp_ijk_np1half=2.0*grid.dLocalGridNew[grid.nEddyVisc][i][j][k]*(dDWDPhi_ijk_np1half
          -0.3333333333333333*dDivU_ijk_np1half);
        
        //calculate Tau_rt_ijk_np1half
        dTau_rt_ijk_np1half=grid.dLocalGridNew[grid.nEddyVisc][i][j][k]*(dDVDM_ijk_np1half
          +dDUDTheta_ijk_np1half);
        
        //calculate dTau_rp_ijk_np1half
        dTau_rp_ijk_np1half=grid.dLocalGridNew[grid.nEddyVisc][i][j][k]*(dDUDPhi_ijk_np1half
          +dDWDM_ijk_np1half);
        
        //calculate dTau_tp_ijk_np1half
        dTau_tp_ijk_np1half=grid.dLocalGridNew[grid.nEddyVisc][i][j][k]*(dDVDPhi_ijk_np1half
          +dDWDTheta_ijk_np1half);
        
        //eddy viscosity terms
        dEddyViscosityTerms=(dTau_rr_ijk_np1half*dDUDM_ijk_np1half+dTau_tt_ijk_np1half
          *dDVDTheta_ijk_np1half+dTau_pp_ijk_np1half*dDWDPhi_ijk_np1half+dTau_rt_ijk_np1half
          *(dDUDTheta_ijk_np1half+dDVDM_ijk_np1half)+dTau_rp_ijk_np1half*(dDUDPhi_ijk_np1half
          +dDWDM_ijk_np1half)+dTau_tp_ijk_np1half*(dDVDPhi_ijk_np1half+dDWDTheta_ijk_np1half))
          /grid.dLocalGridOld[grid.nD][i][j][k];*/
        
        //calculate dT1
        dEGrad_ip1halfjk_np1half=dR4_ip1half_np1half*dEddyVisc_ip1halfjk_n*dRhoAve_ip1half_n
          *(grid.dLocalGridOld[grid.nE][i+1][j][k]-grid.dLocalGridOld[grid.nE][i][j][k])
          /(dRho_ip1halfjk_n*dDM_ip1half);
        dEGrad_im1halfjk_np1half=dR4_im1half_np1half*dEddyVisc_im1halfjk_n*dRhoAve_im1half_n
          *(grid.dLocalGridOld[grid.nE][i][j][k]-grid.dLocalGridOld[grid.nE][i-1][j][k])
          /(dRho_im1halfjk_n*dDM_im1half);
        dT1=16.0*dPiSq*grid.dLocalGridOld[grid.nDenAve][i][0][0]*(dEGrad_ip1halfjk_np1half
          -dEGrad_im1halfjk_np1half)/grid.dLocalGridOld[grid.nDM][i][0][0];
        
        //calculate dT2
        dEGrad_ijp1halfk_np1half=dEddyVisc_ijp1halfk_n
          *grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt][0]
          *(grid.dLocalGridOld[grid.nE][i][j+1][k]-grid.dLocalGridOld[grid.nE][i][j][k])
          /(dRho_ijp1halfk_n*dR_i_np1half*dDelTheta_jp1half);
        dEGrad_ijm1halfk_np1half=dEddyVisc_ijm1halfk_n
          *grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt-1][0]
          *(grid.dLocalGridOld[grid.nE][i][j][k]-grid.dLocalGridOld[grid.nE][i][j-1][k])
          /(dRho_ijm1halfk_n*dR_i_np1half*dDelTheta_jm1half);
        dT2=(dEGrad_ijp1halfk_np1half-dEGrad_ijm1halfk_np1half)/(dR_i_np1half
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]
          *grid.dLocalGridOld[grid.nDTheta][0][j][0]);
        
        //calculate dT3
        dEGrad_ijkp1half_np1half=dEddyVisc_ijkp1half_n*(grid.dLocalGridOld[grid.nE][i][j][k+1]
          -grid.dLocalGridOld[grid.nE][i][j][k])/(dRho_ijkp1half_n
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]*dR_i_np1half*dDelPhi_kp1half);
        dEGrad_ijkm1half_np1half=dEddyVisc_ijkm1half_n*(grid.dLocalGridOld[grid.nE][i][j][k]
          -grid.dLocalGridOld[grid.nE][i][j][k-1])/(dRho_ijkm1half_n
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]*dR_i_np1half*dDelPhi_km1half);
        dT3=(dEGrad_ijkp1half_np1half-dEGrad_ijkm1half_np1half)/(dR_i_np1half
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]
          *grid.dLocalGridOld[grid.nDPhi][0][0][k]);
        
        //eddy viscosity terms
        dEddyViscosityTerms=(dT1+dT2+dT3)/parameters.dPrt;
        
        //calculate new energy
        grid.dLocalGridNew[grid.nE][i][j][k]=grid.dLocalGridOld[grid.nE][i][j][k]-time.dDeltat_n
          *(4.0*parameters.dPi*grid.dLocalGridOld[grid.nDenAve][i][0][0]*(dA1+dS1)+dA2+dS2+dA3+dS3
          -4.0*parameters.dSigma/(3.0*grid.dLocalGridOld[grid.nD][i][j][k])*(dS4+dS5+dS6)
          -dEddyViscosityTerms);
        
        if(grid.dLocalGridNew[grid.nE][i][j][k]<0.0){
          
          #if SIGNEGENG==1
            raise(SIGINT);
          #endif
          
          std::stringstream ssTemp;
          ssTemp<<__FILE__<<":"<<__FUNCTION__<<":"<<__LINE__<<":"<<procTop.nRank
            <<": negative energy calculated in , ("<<i<<","<<j<<","<<k<<")\n";
          throw exception2(ssTemp.str(),CALCULATION);
          
        }
      }
    }
  }
  
  //ghost region 0, outter most ghost region in x1 direction
  for(i=grid.nStartGhostUpdateExplicit[grid.nE][0][0];i<grid.nEndGhostUpdateExplicit[grid.nE][0][0];i++){
    
    //calculate i for interface centered quantities
    nIInt=i+grid.nCenIntOffset[0];
    dR_ip1half_np1half=(grid.dLocalGridOld[grid.nR][nIInt][0][0]
      +grid.dLocalGridNew[grid.nR][nIInt][0][0])*0.5;
    dR_im1half_np1half=(grid.dLocalGridOld[grid.nR][nIInt-1][0][0]
      +grid.dLocalGridNew[grid.nR][nIInt-1][0][0])*0.5;
    dR_im1_np1half=(grid.dLocalGridOld[grid.nR][nIInt-1][0][0]
      +grid.dLocalGridOld[grid.nR][nIInt-2][0][0]+grid.dLocalGridNew[grid.nR][nIInt-1][0][0]
      +grid.dLocalGridNew[grid.nR][nIInt-2][0][0])*0.25;
    dRSq_im1_np1half=dR_im1_np1half*dR_im1_np1half;
    dR_i_np1half=(dR_ip1half_np1half+dR_im1half_np1half)*0.5;
    dRSq_i_np1half=dR_i_np1half*dR_i_np1half;
    dRSq_ip1half_np1half=dR_ip1half_np1half*dR_ip1half_np1half;
    dR4_ip1half_np1half=dRSq_ip1half_np1half*dRSq_ip1half_np1half;
    dRSq_im1half_np1half=dR_im1half_np1half*dR_im1half_np1half;
    dR4_im1half_np1half=dRSq_im1half_np1half*dRSq_im1half_np1half;
    dRhoAve_ip1half_n=(grid.dLocalGridOld[grid.nDenAve][i][0][0])*0.5;/*\BC missing average density
      outside model setting it to zero*/
    dRhoAve_im1half_n=(grid.dLocalGridOld[grid.nDenAve][i][0][0]
      +grid.dLocalGridOld[grid.nDenAve][i-1][0][0])*0.5;
    dU0_i_np1half=(grid.dLocalGridNew[grid.nU0][nIInt][0][0]
      +grid.dLocalGridNew[grid.nU0][nIInt-1][0][0])*0.5;
    dDM_ip1half=(grid.dLocalGridOld[grid.nDM][i][0][0])*(0.5+parameters.dAlpha
      +parameters.dAlphaExtra);/**\BC Missing \f$\Delta M_r\f$ outside model using 
      \ref Parameters.dAlpha times \f$\Delta M_r\f$ in the last zone instead.*/
    dDM_im1half=(grid.dLocalGridOld[grid.nDM][i][0][0]+grid.dLocalGridOld[grid.nDM][i-1][0][0])*0.5;
    
    for(j=grid.nStartGhostUpdateExplicit[grid.nE][0][1];
      j<grid.nEndGhostUpdateExplicit[grid.nE][0][1];j++){
      
      //calculate i for interface centered quantities
      nJInt=j+grid.nCenIntOffset[1];
      dDelTheta_jp1half=(grid.dLocalGridOld[grid.nDTheta][0][j][0]
        +grid.dLocalGridOld[grid.nDTheta][0][j+1][0])*0.5;
      dDelTheta_jm1half=(grid.dLocalGridOld[grid.nDTheta][0][j][0]
        +grid.dLocalGridOld[grid.nDTheta][0][j-1][0])*0.5;
      
      for(k=grid.nStartGhostUpdateExplicit[grid.nE][0][2];
        k<grid.nEndGhostUpdateExplicit[grid.nE][0][2];k++){
        
        nKInt=k+grid.nCenIntOffset[2];
        dDelPhi_kp1half=(grid.dLocalGridOld[grid.nDPhi][0][0][k]
          +grid.dLocalGridOld[grid.nDPhi][0][0][k+1])*0.5;
        dDelPhi_km1half=(grid.dLocalGridOld[grid.nDPhi][0][0][k]
          +grid.dLocalGridOld[grid.nDPhi][0][0][k-1])*0.5;
        
        //Calculate interpolated quantities
        dU_ijk_np1half=(grid.dLocalGridNew[grid.nU][nIInt][j][k]
          +grid.dLocalGridNew[grid.nU][nIInt-1][j][k])*0.5;
        dU_ijp1halfk_np1half=(grid.dLocalGridNew[grid.nU][nIInt][j+1][k]
          +grid.dLocalGridNew[grid.nU][nIInt-1][j+1][k]+grid.dLocalGridNew[grid.nU][nIInt][j][k]
          +grid.dLocalGridNew[grid.nU][nIInt-1][j][k])*0.25;
        dU_ijm1halfk_np1half=(grid.dLocalGridNew[grid.nU][nIInt][j-1][k]
          +grid.dLocalGridNew[grid.nU][nIInt-1][j-1][k]+grid.dLocalGridNew[grid.nU][nIInt][j][k]
          +grid.dLocalGridNew[grid.nU][nIInt-1][j][k])*0.25;
        dU_ijkp1half_np1half=(grid.dLocalGridNew[grid.nU][nIInt][j][k]
          +grid.dLocalGridNew[grid.nU][nIInt][j][k+1]+grid.dLocalGridNew[grid.nU][nIInt-1][j][k]
          +grid.dLocalGridNew[grid.nU][nIInt-1][j][k+1])*0.25;
        dU_ijkm1half_np1half=(grid.dLocalGridNew[grid.nU][nIInt][j][k]
          +grid.dLocalGridNew[grid.nU][nIInt][j][k-1]+grid.dLocalGridNew[grid.nU][nIInt-1][j][k]
          +grid.dLocalGridNew[grid.nU][nIInt-1][j][k-1])*0.25;
        dV_ijk_np1half=(grid.dLocalGridNew[grid.nV][i][nJInt][k]
          +grid.dLocalGridNew[grid.nV][i][nJInt-1][k])*0.5;
        dV_ip1halfjk_np1half=dV_ijk_np1half;
        dV_im1halfjk_np1half=(grid.dLocalGridNew[grid.nV][i][nJInt][k]
          +grid.dLocalGridNew[grid.nV][i][nJInt-1][k]+grid.dLocalGridNew[grid.nV][i-1][nJInt][k]
          +grid.dLocalGridNew[grid.nV][i-1][nJInt-1][k])*0.25;
        dV_ijkp1half_np1half=(grid.dLocalGridNew[grid.nV][i][nJInt][k+1]
          +grid.dLocalGridNew[grid.nV][i][nJInt-1][k+1]+grid.dLocalGridNew[grid.nV][i][nJInt][k]
          +grid.dLocalGridNew[grid.nV][i][nJInt-1][k])*0.25;
        dV_ijkm1half_np1half=(grid.dLocalGridNew[grid.nV][i][nJInt][k]
          +grid.dLocalGridNew[grid.nV][i][nJInt-1][k]+grid.dLocalGridNew[grid.nV][i][nJInt][k-1]
          +grid.dLocalGridNew[grid.nV][i][nJInt-1][k-1])*0.25;
        dW_ijk_np1half=(grid.dLocalGridNew[grid.nW][i][j][nKInt]
          +grid.dLocalGridNew[grid.nW][i][j][nKInt-1])*0.5;
        dW_ijkp1half_np1half=(grid.dLocalGridNew[grid.nW][i][j][nKInt]);
        dW_ijkm1half_np1half=(grid.dLocalGridNew[grid.nW][i][j][nKInt-1]);
        dW_ip1halfjk_np1half=(grid.dLocalGridNew[grid.nW][i][j][nKInt]
          +grid.dLocalGridNew[grid.nW][i][j][nKInt-1])*0.5;/**\BC Missing W at i+1, assuming the 
          same as at i*/
        dW_im1halfjk_np1half=(grid.dLocalGridNew[grid.nW][i][j][nKInt]
          +grid.dLocalGridNew[grid.nW][i][j][nKInt-1]+grid.dLocalGridNew[grid.nW][i-1][j][nKInt]
          +grid.dLocalGridNew[grid.nW][i-1][j][nKInt-1])*0.25;
        dW_ijp1halfk_np1half=(grid.dLocalGridNew[grid.nW][i][j+1][nKInt]
          +grid.dLocalGridNew[grid.nW][i][j+1][nKInt-1]+grid.dLocalGridNew[grid.nW][i][j][nKInt]
          +grid.dLocalGridNew[grid.nW][i][j][nKInt-1])*0.25;
        dW_ijm1halfk_np1half=(grid.dLocalGridNew[grid.nW][i][j][nKInt]
          +grid.dLocalGridNew[grid.nW][i][j][nKInt-1]+grid.dLocalGridNew[grid.nW][i][j-1][nKInt]
          +grid.dLocalGridNew[grid.nW][i][j-1][nKInt-1])*0.25;
        dE_ip1halfjk_n=(grid.dLocalGridOld[grid.nE][i][j][k]);/**\BC Missing
          grid.dLocalGridOld[grid.nE][i+1][j][k] in calculation of \f$E_{i+1/2,j,k}\f$ 
          setting it equal to the value at i.*/
        dE_im1halfjk_n=(grid.dLocalGridOld[grid.nE][i][j][k]+grid.dLocalGridOld[grid.nE][i-1][j][k])
          *0.5;
        dE_ijp1halfk_n=(grid.dLocalGridOld[grid.nE][i][j+1][k]+grid.dLocalGridOld[grid.nE][i][j][k])
          *0.5;
        dE_ijm1halfk_n=(grid.dLocalGridOld[grid.nE][i][j][k]+grid.dLocalGridOld[grid.nE][i][j-1][k])
          *0.5;
        dE_ijkp1half_n=(grid.dLocalGridOld[grid.nE][i][j][k+1]+grid.dLocalGridOld[grid.nE][i][j][k])
          *0.5;
        dE_ijkm1half_n=(grid.dLocalGridOld[grid.nE][i][j][k-1]+grid.dLocalGridOld[grid.nE][i][j][k])
          *0.5;
        dRho_ip1halfjk_n=(grid.dLocalGridOld[grid.nD][i+1][j][k])*0.5;/**\BC missing density outside
          model, setting it to zero*/
        dRho_im1halfjk_n=(grid.dLocalGridOld[grid.nD][i][j][k]
          +grid.dLocalGridOld[grid.nD][i-1][j][k])*0.5;
        dRho_ijp1halfk_n=(grid.dLocalGridOld[grid.nD][i][j+1][k]
          +grid.dLocalGridOld[grid.nD][i][j][k])*0.5;
        dRho_ijm1halfk_n=(grid.dLocalGridOld[grid.nD][i][j][k]
          +grid.dLocalGridOld[grid.nD][i][j-1][k])*0.5;
        dRho_ijkp1half_n=(grid.dLocalGridOld[grid.nD][i][j][k+1]
          +grid.dLocalGridOld[grid.nD][i][j][k])*0.5;
        dRho_ijkm1half_n=(grid.dLocalGridOld[grid.nD][i][j][k]
          +grid.dLocalGridOld[grid.nD][i][j][k-1])*0.5;
        dEddyVisc_ip1halfjk_n=(grid.dLocalGridOld[grid.nEddyVisc][i][j][k])*0.5;/**\BC missing 
          eddy viscosity outside the model setting it to zero*/
        dEddyVisc_im1halfjk_n=(grid.dLocalGridOld[grid.nEddyVisc][i-1][j][k]
          +grid.dLocalGridOld[grid.nEddyVisc][i][j][k])*0.5;
        dEddyVisc_ijp1halfk_n=(grid.dLocalGridOld[grid.nEddyVisc][i][j+1][k]
        +grid.dLocalGridOld[grid.nEddyVisc][i][j][k])*0.5;
        dEddyVisc_ijm1halfk_n=(grid.dLocalGridOld[grid.nEddyVisc][i][j-1][k]
          +grid.dLocalGridOld[grid.nEddyVisc][i][j][k])*0.5;
        dEddyVisc_ijkp1half_n=(grid.dLocalGridOld[grid.nEddyVisc][i][j][k+1]
          +grid.dLocalGridOld[grid.nEddyVisc][i][j][k])*0.5;
        dEddyVisc_ijkm1half_n=(grid.dLocalGridOld[grid.nEddyVisc][i][j][k-1]
          +grid.dLocalGridOld[grid.nEddyVisc][i][j][k])*0.5;
        
        //calculate derived quantities
        VSinTheta_ijp1halfk_np1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt][0]
          *grid.dLocalGridNew[grid.nV][i][nJInt][k];
        dVSinTheta_ijm1halfk_np1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt-1][0]
          *grid.dLocalGridNew[grid.nV][i][nJInt-1][k];
        dUR2_im1halfjk_np1half=grid.dLocalGridNew[grid.nU][nIInt-1][j][k]*dRSq_im1half_np1half;
        dUR2_ip1halfjk_np1half=grid.dLocalGridNew[grid.nU][nIInt][j][k]*dRSq_ip1half_np1half;
        dTSq_ijk_n=grid.dLocalGridOld[grid.nT][i][j][k]*grid.dLocalGridOld[grid.nT][i][j][k];
        dT4_ijk_n=dTSq_ijk_n*dTSq_ijk_n;
        dTSq_im1jk_n=grid.dLocalGridOld[grid.nT][i-1][j][k]*grid.dLocalGridOld[grid.nT][i-1][j][k];
        dT4_im1jk_n=dTSq_im1jk_n*dTSq_im1jk_n;
        dTSq_ijp1k_n=grid.dLocalGridOld[grid.nT][i][j+1][k]*grid.dLocalGridOld[grid.nT][i][j+1][k];
        dT4_ijp1k_n=dTSq_ijp1k_n*dTSq_ijp1k_n;
        dTSq_ijm1k_n=grid.dLocalGridOld[grid.nT][i][j-1][k]*grid.dLocalGridOld[grid.nT][i][j-1][k];
        dT4_ijm1k_n=dTSq_ijm1k_n*dTSq_ijm1k_n;
        dTSq_ijkp1_n=grid.dLocalGridOld[grid.nT][i][j][k+1]*grid.dLocalGridOld[grid.nT][i][j][k+1];
        dT4_ijkp1_n=dTSq_ijkp1_n*dTSq_ijkp1_n;
        dTSq_ijkm1_n=grid.dLocalGridOld[grid.nT][i][j][k-1]*grid.dLocalGridOld[grid.nT][i][j][k-1];
        dT4_ijkm1_n=dTSq_ijkm1_n*dTSq_ijkm1_n;
        dKappa_im1halfjk_n=(dT4_im1jk_n+dT4_ijk_n)/(dT4_ijk_n
          /grid.dLocalGridOld[grid.nKappa][i][j][k]+dT4_im1jk_n
          /grid.dLocalGridOld[grid.nKappa][i-1][j][k]);
        dKappa_ijp1halfk_n=(dT4_ijp1k_n+dT4_ijk_n)/(dT4_ijk_n
          /grid.dLocalGridOld[grid.nKappa][i][j][k]+dT4_ijp1k_n
          /grid.dLocalGridOld[grid.nKappa][i][j+1][k]);
        dKappa_ijm1halfk_n=(dT4_ijm1k_n+dT4_ijk_n)/(dT4_ijk_n
          /grid.dLocalGridOld[grid.nKappa][i][j][k]+dT4_ijm1k_n
          /grid.dLocalGridOld[grid.nKappa][i][j-1][k]);
        dKappa_ijkp1half=(dT4_ijkp1_n+dT4_ijk_n)/(dT4_ijkp1_n
          /grid.dLocalGridOld[grid.nKappa][i][j][k+1]+dT4_ijk_n
          /grid.dLocalGridOld[grid.nKappa][i][j][k]);
        dKappa_ijkm1half=(dT4_ijkm1_n+dT4_ijk_n)/(dT4_ijkm1_n
          /grid.dLocalGridOld[grid.nKappa][i][j][k-1]+dT4_ijk_n
          /grid.dLocalGridOld[grid.nKappa][i][j][k]);
        dP_ijk_n=grid.dLocalGridOld[grid.nP][i][j][k];
        #if VISCOUS_ENERGY_EQ==1
          dP_ijk_n=dP_ijk_n+grid.dLocalGridOld[grid.nQ0][i][j][k]
            +grid.dLocalGridOld[grid.nQ1][i][j][k]+grid.dLocalGridOld[grid.nQ2][i][j][k];
        #endif
        
        //Calcuate dA1
        dA1CenGrad=(dE_ip1halfjk_n-dE_im1halfjk_n)/grid.dLocalGridOld[grid.nDM][i][0][0];
        dA1UpWindGrad=0.0;
        dU_U0_Diff=(dU_ijk_np1half-dU0_i_np1half);
        if(dU_U0_Diff<0.0){//moving in the negative direction
          dA1UpWindGrad=dA1CenGrad;/**\BC grid.dLocalGridOld[grid.nDM][i+1][0][0] and 
            grid.dLocalGridOld[grid.nE][i+1][j][k] missing in the calculation of upwind gradient in 
            dA1. Using the centered gradient instead.*/
        }
        else{//moving in the postive direction*/
          dA1UpWindGrad=(grid.dLocalGridOld[grid.nE][i][j][k]
            -grid.dLocalGridOld[grid.nE][i-1][j][k])/(grid.dLocalGridOld[grid.nDM][i][0][0]
            +grid.dLocalGridOld[grid.nDM][i-1][0][0])*2.0;
        }
        dA1=dU_U0_Diff*dRSq_i_np1half*((1.0-parameters.dDonorFrac)*dA1CenGrad
          +parameters.dDonorFrac*dA1UpWindGrad);
        
        //calculate dS1
        dS1=dP_ijk_n/grid.dLocalGridOld[grid.nD][i][j][k]
          *(dUR2_ip1halfjk_np1half-dUR2_im1halfjk_np1half)/grid.dLocalGridOld[grid.nDM][i][0][0];
        
        //Calcualte dA2
        dA2CenGrad=(dE_ijp1halfk_n-dE_ijm1halfk_n)/grid.dLocalGridOld[grid.nDTheta][0][j][0];
        dA2UpWindGrad=0.0;
        if(dV_ijk_np1half<0.0){//moving in the negative direction
          dA2UpWindGrad=(grid.dLocalGridOld[grid.nE][i][j+1][k]
            -grid.dLocalGridOld[grid.nE][i][j][k])/(grid.dLocalGridOld[grid.nDTheta][0][j+1][0]
            +grid.dLocalGridOld[grid.nDTheta][0][j][0])*2.0;
        }
        else{//moving in the positive direction
          dA2UpWindGrad=(grid.dLocalGridOld[grid.nE][i][j][k]
            -grid.dLocalGridOld[grid.nE][i][j-1][k])/(grid.dLocalGridOld[grid.nDTheta][0][j][0]
            +grid.dLocalGridOld[grid.nDTheta][0][j-1][0])*2.0;
        }
        dA2=dV_ijk_np1half/dR_i_np1half*((1.0-parameters.dDonorFrac)*dA2CenGrad
          +parameters.dDonorFrac*dA2UpWindGrad);
          
        //Calcualte dS2
        dS2=dP_ijk_n/(grid.dLocalGridOld[grid.nD][i][j][k]*dR_i_np1half
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]*grid.dLocalGridOld[grid.nDTheta][0][j][0])
          *(dVSinTheta_ijp1halfk_np1half-dVSinTheta_ijm1halfk_np1half);
        
        //Calcualte dA3
        dA3CenGrad=(dE_ijkp1half_n-dE_ijkm1half_n)/grid.dLocalGridOld[grid.nDPhi][0][0][k];
        dA3UpWindGrad=0.0;
        if(dW_ijk_np1half<0.0){//moving in the negative direction
          dA3UpWindGrad=(grid.dLocalGridOld[grid.nE][i][j][k+1]
            -grid.dLocalGridOld[grid.nE][i][j][k])/(grid.dLocalGridOld[grid.nDPhi][0][0][k+1]
            +grid.dLocalGridOld[grid.nDPhi][0][0][k])*2.0;
        }
        else{//moving in the positive direction
          dA3UpWindGrad=(grid.dLocalGridOld[grid.nE][i][j][k]
            -grid.dLocalGridOld[grid.nE][i][j][k-1])/(grid.dLocalGridOld[grid.nDPhi][0][0][k]
            +grid.dLocalGridOld[grid.nDPhi][0][0][k-1])*2.0;
        }
        dA3=dW_ijk_np1half/(dR_i_np1half*grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0])*
          ((1.0-parameters.dDonorFrac)*dA3CenGrad+parameters.dDonorFrac*dA3UpWindGrad);
        
        //Calcualte dS3
        dS3=dP_ijk_n/(grid.dLocalGridOld[grid.nD][i][j][k]*dR_i_np1half
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]*grid.dLocalGridOld[grid.nDPhi][0][0][k])
          *(dW_ijkp1half_np1half-dW_ijkm1half_np1half);
        
        //Calculate dS4
        dTGrad_im1half=(dT4_ijk_n-dT4_im1jk_n)/(grid.dLocalGridOld[grid.nDM][i][0][0]
          +grid.dLocalGridOld[grid.nDM][i-1][0][0])*2.0;
        dGrad_ip1half=-3.0*dRSq_ip1half_np1half*dT4_ijk_n/(8.0*parameters.dPi);/**\BC
          Missing grid.dLocalGridOld[grid.nT][i+1][0][0]*/
        dGrad_im1half=dRhoAve_im1half_n*dR4_im1half_np1half/(dKappa_im1halfjk_n*dRho_im1halfjk_n)
          *dTGrad_im1half;
        dS4=16.0*parameters.dPi*parameters.dPi*grid.dLocalGridOld[grid.nDenAve][i][0][0]
          *(dGrad_ip1half-dGrad_im1half)/grid.dLocalGridOld[grid.nDM][i][0][0];
        
        //Calculate dS5
        dTGrad_jp1half=(dT4_ijp1k_n-dT4_ijk_n)/(grid.dLocalGridOld[grid.nDTheta][0][j+1][0]
          +grid.dLocalGridOld[grid.nDTheta][0][j][0])*2.0;
        dTGrad_jm1half=(dT4_ijk_n-dT4_ijm1k_n)/(grid.dLocalGridOld[grid.nDTheta][0][j][0]
          +grid.dLocalGridOld[grid.nDTheta][0][j-1][0])*2.0;;
        dGrad_jp1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt][0]
          /(dKappa_ijp1halfk_n*dRho_ijp1halfk_n)*dTGrad_jp1half;
        dGrad_jm1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt-1][0]
          /(dKappa_ijm1halfk_n*dRho_ijm1halfk_n)*dTGrad_jm1half;;
        dS5=(dGrad_jp1half-dGrad_jm1half)/(grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]
          *dRSq_i_np1half*grid.dLocalGridOld[grid.nDTheta][0][j][0]);
        
        //Calculate dS6
        dTGrad_kp1half=(dT4_ijkp1_n-dT4_ijk_n)/(grid.dLocalGridOld[grid.nDPhi][0][0][k+1]
          +grid.dLocalGridOld[grid.nDPhi][0][0][k])*2.0;
        dTGrad_km1half=(dT4_ijk_n-dT4_ijkm1_n)/(grid.dLocalGridOld[grid.nDPhi][0][0][k]
          +grid.dLocalGridOld[grid.nDPhi][0][0][k-1])*2.0;;
        dGrad_kp1half=dTGrad_kp1half/(dKappa_ijkp1half*dRho_ijkp1half_n);
        dGrad_km1half=dTGrad_km1half/(dKappa_ijkm1half*dRho_ijkm1half_n);
        dS6=(dGrad_kp1half-dGrad_km1half)/(dRSq_i_np1half
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]*grid.dLocalGridOld[grid.nDPhi][0][0][k]);
        
        /*//calculate DUDM_ijk_np1half
        dDUDM_ijk_np1half=4.0*parameters.dPi*grid.dLocalGridOld[grid.nDenAve][i][0][0]
          *dRSq_i_np1half*((grid.dLocalGridNew[grid.nU][nIInt][j][k]
          -grid.dLocalGridNew[grid.nU0][nIInt][0][0])-(grid.dLocalGridNew[grid.nU][nIInt-1][j][k]
          -grid.dLocalGridNew[grid.nU0][nIInt-1][0][0]))/grid.dLocalGridOld[grid.nDM][i][0][0];
        
        //calculate DVDM_ijk_np1half
        dDVDM_ijk_np1half=4.0*parameters.dPi*grid.dLocalGridOld[grid.nDenAve][i][0][0]
          *dRSq_i_np1half*(dV_ip1halfjk_np1half-dV_im1halfjk_np1half)
          /grid.dLocalGridOld[grid.nDM][i][0][0];
        
        //calculate DUDTheta_ijk_np1half
        dDUDTheta_ijk_np1half=((dU_ijp1halfk_np1half-dU0_i_np1half)-(dU_ijm1halfk_np1half
          -dU0_i_np1half))/(dR_i_np1half*grid.dLocalGridOld[grid.nDTheta][0][j][0]);
        
        //calculate DVDTheta_ijk_np1half
        dDVDTheta_ijk_np1half=(grid.dLocalGridNew[grid.nV][i][nJInt][k]
          -grid.dLocalGridNew[grid.nV][i][nJInt-1][k])/(dR_i_np1half
          *grid.dLocalGridOld[grid.nDTheta][0][j][0]);
        
        //calculate DUDPhi_ijk_np1half
        dDUDPhi_ijk_np1half=((dU_ijkp1half_np1half-dU0_i_np1half)-(dU_ijkm1half_np1half
          -dU0_i_np1half))/(dR_i_np1half*grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]
          *grid.dLocalGridOld[grid.nDPhi][0][0][k]);
        
        //calculate DVDPhi_ijk_np1half
        dDVDPhi_ijk_np1half=(dV_ijkp1half_np1half-dV_ijkm1half_np1half)/(dR_i_np1half
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]*grid.dLocalGridOld[grid.nDPhi][0][0][k]);
        
        //calculate DWDM_ijk_np1half
        dDWDM_ijk_np1half=4.0*parameters.dPi*grid.dLocalGridOld[grid.nDenAve][i][0][0]
          *dRSq_i_np1half*(dW_ip1halfjk_np1half-dW_im1halfjk_np1half)
          /grid.dLocalGridOld[grid.nDM][i][0][0];
        
        //calculate DWDTheta_ijk_np1half
        dDWDTheta_ijk_np1half=(dW_ijp1halfk_np1half-dW_ijm1halfk_np1half)/(dR_i_np1half
          *grid.dLocalGridOld[grid.nDTheta][0][j][0]);
        
        //calculate DWDPhi_ijk_np1half
        dDWDPhi_ijk_np1half=(grid.dLocalGridNew[grid.nW][i][j][nKInt]
          -grid.dLocalGridNew[grid.nW][i][j][nKInt-1])/(dR_i_np1half
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]*grid.dLocalGridOld[grid.nDPhi][0][0][k]);
        
        //cal DivU_ijk_np1half
        dDivU_ijk_np1half=dDUDM_ijk_np1half+dDVDTheta_ijk_np1half+dDWDPhi_ijk_np1half;
        
        //cal Tau_rr_ijk_np1half
        dTau_rr_ijk_np1half=2.0*grid.dLocalGridNew[grid.nEddyVisc][i][j][k]*(dDUDM_ijk_np1half
          -0.3333333333333333*dDivU_ijk_np1half);
        
        //calculate Tau_tt_ijp1halfk_np1half
        dTau_tt_ijk_np1half=2.0*grid.dLocalGridNew[grid.nEddyVisc][i][j][k]*(dDVDTheta_ijk_np1half
          -0.333333333333333*dDivU_ijk_np1half);
        
        //calculate dTau_pp_ijk_np1half
        dTau_pp_ijk_np1half=2.0*grid.dLocalGridNew[grid.nEddyVisc][i][j][k]*(dDWDPhi_ijk_np1half
          -0.3333333333333333*dDivU_ijk_np1half);
        
        //calculate Tau_rt_ijk_np1half
        dTau_rt_ijk_np1half=grid.dLocalGridNew[grid.nEddyVisc][i][j][k]*(dDVDM_ijk_np1half
          +dDUDTheta_ijk_np1half);
        
        //calculate dTau_rp_ijk_np1half
        dTau_rp_ijk_np1half=grid.dLocalGridNew[grid.nEddyVisc][i][j][k]*(dDUDPhi_ijk_np1half
          +dDWDM_ijk_np1half);
        
        //calculate dTau_tp_ijk_np1half
        dTau_tp_ijk_np1half=grid.dLocalGridNew[grid.nEddyVisc][i][j][k]*(dDVDPhi_ijk_np1half
          +dDWDTheta_ijk_np1half);
        
        //eddy viscosity terms
        dEddyViscosityTerms=(dTau_rr_ijk_np1half*dDUDM_ijk_np1half+dTau_tt_ijk_np1half
          *dDVDTheta_ijk_np1half+dTau_pp_ijk_np1half*dDWDPhi_ijk_np1half+dTau_rt_ijk_np1half
          *(dDUDTheta_ijk_np1half+dDVDM_ijk_np1half)+dTau_rp_ijk_np1half*(dDUDPhi_ijk_np1half
          +dDWDM_ijk_np1half)+dTau_tp_ijk_np1half*(dDVDPhi_ijk_np1half+dDWDTheta_ijk_np1half))
          /grid.dLocalGridOld[grid.nD][i][j][k];*/
        
        //calculate dT1
        dEGrad_ip1halfjk_np1half=dR4_ip1half_np1half*dEddyVisc_ip1halfjk_n*dRhoAve_ip1half_n
          *(grid.dLocalGridOld[grid.nE][i+1][j][k]-grid.dLocalGridOld[grid.nE][i][j][k])
          /(dRho_ip1halfjk_n*dDM_ip1half);
        dEGrad_im1halfjk_np1half=dR4_im1half_np1half*dEddyVisc_im1halfjk_n*dRhoAve_im1half_n
          *(grid.dLocalGridOld[grid.nE][i][j][k]-grid.dLocalGridOld[grid.nE][i-1][j][k])
          /(dRho_im1halfjk_n*dDM_im1half);
        dT1=16.0*dPiSq*grid.dLocalGridOld[grid.nDenAve][i][0][0]*(dEGrad_ip1halfjk_np1half
          -dEGrad_im1halfjk_np1half)/grid.dLocalGridOld[grid.nDM][i][0][0];
        
        //calculate dT2
        dEGrad_ijp1halfk_np1half=dEddyVisc_ijp1halfk_n
          *grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt][0]
          *(grid.dLocalGridOld[grid.nE][i][j+1][k]-grid.dLocalGridOld[grid.nE][i][j][k])
          /(dRho_ijp1halfk_n*dR_i_np1half*dDelTheta_jp1half);
        dEGrad_ijm1halfk_np1half=dEddyVisc_ijm1halfk_n
          *grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt-1][0]
          *(grid.dLocalGridOld[grid.nE][i][j][k]-grid.dLocalGridOld[grid.nE][i][j-1][k])
          /(dRho_ijm1halfk_n*dR_i_np1half*dDelTheta_jm1half);
        dT2=(dEGrad_ijp1halfk_np1half-dEGrad_ijm1halfk_np1half)/(dR_i_np1half
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]
          *grid.dLocalGridOld[grid.nDTheta][0][j][0]);
        
        //calculate dT3
        dEGrad_ijkp1half_np1half=dEddyVisc_ijkp1half_n*(grid.dLocalGridOld[grid.nE][i][j][k+1]
          -grid.dLocalGridOld[grid.nE][i][j][k])/(dRho_ijkp1half_n
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]*dR_i_np1half*dDelPhi_kp1half);
        dEGrad_ijkm1half_np1half=dEddyVisc_ijkm1half_n*(grid.dLocalGridOld[grid.nE][i][j][k]
          -grid.dLocalGridOld[grid.nE][i][j][k-1])/(dRho_ijkm1half_n
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]*dR_i_np1half*dDelPhi_km1half);
        dT3=(dEGrad_ijkp1half_np1half-dEGrad_ijkm1half_np1half)/(dR_i_np1half
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]
          *grid.dLocalGridOld[grid.nDPhi][0][0][k]);
        
        //eddy viscosity terms
        dEddyViscosityTerms=(dT1+dT2+dT3)/parameters.dPrt;
        
        //calculate new energy
        grid.dLocalGridNew[grid.nE][i][j][k]=grid.dLocalGridOld[grid.nE][i][j][k]-time.dDeltat_n
          *(4.0*parameters.dPi*grid.dLocalGridOld[grid.nDenAve][i][0][0]*(dA1+dS1)+dA2+dS2+dA3+dS3
          -4.0*parameters.dSigma/(3.0*grid.dLocalGridOld[grid.nD][i][j][k])*(dS4+dS5+dS6)
          -dEddyViscosityTerms);
        
        if(grid.dLocalGridNew[grid.nE][i][j][k]<0.0){
          
          #if SIGNEGENG==1
            raise(SIGINT);
          #endif
          
          std::stringstream ssTemp;
          ssTemp<<__FILE__<<":"<<__FUNCTION__<<":"<<__LINE__<<":"<<procTop.nRank
            <<": negative energy calculated in , ("<<i<<","<<j<<","<<k<<")\n";
          throw exception2(ssTemp.str(),CALCULATION);
          
        }
      }
    }
  }
}
void calNewDenave_None(Grid &grid){
}
void calNewDenave_R(Grid &grid){
  
  //most ghost cell, since we don't have R at outer interface. This should be ok in most cases
  for(int i=grid.nStartUpdateExplicit[grid.nDenAve][0];i<grid.nEndUpdateExplicit[grid.nDenAve][0];i++){
    grid.dLocalGridNew[grid.nDenAve][i][0][0]=grid.dLocalGridNew[grid.nD][i][0][0];
  }
  //ghost region 0, outter most ghost region in x1 direction
  for(int i=grid.nStartGhostUpdateExplicit[grid.nDenAve][0][0];i<grid.nEndGhostUpdateExplicit[grid.nDenAve][0][0];i++){
    grid.dLocalGridNew[grid.nDenAve][i][0][0]=grid.dLocalGridNew[grid.nD][i][0][0];
  }
}
void calNewDenave_RT(Grid &grid){
  for(int i=grid.nStartUpdateExplicit[grid.nDenAve][0];i<grid.nEndUpdateExplicit[grid.nDenAve][0];i++){
    
    //calculate i for interface centered quantities
    int nIInt=i+grid.nCenIntOffset[0];
    
    double dSum=0.0;
    double dVolume=0.0;
    double dRFactor=0.33333333333333333*(pow(grid.dLocalGridNew[grid.nR][nIInt][0][0],3.0)
      -pow(grid.dLocalGridNew[grid.nR][nIInt-1][0][0],3.0));
    for(int j=grid.nStartUpdateExplicit[grid.nD][1];j<grid.nEndUpdateExplicit[grid.nD][1];j++){
      for(int k=grid.nStartUpdateExplicit[grid.nD][2];k<grid.nEndUpdateExplicit[grid.nD][2];k++){
        double dVolumeTemp=dRFactor*grid.dLocalGridOld[grid.nDCosThetaIJK][0][j][0];
        dSum+=dVolumeTemp*grid.dLocalGridNew[grid.nD][i][j][k];
        dVolume+=dVolumeTemp;
      }
    }
    grid.dLocalGridNew[grid.nDenAve][i][0][0]=dSum/dVolume;
  }
  
  //ghost region 0, outter most ghost region in x1 direction
  for(int i=grid.nStartGhostUpdateExplicit[grid.nDenAve][0][0];i<grid.nEndGhostUpdateExplicit[grid.nDenAve][0][0];i++){
    
    //calculate i for interface centered quantities
    int nIInt=i+grid.nCenIntOffset[0];
    
    double dSum=0.0;
    double dVolume=0.0;
    double dRFactor=0.33333333333333333*(pow(grid.dLocalGridNew[grid.nR][nIInt][0][0],3.0)
      -pow(grid.dLocalGridNew[grid.nR][nIInt-1][0][0],3.0));
    for(int j=grid.nStartUpdateExplicit[grid.nD][1];j<grid.nEndUpdateExplicit[grid.nD][1];j++){
      for(int k=grid.nStartUpdateExplicit[grid.nD][2];k<grid.nEndUpdateExplicit[grid.nD][2];k++){
        double dVolumeTemp=dRFactor*grid.dLocalGridOld[grid.nDCosThetaIJK][0][j][0];
        dSum+=dVolumeTemp*grid.dLocalGridNew[grid.nD][i][j][k];
        dVolume+=dVolumeTemp;
      }
    }
    grid.dLocalGridNew[grid.nDenAve][i][0][0]=dSum/dVolume;
  }
}
void calNewDenave_RTP(Grid &grid){
  for(int i=grid.nStartUpdateExplicit[grid.nDenAve][0];i<grid.nEndUpdateExplicit[grid.nDenAve][0];i++){
    
    //calculate i for interface centered quantities
    int nIInt=i+grid.nCenIntOffset[0];
    
    double dSum=0.0;
    double dVolume=0.0;
    double dRFactor=0.33333333333333333*(pow(grid.dLocalGridNew[grid.nR][nIInt][0][0],3.0)
      -pow(grid.dLocalGridNew[grid.nR][nIInt-1][0][0],3.0));
    for(int j=grid.nStartUpdateExplicit[grid.nD][1];j<grid.nEndUpdateExplicit[grid.nD][1];j++){
      for(int k=grid.nStartUpdateExplicit[grid.nD][2];k<grid.nEndUpdateExplicit[grid.nD][2];k++){
        double dVolumeTemp=dRFactor*grid.dLocalGridOld[grid.nDCosThetaIJK][0][j][0]
          *grid.dLocalGridOld[grid.nDPhi][0][0][k];
        dSum+=dVolumeTemp*grid.dLocalGridNew[grid.nD][i][j][k];
        dVolume+=dVolumeTemp;
      }
    }
    grid.dLocalGridNew[grid.nDenAve][i][0][0]=dSum/dVolume;
  }
  
  //ghost region 0, outter most ghost region in x1 direction
  for(int i=grid.nStartGhostUpdateExplicit[grid.nDenAve][0][0];i<grid.nEndGhostUpdateExplicit[grid.nDenAve][0][0];i++){
    
    //calculate i for interface centered quantities
    int nIInt=i+grid.nCenIntOffset[0];
    
    double dSum=0.0;
    double dVolume=0.0;
    double dRFactor=0.33333333333333333*(pow(grid.dLocalGridNew[grid.nR][nIInt][0][0],3.0)
      -pow(grid.dLocalGridNew[grid.nR][nIInt-1][0][0],3.0));
    for(int j=grid.nStartUpdateExplicit[grid.nD][1];j<grid.nEndUpdateExplicit[grid.nD][1];j++){
      for(int k=grid.nStartUpdateExplicit[grid.nD][2];k<grid.nEndUpdateExplicit[grid.nD][2];k++){
        double dVolumeTemp=dRFactor*grid.dLocalGridOld[grid.nDCosThetaIJK][0][j][0]
          *grid.dLocalGridOld[grid.nDPhi][0][0][k];
        dSum+=dVolumeTemp*grid.dLocalGridNew[grid.nD][i][j][k];
        dVolume+=dVolumeTemp;
      }
    }
    grid.dLocalGridNew[grid.nDenAve][i][0][0]=dSum/dVolume;
  }
}
void calNewP_GL(Grid& grid,Parameters &parameters){
  for(int i=grid.nStartUpdateExplicit[grid.nP][0];i<grid.nEndUpdateExplicit[grid.nP][0];i++){
    for(int j=grid.nStartUpdateExplicit[grid.nP][1];j<grid.nEndUpdateExplicit[grid.nP][1];j++){
      for(int k=grid.nStartUpdateExplicit[grid.nP][2];k<grid.nEndUpdateExplicit[grid.nP][2];k++){
        grid.dLocalGridNew[grid.nP][i][j][k]=dEOS_GL(grid.dLocalGridNew[grid.nD][i][j][k]
          ,grid.dLocalGridNew[grid.nE][i][j][k],parameters);
      }
    }
  }
  for(int i=grid.nStartGhostUpdateExplicit[grid.nP][0][0];i<grid.nEndGhostUpdateExplicit[grid.nP][0][0];i++){
    for(int j=grid.nStartGhostUpdateExplicit[grid.nP][0][1];j<grid.nEndGhostUpdateExplicit[grid.nP][0][1];j++){
      for(int k=grid.nStartGhostUpdateExplicit[grid.nP][0][2];k<grid.nEndGhostUpdateExplicit[grid.nP][0][2];k++){
        grid.dLocalGridNew[grid.nP][i][j][k]=dEOS_GL(grid.dLocalGridNew[grid.nD][i][j][k]
          ,grid.dLocalGridNew[grid.nE][i][j][k],parameters);
      }
    }
  }
  #if SEDOV==1 //use zero P, E, and rho gradients
    for(int i=grid.nStartGhostUpdateExplicit[grid.nP][1][0];i<grid.nEndGhostUpdateExplicit[grid.nP][1][0];i++){
      for(int j=grid.nStartGhostUpdateExplicit[grid.nP][1][1];j<grid.nEndGhostUpdateExplicit[grid.nP][1][1];j++){
        for(int k=grid.nStartGhostUpdateExplicit[grid.nP][1][2];k<grid.nEndGhostUpdateExplicit[grid.nP][1][2];k++){
          grid.dLocalGridNew[grid.nP][i][j][k]=dEOS_GL(grid.dLocalGridNew[grid.nD][i][j][k]
            ,grid.dLocalGridNew[grid.nE][i][j][k],parameters);
        }
      }
    }
  #endif
}
void calNewTPKappaGamma_TEOS(Grid& grid,Parameters &parameters){
  int i;
  int j;
  int k;
  int nCount;
  double dError;
  double dDTDE;
  double dT;
  double dE;
  double dDelE;
  
  //P, T, Kappa, and Gamma are all cenetered quantities, so bounds of any will be the same
  for(i=grid.nStartUpdateExplicit[grid.nP][0];i<grid.nEndUpdateExplicit[grid.nP][0];i++){
    for(j=grid.nStartUpdateExplicit[grid.nP][1];j<grid.nEndUpdateExplicit[grid.nP][1];j++){
      for(k=grid.nStartUpdateExplicit[grid.nP][2];k<grid.nEndUpdateExplicit[grid.nP][2];k++){
        
        //calculate new temperature
        dError=std::numeric_limits<double>::max();
        dT=grid.dLocalGridOld[grid.nT][i][j][k];
        nCount=0;
        while(dError>parameters.dTolerance&&nCount<parameters.nMaxIterations){
          parameters.eosTable.getEAndDTDE(dT,grid.dLocalGridNew[grid.nD][i][j][k],dE,dDTDE);
          
          //correct temperature
          dDelE=grid.dLocalGridNew[grid.nE][i][j][k]-dE;
          dT=dDelE*dDTDE+dT;
          
          //how far off was the energy
          dError=fabs(dDelE)/grid.dLocalGridNew[grid.nE][i][j][k];
          nCount++;
        }
        if(nCount>=parameters.nMaxIterations){
          std::cout<<__FILE__<<":"<<__FUNCTION__<<":"<<__LINE__<<": The maximum number of iteration"
          <<" for converging temperature in explicit region from equation of state ("
          <<parameters.nMaxIterations<<") has been exceeded with a maximum relative error in "
          <<"matching the energy of "<<dError<<std::endl;
        }
        grid.dLocalGridNew[grid.nT][i][j][k]=dT;
        
        //get P, Kappa, Gamma
        parameters.eosTable.getPKappaGamma(grid.dLocalGridNew[grid.nT][i][j][k]
          ,grid.dLocalGridNew[grid.nD][i][j][k],grid.dLocalGridNew[grid.nP][i][j][k]
          ,grid.dLocalGridNew[grid.nKappa][i][j][k],grid.dLocalGridNew[grid.nGamma][i][j][k]);
      }
    }
  }
  for(i=grid.nStartGhostUpdateExplicit[grid.nP][0][0];
    i<grid.nEndGhostUpdateExplicit[grid.nP][0][0];i++){
    for(j=grid.nStartGhostUpdateExplicit[grid.nP][0][1];
      j<grid.nEndGhostUpdateExplicit[grid.nP][0][1];j++){
      for(k=grid.nStartGhostUpdateExplicit[grid.nP][0][2];
        k<grid.nEndGhostUpdateExplicit[grid.nP][0][2];k++){
        
        //calculate new temperature
        dError=std::numeric_limits<double>::max();
        dT=grid.dLocalGridOld[grid.nT][i][j][k];
        nCount=0;
        while(dError>parameters.dTolerance&&nCount<parameters.nMaxIterations){
          parameters.eosTable.getEAndDTDE(dT,grid.dLocalGridNew[grid.nD][i][j][k],dE,dDTDE);
          
          //correct temperature
          dDelE=grid.dLocalGridNew[grid.nE][i][j][k]-dE;
          dT=dDelE*dDTDE+dT;
          
          //how far off was the energy
          dError=fabs(dDelE)/grid.dLocalGridNew[grid.nE][i][j][k];
          nCount++;
        }
        if(nCount>=parameters.nMaxIterations){
          std::cout<<__FILE__<<":"<<__FUNCTION__<<":"<<__LINE__<<": The maximum number of iteration"
          <<" for converging temperature in explicit region from equation of state ("
          <<parameters.nMaxIterations<<") has been exceeded with a maximum relative error in "
          <<"matching the energy of "<<dError<<std::endl;
        }
        grid.dLocalGridNew[grid.nT][i][j][k]=dT;
        
        //get P, Kappa, Gamma
        parameters.eosTable.getPKappaGamma(grid.dLocalGridNew[grid.nT][i][j][k]
          ,grid.dLocalGridNew[grid.nD][i][j][k],grid.dLocalGridNew[grid.nP][i][j][k]
          ,grid.dLocalGridNew[grid.nKappa][i][j][k],grid.dLocalGridNew[grid.nGamma][i][j][k]);
      }
    }
  }
}
void calNewPEKappaGamma_TEOS(Grid& grid,Parameters &parameters){
  
  //P, T, Kappa, and Gamma are all cenetered quantities, so bounds of any will be the same
  for(int i=grid.nStartUpdateImplicit[grid.nP][0];i<grid.nEndUpdateImplicit[grid.nP][0];i++){
    for(int j=grid.nStartUpdateImplicit[grid.nP][1];j<grid.nEndUpdateImplicit[grid.nP][1];j++){
      for(int k=grid.nStartUpdateImplicit[grid.nP][2];k<grid.nEndUpdateImplicit[grid.nP][2];k++){
        
        parameters.eosTable.getPEKappaGamma(grid.dLocalGridNew[grid.nT][i][j][k]
          ,grid.dLocalGridNew[grid.nD][i][j][k],grid.dLocalGridNew[grid.nP][i][j][k]
          ,grid.dLocalGridNew[grid.nE][i][j][k],grid.dLocalGridNew[grid.nKappa][i][j][k]
          ,grid.dLocalGridNew[grid.nGamma][i][j][k]);
      }
    }
  }
  for(int i=grid.nStartGhostUpdateImplicit[grid.nP][0][0];
    i<grid.nEndGhostUpdateImplicit[grid.nP][0][0];i++){
    for(int j=grid.nStartGhostUpdateImplicit[grid.nP][0][1];
      j<grid.nEndGhostUpdateImplicit[grid.nP][0][1];j++){
      for(int k=grid.nStartGhostUpdateImplicit[grid.nP][0][2];
        k<grid.nEndGhostUpdateImplicit[grid.nP][0][2];k++){
        
        parameters.eosTable.getPEKappaGamma(grid.dLocalGridNew[grid.nT][i][j][k]
          ,grid.dLocalGridNew[grid.nD][i][j][k],grid.dLocalGridNew[grid.nP][i][j][k]
          ,grid.dLocalGridNew[grid.nE][i][j][k],grid.dLocalGridNew[grid.nKappa][i][j][k]
          ,grid.dLocalGridNew[grid.nGamma][i][j][k]);
      }
    }
  }
}
void calNewQ0_R_TEOS(Grid& grid,Parameters &parameters){
  
  double dA_sq=parameters.dA*parameters.dA;
  double dDVDt;
  double dA_ip1half;
  double dA_im1half;
  double dR_i;
  int nIInt;
  double dC;
  double dDVDtThreshold;
  double dR_i_sq;
  double dDVDt_mthreshold;
  int i;
  
  //in the main grid
  for(i=grid.nStartUpdateExplicit[grid.nQ0][0];i<grid.nEndUpdateExplicit[grid.nQ0][0];i++){
    
    //calculate i for interface centered quantities
    nIInt=i+grid.nCenIntOffset[0];
    
    //calculate volume change
    dA_ip1half=grid.dLocalGridNew[grid.nR][nIInt][0][0]*grid.dLocalGridNew[grid.nR][nIInt][0][0];
    dA_im1half=grid.dLocalGridNew[grid.nR][nIInt-1][0][0]
      *grid.dLocalGridNew[grid.nR][nIInt-1][0][0];
    dR_i=(grid.dLocalGridNew[grid.nR][nIInt][0][0]+grid.dLocalGridNew[grid.nR][nIInt-1][0][0])*0.5;
    dR_i_sq=dR_i*dR_i;
    dDVDt=(dA_ip1half*grid.dLocalGridNew[grid.nU][nIInt][0][0]
      -dA_im1half*grid.dLocalGridNew[grid.nU][nIInt-1][0][0])/dR_i_sq;
    
    //calculate threshold compression to turn viscosity on at
    dC=sqrt(grid.dLocalGridNew[grid.nGamma][i][0][0]*(grid.dLocalGridNew[grid.nP][i][0][0])
      /grid.dLocalGridNew[grid.nD][i][0][0]);
    dDVDtThreshold=parameters.dAVThreshold*dC;
    
    if(dDVDt<-1.0*dDVDtThreshold){//being compressed
      dDVDt_mthreshold=dDVDt+dDVDtThreshold;
      grid.dLocalGridNew[grid.nQ0][i][0][0]=dA_sq*grid.dLocalGridNew[grid.nD][i][0][0]
        *dDVDt_mthreshold*dDVDt_mthreshold;
    }
    else{
      grid.dLocalGridNew[grid.nQ0][i][0][0]=0.0;
    }
  }
  
  //outter ghost region
  for(i=grid.nStartGhostUpdateExplicit[grid.nQ0][0][0];
    i<grid.nEndGhostUpdateExplicit[grid.nQ0][0][0];i++){
    
    //calculate i for interface centered quantities
    nIInt=i+grid.nCenIntOffset[0];
    
    //calculate volume change
    dA_ip1half=grid.dLocalGridNew[grid.nR][nIInt][0][0]*grid.dLocalGridNew[grid.nR][nIInt][0][0];
    dA_im1half=grid.dLocalGridNew[grid.nR][nIInt-1][0][0]
      *grid.dLocalGridNew[grid.nR][nIInt-1][0][0];
    dR_i=(grid.dLocalGridNew[grid.nR][nIInt][0][0]+grid.dLocalGridNew[grid.nR][nIInt-1][0][0])*0.5;
    dR_i_sq=dR_i*dR_i;
    dDVDt=(dA_ip1half*grid.dLocalGridNew[grid.nU][nIInt][0][0]
      -dA_im1half*grid.dLocalGridNew[grid.nU][nIInt-1][0][0])/dR_i_sq;
    
    //calculate threshold compression to turn viscosity on at
    dC=sqrt(grid.dLocalGridNew[grid.nGamma][i][0][0]*(grid.dLocalGridNew[grid.nP][i][0][0])
      /grid.dLocalGridNew[grid.nD][i][0][0]);
    dDVDtThreshold=parameters.dAVThreshold*dC;
    
    if(dDVDt<-1.0*dDVDtThreshold){//being compressed
      dDVDt_mthreshold=dDVDt+dDVDtThreshold;
      grid.dLocalGridNew[grid.nQ0][i][0][0]=dA_sq*grid.dLocalGridNew[grid.nD][i][0][0]
        *dDVDt_mthreshold*dDVDt_mthreshold;
    }
    else{
      grid.dLocalGridNew[grid.nQ0][i][0][0]=0.0;
    }
  }
}
void calNewQ0_R_GL(Grid& grid,Parameters &parameters){
  
  double dA_sq=parameters.dA*parameters.dA;
  double dDVDt;
  double dA_ip1half;
  double dA_im1half;
  double dR_i;
  int nIInt;
  double dC;
  double dDVDtThreshold;
  double dR_i_sq;
  double dDVDt_mthreshold;
  int i;
  //in the main grid
  for(i=grid.nStartUpdateExplicit[grid.nQ0][0];i<grid.nEndUpdateExplicit[grid.nQ0][0];i++){
    
    //calculate i for interface centered quantities
    nIInt=i+grid.nCenIntOffset[0];
    
    //calculate volume change
    dA_ip1half=grid.dLocalGridNew[grid.nR][nIInt][0][0]*grid.dLocalGridNew[grid.nR][nIInt][0][0];
    dA_im1half=grid.dLocalGridNew[grid.nR][nIInt-1][0][0]
      *grid.dLocalGridNew[grid.nR][nIInt-1][0][0];
    dR_i=(grid.dLocalGridNew[grid.nR][nIInt][0][0]+grid.dLocalGridNew[grid.nR][nIInt-1][0][0])*0.5;
    dR_i_sq=dR_i*dR_i;
    dDVDt=(dA_ip1half*grid.dLocalGridNew[grid.nU][nIInt][0][0]
      -dA_im1half*grid.dLocalGridNew[grid.nU][nIInt-1][0][0])/dR_i_sq;
    
    //calculate threshold compression to turn viscosity on at
    dC=sqrt(parameters.dGamma*(grid.dLocalGridNew[grid.nP][i][0][0])
      /grid.dLocalGridNew[grid.nD][i][0][0]);
    dDVDtThreshold=parameters.dAVThreshold*dC;
    
    if(dDVDt<-1.0*dDVDtThreshold){//being compressed
      dDVDt_mthreshold=dDVDt+dDVDtThreshold;
      grid.dLocalGridNew[grid.nQ0][i][0][0]=dA_sq*grid.dLocalGridNew[grid.nD][i][0][0]
        *dDVDt_mthreshold*dDVDt_mthreshold;
    }
    else{
      grid.dLocalGridNew[grid.nQ0][i][0][0]=0.0;
    }
  }
  
  //outter ghost region
  for(i=grid.nStartGhostUpdateExplicit[grid.nQ0][0][0];
    i<grid.nEndGhostUpdateExplicit[grid.nQ0][0][0];i++){
    
    //calculate i for interface centered quantities
    nIInt=i+grid.nCenIntOffset[0];
    
    //calculate volume change
    dA_ip1half=grid.dLocalGridNew[grid.nR][nIInt][0][0]*grid.dLocalGridNew[grid.nR][nIInt][0][0];
    dA_im1half=grid.dLocalGridNew[grid.nR][nIInt-1][0][0]
      *grid.dLocalGridNew[grid.nR][nIInt-1][0][0];
    dR_i=(grid.dLocalGridNew[grid.nR][nIInt][0][0]+grid.dLocalGridNew[grid.nR][nIInt-1][0][0])*0.5;
    dR_i_sq=dR_i*dR_i;
    dDVDt=(dA_ip1half*grid.dLocalGridNew[grid.nU][nIInt][0][0]
      -dA_im1half*grid.dLocalGridNew[grid.nU][nIInt-1][0][0])/dR_i_sq;
    
    //calculate threshold compression to turn viscosity on at
    dC=sqrt(parameters.dGamma*(grid.dLocalGridNew[grid.nP][i][0][0])
      /grid.dLocalGridNew[grid.nD][i][0][0]);
    dDVDtThreshold=parameters.dAVThreshold*dC;
    
    if(dDVDt<-1.0*dDVDtThreshold){//being compressed
      dDVDt_mthreshold=dDVDt+dDVDtThreshold;
      grid.dLocalGridNew[grid.nQ0][i][0][0]=dA_sq*grid.dLocalGridNew[grid.nD][i][0][0]
        *dDVDt_mthreshold*dDVDt_mthreshold;
    }
    else{
      grid.dLocalGridNew[grid.nQ0][i][0][0]=0.0;
    }
  }
  
  //inner ghost region
  #if SEDOV==1
    for(int i=grid.nStartGhostUpdateExplicit[grid.nQ0][1][0];
      i<grid.nEndGhostUpdateExplicit[grid.nQ0][1][0];i++){
    
    //calculate i for interface centered quantities
    nIInt=i+grid.nCenIntOffset[0];
    
    //calculate volume change
    dA_ip1half=grid.dLocalGridNew[grid.nR][nIInt][0][0]*grid.dLocalGridNew[grid.nR][nIInt][0][0];
    dA_im1half=grid.dLocalGridNew[grid.nR][nIInt-1][0][0]
      *grid.dLocalGridNew[grid.nR][nIInt-1][0][0];
    dR_i=(grid.dLocalGridNew[grid.nR][nIInt][0][0]+grid.dLocalGridNew[grid.nR][nIInt-1][0][0])*0.5;
    dR_i_sq=dR_i*dR_i;
    dDVDt=(dA_ip1half*grid.dLocalGridNew[grid.nU][nIInt][0][0]
      -dA_im1half*grid.dLocalGridNew[grid.nU][nIInt-1][0][0])/dR_i_sq;
    
    //calculate threshold compression to turn viscosity on at
    dC=sqrt(parameters.dGamma*(grid.dLocalGridNew[grid.nP][i][0][0])
      /grid.dLocalGridNew[grid.nD][i][0][0]);
    dDVDtThreshold=parameters.dAVThreshold*dC;
    
    if(dDVDt<-1.0*dDVDtThreshold){//being compressed
      dDVDt_mthreshold=dDVDt+dDVDtThreshold;
      grid.dLocalGridNew[grid.nQ0][i][0][0]=dA_sq*grid.dLocalGridNew[grid.nD][i][0][0]
        *dDVDt_mthreshold*dDVDt_mthreshold;
    }
    else{
      grid.dLocalGridNew[grid.nQ0][i][0][0]=0.0;
    }
    }
  #endif
}
void calNewQ0Q1_RT_TEOS(Grid& grid,Parameters &parameters){
  
  double dA_sq=parameters.dA*parameters.dA;
  double dDVDt;
  double dA_ip1half;
  double dA_im1half;
  double dA_jp1half;
  double dA_jm1half;
  double dA_j;
  double dR_i;
  int nIInt;
  int nJInt;
  double dC;
  double dDVDtThreshold;
  double dR_i_sq;
  double dDVDt_mthreshold;
  int i;
  int j;
  
  //in the main grid
  for(i=grid.nStartUpdateExplicit[grid.nQ0][0];i<grid.nEndUpdateExplicit[grid.nQ0][0];i++){
    
    //calculate i for interface centered quantities
    nIInt=i+grid.nCenIntOffset[0];
    dR_i=(grid.dLocalGridNew[grid.nR][nIInt][0][0]+grid.dLocalGridNew[grid.nR][nIInt-1][0][0])*0.5;
    dR_i_sq=dR_i*dR_i;
    dA_ip1half=grid.dLocalGridNew[grid.nR][nIInt][0][0]*grid.dLocalGridNew[grid.nR][nIInt][0][0];
    dA_im1half=grid.dLocalGridNew[grid.nR][nIInt-1][0][0]
      *grid.dLocalGridNew[grid.nR][nIInt-1][0][0];
    
    for(j=grid.nStartUpdateExplicit[grid.nQ0][1];j<grid.nEndUpdateExplicit[grid.nQ0][1];j++){
      
      //calculate j for interface centered quantities
      nJInt=j+grid.nCenIntOffset[1];
      
      //calculate Q0
      //calculate volume change
      dDVDt=(dA_ip1half*grid.dLocalGridNew[grid.nU][nIInt][j][0]
        -dA_im1half*grid.dLocalGridNew[grid.nU][nIInt-1][j][0])/dR_i_sq;
      
      //calculate threshold compression to turn viscosity on at
      dC=sqrt(grid.dLocalGridNew[grid.nGamma][i][j][0]*(grid.dLocalGridNew[grid.nP][i][j][0])
        /grid.dLocalGridNew[grid.nD][i][j][0]);
      dDVDtThreshold=parameters.dAVThreshold*dC;
      
      if(dDVDt<-1.0*dDVDtThreshold){//being compressed
        dDVDt_mthreshold=dDVDt+dDVDtThreshold;//need to add it since dVDt will be negative
        grid.dLocalGridNew[grid.nQ0][i][j][0]=dA_sq*grid.dLocalGridNew[grid.nD][i][j][0]
          *dDVDt_mthreshold*dDVDt_mthreshold;
      }
      else{
        grid.dLocalGridNew[grid.nQ0][i][j][0]=0.0;
      }
      
      //calculate Q1
      //calculate volume change
      dA_jp1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt][0];
      dA_jm1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt-1][0];
      dA_j=grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0];
      
      dDVDt=(dA_jp1half*grid.dLocalGridNew[grid.nV][i][nJInt][0]
        -dA_jm1half*grid.dLocalGridNew[grid.nV][i][nJInt-1][0])/dA_j;
      
      if(dDVDt<-1.0*dDVDtThreshold){//being compressed
        dDVDt_mthreshold=dDVDt+dDVDtThreshold;
        grid.dLocalGridNew[grid.nQ1][i][j][0]=dA_sq*grid.dLocalGridNew[grid.nD][i][j][0]
          *dDVDt_mthreshold*dDVDt_mthreshold;
      }
      else{
        grid.dLocalGridNew[grid.nQ1][i][j][0]=0.0;
      }
    }
  }
  
  //outter ghost region
  for(i=grid.nStartGhostUpdateExplicit[grid.nQ0][0][0];
    i<grid.nEndGhostUpdateExplicit[grid.nQ0][0][0];i++){
    
    //calculate i for interface centered quantities
    nIInt=i+grid.nCenIntOffset[0];
    dR_i=(grid.dLocalGridNew[grid.nR][nIInt][0][0]+grid.dLocalGridNew[grid.nR][nIInt-1][0][0])*0.5;
    dR_i_sq=dR_i*dR_i;
    dA_ip1half=grid.dLocalGridNew[grid.nR][nIInt][0][0]*grid.dLocalGridNew[grid.nR][nIInt][0][0];
    dA_im1half=grid.dLocalGridNew[grid.nR][nIInt-1][0][0]
      *grid.dLocalGridNew[grid.nR][nIInt-1][0][0];
    
    for(j=grid.nStartGhostUpdateExplicit[grid.nQ0][0][1];
      j<grid.nEndGhostUpdateExplicit[grid.nQ0][0][1];j++){
      
      //calculate j for interface centered quantities
      nJInt=j+grid.nCenIntOffset[1];
      
      //calculate Q0
      //calculate volume change
      dDVDt=(dA_ip1half*grid.dLocalGridNew[grid.nU][nIInt][j][0]
        -dA_im1half*grid.dLocalGridNew[grid.nU][nIInt-1][j][0])/dR_i_sq;
      
      //calculate threshold compression to turn viscosity on at
      dC=sqrt(grid.dLocalGridNew[grid.nGamma][i][j][0]*(grid.dLocalGridNew[grid.nP][i][j][0])
        /grid.dLocalGridNew[grid.nD][i][j][0]);
      dDVDtThreshold=parameters.dAVThreshold*dC;
      
      if(dDVDt<-1.0*dDVDtThreshold){//being compressed
        dDVDt_mthreshold=dDVDt+dDVDtThreshold;
        grid.dLocalGridNew[grid.nQ0][i][j][0]=dA_sq*grid.dLocalGridNew[grid.nD][i][j][0]
          *dDVDt_mthreshold*dDVDt_mthreshold;
      }
      else{
        grid.dLocalGridNew[grid.nQ0][i][j][0]=0.0;
      }
      
      //calculate Q1
      //calculate volume change
      dA_jp1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt][0];
      dA_jm1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt-1][0];
      dA_j=grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0];
      
      dDVDt=(dA_jp1half*grid.dLocalGridNew[grid.nV][i][nJInt][0]
        -dA_jm1half*grid.dLocalGridNew[grid.nV][i][nJInt-1][0])/dA_j;
      
      if(dDVDt<-1.0*dDVDtThreshold){//being compressed
        dDVDt_mthreshold=dDVDt+dDVDtThreshold;
        grid.dLocalGridNew[grid.nQ1][i][j][0]=dA_sq*grid.dLocalGridNew[grid.nD][i][j][0]
          *dDVDt_mthreshold*dDVDt_mthreshold;
      }
      else{
        grid.dLocalGridNew[grid.nQ1][i][j][0]=0.0;
      }
    }
  }
}
void calNewQ0Q1_RT_GL(Grid& grid,Parameters &parameters){
  
  double dA_sq=parameters.dA*parameters.dA;
  double dDVDt;
  double dA_ip1half;
  double dA_im1half;
  double dA_jp1half;
  double dA_jm1half;
  double dA_j;
  double dR_i;
  int nIInt;
  int nJInt;
  double dC;
  double dDVDtThreshold;
  double dR_i_sq;
  double dDVDt_mthreshold;
  int i;
  int j;
  
  //in the main grid
  for(i=grid.nStartUpdateExplicit[grid.nQ0][0];i<grid.nEndUpdateExplicit[grid.nQ0][0];i++){
    
    //calculate i for interface centered quantities
    nIInt=i+grid.nCenIntOffset[0];
    dR_i=(grid.dLocalGridNew[grid.nR][nIInt][0][0]+grid.dLocalGridNew[grid.nR][nIInt-1][0][0])*0.5;
    dR_i_sq=dR_i*dR_i;
    dA_ip1half=grid.dLocalGridNew[grid.nR][nIInt][0][0]*grid.dLocalGridNew[grid.nR][nIInt][0][0];
    dA_im1half=grid.dLocalGridNew[grid.nR][nIInt-1][0][0]
      *grid.dLocalGridNew[grid.nR][nIInt-1][0][0];
    
    for(j=grid.nStartUpdateExplicit[grid.nQ0][1];j<grid.nEndUpdateExplicit[grid.nQ0][1];j++){
      
      //calculate j for interface centered quantities
      nJInt=j+grid.nCenIntOffset[1];
      
      //calculate Q0
      //calculate volume change
      dDVDt=(dA_ip1half*grid.dLocalGridNew[grid.nU][nIInt][j][0]
        -dA_im1half*grid.dLocalGridNew[grid.nU][nIInt-1][j][0])/dR_i_sq;
      
      //calculate threshold compression to turn viscosity on at
      dC=sqrt(parameters.dGamma*(grid.dLocalGridNew[grid.nP][i][j][0])
        /grid.dLocalGridNew[grid.nD][i][j][0]);
      dDVDtThreshold=parameters.dAVThreshold*dC;
      
      if(dDVDt<-1.0*dDVDtThreshold){//being compressed
        dDVDt_mthreshold=dDVDt+dDVDtThreshold;
        grid.dLocalGridNew[grid.nQ0][i][j][0]=dA_sq*grid.dLocalGridNew[grid.nD][i][j][0]
          *dDVDt_mthreshold*dDVDt_mthreshold;
      }
      else{
        grid.dLocalGridNew[grid.nQ0][i][j][0]=0.0;
      }
      
      //calculate Q1
      //calculate volume change
      dA_jp1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt][0];
      dA_jm1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt-1][0];
      dA_j=grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0];
      
      dDVDt=(dA_jp1half*grid.dLocalGridNew[grid.nV][i][nJInt][0]
        -dA_jm1half*grid.dLocalGridNew[grid.nV][i][nJInt-1][0])/dA_j;
      
      if(dDVDt<-1.0*dDVDtThreshold){//being compressed
        dDVDt_mthreshold=dDVDt+dDVDtThreshold;
        grid.dLocalGridNew[grid.nQ1][i][j][0]=dA_sq*grid.dLocalGridNew[grid.nD][i][j][0]
          *dDVDt_mthreshold*dDVDt_mthreshold;
      }
      else{
        grid.dLocalGridNew[grid.nQ1][i][j][0]=0.0;
      }
    }
  }
  
  //outter ghost region
  for(i=grid.nStartGhostUpdateExplicit[grid.nQ0][0][0];
    i<grid.nEndGhostUpdateExplicit[grid.nQ0][0][0];i++){
    
    //calculate i for interface centered quantities
    nIInt=i+grid.nCenIntOffset[0];
    dR_i=(grid.dLocalGridNew[grid.nR][nIInt][0][0]+grid.dLocalGridNew[grid.nR][nIInt-1][0][0])*0.5;
    dR_i_sq=dR_i*dR_i;
    dA_ip1half=grid.dLocalGridNew[grid.nR][nIInt][0][0]*grid.dLocalGridNew[grid.nR][nIInt][0][0];
    dA_im1half=grid.dLocalGridNew[grid.nR][nIInt-1][0][0]
      *grid.dLocalGridNew[grid.nR][nIInt-1][0][0];
    
    for(j=grid.nStartGhostUpdateExplicit[grid.nQ0][0][1];
      j<grid.nEndGhostUpdateExplicit[grid.nQ0][0][1];j++){
      
      //calculate j for interface centered quantities
      nJInt=j+grid.nCenIntOffset[1];
      
      //calculate Q0
      //calculate volume change
      dDVDt=(dA_ip1half*grid.dLocalGridNew[grid.nU][nIInt][j][0]
        -dA_im1half*grid.dLocalGridNew[grid.nU][nIInt-1][j][0])/dR_i_sq;
      
      //calculate threshold compression to turn viscosity on at
      dC=sqrt(parameters.dGamma*(grid.dLocalGridNew[grid.nP][i][j][0])
        /grid.dLocalGridNew[grid.nD][i][j][0]);
      dDVDtThreshold=parameters.dAVThreshold*dC;
      
      if(dDVDt<-1.0*dDVDtThreshold){//being compressed
        dDVDt_mthreshold=dDVDt+dDVDtThreshold;
        grid.dLocalGridNew[grid.nQ0][i][j][0]=dA_sq*grid.dLocalGridNew[grid.nD][i][j][0]
          *dDVDt_mthreshold*dDVDt_mthreshold;
      }
      else{
        grid.dLocalGridNew[grid.nQ0][i][j][0]=0.0;
      }
      
      //calculate Q1
      //calculate volume change
      dA_jp1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt][0];
      dA_jm1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt-1][0];
      dA_j=grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0];
      
      dDVDt=(dA_jp1half*grid.dLocalGridNew[grid.nV][i][nJInt][0]
        -dA_jm1half*grid.dLocalGridNew[grid.nV][i][nJInt-1][0])/dA_j;
      
      if(dDVDt<-1.0*dDVDtThreshold){//being compressed
        dDVDt_mthreshold=dDVDt+dDVDtThreshold;
        grid.dLocalGridNew[grid.nQ1][i][j][0]=dA_sq*grid.dLocalGridNew[grid.nD][i][j][0]
          *dDVDt_mthreshold*dDVDt_mthreshold;
      }
      else{
        grid.dLocalGridNew[grid.nQ1][i][j][0]=0.0;
      }
    }
  }
  
  //inner ghost region
  #if SEDOV==1
    for(i=grid.nStartGhostUpdateExplicit[grid.nQ0][1][0];
      i<grid.nEndGhostUpdateExplicit[grid.nQ0][1][0];i++){
      
      //calculate i for interface centered quantities
      nIInt=i+grid.nCenIntOffset[0];
      dR_i=(grid.dLocalGridNew[grid.nR][nIInt][0][0]+grid.dLocalGridNew[grid.nR][nIInt-1][0][0])*0.5;
      dR_i_sq=dR_i*dR_i;
      dA_ip1half=grid.dLocalGridNew[grid.nR][nIInt][0][0]*grid.dLocalGridNew[grid.nR][nIInt][0][0];
      dA_im1half=grid.dLocalGridNew[grid.nR][nIInt-1][0][0]
        *grid.dLocalGridNew[grid.nR][nIInt-1][0][0];
      
      for(j=grid.nStartGhostUpdateExplicit[grid.nQ0][1][1];
        j<grid.nEndGhostUpdateExplicit[grid.nQ0][1][1];j++){
        
        //calculate j for interface centered quantities
        nJInt=j+grid.nCenIntOffset[1];
        
        //calculate Q0
        //calculate volume change
        dDVDt=(dA_ip1half*grid.dLocalGridNew[grid.nU][nIInt][j][0]
          -dA_im1half*grid.dLocalGridNew[grid.nU][nIInt-1][j][0])/dR_i_sq;
        
        //calculate threshold compression to turn viscosity on at
        dC=sqrt(parameters.dGamma*(grid.dLocalGridNew[grid.nP][i][j][0])
          /grid.dLocalGridNew[grid.nD][i][j][0]);
        dDVDtThreshold=parameters.dAVThreshold*dC;
        
        if(dDVDt<-1.0*dDVDtThreshold){//being compressed
          dDVDt_mthreshold=dDVDt+dDVDtThreshold;
          grid.dLocalGridNew[grid.nQ0][i][j][0]=dA_sq*grid.dLocalGridNew[grid.nD][i][j][0]
            *dDVDt_mthreshold*dDVDt_mthreshold;
        }
        else{
          grid.dLocalGridNew[grid.nQ0][i][j][0]=0.0;
        }
        
        //calculate Q1
        //calculate volume change
        dA_jp1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt][0];
        dA_jm1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt-1][0];
        dA_j=grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0];
        
        dDVDt=(dA_jp1half*grid.dLocalGridNew[grid.nV][i][nJInt][0]
          -dA_jm1half*grid.dLocalGridNew[grid.nV][i][nJInt-1][0])/dA_j;
        
        if(dDVDt<-1.0*dDVDtThreshold){//being compressed
          dDVDt_mthreshold=dDVDt+dDVDtThreshold;
          grid.dLocalGridNew[grid.nQ1][i][j][0]=dA_sq*grid.dLocalGridNew[grid.nD][i][j][0]
            *dDVDt_mthreshold*dDVDt_mthreshold;
        }
        else{
          grid.dLocalGridNew[grid.nQ1][i][j][0]=0.0;
        }
      }
    }
  #endif
}
void calNewQ0Q1Q2_RTP_TEOS(Grid& grid,Parameters &parameters){
  
  double dA_sq=parameters.dA*parameters.dA;
  double dDVDt;/*Rate of change of the volume*/
  double dA_ip1half;
  double dA_im1half;
  double dA_jp1half;
  double dA_jm1half;
  double dA_j;
  double dR_i;
  int nIInt;
  int nJInt;
  int nKInt;
  double dC;
  double dDVDtThreshold;
  double dR_i_sq;
  double dDVDt_mthreshold;
  int i;
  int j;
  int k;
  
  //in the main grid
  for(i=grid.nStartUpdateExplicit[grid.nQ0][0];i<grid.nEndUpdateExplicit[grid.nQ0][0];i++){
    
    //calculate i for interface centered quantities
    nIInt=i+grid.nCenIntOffset[0];
    dR_i=(grid.dLocalGridNew[grid.nR][nIInt][0][0]+grid.dLocalGridNew[grid.nR][nIInt-1][0][0])*0.5;
    dR_i_sq=dR_i*dR_i;
    dA_ip1half=grid.dLocalGridNew[grid.nR][nIInt][0][0]*grid.dLocalGridNew[grid.nR][nIInt][0][0];
    dA_im1half=grid.dLocalGridNew[grid.nR][nIInt-1][0][0]
      *grid.dLocalGridNew[grid.nR][nIInt-1][0][0];
    
    for(j=grid.nStartUpdateExplicit[grid.nQ0][1];j<grid.nEndUpdateExplicit[grid.nQ0][1];j++){
      
      //calculate j for interface centered quantities
      nJInt=j+grid.nCenIntOffset[1];
      dA_jp1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt][0];
      dA_jm1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt-1][0];
      dA_j=grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0];
      
      for(k=grid.nStartUpdateExplicit[grid.nQ0][2];k<grid.nEndUpdateExplicit[grid.nQ0][2];k++){
        
        nKInt=k+grid.nCenIntOffset[2];
        
        //calculate threshold compression to turn viscosity on at
        dC=sqrt(grid.dLocalGridNew[grid.nGamma][i][j][k]*(grid.dLocalGridNew[grid.nP][i][j][k])
          /grid.dLocalGridNew[grid.nD][i][j][k]);
        dDVDtThreshold=parameters.dAVThreshold*dC;
        
        //calculate Q0
        dDVDt=(dA_ip1half*grid.dLocalGridNew[grid.nU][nIInt][j][k]
          -dA_im1half*grid.dLocalGridNew[grid.nU][nIInt-1][j][k])/dR_i_sq;
        if(dDVDt<-1.0*dDVDtThreshold){//being compressed
          dDVDt_mthreshold=dDVDt+dDVDtThreshold;
          grid.dLocalGridNew[grid.nQ0][i][j][k]=dA_sq*grid.dLocalGridNew[grid.nD][i][j][k]
            *dDVDt_mthreshold*dDVDt_mthreshold;
        }
        else{
          grid.dLocalGridNew[grid.nQ0][i][j][k]=0.0;
        }
        
        //calculate Q1
        dDVDt=(dA_jp1half*grid.dLocalGridNew[grid.nV][i][nJInt][k]
          -dA_jm1half*grid.dLocalGridNew[grid.nV][i][nJInt-1][k])/dA_j;
        if(dDVDt<-1.0*dDVDtThreshold){//being compressed
          dDVDt_mthreshold=dDVDt+dDVDtThreshold;
          grid.dLocalGridNew[grid.nQ1][i][j][k]=dA_sq*grid.dLocalGridNew[grid.nD][i][j][k]
            *dDVDt_mthreshold*dDVDt_mthreshold;
        }
        else{
          grid.dLocalGridNew[grid.nQ1][i][j][k]=0.0;
        }
        
        //calculate Q2
        dDVDt=(grid.dLocalGridNew[grid.nW][i][j][nKInt]
          -grid.dLocalGridNew[grid.nW][i][j][nKInt-1]);
        if(dDVDt<-1.0*dDVDtThreshold){//being compressed
          dDVDt_mthreshold=dDVDt+dDVDtThreshold;
          grid.dLocalGridNew[grid.nQ2][i][j][k]=dA_sq*grid.dLocalGridNew[grid.nD][i][j][k]
            *dDVDt_mthreshold*dDVDt_mthreshold;
        }
        else{
          grid.dLocalGridNew[grid.nQ2][i][j][k]=0.0;
        }
        
      }
    }
  }
  
  //outter ghost region
  for(i=grid.nStartGhostUpdateExplicit[grid.nQ0][0][0];
    i<grid.nEndGhostUpdateExplicit[grid.nQ0][0][0];i++){
    
    //calculate i for interface centered quantities
    nIInt=i+grid.nCenIntOffset[0];
    dR_i=(grid.dLocalGridNew[grid.nR][nIInt][0][0]+grid.dLocalGridNew[grid.nR][nIInt-1][0][0])*0.5;
    dR_i_sq=dR_i*dR_i;
    dA_ip1half=grid.dLocalGridNew[grid.nR][nIInt][0][0]*grid.dLocalGridNew[grid.nR][nIInt][0][0];
    dA_im1half=grid.dLocalGridNew[grid.nR][nIInt-1][0][0]
      *grid.dLocalGridNew[grid.nR][nIInt-1][0][0];
    
    for(j=grid.nStartGhostUpdateExplicit[grid.nQ0][0][1];
      j<grid.nEndGhostUpdateExplicit[grid.nQ0][0][1];j++){
      
      //calculate j for interface centered quantities
      nJInt=j+grid.nCenIntOffset[1];
      dA_jp1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt][0];
      dA_jm1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt-1][0];
      dA_j=grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0];
      
      for(k=grid.nStartGhostUpdateExplicit[grid.nQ0][0][2];
        k<grid.nEndGhostUpdateExplicit[grid.nQ0][0][2];k++){
        
        nKInt=k+grid.nCenIntOffset[2];
        
        //calculate threshold compression to turn viscosity on at
        dC=sqrt(grid.dLocalGridNew[grid.nGamma][i][j][k]*(grid.dLocalGridNew[grid.nP][i][j][k])
          /grid.dLocalGridNew[grid.nD][i][j][k]);
        dDVDtThreshold=parameters.dAVThreshold*dC;
        
        //calculate Q0
        dDVDt=(dA_ip1half*grid.dLocalGridNew[grid.nU][nIInt][j][k]
          -dA_im1half*grid.dLocalGridNew[grid.nU][nIInt-1][j][k])/dR_i_sq;
        if(dDVDt<-1.0*dDVDtThreshold){//being compressed
          dDVDt_mthreshold=dDVDt+dDVDtThreshold;
          grid.dLocalGridNew[grid.nQ0][i][j][k]=dA_sq*grid.dLocalGridNew[grid.nD][i][j][k]
            *dDVDt_mthreshold*dDVDt_mthreshold;
        }
        else{
          grid.dLocalGridNew[grid.nQ0][i][j][k]=0.0;
        }
        
        //calculate Q1
        dDVDt=(dA_jp1half*grid.dLocalGridNew[grid.nV][i][nJInt][k]
          -dA_jm1half*grid.dLocalGridNew[grid.nV][i][nJInt-1][k])/dA_j;
        if(dDVDt<-1.0*dDVDtThreshold){//being compressed
          dDVDt_mthreshold=dDVDt+dDVDtThreshold;
          grid.dLocalGridNew[grid.nQ1][i][j][k]=dA_sq*grid.dLocalGridNew[grid.nD][i][j][k]
            *dDVDt_mthreshold*dDVDt_mthreshold;
        }
        else{
          grid.dLocalGridNew[grid.nQ1][i][j][k]=0.0;
        }
        
        //calculate Q2
        dDVDt=(grid.dLocalGridNew[grid.nW][i][j][nKInt]
          -grid.dLocalGridNew[grid.nW][i][j][nKInt-1]);
        if(dDVDt<-1.0*dDVDtThreshold){//being compressed
          dDVDt_mthreshold=dDVDt+dDVDtThreshold;
          grid.dLocalGridNew[grid.nQ2][i][j][k]=dA_sq*grid.dLocalGridNew[grid.nD][i][j][k]
            *dDVDt_mthreshold*dDVDt_mthreshold;
        }
        else{
          grid.dLocalGridNew[grid.nQ2][i][j][k]=0.0;
        }
      }
    }
  }
  
  //inner ghost region
  #if SEDOV==1
    for(i=grid.nStartGhostUpdateExplicit[grid.nQ0][1][0];
      i<grid.nEndGhostUpdateExplicit[grid.nQ0][1][0];i++){
      
      //calculate i for interface centered quantities
      nIInt=i+grid.nCenIntOffset[0];
      dR_i=(grid.dLocalGridNew[grid.nR][nIInt][0][0]+grid.dLocalGridNew[grid.nR][nIInt-1][0][0])*0.5;
      dR_i_sq=dR_i*dR_i;
      dA_ip1half=grid.dLocalGridNew[grid.nR][nIInt][0][0]*grid.dLocalGridNew[grid.nR][nIInt][0][0];
      dA_im1half=grid.dLocalGridNew[grid.nR][nIInt-1][0][0]
        *grid.dLocalGridNew[grid.nR][nIInt-1][0][0];
      
      for(j=grid.nStartGhostUpdateExplicit[grid.nQ0][1][1];
        j<grid.nEndGhostUpdateExplicit[grid.nQ0][1][1];j++){
        
        //calculate j for interface centered quantities
        nJInt=j+grid.nCenIntOffset[1];
        dA_jp1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt][0];
        dA_jm1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt-1][0];
        dA_j=grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0];
        
        for(int k=grid.nStartGhostUpdateExplicit[grid.nQ0][1][2];
          k<grid.nEndGhostUpdateExplicit[grid.nQ0][1][2];k++){
          
          nKInt=k+grid.nCenIntOffset[2];
          
          //calculate threshold compression to turn viscosity on at
          dC=sqrt(grid.dLocalGridNew[grid.nGamma][i][j][k]*(grid.dLocalGridNew[grid.nP][i][j][k])
            /grid.dLocalGridNew[grid.nD][i][j][k]);
          dDVDtThreshold=parameters.dAVThreshold*dC;
          
          //calculate Q0
          dDVDt=(dA_ip1half*grid.dLocalGridNew[grid.nU][nIInt][j][k]
            -dA_im1half*grid.dLocalGridNew[grid.nU][nIInt-1][j][k])/dR_i_sq;
          if(dDVDt<-1.0*dDVDtThreshold){//being compressed
            dDVDt_mthreshold=dDVDt+dDVDtThreshold;
            grid.dLocalGridNew[grid.nQ0][i][j][k]=dA_sq*grid.dLocalGridNew[grid.nD][i][j][k]
              *dDVDt_mthreshold*dDVDt_mthreshold;
          }
          else{
            grid.dLocalGridNew[grid.nQ0][i][j][k]=0.0;
          }
          
          //calculate Q1
          dDVDt=(dA_jp1half*grid.dLocalGridNew[grid.nV][i][nJInt][k]
            -dA_jm1half*grid.dLocalGridNew[grid.nV][i][nJInt-1][k])/dA_j;
          if(dDVDt<-1.0*dDVDtThreshold){//being compressed
            dDVDt_mthreshold=dDVDt+dDVDtThreshold;
            grid.dLocalGridNew[grid.nQ1][i][j][k]=dA_sq*grid.dLocalGridNew[grid.nD][i][j][k]
              *dDVDt_mthreshold*dDVDt_mthreshold;
          }
          else{
            grid.dLocalGridNew[grid.nQ1][i][j][k]=0.0;
          }
          
          //calculate Q2
          dDVDt=(grid.dLocalGridNew[grid.nW][i][j][nKInt]
            -grid.dLocalGridNew[grid.nW][i][j][nKInt-1]);
          if(dDVDt<-1.0*dDVDtThreshold){//being compressed
            dDVDt_mthreshold=dDVDt+dDVDtThreshold;
            grid.dLocalGridNew[grid.nQ2][i][j][k]=dA_sq*grid.dLocalGridNew[grid.nD][i][j][k]
              *dDVDt_mthreshold*dDVDt_mthreshold;
          }
          else{
            grid.dLocalGridNew[grid.nQ2][i][j][k]=0.0;
          }

        }
      }
    }
  #endif
}
void calNewQ0Q1Q2_RTP_GL(Grid& grid,Parameters &parameters){
  
  double dA_sq=parameters.dA*parameters.dA;
  double dDVDt;
  double dA_ip1half;
  double dA_im1half;
  double dA_jp1half;
  double dA_jm1half;
  double dA_j;
  double dR_i;
  int nIInt;
  int nJInt;
  int nKInt;
  double dC;
  double dDVDtThreshold;
  double dR_i_sq;
  double dDVDt_mthreshold;
  int i;
  int j;
  int k;
  
  //in the main grid
  for(i=grid.nStartUpdateExplicit[grid.nQ0][0];i<grid.nEndUpdateExplicit[grid.nQ0][0];i++){
    
    //calculate i for interface centered quantities
    nIInt=i+grid.nCenIntOffset[0];
    dR_i=(grid.dLocalGridNew[grid.nR][nIInt][0][0]+grid.dLocalGridNew[grid.nR][nIInt-1][0][0])*0.5;
    dR_i_sq=dR_i*dR_i;
    dA_ip1half=grid.dLocalGridNew[grid.nR][nIInt][0][0]*grid.dLocalGridNew[grid.nR][nIInt][0][0];
    dA_im1half=grid.dLocalGridNew[grid.nR][nIInt-1][0][0]
      *grid.dLocalGridNew[grid.nR][nIInt-1][0][0];
    
    for(j=grid.nStartUpdateExplicit[grid.nQ0][1];j<grid.nEndUpdateExplicit[grid.nQ0][1];j++){
      
      //calculate j for interface centered quantities
      nJInt=j+grid.nCenIntOffset[1];
      dA_jp1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt][0];
      dA_jm1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt-1][0];
      dA_j=grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0];
      
      for(k=grid.nStartUpdateExplicit[grid.nQ0][2];k<grid.nEndUpdateExplicit[grid.nQ0][2];k++){
        
        nKInt=k+grid.nCenIntOffset[2];
        
        //calculate threshold compression to turn viscosity on at
        dC=sqrt(parameters.dGamma*(grid.dLocalGridNew[grid.nP][i][j][k])
          /grid.dLocalGridNew[grid.nD][i][j][k]);
        dDVDtThreshold=parameters.dAVThreshold*dC;
        
        //calculate Q0
        dDVDt=(dA_ip1half*grid.dLocalGridNew[grid.nU][nIInt][j][k]
          -dA_im1half*grid.dLocalGridNew[grid.nU][nIInt-1][j][k])/dR_i_sq;
        if(dDVDt<-1.0*dDVDtThreshold){//being compressed
          dDVDt_mthreshold=dDVDt+dDVDtThreshold;
          grid.dLocalGridNew[grid.nQ0][i][j][k]=dA_sq*grid.dLocalGridNew[grid.nD][i][j][k]
            *dDVDt_mthreshold*dDVDt_mthreshold;
        }
        else{
          grid.dLocalGridNew[grid.nQ0][i][j][k]=0.0;
        }
        
        //calculate Q1
        dDVDt=(dA_jp1half*grid.dLocalGridNew[grid.nV][i][nJInt][k]
          -dA_jm1half*grid.dLocalGridNew[grid.nV][i][nJInt-1][k])/dA_j;
        if(dDVDt<-1.0*dDVDtThreshold){//being compressed
          dDVDt_mthreshold=dDVDt+dDVDtThreshold;
          grid.dLocalGridNew[grid.nQ1][i][j][k]=dA_sq*grid.dLocalGridNew[grid.nD][i][j][k]
            *dDVDt_mthreshold*dDVDt_mthreshold;
        }
        else{
          grid.dLocalGridNew[grid.nQ1][i][j][k]=0.0;
        }
        
        //calculate Q2
        dDVDt=(grid.dLocalGridNew[grid.nW][i][j][nKInt]
          -grid.dLocalGridNew[grid.nW][i][j][nKInt-1]);
        if(dDVDt<-1.0*dDVDtThreshold){//being compressed
          dDVDt_mthreshold=dDVDt+dDVDtThreshold;
          grid.dLocalGridNew[grid.nQ2][i][j][k]=dA_sq*grid.dLocalGridNew[grid.nD][i][j][k]
            *dDVDt_mthreshold*dDVDt_mthreshold;
        }
        else{
          grid.dLocalGridNew[grid.nQ2][i][j][k]=0.0;
        }
        
      }
    }
  }
  
  //outter ghost region
  for(i=grid.nStartGhostUpdateExplicit[grid.nQ0][0][0];
    i<grid.nEndGhostUpdateExplicit[grid.nQ0][0][0];i++){
    
    //calculate i for interface centered quantities
    nIInt=i+grid.nCenIntOffset[0];
    dR_i=(grid.dLocalGridNew[grid.nR][nIInt][0][0]+grid.dLocalGridNew[grid.nR][nIInt-1][0][0])*0.5;
    dR_i_sq=dR_i*dR_i;
    dA_ip1half=grid.dLocalGridNew[grid.nR][nIInt][0][0]*grid.dLocalGridNew[grid.nR][nIInt][0][0];
    dA_im1half=grid.dLocalGridNew[grid.nR][nIInt-1][0][0]
      *grid.dLocalGridNew[grid.nR][nIInt-1][0][0];
    
    for(j=grid.nStartGhostUpdateExplicit[grid.nQ0][0][1];
      j<grid.nEndGhostUpdateExplicit[grid.nQ0][0][1];j++){
      
      //calculate j for interface centered quantities
      nJInt=j+grid.nCenIntOffset[1];
      
      for(k=grid.nStartGhostUpdateExplicit[grid.nQ0][0][2];
        k<grid.nEndGhostUpdateExplicit[grid.nQ0][0][2];k++){
        
        nKInt=k+grid.nCenIntOffset[2];
        
        //calculate threshold compression to turn viscosity on at
        dC=sqrt(parameters.dGamma*(grid.dLocalGridNew[grid.nP][i][j][k])
          /grid.dLocalGridNew[grid.nD][i][j][k]);
        dDVDtThreshold=parameters.dAVThreshold*dC;
        
        //calculate Q0
        dDVDt=(dA_ip1half*grid.dLocalGridNew[grid.nU][nIInt][j][k]
          -dA_im1half*grid.dLocalGridNew[grid.nU][nIInt-1][j][k])/dR_i_sq;
        if(dDVDt<-1.0*dDVDtThreshold){//being compressed
          dDVDt_mthreshold=dDVDt+dDVDtThreshold;
          grid.dLocalGridNew[grid.nQ0][i][j][k]=dA_sq*grid.dLocalGridNew[grid.nD][i][j][k]
            *dDVDt_mthreshold*dDVDt_mthreshold;
        }
        else{
          grid.dLocalGridNew[grid.nQ0][i][j][k]=0.0;
        }
        
        //calculate Q1
        dDVDt=(dA_jp1half*grid.dLocalGridNew[grid.nV][i][nJInt][k]
          -dA_jm1half*grid.dLocalGridNew[grid.nV][i][nJInt-1][k])/dA_j;
        if(dDVDt<-1.0*dDVDtThreshold){//being compressed
          dDVDt_mthreshold=dDVDt+dDVDtThreshold;
          grid.dLocalGridNew[grid.nQ1][i][j][k]=dA_sq*grid.dLocalGridNew[grid.nD][i][j][k]
            *dDVDt_mthreshold*dDVDt_mthreshold;
        }
        else{
          grid.dLocalGridNew[grid.nQ1][i][j][k]=0.0;
        }
        
        //calculate Q2
        dDVDt=(grid.dLocalGridNew[grid.nW][i][j][nKInt]
          -grid.dLocalGridNew[grid.nW][i][j][nKInt-1]);
        if(dDVDt<-1.0*dDVDtThreshold){//being compressed
          dDVDt_mthreshold=dDVDt+dDVDtThreshold;
          grid.dLocalGridNew[grid.nQ2][i][j][k]=dA_sq*grid.dLocalGridNew[grid.nD][i][j][k]
            *dDVDt_mthreshold*dDVDt_mthreshold;
        }
        else{
          grid.dLocalGridNew[grid.nQ2][i][j][k]=0.0;
        }
      }
    }
  }
  
  //inner ghost region
  #if SEDOV==1
    for(i=grid.nStartGhostUpdateExplicit[grid.nQ0][1][0];
      i<grid.nEndGhostUpdateExplicit[grid.nQ0][1][0];i++){
      
      //calculate i for interface centered quantities
      nIInt=i+grid.nCenIntOffset[0];
      dR_i=(grid.dLocalGridNew[grid.nR][nIInt][0][0]+grid.dLocalGridNew[grid.nR][nIInt-1][0][0])*0.5;
      dR_i_sq=dR_i*dR_i;
      dA_ip1half=grid.dLocalGridNew[grid.nR][nIInt][0][0]*grid.dLocalGridNew[grid.nR][nIInt][0][0];
      dA_im1half=grid.dLocalGridNew[grid.nR][nIInt-1][0][0]
        *grid.dLocalGridNew[grid.nR][nIInt-1][0][0];
      
      for(j=grid.nStartGhostUpdateExplicit[grid.nQ0][1][1];
        j<grid.nEndGhostUpdateExplicit[grid.nQ0][1][1];j++){
        
        //calculate j for interface centered quantities
        nJInt=j+grid.nCenIntOffset[1];
        
        for(k=grid.nStartGhostUpdateExplicit[grid.nQ0][1][2];
          k<grid.nEndGhostUpdateExplicit[grid.nQ0][1][2];k++){
          
          nKInt=k+grid.nCenIntOffset[2];
          
          //calculate threshold compression to turn viscosity on at
          dC=sqrt(parameters.dGamma*(grid.dLocalGridNew[grid.nP][i][j][k])
            /grid.dLocalGridNew[grid.nD][i][j][k]);
          dDVDtThreshold=parameters.dAVThreshold*dC;
          
          //calculate Q0
          dDVDt=(dA_ip1half*grid.dLocalGridNew[grid.nU][nIInt][j][k]
            -dA_im1half*grid.dLocalGridNew[grid.nU][nIInt-1][j][k])/dR_i_sq;
          if(dDVDt<-1.0*dDVDtThreshold){//being compressed
            dDVDt_mthreshold=dDVDt+dDVDtThreshold;
            grid.dLocalGridNew[grid.nQ0][i][j][k]=dA_sq*grid.dLocalGridNew[grid.nD][i][j][k]
              *dDVDt_mthreshold*dDVDt_mthreshold;
          }
          else{
            grid.dLocalGridNew[grid.nQ0][i][j][k]=0.0;
          }
          
          //calculate Q1
          dDVDt=(dA_jp1half*grid.dLocalGridNew[grid.nV][i][nJInt][k]
            -dA_jm1half*grid.dLocalGridNew[grid.nV][i][nJInt-1][k])/dA_j;
          if(dDVDt<-1.0*dDVDtThreshold){//being compressed
            dDVDt_mthreshold=dDVDt+dDVDtThreshold;
            grid.dLocalGridNew[grid.nQ1][i][j][k]=dA_sq*grid.dLocalGridNew[grid.nD][i][j][k]
              *dDVDt_mthreshold*dDVDt_mthreshold;
          }
          else{
            grid.dLocalGridNew[grid.nQ1][i][j][k]=0.0;
          }
          
          //calculate Q2
          dDVDt=(grid.dLocalGridNew[grid.nW][i][j][nKInt]
            -grid.dLocalGridNew[grid.nW][i][j][nKInt-1]);
          if(dDVDt<-1.0*dDVDtThreshold){//being compressed
            dDVDt_mthreshold=dDVDt+dDVDtThreshold;
            grid.dLocalGridNew[grid.nQ2][i][j][k]=dA_sq*grid.dLocalGridNew[grid.nD][i][j][k]
              *dDVDt_mthreshold*dDVDt_mthreshold;
          }
          else{
            grid.dLocalGridNew[grid.nQ2][i][j][k]=0.0;
          }
        }
      }
    }
  #endif
}
void calNewEddyVisc_None(Grid &grid, Parameters &parameters){
  //call if there is no eddy viscosity model being used
}
void calNewEddyVisc_R_CN(Grid &grid, Parameters &parameters){
  
  int i;
  int j;
  int k;
  int nIInt;//i+1/2 index
  double dR_ip1half_np1half;
  double dR_im1half_np1half;
  double dDelR_i_np1half;
  double dConstant=parameters.dEddyViscosity;
  double dLengthScaleSq;
  
  //main grid explicit
  for(i=grid.nStartUpdateExplicit[grid.nEddyVisc][0];
    i<grid.nEndUpdateExplicit[grid.nEddyVisc][0];i++){
      
    //calculate i for interface centered quantities
    nIInt=i+grid.nCenIntOffset[0];
    dR_ip1half_np1half=grid.dLocalGridNew[grid.nR][nIInt][0][0];
    dR_im1half_np1half=grid.dLocalGridNew[grid.nR][nIInt-1][0][0];
    dDelR_i_np1half=dR_ip1half_np1half-dR_im1half_np1half;
    
    for(j=grid.nStartUpdateExplicit[grid.nEddyVisc][1];
      j<grid.nEndUpdateExplicit[grid.nEddyVisc][1];j++){
      for(k=grid.nStartUpdateExplicit[grid.nEddyVisc][2];
        k<grid.nEndUpdateExplicit[grid.nEddyVisc][2];k++){
        dLengthScaleSq=dDelR_i_np1half*dDelR_i_np1half;
        grid.dLocalGridNew[grid.nEddyVisc][i][j][k]=dLengthScaleSq*dConstant
          *parameters.dMaxConvectiveVelocity/1.0e6;
      }
    }
  }
  
  //outter radial ghost cells,explicit
  for(i=grid.nStartGhostUpdateExplicit[grid.nEddyVisc][0][0];
    i<grid.nEndGhostUpdateExplicit[grid.nEddyVisc][0][0];i++){
      
    //calculate i for interface centered quantities
    nIInt=i+grid.nCenIntOffset[0];
    dR_ip1half_np1half=grid.dLocalGridNew[grid.nR][nIInt][0][0];
    dR_im1half_np1half=grid.dLocalGridNew[grid.nR][nIInt-1][0][0];
    dDelR_i_np1half=dR_ip1half_np1half-dR_im1half_np1half;
    
    for(j=grid.nStartGhostUpdateExplicit[grid.nEddyVisc][0][1];
      j<grid.nEndGhostUpdateExplicit[grid.nEddyVisc][0][1];j++){
      for(k=grid.nStartGhostUpdateExplicit[grid.nEddyVisc][0][2];
        k<grid.nEndGhostUpdateExplicit[grid.nEddyVisc][0][2];k++){
        dLengthScaleSq=dDelR_i_np1half*dDelR_i_np1half;
        grid.dLocalGridNew[grid.nEddyVisc][i][j][k]=dLengthScaleSq*dConstant
          *parameters.dMaxConvectiveVelocity/1.0e6;
      }
    }
  }
}
void calNewEddyVisc_RT_CN(Grid &grid, Parameters &parameters){
  
  int i;
  int j;
  int k;
  int nIInt;//i+1/2 index
  double dR_ip1half_np1half;
  double dR_im1half_np1half;
  double dR_i_np1half;
  double dDelR_i_np1half;
  double dConstant=parameters.dEddyViscosity;
  double dLengthScaleSq;
  
  //main grid explicit
  for(i=grid.nStartUpdateExplicit[grid.nEddyVisc][0];
    i<grid.nEndUpdateExplicit[grid.nEddyVisc][0];i++){
      
    //calculate i for interface centered quantities
    nIInt=i+grid.nCenIntOffset[0];
    dR_ip1half_np1half=grid.dLocalGridNew[grid.nR][nIInt][0][0];
    dR_im1half_np1half=grid.dLocalGridNew[grid.nR][nIInt-1][0][0];
    dR_i_np1half=(dR_ip1half_np1half+dR_im1half_np1half)*0.5;
    dDelR_i_np1half=dR_ip1half_np1half-dR_im1half_np1half;
    
    for(j=grid.nStartUpdateExplicit[grid.nEddyVisc][1];
      j<grid.nEndUpdateExplicit[grid.nEddyVisc][1];j++){
      for(k=grid.nStartUpdateExplicit[grid.nEddyVisc][2];
        k<grid.nEndUpdateExplicit[grid.nEddyVisc][2];k++){
        dLengthScaleSq=dDelR_i_np1half*dR_i_np1half*grid.dLocalGridOld[grid.nDTheta][0][j][0];
        grid.dLocalGridNew[grid.nEddyVisc][i][j][k]=dLengthScaleSq*dConstant
          *parameters.dMaxConvectiveVelocity/1.0e6;
     }
    }
  }
  
  //outter radial ghost cells,explicit
  for(i=grid.nStartGhostUpdateExplicit[grid.nEddyVisc][0][0];
    i<grid.nEndGhostUpdateExplicit[grid.nEddyVisc][0][0];i++){
      
    //calculate i for interface centered quantities
    nIInt=i+grid.nCenIntOffset[0];
    dR_ip1half_np1half=grid.dLocalGridNew[grid.nR][nIInt][0][0];
    dR_im1half_np1half=grid.dLocalGridNew[grid.nR][nIInt-1][0][0];
    dR_i_np1half=(dR_ip1half_np1half+dR_im1half_np1half)*0.5;
    dDelR_i_np1half=dR_ip1half_np1half-dR_im1half_np1half;
    
    for(j=grid.nStartGhostUpdateExplicit[grid.nEddyVisc][0][1];
      j<grid.nEndGhostUpdateExplicit[grid.nEddyVisc][0][1];j++){
      for(k=grid.nStartGhostUpdateExplicit[grid.nEddyVisc][0][2];
        k<grid.nEndGhostUpdateExplicit[grid.nEddyVisc][0][2];k++){
        dLengthScaleSq=dDelR_i_np1half*dR_i_np1half*grid.dLocalGridOld[grid.nDTheta][0][j][0];
        grid.dLocalGridNew[grid.nEddyVisc][i][j][k]=dLengthScaleSq*dConstant
          *parameters.dMaxConvectiveVelocity/1.0e6;
      }
    }
  }
}
void calNewEddyVisc_RTP_CN(Grid &grid, Parameters &parameters){
  int i;
  int j;
  int k;
  int nIInt;//i+1/2 index
  double dR_ip1half_np1half;
  double dR_im1half_np1half;
  double dR_i_np1half;
  double dR_i_np1half_Sq;
  double dDelR_i_np1half;
  double dConstant=parameters.dEddyViscosity;
  double dLengthScaleSq;
  
  //main grid
  for(i=grid.nStartUpdateExplicit[grid.nEddyVisc][0];
    i<grid.nEndUpdateExplicit[grid.nEddyVisc][0];i++){
      
    //calculate i for interface centered quantities
    nIInt=i+grid.nCenIntOffset[0];
    dR_ip1half_np1half=grid.dLocalGridNew[grid.nR][nIInt][0][0];
    dR_im1half_np1half=grid.dLocalGridNew[grid.nR][nIInt-1][0][0];
    dR_i_np1half=(dR_ip1half_np1half+dR_im1half_np1half)*0.5;
    dR_i_np1half_Sq=dR_i_np1half*dR_i_np1half;
    dDelR_i_np1half=dR_ip1half_np1half-dR_im1half_np1half;
    
    for(j=grid.nStartUpdateExplicit[grid.nEddyVisc][1];
      j<grid.nEndUpdateExplicit[grid.nEddyVisc][1];j++){
      for(k=grid.nStartUpdateExplicit[grid.nEddyVisc][2];
        k<grid.nEndUpdateExplicit[grid.nEddyVisc][2];k++){
        dLengthScaleSq=dR_i_np1half_Sq*dDelR_i_np1half*grid.dLocalGridOld[grid.nDTheta][0][j][0]
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]*grid.dLocalGridOld[grid.nDPhi][0][0][k];
        dLengthScaleSq=pow(dLengthScaleSq,0.666666666666666);
        grid.dLocalGridNew[grid.nEddyVisc][i][j][k]=dConstant*dLengthScaleSq
          *parameters.dMaxConvectiveVelocity/1.0e6;
      }
    }
  }
  
  //outter radial ghost cells,explicit
  for(int i=grid.nStartGhostUpdateExplicit[grid.nEddyVisc][0][0];
    i<grid.nEndGhostUpdateExplicit[grid.nEddyVisc][0][0];i++){
    
    //calculate i for interface centered quantities
    nIInt=i+grid.nCenIntOffset[0];
    dR_ip1half_np1half=grid.dLocalGridNew[grid.nR][nIInt][0][0];
    dR_im1half_np1half=grid.dLocalGridNew[grid.nR][nIInt-1][0][0];
    dR_i_np1half=(dR_ip1half_np1half+dR_im1half_np1half)*0.5;
    dDelR_i_np1half=dR_ip1half_np1half-dR_im1half_np1half;
    
    for(int j=grid.nStartGhostUpdateExplicit[grid.nEddyVisc][0][1];
      j<grid.nEndGhostUpdateExplicit[grid.nEddyVisc][0][1];j++){
      for(int k=grid.nStartGhostUpdateExplicit[grid.nEddyVisc][0][2];
        k<grid.nEndGhostUpdateExplicit[grid.nEddyVisc][0][2];k++){
        dLengthScaleSq=dR_i_np1half_Sq*dDelR_i_np1half*grid.dLocalGridOld[grid.nDTheta][0][j][0]
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]*grid.dLocalGridOld[grid.nDPhi][0][0][k];
        dLengthScaleSq=pow(dLengthScaleSq,0.666666666666666);
        grid.dLocalGridNew[grid.nEddyVisc][i][j][k]=dConstant*dLengthScaleSq
          *parameters.dMaxConvectiveVelocity/1.0e6;
      }
    }
  }
}
void calNewEddyVisc_R_SM(Grid &grid, Parameters &parameters){
  
  int i;
  int j;
  int k;
  int nIInt;//i+1/2 index
  double d1;//term 1
  double dA;
  double dR_ip1half_np1half;
  double dR_im1half_np1half;
  double dR_i_np1half;
  double dU_ip1halfjk_np1half;
  double dU_im1halfjk_np1half;
  double dDelR_i_np1half;
  double dConstantSq=parameters.dEddyViscosity*parameters.dEddyViscosity/pow(2.0,0.5);
  double dLengthScaleSq;
  double dTerms;
  double dRho_ijk_np1half;
  
  //main grid explicit
  for(i=grid.nStartUpdateExplicit[grid.nEddyVisc][0];
    i<grid.nEndUpdateExplicit[grid.nEddyVisc][0];i++){
      
    //calculate i for interface centered quantities
    nIInt=i+grid.nCenIntOffset[0];
    dR_ip1half_np1half=grid.dLocalGridNew[grid.nR][nIInt][0][0];
    dR_im1half_np1half=grid.dLocalGridNew[grid.nR][nIInt-1][0][0];
    dR_i_np1half=(dR_ip1half_np1half+dR_im1half_np1half)*0.5;
    dDelR_i_np1half=dR_ip1half_np1half-dR_im1half_np1half;
    
    for(j=grid.nStartUpdateExplicit[grid.nEddyVisc][1];
      j<grid.nEndUpdateExplicit[grid.nEddyVisc][1];j++){
      for(k=grid.nStartUpdateExplicit[grid.nEddyVisc][2];
        k<grid.nEndUpdateExplicit[grid.nEddyVisc][2];k++){
        
        dLengthScaleSq=dDelR_i_np1half*dDelR_i_np1half;
        
        //interpolate
        dU_ip1halfjk_np1half=grid.dLocalGridNew[grid.nU][nIInt][j][k];
        dU_im1halfjk_np1half=grid.dLocalGridNew[grid.nU][nIInt-1][j][k];
        dRho_ijk_np1half=grid.dLocalGridNew[grid.nD][i][j][k];
        
        //calculate dA term
        d1=(dU_ip1halfjk_np1half-dU_im1halfjk_np1half)/(dR_ip1half_np1half-dR_im1half_np1half);
        dA=2.0*d1*d1;
        
        dTerms=dA;
        grid.dLocalGridNew[grid.nEddyVisc][i][j][k]=dLengthScaleSq*dConstantSq*dRho_ijk_np1half
          *pow(dTerms,0.5);
      }
    }
  }
  
  //outter radial ghost cells,explicit
  for(i=grid.nStartGhostUpdateExplicit[grid.nEddyVisc][0][0];
    i<grid.nEndGhostUpdateExplicit[grid.nEddyVisc][0][0];i++){
      
    //calculate i for interface centered quantities
    nIInt=i+grid.nCenIntOffset[0];
    dR_ip1half_np1half=grid.dLocalGridOld[grid.nR][nIInt][0][0];
    dR_im1half_np1half=grid.dLocalGridOld[grid.nR][nIInt-1][0][0];
    dR_i_np1half=(dR_ip1half_np1half+dR_im1half_np1half)*0.5;
    dDelR_i_np1half=dR_ip1half_np1half-dR_im1half_np1half;
    dLengthScaleSq=dDelR_i_np1half*dDelR_i_np1half;
    for(j=grid.nStartGhostUpdateExplicit[grid.nEddyVisc][0][1];
      j<grid.nEndGhostUpdateExplicit[grid.nEddyVisc][0][1];j++){
      for(k=grid.nStartGhostUpdateExplicit[grid.nEddyVisc][0][2];
        k<grid.nEndGhostUpdateExplicit[grid.nEddyVisc][0][2];k++){
        
        
        //interpolate
        dU_ip1halfjk_np1half=grid.dLocalGridNew[grid.nU][nIInt][j][k];
        dU_im1halfjk_np1half=grid.dLocalGridNew[grid.nU][nIInt-1][j][k];
        dRho_ijk_np1half=grid.dLocalGridNew[grid.nD][i][j][k];
        
        //calculate dA term
        d1=(dU_ip1halfjk_np1half-dU_im1halfjk_np1half)/(dR_ip1half_np1half-dR_im1half_np1half);
        dA=2.0*d1*d1;
        
        dTerms=dA;
        grid.dLocalGridNew[grid.nEddyVisc][i][j][k]=dLengthScaleSq*dConstantSq*dRho_ijk_np1half
          *pow(dTerms,0.5);
      }
    }
  }
}
void calNewEddyVisc_RT_SM(Grid &grid, Parameters &parameters){
  
  int i;
  int j;
  int k;
  int nIInt;//i+1/2 index
  int nJInt;//j+1/2 index
  double d1;//term 1
  double d2;//term 2
  double d3;//term 3
  double d7;//term 7
  double d8;//term 8
  double d9;//term 9
  double dA;//term A
  double dB;//term B
  double dD;//term D
  double dE;//term E
  double dTerms;
  double dR_ip1half_np1half;
  double dR_im1half_np1half;
  double dR_i_np1half;
  double dU_ip1halfjk_np1half;
  double dU_im1halfjk_np1half;
  double dU_ijp1halfk_np1half;
  double dU_ijm1halfk_np1half;
  double dU_ijk_np1half;
  double dV_ijk_np1half;
  double dV_ip1halfjk_np1half;
  double dV_im1halfjk_np1half;
  double dV_ijp1halfk_np1half;
  double dV_ijm1halfk_np1half;
  double dDelR_i_np1half;
  double dD_ijk_np1half;
  double dConstantSq=parameters.dEddyViscosity*parameters.dEddyViscosity/pow(2.0,0.5);
  double dLengthScaleSq;
  double dU0_i_np1half;
  
  //main grid explicit
  for(i=grid.nStartUpdateExplicit[grid.nEddyVisc][0];
    i<grid.nEndUpdateExplicit[grid.nEddyVisc][0];i++){
      
    //calculate i for interface centered quantities
    nIInt=i+grid.nCenIntOffset[0];
    dR_ip1half_np1half=grid.dLocalGridNew[grid.nR][nIInt][0][0];
    dR_im1half_np1half=grid.dLocalGridNew[grid.nR][nIInt-1][0][0];
    dR_i_np1half=(dR_ip1half_np1half+dR_im1half_np1half)*0.5;
    dDelR_i_np1half=dR_ip1half_np1half-dR_im1half_np1half;
    dU0_i_np1half=(grid.dLocalGridNew[grid.nU0][nIInt][0][0]
      +grid.dLocalGridNew[grid.nU0][nIInt-1][0][0])*0.5;
    
    for(j=grid.nStartUpdateExplicit[grid.nEddyVisc][1];
      j<grid.nEndUpdateExplicit[grid.nEddyVisc][1];j++){
      
      nJInt=j+grid.nCenIntOffset[1];
      
      for(k=grid.nStartUpdateExplicit[grid.nEddyVisc][2];
        k<grid.nEndUpdateExplicit[grid.nEddyVisc][2];k++){
        
        dLengthScaleSq=dDelR_i_np1half*dR_i_np1half*grid.dLocalGridOld[grid.nDTheta][0][j][0];
        
        //interpolate
        dU_ip1halfjk_np1half=grid.dLocalGridNew[grid.nU][nIInt][j][k];
        dU_im1halfjk_np1half=grid.dLocalGridNew[grid.nU][nIInt-1][j][k];
        dU_ijk_np1half=(grid.dLocalGridNew[grid.nU][nIInt][j][k]
          +grid.dLocalGridNew[grid.nU][nIInt-1][j][k])*0.5;
        dU_ijp1halfk_np1half=(grid.dLocalGridNew[grid.nU][nIInt][j][k]
          +grid.dLocalGridNew[grid.nU][nIInt-1][j][k]+grid.dLocalGridNew[grid.nU][nIInt][j+1][k]
          +grid.dLocalGridNew[grid.nU][nIInt-1][j+1][k])*0.25;
        dU_ijm1halfk_np1half=(grid.dLocalGridNew[grid.nU][nIInt][j][k]
          +grid.dLocalGridNew[grid.nU][nIInt-1][j][k]+grid.dLocalGridNew[grid.nU][nIInt][j-1][k]
          +grid.dLocalGridNew[grid.nU][nIInt-1][j-1][k])*0.25;
        dV_ijk_np1half=(grid.dLocalGridNew[grid.nV][i][nJInt][k]
          +grid.dLocalGridNew[grid.nV][i][nJInt-1][k])*0.5;
        dV_ip1halfjk_np1half=(grid.dLocalGridNew[grid.nV][i][nJInt][k]
          +grid.dLocalGridNew[grid.nV][i][nJInt-1][k]+grid.dLocalGridNew[grid.nV][i+1][nJInt][k]
          +grid.dLocalGridNew[grid.nV][i+1][nJInt-1][k])*0.25;
        dV_im1halfjk_np1half=(grid.dLocalGridNew[grid.nV][i][nJInt][k]
          +grid.dLocalGridNew[grid.nV][i][nJInt-1][k]+grid.dLocalGridNew[grid.nV][i-1][nJInt][k]
          +grid.dLocalGridNew[grid.nV][i-1][nJInt-1][k])*0.25;
        dV_ijp1halfk_np1half=grid.dLocalGridNew[grid.nV][i][nJInt][k];
        dV_ijm1halfk_np1half=grid.dLocalGridNew[grid.nV][i][nJInt-1][k];
        dD_ijk_np1half=grid.dLocalGridNew[grid.nD][i][j][k];
        
        //term 1
        d1=((dU_ip1halfjk_np1half-grid.dLocalGridNew[grid.nU0][nIInt][0][0])
          -(dU_im1halfjk_np1half-grid.dLocalGridNew[grid.nU0][nIInt-1][0][0]))
          /(dR_ip1half_np1half-dR_im1half_np1half);
        
        //term 2
        d2=1.0/dR_i_np1half*(dU_ijp1halfk_np1half-dU_ijm1halfk_np1half)
          /grid.dLocalGridOld[grid.nDTheta][0][j][0];
        
        //term 3
        d3=dV_ijk_np1half/dR_i_np1half;
        
        //term 7
        d7=(dV_ip1halfjk_np1half-dV_im1halfjk_np1half)/dDelR_i_np1half;
        
        //term 8
        d8=1.0/dR_i_np1half*(dV_ijp1halfk_np1half-dV_ijm1halfk_np1half)
          /grid.dLocalGridOld[grid.nDTheta][0][j][0];
        
        //term 9
        d9=(dU_ijk_np1half-dU0_i_np1half)/dR_i_np1half;
        
        //term A
        dA=2.0*d1*d1;
        
        //term B
        dB=(d2+d1-d3)*(d2-d3);
        
        //term D
        dD=d7*(d2+d7-d3);
        
        //term E
        dE=d8+d9;
        dE=2.0*dE*dE;
        
        dTerms=dA+dB+dD+dE;
        grid.dLocalGridNew[grid.nEddyVisc][i][j][k]=dLengthScaleSq*dConstantSq*dD_ijk_np1half
          *pow(dTerms,0.5);
      }
    }
  }
  
  //outter radial ghost cells,explicit
  for(i=grid.nStartGhostUpdateExplicit[grid.nEddyVisc][0][0];
    i<grid.nEndGhostUpdateExplicit[grid.nEddyVisc][0][0];i++){
      
    //calculate i for interface centered quantities
    nIInt=i+grid.nCenIntOffset[0];
    dR_ip1half_np1half=grid.dLocalGridNew[grid.nR][nIInt][0][0];
    dR_im1half_np1half=grid.dLocalGridNew[grid.nR][nIInt-1][0][0];
    dR_i_np1half=(dR_ip1half_np1half+dR_im1half_np1half)*0.5;
    dDelR_i_np1half=dR_ip1half_np1half-dR_im1half_np1half;
    dU0_i_np1half=(grid.dLocalGridNew[grid.nU0][nIInt][0][0]
      +grid.dLocalGridNew[grid.nU0][nIInt-1][0][0])*0.5;
    
    for(j=grid.nStartGhostUpdateExplicit[grid.nEddyVisc][0][1];
      j<grid.nEndGhostUpdateExplicit[grid.nEddyVisc][0][1];j++){
      
      nJInt=j+grid.nCenIntOffset[1];
      
      for(k=grid.nStartGhostUpdateExplicit[grid.nEddyVisc][0][2];
        k<grid.nEndGhostUpdateExplicit[grid.nEddyVisc][0][2];k++){
        
        dLengthScaleSq=dDelR_i_np1half*dR_i_np1half*grid.dLocalGridOld[grid.nDTheta][0][j][0];
        
        //interpolate
        dU_ip1halfjk_np1half=grid.dLocalGridNew[grid.nU][nIInt][j][k];
        dU_im1halfjk_np1half=grid.dLocalGridNew[grid.nU][nIInt-1][j][k];
        dU_ijk_np1half=(grid.dLocalGridNew[grid.nU][nIInt][j][k]
          +grid.dLocalGridNew[grid.nU][nIInt-1][j][k])*0.5;
        dU_ijp1halfk_np1half=(grid.dLocalGridNew[grid.nU][nIInt][j][k]
          +grid.dLocalGridNew[grid.nU][nIInt-1][j][k]+grid.dLocalGridNew[grid.nU][nIInt][j+1][k]
          +grid.dLocalGridNew[grid.nU][nIInt-1][j+1][k])*0.25;
        dU_ijm1halfk_np1half=(grid.dLocalGridNew[grid.nU][nIInt][j][k]
          +grid.dLocalGridNew[grid.nU][nIInt-1][j][k]+grid.dLocalGridNew[grid.nU][nIInt][j-1][k]
          +grid.dLocalGridNew[grid.nU][nIInt-1][j-1][k])*0.25;
        dV_ijk_np1half=(grid.dLocalGridNew[grid.nV][i][nJInt][k]
          +grid.dLocalGridNew[grid.nV][i][nJInt-1][k])*0.5;
        dV_ip1halfjk_np1half=(grid.dLocalGridNew[grid.nV][i][nJInt][k]
          +grid.dLocalGridNew[grid.nV][i][nJInt-1][k])*0.25;
        dV_im1halfjk_np1half=(grid.dLocalGridNew[grid.nV][i][nJInt][k]
          +grid.dLocalGridNew[grid.nV][i][nJInt-1][k]+grid.dLocalGridNew[grid.nV][i-1][nJInt][k]
          +grid.dLocalGridNew[grid.nV][i-1][nJInt-1][k])*0.25;
        dV_ijp1halfk_np1half=grid.dLocalGridNew[grid.nV][i][nJInt][k];
        dV_ijm1halfk_np1half=grid.dLocalGridNew[grid.nV][i][nJInt-1][k];
        dD_ijk_np1half=grid.dLocalGridNew[grid.nD][i][j][k];
        
        //term 1
        d1=((dU_ip1halfjk_np1half-grid.dLocalGridNew[grid.nU0][nIInt][0][0])
          -(dU_im1halfjk_np1half-grid.dLocalGridNew[grid.nU0][nIInt-1][0][0]))
          /(dR_ip1half_np1half-dR_im1half_np1half);
        
        //term 2
        d2=1.0/dR_i_np1half*(dU_ijp1halfk_np1half-dU_ijm1halfk_np1half)
          /grid.dLocalGridOld[grid.nDTheta][0][j][0];
        
        //term 3
        d3=dV_ijk_np1half/dR_i_np1half;
        
        //term 7
        d7=(dV_ip1halfjk_np1half-dV_im1halfjk_np1half)/dDelR_i_np1half;
        
        //term 8
        d8=1.0/dR_i_np1half*(dV_ijp1halfk_np1half-dV_ijm1halfk_np1half)
          /grid.dLocalGridOld[grid.nDTheta][0][j][0];
        
        //term 9
        d9=(dU_ijk_np1half-dU0_i_np1half)/dR_i_np1half;
        
        //term A
        dA=2.0*d1*d1;
        
        //term B
        dB=(d2+d1-d3)*(d2-d3);
        
        //term D
        dD=d7*(d2+d7-d3);
        
        //term E
        dE=d8+d9;
        dE=2.0*dE*dE;
        
        dTerms=dA+dB+dD+dE;
        grid.dLocalGridNew[grid.nEddyVisc][i][j][k]=dLengthScaleSq*dConstantSq*dD_ijk_np1half
          *pow(dTerms,0.5);
      }
    }
  }
}
void calNewEddyVisc_RTP_SM(Grid &grid, Parameters &parameters){
  int i;
  int j;
  int k;
  int nIInt;//i+1/2 index
  int nJInt;//j+1/2 index
  int nKInt;//k+1/2 index
  double d1;
  double d2;
  double d3;
  double d4;
  double d5;
  double d6;
  double d7;
  double d8;
  double d9;
  double d10;
  double d11;
  double d12;
  double d13;
  double d14;
  double dA;
  double dB;
  double dC;
  double dD;
  double dE;
  double dF;
  double dG;
  double dH;
  double dI;
  double dTerms;
  double dR_ip1half_np1half;
  double dR_im1half_np1half;
  double dR_i_np1half;
  double dR_i_np1half_Sq;
  double dU0_i_np1half;
  double dU_ip1halfjk_np1half;
  double dU_im1halfjk_np1half;
  double dU_ijp1halfk_np1half;
  double dU_ijm1halfk_np1half;
  double dU_ijk_np1half;
  double dU_ijkp1half_np1half;
  double dU_ijkm1half_np1half;
  double dV_ijk_np1half;
  double dV_ip1halfjk_np1half;
  double dV_im1halfjk_np1half;
  double dV_ijp1halfk_np1half;
  double dV_ijm1halfk_np1half;
  double dV_ijkp1half_np1half;
  double dV_ijkm1half_np1half;
  double dW_ijk_np1half;
  double dW_ip1halfjk_np1half;
  double dW_ijp1halfk_np1half;
  double dW_ijkp1half_np1half;
  double dW_ijkm1half_np1half;
  double dDelR_i_np1half;
  double dConstant=parameters.dEddyViscosity*parameters.dEddyViscosity/pow(2.0,0.5);
  double dLengthScaleSq;
  double dW_im1halfjk_np1half;
  double dW_ijm1halfk_np1half;
  //main grid
  for(i=grid.nStartUpdateExplicit[grid.nEddyVisc][0];
    i<grid.nEndUpdateExplicit[grid.nEddyVisc][0];i++){
      
    //calculate i for interface centered quantities
    nIInt=i+grid.nCenIntOffset[0];
    dR_ip1half_np1half=grid.dLocalGridNew[grid.nR][nIInt][0][0];
    dR_im1half_np1half=grid.dLocalGridNew[grid.nR][nIInt-1][0][0];
    dR_i_np1half=(dR_ip1half_np1half+dR_im1half_np1half)*0.5;
    dR_i_np1half_Sq=dR_i_np1half*dR_i_np1half;
    dDelR_i_np1half=dR_ip1half_np1half-dR_im1half_np1half;
    dU0_i_np1half=(grid.dLocalGridNew[grid.nU0][nIInt][0][0]
      +grid.dLocalGridNew[grid.nU0][nIInt-1][0][0])*0.5;
    for(j=grid.nStartUpdateExplicit[grid.nEddyVisc][1];
      j<grid.nEndUpdateExplicit[grid.nEddyVisc][1];j++){
      
      nJInt=j+grid.nCenIntOffset[1];
      
      for(k=grid.nStartUpdateExplicit[grid.nEddyVisc][2];
        k<grid.nEndUpdateExplicit[grid.nEddyVisc][2];k++){
        
        nKInt=k+grid.nCenIntOffset[2];
        dLengthScaleSq=dR_i_np1half_Sq*dDelR_i_np1half*grid.dLocalGridOld[grid.nDTheta][0][j][0]
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]*grid.dLocalGridOld[grid.nDPhi][0][0][k];
        dLengthScaleSq=pow(dLengthScaleSq,0.666666666666666);
        
        //interpolate
        dU_ip1halfjk_np1half=grid.dLocalGridNew[grid.nU][nIInt][j][k];
        dU_im1halfjk_np1half=grid.dLocalGridNew[grid.nU][nIInt-1][j][k];
        dU_ijk_np1half=(grid.dLocalGridNew[grid.nU][nIInt][j][k]
          +grid.dLocalGridNew[grid.nU][nIInt-1][j][k])*0.5;
        dU_ijkp1half_np1half=(grid.dLocalGridNew[grid.nU][nIInt][j][k+1]
          +grid.dLocalGridNew[grid.nU][nIInt][j][k]+grid.dLocalGridNew[grid.nU][nIInt-1][j][k+1]
          +grid.dLocalGridNew[grid.nU][nIInt-1][j][k])*0.25;
        dU_ijkm1half_np1half=(grid.dLocalGridNew[grid.nU][nIInt][j][k]
          +grid.dLocalGridNew[grid.nU][nIInt][j][k-1]+grid.dLocalGridNew[grid.nU][nIInt-1][j][k]
          +grid.dLocalGridNew[grid.nU][nIInt-1][j][k-1])*0.25;
        dU_ijp1halfk_np1half=(grid.dLocalGridNew[grid.nU][nIInt][j][k]
          +grid.dLocalGridNew[grid.nU][nIInt-1][j][k]+grid.dLocalGridNew[grid.nU][nIInt][j+1][k]
          +grid.dLocalGridNew[grid.nU][nIInt-1][j+1][k])*0.25;
        dU_ijm1halfk_np1half=(grid.dLocalGridNew[grid.nU][nIInt][j][k]
          +grid.dLocalGridNew[grid.nU][nIInt-1][j][k]+grid.dLocalGridNew[grid.nU][nIInt][j-1][k]
          +grid.dLocalGridNew[grid.nU][nIInt-1][j-1][k])*0.25;
        dV_ijk_np1half=(grid.dLocalGridNew[grid.nV][i][nJInt][k]
          +grid.dLocalGridNew[grid.nV][i][nJInt-1][k])*0.5;
        dV_ip1halfjk_np1half=(grid.dLocalGridNew[grid.nV][i][nJInt][k]
          +grid.dLocalGridNew[grid.nV][i][nJInt-1][k]+grid.dLocalGridNew[grid.nV][i+1][nJInt][k]
          +grid.dLocalGridNew[grid.nV][i+1][nJInt-1][k])*0.25;
        dV_im1halfjk_np1half=(grid.dLocalGridNew[grid.nV][i][nJInt][k]
          +grid.dLocalGridNew[grid.nV][i][nJInt-1][k]+grid.dLocalGridNew[grid.nV][i-1][nJInt][k]
          +grid.dLocalGridNew[grid.nV][i-1][nJInt-1][k])*0.25;
        dV_ijp1halfk_np1half=grid.dLocalGridNew[grid.nV][i][nJInt][k];
        dV_ijm1halfk_np1half=grid.dLocalGridNew[grid.nV][i][nJInt-1][k];
        dV_ijkp1half_np1half=(grid.dLocalGridNew[grid.nV][i][nJInt][k+1]
          +grid.dLocalGridNew[grid.nV][i][nJInt][k]
          +grid.dLocalGridNew[grid.nV][i][nJInt-1][k+1]
          +grid.dLocalGridNew[grid.nV][i][nJInt-1][k])*0.25;
        dV_ijkm1half_np1half=(grid.dLocalGridNew[grid.nV][i][nJInt][k]
          +grid.dLocalGridNew[grid.nV][i][nJInt][k-1]
          +grid.dLocalGridNew[grid.nV][i][nJInt-1][k]
          +grid.dLocalGridNew[grid.nV][i][nJInt-1][k-1])*0.25;
        dW_ijk_np1half=(grid.dLocalGridNew[grid.nW][i][j][nKInt]
          +grid.dLocalGridNew[grid.nW][i][j][nKInt-1])*0.5;
        dW_ip1halfjk_np1half=(grid.dLocalGridNew[grid.nW][i+1][j][nKInt]
          +grid.dLocalGridNew[grid.nW][i+1][j][nKInt-1]
          +grid.dLocalGridNew[grid.nW][i][j][nKInt]
          +grid.dLocalGridNew[grid.nW][i][j][nKInt-1])*0.25;
        dW_im1halfjk_np1half=(grid.dLocalGridNew[grid.nW][i-1][j][nKInt]
          +grid.dLocalGridNew[grid.nW][i-1][j][nKInt-1]
          +grid.dLocalGridNew[grid.nW][i][j][nKInt]
          +grid.dLocalGridNew[grid.nW][i][j][nKInt-1])*0.25;
        dW_ijp1halfk_np1half=(grid.dLocalGridNew[grid.nW][i][j+1][nKInt]
          +grid.dLocalGridNew[grid.nW][i][j+1][nKInt-1]
          +grid.dLocalGridNew[grid.nW][i][j][nKInt]
          +grid.dLocalGridNew[grid.nW][i][j][nKInt-1])*0.25;
        dW_ijm1halfk_np1half=(grid.dLocalGridNew[grid.nW][i][j-1][nKInt]
          +grid.dLocalGridNew[grid.nW][i][j-1][nKInt-1]
          +grid.dLocalGridNew[grid.nW][i][j][nKInt]
          +grid.dLocalGridNew[grid.nW][i][j][nKInt-1])*0.25;
        dW_ijkp1half_np1half=grid.dLocalGridNew[grid.nW][i][j][nKInt];
        dW_ijkm1half_np1half=grid.dLocalGridNew[grid.nW][i][j][nKInt-1];
        
        //term 1
        d1=((dU_ip1halfjk_np1half-grid.dLocalGridNew[grid.nU0][nIInt][0][0])-(dU_im1halfjk_np1half
          -grid.dLocalGridNew[grid.nU0][nIInt-1][0][0]))/(dR_ip1half_np1half-dR_im1half_np1half);
        
        //term 2
        d2=1.0/dR_i_np1half*(dU_ijp1halfk_np1half-dU_ijm1halfk_np1half)
          /grid.dLocalGridOld[grid.nDTheta][0][j][0];
        
        //term 3
        d3=dV_ijk_np1half/dR_i_np1half;
        
        //term 4
        d4=(dU_ijkp1half_np1half-dU_ijkm1half_np1half)/(dR_i_np1half
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]*grid.dLocalGridOld[grid.nDPhi][0][0][k]);
        
        //term 5
        d5=(dW_ip1halfjk_np1half-dW_im1halfjk_np1half)/(dR_ip1half_np1half-dR_im1half_np1half);
        
        //term 6
        d6=dW_ijk_np1half/dR_i_np1half;
        
        //term 7
        d7=(dV_ip1halfjk_np1half-dV_im1halfjk_np1half)/dDelR_i_np1half;
        
        //term 8
        d8=1.0/dR_i_np1half*(dV_ijp1halfk_np1half-dV_ijm1halfk_np1half)
          /grid.dLocalGridOld[grid.nDTheta][0][j][0];
        
        //term 9
        d9=(dU_ijk_np1half-dU0_i_np1half)/dR_i_np1half;
        
        //term 10
        d10=(dW_ijp1halfk_np1half-dW_ijm1halfk_np1half)/(dR_i_np1half
          *grid.dLocalGridOld[grid.nDTheta][0][j][0]);
        
        //term 11
        d11=(dV_ijkp1half_np1half-dV_ijkm1half_np1half)/(dR_i_np1half
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]*grid.dLocalGridOld[grid.nDPhi][0][0][k]);
        
        //term 12
        d12=dW_ijk_np1half*grid.dLocalGridOld[grid.nCotThetaIJK][0][j][0]/dR_i_np1half;
        
        //term 13
        d13=(dW_ijkp1half_np1half-dW_ijkm1half_np1half)/(dR_i_np1half
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]*grid.dLocalGridOld[grid.nDPhi][0][0][k]);
        
        //term 14
        d14=dV_ijk_np1half*grid.dLocalGridOld[grid.nCotThetaIJK][0][j][0]/dR_i_np1half;
        
        //term A
        dA=2.0*d1*d1;
        
        //term B
        dB=(d2+d1-d3)*(d2-d3);
        
        //term C
        dC=(d4+d5-d6)*(d4-d6);
        
        //term D
        dD=d7*(d2+d7-d3);
        
        //term E
        dE=d8+d9;
        dE=2.0*dE*dE;
        
        //term dF
        dF=(d10+d11-d12)*(d11-d12);
        
        //term dG
        dG=d5*(d4+d5-d6);
        
        //term dH
        dH=d10*(d10+d11-d12);
        
        //term dI
        dI=d13+d14+d9;
        dI=2.0*dI*dI;
        
        dTerms=dA+dB+dC+dD+dE+dF+dG+dH+dI;
        grid.dLocalGridNew[grid.nEddyVisc][i][j][k]=dConstant*dLengthScaleSq
          *grid.dLocalGridNew[grid.nD][i][j][k]*pow(dTerms,0.5);
      }
    }
  }
  
  //outter radial ghost cells,explicit
  for(i=grid.nStartGhostUpdateExplicit[grid.nEddyVisc][0][0];
    i<grid.nEndGhostUpdateExplicit[grid.nEddyVisc][0][0];i++){
    
    //calculate i for interface centered quantities
    nIInt=i+grid.nCenIntOffset[0];
    dR_ip1half_np1half=grid.dLocalGridNew[grid.nR][nIInt][0][0];
    dR_im1half_np1half=grid.dLocalGridNew[grid.nR][nIInt-1][0][0];
    dR_i_np1half=(dR_ip1half_np1half+dR_im1half_np1half)*0.5;
    dR_i_np1half_Sq=dR_i_np1half*dR_i_np1half;
    dDelR_i_np1half=dR_ip1half_np1half-dR_im1half_np1half;
    dU0_i_np1half=(grid.dLocalGridNew[grid.nU0][nIInt][0][0]
      +grid.dLocalGridNew[grid.nU0][nIInt-1][0][0])*0.5;
    
    for(j=grid.nStartGhostUpdateExplicit[grid.nEddyVisc][0][1];
      j<grid.nEndGhostUpdateExplicit[grid.nEddyVisc][0][1];j++){
      
      nJInt=j+grid.nCenIntOffset[1];
      
      for(k=grid.nStartGhostUpdateExplicit[grid.nEddyVisc][0][2];
        k<grid.nEndGhostUpdateExplicit[grid.nEddyVisc][0][2];k++){
        
        nKInt=k+grid.nCenIntOffset[2];
        dLengthScaleSq=dR_i_np1half_Sq*dDelR_i_np1half*grid.dLocalGridOld[grid.nDTheta][0][j][0]
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]*grid.dLocalGridOld[grid.nDPhi][0][0][k];
        dLengthScaleSq=pow(dLengthScaleSq,0.666666666666666);
        
        //interpolate
        dU_ip1halfjk_np1half=grid.dLocalGridNew[grid.nU][nIInt][j][k];
        dU_im1halfjk_np1half=grid.dLocalGridNew[grid.nU][nIInt-1][j][k];
        dU_ijk_np1half=(grid.dLocalGridNew[grid.nU][nIInt][j][k]
          +grid.dLocalGridNew[grid.nU][nIInt-1][j][k])*0.5;
        dU_ijkp1half_np1half=(grid.dLocalGridNew[grid.nU][nIInt][j][k+1]
          +grid.dLocalGridNew[grid.nU][nIInt][j][k]+grid.dLocalGridNew[grid.nU][nIInt-1][j][k+1]
          +grid.dLocalGridNew[grid.nU][nIInt-1][j][k])*0.25;
        dU_ijkm1half_np1half=(grid.dLocalGridNew[grid.nU][nIInt][j][k]
          +grid.dLocalGridNew[grid.nU][nIInt][j][k-1]+grid.dLocalGridNew[grid.nU][nIInt-1][j][k]
          +grid.dLocalGridNew[grid.nU][nIInt-1][j][k-1])*0.25;
        dU_ijp1halfk_np1half=(grid.dLocalGridNew[grid.nU][nIInt][j][k]
          +grid.dLocalGridNew[grid.nU][nIInt-1][j][k]+grid.dLocalGridNew[grid.nU][nIInt][j+1][k]
          +grid.dLocalGridNew[grid.nU][nIInt-1][j+1][k])*0.25;
        dU_ijm1halfk_np1half=(grid.dLocalGridNew[grid.nU][nIInt][j][k]
          +grid.dLocalGridNew[grid.nU][nIInt-1][j][k]+grid.dLocalGridNew[grid.nU][nIInt][j-1][k]
          +grid.dLocalGridNew[grid.nU][nIInt-1][j-1][k])*0.25;
        dV_ijk_np1half=(grid.dLocalGridNew[grid.nV][i][nJInt][k]
          +grid.dLocalGridNew[grid.nV][i][nJInt-1][k])*0.5;
        dV_ip1halfjk_np1half=(grid.dLocalGridNew[grid.nV][i][nJInt][k]
          +grid.dLocalGridNew[grid.nV][i][nJInt-1][k])*0.5;/**\BC assuming that theta velocity
          is constant across surface*/
        dV_im1halfjk_np1half=(grid.dLocalGridNew[grid.nV][i][nJInt][k]
          +grid.dLocalGridNew[grid.nV][i][nJInt-1][k]+grid.dLocalGridNew[grid.nV][i-1][nJInt][k]
          +grid.dLocalGridNew[grid.nV][i-1][nJInt-1][k])*0.25;
        dV_ijp1halfk_np1half=grid.dLocalGridNew[grid.nV][i][nJInt][k];
        dV_ijm1halfk_np1half=grid.dLocalGridNew[grid.nV][i][nJInt-1][k];
        dV_ijkp1half_np1half=(grid.dLocalGridNew[grid.nV][i][nJInt][k+1]
          +grid.dLocalGridNew[grid.nV][i][nJInt][k]
          +grid.dLocalGridNew[grid.nV][i][nJInt-1][k+1]
          +grid.dLocalGridNew[grid.nV][i][nJInt-1][k])*0.25;
        dV_ijkm1half_np1half=(grid.dLocalGridNew[grid.nV][i][nJInt][k]
          +grid.dLocalGridNew[grid.nV][i][nJInt][k-1]
          +grid.dLocalGridNew[grid.nV][i][nJInt-1][k]
          +grid.dLocalGridNew[grid.nV][i][nJInt-1][k-1])*0.25;
        dW_ijk_np1half=(grid.dLocalGridNew[grid.nW][i][j][nKInt]
          +grid.dLocalGridNew[grid.nW][i][j][nKInt-1])*0.5;
        dW_ip1halfjk_np1half=(grid.dLocalGridNew[grid.nW][i][j][nKInt]
          +grid.dLocalGridNew[grid.nW][i][j][nKInt-1])*0.5;/**\BC assume phi velocity is constant
          across surface*/
        dW_im1halfjk_np1half=(grid.dLocalGridNew[grid.nW][i-1][j][nKInt]
          +grid.dLocalGridNew[grid.nW][i-1][j][nKInt-1]
          +grid.dLocalGridNew[grid.nW][i][j][nKInt]
          +grid.dLocalGridNew[grid.nW][i][j][nKInt-1])*0.25;
        dW_ijp1halfk_np1half=(grid.dLocalGridNew[grid.nW][i][j+1][nKInt]
          +grid.dLocalGridNew[grid.nW][i][j+1][nKInt-1]
          +grid.dLocalGridNew[grid.nW][i][j][nKInt]
          +grid.dLocalGridNew[grid.nW][i][j][nKInt-1])*0.25;
        dW_ijm1halfk_np1half=(grid.dLocalGridNew[grid.nW][i][j-1][nKInt]
          +grid.dLocalGridNew[grid.nW][i][j-1][nKInt-1]
          +grid.dLocalGridNew[grid.nW][i][j][nKInt]
          +grid.dLocalGridNew[grid.nW][i][j][nKInt-1])*0.25;
        dW_ijkp1half_np1half=grid.dLocalGridNew[grid.nW][i][j][nKInt];
        dW_ijkm1half_np1half=grid.dLocalGridNew[grid.nW][i][j][nKInt-1];
        
        //term 1
        d1=((dU_ip1halfjk_np1half-grid.dLocalGridNew[grid.nU0][nIInt][0][0])-(dU_im1halfjk_np1half
          -grid.dLocalGridNew[grid.nU0][nIInt-1][0][0]))/(dR_ip1half_np1half-dR_im1half_np1half);
        
        //term 2
        d2=1.0/dR_i_np1half*(dU_ijp1halfk_np1half-dU_ijm1halfk_np1half)
          /grid.dLocalGridOld[grid.nDTheta][0][j][0];
        
        //term 3
        d3=dV_ijk_np1half/dR_i_np1half;
        
        //term 4
        d4=(dU_ijkp1half_np1half-dU_ijkm1half_np1half)/(dR_i_np1half
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]*grid.dLocalGridOld[grid.nDPhi][0][0][k]);
        
        //term 5
        d5=(dW_ip1halfjk_np1half-dW_im1halfjk_np1half)/(dR_ip1half_np1half-dR_im1half_np1half);
        
        //term 6
        d6=dW_ijk_np1half/dR_i_np1half;
        
        //term 7
        d7=(dV_ip1halfjk_np1half-dV_im1halfjk_np1half)/dDelR_i_np1half;
        
        //term 8
        d8=1.0/dR_i_np1half*(dV_ijp1halfk_np1half-dV_ijm1halfk_np1half)
          /grid.dLocalGridOld[grid.nDTheta][0][j][0];
        
        //term 9
        d9=(dU_ijk_np1half-dU0_i_np1half)/dR_i_np1half;
        
        //term 10
        d10=(dW_ijp1halfk_np1half-dW_ijm1halfk_np1half)/(dR_i_np1half
          *grid.dLocalGridOld[grid.nDTheta][0][j][0]);
        
        //term 11
        d11=(dV_ijkp1half_np1half-dV_ijkm1half_np1half)/(dR_i_np1half
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]*grid.dLocalGridOld[grid.nDPhi][0][0][k]);
        
        //term 12
        d12=dW_ijk_np1half*grid.dLocalGridOld[grid.nCotThetaIJK][0][j][0]/dR_i_np1half;
        
        //term 13
        d13=(dW_ijkp1half_np1half-dW_ijkm1half_np1half)/(dR_i_np1half
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]*grid.dLocalGridOld[grid.nDPhi][0][0][k]);
        
        //term 14
        d14=dV_ijk_np1half*grid.dLocalGridOld[grid.nCotThetaIJK][0][j][0]/dR_i_np1half;
        
        //term A
        dA=2.0*d1*d1;
        
        //term B
        dB=(d2+d1-d3)*(d2-d3);
        
        //term C
        dC=(d4+d5-d6)*(d4-d6);
        
        //term D
        dD=d7*(d2+d7-d3);
        
        //term E
        dE=d8+d9;
        dE=2.0*dE*dE;
        
        //term dF
        dF=(d10+d11-d12)*(d11-d12);
        
        //term dG
        dG=d5*(d4+d5-d6);
        
        //term dH
        dH=d10*(d10+d11-d12);
        
        //term dI
        dI=d13+d14+d9;
        dI=2.0*dI*dI;
        
        dTerms=dA+dB+dC+dD+dE+dF+dG+dH+dI;
        grid.dLocalGridNew[grid.nEddyVisc][i][j][k]=dConstant*dLengthScaleSq
          *grid.dLocalGridNew[grid.nD][i][j][k]*pow(dTerms,0.5);
      }
    }
  }
}
void calOldDenave_None(Grid &grid){
}
void calOldDenave_R(Grid &grid){
  
  //explicit region
  for(int i=grid.nStartUpdateExplicit[grid.nDenAve][0];
    i<grid.nEndUpdateExplicit[grid.nDenAve][0];i++){
    grid.dLocalGridOld[grid.nDenAve][i][0][0]=grid.dLocalGridOld[grid.nD][i][0][0];
  }
  for(int i=grid.nStartGhostUpdateExplicit[grid.nDenAve][0][0];
    i<grid.nEndGhostUpdateExplicit[grid.nDenAve][0][0];i++){
    grid.dLocalGridOld[grid.nDenAve][i][0][0]=grid.dLocalGridOld[grid.nD][i][0][0];
  }
  
  //implicit region
  for(int i=grid.nStartUpdateImplicit[grid.nDenAve][0];i<grid.nEndUpdateImplicit[grid.nDenAve][0];
    i++){
    grid.dLocalGridOld[grid.nDenAve][i][0][0]=grid.dLocalGridOld[grid.nD][i][0][0];
  }
  for(int i=grid.nStartGhostUpdateImplicit[grid.nDenAve][0][0];
    i<grid.nEndGhostUpdateImplicit[grid.nDenAve][0][0];i++){
    grid.dLocalGridOld[grid.nDenAve][i][0][0]=grid.dLocalGridOld[grid.nD][i][0][0];
  }
}
void calOldDenave_RT(Grid &grid){
  
  //EXPLICIT REGION
  //most ghost cell, since we don't have R at outer interface. This should be ok in most cases
  for(int i=grid.nStartUpdateExplicit[grid.nDenAve][0];i<grid.nEndUpdateExplicit[grid.nDenAve][0];i++){
    
    //calculate i for interface centered quantities
    int nIInt=i+grid.nCenIntOffset[0];
    
    double dSum=0.0;
    double dVolume=0.0;
    double dRFactor=0.33333333333333333*(pow(grid.dLocalGridOld[grid.nR][nIInt][0][0],3.0)
      -pow(grid.dLocalGridOld[grid.nR][nIInt-1][0][0],3.0));
    for(int j=grid.nStartUpdateExplicit[grid.nD][1];j<grid.nEndUpdateExplicit[grid.nD][1];j++){
      for(int k=grid.nStartUpdateExplicit[grid.nD][2];k<grid.nEndUpdateExplicit[grid.nD][2];k++){
        double dVolumeTemp=dRFactor*grid.dLocalGridOld[grid.nDCosThetaIJK][0][j][0];
        dSum+=dVolumeTemp*grid.dLocalGridOld[grid.nD][i][j][k];
        dVolume+=dVolumeTemp;
      }
    }
    grid.dLocalGridOld[grid.nDenAve][i][0][0]=dSum/dVolume;
  }
  //ghost region 0, outter most ghost region in x1 direction
  for(int i=grid.nStartGhostUpdateExplicit[grid.nDenAve][0][0];
    i<grid.nEndGhostUpdateExplicit[grid.nDenAve][0][0];i++){
    double dSum=0.0;
    double dVolume=0.0;
    double dRFactor=0.33333333333333333*(pow(grid.dLocalGridOld[grid.nR][i][0][0],3.0)
      -pow(grid.dLocalGridOld[grid.nR-1][i][0][0],3.0));
    for(int j=grid.nStartUpdateExplicit[grid.nD][1];j<grid.nEndUpdateExplicit[grid.nD][1];j++){
      for(int k=grid.nStartUpdateExplicit[grid.nD][2];k<grid.nEndUpdateExplicit[grid.nD][2];k++){
        double dVolumeTemp=dRFactor*grid.dLocalGridOld[grid.nDCosThetaIJK][0][j][0];
        dSum+=dVolumeTemp*grid.dLocalGridOld[grid.nD][i][j][k];
        dVolume+=dVolumeTemp;
      }
    }
    grid.dLocalGridOld[grid.nDenAve][i][0][0]=dSum/dVolume;
  }
  
  //IMPLICIT REGION
  //most ghost cell, since we don't have R at outer interface. This should be ok in most cases
  for(int i=grid.nStartUpdateImplicit[grid.nDenAve][0];
    i<grid.nEndUpdateImplicit[grid.nDenAve][0];i++){
    
    //calculate i for interface centered quantities
    int nIInt=i+grid.nCenIntOffset[0];
    
    double dSum=0.0;
    double dVolume=0.0;
    double dRFactor=0.33333333333333333*(pow(grid.dLocalGridOld[grid.nR][nIInt][0][0],3.0)
      -pow(grid.dLocalGridOld[grid.nR][nIInt-1][0][0],3.0));
    for(int j=grid.nStartUpdateImplicit[grid.nD][1];j<grid.nEndUpdateImplicit[grid.nD][1];j++){
      for(int k=grid.nStartUpdateImplicit[grid.nD][2];k<grid.nEndUpdateImplicit[grid.nD][2];k++){
        double dVolumeTemp=dRFactor*grid.dLocalGridOld[grid.nDCosThetaIJK][0][j][0];
        dSum+=dVolumeTemp*grid.dLocalGridOld[grid.nD][i][j][k];
        dVolume+=dVolumeTemp;
      }
    }
    grid.dLocalGridOld[grid.nDenAve][i][0][0]=dSum/dVolume;
  }
  //ghost region 0, outter most ghost region in x1 direction
  for(int i=grid.nStartGhostUpdateImplicit[grid.nDenAve][0][0];
    i<grid.nEndGhostUpdateImplicit[grid.nDenAve][0][0];i++){
    double dSum=0.0;
    double dVolume=0.0;
    double dRFactor=0.33333333333333333*(pow(grid.dLocalGridOld[grid.nR][i][0][0],3.0)
      -pow(grid.dLocalGridOld[grid.nR-1][i][0][0],3.0));
    for(int j=grid.nStartUpdateImplicit[grid.nD][1];j<grid.nEndUpdateImplicit[grid.nD][1];j++){
      for(int k=grid.nStartUpdateImplicit[grid.nD][2];k<grid.nEndUpdateImplicit[grid.nD][2];k++){
        double dVolumeTemp=dRFactor*grid.dLocalGridOld[grid.nDCosThetaIJK][0][j][0];
        dSum+=dVolumeTemp*grid.dLocalGridOld[grid.nD][i][j][k];
        dVolume+=dVolumeTemp;
      }
    }
    grid.dLocalGridOld[grid.nDenAve][i][0][0]=dSum/dVolume;
  }
}
void calOldDenave_RTP(Grid &grid){
  
  //EXPLICIT REGION
  //most ghost cell, since we don't have R at outer interface. This should be ok in most cases
  for(int i=grid.nStartUpdateExplicit[grid.nDenAve][0];i<grid.nEndUpdateExplicit[grid.nDenAve][0];
    i++){
    
    //calculate i for interface centered quantities
    int nIInt=i+grid.nCenIntOffset[0];
    
    double dSum=0.0;
    double dVolume=0.0;
    double dRFactor=0.33333333333333333*(pow(grid.dLocalGridOld[grid.nR][nIInt][0][0],3.0)
      -pow(grid.dLocalGridOld[grid.nR][nIInt-1][0][0],3.0));
    for(int j=grid.nStartUpdateExplicit[grid.nD][1];j<grid.nEndUpdateExplicit[grid.nD][1];j++){
      for(int k=grid.nStartUpdateExplicit[grid.nD][2];k<grid.nEndUpdateExplicit[grid.nD][2];k++){
        double dVolumeTemp=dRFactor*grid.dLocalGridOld[grid.nDCosThetaIJK][0][j][0]
          *grid.dLocalGridOld[grid.nDPhi][0][0][k];
        dSum+=dVolumeTemp*grid.dLocalGridOld[grid.nD][i][j][k];
        dVolume+=dVolumeTemp;
      }
    }
    grid.dLocalGridOld[grid.nDenAve][i][0][0]=dSum/dVolume;
  }
  //ghost region 0, outter most ghost region in x1 direction
  for(int i=grid.nStartGhostUpdateExplicit[grid.nDenAve][0][0];
    i<grid.nEndGhostUpdateExplicit[grid.nDenAve][0][0];i++){
    double dSum=0.0;
    double dVolume=0.0;
    double dRFactor=0.33333333333333333*(pow(grid.dLocalGridOld[grid.nR][i][0][0],3.0)
      -pow(grid.dLocalGridOld[grid.nR-1][i][0][0],3.0));
    for(int j=grid.nStartUpdateExplicit[grid.nD][1];j<grid.nEndUpdateExplicit[grid.nD][1];j++){
      for(int k=grid.nStartUpdateExplicit[grid.nD][2];k<grid.nEndUpdateExplicit[grid.nD][2];k++){
        double dVolumeTemp=dRFactor*grid.dLocalGridOld[grid.nDCosThetaIJK][0][j][0]
          *grid.dLocalGridOld[grid.nDPhi][0][0][k];
        dSum+=dVolumeTemp*grid.dLocalGridOld[grid.nD][i][j][k];
        dVolume+=dVolumeTemp;
      }
    }
    grid.dLocalGridOld[grid.nDenAve][i][0][0]=dSum/dVolume;
  }
  
  //IMPLICT REGION
  //most ghost cell, since we don't have R at outer interface. This should be ok in most cases
  for(int i=grid.nStartUpdateImplicit[grid.nDenAve][0];i<grid.nEndUpdateImplicit[grid.nDenAve][0];
    i++){
    
    //calculate i for interface centered quantities
    int nIInt=i+grid.nCenIntOffset[0];
    
    double dSum=0.0;
    double dVolume=0.0;
    double dRFactor=0.33333333333333333*(pow(grid.dLocalGridOld[grid.nR][nIInt][0][0],3.0)
      -pow(grid.dLocalGridOld[grid.nR][nIInt-1][0][0],3.0));
    for(int j=grid.nStartUpdateImplicit[grid.nD][1];j<grid.nEndUpdateImplicit[grid.nD][1];j++){
      for(int k=grid.nStartUpdateImplicit[grid.nD][2];k<grid.nEndUpdateImplicit[grid.nD][2];k++){
        double dVolumeTemp=dRFactor*grid.dLocalGridOld[grid.nDCosThetaIJK][0][j][0]
          *grid.dLocalGridOld[grid.nDPhi][0][0][k];
        dSum+=dVolumeTemp*grid.dLocalGridOld[grid.nD][i][j][k];
        dVolume+=dVolumeTemp;
      }
    }
    grid.dLocalGridOld[grid.nDenAve][i][0][0]=dSum/dVolume;
  }
  //ghost region 0, outter most ghost region in x1 direction
  for(int i=grid.nStartGhostUpdateImplicit[grid.nDenAve][0][0];
    i<grid.nEndGhostUpdateImplicit[grid.nDenAve][0][0];i++){
    double dSum=0.0;
    double dVolume=0.0;
    double dRFactor=0.33333333333333333*(pow(grid.dLocalGridOld[grid.nR][i][0][0],3.0)
      -pow(grid.dLocalGridOld[grid.nR-1][i][0][0],3.0));
    for(int j=grid.nStartUpdateExplicit[grid.nD][1];j<grid.nEndUpdateExplicit[grid.nD][1];j++){
      for(int k=grid.nStartUpdateExplicit[grid.nD][2];k<grid.nEndUpdateExplicit[grid.nD][2];k++){
        double dVolumeTemp=dRFactor*grid.dLocalGridOld[grid.nDCosThetaIJK][0][j][0]
          *grid.dLocalGridOld[grid.nDPhi][0][0][k];
        dSum+=dVolumeTemp*grid.dLocalGridOld[grid.nD][i][j][k];
        dVolume+=dVolumeTemp;
      }
    }
    grid.dLocalGridOld[grid.nDenAve][i][0][0]=dSum/dVolume;
  }

}
void calOldP_GL(Grid& grid,Parameters &parameters){
  for(int i=grid.nStartUpdateExplicit[grid.nP][0];i<grid.nEndUpdateExplicit[grid.nP][0];i++){
    for(int j=grid.nStartUpdateExplicit[grid.nP][1];j<grid.nEndUpdateExplicit[grid.nP][1];j++){
      for(int k=grid.nStartUpdateExplicit[grid.nP][2];k<grid.nEndUpdateExplicit[grid.nP][2];k++){
        grid.dLocalGridOld[grid.nP][i][j][k]=dEOS_GL(grid.dLocalGridOld[grid.nD][i][j][k]
          ,grid.dLocalGridOld[grid.nE][i][j][k],parameters);
      }
    }
  }
  for(int i=grid.nStartGhostUpdateExplicit[grid.nP][0][0];i<grid.nEndGhostUpdateExplicit[grid.nP][0][0];i++){
    for(int j=grid.nStartGhostUpdateExplicit[grid.nP][0][1];j<grid.nEndGhostUpdateExplicit[grid.nP][0][1];j++){
      for(int k=grid.nStartGhostUpdateExplicit[grid.nP][0][2];k<grid.nEndGhostUpdateExplicit[grid.nP][0][2];k++){
        grid.dLocalGridOld[grid.nP][i][j][k]=dEOS_GL(grid.dLocalGridOld[grid.nD][i][j][k]
          ,grid.dLocalGridOld[grid.nE][i][j][k],parameters);
      }
    }
  }
  for(int i=grid.nStartGhostUpdateExplicit[grid.nP][1][0];i<grid.nEndGhostUpdateExplicit[grid.nP][1][0];i++){
    for(int j=grid.nStartGhostUpdateExplicit[grid.nP][1][1];j<grid.nEndGhostUpdateExplicit[grid.nP][1][1];j++){
      for(int k=grid.nStartGhostUpdateExplicit[grid.nP][1][2];k<grid.nEndGhostUpdateExplicit[grid.nP][1][2];k++){
        grid.dLocalGridOld[grid.nP][i][j][k]=dEOS_GL(grid.dLocalGridOld[grid.nD][i][j][k]
          ,grid.dLocalGridOld[grid.nE][i][j][k],parameters);
      }
    }
  }
  
  //NO IMPLICIT REGION FOR GAMMA LAWA GAS
}
void calOldPEKappaGamma_TEOS(Grid& grid,Parameters &parameters){
  
  //main grid
  for(int i=grid.nStartUpdateExplicit[grid.nP][0];i<grid.nEndUpdateExplicit[grid.nP][0];i++){
    for(int j=grid.nStartUpdateExplicit[grid.nP][1];j<grid.nEndUpdateExplicit[grid.nP][1];j++){
      for(int k=grid.nStartUpdateExplicit[grid.nP][2];k<grid.nEndUpdateExplicit[grid.nP][2];k++){
        parameters.eosTable.getPEKappaGamma(grid.dLocalGridOld[grid.nT][i][j][k]
          ,grid.dLocalGridOld[grid.nD][i][j][k],grid.dLocalGridOld[grid.nP][i][j][k]
          ,grid.dLocalGridOld[grid.nE][i][j][k],grid.dLocalGridOld[grid.nKappa][i][j][k]
          ,grid.dLocalGridOld[grid.nGamma][i][j][k]);
      }
    }
  }
  
  //inner radial ghost cells
  for(int i=0;i<grid.nNumGhostCells;i++){
    for(int j=grid.nStartUpdateExplicit[grid.nP][1];j<grid.nEndUpdateExplicit[grid.nP][1];j++){
      for(int k=grid.nStartUpdateExplicit[grid.nP][2];k<grid.nEndUpdateExplicit[grid.nP][2];k++){
        parameters.eosTable.getPEKappaGamma(grid.dLocalGridOld[grid.nT][i][j][k]
          ,grid.dLocalGridOld[grid.nD][i][j][k],grid.dLocalGridOld[grid.nP][i][j][k]
          ,grid.dLocalGridOld[grid.nE][i][j][k],grid.dLocalGridOld[grid.nKappa][i][j][k]
          ,grid.dLocalGridOld[grid.nGamma][i][j][k]);
      }
    }
  }
  
  //outter radial ghost cells
  for(int i=grid.nStartGhostUpdateExplicit[grid.nP][0][0];i<grid.nEndGhostUpdateExplicit[grid.nP][0][0];i++){
    for(int j=grid.nStartGhostUpdateExplicit[grid.nP][0][1];j<grid.nEndGhostUpdateExplicit[grid.nP][0][1];j++){
      for(int k=grid.nStartGhostUpdateExplicit[grid.nP][0][2];k<grid.nEndGhostUpdateExplicit[grid.nP][0][2];k++){
        parameters.eosTable.getPEKappaGamma(grid.dLocalGridOld[grid.nT][i][j][k]
          ,grid.dLocalGridOld[grid.nD][i][j][k],grid.dLocalGridOld[grid.nP][i][j][k]
          ,grid.dLocalGridOld[grid.nE][i][j][k],grid.dLocalGridOld[grid.nKappa][i][j][k]
          ,grid.dLocalGridOld[grid.nGamma][i][j][k]);
      }
    }
  }
  
  //main grid
  for(int i=grid.nStartUpdateImplicit[grid.nP][0];i<grid.nEndUpdateImplicit[grid.nP][0];i++){
    for(int j=grid.nStartUpdateImplicit[grid.nP][1];j<grid.nEndUpdateImplicit[grid.nP][1];j++){
      for(int k=grid.nStartUpdateImplicit[grid.nP][2];k<grid.nEndUpdateImplicit[grid.nP][2];k++){
        parameters.eosTable.getPEKappaGamma(grid.dLocalGridOld[grid.nT][i][j][k]
          ,grid.dLocalGridOld[grid.nD][i][j][k],grid.dLocalGridOld[grid.nP][i][j][k]
          ,grid.dLocalGridOld[grid.nE][i][j][k],grid.dLocalGridOld[grid.nKappa][i][j][k]
          ,grid.dLocalGridOld[grid.nGamma][i][j][k]);
      }
    }
  }
  
  //inner radial ghost cells
  for(int i=0;i<grid.nNumGhostCells;i++){
    for(int j=grid.nStartUpdateImplicit[grid.nP][1];j<grid.nEndUpdateImplicit[grid.nP][1];j++){
      for(int k=grid.nStartUpdateImplicit[grid.nP][2];k<grid.nEndUpdateImplicit[grid.nP][2];k++){
        parameters.eosTable.getPEKappaGamma(grid.dLocalGridOld[grid.nT][i][j][k]
          ,grid.dLocalGridOld[grid.nD][i][j][k],grid.dLocalGridOld[grid.nP][i][j][k]
          ,grid.dLocalGridOld[grid.nE][i][j][k],grid.dLocalGridOld[grid.nKappa][i][j][k]
          ,grid.dLocalGridOld[grid.nGamma][i][j][k]);
      }
    }
  }
  
  //outter radial ghost cells
  for(int i=grid.nStartGhostUpdateImplicit[grid.nP][0][0];i<grid.nEndGhostUpdateImplicit[grid.nP][0][0];i++){
    for(int j=grid.nStartGhostUpdateImplicit[grid.nP][0][1];j<grid.nEndGhostUpdateImplicit[grid.nP][0][1];j++){
      for(int k=grid.nStartGhostUpdateImplicit[grid.nP][0][2];k<grid.nEndGhostUpdateImplicit[grid.nP][0][2];k++){
        parameters.eosTable.getPEKappaGamma(grid.dLocalGridOld[grid.nT][i][j][k]
          ,grid.dLocalGridOld[grid.nD][i][j][k],grid.dLocalGridOld[grid.nP][i][j][k]
          ,grid.dLocalGridOld[grid.nE][i][j][k],grid.dLocalGridOld[grid.nKappa][i][j][k]
          ,grid.dLocalGridOld[grid.nGamma][i][j][k]);
      }
    }
  }
}
void calOldQ0_R_GL(Grid& grid, Parameters &parameters){
  
  double dA_sq=parameters.dA*parameters.dA;
  double dDVDt;
  double dA_ip1half;
  double dA_im1half;
  double dR_i;
  int nIInt;
  double dC;
  double dDVDtThreshold;
  double dR_i_sq;
  double dDVDt_mthreshold;
  int i;
  //in the main grid
  for(i=grid.nStartUpdateExplicit[grid.nQ0][0];i<grid.nEndUpdateExplicit[grid.nQ0][0];i++){
    
    //calculate i for interface centered quantities
    nIInt=i+grid.nCenIntOffset[0];
    
    //calculate volume change
    dA_ip1half=grid.dLocalGridOld[grid.nR][nIInt][0][0]*grid.dLocalGridOld[grid.nR][nIInt][0][0];
    dA_im1half=grid.dLocalGridOld[grid.nR][nIInt-1][0][0]
      *grid.dLocalGridOld[grid.nR][nIInt-1][0][0];
    dR_i=(grid.dLocalGridOld[grid.nR][nIInt][0][0]+grid.dLocalGridOld[grid.nR][nIInt-1][0][0])*0.5;
    dR_i_sq=dR_i*dR_i;
    dDVDt=(dA_ip1half*grid.dLocalGridOld[grid.nU][nIInt][0][0]
      -dA_im1half*grid.dLocalGridOld[grid.nU][nIInt-1][0][0])/dR_i_sq;
    
    //calculate threshold compression to turn viscosity on at
    dC=sqrt(parameters.dGamma*(grid.dLocalGridOld[grid.nP][i][0][0])
      /grid.dLocalGridOld[grid.nD][i][0][0]);
    dDVDtThreshold=parameters.dAVThreshold*dC;
    
    if(dDVDt<-1.0*dDVDtThreshold){//being compressed
      dDVDt_mthreshold=dDVDt+dDVDtThreshold;
      grid.dLocalGridOld[grid.nQ0][i][0][0]=dA_sq*grid.dLocalGridOld[grid.nD][i][0][0]
        *dDVDt_mthreshold*dDVDt_mthreshold;
    }
    else{
      grid.dLocalGridOld[grid.nQ0][i][0][0]=0.0;
    }
  }
  
  //outter ghost region
  for(i=grid.nStartGhostUpdateExplicit[grid.nQ0][0][0];
    i<grid.nEndGhostUpdateExplicit[grid.nQ0][0][0];i++){
    
    //calculate i for interface centered quantities
    nIInt=i+grid.nCenIntOffset[0];
    
    //calculate volume change
    dA_ip1half=grid.dLocalGridOld[grid.nR][nIInt][0][0]*grid.dLocalGridOld[grid.nR][nIInt][0][0];
    dA_im1half=grid.dLocalGridOld[grid.nR][nIInt-1][0][0]
      *grid.dLocalGridOld[grid.nR][nIInt-1][0][0];
    dR_i=(grid.dLocalGridOld[grid.nR][nIInt][0][0]+grid.dLocalGridOld[grid.nR][nIInt-1][0][0])*0.5;
    dR_i_sq=dR_i*dR_i;
    dDVDt=(dA_ip1half*grid.dLocalGridOld[grid.nU][nIInt][0][0]
      -dA_im1half*grid.dLocalGridOld[grid.nU][nIInt-1][0][0])/dR_i_sq;
    
    //calculate threshold compression to turn viscosity on at
    dC=sqrt(parameters.dGamma*(grid.dLocalGridOld[grid.nP][i][0][0])
      /grid.dLocalGridOld[grid.nD][i][0][0]);
    dDVDtThreshold=parameters.dAVThreshold*dC;
    
    if(dDVDt<-1.0*dDVDtThreshold){//being compressed
      dDVDt_mthreshold=dDVDt+dDVDtThreshold;
      grid.dLocalGridOld[grid.nQ0][i][0][0]=dA_sq*grid.dLocalGridOld[grid.nD][i][0][0]
        *dDVDt_mthreshold*dDVDt_mthreshold;
    }
    else{
      grid.dLocalGridOld[grid.nQ0][i][0][0]=0.0;
    }
  }
  
  //inner ghost region
  #if SEDOV==1
    for(int i=grid.nStartGhostUpdateExplicit[grid.nQ0][1][0];
      i<grid.nEndGhostUpdateExplicit[grid.nQ0][1][0];i++){
    
    //calculate i for interface centered quantities
    nIInt=i+grid.nCenIntOffset[0];
    
    //calculate volume change
    dA_ip1half=grid.dLocalGridOld[grid.nR][nIInt][0][0]*grid.dLocalGridOld[grid.nR][nIInt][0][0];
    dA_im1half=grid.dLocalGridOld[grid.nR][nIInt-1][0][0]
      *grid.dLocalGridOld[grid.nR][nIInt-1][0][0];
    dR_i=(grid.dLocalGridOld[grid.nR][nIInt][0][0]+grid.dLocalGridOld[grid.nR][nIInt-1][0][0])*0.5;
    dR_i_sq=dR_i*dR_i;
    dDVDt=(dA_ip1half*grid.dLocalGridOld[grid.nU][nIInt][0][0]
      -dA_im1half*grid.dLocalGridOld[grid.nU][nIInt-1][0][0])/dR_i_sq;
    
    //calculate threshold compression to turn viscosity on at
    dC=sqrt(parameters.dGamma*(grid.dLocalGridOld[grid.nP][i][0][0])
      /grid.dLocalGridOld[grid.nD][i][0][0]);
    dDVDtThreshold=parameters.dAVThreshold*dC;
    
    if(dDVDt<-1.0*dDVDtThreshold){//being compressed
      dDVDt_mthreshold=dDVDt+dDVDtThreshold;
      grid.dLocalGridOld[grid.nQ0][i][0][0]=dA_sq*grid.dLocalGridOld[grid.nD][i][0][0]
        *dDVDt_mthreshold*dDVDt_mthreshold;
    }
    else{
      grid.dLocalGridOld[grid.nQ0][i][0][0]=0.0;
    }
    }
  #endif
}
void calOldQ0_R_TEOS(Grid& grid, Parameters &parameters){
  
  double dA_sq=parameters.dA*parameters.dA;
  double dDVDt;
  double dA_ip1half;
  double dA_im1half;
  double dR_i;
  int nIInt;
  double dC;
  double dDVDtThreshold;
  double dR_i_sq;
  double dDVDt_mthreshold;
  int i;
  
  //in the main grid
  for(i=grid.nStartUpdateExplicit[grid.nQ0][0];i<grid.nEndUpdateExplicit[grid.nQ0][0];i++){
    
    //calculate i for interface centered quantities
    nIInt=i+grid.nCenIntOffset[0];
    
    //calculate volume change
    dA_ip1half=grid.dLocalGridOld[grid.nR][nIInt][0][0]*grid.dLocalGridOld[grid.nR][nIInt][0][0];
    dA_im1half=grid.dLocalGridOld[grid.nR][nIInt-1][0][0]
      *grid.dLocalGridOld[grid.nR][nIInt-1][0][0];
    dR_i=(grid.dLocalGridOld[grid.nR][nIInt][0][0]+grid.dLocalGridOld[grid.nR][nIInt-1][0][0])*0.5;
    dR_i_sq=dR_i*dR_i;
    dDVDt=(dA_ip1half*grid.dLocalGridOld[grid.nU][nIInt][0][0]
      -dA_im1half*grid.dLocalGridOld[grid.nU][nIInt-1][0][0])/dR_i_sq;
    
    //calculate threshold compression to turn viscosity on at
    /**\todo should use new P and rho*/
    dC=sqrt(grid.dLocalGridOld[grid.nGamma][i][0][0]*(grid.dLocalGridOld[grid.nP][i][0][0])
      /grid.dLocalGridOld[grid.nD][i][0][0]);
    dDVDtThreshold=parameters.dAVThreshold*dC;
    
    if(dDVDt<-1.0*dDVDtThreshold){//being compressed
      dDVDt_mthreshold=dDVDt+dDVDtThreshold;
      grid.dLocalGridOld[grid.nQ0][i][0][0]=dA_sq*grid.dLocalGridOld[grid.nD][i][0][0]
        *dDVDt_mthreshold*dDVDt_mthreshold;
    }
    else{
      grid.dLocalGridOld[grid.nQ0][i][0][0]=0.0;
    }
  }
  
  //outter ghost region
  for(i=grid.nStartGhostUpdateExplicit[grid.nQ0][0][0];
    i<grid.nEndGhostUpdateExplicit[grid.nQ0][0][0];i++){
    
    //calculate i for interface centered quantities
    nIInt=i+grid.nCenIntOffset[0];
    
    //calculate volume change
    dA_ip1half=grid.dLocalGridOld[grid.nR][nIInt][0][0]*grid.dLocalGridOld[grid.nR][nIInt][0][0];
    dA_im1half=grid.dLocalGridOld[grid.nR][nIInt-1][0][0]
      *grid.dLocalGridOld[grid.nR][nIInt-1][0][0];
    dR_i=(grid.dLocalGridOld[grid.nR][nIInt][0][0]+grid.dLocalGridOld[grid.nR][nIInt-1][0][0])*0.5;
    dR_i_sq=dR_i*dR_i;
    dDVDt=(dA_ip1half*grid.dLocalGridOld[grid.nU][nIInt][0][0]
      -dA_im1half*grid.dLocalGridOld[grid.nU][nIInt-1][0][0])/dR_i_sq;
    
    //calculate threshold compression to turn viscosity on at
    dC=sqrt(grid.dLocalGridOld[grid.nGamma][i][0][0]*(grid.dLocalGridOld[grid.nP][i][0][0])
      /grid.dLocalGridOld[grid.nD][i][0][0]);
    dDVDtThreshold=parameters.dAVThreshold*dC;
    
    if(dDVDt<-1.0*dDVDtThreshold){//being compressed
      dDVDt_mthreshold=dDVDt+dDVDtThreshold;
      grid.dLocalGridOld[grid.nQ0][i][0][0]=dA_sq*grid.dLocalGridOld[grid.nD][i][0][0]
        *dDVDt_mthreshold*dDVDt_mthreshold;
    }
    else{
      grid.dLocalGridOld[grid.nQ0][i][0][0]=0.0;
    }
  }
}
void calOldQ0Q1_RT_GL(Grid& grid, Parameters &parameters){
  
  double dA_sq=parameters.dA*parameters.dA;
  double dDVDt;
  double dA_ip1half;
  double dA_im1half;
  double dA_jp1half;
  double dA_jm1half;
  double dA_j;
  double dR_i;
  int nIInt;
  int nJInt;
  double dC;
  double dDVDtThreshold;
  double dR_i_sq;
  double dDVDt_mthreshold;
  int i;
  int j;
  
  //in the main grid
  for(i=grid.nStartUpdateExplicit[grid.nQ0][0];i<grid.nEndUpdateExplicit[grid.nQ0][0];i++){
    
    //calculate i for interface centered quantities
    nIInt=i+grid.nCenIntOffset[0];
    dR_i=(grid.dLocalGridOld[grid.nR][nIInt][0][0]+grid.dLocalGridOld[grid.nR][nIInt-1][0][0])*0.5;
    dR_i_sq=dR_i*dR_i;
    dA_ip1half=grid.dLocalGridOld[grid.nR][nIInt][0][0]*grid.dLocalGridOld[grid.nR][nIInt][0][0];
    dA_im1half=grid.dLocalGridOld[grid.nR][nIInt-1][0][0]
      *grid.dLocalGridOld[grid.nR][nIInt-1][0][0];
    
    for(j=grid.nStartUpdateExplicit[grid.nQ0][1];j<grid.nEndUpdateExplicit[grid.nQ0][1];j++){
      
      //calculate j for interface centered quantities
      nJInt=j+grid.nCenIntOffset[1];
      
      //calculate Q0
      //calculate volume change
      dDVDt=(dA_ip1half*grid.dLocalGridOld[grid.nU][nIInt][j][0]
        -dA_im1half*grid.dLocalGridOld[grid.nU][nIInt-1][j][0])/dR_i_sq;
      
      //calculate threshold compression to turn viscosity on at
      dC=sqrt(parameters.dGamma*(grid.dLocalGridOld[grid.nP][i][j][0])
        /grid.dLocalGridOld[grid.nD][i][j][0]);
      dDVDtThreshold=parameters.dAVThreshold*dC;
      
      if(dDVDt<-1.0*dDVDtThreshold){//being compressed
        dDVDt_mthreshold=dDVDt+dDVDtThreshold;
        grid.dLocalGridOld[grid.nQ0][i][j][0]=dA_sq*grid.dLocalGridOld[grid.nD][i][j][0]
          *dDVDt_mthreshold*dDVDt_mthreshold;
      }
      else{
        grid.dLocalGridOld[grid.nQ0][i][j][0]=0.0;
      }
      
      //calculate Q1
      //calculate volume change
      dA_jp1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt][0];
      dA_jm1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt-1][0];
      dA_j=grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0];
      
      dDVDt=(dA_jp1half*grid.dLocalGridOld[grid.nV][i][nJInt][0]
        -dA_jm1half*grid.dLocalGridOld[grid.nV][i][nJInt-1][0])/dA_j;
      
      if(dDVDt<-1.0*dDVDtThreshold){//being compressed
        dDVDt_mthreshold=dDVDt+dDVDtThreshold;
        grid.dLocalGridOld[grid.nQ1][i][j][0]=dA_sq*grid.dLocalGridOld[grid.nD][i][j][0]
          *dDVDt_mthreshold*dDVDt_mthreshold;
      }
      else{
        grid.dLocalGridOld[grid.nQ1][i][j][0]=0.0;
      }
    }
  }
  
  //outter ghost region
  for(i=grid.nStartGhostUpdateExplicit[grid.nQ0][0][0];
    i<grid.nEndGhostUpdateExplicit[grid.nQ0][0][0];i++){
    
    //calculate i for interface centered quantities
    nIInt=i+grid.nCenIntOffset[0];
    dR_i=(grid.dLocalGridOld[grid.nR][nIInt][0][0]+grid.dLocalGridOld[grid.nR][nIInt-1][0][0])*0.5;
    dR_i_sq=dR_i*dR_i;
    dA_ip1half=grid.dLocalGridOld[grid.nR][nIInt][0][0]*grid.dLocalGridOld[grid.nR][nIInt][0][0];
    dA_im1half=grid.dLocalGridOld[grid.nR][nIInt-1][0][0]
      *grid.dLocalGridOld[grid.nR][nIInt-1][0][0];
    
    for(j=grid.nStartGhostUpdateExplicit[grid.nQ0][0][1];
      j<grid.nEndGhostUpdateExplicit[grid.nQ0][0][1];j++){
      
      //calculate j for interface centered quantities
      nJInt=j+grid.nCenIntOffset[1];
      
      //calculate Q0
      //calculate volume change
      dDVDt=(dA_ip1half*grid.dLocalGridOld[grid.nU][nIInt][j][0]
        -dA_im1half*grid.dLocalGridOld[grid.nU][nIInt-1][j][0])/dR_i_sq;
      
      //calculate threshold compression to turn viscosity on at
      dC=sqrt(parameters.dGamma*(grid.dLocalGridOld[grid.nP][i][j][0])
        /grid.dLocalGridOld[grid.nD][i][j][0]);
      dDVDtThreshold=parameters.dAVThreshold*dC;
      
      if(dDVDt<-1.0*dDVDtThreshold){//being compressed
        dDVDt_mthreshold=dDVDt+dDVDtThreshold;
        grid.dLocalGridOld[grid.nQ0][i][j][0]=dA_sq*grid.dLocalGridOld[grid.nD][i][j][0]
          *dDVDt_mthreshold*dDVDt_mthreshold;
      }
      else{
        grid.dLocalGridOld[grid.nQ0][i][j][0]=0.0;
      }
      
      //calculate Q1
      //calculate volume change
      dA_jp1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt][0];
      dA_jm1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt-1][0];
      dA_j=grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0];
      
      dDVDt=(dA_jp1half*grid.dLocalGridOld[grid.nV][i][nJInt][0]
        -dA_jm1half*grid.dLocalGridOld[grid.nV][i][nJInt-1][0])/dA_j;
      
      if(dDVDt<-1.0*dDVDtThreshold){//being compressed
        dDVDt_mthreshold=dDVDt+dDVDtThreshold;
        grid.dLocalGridOld[grid.nQ1][i][j][0]=dA_sq*grid.dLocalGridOld[grid.nD][i][j][0]
          *dDVDt_mthreshold*dDVDt_mthreshold;
      }
      else{
        grid.dLocalGridOld[grid.nQ1][i][j][0]=0.0;
      }
    }
  }
  
  //inner ghost region
  #if SEDOV==1
    for(i=grid.nStartGhostUpdateExplicit[grid.nQ0][1][0];
      i<grid.nEndGhostUpdateExplicit[grid.nQ0][1][0];i++){
      
      //calculate i for interface centered quantities
      nIInt=i+grid.nCenIntOffset[0];
      dR_i=(grid.dLocalGridOld[grid.nR][nIInt][0][0]+grid.dLocalGridOld[grid.nR][nIInt-1][0][0])*0.5;
      dR_i_sq=dR_i*dR_i;
      dA_ip1half=grid.dLocalGridOld[grid.nR][nIInt][0][0]*grid.dLocalGridOld[grid.nR][nIInt][0][0];
      dA_im1half=grid.dLocalGridOld[grid.nR][nIInt-1][0][0]
        *grid.dLocalGridOld[grid.nR][nIInt-1][0][0];
      
      for(j=grid.nStartGhostUpdateExplicit[grid.nQ0][1][1];
        j<grid.nEndGhostUpdateExplicit[grid.nQ0][1][1];j++){
        
        //calculate j for interface centered quantities
        nJInt=j+grid.nCenIntOffset[1];
        
        //calculate Q0
        //calculate volume change
        dDVDt=(dA_ip1half*grid.dLocalGridOld[grid.nU][nIInt][j][0]
          -dA_im1half*grid.dLocalGridOld[grid.nU][nIInt-1][j][0])/dR_i_sq;
        
        //calculate threshold compression to turn viscosity on at
        dC=sqrt(parameters.dGamma*(grid.dLocalGridOld[grid.nP][i][j][0])
          /grid.dLocalGridOld[grid.nD][i][j][0]);
        dDVDtThreshold=parameters.dAVThreshold*dC;
        
        if(dDVDt<-1.0*dDVDtThreshold){//being compressed
          dDVDt_mthreshold=dDVDt+dDVDtThreshold;
          grid.dLocalGridOld[grid.nQ0][i][j][0]=dA_sq*grid.dLocalGridOld[grid.nD][i][j][0]
            *dDVDt_mthreshold*dDVDt_mthreshold;
        }
        else{
          grid.dLocalGridOld[grid.nQ0][i][j][0]=0.0;
        }
        
        //calculate Q1
        //calculate volume change
        dA_jp1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt][0];
        dA_jm1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt-1][0];
        dA_j=grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0];
        
        dDVDt=(dA_jp1half*grid.dLocalGridOld[grid.nV][i][nJInt][0]
          -dA_jm1half*grid.dLocalGridOld[grid.nV][i][nJInt-1][0])/dA_j;
        
        if(dDVDt<-1.0*dDVDtThreshold){//being compressed
          dDVDt_mthreshold=dDVDt+dDVDtThreshold;
          grid.dLocalGridOld[grid.nQ1][i][j][0]=dA_sq*grid.dLocalGridOld[grid.nD][i][j][0]
            *dDVDt_mthreshold*dDVDt_mthreshold;
        }
        else{
          grid.dLocalGridOld[grid.nQ1][i][j][0]=0.0;
        }
      }
    }
  #endif
}
void calOldQ0Q1_RT_TEOS(Grid& grid, Parameters &parameters){
  
  double dA_sq=parameters.dA*parameters.dA;
  double dDVDt;
  double dA_ip1half;
  double dA_im1half;
  double dA_jp1half;
  double dA_jm1half;
  double dA_j;
  double dR_i;
  int nIInt;
  int nJInt;
  double dC;
  double dDVDtThreshold;
  double dR_i_sq;
  double dDVDt_mthreshold;
  int i;
  int j;
  
  //in the main grid
  for(i=grid.nStartUpdateExplicit[grid.nQ0][0];i<grid.nEndUpdateExplicit[grid.nQ0][0];i++){
    
    //calculate i for interface centered quantities
    nIInt=i+grid.nCenIntOffset[0];
    dR_i=(grid.dLocalGridOld[grid.nR][nIInt][0][0]+grid.dLocalGridOld[grid.nR][nIInt-1][0][0])*0.5;
    dR_i_sq=dR_i*dR_i;
    dA_ip1half=grid.dLocalGridOld[grid.nR][nIInt][0][0]*grid.dLocalGridOld[grid.nR][nIInt][0][0];
    dA_im1half=grid.dLocalGridOld[grid.nR][nIInt-1][0][0]
      *grid.dLocalGridOld[grid.nR][nIInt-1][0][0];
    
    for(j=grid.nStartUpdateExplicit[grid.nQ0][1];j<grid.nEndUpdateExplicit[grid.nQ0][1];j++){
      
      //calculate j for interface centered quantities
      nJInt=j+grid.nCenIntOffset[1];
      
      //calculate Q0
      //calculate volume change
      dDVDt=(dA_ip1half*grid.dLocalGridOld[grid.nU][nIInt][j][0]
        -dA_im1half*grid.dLocalGridOld[grid.nU][nIInt-1][j][0])/dR_i_sq;
      
      //calculate threshold compression to turn viscosity on at
      dC=sqrt(grid.dLocalGridOld[grid.nGamma][i][j][0]*(grid.dLocalGridOld[grid.nP][i][j][0])
        /grid.dLocalGridOld[grid.nD][i][j][0]);
      dDVDtThreshold=parameters.dAVThreshold*dC;
      
      if(dDVDt<-1.0*dDVDtThreshold){//being compressed
        dDVDt_mthreshold=dDVDt+dDVDtThreshold;//need to add it since dVDt will be negative
        grid.dLocalGridOld[grid.nQ0][i][j][0]=dA_sq*grid.dLocalGridOld[grid.nD][i][j][0]
          *dDVDt_mthreshold*dDVDt_mthreshold;
      }
      else{
        grid.dLocalGridOld[grid.nQ0][i][j][0]=0.0;
      }
      
      //calculate Q1
      //calculate volume change
      dA_jp1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt][0];
      dA_jm1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt-1][0];
      dA_j=grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0];
      
      dDVDt=(dA_jp1half*grid.dLocalGridOld[grid.nV][i][nJInt][0]
        -dA_jm1half*grid.dLocalGridOld[grid.nV][i][nJInt-1][0])/dA_j;
      
      if(dDVDt<-1.0*dDVDtThreshold){//being compressed
        dDVDt_mthreshold=dDVDt+dDVDtThreshold;
        grid.dLocalGridOld[grid.nQ1][i][j][0]=dA_sq*grid.dLocalGridOld[grid.nD][i][j][0]
          *dDVDt_mthreshold*dDVDt_mthreshold;
      }
      else{
        grid.dLocalGridOld[grid.nQ1][i][j][0]=0.0;
      }
    }
  }
  
  //outter ghost region
  for(i=grid.nStartGhostUpdateExplicit[grid.nQ0][0][0];
    i<grid.nEndGhostUpdateExplicit[grid.nQ0][0][0];i++){
    
    //calculate i for interface centered quantities
    nIInt=i+grid.nCenIntOffset[0];
    dR_i=(grid.dLocalGridOld[grid.nR][nIInt][0][0]+grid.dLocalGridOld[grid.nR][nIInt-1][0][0])*0.5;
    dR_i_sq=dR_i*dR_i;
    dA_ip1half=grid.dLocalGridOld[grid.nR][nIInt][0][0]*grid.dLocalGridOld[grid.nR][nIInt][0][0];
    dA_im1half=grid.dLocalGridOld[grid.nR][nIInt-1][0][0]
      *grid.dLocalGridOld[grid.nR][nIInt-1][0][0];
    
    for(j=grid.nStartGhostUpdateExplicit[grid.nQ0][0][1];
      j<grid.nEndGhostUpdateExplicit[grid.nQ0][0][1];j++){
      
      //calculate j for interface centered quantities
      nJInt=j+grid.nCenIntOffset[1];
      
      //calculate Q0
      //calculate volume change
      dDVDt=(dA_ip1half*grid.dLocalGridOld[grid.nU][nIInt][j][0]
        -dA_im1half*grid.dLocalGridOld[grid.nU][nIInt-1][j][0])/dR_i_sq;
      
      //calculate threshold compression to turn viscosity on at
      dC=sqrt(grid.dLocalGridOld[grid.nGamma][i][j][0]*(grid.dLocalGridOld[grid.nP][i][j][0])
        /grid.dLocalGridOld[grid.nD][i][j][0]);
      dDVDtThreshold=parameters.dAVThreshold*dC;
      
      if(dDVDt<-1.0*dDVDtThreshold){//being compressed
        dDVDt_mthreshold=dDVDt+dDVDtThreshold;
        grid.dLocalGridOld[grid.nQ0][i][j][0]=dA_sq*grid.dLocalGridOld[grid.nD][i][j][0]
          *dDVDt_mthreshold*dDVDt_mthreshold;
      }
      else{
        grid.dLocalGridOld[grid.nQ0][i][j][0]=0.0;
      }
      
      //calculate Q1
      //calculate volume change
      dA_jp1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt][0];
      dA_jm1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt-1][0];
      dA_j=grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0];
      
      dDVDt=(dA_jp1half*grid.dLocalGridOld[grid.nV][i][nJInt][0]
        -dA_jm1half*grid.dLocalGridOld[grid.nV][i][nJInt-1][0])/dA_j;
      
      if(dDVDt<-1.0*dDVDtThreshold){//being compressed
        dDVDt_mthreshold=dDVDt+dDVDtThreshold;
        grid.dLocalGridOld[grid.nQ1][i][j][0]=dA_sq*grid.dLocalGridOld[grid.nD][i][j][0]
          *dDVDt_mthreshold*dDVDt_mthreshold;
      }
      else{
        grid.dLocalGridOld[grid.nQ1][i][j][0]=0.0;
      }
    }
  }
}
void calOldQ0Q1Q2_RTP_GL(Grid& grid, Parameters &parameters){
  
  double dA_sq=parameters.dA*parameters.dA;
  double dDVDt;
  double dA_ip1half;
  double dA_im1half;
  double dA_jp1half;
  double dA_jm1half;
  double dA_j;
  double dR_i;
  int nIInt;
  int nJInt;
  int nKInt;
  double dC;
  double dDVDtThreshold;
  double dR_i_sq;
  double dDVDt_mthreshold;
  int i;
  int j;
  int k;
  
  //in the main grid
  for(i=grid.nStartUpdateExplicit[grid.nQ0][0];i<grid.nEndUpdateExplicit[grid.nQ0][0];i++){
    
    //calculate i for interface centered quantities
    nIInt=i+grid.nCenIntOffset[0];
    dR_i=(grid.dLocalGridOld[grid.nR][nIInt][0][0]+grid.dLocalGridOld[grid.nR][nIInt-1][0][0])*0.5;
    dR_i_sq=dR_i*dR_i;
    dA_ip1half=grid.dLocalGridOld[grid.nR][nIInt][0][0]*grid.dLocalGridOld[grid.nR][nIInt][0][0];
    dA_im1half=grid.dLocalGridOld[grid.nR][nIInt-1][0][0]
      *grid.dLocalGridOld[grid.nR][nIInt-1][0][0];
    
    for(j=grid.nStartUpdateExplicit[grid.nQ0][1];j<grid.nEndUpdateExplicit[grid.nQ0][1];j++){
      
      //calculate j for interface centered quantities
      nJInt=j+grid.nCenIntOffset[1];
      dA_jp1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt][0];
      dA_jm1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt-1][0];
      dA_j=grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0];
      
      for(k=grid.nStartUpdateExplicit[grid.nQ0][2];k<grid.nEndUpdateExplicit[grid.nQ0][2];k++){
        
        nKInt=k+grid.nCenIntOffset[2];
        
        //calculate threshold compression to turn viscosity on at
        dC=sqrt(parameters.dGamma*(grid.dLocalGridOld[grid.nP][i][j][k])
          /grid.dLocalGridOld[grid.nD][i][j][k]);
        dDVDtThreshold=parameters.dAVThreshold*dC;
        
        //calculate Q0
        dDVDt=(dA_ip1half*grid.dLocalGridOld[grid.nU][nIInt][j][k]
          -dA_im1half*grid.dLocalGridOld[grid.nU][nIInt-1][j][k])/dR_i_sq;
        if(dDVDt<-1.0*dDVDtThreshold){//being compressed
          dDVDt_mthreshold=dDVDt+dDVDtThreshold;
          grid.dLocalGridOld[grid.nQ0][i][j][k]=dA_sq*grid.dLocalGridOld[grid.nD][i][j][k]
            *dDVDt_mthreshold*dDVDt_mthreshold;
        }
        else{
          grid.dLocalGridOld[grid.nQ0][i][j][k]=0.0;
        }
        
        //calculate Q1
        dDVDt=(dA_jp1half*grid.dLocalGridOld[grid.nV][i][nJInt][k]
          -dA_jm1half*grid.dLocalGridOld[grid.nV][i][nJInt-1][k])/dA_j;
        if(dDVDt<-1.0*dDVDtThreshold){//being compressed
          dDVDt_mthreshold=dDVDt+dDVDtThreshold;
          grid.dLocalGridOld[grid.nQ1][i][j][k]=dA_sq*grid.dLocalGridOld[grid.nD][i][j][k]
            *dDVDt_mthreshold*dDVDt_mthreshold;
        }
        else{
          grid.dLocalGridOld[grid.nQ1][i][j][k]=0.0;
        }
        
        //calculate Q2
        dDVDt=(grid.dLocalGridOld[grid.nW][i][j][nKInt]
          -grid.dLocalGridOld[grid.nW][i][j][nKInt-1]);
        if(dDVDt<-1.0*dDVDtThreshold){//being compressed
          dDVDt_mthreshold=dDVDt+dDVDtThreshold;
          grid.dLocalGridOld[grid.nQ2][i][j][k]=dA_sq*grid.dLocalGridOld[grid.nD][i][j][k]
            *dDVDt_mthreshold*dDVDt_mthreshold;
        }
        else{
          grid.dLocalGridOld[grid.nQ2][i][j][k]=0.0;
        }
        
      }
    }
  }
  
  //outter ghost region
  for(i=grid.nStartGhostUpdateExplicit[grid.nQ0][0][0];
    i<grid.nEndGhostUpdateExplicit[grid.nQ0][0][0];i++){
    
    //calculate i for interface centered quantities
    nIInt=i+grid.nCenIntOffset[0];
    dR_i=(grid.dLocalGridOld[grid.nR][nIInt][0][0]+grid.dLocalGridOld[grid.nR][nIInt-1][0][0])*0.5;
    dR_i_sq=dR_i*dR_i;
    dA_ip1half=grid.dLocalGridOld[grid.nR][nIInt][0][0]*grid.dLocalGridOld[grid.nR][nIInt][0][0];
    dA_im1half=grid.dLocalGridOld[grid.nR][nIInt-1][0][0]
      *grid.dLocalGridOld[grid.nR][nIInt-1][0][0];
    
    for(j=grid.nStartGhostUpdateExplicit[grid.nQ0][0][1];
      j<grid.nEndGhostUpdateExplicit[grid.nQ0][0][1];j++){
      
      //calculate j for interface centered quantities
      nJInt=j+grid.nCenIntOffset[1];
      
      for(k=grid.nStartGhostUpdateExplicit[grid.nQ0][0][2];
        k<grid.nEndGhostUpdateExplicit[grid.nQ0][0][2];k++){
        
        nKInt=k+grid.nCenIntOffset[2];
        
        //calculate threshold compression to turn viscosity on at
        dC=sqrt(parameters.dGamma*(grid.dLocalGridOld[grid.nP][i][j][k])
          /grid.dLocalGridOld[grid.nD][i][j][k]);
        dDVDtThreshold=parameters.dAVThreshold*dC;
        
        //calculate Q0
        dDVDt=(dA_ip1half*grid.dLocalGridOld[grid.nU][nIInt][j][k]
          -dA_im1half*grid.dLocalGridOld[grid.nU][nIInt-1][j][k])/dR_i_sq;
        if(dDVDt<-1.0*dDVDtThreshold){//being compressed
          dDVDt_mthreshold=dDVDt+dDVDtThreshold;
          grid.dLocalGridOld[grid.nQ0][i][j][k]=dA_sq*grid.dLocalGridOld[grid.nD][i][j][k]
            *dDVDt_mthreshold*dDVDt_mthreshold;
        }
        else{
          grid.dLocalGridOld[grid.nQ0][i][j][k]=0.0;
        }
        
        //calculate Q1
        dDVDt=(dA_jp1half*grid.dLocalGridOld[grid.nV][i][nJInt][k]
          -dA_jm1half*grid.dLocalGridOld[grid.nV][i][nJInt-1][k])/dA_j;
        if(dDVDt<-1.0*dDVDtThreshold){//being compressed
          dDVDt_mthreshold=dDVDt+dDVDtThreshold;
          grid.dLocalGridOld[grid.nQ1][i][j][k]=dA_sq*grid.dLocalGridOld[grid.nD][i][j][k]
            *dDVDt_mthreshold*dDVDt_mthreshold;
        }
        else{
          grid.dLocalGridOld[grid.nQ1][i][j][k]=0.0;
        }
        
        //calculate Q2
        dDVDt=(grid.dLocalGridOld[grid.nW][i][j][nKInt]
          -grid.dLocalGridOld[grid.nW][i][j][nKInt-1]);
        if(dDVDt<-1.0*dDVDtThreshold){//being compressed
          dDVDt_mthreshold=dDVDt+dDVDtThreshold;
          grid.dLocalGridOld[grid.nQ2][i][j][k]=dA_sq*grid.dLocalGridOld[grid.nD][i][j][k]
            *dDVDt_mthreshold*dDVDt_mthreshold;
        }
        else{
          grid.dLocalGridOld[grid.nQ2][i][j][k]=0.0;
        }
      }
    }
  }
  
  //inner ghost region
  #if SEDOV==1
    for(i=grid.nStartGhostUpdateExplicit[grid.nQ0][1][0];
      i<grid.nEndGhostUpdateExplicit[grid.nQ0][1][0];i++){
      
      //calculate i for interface centered quantities
      nIInt=i+grid.nCenIntOffset[0];
      dR_i=(grid.dLocalGridOld[grid.nR][nIInt][0][0]+grid.dLocalGridOld[grid.nR][nIInt-1][0][0])*0.5;
      dR_i_sq=dR_i*dR_i;
      dA_ip1half=grid.dLocalGridOld[grid.nR][nIInt][0][0]*grid.dLocalGridOld[grid.nR][nIInt][0][0];
      dA_im1half=grid.dLocalGridOld[grid.nR][nIInt-1][0][0]
        *grid.dLocalGridOld[grid.nR][nIInt-1][0][0];
      
      for(j=grid.nStartGhostUpdateExplicit[grid.nQ0][1][1];
        j<grid.nEndGhostUpdateExplicit[grid.nQ0][1][1];j++){
        
        //calculate j for interface centered quantities
        nJInt=j+grid.nCenIntOffset[1];
        
        for(k=grid.nStartGhostUpdateExplicit[grid.nQ0][1][2];
          k<grid.nEndGhostUpdateExplicit[grid.nQ0][1][2];k++){
          
          nKInt=k+grid.nCenIntOffset[2];
          
          //calculate threshold compression to turn viscosity on at
          dC=sqrt(parameters.dGamma*(grid.dLocalGridOld[grid.nP][i][j][k])
            /grid.dLocalGridOld[grid.nD][i][j][k]);
          dDVDtThreshold=parameters.dAVThreshold*dC;
          
          //calculate Q0
          dDVDt=(dA_ip1half*grid.dLocalGridOld[grid.nU][nIInt][j][k]
            -dA_im1half*grid.dLocalGridOld[grid.nU][nIInt-1][j][k])/dR_i_sq;
          if(dDVDt<-1.0*dDVDtThreshold){//being compressed
            dDVDt_mthreshold=dDVDt+dDVDtThreshold;
            grid.dLocalGridOld[grid.nQ0][i][j][k]=dA_sq*grid.dLocalGridOld[grid.nD][i][j][k]
              *dDVDt_mthreshold*dDVDt_mthreshold;
          }
          else{
            grid.dLocalGridOld[grid.nQ0][i][j][k]=0.0;
          }
          
          //calculate Q1
          dDVDt=(dA_jp1half*grid.dLocalGridOld[grid.nV][i][nJInt][k]
            -dA_jm1half*grid.dLocalGridOld[grid.nV][i][nJInt-1][k])/dA_j;
          if(dDVDt<-1.0*dDVDtThreshold){//being compressed
            dDVDt_mthreshold=dDVDt+dDVDtThreshold;
            grid.dLocalGridOld[grid.nQ1][i][j][k]=dA_sq*grid.dLocalGridOld[grid.nD][i][j][k]
              *dDVDt_mthreshold*dDVDt_mthreshold;
          }
          else{
            grid.dLocalGridOld[grid.nQ1][i][j][k]=0.0;
          }
          
          //calculate Q2
          dDVDt=(grid.dLocalGridOld[grid.nW][i][j][nKInt]
            -grid.dLocalGridOld[grid.nW][i][j][nKInt-1]);
          if(dDVDt<-1.0*dDVDtThreshold){//being compressed
            dDVDt_mthreshold=dDVDt+dDVDtThreshold;
            grid.dLocalGridOld[grid.nQ2][i][j][k]=dA_sq*grid.dLocalGridOld[grid.nD][i][j][k]
              *dDVDt_mthreshold*dDVDt_mthreshold;
          }
          else{
            grid.dLocalGridOld[grid.nQ2][i][j][k]=0.0;
          }
        }
      }
    }
  #endif
}
void calOldQ0Q1Q2_RTP_TEOS(Grid& grid, Parameters &parameters){
  
  double dA_sq=parameters.dA*parameters.dA;
  double dDVDt;/*Rate of change of the volume*/
  double dA_ip1half;
  double dA_im1half;
  double dA_jp1half;
  double dA_jm1half;
  double dA_j;
  double dR_i;
  int nIInt;
  int nJInt;
  int nKInt;
  double dC;
  double dDVDtThreshold;
  double dR_i_sq;
  double dDVDt_mthreshold;
  int i;
  int j;
  int k;
  
  //in the main grid
  for(i=grid.nStartUpdateExplicit[grid.nQ0][0];i<grid.nEndUpdateExplicit[grid.nQ0][0];i++){
    
    //calculate i for interface centered quantities
    nIInt=i+grid.nCenIntOffset[0];
    dR_i=(grid.dLocalGridOld[grid.nR][nIInt][0][0]+grid.dLocalGridOld[grid.nR][nIInt-1][0][0])*0.5;
    dR_i_sq=dR_i*dR_i;
    dA_ip1half=grid.dLocalGridOld[grid.nR][nIInt][0][0]*grid.dLocalGridOld[grid.nR][nIInt][0][0];
    dA_im1half=grid.dLocalGridOld[grid.nR][nIInt-1][0][0]
      *grid.dLocalGridOld[grid.nR][nIInt-1][0][0];
    
    for(j=grid.nStartUpdateExplicit[grid.nQ0][1];j<grid.nEndUpdateExplicit[grid.nQ0][1];j++){
      
      //calculate j for interface centered quantities
      nJInt=j+grid.nCenIntOffset[1];
      dA_jp1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt][0];
      dA_jm1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt-1][0];
      dA_j=grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0];
      
      for(k=grid.nStartUpdateExplicit[grid.nQ0][2];k<grid.nEndUpdateExplicit[grid.nQ0][2];k++){
        
        nKInt=k+grid.nCenIntOffset[2];
        
        //calculate threshold compression to turn viscosity on at
        dC=sqrt(grid.dLocalGridOld[grid.nGamma][i][j][k]*(grid.dLocalGridOld[grid.nP][i][j][k])
          /grid.dLocalGridOld[grid.nD][i][j][k]);
        dDVDtThreshold=parameters.dAVThreshold*dC;
        
        //calculate Q0
        dDVDt=(dA_ip1half*grid.dLocalGridOld[grid.nU][nIInt][j][k]
          -dA_im1half*grid.dLocalGridOld[grid.nU][nIInt-1][j][k])/dR_i_sq;
        if(dDVDt<-1.0*dDVDtThreshold){//being compressed
          dDVDt_mthreshold=dDVDt+dDVDtThreshold;
          grid.dLocalGridOld[grid.nQ0][i][j][k]=dA_sq*grid.dLocalGridOld[grid.nD][i][j][k]
            *dDVDt_mthreshold*dDVDt_mthreshold;
        }
        else{
          grid.dLocalGridOld[grid.nQ0][i][j][k]=0.0;
        }
        
        //calculate Q1
        dDVDt=(dA_jp1half*grid.dLocalGridOld[grid.nV][i][nJInt][k]
          -dA_jm1half*grid.dLocalGridOld[grid.nV][i][nJInt-1][k])/dA_j;
        if(dDVDt<-1.0*dDVDtThreshold){//being compressed
          dDVDt_mthreshold=dDVDt+dDVDtThreshold;
          grid.dLocalGridOld[grid.nQ1][i][j][k]=dA_sq*grid.dLocalGridOld[grid.nD][i][j][k]
            *dDVDt_mthreshold*dDVDt_mthreshold;
        }
        else{
          grid.dLocalGridOld[grid.nQ1][i][j][k]=0.0;
        }
        
        //calculate Q2
        dDVDt=(grid.dLocalGridOld[grid.nW][i][j][nKInt]
          -grid.dLocalGridOld[grid.nW][i][j][nKInt-1]);
        if(dDVDt<-1.0*dDVDtThreshold){//being compressed
          dDVDt_mthreshold=dDVDt+dDVDtThreshold;
          grid.dLocalGridOld[grid.nQ2][i][j][k]=dA_sq*grid.dLocalGridOld[grid.nD][i][j][k]
            *dDVDt_mthreshold*dDVDt_mthreshold;
        }
        else{
          grid.dLocalGridOld[grid.nQ2][i][j][k]=0.0;
        }
        
      }
    }
  }
  
  //outter ghost region
  for(i=grid.nStartGhostUpdateExplicit[grid.nQ0][0][0];
    i<grid.nEndGhostUpdateExplicit[grid.nQ0][0][0];i++){
    
    //calculate i for interface centered quantities
    nIInt=i+grid.nCenIntOffset[0];
    dR_i=(grid.dLocalGridOld[grid.nR][nIInt][0][0]+grid.dLocalGridOld[grid.nR][nIInt-1][0][0])*0.5;
    dR_i_sq=dR_i*dR_i;
    dA_ip1half=grid.dLocalGridOld[grid.nR][nIInt][0][0]*grid.dLocalGridOld[grid.nR][nIInt][0][0];
    dA_im1half=grid.dLocalGridOld[grid.nR][nIInt-1][0][0]
      *grid.dLocalGridOld[grid.nR][nIInt-1][0][0];
    
    for(j=grid.nStartGhostUpdateExplicit[grid.nQ0][0][1];
      j<grid.nEndGhostUpdateExplicit[grid.nQ0][0][1];j++){
      
      //calculate j for interface centered quantities
      nJInt=j+grid.nCenIntOffset[1];
      dA_jp1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt][0];
      dA_jm1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt-1][0];
      dA_j=grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0];
      
      for(k=grid.nStartGhostUpdateExplicit[grid.nQ0][0][2];
        k<grid.nEndGhostUpdateExplicit[grid.nQ0][0][2];k++){
        
        nKInt=k+grid.nCenIntOffset[2];
        
        //calculate threshold compression to turn viscosity on at
        dC=sqrt(grid.dLocalGridOld[grid.nGamma][i][j][k]*(grid.dLocalGridOld[grid.nP][i][j][k])
          /grid.dLocalGridOld[grid.nD][i][j][k]);
        dDVDtThreshold=parameters.dAVThreshold*dC;
        
        //calculate Q0
        dDVDt=(dA_ip1half*grid.dLocalGridOld[grid.nU][nIInt][j][k]
          -dA_im1half*grid.dLocalGridOld[grid.nU][nIInt-1][j][k])/dR_i_sq;
        if(dDVDt<-1.0*dDVDtThreshold){//being compressed
          dDVDt_mthreshold=dDVDt+dDVDtThreshold;
          grid.dLocalGridOld[grid.nQ0][i][j][k]=dA_sq*grid.dLocalGridOld[grid.nD][i][j][k]
            *dDVDt_mthreshold*dDVDt_mthreshold;
        }
        else{
          grid.dLocalGridOld[grid.nQ0][i][j][k]=0.0;
        }
        
        //calculate Q1
        dDVDt=(dA_jp1half*grid.dLocalGridOld[grid.nV][i][nJInt][k]
          -dA_jm1half*grid.dLocalGridOld[grid.nV][i][nJInt-1][k])/dA_j;
        if(dDVDt<-1.0*dDVDtThreshold){//being compressed
          dDVDt_mthreshold=dDVDt+dDVDtThreshold;
          grid.dLocalGridOld[grid.nQ1][i][j][k]=dA_sq*grid.dLocalGridOld[grid.nD][i][j][k]
            *dDVDt_mthreshold*dDVDt_mthreshold;
        }
        else{
          grid.dLocalGridOld[grid.nQ1][i][j][k]=0.0;
        }
        
        //calculate Q2
        dDVDt=(grid.dLocalGridOld[grid.nW][i][j][nKInt]
          -grid.dLocalGridOld[grid.nW][i][j][nKInt-1]);
        if(dDVDt<-1.0*dDVDtThreshold){//being compressed
          dDVDt_mthreshold=dDVDt+dDVDtThreshold;
          grid.dLocalGridOld[grid.nQ2][i][j][k]=dA_sq*grid.dLocalGridOld[grid.nD][i][j][k]
            *dDVDt_mthreshold*dDVDt_mthreshold;
        }
        else{
          grid.dLocalGridOld[grid.nQ2][i][j][k]=0.0;
        }
      }
    }
  }
  
  //inner ghost region
  #if SEDOV==1
    for(i=grid.nStartGhostUpdateExplicit[grid.nQ0][1][0];
      i<grid.nEndGhostUpdateExplicit[grid.nQ0][1][0];i++){
      
      //calculate i for interface centered quantities
      nIInt=i+grid.nCenIntOffset[0];
      dR_i=(grid.dLocalGridOld[grid.nR][nIInt][0][0]+grid.dLocalGridOld[grid.nR][nIInt-1][0][0])*0.5;
      dR_i_sq=dR_i*dR_i;
      dA_ip1half=grid.dLocalGridOld[grid.nR][nIInt][0][0]*grid.dLocalGridOld[grid.nR][nIInt][0][0];
      dA_im1half=grid.dLocalGridOld[grid.nR][nIInt-1][0][0]
        *grid.dLocalGridOld[grid.nR][nIInt-1][0][0];
      
      for(j=grid.nStartGhostUpdateExplicit[grid.nQ0][1][1];
        j<grid.nEndGhostUpdateExplicit[grid.nQ0][1][1];j++){
        
        //calculate j for interface centered quantities
        nJInt=j+grid.nCenIntOffset[1];
        dA_jp1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt][0];
        dA_jm1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt-1][0];
        dA_j=grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0];
        
        for(int k=grid.nStartGhostUpdateExplicit[grid.nQ0][1][2];
          k<grid.nEndGhostUpdateExplicit[grid.nQ0][1][2];k++){
          
          nKInt=k+grid.nCenIntOffset[2];
          
          //calculate threshold compression to turn viscosity on at
          dC=sqrt(grid.dLocalGridOld[grid.nGamma][i][j][k]*(grid.dLocalGridOld[grid.nP][i][j][k])
            /grid.dLocalGridOld[grid.nD][i][j][k]);
          dDVDtThreshold=parameters.dAVThreshold*dC;
          
          //calculate Q0
          dDVDt=(dA_ip1half*grid.dLocalGridOld[grid.nU][nIInt][j][k]
            -dA_im1half*grid.dLocalGridOld[grid.nU][nIInt-1][j][k])/dR_i_sq;
          if(dDVDt<-1.0*dDVDtThreshold){//being compressed
            dDVDt_mthreshold=dDVDt+dDVDtThreshold;
            grid.dLocalGridOld[grid.nQ0][i][j][k]=dA_sq*grid.dLocalGridOld[grid.nD][i][j][k]
              *dDVDt_mthreshold*dDVDt_mthreshold;
          }
          else{
            grid.dLocalGridOld[grid.nQ0][i][j][k]=0.0;
          }
          
          //calculate Q1
          dDVDt=(dA_jp1half*grid.dLocalGridOld[grid.nV][i][nJInt][k]
            -dA_jm1half*grid.dLocalGridOld[grid.nV][i][nJInt-1][k])/dA_j;
          if(dDVDt<-1.0*dDVDtThreshold){//being compressed
            dDVDt_mthreshold=dDVDt+dDVDtThreshold;
            grid.dLocalGridOld[grid.nQ1][i][j][k]=dA_sq*grid.dLocalGridOld[grid.nD][i][j][k]
              *dDVDt_mthreshold*dDVDt_mthreshold;
          }
          else{
            grid.dLocalGridOld[grid.nQ1][i][j][k]=0.0;
          }
          
          //calculate Q2
          dDVDt=(grid.dLocalGridOld[grid.nW][i][j][nKInt]
            -grid.dLocalGridOld[grid.nW][i][j][nKInt-1]);
          if(dDVDt<-1.0*dDVDtThreshold){//being compressed
            dDVDt_mthreshold=dDVDt+dDVDtThreshold;
            grid.dLocalGridOld[grid.nQ2][i][j][k]=dA_sq*grid.dLocalGridOld[grid.nD][i][j][k]
              *dDVDt_mthreshold*dDVDt_mthreshold;
          }
          else{
            grid.dLocalGridOld[grid.nQ2][i][j][k]=0.0;
          }

        }
      }
    }
  #endif
}
void calOldEddyVisc_R_CN(Grid &grid, Parameters &parameters){
  
  int nIInt;//i+1/2 index
  double dR_ip1half_np1half;
  double dR_im1half_np1half;
  double dDelR_i_np1half;
  double dLengthScaleSq;
  double dConstant=parameters.dEddyViscosity;
  
  //main grid explicit
  for(int i=grid.nStartUpdateExplicit[grid.nEddyVisc][0];
    i<grid.nEndUpdateExplicit[grid.nEddyVisc][0];i++){
      
    //calculate i for interface centered quantities
    nIInt=i+grid.nCenIntOffset[0];
    dR_ip1half_np1half=grid.dLocalGridOld[grid.nR][nIInt][0][0];
    dR_im1half_np1half=grid.dLocalGridOld[grid.nR][nIInt-1][0][0];
    dDelR_i_np1half=dR_ip1half_np1half-dR_im1half_np1half;
    
    for(int j=grid.nStartUpdateExplicit[grid.nEddyVisc][1];
      j<grid.nEndUpdateExplicit[grid.nEddyVisc][1];j++){
      for(int k=grid.nStartUpdateExplicit[grid.nEddyVisc][2];
        k<grid.nEndUpdateExplicit[grid.nEddyVisc][2];k++){
        
        dLengthScaleSq=dDelR_i_np1half*dDelR_i_np1half;
        grid.dLocalGridNew[grid.nEddyVisc][i][j][k]=dLengthScaleSq*dConstant
          *parameters.dMaxConvectiveVelocity/1.0e6;
      }
    }
  }
  
  //inner radial ghost cells, set to zero
  for(int i=0;i<grid.nNumGhostCells;i++){
    for(int j=grid.nStartUpdateExplicit[grid.nEddyVisc][1];
      j<grid.nEndUpdateExplicit[grid.nEddyVisc][1];j++){
      for(int k=grid.nStartUpdateExplicit[grid.nEddyVisc][2];
        k<grid.nEndUpdateExplicit[grid.nEddyVisc][2];k++){
        grid.dLocalGridOld[grid.nEddyVisc][i][j][k]=0.0;
      }
    }
  }
  
  //outter radial ghost cells,explicit
  for(int i=grid.nStartGhostUpdateExplicit[grid.nEddyVisc][0][0];
    i<grid.nEndGhostUpdateExplicit[grid.nEddyVisc][0][0];i++){
      
    //calculate i for interface centered quantities
    nIInt=i+grid.nCenIntOffset[0];
    dR_ip1half_np1half=grid.dLocalGridOld[grid.nR][nIInt][0][0];
    dR_im1half_np1half=grid.dLocalGridOld[grid.nR][nIInt-1][0][0];
    dDelR_i_np1half=dR_ip1half_np1half-dR_im1half_np1half;
    for(int j=grid.nStartGhostUpdateExplicit[grid.nEddyVisc][0][1];
      j<grid.nEndGhostUpdateExplicit[grid.nEddyVisc][0][1];j++){
      for(int k=grid.nStartGhostUpdateExplicit[grid.nEddyVisc][0][2];
        k<grid.nEndGhostUpdateExplicit[grid.nEddyVisc][0][2];k++){
        dLengthScaleSq=dDelR_i_np1half*dDelR_i_np1half;
        grid.dLocalGridNew[grid.nEddyVisc][i][j][k]=dLengthScaleSq*dConstant
          *parameters.dMaxConvectiveVelocity/1.0e6;
      }
    }
  }
}
void calOldEddyVisc_RT_CN(Grid &grid, Parameters &parameters){
  
  int nIInt;//i+1/2 index
  double dR_ip1half_np1half;
  double dR_im1half_np1half;
  double dR_i_np1half;
  double dDelR_i_np1half;
  double dLengthScaleSq;
  double dConstant=parameters.dEddyViscosity;
  
  //main grid explicit
  for(int i=grid.nStartUpdateExplicit[grid.nEddyVisc][0];
    i<grid.nEndUpdateExplicit[grid.nEddyVisc][0];i++){
      
    //calculate i for interface centered quantities
    nIInt=i+grid.nCenIntOffset[0];
    dR_ip1half_np1half=grid.dLocalGridOld[grid.nR][nIInt][0][0];
    dR_im1half_np1half=grid.dLocalGridOld[grid.nR][nIInt-1][0][0];
    dR_i_np1half=(dR_ip1half_np1half+dR_im1half_np1half)*0.5;
    dDelR_i_np1half=dR_ip1half_np1half-dR_im1half_np1half;
      
    for(int j=grid.nStartUpdateExplicit[grid.nEddyVisc][1];
      j<grid.nEndUpdateExplicit[grid.nEddyVisc][1];j++){
      for(int k=grid.nStartUpdateExplicit[grid.nEddyVisc][2];
        k<grid.nEndUpdateExplicit[grid.nEddyVisc][2];k++){
        dLengthScaleSq=dDelR_i_np1half*dR_i_np1half*grid.dLocalGridOld[grid.nDTheta][0][j][0];
        grid.dLocalGridNew[grid.nEddyVisc][i][j][k]=dLengthScaleSq*dConstant
          *parameters.dMaxConvectiveVelocity/1.0e6;
      }
    }
  }
  
  //inner radial ghost cells, set to zero
  for(int i=0;i<grid.nNumGhostCells;i++){
    for(int j=grid.nStartUpdateExplicit[grid.nEddyVisc][1];
      j<grid.nEndUpdateExplicit[grid.nEddyVisc][1];j++){
      for(int k=grid.nStartUpdateExplicit[grid.nEddyVisc][2];
        k<grid.nEndUpdateExplicit[grid.nEddyVisc][2];k++){
        grid.dLocalGridOld[grid.nEddyVisc][i][j][k]=0.0;
      }
    }
  }
  
  //outter radial ghost cells,explicit
  for(int i=grid.nStartGhostUpdateExplicit[grid.nEddyVisc][0][0];
    i<grid.nEndGhostUpdateExplicit[grid.nEddyVisc][0][0];i++){
      
    //calculate i for interface centered quantities
    nIInt=i+grid.nCenIntOffset[0];
    dR_ip1half_np1half=grid.dLocalGridOld[grid.nR][nIInt][0][0];
    dR_im1half_np1half=grid.dLocalGridOld[grid.nR][nIInt-1][0][0];
    dR_i_np1half=(dR_ip1half_np1half+dR_im1half_np1half)*0.5;
    dDelR_i_np1half=dR_ip1half_np1half-dR_im1half_np1half;
    for(int j=grid.nStartGhostUpdateExplicit[grid.nEddyVisc][0][1];
      j<grid.nEndGhostUpdateExplicit[grid.nEddyVisc][0][1];j++){
      for(int k=grid.nStartGhostUpdateExplicit[grid.nEddyVisc][0][2];
        k<grid.nEndGhostUpdateExplicit[grid.nEddyVisc][0][2];k++){
        dLengthScaleSq=dDelR_i_np1half*dR_i_np1half*grid.dLocalGridOld[grid.nDTheta][0][j][0];
        grid.dLocalGridNew[grid.nEddyVisc][i][j][k]=dLengthScaleSq*dConstant
          *parameters.dMaxConvectiveVelocity/1.0e6;
      }
    }
  }
}
void calOldEddyVisc_RTP_CN(Grid &grid, Parameters &parameters){
  int i;
  int j;
  int k;
  int nIInt;
  double dR_ip1half_np1half;
  double dR_im1half_np1half;
  double dR_i_np1half;
  double dDelR_i_np1half;
  double dR_i_np1half_Sq;
  double dLengthScaleSq;
  double dConstant=parameters.dEddyViscosity;
  
  //main grid
  for(i=grid.nStartUpdateExplicit[grid.nEddyVisc][0];
    i<grid.nEndUpdateExplicit[grid.nEddyVisc][0];i++){
      
    //calculate i for interface centered quantities
    nIInt=i+grid.nCenIntOffset[0];
    dR_ip1half_np1half=grid.dLocalGridOld[grid.nR][nIInt][0][0];
    dR_im1half_np1half=grid.dLocalGridOld[grid.nR][nIInt-1][0][0];
    dR_i_np1half=(dR_ip1half_np1half+dR_im1half_np1half)*0.5;
    dR_i_np1half_Sq=dR_i_np1half*dR_i_np1half;
    dDelR_i_np1half=dR_ip1half_np1half-dR_im1half_np1half;
    
    for(j=grid.nStartUpdateExplicit[grid.nEddyVisc][1];
      j<grid.nEndUpdateExplicit[grid.nEddyVisc][1];j++){
      for(k=grid.nStartUpdateExplicit[grid.nEddyVisc][2];
        k<grid.nEndUpdateExplicit[grid.nEddyVisc][2];k++){
        dLengthScaleSq=dDelR_i_np1half*dR_i_np1half_Sq*grid.dLocalGridOld[grid.nDTheta][0][j][0]
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]*grid.dLocalGridOld[grid.nDPhi][0][0][k];
        dLengthScaleSq=pow(dLengthScaleSq,0.666666666666666);
        grid.dLocalGridNew[grid.nEddyVisc][i][j][k]=dLengthScaleSq*dConstant
          *parameters.dMaxConvectiveVelocity/1.0e6;
      }
    }
  }
  
  //inner radial ghost cells, set to zero
  for(i=0;i<grid.nNumGhostCells;i++){
    for(j=grid.nStartUpdateExplicit[grid.nEddyVisc][1];
      j<grid.nEndUpdateExplicit[grid.nEddyVisc][1];j++){
      for(k=grid.nStartUpdateExplicit[grid.nEddyVisc][2];
        k<grid.nEndUpdateExplicit[grid.nEddyVisc][2];k++){
        grid.dLocalGridOld[grid.nEddyVisc][i][j][k]=0.0;
      }
    }
  }
  
  //outter radial ghost cells,explicit
  for(int i=grid.nStartGhostUpdateExplicit[grid.nEddyVisc][0][0];
    i<grid.nEndGhostUpdateExplicit[grid.nEddyVisc][0][0];i++){
    
    //calculate i for interface centered quantities
    nIInt=i+grid.nCenIntOffset[0];
    dR_ip1half_np1half=grid.dLocalGridOld[grid.nR][nIInt][0][0];
    dR_im1half_np1half=grid.dLocalGridOld[grid.nR][nIInt-1][0][0];
    dR_i_np1half=(dR_ip1half_np1half+dR_im1half_np1half)*0.5;
    dDelR_i_np1half=dR_ip1half_np1half-dR_im1half_np1half;
    
    for(int j=grid.nStartGhostUpdateExplicit[grid.nEddyVisc][0][1];
      j<grid.nEndGhostUpdateExplicit[grid.nEddyVisc][0][1];j++){
      for(int k=grid.nStartGhostUpdateExplicit[grid.nEddyVisc][0][2];
        k<grid.nEndGhostUpdateExplicit[grid.nEddyVisc][0][2];k++){
        dLengthScaleSq=dDelR_i_np1half*dR_i_np1half_Sq*grid.dLocalGridOld[grid.nDTheta][0][j][0]
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]*grid.dLocalGridOld[grid.nDPhi][0][0][k];
        dLengthScaleSq=pow(dLengthScaleSq,0.666666666666666);
        grid.dLocalGridNew[grid.nEddyVisc][i][j][k]=dLengthScaleSq*dConstant
          *parameters.dMaxConvectiveVelocity/1.0e6;
      }
    }
  }
}
void calOldEddyVisc_R_SM(Grid &grid, Parameters &parameters){
  
  int nIInt;//i+1/2 index
  double d1;//term 1
  double dA;
  double dR_ip1half_np1half;
  double dR_im1half_np1half;
  double dR_i_np1half;
  double dU_ip1halfjk_np1half;
  double dU_im1halfjk_np1half;
  double dDelR_i_np1half;
  double dConstantSq=parameters.dEddyViscosity*parameters.dEddyViscosity/pow(2.0,0.5);
  double dLengthScaleSq;
  double dTerms;
  //main grid explicit
  for(int i=grid.nStartUpdateExplicit[grid.nEddyVisc][0];
    i<grid.nEndUpdateExplicit[grid.nEddyVisc][0];i++){
      
    //calculate i for interface centered quantities
    nIInt=i+grid.nCenIntOffset[0];
    dR_ip1half_np1half=grid.dLocalGridOld[grid.nR][nIInt][0][0];
    dR_im1half_np1half=grid.dLocalGridOld[grid.nR][nIInt-1][0][0];
    dR_i_np1half=(dR_ip1half_np1half+dR_im1half_np1half)*0.5;
    dDelR_i_np1half=dR_ip1half_np1half-dR_im1half_np1half;
    
    for(int j=grid.nStartUpdateExplicit[grid.nEddyVisc][1];
      j<grid.nEndUpdateExplicit[grid.nEddyVisc][1];j++){
      for(int k=grid.nStartUpdateExplicit[grid.nEddyVisc][2];
        k<grid.nEndUpdateExplicit[grid.nEddyVisc][2];k++){
        
        dLengthScaleSq=dDelR_i_np1half*dDelR_i_np1half;
        
        //interpolate
        dU_ip1halfjk_np1half=grid.dLocalGridOld[grid.nU][nIInt][j][k];
        dU_im1halfjk_np1half=grid.dLocalGridOld[grid.nU][nIInt-1][j][k];
        
        //calculate dA term
        d1=(dU_ip1halfjk_np1half-dU_im1halfjk_np1half)/(dR_ip1half_np1half-dR_im1half_np1half);
        dA=2.0*d1*d1;
        
        dTerms=dA;
        grid.dLocalGridOld[grid.nEddyVisc][i][j][k]=dLengthScaleSq*dConstantSq
          *grid.dLocalGridOld[grid.nD][i][j][k]*pow(dTerms,0.5);
      }
    }
  }
  
  //inner radial ghost cells, set to zero
  for(int i=0;i<grid.nNumGhostCells;i++){
    for(int j=grid.nStartUpdateExplicit[grid.nEddyVisc][1];
      j<grid.nEndUpdateExplicit[grid.nEddyVisc][1];j++){
      for(int k=grid.nStartUpdateExplicit[grid.nEddyVisc][2];
        k<grid.nEndUpdateExplicit[grid.nEddyVisc][2];k++){
        grid.dLocalGridOld[grid.nEddyVisc][i][j][k]=0.0;
      }
    }
  }
  
  //outter radial ghost cells,explicit
  for(int i=grid.nStartGhostUpdateExplicit[grid.nEddyVisc][0][0];
    i<grid.nEndGhostUpdateExplicit[grid.nEddyVisc][0][0];i++){
      
    //calculate i for interface centered quantities
    nIInt=i+grid.nCenIntOffset[0];
    dR_ip1half_np1half=grid.dLocalGridOld[grid.nR][nIInt][0][0];
    dR_im1half_np1half=grid.dLocalGridOld[grid.nR][nIInt-1][0][0];
    dR_i_np1half=(dR_ip1half_np1half+dR_im1half_np1half)*0.5;
    dDelR_i_np1half=dR_ip1half_np1half-dR_im1half_np1half;
    for(int j=grid.nStartGhostUpdateExplicit[grid.nEddyVisc][0][1];
      j<grid.nEndGhostUpdateExplicit[grid.nEddyVisc][0][1];j++){
      for(int k=grid.nStartGhostUpdateExplicit[grid.nEddyVisc][0][2];
        k<grid.nEndGhostUpdateExplicit[grid.nEddyVisc][0][2];k++){
        
        dLengthScaleSq=dDelR_i_np1half*dDelR_i_np1half;
        
        //interpolate
        dU_ip1halfjk_np1half=grid.dLocalGridOld[grid.nU][nIInt][j][k];
        dU_im1halfjk_np1half=grid.dLocalGridOld[grid.nU][nIInt-1][j][k];
        
        //calculate dA term
        d1=(dU_ip1halfjk_np1half-dU_im1halfjk_np1half)/(dR_ip1half_np1half-dR_im1half_np1half);
        dA=2.0*d1*d1;
        
        dTerms=dA;
        grid.dLocalGridOld[grid.nEddyVisc][i][j][k]=dLengthScaleSq*dConstantSq
          *grid.dLocalGridOld[grid.nD][i][j][k]*pow(dTerms,0.5);
      }
    }
  }
}
void calOldEddyVisc_RT_SM(Grid &grid, Parameters &parameters){
  
  int nIInt;//i+1/2 index
  int nJInt;//j+1/2 index
  double d1;//term 1
  double d2;//term 2
  double d3;//term 3
  double d7;//term 7
  double d8;//term 8
  double d9;//term 9
  double dA;//term A
  double dB;//term B
  double dD;//term D
  double dE;//term E
  double dTerms;
  double dR_ip1half_np1half;
  double dR_im1half_np1half;
  double dR_i_np1half;
  double dU_ip1halfjk_np1half;
  double dU_im1halfjk_np1half;
  double dU_ijp1halfk_np1half;
  double dU_ijm1halfk_np1half;
  double dU_ijk_np1half;
  double dV_ijk_np1half;
  double dV_ip1halfjk_np1half;
  double dV_im1halfjk_np1half;
  double dV_ijp1halfk_np1half;
  double dV_ijm1halfk_np1half;
  double dDelR_i_np1half;
  double dConstantSq=parameters.dEddyViscosity*parameters.dEddyViscosity/pow(2.0,0.5);
  double dLengthScaleSq;
  double dU0_i_np1half;
  double dU0_ip1half_np1half;
  double dU0_im1half_np1half;
  
  //main grid explicit
  for(int i=grid.nStartUpdateExplicit[grid.nEddyVisc][0];
    i<grid.nEndUpdateExplicit[grid.nEddyVisc][0];i++){
      
    //calculate i for interface centered quantities
    nIInt=i+grid.nCenIntOffset[0];
    dR_ip1half_np1half=grid.dLocalGridOld[grid.nR][nIInt][0][0];
    dR_im1half_np1half=grid.dLocalGridOld[grid.nR][nIInt-1][0][0];
    dR_i_np1half=(dR_ip1half_np1half+dR_im1half_np1half)*0.5;
    dDelR_i_np1half=dR_ip1half_np1half-dR_im1half_np1half;
    dU0_ip1half_np1half=grid.dLocalGridOld[grid.nU0][nIInt][0][0];
    dU0_im1half_np1half=grid.dLocalGridOld[grid.nU0][nIInt-1][0][0];
    dU0_i_np1half=(grid.dLocalGridOld[grid.nU0][nIInt][0][0]
      +grid.dLocalGridOld[grid.nU0][nIInt-1][0][0])*0.5;
      
    for(int j=grid.nStartUpdateExplicit[grid.nEddyVisc][1];
      j<grid.nEndUpdateExplicit[grid.nEddyVisc][1];j++){
      
      nJInt=j+grid.nCenIntOffset[1];
      
      for(int k=grid.nStartUpdateExplicit[grid.nEddyVisc][2];
        k<grid.nEndUpdateExplicit[grid.nEddyVisc][2];k++){
        
        dLengthScaleSq=dDelR_i_np1half*dR_i_np1half*grid.dLocalGridOld[grid.nDTheta][0][j][0];
        
        //interpolate
        dU_ip1halfjk_np1half=grid.dLocalGridOld[grid.nU][nIInt][j][k];
        dU_im1halfjk_np1half=grid.dLocalGridOld[grid.nU][nIInt-1][j][k];
        dU_ijk_np1half=(grid.dLocalGridOld[grid.nU][nIInt][j][k]
          +grid.dLocalGridOld[grid.nU][nIInt-1][j][k])*0.5;
        dU_ijp1halfk_np1half=(grid.dLocalGridOld[grid.nU][nIInt][j][k]
          +grid.dLocalGridOld[grid.nU][nIInt-1][j][k]+grid.dLocalGridOld[grid.nU][nIInt][j+1][k]
          +grid.dLocalGridOld[grid.nU][nIInt-1][j+1][k])*0.25;
        dU_ijm1halfk_np1half=(grid.dLocalGridOld[grid.nU][nIInt][j][k]
          +grid.dLocalGridOld[grid.nU][nIInt-1][j][k]+grid.dLocalGridOld[grid.nU][nIInt][j-1][k]
          +grid.dLocalGridOld[grid.nU][nIInt-1][j-1][k])*0.25;
        dV_ijk_np1half=(grid.dLocalGridOld[grid.nV][i][nJInt][k]
          +grid.dLocalGridOld[grid.nV][i][nJInt-1][k])*0.5;
        dV_ip1halfjk_np1half=(grid.dLocalGridOld[grid.nV][i][nJInt][k]
          +grid.dLocalGridOld[grid.nV][i][nJInt-1][k]+grid.dLocalGridOld[grid.nV][i+1][nJInt][k]
          +grid.dLocalGridOld[grid.nV][i+1][nJInt-1][k])*0.25;
        dV_im1halfjk_np1half=(grid.dLocalGridOld[grid.nV][i][nJInt][k]
          +grid.dLocalGridOld[grid.nV][i][nJInt-1][k]+grid.dLocalGridOld[grid.nV][i-1][nJInt][k]
          +grid.dLocalGridOld[grid.nV][i-1][nJInt-1][k])*0.25;
        dV_ijp1halfk_np1half=grid.dLocalGridOld[grid.nV][i][nJInt][k];
        dV_ijm1halfk_np1half=grid.dLocalGridOld[grid.nV][i][nJInt-1][k];
        
        //term 1
        d1=((dU_ip1halfjk_np1half-dU0_ip1half_np1half)-(dU_im1halfjk_np1half-dU0_im1half_np1half))
          /(dR_ip1half_np1half-dR_im1half_np1half);
        
        //term 2
        d2=1.0/dR_i_np1half*(dU_ijp1halfk_np1half-dU_ijm1halfk_np1half)
          /grid.dLocalGridOld[grid.nDTheta][0][j][0];
        
        //term 3
        d3=dV_ijk_np1half/dR_i_np1half;
        
        //term 7
        d7=(dV_ip1halfjk_np1half-dV_im1halfjk_np1half)/dDelR_i_np1half;
        
        //term 8
        d8=1.0/dR_i_np1half*(dV_ijp1halfk_np1half-dV_ijm1halfk_np1half)
          /grid.dLocalGridOld[grid.nDTheta][0][j][0];
        
        //term 9
        d9=(dU_ijk_np1half-dU0_i_np1half)/dR_i_np1half;
        
        //term A
        dA=2.0*d1*d1;
        
        //term B
        dB=(d2+d1-d3)*(d2-d3);
        
        //term D
        dD=d7*(d2+d7-d3);
        
        //term E
        dE=d8+d9;
        dE=2.0*dE*dE;
        
        //term F
        
        dTerms=dA+dB+dD+dE;
        grid.dLocalGridOld[grid.nEddyVisc][i][j][k]=dLengthScaleSq*dConstantSq
          *grid.dLocalGridOld[grid.nD][i][j][k]*pow(dTerms,0.5);
      }
    }
  }
  
  //inner radial ghost cells, set to zero
  for(int i=0;i<grid.nNumGhostCells;i++){
    for(int j=grid.nStartUpdateExplicit[grid.nEddyVisc][1];
      j<grid.nEndUpdateExplicit[grid.nEddyVisc][1];j++){
      for(int k=grid.nStartUpdateExplicit[grid.nEddyVisc][2];
        k<grid.nEndUpdateExplicit[grid.nEddyVisc][2];k++){
        grid.dLocalGridOld[grid.nEddyVisc][i][j][k]=0.0;
      }
    }
  }
  
  //outter radial ghost cells,explicit
  for(int i=grid.nStartGhostUpdateExplicit[grid.nEddyVisc][0][0];
    i<grid.nEndGhostUpdateExplicit[grid.nEddyVisc][0][0];i++){
      
    //calculate i for interface centered quantities
    nIInt=i+grid.nCenIntOffset[0];
    dR_ip1half_np1half=grid.dLocalGridOld[grid.nR][nIInt][0][0];
    dR_im1half_np1half=grid.dLocalGridOld[grid.nR][nIInt-1][0][0];
    dR_i_np1half=(dR_ip1half_np1half+dR_im1half_np1half)*0.5;
    dDelR_i_np1half=dR_ip1half_np1half-dR_im1half_np1half;
    dU0_ip1half_np1half=grid.dLocalGridOld[grid.nU0][nIInt][0][0];
    dU0_im1half_np1half=grid.dLocalGridOld[grid.nU0][nIInt-1][0][0];
    dU0_i_np1half=(grid.dLocalGridOld[grid.nU0][nIInt][0][0]
      +grid.dLocalGridOld[grid.nU0][nIInt-1][0][0])*0.5;
    
    for(int j=grid.nStartGhostUpdateExplicit[grid.nEddyVisc][0][1];
      j<grid.nEndGhostUpdateExplicit[grid.nEddyVisc][0][1];j++){
      
      nJInt=j+grid.nCenIntOffset[1];
      
      for(int k=grid.nStartGhostUpdateExplicit[grid.nEddyVisc][0][2];
        k<grid.nEndGhostUpdateExplicit[grid.nEddyVisc][0][2];k++){
        
        dLengthScaleSq=dDelR_i_np1half*dR_i_np1half*grid.dLocalGridOld[grid.nDTheta][0][j][0];
        
        //interpolate
        dU_ip1halfjk_np1half=grid.dLocalGridOld[grid.nU][nIInt][j][k];
        dU_im1halfjk_np1half=grid.dLocalGridOld[grid.nU][nIInt-1][j][k];
        dU_ijk_np1half=(grid.dLocalGridOld[grid.nU][nIInt][j][k]
          +grid.dLocalGridOld[grid.nU][nIInt-1][j][k])*0.5;
        dU_ijp1halfk_np1half=(grid.dLocalGridOld[grid.nU][nIInt][j][k]
          +grid.dLocalGridOld[grid.nU][nIInt-1][j][k]+grid.dLocalGridOld[grid.nU][nIInt][j+1][k]
          +grid.dLocalGridOld[grid.nU][nIInt-1][j+1][k])*0.25;
        dU_ijm1halfk_np1half=(grid.dLocalGridOld[grid.nU][nIInt][j][k]
          +grid.dLocalGridOld[grid.nU][nIInt-1][j][k]+grid.dLocalGridOld[grid.nU][nIInt][j-1][k]
          +grid.dLocalGridOld[grid.nU][nIInt-1][j-1][k])*0.25;
        dV_ijk_np1half=(grid.dLocalGridOld[grid.nV][i][nJInt][k]
          +grid.dLocalGridOld[grid.nV][i][nJInt-1][k])*0.5;
        dV_ip1halfjk_np1half=(grid.dLocalGridOld[grid.nV][i][nJInt][k]
          +grid.dLocalGridOld[grid.nV][i][nJInt-1][k])*0.25;
        dV_im1halfjk_np1half=(grid.dLocalGridOld[grid.nV][i][nJInt][k]
          +grid.dLocalGridOld[grid.nV][i][nJInt-1][k]+grid.dLocalGridOld[grid.nV][i-1][nJInt][k]
          +grid.dLocalGridOld[grid.nV][i-1][nJInt-1][k])*0.25;
        dV_ijp1halfk_np1half=grid.dLocalGridOld[grid.nV][i][nJInt][k];
        dV_ijm1halfk_np1half=grid.dLocalGridOld[grid.nV][i][nJInt-1][k];
        
        //term 1
        d1=((dU_ip1halfjk_np1half-dU0_ip1half_np1half)-(dU_im1halfjk_np1half-dU0_im1half_np1half))
          /(dR_ip1half_np1half-dR_im1half_np1half);
        
        //term 2
        d2=1.0/dR_i_np1half*(dU_ijp1halfk_np1half-dU_ijm1halfk_np1half)
          /grid.dLocalGridOld[grid.nDTheta][0][j][0];
        
        //term 3
        d3=dV_ijk_np1half/dR_i_np1half;
        
        //term 7
        d7=(dV_ip1halfjk_np1half-dV_im1halfjk_np1half)/dDelR_i_np1half;
        
        //term 8
        d8=1.0/dR_i_np1half*(dV_ijp1halfk_np1half-dV_ijm1halfk_np1half)
          /grid.dLocalGridOld[grid.nDTheta][0][j][0];
        
        //term 9
        d9=(dU_ijk_np1half-dU0_i_np1half)/dR_i_np1half;
        
        //term A
        dA=2.0*d1*d1;
        
        //term B
        dB=(d2+d1-d3)*(d2-d3);
        
        //term D
        dD=d7*(d2+d7-d3);
        
        //term E
        dE=d8+d9;
        dE=2.0*dE*dE;
        
        dTerms=dA+dB+dD+dE;
        grid.dLocalGridOld[grid.nEddyVisc][i][j][k]=dLengthScaleSq*dConstantSq
          *grid.dLocalGridOld[grid.nD][i][j][k]*pow(dTerms,0.5);
      }
    }
  }
}
void calOldEddyVisc_RTP_SM(Grid &grid, Parameters &parameters){
  int i;
  int j;
  int k;
  int nIInt;//i+1/2 index
  int nJInt;//j+1/2 index
  int nKInt;//k+1/2 index
  double d1;
  double d2;
  double d3;
  double d4;
  double d5;
  double d6;
  double d7;
  double d8;
  double d9;
  double d10;
  double d11;
  double d12;
  double d13;
  double d14;
  double dA;
  double dB;
  double dC;
  double dD;
  double dE;
  double dF;
  double dG;
  double dH;
  double dI;
  double dTerms;
  double dR_ip1half_np1half;
  double dR_im1half_np1half;
  double dR_i_np1half;
  double dU0_ip1half_np1half;
  double dU0_im1half_np1half;
  double dU0_i_np1half;
  double dU_ip1halfjk_np1half;
  double dU_im1halfjk_np1half;
  double dU_ijp1halfk_np1half;
  double dU_ijm1halfk_np1half;
  double dU_ijk_np1half;
  double dU_ijkp1half_np1half;
  double dU_ijkm1half_np1half;
  double dV_ijk_np1half;
  double dV_ip1halfjk_np1half;
  double dV_im1halfjk_np1half;
  double dV_ijp1halfk_np1half;
  double dV_ijm1halfk_np1half;
  double dV_ijkp1half_np1half;
  double dV_ijkm1half_np1half;
  double dW_ijk_np1half;
  double dW_ip1halfjk_np1half;
  double dW_im1halfjk_np1half;
  double dW_ijp1halfk_np1half;
  double dW_ijm1halfk_np1half;
  double dW_ijkp1half_np1half;
  double dW_ijkm1half_np1half;
  double dDelR_i_np1half;
  double dR_i_np1half_sq;
  double dConstant=parameters.dEddyViscosity*parameters.dEddyViscosity/pow(2.0,0.5);
  double dLengthScaleSq;
  
  //main grid
  for(i=grid.nStartUpdateExplicit[grid.nEddyVisc][0];
    i<grid.nEndUpdateExplicit[grid.nEddyVisc][0];i++){
      
    //calculate i for interface centered quantities
    nIInt=i+grid.nCenIntOffset[0];
    dR_ip1half_np1half=grid.dLocalGridOld[grid.nR][nIInt][0][0];
    dR_im1half_np1half=grid.dLocalGridOld[grid.nR][nIInt-1][0][0];
    dR_i_np1half=(dR_ip1half_np1half+dR_im1half_np1half)*0.5;
    dR_i_np1half_sq=dR_i_np1half*dR_i_np1half;
    dDelR_i_np1half=dR_ip1half_np1half-dR_im1half_np1half;
    dU0_ip1half_np1half=grid.dLocalGridOld[grid.nU0][nIInt][0][0];
    dU0_im1half_np1half=grid.dLocalGridOld[grid.nU0][nIInt-1][0][0];
    dU0_i_np1half=(grid.dLocalGridOld[grid.nU0][nIInt][0][0]
      +grid.dLocalGridOld[grid.nU0][nIInt-1][0][0])*0.5;
    
    for(j=grid.nStartUpdateExplicit[grid.nEddyVisc][1];
      j<grid.nEndUpdateExplicit[grid.nEddyVisc][1];j++){
      
      nJInt=j+grid.nCenIntOffset[1];
      
      for(k=grid.nStartUpdateExplicit[grid.nEddyVisc][2];
        k<grid.nEndUpdateExplicit[grid.nEddyVisc][2];k++){
        
        nKInt=k+grid.nCenIntOffset[2];
        dLengthScaleSq=dR_i_np1half_sq*dDelR_i_np1half*grid.dLocalGridOld[grid.nDTheta][0][j][0]
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]*grid.dLocalGridOld[grid.nDPhi][0][0][k];
        dLengthScaleSq=pow(dLengthScaleSq,0.666666666666666);
        
        //interpolate
        dU_ip1halfjk_np1half=grid.dLocalGridOld[grid.nU][nIInt][j][k];
        dU_im1halfjk_np1half=grid.dLocalGridOld[grid.nU][nIInt-1][j][k];
        dU_ijk_np1half=(grid.dLocalGridOld[grid.nU][nIInt][j][k]
          +grid.dLocalGridOld[grid.nU][nIInt-1][j][k])*0.5;
        dU_ijkp1half_np1half=(grid.dLocalGridOld[grid.nU][nIInt][j][k+1]
          +grid.dLocalGridOld[grid.nU][nIInt][j][k]+grid.dLocalGridOld[grid.nU][nIInt-1][j][k+1]
          +grid.dLocalGridOld[grid.nU][nIInt-1][j][k])*0.25;
        dU_ijkm1half_np1half=(grid.dLocalGridOld[grid.nU][nIInt][j][k]
          +grid.dLocalGridOld[grid.nU][nIInt][j][k-1]+grid.dLocalGridOld[grid.nU][nIInt-1][j][k]
          +grid.dLocalGridOld[grid.nU][nIInt-1][j][k-1])*0.25;
        dU_ijp1halfk_np1half=(grid.dLocalGridOld[grid.nU][nIInt][j][k]
          +grid.dLocalGridOld[grid.nU][nIInt-1][j][k]+grid.dLocalGridOld[grid.nU][nIInt][j+1][k]
          +grid.dLocalGridOld[grid.nU][nIInt-1][j+1][k])*0.25;
        dU_ijm1halfk_np1half=(grid.dLocalGridOld[grid.nU][nIInt][j][k]
          +grid.dLocalGridOld[grid.nU][nIInt-1][j][k]+grid.dLocalGridOld[grid.nU][nIInt][j-1][k]
          +grid.dLocalGridOld[grid.nU][nIInt-1][j-1][k])*0.25;
        dV_ijk_np1half=(grid.dLocalGridOld[grid.nV][i][nJInt][k]
          +grid.dLocalGridOld[grid.nV][i][nJInt-1][k])*0.5;
        dV_ip1halfjk_np1half=(grid.dLocalGridOld[grid.nV][i][nJInt][k]
          +grid.dLocalGridOld[grid.nV][i][nJInt-1][k]+grid.dLocalGridOld[grid.nV][i+1][nJInt][k]
          +grid.dLocalGridOld[grid.nV][i+1][nJInt-1][k])*0.25;
        dV_im1halfjk_np1half=(grid.dLocalGridOld[grid.nV][i][nJInt][k]
          +grid.dLocalGridOld[grid.nV][i][nJInt-1][k]+grid.dLocalGridOld[grid.nV][i-1][nJInt][k]
          +grid.dLocalGridOld[grid.nV][i-1][nJInt-1][k])*0.25;
        dV_ijp1halfk_np1half=grid.dLocalGridOld[grid.nV][i][nJInt][k];
        dV_ijm1halfk_np1half=grid.dLocalGridOld[grid.nV][i][nJInt-1][k];
        dV_ijkp1half_np1half=(grid.dLocalGridOld[grid.nV][i][nJInt][k+1]
          +grid.dLocalGridOld[grid.nV][i][nJInt][k]
          +grid.dLocalGridOld[grid.nV][i][nJInt-1][k+1]
          +grid.dLocalGridOld[grid.nV][i][nJInt-1][k])*0.25;
        dV_ijkm1half_np1half=(grid.dLocalGridOld[grid.nV][i][nJInt][k]
          +grid.dLocalGridOld[grid.nV][i][nJInt][k-1]
          +grid.dLocalGridOld[grid.nV][i][nJInt-1][k]
          +grid.dLocalGridOld[grid.nV][i][nJInt-1][k-1])*0.25;
        dW_ijk_np1half=(grid.dLocalGridOld[grid.nW][i][j][nKInt]
          +grid.dLocalGridOld[grid.nW][i][j][nKInt-1])*0.5;
        dW_ip1halfjk_np1half=(grid.dLocalGridOld[grid.nW][i+1][j][nKInt]
          +grid.dLocalGridOld[grid.nW][i+1][j][nKInt-1]
          +grid.dLocalGridOld[grid.nW][i][j][nKInt]
          +grid.dLocalGridOld[grid.nW][i][j][nKInt-1])*0.25;
        dW_im1halfjk_np1half=(grid.dLocalGridOld[grid.nW][i-1][j][nKInt]
          +grid.dLocalGridOld[grid.nW][i-1][j][nKInt-1]
          +grid.dLocalGridOld[grid.nW][i][j][nKInt]
          +grid.dLocalGridOld[grid.nW][i][j][nKInt-1])*0.25;
        dW_ijp1halfk_np1half=(grid.dLocalGridOld[grid.nW][i][j+1][nKInt]
          +grid.dLocalGridOld[grid.nW][i][j+1][nKInt-1]
          +grid.dLocalGridOld[grid.nW][i][j][nKInt]
          +grid.dLocalGridOld[grid.nW][i][j][nKInt-1])*0.25;
        dW_ijm1halfk_np1half=(grid.dLocalGridOld[grid.nW][i][j-1][nKInt]
          +grid.dLocalGridOld[grid.nW][i][j-1][nKInt-1]
          +grid.dLocalGridOld[grid.nW][i][j][nKInt]
          +grid.dLocalGridOld[grid.nW][i][j][nKInt-1])*0.25;
        dW_ijkp1half_np1half=grid.dLocalGridOld[grid.nW][i][j][nKInt];
        dW_ijkm1half_np1half=grid.dLocalGridOld[grid.nW][i][j][nKInt-1];
        
        //term 1
        d1=((dU_ip1halfjk_np1half-dU0_ip1half_np1half)-(dU_im1halfjk_np1half-dU0_im1half_np1half))
          /(dR_ip1half_np1half-dR_im1half_np1half);
        
        //term 2
        d2=1.0/dR_i_np1half*(dU_ijp1halfk_np1half-dU_ijm1halfk_np1half)
          /grid.dLocalGridOld[grid.nDTheta][0][j][0];
        
        //term 3
        d3=dV_ijk_np1half/dR_i_np1half;
        
        //term 4
        d4=(dU_ijkp1half_np1half-dU_ijkm1half_np1half)/(dR_i_np1half
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]*grid.dLocalGridOld[grid.nDPhi][0][0][k]);
        
        //term 5
        d5=(dW_ip1halfjk_np1half-dW_im1halfjk_np1half)/(dR_ip1half_np1half-dR_im1half_np1half);
        
        //term 6
        d6=dW_ijk_np1half/dR_i_np1half;
        
        //term 7
        d7=(dV_ip1halfjk_np1half-dV_im1halfjk_np1half)/dDelR_i_np1half;
        
        //term 8
        d8=1.0/dR_i_np1half*(dV_ijp1halfk_np1half-dV_ijm1halfk_np1half)
          /grid.dLocalGridOld[grid.nDTheta][0][j][0];
        
        //term 9
        d9=(dU_ijk_np1half-dU0_i_np1half)/dR_i_np1half;
        
        //term 10
        d10=(dW_ijp1halfk_np1half-dW_ijm1halfk_np1half)/(dR_i_np1half
          *grid.dLocalGridOld[grid.nDTheta][0][j][0]);
        
        //term 11
        d11=(dV_ijkp1half_np1half-dV_ijkm1half_np1half)/(dR_i_np1half
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]*grid.dLocalGridOld[grid.nDPhi][0][0][k]);
        
        //term 12
        d12=dW_ijk_np1half*grid.dLocalGridOld[grid.nCotThetaIJK][0][j][0]/dR_i_np1half;
        
        //term 13
        d13=(dW_ijkp1half_np1half-dW_ijkm1half_np1half)/(dR_i_np1half
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]*grid.dLocalGridOld[grid.nDPhi][0][0][k]);
        
        //term 14
        d14=dV_ijk_np1half*grid.dLocalGridOld[grid.nCotThetaIJK][0][j][0]/dR_i_np1half;
        
        //term A
        dA=2.0*d1*d1;
        
        //term B
        dB=(d2+d1-d3)*(d2-d3);
        
        //term C
        dC=(d4+d5-d6)*(d4-d6);
        
        //term D
        dD=d7*(d2+d7-d3);
        
        //term E
        dE=d8+d9;
        dE=2.0*dE*dE;
        
        //term dF
        dF=(d10+d11-d12)*(d11-d12);
        
        //term dG
        dG=d5*(d4+d5-d6);
        
        //term dH
        dH=d10*(d10+d11-d12);
        
        //term dI
        dI=d13+d14+d9;
        dI=2.0*dI*dI;
        
        dTerms=dA+dB+dC+dD+dE+dF+dG+dH+dI;
        grid.dLocalGridOld[grid.nEddyVisc][i][j][k]=dConstant*dLengthScaleSq
          *grid.dLocalGridOld[grid.nD][i][j][k]*pow(dTerms,0.5);
      }
    }
  }
  
  //inner radial ghost cells, set to zero
  for(i=0;i<grid.nNumGhostCells;i++){
    for(j=grid.nStartUpdateExplicit[grid.nEddyVisc][1];
      j<grid.nEndUpdateExplicit[grid.nEddyVisc][1];j++){
      for(k=grid.nStartUpdateExplicit[grid.nEddyVisc][2];
        k<grid.nEndUpdateExplicit[grid.nEddyVisc][2];k++){
        grid.dLocalGridOld[grid.nEddyVisc][i][j][k]=0.0;
      }
    }
  }
  
  //outter radial ghost cells,explicit
  for(i=grid.nStartGhostUpdateExplicit[grid.nEddyVisc][0][0];
    i<grid.nEndGhostUpdateExplicit[grid.nEddyVisc][0][0];i++){
    
    //calculate i for interface centered quantities
    nIInt=i+grid.nCenIntOffset[0];
    dR_ip1half_np1half=grid.dLocalGridOld[grid.nR][nIInt][0][0];
    dR_im1half_np1half=grid.dLocalGridOld[grid.nR][nIInt-1][0][0];
    dR_i_np1half=(dR_ip1half_np1half+dR_im1half_np1half)*0.5;
    dR_i_np1half_sq=dR_i_np1half*dR_i_np1half;
    dDelR_i_np1half=dR_ip1half_np1half-dR_im1half_np1half;
    dU0_ip1half_np1half=grid.dLocalGridOld[grid.nU0][nIInt][0][0];
    dU0_im1half_np1half=grid.dLocalGridOld[grid.nU0][nIInt-1][0][0];
    dU0_i_np1half=(grid.dLocalGridOld[grid.nU0][nIInt][0][0]
      +grid.dLocalGridOld[grid.nU0][nIInt-1][0][0])*0.5;
    
    for(j=grid.nStartGhostUpdateExplicit[grid.nEddyVisc][0][1];
      j<grid.nEndGhostUpdateExplicit[grid.nEddyVisc][0][1];j++){
      
      nJInt=j+grid.nCenIntOffset[1];
      
      for(k=grid.nStartGhostUpdateExplicit[grid.nEddyVisc][0][2];
        k<grid.nEndGhostUpdateExplicit[grid.nEddyVisc][0][2];k++){
        
        nKInt=k+grid.nCenIntOffset[2];
        dLengthScaleSq=dR_i_np1half_sq*dDelR_i_np1half*grid.dLocalGridOld[grid.nDTheta][0][j][0]
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]*grid.dLocalGridOld[grid.nDPhi][0][0][k];
        dLengthScaleSq=pow(dLengthScaleSq,0.666666666666666);
        
        //interpolate
        dU_ip1halfjk_np1half=grid.dLocalGridOld[grid.nU][nIInt][j][k];
        dU_im1halfjk_np1half=grid.dLocalGridOld[grid.nU][nIInt-1][j][k];
        dU_ijk_np1half=(grid.dLocalGridOld[grid.nU][nIInt][j][k]
          +grid.dLocalGridOld[grid.nU][nIInt-1][j][k])*0.5;
        dU_ijkp1half_np1half=(grid.dLocalGridOld[grid.nU][nIInt][j][k+1]
          +grid.dLocalGridOld[grid.nU][nIInt][j][k]+grid.dLocalGridOld[grid.nU][nIInt-1][j][k+1]
          +grid.dLocalGridOld[grid.nU][nIInt-1][j][k])*0.25;
        dU_ijkm1half_np1half=(grid.dLocalGridOld[grid.nU][nIInt][j][k]
          +grid.dLocalGridOld[grid.nU][nIInt][j][k-1]+grid.dLocalGridOld[grid.nU][nIInt-1][j][k]
          +grid.dLocalGridOld[grid.nU][nIInt-1][j][k-1])*0.25;
        dU_ijp1halfk_np1half=(grid.dLocalGridOld[grid.nU][nIInt][j][k]
          +grid.dLocalGridOld[grid.nU][nIInt-1][j][k]+grid.dLocalGridOld[grid.nU][nIInt][j+1][k]
          +grid.dLocalGridOld[grid.nU][nIInt-1][j+1][k])*0.25;
        dU_ijm1halfk_np1half=(grid.dLocalGridOld[grid.nU][nIInt][j][k]
          +grid.dLocalGridOld[grid.nU][nIInt-1][j][k]+grid.dLocalGridOld[grid.nU][nIInt][j-1][k]
          +grid.dLocalGridOld[grid.nU][nIInt-1][j-1][k])*0.25;
        dV_ijk_np1half=(grid.dLocalGridOld[grid.nV][i][nJInt][k]
          +grid.dLocalGridOld[grid.nV][i][nJInt-1][k])*0.5;
        dV_ip1halfjk_np1half=(grid.dLocalGridOld[grid.nV][i][nJInt][k]
          +grid.dLocalGridOld[grid.nV][i][nJInt-1][k])*0.5;/**\BC assuming that theta velocity
          is constant across surface*/
        dV_im1halfjk_np1half=(grid.dLocalGridOld[grid.nV][i][nJInt][k]
          +grid.dLocalGridOld[grid.nV][i][nJInt-1][k]+grid.dLocalGridOld[grid.nV][i-1][nJInt][k]
          +grid.dLocalGridOld[grid.nV][i-1][nJInt-1][k])*0.25;
        dV_ijp1halfk_np1half=grid.dLocalGridOld[grid.nV][i][nJInt][k];
        dV_ijm1halfk_np1half=grid.dLocalGridOld[grid.nV][i][nJInt-1][k];
        dV_ijkp1half_np1half=(grid.dLocalGridOld[grid.nV][i][nJInt][k+1]
          +grid.dLocalGridOld[grid.nV][i][nJInt][k]
          +grid.dLocalGridOld[grid.nV][i][nJInt-1][k+1]
          +grid.dLocalGridOld[grid.nV][i][nJInt-1][k])*0.25;
        dV_ijkm1half_np1half=(grid.dLocalGridOld[grid.nV][i][nJInt][k]
          +grid.dLocalGridOld[grid.nV][i][nJInt][k-1]
          +grid.dLocalGridOld[grid.nV][i][nJInt-1][k]
          +grid.dLocalGridOld[grid.nV][i][nJInt-1][k-1])*0.25;
        dW_ijk_np1half=(grid.dLocalGridOld[grid.nW][i][j][nKInt]
          +grid.dLocalGridOld[grid.nW][i][j][nKInt-1])*0.5;
        dW_ip1halfjk_np1half=(grid.dLocalGridOld[grid.nW][i][j][nKInt]
          +grid.dLocalGridOld[grid.nW][i][j][nKInt-1])*0.5;/**\BC assume phi velocity is constant
          across surface*/
        dW_im1halfjk_np1half=(grid.dLocalGridOld[grid.nW][i-1][j][nKInt]
          +grid.dLocalGridOld[grid.nW][i-1][j][nKInt-1]
          +grid.dLocalGridOld[grid.nW][i][j][nKInt]
          +grid.dLocalGridOld[grid.nW][i][j][nKInt-1])*0.25;
        dW_ijp1halfk_np1half=(grid.dLocalGridOld[grid.nW][i][j+1][nKInt]
          +grid.dLocalGridOld[grid.nW][i][j+1][nKInt-1]
          +grid.dLocalGridOld[grid.nW][i][j][nKInt]
          +grid.dLocalGridOld[grid.nW][i][j][nKInt-1])*0.25;
        dW_ijm1halfk_np1half=(grid.dLocalGridOld[grid.nW][i][j-1][nKInt]
          +grid.dLocalGridOld[grid.nW][i][j-1][nKInt-1]
          +grid.dLocalGridOld[grid.nW][i][j][nKInt]
          +grid.dLocalGridOld[grid.nW][i][j][nKInt-1])*0.25;
        dW_ijkp1half_np1half=grid.dLocalGridOld[grid.nW][i][j][nKInt];
        dW_ijkm1half_np1half=grid.dLocalGridOld[grid.nW][i][j][nKInt-1];
        
        //term 1
        d1=((dU_ip1halfjk_np1half-dU0_ip1half_np1half)-(dU_im1halfjk_np1half-dU0_im1half_np1half))
          /(dR_ip1half_np1half-dR_im1half_np1half);
        
        //term 2
        d2=1.0/dR_i_np1half*(dU_ijp1halfk_np1half-dU_ijm1halfk_np1half)
          /grid.dLocalGridOld[grid.nDTheta][0][j][0];
        
        //term 3
        d3=dV_ijk_np1half/dR_i_np1half;
        
        //term 4
        d4=(dU_ijkp1half_np1half-dU_ijkm1half_np1half)/(dR_i_np1half
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]*grid.dLocalGridOld[grid.nDPhi][0][0][k]);
        
        //term 5
        d5=(dW_ip1halfjk_np1half-dW_im1halfjk_np1half)/(dR_ip1half_np1half-dR_im1half_np1half);
        
        //term 6
        d6=dW_ijk_np1half/dR_i_np1half;
        
        //term 7
        d7=(dV_ip1halfjk_np1half-dV_im1halfjk_np1half)/dDelR_i_np1half;
        
        //term 8
        d8=1.0/dR_i_np1half*(dV_ijp1halfk_np1half-dV_ijm1halfk_np1half)
          /grid.dLocalGridOld[grid.nDTheta][0][j][0];
        
        //term 9
        d9=(dU_ijk_np1half-dU0_i_np1half)/dR_i_np1half;
        
        //term 10
        d10=(dW_ijp1halfk_np1half-dW_ijm1halfk_np1half)/(dR_i_np1half
          *grid.dLocalGridOld[grid.nDTheta][0][j][0]);
        
        //term 11
        d11=(dV_ijkp1half_np1half-dV_ijkm1half_np1half)/(dR_i_np1half
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]*grid.dLocalGridOld[grid.nDPhi][0][0][k]);
        
        //term 12
        d12=dW_ijk_np1half*grid.dLocalGridOld[grid.nCotThetaIJK][0][j][0]/dR_i_np1half;
        
        //term 13
        d13=(dW_ijkp1half_np1half-dW_ijkm1half_np1half)/(dR_i_np1half
          *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]*grid.dLocalGridOld[grid.nDPhi][0][0][k]);
        
        //term 14
        d14=dV_ijk_np1half*grid.dLocalGridOld[grid.nCotThetaIJK][0][j][0]/dR_i_np1half;
        
        //term A
        dA=2.0*d1*d1;
        
        //term B
        dB=(d2+d1-d3)*(d2-d3);
        
        //term C
        dC=(d4+d5-d6)*(d4-d6);
        
        //term D
        dD=d7*(d2+d7-d3);
        
        //term E
        dE=d8+d9;
        dE=2.0*dE*dE;
        
        //term dF
        dF=(d10+d11-d12)*(d11-d12);
        
        //term dG
        dG=d5*(d4+d5-d6);
        
        //term dH
        dH=d10*(d10+d11-d12);
        
        //term dI
        dI=d13+d14+d9;
        dI=2.0*dI*dI;
        
        dTerms=dA+dB+dC+dD+dE+dF+dG+dH+dI;
        grid.dLocalGridOld[grid.nEddyVisc][i][j][k]=dConstant*dLengthScaleSq
          *grid.dLocalGridOld[grid.nD][i][j][k]*pow(dTerms,0.5);
      }
    }
  }
}
void calDelt_R_GL(Grid &grid, Parameters &parameters, Time &time, ProcTop &procTop){
  
  int nShellWithSmallestDT=-1;
  int nEndCalc=std::max(grid.nEndGhostUpdateExplicit[grid.nD][0][0],grid.nEndUpdateExplicit[grid.nD][0]);
  int i;
  int j;
  int k;
  double dTemp=1e300;
  int nIInt;
  double dDelR;
  double dC;
  double dTTestR;
  double dTest_ConVelOverSoundSpeed_R;
  double dTest_ConVelOverSoundSpeed=0.0;
  double dVelToSetTimeStep;
  double dUmdU0_ijk_nm1half;
  double dDelRho_t_Rho_max;
  double dDelRho_t_Rho_max_test;
  double dDelRho_t_Rho_max_local=-1.0;
  double dDelE_t_E_max;
  double dDelE_t_E_max_test;
  double dDelE_t_E_max_local=-1.0;
  double dDelUmU0_t_UmU0_max=0.0;
  double dDelUmU0_t_UmU0_max_local=0.0;
  double dDelV_t_V_max=0.0;
  double dDelV_t_V_max_local=0.0;
  double dMaxChange;
  double dTest_ConVel_R;
  double dTest_ConVel=0.0;
  
  for(i=grid.nStartUpdateExplicit[grid.nD][0];i<nEndCalc;i++){
    
    //calculate i for interface centered quantities
    nIInt=i+grid.nCenIntOffset[0];
    dDelR=grid.dLocalGridNew[grid.nR][nIInt][0][0]-grid.dLocalGridNew[grid.nR][nIInt-1][0][0];
    
    for(j=grid.nStartUpdateExplicit[grid.nD][1];j<grid.nEndUpdateExplicit[grid.nD][1];j++){
      for(k=grid.nStartUpdateExplicit[grid.nD][2];k<grid.nEndUpdateExplicit[grid.nD][2];k++){
        
        dC=sqrt(parameters.dGamma*(grid.dLocalGridNew[grid.nP][i][j][k]
          +grid.dLocalGridNew[grid.nQ0][i][j][k])/grid.dLocalGridNew[grid.nD][i][j][k]);
        dUmdU0_ijk_nm1half=((grid.dLocalGridNew[grid.nU][nIInt][j][k]
          -grid.dLocalGridNew[grid.nU0][nIInt][0][0])+(grid.dLocalGridNew[grid.nU][nIInt-1][j][k]
          -grid.dLocalGridNew[grid.nU0][nIInt-1][0][0]))*0.5;
        dVelToSetTimeStep=sqrt(dC*dC+dUmdU0_ijk_nm1half*dUmdU0_ijk_nm1half);
        dTTestR=dDelR/dVelToSetTimeStep;
        dTest_ConVelOverSoundSpeed_R=fabs(grid.dLocalGridNew[grid.nU][nIInt][j][k]
          -grid.dLocalGridNew[grid.nU0][nIInt][0][0])/dC;
        dTest_ConVel_R=fabs(grid.dLocalGridNew[grid.nU][nIInt][j][k]
          -grid.dLocalGridNew[grid.nU0][nIInt][0][0]);
        
        //keep smallest time step
        if(dTTestR<dTemp){
          dTemp=dTTestR;
          nShellWithSmallestDT=i;
        }
        
        //keep largest convective velocity over sound speed
        if(dTest_ConVelOverSoundSpeed_R>dTest_ConVelOverSoundSpeed){
          dTest_ConVelOverSoundSpeed=dTest_ConVelOverSoundSpeed_R;
        }
        
        //keep largest convective velocity
        if(dTest_ConVel_R>dTest_ConVel){
          dTest_ConVel=dTest_ConVel_R;
        }
        
        //keep max change in rho
        dDelRho_t_Rho_max_test=fabs((grid.dLocalGridOld[grid.nD][i][j][k]
          -grid.dLocalGridNew[grid.nD][i][j][k])/grid.dLocalGridNew[grid.nD][i][j][k]);
        if(dDelRho_t_Rho_max_test>dDelRho_t_Rho_max_local){
          dDelRho_t_Rho_max_local=dDelRho_t_Rho_max_test;
        }
        
        //keep max change in E
        dDelE_t_E_max_test=fabs((grid.dLocalGridOld[grid.nE][i][j][k]
          -grid.dLocalGridNew[grid.nE][i][j][k])/grid.dLocalGridNew[grid.nE][i][j][k]);
        if(dDelE_t_E_max_test>dDelE_t_E_max_local){
          dDelE_t_E_max_local=dDelE_t_E_max_test;
        }
      }
    }
  }
  
  //use MPI::allreduce to send the smallest of all calculated time steps to all procs.
  double dTemp2;
  double dTest_ConVelOverSoundSpeed2;
  MPI::COMM_WORLD.Allreduce(&dTemp,&dTemp2,1,MPI::DOUBLE,MPI_MIN);
  if(dTemp<=0.0){//current processor found negative time step
    std::stringstream ssTemp;
    ssTemp<<__FILE__<<":"<<__FUNCTION__<<":"<<__LINE__<<":"<<procTop.nRank
      <<": smallest time step is negative in shell, "<<nShellWithSmallestDT<<std::endl;
    throw exception2(ssTemp.str(),INPUT);
  }
  if(dTemp2<=0.0){//some other processor found negative time step, should quit
    std::stringstream ssTemp;
    ssTemp.str("");
    throw exception2(ssTemp.str(),INPUT);
  }
  
  //find largest changes
  MPI::COMM_WORLD.Allreduce(&dDelRho_t_Rho_max_local,&dDelRho_t_Rho_max,1,MPI::DOUBLE,MPI_MAX);
  MPI::COMM_WORLD.Allreduce(&dDelE_t_E_max_local,&dDelE_t_E_max,1,MPI::DOUBLE,MPI_MAX);
  MPI::COMM_WORLD.Allreduce(&dDelUmU0_t_UmU0_max_local,&dDelUmU0_t_UmU0_max,1,MPI::DOUBLE,MPI_MAX);
  MPI::COMM_WORLD.Allreduce(&dDelV_t_V_max_local,&dDelV_t_V_max,1,MPI::DOUBLE,MPI_MAX);
  time.dDelRho_t_Rho_max=dDelRho_t_Rho_max;
  time.dDelE_t_E_max=dDelE_t_E_max;
  time.dDelUmU0_t_UmU0_max=dDelUmU0_t_UmU0_max;
  time.dDelV_t_V_max=dDelV_t_V_max;
  
  //pick largest change to limit time step
  dMaxChange=time.dDelRho_t_Rho_max;
  if(time.dDelE_t_E_max>dMaxChange){
    dMaxChange=time.dDelE_t_E_max;
  }
  if(time.dDelUmU0_t_UmU0_max>dMaxChange){
    dMaxChange=time.dDelUmU0_t_UmU0_max;
  }
  if(time.dDelV_t_V_max>dMaxChange){
    dMaxChange=time.dDelV_t_V_max;
  }
  if(time.dDelW_t_W_max>dMaxChange){
    dMaxChange=time.dDelW_t_W_max;
  }
  if(time.dPerChange/dMaxChange<1.0){
    dTemp2=time.dPerChange/dMaxChange*time.dDeltat_np1half;
  }
  else{
    dTemp2=dTemp2*time.dTimeStepFactor;//apply courant factor
  }
  
  if(dTemp2>time.dDeltat_np1half*1.02){//limit how fast the timestep can grow by 2%
    dTemp2=time.dDeltat_np1half*1.02;
  }
  
  //update time info
  time.dDeltat_nm1half=time.dDeltat_np1half;// time between t^n and t^{n+1}
  time.dDeltat_np1half=dTemp2;
  time.dDeltat_n=(time.dDeltat_np1half+time.dDeltat_nm1half)*0.5;//time between t^{n-1/2} and t^{n+1/2}
  time.dt+=time.dDeltat_np1half;
  time.nTimeStepIndex++;
  
  //keep largest convective velocity
  double dTest_ConVel2;
  MPI::COMM_WORLD.Allreduce(&dTest_ConVel,&dTest_ConVel2,1,MPI::DOUBLE,MPI_MAX);
  parameters.dMaxConvectiveVelocity=dTest_ConVel2;
  
  //set donnor fraction
  MPI::COMM_WORLD.Allreduce(&dTest_ConVelOverSoundSpeed,&dTest_ConVelOverSoundSpeed2,1
    ,MPI::DOUBLE,MPI_MAX);
  parameters.dMaxConvectiveVelocity_c=dTest_ConVelOverSoundSpeed2;
  if(dTest_ConVelOverSoundSpeed2>1.0){
    parameters.dDonorFrac=1.0;
  }
  else if(dTest_ConVelOverSoundSpeed2<0.1){
    parameters.dDonorFrac=0.1;
  }
  else{
    parameters.dDonorFrac=dTest_ConVelOverSoundSpeed2;
  }
}
void calDelt_R_TEOS(Grid &grid, Parameters &parameters, Time &time, ProcTop &procTop){
  
  int nShellWithSmallestDT=-1;
  int nEndCalc=std::max(grid.nEndGhostUpdateExplicit[grid.nD][0][0],grid.nEndUpdateExplicit[grid.nD][0]);
  int i;
  int j;
  int k;
  double dTemp=1e300;
  int nIInt;
  double dDelR;
  double dC;
  double dTTestR;
  double dTest_ConVelOverSoundSpeed_R;
  double dTest_ConVelOverSoundSpeed=0.0;
  double dVelToSetTimeStep;
  double dUmdU0_ijk_nm1half;
  double dDelRho_t_Rho_max;
  double dDelRho_t_Rho_max_test;
  double dDelRho_t_Rho_max_local=0.0;
  double dDelT_t_T_max;
  double dDelT_t_T_max_test;
  double dDelT_t_T_max_local=0.0;
  double dDelUmU0_t_UmU0_max=0.0;
  double dDelUmU0_t_UmU0_max_local=0.0;
  double dDelV_t_V_max=0.0;
  double dDelV_t_V_max_local=0.0;
  double dDelW_t_W_max;
  double dDelW_t_W_max_local=0.0;
  double dMaxChange;
  double dTest_ConVel_R;
  double dTest_ConVel=0.0;
  
  for(i=grid.nStartUpdateExplicit[grid.nD][0];i<nEndCalc;i++){
    
    //calculate i for interface centered quantities
    nIInt=i+grid.nCenIntOffset[0];
    dDelR=grid.dLocalGridNew[grid.nR][nIInt][0][0]-grid.dLocalGridNew[grid.nR][nIInt-1][0][0];
    
    for(j=grid.nStartUpdateExplicit[grid.nD][1];j<grid.nEndUpdateExplicit[grid.nD][1];j++){
      for(k=grid.nStartUpdateExplicit[grid.nD][2];k<grid.nEndUpdateExplicit[grid.nD][2];k++){
        
        dC=sqrt(grid.dLocalGridNew[grid.nGamma][i][j][k]*(grid.dLocalGridNew[grid.nP][i][j][k]
          +grid.dLocalGridNew[grid.nQ0][i][j][k])/grid.dLocalGridNew[grid.nD][i][j][k]);
        dUmdU0_ijk_nm1half=((grid.dLocalGridNew[grid.nU][nIInt][j][k]
          -grid.dLocalGridNew[grid.nU0][nIInt][0][0])+(grid.dLocalGridNew[grid.nU][nIInt-1][j][k]
          -grid.dLocalGridNew[grid.nU0][nIInt-1][0][0]))*0.5;
        dVelToSetTimeStep=sqrt(dC*dC+dUmdU0_ijk_nm1half*dUmdU0_ijk_nm1half);
        dTTestR=dDelR/dVelToSetTimeStep;
        dTest_ConVelOverSoundSpeed_R=fabs(grid.dLocalGridNew[grid.nU][nIInt][j][k]
          -grid.dLocalGridNew[grid.nU0][nIInt][0][0])/dC;
        dTest_ConVel_R=fabs(grid.dLocalGridNew[grid.nU][nIInt][j][k]
          -grid.dLocalGridNew[grid.nU0][nIInt][0][0]);
        
        //keep smallest time step
        if(dTTestR<dTemp){
          dTemp=dTTestR;
          nShellWithSmallestDT=i;
        }
        
        //keep largest convective velocity over sound speed
        if(dTest_ConVelOverSoundSpeed_R>dTest_ConVelOverSoundSpeed){
          dTest_ConVelOverSoundSpeed=dTest_ConVelOverSoundSpeed_R;
        }
        
        //keep largest convective velocity
        if(dTest_ConVel_R>dTest_ConVel){
          dTest_ConVel=dTest_ConVel_R;
        }
        
        //keep max change in rho
        dDelRho_t_Rho_max_test=fabs((grid.dLocalGridOld[grid.nD][i][j][k]
          -grid.dLocalGridNew[grid.nD][i][j][k])/grid.dLocalGridNew[grid.nD][i][j][k]);
        if(dDelRho_t_Rho_max_test>dDelRho_t_Rho_max_local){
          dDelRho_t_Rho_max_local=dDelRho_t_Rho_max_test;
        }
        
        //keep max change in T
        dDelT_t_T_max_test=fabs((grid.dLocalGridOld[grid.nT][i][j][k]
          -grid.dLocalGridNew[grid.nT][i][j][k])/grid.dLocalGridNew[grid.nT][i][j][k]);
        if(dDelT_t_T_max_test>dDelT_t_T_max_local){
          dDelT_t_T_max_local=dDelT_t_T_max_test;
        }
        
      }
    }
  }
  
  //use MPI::allreduce to send the smallest of all calculated time steps to all procs.
  double dTemp2;
  double dTest_ConVelOverSoundSpeed2;
  MPI::COMM_WORLD.Allreduce(&dTemp,&dTemp2,1,MPI::DOUBLE,MPI_MIN);
  if(dTemp<=0.0){//current processor found negative time step
    std::stringstream ssTemp;
    ssTemp<<__FILE__<<":"<<__FUNCTION__<<":"<<__LINE__<<":"<<procTop.nRank
      <<": smallest time step is negative in shell, "<<nShellWithSmallestDT<<std::endl;
    throw exception2(ssTemp.str(),INPUT);
  }
  if(dTemp2<=0.0){//some other processor found negative time step, should quit
    std::stringstream ssTemp;
    ssTemp.str("");
    throw exception2(ssTemp.str(),INPUT);
  }
  
  //find largest changes
  MPI::COMM_WORLD.Allreduce(&dDelRho_t_Rho_max_local,&dDelRho_t_Rho_max,1,MPI::DOUBLE,MPI_MAX);
  MPI::COMM_WORLD.Allreduce(&dDelT_t_T_max_local,&dDelT_t_T_max,1,MPI::DOUBLE,MPI_MAX);
  MPI::COMM_WORLD.Allreduce(&dDelUmU0_t_UmU0_max_local,&dDelUmU0_t_UmU0_max,1,MPI::DOUBLE,MPI_MAX);
  MPI::COMM_WORLD.Allreduce(&dDelV_t_V_max_local,&dDelV_t_V_max,1,MPI::DOUBLE,MPI_MAX);
  MPI::COMM_WORLD.Allreduce(&dDelW_t_W_max_local,&dDelW_t_W_max,1,MPI::DOUBLE,MPI_MAX);
  time.dDelRho_t_Rho_max=dDelRho_t_Rho_max;
  time.dDelT_t_T_max=dDelT_t_T_max;
  time.dDelUmU0_t_UmU0_max=dDelUmU0_t_UmU0_max;
  time.dDelV_t_V_max=dDelV_t_V_max;
  time.dDelW_t_W_max=dDelW_t_W_max;
  
  //pick largest change to limit time step
  dMaxChange=time.dDelRho_t_Rho_max;
  if(time.dDelT_t_T_max>dMaxChange){
    dMaxChange=time.dDelT_t_T_max;
  }
  if(time.dDelUmU0_t_UmU0_max>dMaxChange){
    dMaxChange=time.dDelUmU0_t_UmU0_max;
  }
  if(time.dDelV_t_V_max>dMaxChange){
    dMaxChange=time.dDelV_t_V_max;
  }
  if(time.dDelW_t_W_max>dMaxChange){
    dMaxChange=time.dDelW_t_W_max;
  }
  if(time.dPerChange/dMaxChange<1.0){
    dTemp2=time.dPerChange/dMaxChange*time.dDeltat_np1half;
  }
  else{
    dTemp2=dTemp2*time.dTimeStepFactor;//apply courant factor
  }
  
  if(dTemp2>time.dDeltat_np1half*1.02){//limit how fast the timestep can grow by 2%
    dTemp2=time.dDeltat_np1half*1.02;
  }
  
  //update time info
  time.dDeltat_nm1half=time.dDeltat_np1half;// time between t^n and t^{n+1}
  time.dDeltat_np1half=dTemp2;
  time.dDeltat_n=(time.dDeltat_np1half+time.dDeltat_nm1half)*0.5;//time between t^{n-1/2} and t^{n+1/2}
  time.dt+=time.dDeltat_np1half;
  time.nTimeStepIndex++;
  
  //keep largest convective velocity
  double dTest_ConVel2;
  MPI::COMM_WORLD.Allreduce(&dTest_ConVel,&dTest_ConVel2,1,MPI::DOUBLE,MPI_MAX);
  parameters.dMaxConvectiveVelocity=dTest_ConVel2;
  
  //set donnor fraction
  MPI::COMM_WORLD.Allreduce(&dTest_ConVelOverSoundSpeed,&dTest_ConVelOverSoundSpeed2,1
    ,MPI::DOUBLE,MPI_MAX);
  parameters.dMaxConvectiveVelocity_c=dTest_ConVelOverSoundSpeed2;
  if(dTest_ConVelOverSoundSpeed2>1.0){
    parameters.dDonorFrac=1.0;
  }
  else if(dTest_ConVelOverSoundSpeed2<0.1){
    parameters.dDonorFrac=0.1;
  }
  else{
    parameters.dDonorFrac=dTest_ConVelOverSoundSpeed2;
  }
}
void calDelt_RT_GL(Grid &grid, Parameters &parameters, Time &time, ProcTop &procTop){
  int nShellWithSmallestDT=-1;
  int nEndCalc=std::max(grid.nEndGhostUpdateExplicit[grid.nD][0][0],grid.nEndUpdateExplicit[grid.nD][0]);
  int i;
  int j;
  int k;
  double dTemp=1e300;
  int nIInt;
  int nJInt;
  double dDelR;
  double dRMid;
  double dC;
  double dTTestR;
  double dTTestTheta;
  double dVelToSetTimeStep;
  double dTest_ConVelOverSoundSpeed_R;
  double dTest_ConVelOverSoundSpeed_T;
  double dTest_ConVelOverSoundSpeed=0.0;
  double dUmdU0_ijk_nm1half;
  double dV_ijk_nm1half;
  double dDelRho_t_Rho_max;
  double dDelRho_t_Rho_max_test;
  double dDelRho_t_Rho_max_local=-1.0;
  double dDelE_t_E_max;
  double dDelE_t_E_max_test;
  double dDelE_t_E_max_local=-1.0;
  double dDelUmU0_t_UmU0_max;
  double dDelUmU0_t_UmU0_max_test;
  double dDelUmU0_t_UmU0_max_local=-1.0;
  double dDelV_t_V_max;
  double dDelV_t_V_max_test;
  double dDelV_t_V_max_local=-1.0;
  double dMaxChange;
  double dTest_ConVel_R;
  double dTest_ConVel_T;
  double dTest_ConVel=0.0;
  
  for(i=grid.nStartUpdateExplicit[grid.nD][0];i<nEndCalc;i++){
    
    //calculate i for interface centered quantities
    nIInt=i+grid.nCenIntOffset[0];
    dDelR=grid.dLocalGridNew[grid.nR][nIInt][0][0]
      -grid.dLocalGridNew[grid.nR][nIInt-1][0][0];
    dRMid=(grid.dLocalGridNew[grid.nR][nIInt][0][0]
      +grid.dLocalGridNew[grid.nR][nIInt-1][0][0])*0.5;
    for(j=grid.nStartUpdateExplicit[grid.nD][1];j<grid.nEndUpdateExplicit[grid.nD][1];j++){
      nJInt=j+grid.nCenIntOffset[1];
      for(k=grid.nStartUpdateExplicit[grid.nD][2];k<grid.nEndUpdateExplicit[grid.nD][2];k++){
        dC=sqrt(parameters.dGamma*(grid.dLocalGridNew[grid.nP][i][j][k]
          +grid.dLocalGridNew[grid.nQ0][i][j][k]+grid.dLocalGridNew[grid.nQ1][i][j][k])
          /grid.dLocalGridNew[grid.nD][i][j][k]);
        dUmdU0_ijk_nm1half=((grid.dLocalGridNew[grid.nU][nIInt][j][k]
          -grid.dLocalGridNew[grid.nU0][nIInt][0][0])+(grid.dLocalGridNew[grid.nU][nIInt-1][j][k]
          -grid.dLocalGridNew[grid.nU0][nIInt-1][0][0]))*0.5;
        dV_ijk_nm1half=(grid.dLocalGridNew[grid.nV][i][nJInt][k]
          +grid.dLocalGridNew[grid.nV][i][nJInt-1][k])*0.5;
        dVelToSetTimeStep=sqrt(dC*dC+dUmdU0_ijk_nm1half*dUmdU0_ijk_nm1half+dV_ijk_nm1half
          *dV_ijk_nm1half);
        
        dTTestR=dDelR/dVelToSetTimeStep;
        dTTestTheta=dRMid*grid.dLocalGridOld[grid.nDTheta][0][j][0]/dVelToSetTimeStep;
        dTest_ConVelOverSoundSpeed_R=fabs(grid.dLocalGridNew[grid.nU][nIInt][j][k]
          -grid.dLocalGridNew[grid.nU0][nIInt][0][0])/dC;
        dTest_ConVelOverSoundSpeed_T=fabs(grid.dLocalGridNew[grid.nV][i][nJInt][k])/dC;
        dTest_ConVel_R=fabs(grid.dLocalGridNew[grid.nU][nIInt][j][k]
          -grid.dLocalGridNew[grid.nU0][nIInt][0][0]);
        dTest_ConVel_T=fabs(grid.dLocalGridNew[grid.nV][i][nJInt][k]);
        
        //keep smallest time step
        if(dTTestR<dTemp){
          dTemp=dTTestR;
          nShellWithSmallestDT=i;
        }
        if(dTTestTheta<dTemp){
          dTemp=dTTestTheta;
          nShellWithSmallestDT=i;
        }
        
        //keep largest convective velocity over sound speed
        if(dTest_ConVelOverSoundSpeed_R>dTest_ConVelOverSoundSpeed){
          dTest_ConVelOverSoundSpeed=dTest_ConVelOverSoundSpeed_R;
        }
        if(dTest_ConVelOverSoundSpeed_T>dTest_ConVelOverSoundSpeed){
          dTest_ConVelOverSoundSpeed=dTest_ConVelOverSoundSpeed_T;
        }
        
        //keep largest convective velocity
        if(dTest_ConVel_R>dTest_ConVel){
          dTest_ConVel=dTest_ConVel_R;
        }
        if(dTest_ConVel_T>dTest_ConVel){
          dTest_ConVel=dTest_ConVel_T;
        }
         
        //keep max change in rho
        dDelRho_t_Rho_max_test=fabs((grid.dLocalGridOld[grid.nD][i][j][k]
          -grid.dLocalGridNew[grid.nD][i][j][k])/grid.dLocalGridNew[grid.nD][i][j][k]);
        if(dDelRho_t_Rho_max_test>dDelRho_t_Rho_max_local){
          dDelRho_t_Rho_max_local=dDelRho_t_Rho_max_test;
        }
        
        //keep max change in E
        dDelE_t_E_max_test=fabs((grid.dLocalGridOld[grid.nE][i][j][k]
          -grid.dLocalGridNew[grid.nE][i][j][k])/grid.dLocalGridNew[grid.nE][i][j][k]);
        if(dDelE_t_E_max_test>dDelE_t_E_max_local){
          dDelE_t_E_max_local=dDelE_t_E_max_test;
        }
        
        //keep max change in U-U0
        if(fabs(grid.dLocalGridNew[grid.nU][nIInt][j][k]
            -grid.dLocalGridNew[grid.nU0][nIInt][0][0])<1.0e4){
          dDelUmU0_t_UmU0_max_test=fabs( ((grid.dLocalGridOld[grid.nU][nIInt][j][k]
            -grid.dLocalGridOld[grid.nU0][nIInt][0][0])-(grid.dLocalGridNew[grid.nU][nIInt][j][k]
            -grid.dLocalGridNew[grid.nU0][nIInt][0][0]))/1.0e4);
        }
        else{
          dDelUmU0_t_UmU0_max_test=fabs( ((grid.dLocalGridOld[grid.nU][nIInt][j][k]
            -grid.dLocalGridOld[grid.nU0][nIInt][0][0])-(grid.dLocalGridNew[grid.nU][nIInt][j][k]
            -grid.dLocalGridNew[grid.nU0][nIInt][0][0]))/(grid.dLocalGridNew[grid.nU][nIInt][j][k]
            -grid.dLocalGridNew[grid.nU0][nIInt][0][0]));
        }
        if(dDelUmU0_t_UmU0_max_test>dDelUmU0_t_UmU0_max_local){
          dDelUmU0_t_UmU0_max_local=dDelUmU0_t_UmU0_max_test;
        }
        
        //keep max change in V
        if(fabs(grid.dLocalGridNew[grid.nV][i][nJInt][k])<1.0e4){
        dDelV_t_V_max_test=fabs((grid.dLocalGridOld[grid.nV][i][nJInt][k]
          -grid.dLocalGridNew[grid.nV][i][nJInt][k])/1.0e4);
        }
        else{
          dDelV_t_V_max_test=fabs((grid.dLocalGridOld[grid.nV][i][nJInt][k]
            -grid.dLocalGridNew[grid.nV][i][nJInt][k])/grid.dLocalGridNew[grid.nV][i][nJInt][k]);
        }
        if(dDelV_t_V_max_test>dDelV_t_V_max_local){
          dDelV_t_V_max_local=dDelV_t_V_max_test;
        }
      }
    }
  }
  
  //use MPI::allreduce to send the smallest of all calculated time steps to all procs.
  double dTemp2;
  double dTest_ConVelOverSoundSpeed2;
  MPI::COMM_WORLD.Allreduce(&dTemp,&dTemp2,1,MPI::DOUBLE,MPI_MIN);
  if(dTemp<=0.0){//current processor found negative time step
    std::stringstream ssTemp;
    ssTemp<<__FILE__<<":"<<__FUNCTION__<<":"<<__LINE__<<":"<<procTop.nRank
      <<": smallest time step is negative in shell, "<<nShellWithSmallestDT<<std::endl;
    throw exception2(ssTemp.str(),INPUT);
  }
  if(dTemp2<=0.0){//some other processor found negative time step, should quit
    std::stringstream ssTemp;
    ssTemp.str("");
    throw exception2(ssTemp.str(),INPUT);
  }
  
  //find largest changes
  MPI::COMM_WORLD.Allreduce(&dDelRho_t_Rho_max_local,&dDelRho_t_Rho_max,1,MPI::DOUBLE,MPI_MAX);
  MPI::COMM_WORLD.Allreduce(&dDelE_t_E_max_local,&dDelE_t_E_max,1,MPI::DOUBLE,MPI_MAX);
  MPI::COMM_WORLD.Allreduce(&dDelUmU0_t_UmU0_max_local,&dDelUmU0_t_UmU0_max,1,MPI::DOUBLE,MPI_MAX);
  MPI::COMM_WORLD.Allreduce(&dDelV_t_V_max_local,&dDelV_t_V_max,1,MPI::DOUBLE,MPI_MAX);
  time.dDelRho_t_Rho_max=dDelRho_t_Rho_max;
  time.dDelE_t_E_max=dDelE_t_E_max;
  time.dDelUmU0_t_UmU0_max=dDelUmU0_t_UmU0_max;
  time.dDelV_t_V_max=dDelV_t_V_max;
  
  //pick largest change to limit time step
  dMaxChange=time.dDelRho_t_Rho_max;
  if(time.dDelE_t_E_max>dMaxChange){
    dMaxChange=time.dDelE_t_E_max;
  }
  if(time.dDelUmU0_t_UmU0_max>dMaxChange){
    dMaxChange=time.dDelUmU0_t_UmU0_max;
  }
  if(time.dDelV_t_V_max>dMaxChange){
    dMaxChange=time.dDelV_t_V_max;
  }
  if(time.dDelW_t_W_max>dMaxChange){
    dMaxChange=time.dDelW_t_W_max;
  }
  if(time.dPerChange/dMaxChange<1.0){
    dTemp2=time.dPerChange/dMaxChange*time.dDeltat_np1half;
  }
  else{
    dTemp2=dTemp2*time.dTimeStepFactor;//apply courant factor
  }
  
  if(dTemp2>time.dDeltat_np1half*1.02){//limit how fast the timestep can grow by 2%
    dTemp2=time.dDeltat_np1half*1.02;
  }
  
  //update time info
  time.dDeltat_nm1half=time.dDeltat_np1half;// time between t^n and t^{n+1}
  time.dDeltat_np1half=dTemp2;
  time.dDeltat_n=(time.dDeltat_np1half+time.dDeltat_nm1half)*0.5;//time between t^{n-1/2} and t^{n+1/2}
  time.dt+=time.dDeltat_np1half;
  time.nTimeStepIndex++;
  
  //keep largest convective velocity
  double dTest_ConVel2;
  MPI::COMM_WORLD.Allreduce(&dTest_ConVel,&dTest_ConVel2,1,MPI::DOUBLE,MPI_MAX);
  parameters.dMaxConvectiveVelocity=dTest_ConVel2;
  
  //set donnor fraction
  MPI::COMM_WORLD.Allreduce(&dTest_ConVelOverSoundSpeed,&dTest_ConVelOverSoundSpeed2,1
    ,MPI::DOUBLE,MPI_MAX);
  parameters.dMaxConvectiveVelocity_c=dTest_ConVelOverSoundSpeed2;
  if(dTest_ConVelOverSoundSpeed2>1.0){
    parameters.dDonorFrac=1.0;
  }
  else if(dTest_ConVelOverSoundSpeed2<0.1){
    parameters.dDonorFrac=0.1;
  }
  else{
    parameters.dDonorFrac=dTest_ConVelOverSoundSpeed2;
  }
}
void calDelt_RT_TEOS(Grid &grid, Parameters &parameters, Time &time, ProcTop &procTop){
  int nShellWithSmallestDT=-1;
  int nEndCalc=std::max(grid.nEndGhostUpdateExplicit[grid.nD][0][0],grid.nEndUpdateExplicit[grid.nD][0]);
  int i;
  int j;
  int k;
  double dTemp=1e300;
  int nIInt;
  int nJInt;
  double dDelR;
  double dRMid;
  double dC;
  double dTTestR;
  double dTTestTheta;
  double dVelToSetTimeStep;
  double dTest_ConVelOverSoundSpeed_R;
  double dTest_ConVelOverSoundSpeed_T;
  double dTest_ConVelOverSoundSpeed=0.0;
  double dUmdU0_ijk_nm1half;
  double dV_ijk_nm1half;
  double dDelRho_t_Rho_max;
  double dDelRho_t_Rho_max_test;
  double dDelRho_t_Rho_max_local=-1.0;
  double dDelT_t_T_max;
  double dDelT_t_T_max_test;
  double dDelT_t_T_max_local=-1.0;
  double dDelUmU0_t_UmU0_max;
  double dDelUmU0_t_UmU0_max_test;
  double dDelUmU0_t_UmU0_max_local=-1.0;
  double dDelV_t_V_max;
  double dDelV_t_V_max_test;
  double dDelV_t_V_max_local=-1.0;
  double dDelW_t_W_max_local=-1.0;
  double dDelW_t_W_max;
  double dMaxChange;
  double dTest_ConVel_R;
  double dTest_ConVel_T;
  double dTest_ConVel=0.0;
  
  for(i=grid.nStartUpdateExplicit[grid.nD][0];i<nEndCalc;i++){
    
    //calculate i for interface centered quantities
    nIInt=i+grid.nCenIntOffset[0];
    dDelR=grid.dLocalGridNew[grid.nR][nIInt][0][0]
      -grid.dLocalGridNew[grid.nR][nIInt-1][0][0];
    dRMid=(grid.dLocalGridNew[grid.nR][nIInt][0][0]
      +grid.dLocalGridNew[grid.nR][nIInt-1][0][0])*0.5;
    for(j=grid.nStartUpdateExplicit[grid.nD][1];j<grid.nEndUpdateExplicit[grid.nD][1];j++){
      nJInt=j+grid.nCenIntOffset[1];
      for(k=grid.nStartUpdateExplicit[grid.nD][2];k<grid.nEndUpdateExplicit[grid.nD][2];k++){
        
        dC=sqrt(grid.dLocalGridNew[grid.nGamma][i][j][k]
          *(grid.dLocalGridNew[grid.nP][i][j][k]+grid.dLocalGridNew[grid.nQ0][i][j][k]
          +grid.dLocalGridNew[grid.nQ1][i][j][k])
          /grid.dLocalGridNew[grid.nD][i][j][k]);
        dUmdU0_ijk_nm1half=((grid.dLocalGridNew[grid.nU][nIInt][j][k]
          -grid.dLocalGridNew[grid.nU0][nIInt][0][0])+(grid.dLocalGridNew[grid.nU][nIInt-1][j][k]
          -grid.dLocalGridNew[grid.nU0][nIInt-1][0][0]))*0.5;
        dV_ijk_nm1half=(grid.dLocalGridNew[grid.nV][i][nJInt][k]
          +grid.dLocalGridNew[grid.nV][i][nJInt-1][k])*0.5;
        dVelToSetTimeStep=sqrt(dC*dC+dUmdU0_ijk_nm1half*dUmdU0_ijk_nm1half+dV_ijk_nm1half
          *dV_ijk_nm1half);
        
        dTTestR=dDelR/dVelToSetTimeStep;
        dTTestTheta=dRMid*grid.dLocalGridOld[grid.nDTheta][0][j][0]/dVelToSetTimeStep;
        dTest_ConVelOverSoundSpeed_R=fabs(grid.dLocalGridNew[grid.nU][nIInt][j][k]
          -grid.dLocalGridNew[grid.nU0][nIInt][0][0])/dC;
        dTest_ConVelOverSoundSpeed_T=fabs(grid.dLocalGridNew[grid.nV][i][nJInt][k])/dC;
        dTest_ConVel_R=fabs(grid.dLocalGridNew[grid.nU][nIInt][j][k]
          -grid.dLocalGridNew[grid.nU0][nIInt][0][0]);
        dTest_ConVel_T=fabs(grid.dLocalGridNew[grid.nV][i][nJInt][k]);
        
        //keep smallest time step
        if(dTTestR<dTemp){
          dTemp=dTTestR;
          nShellWithSmallestDT=i;
        }
        if(dTTestTheta<dTemp){
          dTemp=dTTestTheta;
          nShellWithSmallestDT=i;
        }
        
        //keep largest convective velocity over sound speed
        if(dTest_ConVelOverSoundSpeed_R>dTest_ConVelOverSoundSpeed){
          dTest_ConVelOverSoundSpeed=dTest_ConVelOverSoundSpeed_R;
        }
        if(dTest_ConVelOverSoundSpeed_T>dTest_ConVelOverSoundSpeed){
          dTest_ConVelOverSoundSpeed=dTest_ConVelOverSoundSpeed_T;
        }
        
        //keep largest convective velocity
        if(dTest_ConVel_R>dTest_ConVel){
          dTest_ConVel=dTest_ConVel_R;
        }
        if(dTest_ConVel_T>dTest_ConVel){
          dTest_ConVel=dTest_ConVel_T;
        }
        
        //keep max change in rho
        dDelRho_t_Rho_max_test=fabs((grid.dLocalGridOld[grid.nD][i][j][k]
          -grid.dLocalGridNew[grid.nD][i][j][k])/grid.dLocalGridNew[grid.nD][i][j][k]);
        if(dDelRho_t_Rho_max_test>dDelRho_t_Rho_max_local){
          dDelRho_t_Rho_max_local=dDelRho_t_Rho_max_test;
        }
        
        //keep max change in T
        dDelT_t_T_max_test=fabs((grid.dLocalGridOld[grid.nT][i][j][k]
          -grid.dLocalGridNew[grid.nT][i][j][k])/grid.dLocalGridNew[grid.nT][i][j][k]);
        if(dDelT_t_T_max_test>dDelT_t_T_max_local){
          dDelT_t_T_max_local=dDelT_t_T_max_test;
        }
        
        //keep max change in U-U0
        if(fabs(grid.dLocalGridNew[grid.nU][nIInt][j][k]
            -grid.dLocalGridNew[grid.nU0][nIInt][0][0])<1.0e4){
          dDelUmU0_t_UmU0_max_test=fabs( ((grid.dLocalGridOld[grid.nU][nIInt][j][k]
            -grid.dLocalGridOld[grid.nU0][nIInt][0][0])-(grid.dLocalGridNew[grid.nU][nIInt][j][k]
            -grid.dLocalGridNew[grid.nU0][nIInt][0][0]))/1.0e4);
        }
        else{
          dDelUmU0_t_UmU0_max_test=fabs( ((grid.dLocalGridOld[grid.nU][nIInt][j][k]
            -grid.dLocalGridOld[grid.nU0][nIInt][0][0])-(grid.dLocalGridNew[grid.nU][nIInt][j][k]
            -grid.dLocalGridNew[grid.nU0][nIInt][0][0]))/(grid.dLocalGridNew[grid.nU][nIInt][j][k]
            -grid.dLocalGridNew[grid.nU0][nIInt][0][0]));
        }
        if(dDelUmU0_t_UmU0_max_test>dDelUmU0_t_UmU0_max_local){
          dDelUmU0_t_UmU0_max_local=dDelUmU0_t_UmU0_max_test;
        }
        
        //keep max change in V
        if(fabs(grid.dLocalGridNew[grid.nV][i][nJInt][k])<1.0e4){
        dDelV_t_V_max_test=fabs((grid.dLocalGridOld[grid.nV][i][nJInt][k]
          -grid.dLocalGridNew[grid.nV][i][nJInt][k])/1.0e4);
        }
        else{
          dDelV_t_V_max_test=fabs((grid.dLocalGridOld[grid.nV][i][nJInt][k]
            -grid.dLocalGridNew[grid.nV][i][nJInt][k])/grid.dLocalGridNew[grid.nV][i][nJInt][k]);
        }
        if(dDelV_t_V_max_test>dDelV_t_V_max_local){
          dDelV_t_V_max_local=dDelV_t_V_max_test;
        }
      }
    }
  }
  
  //use MPI::allreduce to send the smallest of all calculated time steps to all procs.
  double dTemp2;
  double dTest_ConVelOverSoundSpeed2;
  MPI::COMM_WORLD.Allreduce(&dTemp,&dTemp2,1,MPI::DOUBLE,MPI_MIN);
  if(dTemp<=0.0){//current processor found negative time step
    std::stringstream ssTemp;
    ssTemp<<__FILE__<<":"<<__FUNCTION__<<":"<<__LINE__<<":"<<procTop.nRank
      <<": smallest time step is negative in shell, "<<nShellWithSmallestDT<<std::endl;
    throw exception2(ssTemp.str(),INPUT);
  }
  if(dTemp2<=0.0){//some other processor found negative time step, should quit
    std::stringstream ssTemp;
    ssTemp.str("");
    throw exception2(ssTemp.str(),INPUT);
  }
  
  //find largest changes
  MPI::COMM_WORLD.Allreduce(&dDelRho_t_Rho_max_local,&dDelRho_t_Rho_max,1,MPI::DOUBLE,MPI_MAX);
  MPI::COMM_WORLD.Allreduce(&dDelT_t_T_max_local,&dDelT_t_T_max,1,MPI::DOUBLE,MPI_MAX);
  MPI::COMM_WORLD.Allreduce(&dDelUmU0_t_UmU0_max_local,&dDelUmU0_t_UmU0_max,1,MPI::DOUBLE,MPI_MAX);
  MPI::COMM_WORLD.Allreduce(&dDelV_t_V_max_local,&dDelV_t_V_max,1,MPI::DOUBLE,MPI_MAX);
  MPI::COMM_WORLD.Allreduce(&dDelW_t_W_max_local,&dDelW_t_W_max,1,MPI::DOUBLE,MPI_MAX);
  time.dDelRho_t_Rho_max=dDelRho_t_Rho_max;
  time.dDelT_t_T_max=dDelT_t_T_max;
  time.dDelUmU0_t_UmU0_max=dDelUmU0_t_UmU0_max;
  time.dDelV_t_V_max=dDelV_t_V_max;
  time.dDelW_t_W_max=dDelW_t_W_max;
  
  //pick largest change to limit time step
  dMaxChange=time.dDelRho_t_Rho_max;
  if(time.dDelT_t_T_max>dMaxChange){
    dMaxChange=time.dDelT_t_T_max;
  }
  if(time.dDelUmU0_t_UmU0_max>dMaxChange){
    dMaxChange=time.dDelUmU0_t_UmU0_max;
  }
  if(time.dDelV_t_V_max>dMaxChange){
    dMaxChange=time.dDelV_t_V_max;
  }
  if(time.dDelW_t_W_max>dMaxChange){
    dMaxChange=time.dDelW_t_W_max;
  }
  if(time.dPerChange/dMaxChange<1.0){
    dTemp2=time.dPerChange/dMaxChange*time.dDeltat_np1half;
  }
  else{
    dTemp2=dTemp2*time.dTimeStepFactor;//apply courant factor
  }
  
  if(dTemp2>time.dDeltat_np1half*1.02){//limit how fast the timestep can grow by 2%
    dTemp2=time.dDeltat_np1half*1.02;
  }
  
  //update time info
  time.dDeltat_nm1half=time.dDeltat_np1half;// time between t^n and t^{n+1}
  time.dDeltat_np1half=dTemp2;
  time.dDeltat_n=(time.dDeltat_np1half+time.dDeltat_nm1half)*0.5;//time between t^{n-1/2} and t^{n+1/2}
  time.dt+=time.dDeltat_np1half;
  time.nTimeStepIndex++;
  
  //keep largest convective velocity
  double dTest_ConVel2;
  MPI::COMM_WORLD.Allreduce(&dTest_ConVel,&dTest_ConVel2,1,MPI::DOUBLE,MPI_MAX);
  parameters.dMaxConvectiveVelocity=dTest_ConVel2;
  
  //set donnor fraction
  MPI::COMM_WORLD.Allreduce(&dTest_ConVelOverSoundSpeed,&dTest_ConVelOverSoundSpeed2,1
    ,MPI::DOUBLE,MPI_MAX);
  parameters.dMaxConvectiveVelocity_c=dTest_ConVelOverSoundSpeed2;
  if(dTest_ConVelOverSoundSpeed2>1.0){
    parameters.dDonorFrac=1.0;
  }
  else if(dTest_ConVelOverSoundSpeed2<0.1){
    parameters.dDonorFrac=0.1;
  }
  else{
    parameters.dDonorFrac=dTest_ConVelOverSoundSpeed2;
  }
}
void calDelt_RTP_GL(Grid &grid, Parameters &parameters, Time &time, ProcTop &procTop){
  int nShellWithSmallestDT=-1;
  int nEndCalc=std::max(grid.nEndGhostUpdateExplicit[grid.nD][0][0],grid.nEndUpdateExplicit[grid.nD][0]);
  int i;
  int j;
  int k;
  double dTemp=1e300;
  int nIInt;
  int nJInt;
  int nKInt;
  double dDelR;
  double dRMid;
  double dC;
  double dTTestR;
  double dTTestTheta;
  double dTTestPhi;
  double dVelToSetTimeStep;
  double dTest_ConVelOverSoundSpeed_R;
  double dTest_ConVelOverSoundSpeed_T;
  double dTest_ConVelOverSoundSpeed_P;
  double dTest_ConVelOverSoundSpeed=0.0;
  double dUmdU0_ijk_nm1half;
  double dV_ijk_nm1half;
  double dW_ijk_nm1half;
  double dDelRho_t_Rho_max;
  double dDelRho_t_Rho_max_test;
  double dDelRho_t_Rho_max_local=-1.0;
  double dDelE_t_E_max;
  double dDelE_t_E_max_test;
  double dDelE_t_E_max_local=-1.0;
  double dDelUmU0_t_UmU0_max;
  double dDelUmU0_t_UmU0_max_test;
  double dDelUmU0_t_UmU0_max_local=-1.0;
  double dDelV_t_V_max;
  double dDelV_t_V_max_test;
  double dDelV_t_V_max_local=-1.0;
  double dDelW_t_W_max;
  double dDelW_t_W_max_test;
  double dDelW_t_W_max_local=-1.0;
  double dMaxChange;
  double dTest_ConVel_R;
  double dTest_ConVel_T;
  double dTest_ConVel_P;
  double dTest_ConVel=0.0;
  
  for(i=grid.nStartUpdateExplicit[grid.nD][0];i<nEndCalc;i++){
    
    //calculate i for interface centered quantities
    nIInt=i+grid.nCenIntOffset[0];
    dDelR=grid.dLocalGridNew[grid.nR][nIInt][0][0]
      -grid.dLocalGridNew[grid.nR][nIInt-1][0][0];
    dRMid=(grid.dLocalGridNew[grid.nR][nIInt][0][0]
      +grid.dLocalGridNew[grid.nR][nIInt-1][0][0])*0.5;
    for(j=grid.nStartUpdateExplicit[grid.nD][1];j<grid.nEndUpdateExplicit[grid.nD][1];j++){
      nJInt=j+grid.nCenIntOffset[1];
      for(k=grid.nStartUpdateExplicit[grid.nD][2];k<grid.nEndUpdateExplicit[grid.nD][2];k++){
        nKInt=k+grid.nCenIntOffset[2];
        dC=sqrt(parameters.dGamma
          *(grid.dLocalGridNew[grid.nP][i][j][k]+grid.dLocalGridNew[grid.nQ0][i][j][k]
          +grid.dLocalGridNew[grid.nQ1][i][j][k]+grid.dLocalGridNew[grid.nQ2][i][j][k])
          /grid.dLocalGridNew[grid.nD][i][j][k]);
        dUmdU0_ijk_nm1half=((grid.dLocalGridNew[grid.nU][nIInt][j][k]
          -grid.dLocalGridNew[grid.nU0][nIInt][0][0])+(grid.dLocalGridNew[grid.nU][nIInt-1][j][k]
          -grid.dLocalGridNew[grid.nU0][nIInt-1][0][0]))*0.5;
        dV_ijk_nm1half=(grid.dLocalGridNew[grid.nV][i][nJInt][k]
          +grid.dLocalGridNew[grid.nV][i][nJInt-1][k])*0.5;
        dW_ijk_nm1half=(grid.dLocalGridNew[grid.nW][i][j][nKInt]
          +grid.dLocalGridNew[grid.nW][i][j][nKInt-1])*0.5;
        dVelToSetTimeStep=sqrt(dC*dC+dUmdU0_ijk_nm1half*dUmdU0_ijk_nm1half+dV_ijk_nm1half
          *dV_ijk_nm1half+dW_ijk_nm1half*dW_ijk_nm1half);
        
        dTTestR=dDelR/dVelToSetTimeStep;
        dTTestTheta=dRMid*grid.dLocalGridOld[grid.nDTheta][0][j][0]/dVelToSetTimeStep;
        dTTestPhi=dRMid*grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]
          *grid.dLocalGridOld[grid.nDPhi][0][0][k]/dVelToSetTimeStep;
        dTest_ConVelOverSoundSpeed_R=fabs(grid.dLocalGridNew[grid.nU][nIInt][j][k]
          -grid.dLocalGridNew[grid.nU0][nIInt][0][0])/dC;
        dTest_ConVelOverSoundSpeed_T=fabs(grid.dLocalGridNew[grid.nV][i][nJInt][k])/dC;
        dTest_ConVelOverSoundSpeed_P=fabs(grid.dLocalGridNew[grid.nW][i][j][nKInt])/dC;
        dTest_ConVel_R=fabs(grid.dLocalGridNew[grid.nU][nIInt][j][k]
          -grid.dLocalGridNew[grid.nU0][nIInt][0][0]);
        dTest_ConVel_T=fabs(grid.dLocalGridNew[grid.nV][i][nJInt][k]);
        dTest_ConVel_P=fabs(grid.dLocalGridNew[grid.nW][i][j][nKInt]);
        
        //keep smallest time step
        if(dTTestR<dTemp){
          dTemp=dTTestR;
          nShellWithSmallestDT=i;
        }
        if(dTTestTheta<dTemp){
          dTemp=dTTestTheta;
          nShellWithSmallestDT=i;
        }
        if(dTTestPhi<dTemp){
          dTemp=dTTestPhi;
          nShellWithSmallestDT=i;
        }
        
        //keep largest convective velocity over sound speed
        if(dTest_ConVelOverSoundSpeed_R>dTest_ConVelOverSoundSpeed){
          dTest_ConVelOverSoundSpeed=dTest_ConVelOverSoundSpeed_R;
        }
        if(dTest_ConVelOverSoundSpeed_T>dTest_ConVelOverSoundSpeed){
          dTest_ConVelOverSoundSpeed=dTest_ConVelOverSoundSpeed_T;
        }
        if(dTest_ConVelOverSoundSpeed_P>dTest_ConVelOverSoundSpeed){
          dTest_ConVelOverSoundSpeed=dTest_ConVelOverSoundSpeed_P;
        }
        
        //keep largest convective velocity
        if(dTest_ConVel_R>dTest_ConVel){
          dTest_ConVel=dTest_ConVel_R;
        }
        if(dTest_ConVel_T>dTest_ConVel){
          dTest_ConVel=dTest_ConVel_T;
        }
        if(dTest_ConVel_P>dTest_ConVel){
          dTest_ConVel=dTest_ConVel_P;
        }
         
        //keep max change in rho
        dDelRho_t_Rho_max_test=fabs((grid.dLocalGridOld[grid.nD][i][j][k]
          -grid.dLocalGridNew[grid.nD][i][j][k])/grid.dLocalGridNew[grid.nD][i][j][k]);
        if(dDelRho_t_Rho_max_test>dDelRho_t_Rho_max_local){
          dDelRho_t_Rho_max_local=dDelRho_t_Rho_max_test;
        }
        
        //keep max change in T
        dDelE_t_E_max_test=fabs((grid.dLocalGridOld[grid.nE][i][j][k]
          -grid.dLocalGridNew[grid.nE][i][j][k])/grid.dLocalGridNew[grid.nE][i][j][k]);
        if(dDelE_t_E_max_test>dDelE_t_E_max_local){
          dDelE_t_E_max_local=dDelE_t_E_max_test;
        }
        
        //keep max change in U-U0
        if(fabs(grid.dLocalGridNew[grid.nU][nIInt][j][k]
            -grid.dLocalGridNew[grid.nU0][nIInt][0][0])<1.0e4){
          dDelUmU0_t_UmU0_max_test=fabs( ((grid.dLocalGridOld[grid.nU][nIInt][j][k]
            -grid.dLocalGridOld[grid.nU0][nIInt][0][0])-(grid.dLocalGridNew[grid.nU][nIInt][j][k]
            -grid.dLocalGridNew[grid.nU0][nIInt][0][0]))/1.0e4);
        }
        else{
          dDelUmU0_t_UmU0_max_test=fabs( ((grid.dLocalGridOld[grid.nU][nIInt][j][k]
            -grid.dLocalGridOld[grid.nU0][nIInt][0][0])-(grid.dLocalGridNew[grid.nU][nIInt][j][k]
            -grid.dLocalGridNew[grid.nU0][nIInt][0][0]))/(grid.dLocalGridNew[grid.nU][nIInt][j][k]
            -grid.dLocalGridNew[grid.nU0][nIInt][0][0]));
        }
        if(dDelUmU0_t_UmU0_max_test>dDelUmU0_t_UmU0_max_local){
          dDelUmU0_t_UmU0_max_local=dDelUmU0_t_UmU0_max_test;
        }
        
        //keep max change in V
        if(fabs(grid.dLocalGridNew[grid.nV][i][nJInt][k])<1.0e4){/* prevents really large value near
          0 which isn't desirable */
          dDelV_t_V_max_test=fabs((grid.dLocalGridOld[grid.nV][i][nJInt][k]
            -grid.dLocalGridNew[grid.nV][i][nJInt][k])/1.0e4);
        }
        else{
          dDelV_t_V_max_test=fabs((grid.dLocalGridOld[grid.nV][i][nJInt][k]
            -grid.dLocalGridNew[grid.nV][i][nJInt][k])/grid.dLocalGridNew[grid.nV][i][nJInt][k]);
        }
        if(dDelV_t_V_max_test>dDelV_t_V_max_local){/* select largest value */
          dDelV_t_V_max_local=dDelV_t_V_max_test;
        }
        
        //keep max change in W
        if(fabs(grid.dLocalGridNew[grid.nW][i][j][nKInt])<1.0e4){/* prevents really large value near
          0 which isn't desirable */
          dDelW_t_W_max_test=fabs((grid.dLocalGridOld[grid.nW][i][j][nKInt]
            -grid.dLocalGridNew[grid.nW][i][j][nKInt])/1.0e4);
        }
        else{
          dDelW_t_W_max_test=fabs((grid.dLocalGridOld[grid.nW][i][j][nKInt]
            -grid.dLocalGridNew[grid.nW][i][j][nKInt])/grid.dLocalGridNew[grid.nW][i][j][nKInt]);
        }
        if(dDelW_t_W_max_test>dDelW_t_W_max_local){/* select largest value */
          dDelW_t_W_max_local=dDelW_t_W_max_test;
        }
      }
    }
  }
  
  //use MPI::allreduce to send the smallest of all calculated time steps to all procs.
  double dTemp2;
  double dTest_ConVelOverSoundSpeed2;
  MPI::COMM_WORLD.Allreduce(&dTemp,&dTemp2,1,MPI::DOUBLE,MPI_MIN);
  if(dTemp<=0.0){//current processor found negative time step
    std::stringstream ssTemp;
    ssTemp<<__FILE__<<":"<<__FUNCTION__<<":"<<__LINE__<<":"<<procTop.nRank
      <<": smallest time step is negative in shell, "<<nShellWithSmallestDT<<std::endl;
    throw exception2(ssTemp.str(),INPUT);
  }
  if(dTemp2<=0.0){//some other processor found negative time step, should quit
    std::stringstream ssTemp;
    ssTemp.str("");
    throw exception2(ssTemp.str(),INPUT);
  }
  
  //find largest changes
  MPI::COMM_WORLD.Allreduce(&dDelRho_t_Rho_max_local,&dDelRho_t_Rho_max,1,MPI::DOUBLE,MPI_MAX);
  MPI::COMM_WORLD.Allreduce(&dDelE_t_E_max_local,&dDelE_t_E_max,1,MPI::DOUBLE,MPI_MAX);
  MPI::COMM_WORLD.Allreduce(&dDelUmU0_t_UmU0_max_local,&dDelUmU0_t_UmU0_max,1,MPI::DOUBLE,MPI_MAX);
  MPI::COMM_WORLD.Allreduce(&dDelV_t_V_max_local,&dDelV_t_V_max,1,MPI::DOUBLE,MPI_MAX);
  MPI::COMM_WORLD.Allreduce(&dDelW_t_W_max_local,&dDelW_t_W_max,1,MPI::DOUBLE,MPI_MAX);
  time.dDelRho_t_Rho_max=dDelRho_t_Rho_max;
  time.dDelE_t_E_max=dDelE_t_E_max;
  time.dDelUmU0_t_UmU0_max=dDelUmU0_t_UmU0_max;
  time.dDelV_t_V_max=dDelV_t_V_max;
  time.dDelW_t_W_max=dDelW_t_W_max;
  
  //pick largest change to limit time step
  dMaxChange=time.dDelRho_t_Rho_max;
  if(time.dDelE_t_E_max>dMaxChange){
    dMaxChange=time.dDelE_t_E_max;
  }
  if(time.dDelUmU0_t_UmU0_max>dMaxChange){
    dMaxChange=time.dDelUmU0_t_UmU0_max;
  }
  if(time.dDelV_t_V_max>dMaxChange){
    dMaxChange=time.dDelV_t_V_max;
  }
  if(time.dDelW_t_W_max>dMaxChange){
    dMaxChange=time.dDelW_t_W_max;
  }
  if(time.dPerChange/dMaxChange<1.0){
    dTemp2=time.dPerChange/dMaxChange*time.dDeltat_np1half;
  }
  else{
    dTemp2=dTemp2*time.dTimeStepFactor;//apply courant factor
  }
  
  if(dTemp2>time.dDeltat_np1half*1.02){//limit how fast the timestep can grow by 2%
    dTemp2=time.dDeltat_np1half*1.02;
  }
  
  //update time info
  time.dDeltat_nm1half=time.dDeltat_np1half;// time between t^n and t^{n+1}
  time.dDeltat_np1half=dTemp2;
  time.dDeltat_n=(time.dDeltat_np1half+time.dDeltat_nm1half)*0.5;//time between t^{n-1/2} and t^{n+1/2}
  time.dt+=time.dDeltat_np1half;
  time.nTimeStepIndex++;
  
  //keep largest convective velocity
  double dTest_ConVel2;
  MPI::COMM_WORLD.Allreduce(&dTest_ConVel,&dTest_ConVel2,1,MPI::DOUBLE,MPI_MAX);
  parameters.dMaxConvectiveVelocity=dTest_ConVel2;
  
  //set donnor fraction
  MPI::COMM_WORLD.Allreduce(&dTest_ConVelOverSoundSpeed,&dTest_ConVelOverSoundSpeed2,1
    ,MPI::DOUBLE,MPI_MAX);
  parameters.dMaxConvectiveVelocity_c=dTest_ConVelOverSoundSpeed2;
  if(dTest_ConVelOverSoundSpeed2>1.0){
    parameters.dDonorFrac=1.0;
  }
  else if(dTest_ConVelOverSoundSpeed2<0.1){
    parameters.dDonorFrac=0.1;
  }
  else{
    parameters.dDonorFrac=dTest_ConVelOverSoundSpeed2;
  }
}
void calDelt_RTP_TEOS(Grid &grid, Parameters &parameters, Time &time, ProcTop &procTop){
  int nShellWithSmallestDT=-1;
  int nEndCalc=std::max(grid.nEndGhostUpdateExplicit[grid.nD][0][0],grid.nEndUpdateExplicit[grid.nD][0]);
  int i;
  int j;
  int k;
  double dTemp=1e300;
  int nIInt;
  int nJInt;
  int nKInt;
  double dDelR;
  double dRMid;
  double dC;
  double dTTestR;
  double dTTestTheta;
  double dTTestPhi;
  double dVelToSetTimeStep;
  double dTest_ConVelOverSoundSpeed_R;
  double dTest_ConVelOverSoundSpeed_T;
  double dTest_ConVelOverSoundSpeed_P;
  double dTest_ConVelOverSoundSpeed=0.0;
  double dUmdU0_ijk_nm1half;
  double dV_ijk_nm1half;
  double dW_ijk_nm1half;
  double dDelRho_t_Rho_max;
  double dDelRho_t_Rho_max_test;
  double dDelRho_t_Rho_max_local=0.0;
  double dDelT_t_T_max;
  double dDelT_t_T_max_test;
  double dDelT_t_T_max_local=0.0;
  double dDelUmU0_t_UmU0_max;
  double dDelUmU0_t_UmU0_max_test;
  double dDelUmU0_t_UmU0_max_local=0.0;
  double dDelV_t_V_max;
  double dDelV_t_V_max_test;
  double dDelV_t_V_max_local=0.0;
  double dDelW_t_W_max;
  double dDelW_t_W_max_test;
  double dDelW_t_W_max_local=0.0;
  double dMaxChange;
  double dTest_ConVel_R;
  double dTest_ConVel_T;
  double dTest_ConVel_P;
  double dTest_ConVel=0.0;
  
  for(i=grid.nStartUpdateExplicit[grid.nD][0];i<nEndCalc;i++){
    
    //calculate i for interface centered quantities
    nIInt=i+grid.nCenIntOffset[0];
    dDelR=grid.dLocalGridNew[grid.nR][nIInt][0][0]
      -grid.dLocalGridNew[grid.nR][nIInt-1][0][0];
    dRMid=(grid.dLocalGridNew[grid.nR][nIInt][0][0]
      +grid.dLocalGridNew[grid.nR][nIInt-1][0][0])*0.5;
    for(j=grid.nStartUpdateExplicit[grid.nD][1];j<grid.nEndUpdateExplicit[grid.nD][1];j++){
      nJInt=j+grid.nCenIntOffset[1];
      for(k=grid.nStartUpdateExplicit[grid.nD][2];k<grid.nEndUpdateExplicit[grid.nD][2];k++){
        nKInt=k+grid.nCenIntOffset[2];
        dC=sqrt(grid.dLocalGridNew[grid.nGamma][i][j][k]
          *(grid.dLocalGridNew[grid.nP][i][j][k]+grid.dLocalGridNew[grid.nQ0][i][j][k]
          +grid.dLocalGridNew[grid.nQ1][i][j][k]+grid.dLocalGridNew[grid.nQ2][i][j][k])
          /grid.dLocalGridNew[grid.nD][i][j][k]);
        dUmdU0_ijk_nm1half=((grid.dLocalGridNew[grid.nU][nIInt][j][k]
          -grid.dLocalGridNew[grid.nU0][nIInt][0][0])+(grid.dLocalGridNew[grid.nU][nIInt-1][j][k]
          -grid.dLocalGridNew[grid.nU0][nIInt-1][0][0]))*0.5;
        dV_ijk_nm1half=(grid.dLocalGridNew[grid.nV][i][nJInt][k]
          +grid.dLocalGridNew[grid.nV][i][nJInt-1][k])*0.5;
        dW_ijk_nm1half=(grid.dLocalGridNew[grid.nW][i][j][nKInt]
          +grid.dLocalGridNew[grid.nW][i][j][nKInt-1])*0.5;
        dVelToSetTimeStep=sqrt(dC*dC+dUmdU0_ijk_nm1half*dUmdU0_ijk_nm1half+dV_ijk_nm1half
          *dV_ijk_nm1half+dW_ijk_nm1half*dW_ijk_nm1half);
        
        dTTestR=dDelR/dVelToSetTimeStep;
        dTTestTheta=dRMid*grid.dLocalGridOld[grid.nDTheta][0][j][0]/dVelToSetTimeStep;
        dTTestPhi=dRMid*grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]
          *grid.dLocalGridOld[grid.nDPhi][0][0][k]/dVelToSetTimeStep;
        dTest_ConVelOverSoundSpeed_R=fabs(grid.dLocalGridNew[grid.nU][nIInt][j][k]
          -grid.dLocalGridNew[grid.nU0][nIInt][0][0])/dC;
        dTest_ConVelOverSoundSpeed_T=fabs(grid.dLocalGridNew[grid.nV][i][nJInt][k])/dC;
        dTest_ConVelOverSoundSpeed_P=fabs(grid.dLocalGridNew[grid.nW][i][j][nKInt])/dC;
        dTest_ConVel_R=fabs(grid.dLocalGridNew[grid.nU][nIInt][j][k]
          -grid.dLocalGridNew[grid.nU0][nIInt][0][0]);
        dTest_ConVel_T=fabs(grid.dLocalGridNew[grid.nV][i][nJInt][k]);
        dTest_ConVel_P=fabs(grid.dLocalGridNew[grid.nW][i][j][nKInt]);
        
        //keep smallest time step
        if(dTTestR<dTemp){
          dTemp=dTTestR;
          nShellWithSmallestDT=i;
        }
        if(dTTestTheta<dTemp){
          dTemp=dTTestTheta;
          nShellWithSmallestDT=i;
        }
        if(dTTestPhi<dTemp){
          dTemp=dTTestPhi;
          nShellWithSmallestDT=i;
        }
        
        //keep largest convective velocity over sound speed
        if(dTest_ConVelOverSoundSpeed_R>dTest_ConVelOverSoundSpeed){
          dTest_ConVelOverSoundSpeed=dTest_ConVelOverSoundSpeed_R;
        }
        if(dTest_ConVelOverSoundSpeed_T>dTest_ConVelOverSoundSpeed){
          dTest_ConVelOverSoundSpeed=dTest_ConVelOverSoundSpeed_T;
        }
        if(dTest_ConVelOverSoundSpeed_P>dTest_ConVelOverSoundSpeed){
          dTest_ConVelOverSoundSpeed=dTest_ConVelOverSoundSpeed_P;
        }
        
        //keep largest convective velocity
        if(dTest_ConVel_R>dTest_ConVel){
          dTest_ConVel=dTest_ConVel_R;
        }
        if(dTest_ConVel_T>dTest_ConVel){
          dTest_ConVel=dTest_ConVel_T;
        }
        if(dTest_ConVel_P>dTest_ConVel){
          dTest_ConVel=dTest_ConVel_P;
        }
        
        //keep max change in rho
        dDelRho_t_Rho_max_test=fabs((grid.dLocalGridOld[grid.nD][i][j][k]
          -grid.dLocalGridNew[grid.nD][i][j][k])/grid.dLocalGridNew[grid.nD][i][j][k]);
        if(dDelRho_t_Rho_max_test>dDelRho_t_Rho_max_local){
          dDelRho_t_Rho_max_local=dDelRho_t_Rho_max_test;
        }
        
        //keep max change in T
        dDelT_t_T_max_test=fabs((grid.dLocalGridOld[grid.nT][i][j][k]
          -grid.dLocalGridNew[grid.nT][i][j][k])/grid.dLocalGridNew[grid.nT][i][j][k]);
        if(dDelT_t_T_max_test>dDelT_t_T_max_local){
          dDelT_t_T_max_local=dDelT_t_T_max_test;
        }
        
        //keep max change in U-U0
        if(fabs(grid.dLocalGridNew[grid.nU][nIInt][j][k]
            -grid.dLocalGridNew[grid.nU0][nIInt][0][0])<1.0e4){
          dDelUmU0_t_UmU0_max_test=fabs( ((grid.dLocalGridOld[grid.nU][nIInt][j][k]
            -grid.dLocalGridOld[grid.nU0][nIInt][0][0])-(grid.dLocalGridNew[grid.nU][nIInt][j][k]
            -grid.dLocalGridNew[grid.nU0][nIInt][0][0]))/1.0e4);
        }
        else{
          dDelUmU0_t_UmU0_max_test=fabs( ((grid.dLocalGridOld[grid.nU][nIInt][j][k]
            -grid.dLocalGridOld[grid.nU0][nIInt][0][0])-(grid.dLocalGridNew[grid.nU][nIInt][j][k]
            -grid.dLocalGridNew[grid.nU0][nIInt][0][0]))/(grid.dLocalGridNew[grid.nU][nIInt][j][k]
            -grid.dLocalGridNew[grid.nU0][nIInt][0][0]));
        }
        if(dDelUmU0_t_UmU0_max_test>dDelUmU0_t_UmU0_max_local){
          dDelUmU0_t_UmU0_max_local=dDelUmU0_t_UmU0_max_test;
        }
        
        //keep max change in V
        if(fabs(grid.dLocalGridNew[grid.nV][i][nJInt][k])<1.0e4){/* prevents really large value near
          0 which isn't desirable */
          dDelV_t_V_max_test=fabs((grid.dLocalGridOld[grid.nV][i][nJInt][k]
            -grid.dLocalGridNew[grid.nV][i][nJInt][k])/1.0e4);
        }
        else{
          dDelV_t_V_max_test=fabs((grid.dLocalGridOld[grid.nV][i][nJInt][k]
            -grid.dLocalGridNew[grid.nV][i][nJInt][k])/grid.dLocalGridNew[grid.nV][i][nJInt][k]);
        }
        if(dDelV_t_V_max_test>dDelV_t_V_max_local){/* select largest value */
          dDelV_t_V_max_local=dDelV_t_V_max_test;
        }
        
        //keep max change in W
        if(fabs(grid.dLocalGridNew[grid.nW][i][j][nKInt])<1.0e4){/* prevents really large value near
          0 which isn't desirable */
          dDelW_t_W_max_test=fabs((grid.dLocalGridOld[grid.nW][i][j][nKInt]
            -grid.dLocalGridNew[grid.nW][i][j][nKInt])/1.0e4);
        }
        else{
          dDelW_t_W_max_test=fabs((grid.dLocalGridOld[grid.nW][i][j][nKInt]
            -grid.dLocalGridNew[grid.nW][i][j][nKInt])/grid.dLocalGridNew[grid.nW][i][j][nKInt]);
        }
        if(dDelW_t_W_max_test>dDelW_t_W_max_local){/* select largest value */
          dDelW_t_W_max_local=dDelW_t_W_max_test;
        }
      }
    }
  }
  
  //use MPI::allreduce to send the smallest of all calculated time steps to all procs.
  double dTemp2;
  double dTest_ConVelOverSoundSpeed2;
  MPI::COMM_WORLD.Allreduce(&dTemp,&dTemp2,1,MPI::DOUBLE,MPI_MIN);
  if(dTemp<=0.0){//current processor found negative time step
    std::stringstream ssTemp;
    ssTemp<<__FILE__<<":"<<__FUNCTION__<<":"<<__LINE__<<":"<<procTop.nRank
      <<": smallest time step is negative in shell, "<<nShellWithSmallestDT<<std::endl;
    throw exception2(ssTemp.str(),INPUT);
  }
  if(dTemp2<=0.0){//some other processor found negative time step, should quit
    std::stringstream ssTemp;
    ssTemp.str("");
    throw exception2(ssTemp.str(),INPUT);
  }
  
  //find largest changes
  MPI::COMM_WORLD.Allreduce(&dDelRho_t_Rho_max_local,&dDelRho_t_Rho_max,1,MPI::DOUBLE,MPI_MAX);
  MPI::COMM_WORLD.Allreduce(&dDelT_t_T_max_local,&dDelT_t_T_max,1,MPI::DOUBLE,MPI_MAX);
  MPI::COMM_WORLD.Allreduce(&dDelUmU0_t_UmU0_max_local,&dDelUmU0_t_UmU0_max,1,MPI::DOUBLE,MPI_MAX);
  MPI::COMM_WORLD.Allreduce(&dDelV_t_V_max_local,&dDelV_t_V_max,1,MPI::DOUBLE,MPI_MAX);
  MPI::COMM_WORLD.Allreduce(&dDelW_t_W_max_local,&dDelW_t_W_max,1,MPI::DOUBLE,MPI_MAX);
  time.dDelRho_t_Rho_max=dDelRho_t_Rho_max;
  time.dDelT_t_T_max=dDelT_t_T_max;
  time.dDelUmU0_t_UmU0_max=dDelUmU0_t_UmU0_max;
  time.dDelV_t_V_max=dDelV_t_V_max;
  time.dDelW_t_W_max=dDelW_t_W_max;
  
  //pick largest change to limit time step
  dMaxChange=time.dDelRho_t_Rho_max;
  if(time.dDelT_t_T_max>dMaxChange){
    dMaxChange=time.dDelT_t_T_max;
  }
  if(time.dDelUmU0_t_UmU0_max>dMaxChange){
    dMaxChange=time.dDelUmU0_t_UmU0_max;
  }
  if(time.dDelV_t_V_max>dMaxChange){
    dMaxChange=time.dDelV_t_V_max;
  }
  if(time.dDelW_t_W_max>dMaxChange){
    dMaxChange=time.dDelW_t_W_max;
  }
  if(time.dPerChange/dMaxChange<1.0){
    dTemp2=time.dPerChange/dMaxChange*time.dDeltat_np1half;
  }
  else{
    dTemp2=dTemp2*time.dTimeStepFactor;//apply courant factor
  }
  
  if(dTemp2>time.dDeltat_np1half*1.02){//limit how fast the timestep can grow by 2%
    dTemp2=time.dDeltat_np1half*1.02;
  }
  
  //update time info
  time.dDeltat_nm1half=time.dDeltat_np1half;// time between t^n and t^{n+1}
  time.dDeltat_np1half=dTemp2;
  time.dDeltat_n=(time.dDeltat_np1half+time.dDeltat_nm1half)*0.5;//time between t^{n-1/2} and t^{n+1/2}
  time.dt+=time.dDeltat_np1half;
  time.nTimeStepIndex++;
  
  //keep largest convective velocity
  double dTest_ConVel2;
  MPI::COMM_WORLD.Allreduce(&dTest_ConVel,&dTest_ConVel2,1,MPI::DOUBLE,MPI_MAX);
  parameters.dMaxConvectiveVelocity=dTest_ConVel2;
  
  //set donnor fraction
  MPI::COMM_WORLD.Allreduce(&dTest_ConVelOverSoundSpeed,&dTest_ConVelOverSoundSpeed2,1
    ,MPI::DOUBLE,MPI_MAX);
  parameters.dMaxConvectiveVelocity_c=dTest_ConVelOverSoundSpeed2;
  if(dTest_ConVelOverSoundSpeed2>1.0){
    parameters.dDonorFrac=1.0;
  }
  else if(dTest_ConVelOverSoundSpeed2<0.1){
    parameters.dDonorFrac=0.1;
  }
  else{
    parameters.dDonorFrac=dTest_ConVelOverSoundSpeed2;
  }
}
void calDelt_CONST(Grid &grid, Parameters &parameters, Time &time, ProcTop &procTop){
  time.dDeltat_nm1half=time.dConstTimeStep;// time between t^n and t^{n+1}
  time.dDeltat_np1half=time.dConstTimeStep;
  time.dDeltat_n=time.dConstTimeStep;//time between t^{n-1/2} and t^{n+1/2}
  time.dt+=time.dConstTimeStep;//increase time by time step
  time.nTimeStepIndex++;//increase time step index
}
void implicitSolve_None(Grid &grid,Implicit &implicit,Parameters &parameters,Time &time
  ,ProcTop &procTop,MessPass &messPass,Functions &functions){
  //this is an empty funciton that is called when no implicit solve is done.
}
void implicitSolve_R(Grid &grid,Implicit &implicit,Parameters &parameters,Time &time
  ,ProcTop &procTop,MessPass &messPass,Functions &functions){
  
  double *dValuesRHS=new double[implicit.nNumRowsALocal+implicit.nNumRowsALocalSB];
  int *nIndicesRHS=new int[implicit.nNumRowsALocal+implicit.nNumRowsALocalSB];
  
  //loop until corrections are small enough
  double dRelTError=std::numeric_limits<double>::max();
  int nNumIterations=0;
  int nI;
  int nJ;
  int nK;
  double dF_ijk_Tijk;
  double dF_ijk_Ti1;
  double dF_ijk_Tijk1;
  double dF_ijk_Tip1;
  double dF_ijk_Tim1;
  double dTemps[]={0,0,0};
  double *dValues;
  double dRelTErrorLocal;
  double dTemp2;
  while(dRelTError>implicit.dTolerance
    &&nNumIterations<implicit.nMaxNumIterations){
    //CALCULATE COEFFECIENT MATRIX AND RHS
    
    //calculate on inner grid
    for(int i=0;i<implicit.nNumRowsALocal;i++){//for each row
      nI=implicit.nLocFun[i][0];
      nJ=implicit.nLocFun[i][1];
      nK=implicit.nLocFun[i][2];
      dTemps[0]=grid.dLocalGridNew[grid.nT][nI][nJ][nK];
      dTemps[1]=grid.dLocalGridNew[grid.nT][nI+1][nJ][nK];
      dTemps[2]=grid.dLocalGridNew[grid.nT][nI-1][nJ][nK];
      dF_ijk_Tijk=functions.fpImplicitEnergyFunction(grid,parameters,time,dTemps,nI,nJ,nK);
      dValuesRHS[i]=-1.0*dF_ijk_Tijk;
      nIndicesRHS[i]=implicit.nLocDer[i][0][0];
      dValues=new double[implicit.nNumDerPerRow[i]];
      for(int j=0;j<implicit.nNumDerPerRow[i];j++){//for each derivative
        
        switch(implicit.nTypeDer[i][j]){
          case 0 :{//calculate derivative of energy equation wrt. T at i
            dTemps[0]=grid.dLocalGridNew[grid.nT][nI][nJ][nK]*(1.0+implicit.dDerivativeStepFraction);
            dTemps[1]=grid.dLocalGridNew[grid.nT][nI+1][nJ][nK];
            dTemps[2]=grid.dLocalGridNew[grid.nT][nI-1][nJ][nK];
            dF_ijk_Tijk1=functions.fpImplicitEnergyFunction(grid,parameters,time,dTemps,nI,nJ,nK);
            dValues[j]=(dF_ijk_Tijk1-dF_ijk_Tijk)
              /(implicit.dDerivativeStepFraction*grid.dLocalGridNew[grid.nT][nI][nJ][nK]);
            break;
          }
          case 1 :{//calculate derivative of energy equation wrt. T at i+1
            dTemps[0]=grid.dLocalGridNew[grid.nT][nI][nJ][nK];
            dTemps[1]=grid.dLocalGridNew[grid.nT][nI+1][nJ][nK]*(1.0+implicit.dDerivativeStepFraction);
            dTemps[2]=grid.dLocalGridNew[grid.nT][nI-1][nJ][nK];
            dF_ijk_Tip1=functions.fpImplicitEnergyFunction(grid,parameters,time,dTemps,nI,nJ,nK);
            dValues[j]=(dF_ijk_Tip1-dF_ijk_Tijk)
              /(implicit.dDerivativeStepFraction*grid.dLocalGridNew[grid.nT][nI+1][nJ][nK]);
            break;
          }
          case 2 :{//calculate derivative of energy equation wrt. T at i-1
            dTemps[0]=grid.dLocalGridNew[grid.nT][nI][nJ][nK];
            dTemps[1]=grid.dLocalGridNew[grid.nT][nI+1][nJ][nK];
            dTemps[2]=grid.dLocalGridNew[grid.nT][nI-1][nJ][nK]*(1.0+implicit.dDerivativeStepFraction);
            dF_ijk_Tim1=functions.fpImplicitEnergyFunction(grid,parameters,time,dTemps,nI,nJ,nK);
            dValues[j]=(dF_ijk_Tim1-dF_ijk_Tijk)
              /(implicit.dDerivativeStepFraction*grid.dLocalGridNew[grid.nT][nI-1][nJ][nK]);
            break;
          }
        }
      }
      MatSetValues(
        implicit.matCoeff,//matrix to set values in
        1,//number or rows
        &implicit.nLocDer[i][0][0],//global index of rows
        implicit.nNumDerPerRow[i],//number of columns
        implicit.nLocDer[i][1],//global index of column
        dValues,//logically two-dimensional array of values
        INSERT_VALUES);
      delete [] dValues;
    }
    
    //calculate at surface
    for(int i=implicit.nNumRowsALocal;i<implicit.nNumRowsALocal+implicit.nNumRowsALocalSB;i++){//for each row
      nI=implicit.nLocFun[i][0];
      nJ=implicit.nLocFun[i][1];
      nK=implicit.nLocFun[i][2];
      dTemps[0]=grid.dLocalGridNew[grid.nT][nI][nJ][nK];
      dTemps[1]=grid.dLocalGridNew[grid.nT][nI-1][nJ][nK];
      dF_ijk_Tijk=functions.fpImplicitEnergyFunction_SB(grid,parameters,time,dTemps,nI,nJ,nK);
      dValuesRHS[i]=-1.0*dF_ijk_Tijk;
      nIndicesRHS[i]=implicit.nLocDer[i][0][0];
      dValues=new double[implicit.nNumDerPerRow[i]];
      for(int j=0;j<implicit.nNumDerPerRow[i];j++){//for each derivative
        
        switch(implicit.nTypeDer[i][j]){
          case 0 :{//calculate derivative of energy equation wrt. T at i
            dTemps[0]=grid.dLocalGridNew[grid.nT][nI][nJ][nK]*(1.0+implicit.dDerivativeStepFraction);
            dTemps[1]=grid.dLocalGridNew[grid.nT][nI-1][nJ][nK];
            dF_ijk_Ti1=functions.fpImplicitEnergyFunction_SB(grid,parameters,time,dTemps,nI,nJ,nK);
            dValues[j]=(dF_ijk_Ti1-dF_ijk_Tijk)
              /(implicit.dDerivativeStepFraction*grid.dLocalGridNew[grid.nT][nI][nJ][nK]);
            break;
          }
          //no case for i+1 at surface boundary
          case 2 :{//calculate derivative of energy equation wrt. T at i-1
            dTemps[0]=grid.dLocalGridNew[grid.nT][nI][nJ][nK];
            dTemps[1]=grid.dLocalGridNew[grid.nT][nI-1][nJ][nK]*(1.0+implicit.dDerivativeStepFraction);
            dF_ijk_Tim1=functions.fpImplicitEnergyFunction_SB(grid,parameters,time,dTemps,nI,nJ,nK);
            dValues[j]=(dF_ijk_Tim1-dF_ijk_Tijk)
              /(implicit.dDerivativeStepFraction*grid.dLocalGridNew[grid.nT][nI-1][nJ][nK]);
            break;
          }
        }
      }
      MatSetValues(
        implicit.matCoeff,//matrix to set values in
        1,//number or rows
        &implicit.nLocDer[i][0][0],//global index of rows
        implicit.nNumDerPerRow[i],//number of columns
        implicit.nLocDer[i][1],//global index of column
        dValues,//logically two-dimensional array of values
        INSERT_VALUES);
      delete [] dValues;
    }
    
    //assemble coeffecient matrix
    MatAssemblyBegin(implicit.matCoeff,MAT_FINAL_ASSEMBLY);
    
    //set values of the RHS
    VecSetValues(implicit.vecRHS
      ,implicit.nNumRowsALocal+implicit.nNumRowsALocalSB
      ,nIndicesRHS
      ,dValuesRHS
      ,INSERT_VALUES);
    
    VecAssemblyBegin(implicit.vecRHS);
    VecAssemblyEnd(implicit.vecRHS);
    MatAssemblyEnd(implicit.matCoeff,MAT_FINAL_ASSEMBLY);
    
    //solve system
    KSPSetOperators(implicit.kspContext,implicit.matCoeff,implicit.matCoeff,SAME_NONZERO_PATTERN);
    KSPSolve(implicit.kspContext,implicit.vecRHS,implicit.vecTCorrections);
    
    //get distributed corrections
    VecScatterBegin(implicit.vecscatTCorrections,implicit.vecTCorrections
      ,implicit.vecTCorrectionsLocal,INSERT_VALUES,SCATTER_FORWARD);
    VecScatterEnd(implicit.vecscatTCorrections,implicit.vecTCorrections
      ,implicit.vecTCorrectionsLocal,INSERT_VALUES,SCATTER_FORWARD);
    VecGetArray(implicit.vecTCorrectionsLocal,&dValues);
    
    //apply corrections
    dRelTErrorLocal=0.0;
    for(int i=0;i<implicit.nNumRowsALocal+implicit.nNumRowsALocalSB;i++){
      nI=implicit.nLocFun[i][0];
      nJ=implicit.nLocFun[i][1];
      nK=implicit.nLocFun[i][2];
      grid.dLocalGridNew[grid.nT][nI][nJ][nK]+=dValues[i];
      if(grid.dLocalGridNew[grid.nT][nI][nJ][nK]<0.0){
        
        #if SIGNEGTEMP==1
        raise(SIGINT);
        #endif
        
        std::stringstream ssTemp;
        ssTemp<<__FILE__<<":"<<__FUNCTION__<<":"<<__LINE__<<":"<<procTop.nRank
          <<": negative temperature calculated in , ("<<nI<<","<<nJ<<","<<nK<<") on iteration "
          <<nNumIterations<<"\n";
        throw exception2(ssTemp.str(),CALCULATION);
        
      }
      dTemp2=fabs(dValues[i]/grid.dLocalGridNew[grid.nT][nI][nJ][nK]);
      if(dRelTErrorLocal<dTemp2){
        dRelTErrorLocal=dTemp2;
      }
    }
    
    updateLocalBoundariesNewGrid(grid.nT,procTop,messPass,grid);
    
    MPI::COMM_WORLD.Allreduce(&dRelTErrorLocal,&dRelTError,1,MPI::DOUBLE,MPI_MAX);
    
    VecRestoreArray(implicit.vecTCorrectionsLocal,&dValues);
    nNumIterations++;
  }
  
  #if TRACKMAXSOLVERERROR==1
    
    /* Calculate absolute error in solver*/
    Vec vecCalRHS1;
    Vec vecCalRHS2;
    VecDuplicate(implicit.vecRHS,&vecCalRHS1);
    VecDuplicate(implicit.vecRHS,&vecCalRHS2);
    MatMult(implicit.matCoeff,implicit.vecTCorrections,vecCalRHS1);
    VecCopy(vecCalRHS1,vecCalRHS2);
    
    VecAXPY(vecCalRHS1,-1.0,implicit.vecRHS);
    
    //get maximum absolute error, and average value of the RHS
    int nIndexLargestError;
    double dMaxError;
    VecMax(vecCalRHS1,&nIndexLargestError,&dMaxError);
    dMaxError=fabs(dMaxError);
    if(dMaxError>implicit.dMaxErrorInRHS){
      implicit.dMaxErrorInRHS=dMaxError;
      double dSumRHS=0.0;
      VecAbs(vecCalRHS2);
      VecSum(vecCalRHS2,&dSumRHS);
      int nSizeVecCalRHS;
      VecGetSize(vecCalRHS2,&nSizeVecCalRHS);
      implicit.dAverageRHS=dSumRHS/double(nSizeVecCalRHS);
    }
    
    //keep track of largest number of interations the solver took
    int nNumIterationsSolve;
    KSPGetIterationNumber(implicit.kspContext,&nNumIterationsSolve);
    if(nNumIterationsSolve>implicit.nMaxNumSolverIterations){
      implicit.nMaxNumSolverIterations=nNumIterationsSolve;
    }
  #endif
  
  delete [] dValuesRHS;
  delete [] nIndicesRHS;
  if(dRelTError>implicit.dCurrentRelTError){
    implicit.dCurrentRelTError=dRelTError;
  }
  if(nNumIterations>implicit.nCurrentNumIterations){
    implicit.nCurrentNumIterations=nNumIterations;
  }
  
  if(procTop.nRank==0){
    if(nNumIterations>=implicit.nMaxNumIterations){
      std::cout<<__FILE__<<":"<<__FUNCTION__<<":"<<__LINE__<<":"<<procTop.nRank
      <<": The maximum number of iteration for implicit solution ("<<implicit.nMaxNumIterations
      <<") has be exceeded in current time step ("<<time.nTimeStepIndex
      <<") with a maximum relative error in the implicit calculation of temperature of "
      <<implicit.dCurrentRelTError<<std::endl;
    }
  }
  
  //calculate E, Kappa, P form new temperature
  calNewPEKappaGamma_TEOS(grid,parameters);
}
void implicitSolve_RT(Grid &grid,Implicit &implicit,Parameters &parameters,Time &time
  ,ProcTop &procTop,MessPass &messPass,Functions &functions){
  
  double *dValuesRHS=new double[implicit.nNumRowsALocal+implicit.nNumRowsALocalSB];
  int *nIndicesRHS=new int[implicit.nNumRowsALocal+implicit.nNumRowsALocalSB];
  
  //loop until corrections are small enough
  double dRelTError=std::numeric_limits<double>::max();
  int nNumIterations=0;
  int nI;
  int nJ;
  int nK;
  double dF_ijk_Tijk;
  double dTemps[5];
  double *dValues;
  double dF_ijk_Tijk1;
  double dF_ijk_Tip1;
  double dF_ijk_Tim1;
  double dF_ijk_Tjp1;
  double dF_ijk_Tjm1;
  double dF_ijk_Ti1;
  double dTemp2;
  double dRelTErrorLocal;
  while(dRelTError>implicit.dTolerance
    &&nNumIterations<implicit.nMaxNumIterations){
    //CALCULATE COEFFECIENT MATRIX AND RHS
    
    //calculate on inner grid
    for(int i=0;i<implicit.nNumRowsALocal;i++){//for each row
      nI=implicit.nLocFun[i][0];
      nJ=implicit.nLocFun[i][1];
      nK=implicit.nLocFun[i][2];
      dTemps[0]=grid.dLocalGridNew[grid.nT][nI][nJ][nK];
      dTemps[1]=grid.dLocalGridNew[grid.nT][nI+1][nJ][nK];
      dTemps[2]=grid.dLocalGridNew[grid.nT][nI-1][nJ][nK];
      dTemps[3]=grid.dLocalGridNew[grid.nT][nI][nJ+1][nK];
      dTemps[4]=grid.dLocalGridNew[grid.nT][nI][nJ-1][nK];
      dF_ijk_Tijk=functions.fpImplicitEnergyFunction(grid,parameters,time,dTemps,nI,nJ,nK);
      dValuesRHS[i]=-1.0*dF_ijk_Tijk;
      nIndicesRHS[i]=implicit.nLocDer[i][0][0];
      dValues=new double[implicit.nNumDerPerRow[i]];
      for(int j=0;j<implicit.nNumDerPerRow[i];j++){//for each derivative
        
        switch(implicit.nTypeDer[i][j]){
          case 0 :{//calculate derivative of energy equation wrt. T at i
            dTemps[0]=grid.dLocalGridNew[grid.nT][nI][nJ][nK]*(1.0+implicit.dDerivativeStepFraction);
            dTemps[1]=grid.dLocalGridNew[grid.nT][nI+1][nJ][nK];
            dTemps[2]=grid.dLocalGridNew[grid.nT][nI-1][nJ][nK];
            dTemps[3]=grid.dLocalGridNew[grid.nT][nI][nJ+1][nK];
            dTemps[4]=grid.dLocalGridNew[grid.nT][nI][nJ-1][nK];
            dF_ijk_Tijk1=functions.fpImplicitEnergyFunction(grid,parameters,time,dTemps,nI,nJ,nK);
            dValues[j]=(dF_ijk_Tijk1-dF_ijk_Tijk)
              /(implicit.dDerivativeStepFraction*grid.dLocalGridNew[grid.nT][nI][nJ][nK]);
            break;
          }
          case 1 :{//calculate derivative of energy equation wrt. T at i+1
            dTemps[0]=grid.dLocalGridNew[grid.nT][nI][nJ][nK];
            dTemps[1]=grid.dLocalGridNew[grid.nT][nI+1][nJ][nK]*(1.0+implicit.dDerivativeStepFraction);
            dTemps[2]=grid.dLocalGridNew[grid.nT][nI-1][nJ][nK];
            dTemps[3]=grid.dLocalGridNew[grid.nT][nI][nJ+1][nK];
            dTemps[4]=grid.dLocalGridNew[grid.nT][nI][nJ-1][nK];
            dF_ijk_Tip1=functions.fpImplicitEnergyFunction(grid,parameters,time,dTemps,nI,nJ,nK);
            dValues[j]=(dF_ijk_Tip1-dF_ijk_Tijk)
              /(implicit.dDerivativeStepFraction*grid.dLocalGridNew[grid.nT][nI+1][nJ][nK]);
            break;
          }
          case 2 :{//calculate derivative of energy equation wrt. T at i-1
            dTemps[0]=grid.dLocalGridNew[grid.nT][nI][nJ][nK];
            dTemps[1]=grid.dLocalGridNew[grid.nT][nI+1][nJ][nK];
            dTemps[2]=grid.dLocalGridNew[grid.nT][nI-1][nJ][nK]*(1.0+implicit.dDerivativeStepFraction);
            dTemps[3]=grid.dLocalGridNew[grid.nT][nI][nJ+1][nK];
            dTemps[4]=grid.dLocalGridNew[grid.nT][nI][nJ-1][nK];
            dF_ijk_Tim1=functions.fpImplicitEnergyFunction(grid,parameters,time,dTemps,nI,nJ,nK);
            dValues[j]=(dF_ijk_Tim1-dF_ijk_Tijk)
              /(implicit.dDerivativeStepFraction*grid.dLocalGridNew[grid.nT][nI-1][nJ][nK]);
            break;
          }
          case 3 :{//calculate derivative of energy equation wrt. T at j+1
            dTemps[0]=grid.dLocalGridNew[grid.nT][nI][nJ][nK];
            dTemps[1]=grid.dLocalGridNew[grid.nT][nI+1][nJ][nK];
            dTemps[2]=grid.dLocalGridNew[grid.nT][nI-1][nJ][nK];
            dTemps[3]=grid.dLocalGridNew[grid.nT][nI][nJ+1][nK]*(1.0+implicit.dDerivativeStepFraction);
            dTemps[4]=grid.dLocalGridNew[grid.nT][nI][nJ-1][nK];
            dF_ijk_Tjp1=functions.fpImplicitEnergyFunction(grid,parameters,time,dTemps,nI,nJ,nK);
            dValues[j]=(dF_ijk_Tjp1-dF_ijk_Tijk)
              /(implicit.dDerivativeStepFraction*grid.dLocalGridNew[grid.nT][nI][nJ+1][nK]);
            break;
          }
          case 4 :{//calculate derivative of energy equation wrt. T at j-1
            dTemps[0]=grid.dLocalGridNew[grid.nT][nI][nJ][nK];
            dTemps[1]=grid.dLocalGridNew[grid.nT][nI+1][nJ][nK];
            dTemps[2]=grid.dLocalGridNew[grid.nT][nI-1][nJ][nK];
            dTemps[3]=grid.dLocalGridNew[grid.nT][nI][nJ+1][nK];
            dTemps[4]=grid.dLocalGridNew[grid.nT][nI][nJ-1][nK]*(1.0+implicit.dDerivativeStepFraction);
            dF_ijk_Tjm1=functions.fpImplicitEnergyFunction(grid,parameters,time,dTemps,nI,nJ,nK);
            dValues[j]=(dF_ijk_Tjm1-dF_ijk_Tijk)
              /(implicit.dDerivativeStepFraction*grid.dLocalGridNew[grid.nT][nI][nJ-1][nK]);
            break;
          }
          case 34 :{//calculate derivative of energy equation wrt. T at j+1 and j-1
            dTemps[0]=grid.dLocalGridNew[grid.nT][nI][nJ][nK];
            dTemps[1]=grid.dLocalGridNew[grid.nT][nI+1][nJ][nK];
            dTemps[2]=grid.dLocalGridNew[grid.nT][nI-1][nJ][nK];
            dTemps[3]=grid.dLocalGridNew[grid.nT][nI][nJ+1][nK]*(1.0+implicit.dDerivativeStepFraction);
            dTemps[4]=grid.dLocalGridNew[grid.nT][nI][nJ-1][nK];
            dF_ijk_Tjp1=functions.fpImplicitEnergyFunction(grid,parameters,time,dTemps,nI,nJ,nK);
            dTemps[3]=grid.dLocalGridNew[grid.nT][nI][nJ+1][nK];
            dTemps[4]=grid.dLocalGridNew[grid.nT][nI][nJ-1][nK]*(1.0+implicit.dDerivativeStepFraction);
            dF_ijk_Tjm1=functions.fpImplicitEnergyFunction(grid,parameters,time,dTemps,nI,nJ,nK);
            dValues[j]=(dF_ijk_Tjp1-dF_ijk_Tijk)
              /(implicit.dDerivativeStepFraction*grid.dLocalGridNew[grid.nT][nI][nJ+1][nK])
              +(dF_ijk_Tjm1-dF_ijk_Tijk)
              /(implicit.dDerivativeStepFraction*grid.dLocalGridNew[grid.nT][nI][nJ-1][nK]);
            break;
          }
        }
      }
      MatSetValues(
        implicit.matCoeff,//matrix to set values in
        1,//number or rows
        &implicit.nLocDer[i][0][0],//global index of rows
        implicit.nNumDerPerRow[i],//number of columns
        implicit.nLocDer[i][1],//global index of column
        dValues,//logically two-dimensional array of values
        INSERT_VALUES);
      delete [] dValues;
    }
    
    //calculate at surface
    for(int i=implicit.nNumRowsALocal;i<implicit.nNumRowsALocal+implicit.nNumRowsALocalSB;i++){//for each row
      nI=implicit.nLocFun[i][0];
      nJ=implicit.nLocFun[i][1];
      nK=implicit.nLocFun[i][2];
      dTemps[0]=grid.dLocalGridNew[grid.nT][nI][nJ][nK];
      dTemps[1]=grid.dLocalGridNew[grid.nT][nI-1][nJ][nK];
      dTemps[2]=grid.dLocalGridNew[grid.nT][nI][nJ+1][nK];
      dTemps[3]=grid.dLocalGridNew[grid.nT][nI][nJ-1][nK];
      dF_ijk_Tijk=functions.fpImplicitEnergyFunction_SB(grid,parameters,time,dTemps
        ,nI,nJ,nK);
      dValuesRHS[i]=-1.0*dF_ijk_Tijk;
      nIndicesRHS[i]=implicit.nLocDer[i][0][0];
      dValues=new double[implicit.nNumDerPerRow[i]];
      for(int j=0;j<implicit.nNumDerPerRow[i];j++){//for each derivative
        
        switch(implicit.nTypeDer[i][j]){
          case 0 :{//calculate derivative of energy equation wrt. T at i
            dTemps[0]=grid.dLocalGridNew[grid.nT][nI][nJ][nK]*(1.0+implicit.dDerivativeStepFraction);
            dTemps[1]=grid.dLocalGridNew[grid.nT][nI-1][nJ][nK];
            dTemps[2]=grid.dLocalGridNew[grid.nT][nI][nJ+1][nK];
            dTemps[3]=grid.dLocalGridNew[grid.nT][nI][nJ-1][nK];
            dF_ijk_Ti1=functions.fpImplicitEnergyFunction_SB(grid,parameters,time,dTemps,nI,nJ,nK);
            dValues[j]=(dF_ijk_Ti1-dF_ijk_Tijk)
              /(implicit.dDerivativeStepFraction*grid.dLocalGridNew[grid.nT][nI][nJ][nK]);
            break;
          }
          //case 1: no i+1 at surface
          case 2 :{//calculate derivative of energy equation wrt. T at i-1
            dTemps[0]=grid.dLocalGridNew[grid.nT][nI][nJ][nK];
            dTemps[1]=grid.dLocalGridNew[grid.nT][nI-1][nJ][nK]*(1.0+implicit.dDerivativeStepFraction);
            dTemps[2]=grid.dLocalGridNew[grid.nT][nI][nJ+1][nK];
            dTemps[3]=grid.dLocalGridNew[grid.nT][nI][nJ-1][nK];
            dF_ijk_Tim1=functions.fpImplicitEnergyFunction_SB(grid,parameters,time,dTemps,nI,nJ,nK);
            dValues[j]=(dF_ijk_Tim1-dF_ijk_Tijk)
              /(implicit.dDerivativeStepFraction*grid.dLocalGridNew[grid.nT][nI-1][nJ][nK]);
            break;
          }
          case 3 :{//calculate derivative of energy equation wrt. T at j+1
            dTemps[0]=grid.dLocalGridNew[grid.nT][nI][nJ][nK];
            dTemps[1]=grid.dLocalGridNew[grid.nT][nI-1][nJ][nK];
            dTemps[2]=grid.dLocalGridNew[grid.nT][nI][nJ+1][nK]*(1.0+implicit.dDerivativeStepFraction);
            dTemps[3]=grid.dLocalGridNew[grid.nT][nI][nJ-1][nK];
            dF_ijk_Tjp1=functions.fpImplicitEnergyFunction_SB(grid,parameters,time,dTemps,nI,nJ,nK);
            dValues[j]=(dF_ijk_Tjp1-dF_ijk_Tijk)
              /(implicit.dDerivativeStepFraction*grid.dLocalGridNew[grid.nT][nI][nJ+1][nK]);
            break;
          }
          case 4 :{//calculate derivative of energy equation wrt. T at j-1
            dTemps[0]=grid.dLocalGridNew[grid.nT][nI][nJ][nK];
            dTemps[1]=grid.dLocalGridNew[grid.nT][nI-1][nJ][nK];
            dTemps[2]=grid.dLocalGridNew[grid.nT][nI][nJ+1][nK];
            dTemps[3]=grid.dLocalGridNew[grid.nT][nI][nJ-1][nK]*(1.0+implicit.dDerivativeStepFraction);
            dF_ijk_Tjm1=functions.fpImplicitEnergyFunction_SB(grid,parameters,time,dTemps,nI,nJ,nK);
            dValues[j]=(dF_ijk_Tjm1-dF_ijk_Tijk)
              /(implicit.dDerivativeStepFraction*grid.dLocalGridNew[grid.nT][nI][nJ-1][nK]);
            break;
          }
          case 34 :{//calculate derivative of energy equation wrt. T at j+1 and j-1
            dTemps[0]=grid.dLocalGridNew[grid.nT][nI][nJ][nK];
            dTemps[1]=grid.dLocalGridNew[grid.nT][nI-1][nJ][nK];
            dTemps[2]=grid.dLocalGridNew[grid.nT][nI][nJ+1][nK]*(1.0+implicit.dDerivativeStepFraction);
            dTemps[3]=grid.dLocalGridNew[grid.nT][nI][nJ-1][nK];
            dF_ijk_Tjp1=functions.fpImplicitEnergyFunction_SB(grid,parameters,time,dTemps,nI,nJ,nK);
            dTemps[2]=grid.dLocalGridNew[grid.nT][nI][nJ+1][nK];
            dTemps[3]=grid.dLocalGridNew[grid.nT][nI][nJ-1][nK]*(1.0+implicit.dDerivativeStepFraction);
            dF_ijk_Tjm1=functions.fpImplicitEnergyFunction_SB(grid,parameters,time,dTemps,nI,nJ,nK);
            dValues[j]=(dF_ijk_Tjp1-dF_ijk_Tijk)
              /(implicit.dDerivativeStepFraction*grid.dLocalGridNew[grid.nT][nI][nJ+1][nK])
              +(dF_ijk_Tjm1-dF_ijk_Tijk)
              /(implicit.dDerivativeStepFraction*grid.dLocalGridNew[grid.nT][nI][nJ-1][nK]);
            break;
          }
        }
      }
      MatSetValues(
        implicit.matCoeff,//matrix to set values in
        1,//number or rows
        &implicit.nLocDer[i][0][0],//global index of rows
        implicit.nNumDerPerRow[i],//number of columns
        implicit.nLocDer[i][1],//global index of column
        dValues,//logically two-dimensional array of values
        INSERT_VALUES);
      delete [] dValues;
    }
    
    //assemble coeffecient matrix
    MatAssemblyBegin(implicit.matCoeff,MAT_FINAL_ASSEMBLY);
    
    //set values of the RHS
    VecSetValues(implicit.vecRHS
      ,implicit.nNumRowsALocal+implicit.nNumRowsALocalSB
      ,nIndicesRHS
      ,dValuesRHS
      ,INSERT_VALUES);
    
    VecAssemblyBegin(implicit.vecRHS);
    VecAssemblyEnd(implicit.vecRHS);
    MatAssemblyEnd(implicit.matCoeff,MAT_FINAL_ASSEMBLY);
    
    //solve system
    KSPSetOperators(implicit.kspContext,implicit.matCoeff,implicit.matCoeff,SAME_NONZERO_PATTERN);
    KSPSolve(implicit.kspContext,implicit.vecRHS,implicit.vecTCorrections);
    
    //get distributed corrections
    VecScatterBegin(implicit.vecscatTCorrections,implicit.vecTCorrections
      ,implicit.vecTCorrectionsLocal,INSERT_VALUES,SCATTER_FORWARD);
    VecScatterEnd(implicit.vecscatTCorrections,implicit.vecTCorrections
      ,implicit.vecTCorrectionsLocal,INSERT_VALUES,SCATTER_FORWARD);
    VecGetArray(implicit.vecTCorrectionsLocal,&dValues);
    
    //apply corrections
    dRelTErrorLocal=0.0;
    for(int i=0;i<implicit.nNumRowsALocal+implicit.nNumRowsALocalSB;i++){
      nI=implicit.nLocFun[i][0];
      nJ=implicit.nLocFun[i][1];
      nK=implicit.nLocFun[i][2];
      grid.dLocalGridNew[grid.nT][nI][nJ][nK]+=dValues[i];
      if(grid.dLocalGridNew[grid.nT][nI][nJ][nK]<0.0){
        
        #if SIGNEGTEMP==1
        raise(SIGINT);
        #endif
        
        std::stringstream ssTemp;
        ssTemp<<__FILE__<<":"<<__FUNCTION__<<":"<<__LINE__<<":"<<procTop.nRank
          <<": negative temperature calculated in , ("<<nI<<","<<nJ<<","<<nK<<") on iteration "
          <<nNumIterations<<"\n";
        throw exception2(ssTemp.str(),CALCULATION);
        
      }
      dTemp2=fabs(dValues[i]/grid.dLocalGridNew[grid.nT][nI][nJ][nK]);
      if(dRelTErrorLocal<dTemp2){
        dRelTErrorLocal=dTemp2;
      }
    }
    
    updateLocalBoundariesNewGrid(grid.nT,procTop,messPass,grid);
    
    MPI::COMM_WORLD.Allreduce(&dRelTErrorLocal,&dRelTError,1,MPI::DOUBLE,MPI_MAX);
    
    VecRestoreArray(implicit.vecTCorrectionsLocal,&dValues);
    
    nNumIterations++;
  }
  
  #if TRACKMAXSOLVERERROR==1
    
    /* Calculate absolute error in solver*/
    Vec vecCalRHS1;
    Vec vecCalRHS2;
    VecDuplicate(implicit.vecRHS,&vecCalRHS1);
    VecDuplicate(implicit.vecRHS,&vecCalRHS2);
    MatMult(implicit.matCoeff,implicit.vecTCorrections,vecCalRHS1);
    VecCopy(vecCalRHS1,vecCalRHS2);
    
    VecAXPY(vecCalRHS1,-1.0,implicit.vecRHS);
    
    //get maximum absolute error, and average value of the RHS
    int nIndexLargestError;
    double dMaxError;
    VecMax(vecCalRHS1,&nIndexLargestError,&dMaxError);
    dMaxError=fabs(dMaxError);
    if(dMaxError>implicit.dMaxErrorInRHS){
      implicit.dMaxErrorInRHS=dMaxError;
      double dSumRHS=0.0;
      VecAbs(vecCalRHS2);
      VecSum(vecCalRHS2,&dSumRHS);
      int nSizeVecCalRHS;
      VecGetSize(vecCalRHS2,&nSizeVecCalRHS);
      implicit.dAverageRHS=dSumRHS/double(nSizeVecCalRHS);
    }
    
    //keep track of largest number of interations the solver took
    int nNumIterationsSolve;
    KSPGetIterationNumber(implicit.kspContext,&nNumIterationsSolve);
    if(nNumIterationsSolve>implicit.nMaxNumSolverIterations){
      implicit.nMaxNumSolverIterations=nNumIterationsSolve;
    }
  #endif
  
  delete [] dValuesRHS;
  delete [] nIndicesRHS;
  if(dRelTError>implicit.dCurrentRelTError){
    implicit.dCurrentRelTError=dRelTError;
  }
  if(nNumIterations>implicit.nCurrentNumIterations){
    implicit.nCurrentNumIterations=nNumIterations;
  }
  
  if(procTop.nRank==0){
    if(nNumIterations>=implicit.nMaxNumIterations){
      std::cout<<__FILE__<<":"<<__FUNCTION__<<":"<<__LINE__<<":"<<procTop.nRank
      <<": The maximum number of iteration for implicit solution ("<<implicit.nMaxNumIterations
      <<") has be exceeded in current time step ("<<time.nTimeStepIndex
      <<") with a maximum relative error in the implicit calculation of temperature of "
      <<implicit.dCurrentRelTError<<std::endl;
    }
  }
  
  //calculate E, Kappa, P form new temperature
  calNewPEKappaGamma_TEOS(grid,parameters);
}
void implicitSolve_RTP(Grid &grid,Implicit &implicit,Parameters &parameters,Time &time
  ,ProcTop &procTop,MessPass &messPass,Functions &functions){
  
  double *dValuesRHS=new double[implicit.nNumRowsALocal+implicit.nNumRowsALocalSB];
  int *nIndicesRHS=new int[implicit.nNumRowsALocal+implicit.nNumRowsALocalSB];
  
  //loop until corrections are small enough
  double dRelTError=std::numeric_limits<double>::max();
  int nNumIterations=0;
  int nI;
  int nJ;
  int nK;
  double dTemps[7];
  double dF_ijk_Tijk;
  double *dValues;
  double dF_ijk_Ti1;
  double dF_ijk_Tip1;
  double dF_ijk_Tim1;
  double dF_ijk_Tjp1;
  double dF_ijk_Tjm1;
  double dF_ijk_Tkp1;
  double dF_ijk_Tkm1;
  double dRelTErrorLocal;
  double dTemp2;
  while(dRelTError>implicit.dTolerance
    &&nNumIterations<implicit.nMaxNumIterations){
    //CALCULATE COEFFECIENT MATRIX AND RHS
    
    //calculate on inner grid
    for(int i=0;i<implicit.nNumRowsALocal;i++){//for each row
      nI=implicit.nLocFun[i][0];
      nJ=implicit.nLocFun[i][1];
      nK=implicit.nLocFun[i][2];
      dTemps[0]=grid.dLocalGridNew[grid.nT][nI][nJ][nK];
      dTemps[1]=grid.dLocalGridNew[grid.nT][nI+1][nJ][nK];
      dTemps[2]=grid.dLocalGridNew[grid.nT][nI-1][nJ][nK];
      dTemps[3]=grid.dLocalGridNew[grid.nT][nI][nJ+1][nK];
      dTemps[4]=grid.dLocalGridNew[grid.nT][nI][nJ-1][nK];
      dTemps[5]=grid.dLocalGridNew[grid.nT][nI][nJ][nK+1];
      dTemps[6]=grid.dLocalGridNew[grid.nT][nI][nJ][nK-1];
      dF_ijk_Tijk=functions.fpImplicitEnergyFunction(grid,parameters,time,dTemps,nI,nJ,nK);
      dValuesRHS[i]=-1.0*dF_ijk_Tijk;
      nIndicesRHS[i]=implicit.nLocDer[i][0][0];
      dValues=new double[implicit.nNumDerPerRow[i]];
      for(int j=0;j<implicit.nNumDerPerRow[i];j++){//for each derivative
        
        switch(implicit.nTypeDer[i][j]){
          case 0 :{//calculate derivative of energy equation wrt. T at i
            dTemps[0]=grid.dLocalGridNew[grid.nT][nI][nJ][nK]*(1.0+implicit.dDerivativeStepFraction);
            dTemps[1]=grid.dLocalGridNew[grid.nT][nI+1][nJ][nK];
            dTemps[2]=grid.dLocalGridNew[grid.nT][nI-1][nJ][nK];
            dTemps[3]=grid.dLocalGridNew[grid.nT][nI][nJ+1][nK];
            dTemps[4]=grid.dLocalGridNew[grid.nT][nI][nJ-1][nK];
            dTemps[5]=grid.dLocalGridNew[grid.nT][nI][nJ][nK+1];
            dTemps[6]=grid.dLocalGridNew[grid.nT][nI][nJ][nK-1];
            dF_ijk_Ti1=functions.fpImplicitEnergyFunction(grid,parameters,time,dTemps,nI,nJ,nK);
            dValues[j]=(dF_ijk_Ti1-dF_ijk_Tijk)
                /(implicit.dDerivativeStepFraction*grid.dLocalGridNew[grid.nT][nI][nJ][nK]);
            break;
          }
          case 1 :{//calculate derivative of energy equation wrt. T at i+1
            dTemps[0]=grid.dLocalGridNew[grid.nT][nI][nJ][nK];
            dTemps[1]=grid.dLocalGridNew[grid.nT][nI+1][nJ][nK]*(1.0+implicit.dDerivativeStepFraction);
            dTemps[2]=grid.dLocalGridNew[grid.nT][nI-1][nJ][nK];
            dTemps[3]=grid.dLocalGridNew[grid.nT][nI][nJ+1][nK];
            dTemps[4]=grid.dLocalGridNew[grid.nT][nI][nJ-1][nK];
            dTemps[5]=grid.dLocalGridNew[grid.nT][nI][nJ][nK+1];
            dTemps[6]=grid.dLocalGridNew[grid.nT][nI][nJ][nK-1];
            dF_ijk_Tip1=functions.fpImplicitEnergyFunction(grid,parameters,time,dTemps,nI,nJ,nK);
            dValues[j]=(dF_ijk_Tip1-dF_ijk_Tijk)
              /(implicit.dDerivativeStepFraction*grid.dLocalGridNew[grid.nT][nI+1][nJ][nK]);
            break;
          }
          case 2 :{//calculate derivative of energy equation wrt. T at i-1
            dTemps[0]=grid.dLocalGridNew[grid.nT][nI][nJ][nK];
            dTemps[1]=grid.dLocalGridNew[grid.nT][nI+1][nJ][nK];
            dTemps[2]=grid.dLocalGridNew[grid.nT][nI-1][nJ][nK]*(1.0+implicit.dDerivativeStepFraction);
            dTemps[3]=grid.dLocalGridNew[grid.nT][nI][nJ+1][nK];
            dTemps[4]=grid.dLocalGridNew[grid.nT][nI][nJ-1][nK];
            dTemps[5]=grid.dLocalGridNew[grid.nT][nI][nJ][nK+1];
            dTemps[6]=grid.dLocalGridNew[grid.nT][nI][nJ][nK-1];
            dF_ijk_Tim1=functions.fpImplicitEnergyFunction(grid,parameters,time,dTemps,nI,nJ,nK);
            dValues[j]=(dF_ijk_Tim1-dF_ijk_Tijk)
              /(implicit.dDerivativeStepFraction*grid.dLocalGridNew[grid.nT][nI-1][nJ][nK]);
            break;
          }
          case 3 :{//calculate derivative of energy equation wrt. T at j+1
            dTemps[0]=grid.dLocalGridNew[grid.nT][nI][nJ][nK];
            dTemps[1]=grid.dLocalGridNew[grid.nT][nI+1][nJ][nK];
            dTemps[2]=grid.dLocalGridNew[grid.nT][nI-1][nJ][nK];
            dTemps[3]=grid.dLocalGridNew[grid.nT][nI][nJ+1][nK]*(1.0+implicit.dDerivativeStepFraction);
            dTemps[4]=grid.dLocalGridNew[grid.nT][nI][nJ-1][nK];
            dTemps[5]=grid.dLocalGridNew[grid.nT][nI][nJ][nK+1];
            dTemps[6]=grid.dLocalGridNew[grid.nT][nI][nJ][nK-1];
            dF_ijk_Tjp1=functions.fpImplicitEnergyFunction(grid,parameters,time,dTemps,nI,nJ,nK);
            dValues[j]=(dF_ijk_Tjp1-dF_ijk_Tijk)
              /(implicit.dDerivativeStepFraction*grid.dLocalGridNew[grid.nT][nI][nJ+1][nK]);
            break;
          }
          case 4 :{//calculate derivative of energy equation wrt. T at j-1
            dTemps[0]=grid.dLocalGridNew[grid.nT][nI][nJ][nK];
            dTemps[1]=grid.dLocalGridNew[grid.nT][nI+1][nJ][nK];
            dTemps[2]=grid.dLocalGridNew[grid.nT][nI-1][nJ][nK];
            dTemps[3]=grid.dLocalGridNew[grid.nT][nI][nJ+1][nK];
            dTemps[4]=grid.dLocalGridNew[grid.nT][nI][nJ-1][nK]*(1.0+implicit.dDerivativeStepFraction);
            dTemps[5]=grid.dLocalGridNew[grid.nT][nI][nJ][nK+1];
            dTemps[6]=grid.dLocalGridNew[grid.nT][nI][nJ][nK-1];
            dF_ijk_Tjm1=functions.fpImplicitEnergyFunction(grid,parameters,time,dTemps,nI,nJ,nK);
            dValues[j]=(dF_ijk_Tjm1-dF_ijk_Tijk)
              /(implicit.dDerivativeStepFraction*grid.dLocalGridNew[grid.nT][nI][nJ-1][nK]);
            break;
          }
          case 34 :{//calculate derivative of energy equation wrt. T at j+1 and j-1
            dTemps[0]=grid.dLocalGridNew[grid.nT][nI][nJ][nK];
            dTemps[1]=grid.dLocalGridNew[grid.nT][nI+1][nJ][nK];
            dTemps[2]=grid.dLocalGridNew[grid.nT][nI-1][nJ][nK];
            dTemps[3]=grid.dLocalGridNew[grid.nT][nI][nJ+1][nK]*(1.0+implicit.dDerivativeStepFraction);
            dTemps[4]=grid.dLocalGridNew[grid.nT][nI][nJ-1][nK];
            dTemps[5]=grid.dLocalGridNew[grid.nT][nI][nJ][nK+1];
            dTemps[6]=grid.dLocalGridNew[grid.nT][nI][nJ][nK-1];
            dF_ijk_Tjp1=functions.fpImplicitEnergyFunction(grid,parameters,time,dTemps,nI,nJ,nK);
            dTemps[3]=grid.dLocalGridNew[grid.nT][nI][nJ+1][nK];
            dTemps[4]=grid.dLocalGridNew[grid.nT][nI][nJ-1][nK]*(1.0+implicit.dDerivativeStepFraction);
            dF_ijk_Tjm1=functions.fpImplicitEnergyFunction(grid,parameters,time,dTemps,nI,nJ,nK);
            dValues[j]=(dF_ijk_Tjp1-dF_ijk_Tijk)
              /(implicit.dDerivativeStepFraction*grid.dLocalGridNew[grid.nT][nI][nJ+1][nK])
              +(dF_ijk_Tjm1-dF_ijk_Tijk)
              /(implicit.dDerivativeStepFraction*grid.dLocalGridNew[grid.nT][nI][nJ-1][nK]);
            break;
          }
          case 5 :{//calculate derivative of energy equation wrt. T at k+1
            dTemps[0]=grid.dLocalGridNew[grid.nT][nI][nJ][nK];
            dTemps[1]=grid.dLocalGridNew[grid.nT][nI+1][nJ][nK];
            dTemps[2]=grid.dLocalGridNew[grid.nT][nI-1][nJ][nK];
            dTemps[3]=grid.dLocalGridNew[grid.nT][nI][nJ+1][nK];
            dTemps[4]=grid.dLocalGridNew[grid.nT][nI][nJ-1][nK];
            dTemps[5]=grid.dLocalGridNew[grid.nT][nI][nJ][nK+1]*(1.0+implicit.dDerivativeStepFraction);
            dTemps[6]=grid.dLocalGridNew[grid.nT][nI][nJ][nK-1];

            dF_ijk_Tkp1=functions.fpImplicitEnergyFunction(grid,parameters,time,dTemps,nI,nJ,nK);
            dValues[j]=(dF_ijk_Tkp1-dF_ijk_Tijk)
              /(implicit.dDerivativeStepFraction*grid.dLocalGridNew[grid.nT][nI][nJ][nK+1]);
            break;
          }
          case 6 :{//calculate derivative of energy equation wrt. T at k-1
            dTemps[0]=grid.dLocalGridNew[grid.nT][nI][nJ][nK];
            dTemps[1]=grid.dLocalGridNew[grid.nT][nI+1][nJ][nK];
            dTemps[2]=grid.dLocalGridNew[grid.nT][nI-1][nJ][nK];
            dTemps[3]=grid.dLocalGridNew[grid.nT][nI][nJ+1][nK];
            dTemps[4]=grid.dLocalGridNew[grid.nT][nI][nJ-1][nK];
            dTemps[5]=grid.dLocalGridNew[grid.nT][nI][nJ][nK+1];
            dTemps[6]=grid.dLocalGridNew[grid.nT][nI][nJ][nK-1]*(1.0+implicit.dDerivativeStepFraction);
            dF_ijk_Tkm1=functions.fpImplicitEnergyFunction(grid,parameters,time,dTemps,nI,nJ,nK);
            dValues[j]=(dF_ijk_Tkm1-dF_ijk_Tijk)
              /(implicit.dDerivativeStepFraction*grid.dLocalGridNew[grid.nT][nI][nJ][nK-1]);
            break;
          }
          case 56 :{//calculate derivative of energy equation wrt. T at k+1 and k-1
            dTemps[0]=grid.dLocalGridNew[grid.nT][nI][nJ][nK];
            dTemps[1]=grid.dLocalGridNew[grid.nT][nI+1][nJ][nK];
            dTemps[2]=grid.dLocalGridNew[grid.nT][nI-1][nJ][nK];
            dTemps[3]=grid.dLocalGridNew[grid.nT][nI][nJ+1][nK];
            dTemps[4]=grid.dLocalGridNew[grid.nT][nI][nJ-1][nK];
            dTemps[5]=grid.dLocalGridNew[grid.nT][nI][nJ][nK+1]*(1.0+implicit.dDerivativeStepFraction);
            dTemps[6]=grid.dLocalGridNew[grid.nT][nI][nJ][nK-1];
            dF_ijk_Tkp1=functions.fpImplicitEnergyFunction(grid,parameters,time,dTemps,nI,nJ,nK);
            dTemps[5]=grid.dLocalGridNew[grid.nT][nI][nJ][nK+1];
            dTemps[6]=grid.dLocalGridNew[grid.nT][nI][nJ][nK-1]*(1.0+implicit.dDerivativeStepFraction);
            dF_ijk_Tkm1=functions.fpImplicitEnergyFunction(grid,parameters,time,dTemps,nI,nJ,nK);
            dValues[j]=(dF_ijk_Tkp1-dF_ijk_Tijk)
              /(implicit.dDerivativeStepFraction*grid.dLocalGridNew[grid.nT][nI][nJ][nK+1])
              +(dF_ijk_Tkm1-dF_ijk_Tijk)
              /(implicit.dDerivativeStepFraction*grid.dLocalGridNew[grid.nT][nI][nJ][nK-1]);
            break;
          }
        }
      }
      MatSetValues(
        implicit.matCoeff,//matrix to set values in
        1,//number or rows
        &implicit.nLocDer[i][0][0],//global index of rows
        implicit.nNumDerPerRow[i],//number of columns
        implicit.nLocDer[i][1],//global index of column
        dValues,//logically two-dimensional array of values
        INSERT_VALUES);
      delete [] dValues;
    }
    
    //calculate at surface
    for(int i=implicit.nNumRowsALocal;i<implicit.nNumRowsALocal+implicit.nNumRowsALocalSB;i++){//for each row
      nI=implicit.nLocFun[i][0];
      nJ=implicit.nLocFun[i][1];
      nK=implicit.nLocFun[i][2];
      dTemps[0]=grid.dLocalGridNew[grid.nT][nI][nJ][nK];
      dTemps[1]=grid.dLocalGridNew[grid.nT][nI-1][nJ][nK];
      dTemps[2]=grid.dLocalGridNew[grid.nT][nI][nJ+1][nK];
      dTemps[3]=grid.dLocalGridNew[grid.nT][nI][nJ-1][nK];
      dTemps[4]=grid.dLocalGridNew[grid.nT][nI][nJ][nK+1];
      dTemps[5]=grid.dLocalGridNew[grid.nT][nI][nJ][nK-1];
      dF_ijk_Tijk=functions.fpImplicitEnergyFunction_SB(grid,parameters,time,dTemps,nI,nJ,nK);
      dValuesRHS[i]=-1.0*dF_ijk_Tijk;
      nIndicesRHS[i]=implicit.nLocDer[i][0][0];
      dValues=new double[implicit.nNumDerPerRow[i]];
      for(int j=0;j<implicit.nNumDerPerRow[i];j++){//for each derivative
        
        switch(implicit.nTypeDer[i][j]){
          case 0 :{//calculate derivative of energy equation wrt. T at i
            dTemps[0]=grid.dLocalGridNew[grid.nT][nI][nJ][nK]*(1.0+implicit.dDerivativeStepFraction);
            dTemps[1]=grid.dLocalGridNew[grid.nT][nI-1][nJ][nK];
            dTemps[2]=grid.dLocalGridNew[grid.nT][nI][nJ+1][nK];
            dTemps[3]=grid.dLocalGridNew[grid.nT][nI][nJ-1][nK];
            dTemps[4]=grid.dLocalGridNew[grid.nT][nI][nJ][nK+1];
            dTemps[5]=grid.dLocalGridNew[grid.nT][nI][nJ][nK-1];
            dF_ijk_Ti1=functions.fpImplicitEnergyFunction_SB(grid,parameters,time,dTemps,nI,nJ,nK);
            dValues[j]=(dF_ijk_Ti1-dF_ijk_Tijk)
              /(implicit.dDerivativeStepFraction*grid.dLocalGridNew[grid.nT][nI][nJ][nK]);
            break;
          }
          //case 1 : no i+1 at surface
          case 2 :{//calculate derivative of energy equation wrt. T at i-1
            dTemps[0]=grid.dLocalGridNew[grid.nT][nI][nJ][nK];
            dTemps[1]=grid.dLocalGridNew[grid.nT][nI-1][nJ][nK]*(1.0+implicit.dDerivativeStepFraction);
            dTemps[2]=grid.dLocalGridNew[grid.nT][nI][nJ+1][nK];
            dTemps[3]=grid.dLocalGridNew[grid.nT][nI][nJ-1][nK];
            dTemps[4]=grid.dLocalGridNew[grid.nT][nI][nJ][nK+1];
            dTemps[5]=grid.dLocalGridNew[grid.nT][nI][nJ][nK-1];
            dF_ijk_Tim1=functions.fpImplicitEnergyFunction_SB(grid,parameters,time,dTemps,nI,nJ,nK);
            dValues[j]=(dF_ijk_Tim1-dF_ijk_Tijk)
              /(implicit.dDerivativeStepFraction*grid.dLocalGridNew[grid.nT][nI-1][nJ][nK]);
            break;
          }
          case 3 :{//calculate derivative of energy equation wrt. T at j+1
            dTemps[0]=grid.dLocalGridNew[grid.nT][nI][nJ][nK];
            dTemps[1]=grid.dLocalGridNew[grid.nT][nI-1][nJ][nK];
            dTemps[2]=grid.dLocalGridNew[grid.nT][nI][nJ+1][nK]*(1.0+implicit.dDerivativeStepFraction);
            dTemps[3]=grid.dLocalGridNew[grid.nT][nI][nJ-1][nK];
            dTemps[4]=grid.dLocalGridNew[grid.nT][nI][nJ][nK+1];
            dTemps[5]=grid.dLocalGridNew[grid.nT][nI][nJ][nK-1];
            dF_ijk_Tjp1=functions.fpImplicitEnergyFunction_SB(grid,parameters,time,dTemps,nI,nJ,nK);
            dValues[j]=(dF_ijk_Tjp1-dF_ijk_Tijk)
              /(implicit.dDerivativeStepFraction*grid.dLocalGridNew[grid.nT][nI][nJ+1][nK]);
            break;
          }
          case 4 :{//calculate derivative of energy equation wrt. T at j-1
            dTemps[0]=grid.dLocalGridNew[grid.nT][nI][nJ][nK];
            dTemps[1]=grid.dLocalGridNew[grid.nT][nI-1][nJ][nK];
            dTemps[2]=grid.dLocalGridNew[grid.nT][nI][nJ+1][nK];
            dTemps[3]=grid.dLocalGridNew[grid.nT][nI][nJ-1][nK]*(1.0+implicit.dDerivativeStepFraction);
            dTemps[4]=grid.dLocalGridNew[grid.nT][nI][nJ][nK+1];
            dTemps[5]=grid.dLocalGridNew[grid.nT][nI][nJ][nK-1];
            dF_ijk_Tjm1=functions.fpImplicitEnergyFunction_SB(grid,parameters,time,dTemps,nI,nJ,nK);
            dValues[j]=(dF_ijk_Tjm1-dF_ijk_Tijk)
              /(implicit.dDerivativeStepFraction*grid.dLocalGridNew[grid.nT][nI][nJ-1][nK]);
            break;
          }
          case 34 :{//calculate derivative of energy equation wrt. T at j+1 and j-1
            dTemps[0]=grid.dLocalGridNew[grid.nT][nI][nJ][nK];
            dTemps[1]=grid.dLocalGridNew[grid.nT][nI-1][nJ][nK];
            dTemps[2]=grid.dLocalGridNew[grid.nT][nI][nJ+1][nK]*(1.0+implicit.dDerivativeStepFraction);
            dTemps[3]=grid.dLocalGridNew[grid.nT][nI][nJ-1][nK];
            dTemps[4]=grid.dLocalGridNew[grid.nT][nI][nJ][nK+1];
            dTemps[5]=grid.dLocalGridNew[grid.nT][nI][nJ][nK-1];
            dF_ijk_Tjp1=functions.fpImplicitEnergyFunction_SB(grid,parameters,time,dTemps,nI,nJ,nK);
            dTemps[2]=grid.dLocalGridNew[grid.nT][nI][nJ+1][nK];
            dTemps[3]=grid.dLocalGridNew[grid.nT][nI][nJ-1][nK]*(1.0+implicit.dDerivativeStepFraction);
            dF_ijk_Tjm1=functions.fpImplicitEnergyFunction_SB(grid,parameters,time,dTemps,nI,nJ,nK);
            dValues[j]=(dF_ijk_Tjp1-dF_ijk_Tijk)
              /(implicit.dDerivativeStepFraction*grid.dLocalGridNew[grid.nT][nI][nJ+1][nK])
              +(dF_ijk_Tjm1-dF_ijk_Tijk)
              /(implicit.dDerivativeStepFraction*grid.dLocalGridNew[grid.nT][nI][nJ-1][nK]);
            break;
          }
          case 5 :{//calculate derivative of energy equation wrt. T at k+1
            dTemps[0]=grid.dLocalGridNew[grid.nT][nI][nJ][nK];
            dTemps[1]=grid.dLocalGridNew[grid.nT][nI-1][nJ][nK];
            dTemps[2]=grid.dLocalGridNew[grid.nT][nI][nJ+1][nK];
            dTemps[3]=grid.dLocalGridNew[grid.nT][nI][nJ-1][nK];
            dTemps[4]=grid.dLocalGridNew[grid.nT][nI][nJ][nK+1]*(1.0+implicit.dDerivativeStepFraction);
            dTemps[5]=grid.dLocalGridNew[grid.nT][nI][nJ][nK-1];
            dF_ijk_Tkp1=functions.fpImplicitEnergyFunction_SB(grid,parameters,time,dTemps,nI,nJ,nK);
            dValues[j]=(dF_ijk_Tkp1-dF_ijk_Tijk)
              /(implicit.dDerivativeStepFraction*grid.dLocalGridNew[grid.nT][nI][nJ][nK+1]);
            break;
          }
          case 6 :{//calculate derivative of energy equation wrt. T at k-1
            dTemps[0]=grid.dLocalGridNew[grid.nT][nI][nJ][nK];
            dTemps[1]=grid.dLocalGridNew[grid.nT][nI-1][nJ][nK];
            dTemps[2]=grid.dLocalGridNew[grid.nT][nI][nJ+1][nK];
            dTemps[3]=grid.dLocalGridNew[grid.nT][nI][nJ-1][nK];
            dTemps[4]=grid.dLocalGridNew[grid.nT][nI][nJ][nK+1];
            dTemps[5]=grid.dLocalGridNew[grid.nT][nI][nJ][nK-1]*(1.0+implicit.dDerivativeStepFraction);
            dF_ijk_Tkm1=functions.fpImplicitEnergyFunction_SB(grid,parameters,time,dTemps,nI,nJ,nK);
            dValues[j]=(dF_ijk_Tkm1-dF_ijk_Tijk)
              /(implicit.dDerivativeStepFraction*grid.dLocalGridNew[grid.nT][nI][nJ][nK-1]);
            break;
          }
          case 56 :{//calculate derivative of energy equation wrt. T at k+1 and k-1
            dTemps[0]=grid.dLocalGridNew[grid.nT][nI][nJ][nK];
            dTemps[1]=grid.dLocalGridNew[grid.nT][nI-1][nJ][nK];
            dTemps[2]=grid.dLocalGridNew[grid.nT][nI][nJ+1][nK];
            dTemps[3]=grid.dLocalGridNew[grid.nT][nI][nJ-1][nK];
            dTemps[4]=grid.dLocalGridNew[grid.nT][nI][nJ][nK+1]*(1.0+implicit.dDerivativeStepFraction);
            dTemps[5]=grid.dLocalGridNew[grid.nT][nI][nJ][nK-1];
            dF_ijk_Tkp1=functions.fpImplicitEnergyFunction_SB(grid,parameters,time,dTemps,nI,nJ,nK);
            dTemps[4]=grid.dLocalGridNew[grid.nT][nI][nJ][nK+1];
            dTemps[5]=grid.dLocalGridNew[grid.nT][nI][nJ][nK-1]*(1.0+implicit.dDerivativeStepFraction);
            dF_ijk_Tkm1=functions.fpImplicitEnergyFunction_SB(grid,parameters,time,dTemps,nI,nJ,nK);
            dValues[j]=(dF_ijk_Tkp1-dF_ijk_Tijk)
              /(implicit.dDerivativeStepFraction*grid.dLocalGridNew[grid.nT][nI][nJ][nK+1])
              +(dF_ijk_Tkm1-dF_ijk_Tijk)
              /(implicit.dDerivativeStepFraction*grid.dLocalGridNew[grid.nT][nI][nJ][nK-1]);
            break;
          }
        }
      }
      MatSetValues(
        implicit.matCoeff,//matrix to set values in
        1,//number or rows
        &implicit.nLocDer[i][0][0],//global index of rows
        implicit.nNumDerPerRow[i],//number of columns
        implicit.nLocDer[i][1],//global index of column
        dValues,//logically two-dimensional array of values
        INSERT_VALUES);
      delete [] dValues;
    }
    
    //assemble coeffecient matrix
    MatAssemblyBegin(implicit.matCoeff,MAT_FINAL_ASSEMBLY);
    
    //set values of the RHS
    VecSetValues(implicit.vecRHS
      ,implicit.nNumRowsALocal+implicit.nNumRowsALocalSB
      ,nIndicesRHS
      ,dValuesRHS
      ,INSERT_VALUES);
    
    VecAssemblyBegin(implicit.vecRHS);
    VecAssemblyEnd(implicit.vecRHS);
    MatAssemblyEnd(implicit.matCoeff,MAT_FINAL_ASSEMBLY);
    
    //print out RHS
    //VecView(implicit.vecRHS,PETSC_VIEWER_STDOUT_WORLD);
    
    //print out matrix
    //MatView(implicit.matCoeff,PETSC_VIEWER_STDOUT_WORLD);
    
    //solve system
    KSPSetOperators(implicit.kspContext,implicit.matCoeff,implicit.matCoeff,SAME_NONZERO_PATTERN);
    KSPSolve(implicit.kspContext,implicit.vecRHS,implicit.vecTCorrections);
    //print out solution
    //VecView(implicit.vecTCorrections,PETSC_VIEWER_STDOUT_WORLD);
    
    //get distributed corrections
    VecScatterBegin(implicit.vecscatTCorrections,implicit.vecTCorrections
      ,implicit.vecTCorrectionsLocal,INSERT_VALUES,SCATTER_FORWARD);
    VecScatterEnd(implicit.vecscatTCorrections,implicit.vecTCorrections
      ,implicit.vecTCorrectionsLocal,INSERT_VALUES,SCATTER_FORWARD);
    VecGetArray(implicit.vecTCorrectionsLocal,&dValues);
    
    //apply corrections
    dRelTErrorLocal=0.0;
    for(int i=0;i<implicit.nNumRowsALocal+implicit.nNumRowsALocalSB;i++){
      nI=implicit.nLocFun[i][0];
      nJ=implicit.nLocFun[i][1];
      nK=implicit.nLocFun[i][2];
      grid.dLocalGridNew[grid.nT][nI][nJ][nK]+=dValues[i];
      if(grid.dLocalGridNew[grid.nT][nI][nJ][nK]<0.0){
        
        #if SIGNEGTEMP==1
        raise(SIGINT);
        #endif
        
        std::stringstream ssTemp;
        ssTemp<<__FILE__<<":"<<__FUNCTION__<<":"<<__LINE__<<":"<<procTop.nRank
          <<": negative temperature calculated in , ("<<nI<<","<<nJ<<","<<nK<<") on iteration "
          <<nNumIterations<<"\n";
        throw exception2(ssTemp.str(),CALCULATION);
        
      }
      dTemp2=fabs(dValues[i]/grid.dLocalGridNew[grid.nT][nI][nJ][nK]);
      if(dRelTErrorLocal<dTemp2){
        dRelTErrorLocal=dTemp2;
      }
    }
    
    updateLocalBoundariesNewGrid(grid.nT,procTop,messPass,grid);
    
    MPI::COMM_WORLD.Allreduce(&dRelTErrorLocal,&dRelTError,1,MPI::DOUBLE,MPI_MAX);
    
    VecRestoreArray(implicit.vecTCorrectionsLocal,&dValues);
    nNumIterations++;
  }
  
  #if TRACKMAXSOLVERERROR==1
    
    /* Calculate absolute error in solver*/
    Vec vecCalRHS1;
    Vec vecCalRHS2;
    VecDuplicate(implicit.vecRHS,&vecCalRHS1);
    VecDuplicate(implicit.vecRHS,&vecCalRHS2);
    MatMult(implicit.matCoeff,implicit.vecTCorrections,vecCalRHS1);
    VecCopy(vecCalRHS1,vecCalRHS2);
    
    VecAXPY(vecCalRHS1,-1.0,implicit.vecRHS);
    
    //get maximum absolute error, and average value of the RHS
    int nIndexLargestError;
    double dMaxError;
    VecMax(vecCalRHS1,&nIndexLargestError,&dMaxError);
    dMaxError=fabs(dMaxError);
    if(dMaxError>implicit.dMaxErrorInRHS){
      implicit.dMaxErrorInRHS=dMaxError;
      double dSumRHS=0.0;
      VecAbs(vecCalRHS2);
      VecSum(vecCalRHS2,&dSumRHS);
      int nSizeVecCalRHS;
      VecGetSize(vecCalRHS2,&nSizeVecCalRHS);
      implicit.dAverageRHS=dSumRHS/double(nSizeVecCalRHS);
    }
    
    //keep track of largest number of interations the solver took
    int nNumIterationsSolve;
    KSPGetIterationNumber(implicit.kspContext,&nNumIterationsSolve);
    if(nNumIterationsSolve>implicit.nMaxNumSolverIterations){
      implicit.nMaxNumSolverIterations=nNumIterationsSolve;
    }
  #endif
  
  delete [] dValuesRHS;
  delete [] nIndicesRHS;
  if(dRelTError>implicit.dCurrentRelTError){
    implicit.dCurrentRelTError=dRelTError;
  }
  if(nNumIterations>implicit.nCurrentNumIterations){
    implicit.nCurrentNumIterations=nNumIterations;
  }
  
  if(procTop.nRank==0){
    if(nNumIterations>=implicit.nMaxNumIterations){
      std::cout<<__FILE__<<":"<<__FUNCTION__<<":"<<__LINE__<<":"<<procTop.nRank
      <<": The maximum number of iteration for implicit solution ("<<implicit.nMaxNumIterations
      <<") has be exceeded in current time step ("<<time.nTimeStepIndex
      <<") with a maximum relative error in the implicit calculation of temperature of "
      <<implicit.dCurrentRelTError<<std::endl;
    }
  }
  
  //calculate E, Kappa, P form new temperature
  calNewPEKappaGamma_TEOS(grid,parameters);
}
double dImplicitEnergyFunction_None(Grid &grid,Parameters &parameters,Time &time,double dTemps[]
  ,int i,int j,int k){
  /*this is an empty funciton that is not even called when no implicit calculation is done. This is 
  simply here incase the off chance that some point in the future it is called when no implicit 
  calculation is being done.
  */
  return 0.0;
}
double dImplicitEnergyFunction_R(Grid &grid,Parameters &parameters,Time &time,double dTemps[]
  ,int i,int j,int k){
  
  double dT_ijk_np1=dTemps[0];
  double dT_ip1jk_np1=dTemps[1];
  double dT_im1jk_np1=dTemps[2];
  //this needs to be properly time centered for T, E, Kappa, and P, and E^n+1 needs to brought over
  //to the other side.
  //Calculate interpolated quantities
  int nIInt=i+grid.nCenIntOffset[0];
  double dU_ijk_np1half=(grid.dLocalGridNew[grid.nU][nIInt][j][k]
    +grid.dLocalGridNew[grid.nU][nIInt-1][j][k])*0.5;
  double dU0_i_np1half=(grid.dLocalGridNew[grid.nU0][nIInt][0][0]
    +grid.dLocalGridNew[grid.nU0][nIInt-1][0][0])*0.5;
  double dR_i_n=(grid.dLocalGridOld[grid.nR][nIInt][0][0]
    +grid.dLocalGridOld[grid.nR][nIInt-1][0][0])*0.5;
  double dR_im1half_n=grid.dLocalGridOld[grid.nR][nIInt-1][0][0];
  double dR_ip1half_n=grid.dLocalGridOld[grid.nR][nIInt][0][0];
  double dRSq_i_n=dR_i_n*dR_i_n;
  double dRSq_ip1half=dR_ip1half_n*dR_ip1half_n;
  double dR4_ip1half=dRSq_ip1half*dRSq_ip1half;
  double dR_im1half_sq=dR_im1half_n*dR_im1half_n;
  double dR_im1half_4=dR_im1half_sq*dR_im1half_sq;
  double dRhoAve_ip1half=(grid.dLocalGridOld[grid.nD][i+1][0][0]
    +grid.dLocalGridOld[grid.nD][i][0][0])*0.5;
  double dRhoAve_im1half=(grid.dLocalGridOld[grid.nD][i][0][0]
    +grid.dLocalGridOld[grid.nD][i-1][0][0])*0.5;
  double dRho_ip1halfjk=(grid.dLocalGridOld[grid.nD][i+1][j][k]
    +grid.dLocalGridOld[grid.nD][i][j][k])*0.5;
  double dRho_im1halfjk=(grid.dLocalGridOld[grid.nD][i][j][k]
    +grid.dLocalGridOld[grid.nD][i-1][j][k])*0.5;
  
  double dT_ip1jk_np1half=(dT_ip1jk_np1+grid.dLocalGridOld[grid.nT][i+1][j][k])*0.5;
  double dT_ip1jk_np1half_sq=dT_ip1jk_np1half*dT_ip1jk_np1half;
  double dT_ip1jk_np1half_4=dT_ip1jk_np1half_sq*dT_ip1jk_np1half_sq;
  
  double dT_ijk_np1half=(dT_ijk_np1+grid.dLocalGridOld[grid.nT][i][j][k])*0.5;
  double dT_ijk_np1half_sq=dT_ijk_np1half*dT_ijk_np1half;
  double dT_ijk_np1half_4=dT_ijk_np1half_sq*dT_ijk_np1half_sq;
  
  double dT_im1jk_np1half=(dT_im1jk_np1+grid.dLocalGridOld[grid.nT][i-1][j][k])*0.5;
  double dT_im1jk_np1half_sq=dT_im1jk_np1half*dT_im1jk_np1half;
  double dT_im1jk_np1half_4=dT_im1jk_np1half_sq*dT_im1jk_np1half_sq;
  
  double dE_ijk_np1=parameters.eosTable.dGetEnergy(dT_ijk_np1
    ,grid.dLocalGridNew[grid.nD][i][j][k]);
  double dE_ip1jk_np1half=parameters.eosTable.dGetEnergy(dT_ip1jk_np1half
    ,grid.dLocalGridOld[grid.nD][i+1][j][k]);
  double dE_ijk_np1half=parameters.eosTable.dGetEnergy(dT_ijk_np1half
    ,grid.dLocalGridOld[grid.nD][i][j][k]);
  double dE_im1jk_np1half=parameters.eosTable.dGetEnergy(dT_im1jk_np1half
    ,grid.dLocalGridOld[grid.nD][i-1][j][k]);
  
  double dE_ip1halfjk_np1half=(dE_ip1jk_np1half+dE_ijk_np1half)*0.5;
  double dE_im1halfjk_np1half=(dE_ijk_np1half+dE_im1jk_np1half)*0.5;
  
  double dPi_ijk_np1half=parameters.eosTable.dGetPressure(dT_ijk_np1half
    ,grid.dLocalGridOld[grid.nD][i][j][k])+grid.dLocalGridOld[grid.nQ0][i][j][k];
  
  double dKappa_ip1jk_np1half=parameters.eosTable.dGetOpacity(dT_ip1jk_np1half
    ,grid.dLocalGridOld[grid.nD][i+1][j][k]);
  double dKappa_ijk_np1half=parameters.eosTable.dGetOpacity(dT_ijk_np1half
    ,grid.dLocalGridOld[grid.nD][i][j][k]);
  double dKappa_im1jk_np1half=parameters.eosTable.dGetOpacity(dT_im1jk_np1half
    ,grid.dLocalGridOld[grid.nD][i-1][j][k]);
  
  double dKappa_ip1halfjk_n_np1half=(dT_ip1jk_np1half_4+dT_ijk_np1half_4)
    /(dT_ijk_np1half_4/dKappa_ijk_np1half+dT_ip1jk_np1half_4/dKappa_ip1jk_np1half);
  double dKappa_im1halfjk_n_np1half=(dT_im1jk_np1half_4+dT_ijk_np1half_4)
    /(dT_ijk_np1half_4/dKappa_ijk_np1half+dT_im1jk_np1half_4/dKappa_im1jk_np1half);
    
  //Calcuate dA1
  double dA1CenGrad=(dE_ip1halfjk_np1half-dE_im1halfjk_np1half)
    /grid.dLocalGridOld[grid.nDM][i][0][0];
  double dA1UpWindGrad=0.0;
  double dU_U0_Diff=(dU_ijk_np1half-dU0_i_np1half);
  if(dU_U0_Diff<0.0){//moving in the negative direction
    dA1UpWindGrad=(dE_ip1jk_np1half-dE_ijk_np1half)/(grid.dLocalGridOld[grid.nDM][i+1][0][0]
      +grid.dLocalGridOld[grid.nDM][i][0][0])*2.0;
  }
  else{//moving in the postive direction
    dA1UpWindGrad=(dE_ijk_np1half-dE_im1jk_np1half)/(grid.dLocalGridOld[grid.nDM][i][0][0]
      +grid.dLocalGridOld[grid.nDM][i-1][0][0])*2.0;
  }
  double dA1=dU_U0_Diff*dRSq_i_n*((1.0-parameters.dDonorFrac)*dA1CenGrad
    +parameters.dDonorFrac*dA1UpWindGrad);
  
  //calculate dS1
  double dUR2_im1half_np1half=grid.dLocalGridNew[grid.nU][nIInt-1][j][k]*dR_im1half_n
    *dR_im1half_n;
  double dUR2_ip1half_np1half=grid.dLocalGridNew[grid.nU][nIInt][j][k]*dR_ip1half_n
    *dR_ip1half_n;
  double dS1=dPi_ijk_np1half/grid.dLocalGridOld[grid.nD][i][j][k]
    *(dUR2_ip1half_np1half-dUR2_im1half_np1half)/grid.dLocalGridOld[grid.nDM][i][0][0];
  
  //Calculate dS4
  double dTGrad_ip1half_np1half=(dT_ip1jk_np1half_4-dT_ijk_np1half_4)
    /(grid.dLocalGridOld[grid.nDM][i+1][0][0]+grid.dLocalGridOld[grid.nDM][i][0][0])*2.0;
  double dTGrad_im1half_np1half=(dT_ijk_np1half_4-dT_im1jk_np1half_4)
    /(grid.dLocalGridOld[grid.nDM][i][0][0]+grid.dLocalGridOld[grid.nDM][i-1][0][0])*2.0;
  double dGrad_ip1half_np1half=dRhoAve_ip1half*dR4_ip1half
    /(dKappa_ip1halfjk_n_np1half*dRho_ip1halfjk)*dTGrad_ip1half_np1half;
  double dGrad_im1half_np1half=dRhoAve_im1half*dR_im1half_4
    /(dKappa_im1halfjk_n_np1half*dRho_im1halfjk)*dTGrad_im1half_np1half;
  double dS4=16.0*parameters.dPi*parameters.dPi*grid.dLocalGridOld[grid.nD][i][0][0]
    *(dGrad_ip1half_np1half-dGrad_im1half_np1half)/grid.dLocalGridOld[grid.nDM][i][0][0];
  
  //calculate new energy
  return (dE_ijk_np1-grid.dLocalGridOld[grid.nE][i][j][k])/time.dDeltat_n
    +4.0*parameters.dPi*grid.dLocalGridOld[grid.nD][i][0][0]*(dA1+dS1)
    -4.0*parameters.dSigma/(3.0*grid.dLocalGridOld[grid.nD][i][j][k])*(dS4);
}
double dImplicitEnergyFunction_R_SB(Grid &grid,Parameters &parameters,Time &time,double dTemps[]
  ,int i,int j,int k){
  
  double dT_ijk_np1=dTemps[0];
  double dT_im1jk_np1=dTemps[1];
  int nIInt=i+grid.nCenIntOffset[0];
  
  //Calculate interpolated quantities
  double dU_ijk_np1half=(grid.dLocalGridNew[grid.nU][nIInt][j][k]
    +grid.dLocalGridNew[grid.nU][nIInt-1][j][k])*0.5;
  double dU0_i_np1half=(grid.dLocalGridNew[grid.nU0][nIInt][0][0]
    +grid.dLocalGridNew[grid.nU0][nIInt-1][0][0])*0.5;
  double dR_i_n=(grid.dLocalGridOld[grid.nR][nIInt][0][0]
    +grid.dLocalGridOld[grid.nR][nIInt-1][0][0])*0.5;
  double dR_im1half_n=grid.dLocalGridOld[grid.nR][nIInt-1][0][0];
  double dR_ip1half_n=grid.dLocalGridOld[grid.nR][nIInt][0][0];
  double dRSq_i_n=dR_i_n*dR_i_n;
  double dRSq_ip1half=dR_ip1half_n*dR_ip1half_n;
  double dR_im1half_sq=dR_im1half_n*dR_im1half_n;
  double dR_im1half_4=dR_im1half_sq*dR_im1half_sq;
  double dRhoAve_im1half=(grid.dLocalGridOld[grid.nD][i][0][0]
    +grid.dLocalGridOld[grid.nD][i-1][0][0])*0.5;
  double dRho_im1halfjk=(grid.dLocalGridOld[grid.nD][i][j][k]
    +grid.dLocalGridOld[grid.nD][i-1][j][k])*0.5;
  
  double dT_ijk_np1half=(dT_ijk_np1+grid.dLocalGridOld[grid.nT][i][j][k])*0.5;
  double dT_ijk_np1half_sq=dT_ijk_np1half*dT_ijk_np1half;
  double dT_ijk_np1half_4=dT_ijk_np1half_sq*dT_ijk_np1half_sq;
  
  double dT_im1jk_np1half=(dT_im1jk_np1+grid.dLocalGridOld[grid.nT][i-1][j][k])*0.5;
  double dT_im1jk_np1half_sq=dT_im1jk_np1half*dT_im1jk_np1half;
  double dT_im1jk_np1half_4=dT_im1jk_np1half_sq*dT_im1jk_np1half_sq;
  
  double dE_ijk_np1=parameters.eosTable.dGetEnergy(dT_ijk_np1
    ,grid.dLocalGridNew[grid.nD][i][j][k]);
  double dE_ijk_np1half=parameters.eosTable.dGetEnergy(dT_ijk_np1half
    ,grid.dLocalGridOld[grid.nD][i][j][k]);
  double dE_im1jk_np1half=parameters.eosTable.dGetEnergy(dT_im1jk_np1half
    ,grid.dLocalGridOld[grid.nD][i][j][k]);
  
  double dE_ip1halfjk_np1half=dE_ijk_np1half;/**\BC Missing
    grid.dLocalGridOld[grid.nE][i+1][j][k] in calculation of \f$E_{i+1/2,j,k}\f$ 
    setting it equal to value at i.*/
  double dE_im1halfjk_np1half=(dE_ijk_np1half+dE_im1jk_np1half)*0.5;
  
  double dKappa_ijk_np1half=parameters.eosTable.dGetOpacity(dT_ijk_np1half
    ,grid.dLocalGridOld[grid.nD][i][j][k]);
  double dKappa_im1jk_np1half=parameters.eosTable.dGetOpacity(dT_im1jk_np1half
    ,grid.dLocalGridOld[grid.nD][i-1][j][k]);
  
  double dKappa_im1halfjk_n_np1half=(dT_im1jk_np1half_4+dT_ijk_np1half_4)
    /(dT_ijk_np1half_4/dKappa_ijk_np1half+dT_im1jk_np1half_4/dKappa_im1jk_np1half);
    
  double dP_ijk_np1half=parameters.eosTable.dGetPressure(dT_ijk_np1half
    ,grid.dLocalGridOld[grid.nD][i][j][k]);
  
  //Calcuate dA1
  double dA1CenGrad=(dE_ip1halfjk_np1half-dE_im1halfjk_np1half)
    /grid.dLocalGridOld[grid.nDM][i][0][0];
  double dA1UpWindGrad=0.0;
  double dU_U0_Diff=(dU_ijk_np1half-dU0_i_np1half);
  if(dU_U0_Diff<0.0){//moving in the negative direction
    dA1UpWindGrad=dA1CenGrad;/**\BC grid.dLocalGridOld[grid.nDM][i+1][0][0] and 
      grid.dLocalGridOld[grid.nE][i+1][j][k] missing in the calculation of upwind gradient in 
      dA1. Using the centered gradient instead.*/
  }
  else{//moving in the postive direction*/
    dA1UpWindGrad=(dE_ijk_np1half-dE_im1jk_np1half)/(grid.dLocalGridOld[grid.nDM][i][0][0]
      +grid.dLocalGridOld[grid.nDM][i-1][0][0])*2.0;
  }
  double dA1=dU_U0_Diff*dRSq_i_n*((1.0-parameters.dDonorFrac)*dA1CenGrad
    +parameters.dDonorFrac*dA1UpWindGrad);
  
  //calculate dS1
  double dUR2_im1half_np1half=grid.dLocalGridNew[grid.nU][nIInt-1][j][k]*dR_im1half_n
    *dR_im1half_n;
  double dUR2_ip1half_np1half=grid.dLocalGridNew[grid.nU][nIInt][j][k]*dR_ip1half_n
    *dR_ip1half_n;
  double dS1=dP_ijk_np1half/grid.dLocalGridOld[grid.nD][i][j][k]
    *(dUR2_ip1half_np1half-dUR2_im1half_np1half)/grid.dLocalGridOld[grid.nDM][i][0][0];
  
  //Calculate dS4
  double dTGrad_im1half_np1half=(dT_ijk_np1half_4-dT_im1jk_np1half_4)
    /(grid.dLocalGridOld[grid.nDM][i][0][0]+grid.dLocalGridOld[grid.nDM][i-1][0][0])*2.0;
  double dGrad_ip1half_np1half=-3.0*dRSq_ip1half*dT_ijk_np1half_4/(8.0*parameters.dPi);/**\BC
    Missing grid.dLocalGridOld[grid.nT][i+1][0][0]*/
  double dGrad_im1half_np1half=dRhoAve_im1half*dR_im1half_4/(dKappa_im1halfjk_n_np1half
    *dRho_im1halfjk)*dTGrad_im1half_np1half;
  double dS4=16.0*parameters.dPi*parameters.dPi*grid.dLocalGridOld[grid.nD][i][0][0]
    *(dGrad_ip1half_np1half-dGrad_im1half_np1half)/grid.dLocalGridOld[grid.nDM][i][0][0];
  
  //calculate new energy
  return (dE_ijk_np1-grid.dLocalGridOld[grid.nE][i][j][k])/time.dDeltat_n
    +(4.0*parameters.dPi*grid.dLocalGridOld[grid.nD][i][0][0]*(dA1+dS1)
    -4.0*parameters.dSigma/(3.0*grid.dLocalGridOld[grid.nD][i][j][k])*(dS4));
  
}
double dImplicitEnergyFunction_RT(Grid &grid,Parameters &parameters,Time &time,double dTemps[]
  ,int i,int j,int k){
  
  double dT_ijk_np1=dTemps[0];
  double dT_ip1jk_np1=dTemps[1];
  double dT_im1jk_np1=dTemps[2];
  double dT_ijp1k_np1=dTemps[3];
  double dT_ijm1k_np1=dTemps[4];
  
  //calculate i for interface centered quantities
  int nIInt=i+grid.nCenIntOffset[0];
  int nJInt=j+grid.nCenIntOffset[1];
  
  //Calculate interpolated quantities
  double dU_ijk_np1half=(grid.dLocalGridNew[grid.nU][nIInt][j][k]+grid.dLocalGridNew[grid.nU][nIInt-1][j][k])*0.5;
  double dU0_i_np1half=(grid.dLocalGridNew[grid.nU0][nIInt][0][0]+grid.dLocalGridNew[grid.nU0][nIInt-1][0][0])*0.5;
  double dR_i_n=(grid.dLocalGridOld[grid.nR][nIInt][0][0]+grid.dLocalGridOld[grid.nR][nIInt-1][0][0])*0.5;
  double dR_im1half_n=grid.dLocalGridOld[grid.nR][nIInt-1][0][0];
  double dR_ip1half_n=grid.dLocalGridOld[grid.nR][nIInt][0][0];
  double dRSq_i_n=dR_i_n*dR_i_n;
  double dRSq_ip1half=dR_ip1half_n*dR_ip1half_n;
  double dR4_ip1half=dRSq_ip1half*dRSq_ip1half;
  double dRSq_im1half=dR_im1half_n*dR_im1half_n;
  double dR4_im1half=dRSq_im1half*dRSq_im1half;
  double dV_ijk_np1half=(grid.dLocalGridNew[grid.nV][i][nJInt][k]+grid.dLocalGridNew[grid.nV][i][nJInt-1][k])*0.5;
  double dVSinTheta_ijp1halfk_np1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt][0]*grid.dLocalGridNew[grid.nV][i][nJInt][k];
  double dVSinTheta_ijm1halfk_np1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt-1][0]*grid.dLocalGridNew[grid.nV][i][nJInt-1][k];
  double dRhoAve_ip1half=(grid.dLocalGridOld[grid.nDenAve][i+1][0][0]+grid.dLocalGridOld[grid.nDenAve][i][0][0])*0.5;
  double dRhoAve_im1half=(grid.dLocalGridOld[grid.nDenAve][i][0][0]+grid.dLocalGridOld[grid.nDenAve][i-1][0][0])*0.5;
  double dRho_ip1halfjk=(grid.dLocalGridOld[grid.nD][i+1][j][k]+grid.dLocalGridOld[grid.nD][i][j][k])*0.5;
  double dRho_im1halfjk=(grid.dLocalGridOld[grid.nD][i][j][k]+grid.dLocalGridOld[grid.nD][i-1][j][k])*0.5;
  double dRho_ijp1halfk=(grid.dLocalGridOld[grid.nD][i][j+1][k]+grid.dLocalGridOld[grid.nD][i][j][k])*0.5;
  double dRho_ijm1halfk=(grid.dLocalGridOld[grid.nD][i][j][k]+grid.dLocalGridOld[grid.nD][i][j-1][k])*0.5;
  
  double dT_ip1jk_np1half   =(dT_ip1jk_np1+grid.dLocalGridOld[grid.nT][i+1][j][k])*0.5;
  double dT_ip1jk_np1half_sq= dT_ip1jk_np1half   *dT_ip1jk_np1half;
  double dT_ip1jk_np1half_4 = dT_ip1jk_np1half_sq*dT_ip1jk_np1half_sq;
  
  double dT_ijk_np1half   =(dT_ijk_np1+grid.dLocalGridOld[grid.nT][i][j][k])*0.5;
  double dT_ijk_np1half_sq= dT_ijk_np1half   *dT_ijk_np1half;
  double dT_ijk_np1half_4 = dT_ijk_np1half_sq*dT_ijk_np1half_sq;
  
  double dT_im1jk_np1half   =(dT_im1jk_np1+grid.dLocalGridOld[grid.nT][i-1][j][k])*0.5;
  double dT_im1jk_np1half_sq= dT_im1jk_np1half   *dT_im1jk_np1half;
  double dT_im1jk_np1half_4 = dT_im1jk_np1half_sq*dT_im1jk_np1half_sq;
  
  double dT_ijp1k_np1half   =(dT_ijp1k_np1+grid.dLocalGridOld[grid.nT][i][j+1][k])*0.5;
  double dT_ijp1k_np1half_sq= dT_ijp1k_np1half   *dT_ijp1k_np1half;
  double dT_ijp1k_np1half_4 = dT_ijp1k_np1half_sq*dT_ijp1k_np1half_sq;
  
  double dT_ijm1k_np1half   =(dT_ijm1k_np1+grid.dLocalGridOld[grid.nT][i][j-1][k])*0.5;
  double dT_ijm1k_np1half_sq= dT_ijm1k_np1half   *dT_ijm1k_np1half;
  double dT_ijm1k_np1half_4 = dT_ijm1k_np1half_sq*dT_ijm1k_np1half_sq;
  
  double dE_ijk_np1=parameters.eosTable.dGetEnergy(dT_ijk_np1,grid.dLocalGridNew[grid.nD][i][j][k]);
  double dE_ip1jk_np1half=parameters.eosTable.dGetEnergy(dT_ip1jk_np1half,grid.dLocalGridOld[grid.nD][i+1][j][k]);
  double dE_ijk_np1half=parameters.eosTable.dGetEnergy(dT_ijk_np1half,grid.dLocalGridOld[grid.nD][i][j][k]);
  double dE_im1jk_np1half=parameters.eosTable.dGetEnergy(dT_im1jk_np1half,grid.dLocalGridOld[grid.nD][i-1][j][k]);
  double dE_ijp1k_np1half=parameters.eosTable.dGetEnergy(dT_ijp1k_np1half,grid.dLocalGridOld[grid.nD][i][j+1][k]);
  double dE_ijm1k_np1half=parameters.eosTable.dGetEnergy(dT_ijm1k_np1half,grid.dLocalGridOld[grid.nD][i][j-1][k]);
    
  double dE_ip1halfjk_np1half=(dE_ip1jk_np1half+dE_ijk_np1half)*0.5;
  double dE_im1halfjk_np1half=(dE_im1jk_np1half+dE_ijk_np1half)*0.5;
  double dE_ijp1halfk_np1half=(dE_ijp1k_np1half+dE_ijk_np1half)*0.5;
  double dE_ijm1halfk_np1half=(dE_ijm1k_np1half+dE_ijk_np1half)*0.5;
  
  double dP_ijk_np1half=parameters.eosTable.dGetPressure(dT_ijk_np1half,grid.dLocalGridOld[grid.nD][i][j][k]);
  double dPi_ijk_np1half=dP_ijk_np1half+grid.dLocalGridOld[grid.nQ0][i][j][k];
  double dPj_ijk_np1half=dP_ijk_np1half+grid.dLocalGridOld[grid.nQ1][i][j][k];
  
  double dKappa_ip1jk_np1half=parameters.eosTable.dGetOpacity(dT_ip1jk_np1half,grid.dLocalGridOld[grid.nD][i+1][j][k]);
  double dKappa_ijk_np1half=parameters.eosTable.dGetOpacity(dT_ijk_np1half,grid.dLocalGridOld[grid.nD][i][j][k]);
  double dKappa_im1jk_np1half=parameters.eosTable.dGetOpacity(dT_im1jk_np1half,grid.dLocalGridOld[grid.nD][i-1][j][k]);
  double dKappa_ijp1k_np1half=parameters.eosTable.dGetOpacity(dT_ijp1k_np1half,grid.dLocalGridOld[grid.nD][i][j+1][k]);
  double dKappa_ijm1k_np1half=parameters.eosTable.dGetOpacity(dT_ijm1k_np1half,grid.dLocalGridOld[grid.nD][i][j-1][k]);
  
  double dKappa_ip1halfjk_np1half=(dT_ip1jk_np1half_4+dT_ijk_np1half_4)/(dT_ijk_np1half_4/dKappa_ijk_np1half+dT_ip1jk_np1half_4/dKappa_ip1jk_np1half);
  double dKappa_im1halfjk_np1half=(dT_im1jk_np1half_4+dT_ijk_np1half_4)/(dT_ijk_np1half_4/dKappa_ijk_np1half+dT_im1jk_np1half_4/dKappa_im1jk_np1half);
  double dKappa_ijp1halfk_np1half=(dT_ijp1k_np1half_4+dT_ijk_np1half_4)/(dT_ijk_np1half_4/dKappa_ijk_np1half+dT_ijp1k_np1half_4/dKappa_ijp1k_np1half);
  double dKappa_ijm1halfk_np1half=(dT_ijm1k_np1half_4+dT_ijk_np1half_4)/(dT_ijk_np1half_4/dKappa_ijk_np1half+dT_ijm1k_np1half_4/dKappa_ijm1k_np1half);
  
  //Calcuate dA1
  double dA1CenGrad=(dE_ip1halfjk_np1half-dE_im1halfjk_np1half)/grid.dLocalGridOld[grid.nDM][i][0][0];
  double dA1UpWindGrad=0.0;
  double dU_U0_Diff=(dU_ijk_np1half-dU0_i_np1half);
  if(dU_U0_Diff<0.0){//moving in the negative direction
    dA1UpWindGrad=(dE_ip1jk_np1half-dE_ijk_np1half)/(grid.dLocalGridOld[grid.nDM][i+1][0][0]
      +grid.dLocalGridOld[grid.nDM][i][0][0])*2.0;
  }
  else{//moving in the postive direction
    dA1UpWindGrad=(dE_ijk_np1half-dE_im1jk_np1half)/(grid.dLocalGridOld[grid.nDM][i][0][0]
      +grid.dLocalGridOld[grid.nDM][i-1][0][0])*2.0;
  }
  double dA1=dU_U0_Diff*dRSq_i_n*((1.0-parameters.dDonorFrac)*dA1CenGrad
    +parameters.dDonorFrac*dA1UpWindGrad);
  
  //calculate dS1
  double dUR2_im1half_np1half=grid.dLocalGridNew[grid.nU][nIInt-1][j][k]*dR_im1half_n
    *dR_im1half_n;
  double dUR2_ip1half_np1half=grid.dLocalGridNew[grid.nU][nIInt][j][k]*dR_ip1half_n
    *dR_ip1half_n;
  double dS1=dPi_ijk_np1half/grid.dLocalGridOld[grid.nD][i][j][k]
    *(dUR2_ip1half_np1half-dUR2_im1half_np1half)/grid.dLocalGridOld[grid.nDM][i][0][0];
  
  //Calcualte dA2
  double dA2CenGrad=(dE_ijp1halfk_np1half-dE_ijm1halfk_np1half)/grid.dLocalGridOld[grid.nDTheta][0][j][0];
  double dA2UpWindGrad=0.0;
  if(dV_ijk_np1half<0.0){//moving in the negative direction
    dA2UpWindGrad=(dE_ijp1k_np1half-dE_ijk_np1half)/(grid.dLocalGridOld[grid.nDTheta][0][j+1][0]
      +grid.dLocalGridOld[grid.nDTheta][0][j][0])*2.0;
  }
  else{//moving in the positive direction
    dA2UpWindGrad=(dE_ijk_np1half-dE_ijm1k_np1half)/(grid.dLocalGridOld[grid.nDTheta][0][j][0]
      +grid.dLocalGridOld[grid.nDTheta][0][j-1][0])*2.0;
  }
  double dA2=dV_ijk_np1half/dR_i_n*((1.0-parameters.dDonorFrac)*dA2CenGrad
    +parameters.dDonorFrac*dA2UpWindGrad);
    
  //Calcualte dS2
  double dS2=dPj_ijk_np1half/(grid.dLocalGridOld[grid.nD][i][j][k]*dR_i_n
    *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]*grid.dLocalGridOld[grid.nDTheta][0][j][0])
    *(dVSinTheta_ijp1halfk_np1half-dVSinTheta_ijm1halfk_np1half);
  
  //Calculate dS4
  double dTGrad_ip1half_np1half=(dT_ip1jk_np1half_4-dT_ijk_np1half_4)/(grid.dLocalGridOld[grid.nDM][i+1][0][0]+grid.dLocalGridOld[grid.nDM][i][0][0])*2.0;
  double dTGrad_im1half_np1half=(dT_ijk_np1half_4-dT_im1jk_np1half_4)/(grid.dLocalGridOld[grid.nDM][i][0][0]+grid.dLocalGridOld[grid.nDM][i-1][0][0])*2.0;
  double dGrad_ip1half=dRhoAve_ip1half*dR4_ip1half/(dKappa_ip1halfjk_np1half*dRho_ip1halfjk)*dTGrad_ip1half_np1half;
  double dGrad_im1half=dRhoAve_im1half*dR4_im1half/(dKappa_im1halfjk_np1half*dRho_im1halfjk)*dTGrad_im1half_np1half;
  double dS4=16.0*parameters.dPi*parameters.dPi*grid.dLocalGridOld[grid.nDenAve][i][0][0]
    *(dGrad_ip1half-dGrad_im1half)/grid.dLocalGridOld[grid.nDM][i][0][0];
  
  //Calculate dS5
  double dTGrad_jp1half_np1half=(dT_ijp1k_np1half_4-dT_ijk_np1half_4)/(grid.dLocalGridOld[grid.nDTheta][0][j+1][0]+grid.dLocalGridOld[grid.nDTheta][0][j][0])*2.0;
  double dTGrad_jm1half_np1half=(dT_ijk_np1half_4-dT_ijm1k_np1half_4)/(grid.dLocalGridOld[grid.nDTheta][0][j][0]+grid.dLocalGridOld[grid.nDTheta][0][j-1][0])*2.0;
  double dGrad_jp1half_np1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt][0]/(dKappa_ijp1halfk_np1half*dRho_ijp1halfk*dR_i_n)*dTGrad_jp1half_np1half;
  double dGrad_jm1half_np1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt-1][0]/(dKappa_ijm1halfk_np1half*dRho_ijm1halfk*dR_i_n)*dTGrad_jm1half_np1half;
  double dS5=(dGrad_jp1half_np1half-dGrad_jm1half_np1half)/(grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]*dR_i_n*grid.dLocalGridOld[grid.nDTheta][0][j][0]);
  
  //calculate new energy
  return (dE_ijk_np1-grid.dLocalGridOld[grid.nE][i][j][k])/time.dDeltat_n
    +4.0*parameters.dPi*grid.dLocalGridOld[grid.nDenAve][i][0][0]*(dA1+dS1)+dA2+dS2
    -4.0*parameters.dSigma/(3.0*grid.dLocalGridOld[grid.nD][i][j][k])*(dS4+dS5);
}
double dImplicitEnergyFunction_RT_SB(Grid &grid,Parameters &parameters,Time &time,double dTemps[]
  ,int i,int j,int k){
  
  double dT_ijk_np1=dTemps[0];
  double dT_im1jk_np1=dTemps[1];
  double dT_ijp1k_np1=dTemps[2];
  double dT_ijm1k_np1=dTemps[3];
  
  //calculate i for interface centered quantities
  int nIInt=i+grid.nCenIntOffset[0];  
  int nJInt=j+grid.nCenIntOffset[1];
  
  //Calculate interpolated quantities
  double dU_ijk_np1half=(grid.dLocalGridNew[grid.nU][nIInt][j][k]+grid.dLocalGridNew[grid.nU][nIInt-1][j][k])*0.5;
  double dU0_i_np1half=(grid.dLocalGridNew[grid.nU0][nIInt][0][0]+grid.dLocalGridNew[grid.nU0][nIInt-1][0][0])*0.5;
  double dR_i_n=(grid.dLocalGridOld[grid.nR][nIInt][0][0]+grid.dLocalGridOld[grid.nR][nIInt-1][0][0])*0.5;
  double dR_im1half_n=grid.dLocalGridOld[grid.nR][nIInt-1][0][0];
  double dR_ip1half_n=grid.dLocalGridOld[grid.nR][nIInt][0][0];
  double dRSq_i_n=dR_i_n*dR_i_n;
  double dRSq_ip1half=dR_ip1half_n*dR_ip1half_n;
  double dR_im1half_sq=dR_im1half_n*dR_im1half_n;
  double dR_im1half_4=dR_im1half_sq*dR_im1half_sq;
  double dV_ijk_np1half=(grid.dLocalGridNew[grid.nV][i][nJInt][k]+grid.dLocalGridNew[grid.nV][i][nJInt-1][k])*0.5;
  double dVSinTheta_ijp1halfk_np1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt][0]*grid.dLocalGridNew[grid.nV][i][nJInt][k];
  double dVSinTheta_ijm1halfk_np1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt-1][0]*grid.dLocalGridNew[grid.nV][i][nJInt-1][k];
  double dRhoAve_im1half=(grid.dLocalGridOld[grid.nDenAve][i][0][0]+grid.dLocalGridOld[grid.nDenAve][i-1][0][0])*0.5;
  double dRho_im1halfjk=(grid.dLocalGridOld[grid.nD][i][j][k]+grid.dLocalGridOld[grid.nD][i-1][j][k])*0.5;
  double dRho_ijp1halfk=(grid.dLocalGridOld[grid.nD][i][j+1][k]+grid.dLocalGridOld[grid.nD][i][j][k])*0.5;
  double dRho_ijm1halfk=(grid.dLocalGridOld[grid.nD][i][j][k]+grid.dLocalGridOld[grid.nD][i][j-1][k])*0.5;
  
  double dT_ijk_np1half   =(dT_ijk_np1+grid.dLocalGridOld[grid.nT][i][j][k])*0.5;
  double dT_ijk_np1half_sq= dT_ijk_np1half   *dT_ijk_np1half;
  double dT_ijk_np1half_4 = dT_ijk_np1half_sq*dT_ijk_np1half_sq;
  
  double dT_im1jk_np1half   =(dT_im1jk_np1+grid.dLocalGridOld[grid.nT][i-1][j][k])*0.5;
  double dT_im1jk_np1half_sq= dT_im1jk_np1half   *dT_im1jk_np1half;
  double dT_im1jk_np1half_4 = dT_im1jk_np1half_sq*dT_im1jk_np1half_sq;
  
  double dT_ijp1k_np1half   =(dT_ijp1k_np1+grid.dLocalGridOld[grid.nT][i][j+1][k])*0.5;
  double dT_ijp1k_np1half_sq= dT_ijp1k_np1half   *dT_ijp1k_np1half;
  double dT_ijp1k_np1half_4 = dT_ijp1k_np1half_sq*dT_ijp1k_np1half_sq;
  
  double dT_ijm1k_np1half   =(dT_ijm1k_np1+grid.dLocalGridOld[grid.nT][i][j-1][k])*0.5;
  double dT_ijm1k_np1half_sq= dT_ijm1k_np1half   *dT_ijm1k_np1half;
  double dT_ijm1k_np1half_4 = dT_ijm1k_np1half_sq*dT_ijm1k_np1half_sq;
  
  double dE_ijk_np1=parameters.eosTable.dGetEnergy(dT_ijk_np1,grid.dLocalGridNew[grid.nD][i][j][k]);
  double dE_ijk_np1half=parameters.eosTable.dGetEnergy(dT_ijk_np1half,grid.dLocalGridOld[grid.nD][i][j][k]);
  double dE_im1jk_np1half=parameters.eosTable.dGetEnergy(dT_im1jk_np1half,grid.dLocalGridOld[grid.nD][i-1][j][k]);
  double dE_ijp1k_np1half=parameters.eosTable.dGetEnergy(dT_ijp1k_np1half,grid.dLocalGridOld[grid.nD][i][j+1][k]);
  double dE_ijm1k_np1half=parameters.eosTable.dGetEnergy(dT_ijm1k_np1half,grid.dLocalGridOld[grid.nD][i][j-1][k]);
    
  double dE_ip1halfjk_np1half=dE_ijk_np1half;
  double dE_im1halfjk_np1half=(dE_im1jk_np1half+dE_ijk_np1half)*0.5;
  double dE_ijp1halfk_np1half=(dE_ijp1k_np1half+dE_ijk_np1half)*0.5;
  double dE_ijm1halfk_np1half=(dE_ijm1k_np1half+dE_ijk_np1half)*0.5;
  
  double dP_ijk_np1half=parameters.eosTable.dGetPressure(dT_ijk_np1half,grid.dLocalGridOld[grid.nD][i][j][k]);
  double dPi_ijk_np1half=dP_ijk_np1half+grid.dLocalGridOld[grid.nQ0][i][j][k];
  double dPj_ijk_np1half=dP_ijk_np1half+grid.dLocalGridOld[grid.nQ1][i][j][k];
  
  double dKappa_ijk_np1half=parameters.eosTable.dGetOpacity(dT_ijk_np1half,grid.dLocalGridOld[grid.nD][i][j][k]);
  double dKappa_im1jk_np1half=parameters.eosTable.dGetOpacity(dT_im1jk_np1half,grid.dLocalGridOld[grid.nD][i-1][j][k]);
  double dKappa_ijp1k_np1half=parameters.eosTable.dGetOpacity(dT_ijp1k_np1half,grid.dLocalGridOld[grid.nD][i][j+1][k]);
  double dKappa_ijm1k_np1half=parameters.eosTable.dGetOpacity(dT_ijm1k_np1half,grid.dLocalGridOld[grid.nD][i][j-1][k]);
  
  double dKappa_im1halfjk_n_np1half=(dT_im1jk_np1half_4+dT_ijk_np1half_4)/(dT_ijk_np1half_4/dKappa_ijk_np1half+dT_im1jk_np1half_4/dKappa_im1jk_np1half);
  double dKappa_ijp1halfk_n_np1half=(dT_ijp1k_np1half_4+dT_ijk_np1half_4)/(dT_ijk_np1half_4/dKappa_ijk_np1half+dT_ijp1k_np1half_4/dKappa_ijp1k_np1half);
  double dKappa_ijm1halfk_n_np1half=(dT_ijm1k_np1half_4+dT_ijk_np1half_4)/(dT_ijk_np1half_4/dKappa_ijk_np1half+dT_ijm1k_np1half_4/dKappa_ijm1k_np1half);
  
  //Calcuate dA1
  double dA1CenGrad=(dE_ip1halfjk_np1half-dE_im1halfjk_np1half)/grid.dLocalGridOld[grid.nDM][i][0][0];
  double dA1UpWindGrad=0.0;
  double dU_U0_Diff=(dU_ijk_np1half-dU0_i_np1half);
  if(dU_U0_Diff<0.0){//moving in the negative direction
    dA1UpWindGrad=dA1UpWindGrad;/**\BC Using centered gradient for upwind gradient when motion is 
      into the star at the surface*/
  }
  else{//moving in the postive direction
    dA1UpWindGrad=(dE_ijk_np1half-dE_im1jk_np1half)/(grid.dLocalGridOld[grid.nDM][i][0][0]
      +grid.dLocalGridOld[grid.nDM][i-1][0][0])*2.0;
  }
  double dA1=dU_U0_Diff*dRSq_i_n*((1.0-parameters.dDonorFrac)*dA1CenGrad
    +parameters.dDonorFrac*dA1UpWindGrad);
  
  //calculate dS1
  double dUR2_im1half_np1half=grid.dLocalGridNew[grid.nU][nIInt-1][j][k]*dR_im1half_n
    *dR_im1half_n;
  double dUR2_ip1half_np1half=grid.dLocalGridNew[grid.nU][nIInt][j][k]*dR_ip1half_n
    *dR_ip1half_n;
  double dS1=dPi_ijk_np1half/grid.dLocalGridOld[grid.nD][i][j][k]
    *(dUR2_ip1half_np1half-dUR2_im1half_np1half)/grid.dLocalGridOld[grid.nDM][i][0][0];
  
  //Calcualte dA2
  double dA2CenGrad=(dE_ijp1halfk_np1half-dE_ijm1halfk_np1half)/grid.dLocalGridOld[grid.nDTheta][0][j][0];
  double dA2UpWindGrad=0.0;
  if(dV_ijk_np1half<0.0){//moving in the negative direction
    dA2UpWindGrad=(dE_ijp1k_np1half-dE_ijk_np1half)/(grid.dLocalGridOld[grid.nDTheta][0][j+1][0]
      +grid.dLocalGridOld[grid.nDTheta][0][j][0])*2.0;
  }
  else{//moving in the positive direction
    dA2UpWindGrad=(dE_ijk_np1half-dE_ijm1k_np1half)/(grid.dLocalGridOld[grid.nDTheta][0][j][0]
      +grid.dLocalGridOld[grid.nDTheta][0][j-1][0])*2.0;
  }
  double dA2=dV_ijk_np1half/dR_i_n*((1.0-parameters.dDonorFrac)*dA2CenGrad
    +parameters.dDonorFrac*dA2UpWindGrad);
    
  //Calcualte dS2
  double dS2=dPj_ijk_np1half/(grid.dLocalGridOld[grid.nD][i][j][k]*dR_i_n
    *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]*grid.dLocalGridOld[grid.nDTheta][0][j][0])
    *(dVSinTheta_ijp1halfk_np1half-dVSinTheta_ijm1halfk_np1half);
  
  //Calculate dS4
  double dTGrad_im1half_np1half=(dT_ijk_np1half_4-dT_im1jk_np1half_4)/(grid.dLocalGridOld[grid.nDM][i][0][0]+grid.dLocalGridOld[grid.nDM][i-1][0][0])*2.0;
  double dGrad_ip1half=-3.0*dRSq_ip1half*dT_ijk_np1half_4/(8.0*parameters.dPi);/**\BC 
    Missing grid.dLocalGridOld[grid.nT][i+1][0][0] using flux equals \f$2\sigma T^4\f$ at surface.*/
  double dGrad_im1half=dRhoAve_im1half*dR_im1half_4/(dKappa_im1halfjk_n_np1half*dRho_im1halfjk)*dTGrad_im1half_np1half;
  double dS4=16.0*parameters.dPi*parameters.dPi*grid.dLocalGridOld[grid.nDenAve][i][0][0]
    *(dGrad_ip1half-dGrad_im1half)/grid.dLocalGridOld[grid.nDM][i][0][0];
  
  //Calculate dS5
  double dTGrad_jp1half_np1half=(dT_ijp1k_np1half_4-dT_ijk_np1half_4)/(grid.dLocalGridOld[grid.nDTheta][0][j+1][0]
    +grid.dLocalGridOld[grid.nDTheta][0][j][0])*2.0;
  double dTGrad_jm1half_np1half=(dT_ijk_np1half_4-dT_ijm1k_np1half_4)/(grid.dLocalGridOld[grid.nDTheta][0][j][0]
    +grid.dLocalGridOld[grid.nDTheta][0][j-1][0])*2.0;
  double dGrad_jp1half_np1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt][0]
    /(dKappa_ijp1halfk_n_np1half*dRho_ijp1halfk*dR_i_n)*dTGrad_jp1half_np1half;
  double dGrad_jm1half_np1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt-1][0]
    /(dKappa_ijm1halfk_n_np1half*dRho_ijm1halfk*dR_i_n)*dTGrad_jm1half_np1half;
  double dS5=(dGrad_jp1half_np1half-dGrad_jm1half_np1half)/(grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]
    *dR_i_n*grid.dLocalGridOld[grid.nDTheta][0][j][0]);
  
  //calculate new energy
  return (dE_ijk_np1-grid.dLocalGridOld[grid.nE][i][j][k])/time.dDeltat_n
    +4.0*parameters.dPi*grid.dLocalGridOld[grid.nDenAve][i][0][0]*(dA1+dS1)+dA2+dS2
    -4.0*parameters.dSigma/(3.0*grid.dLocalGridOld[grid.nD][i][j][k])*(dS4+dS5);
}
double dImplicitEnergyFunction_RTP(Grid &grid,Parameters &parameters,Time &time,double dTemps[]
  ,int i,int j,int k){
  
  double dT_ijk_np1=dTemps[0];
  double dT_ip1jk_np1=dTemps[1];
  double dT_im1jk_np1=dTemps[2];
  double dT_ijp1k_np1=dTemps[3];
  double dT_ijm1k_np1=dTemps[4];
  double dT_ijkp1_np1=dTemps[5];
  double dT_ijkm1_np1=dTemps[6];
  
  //calculate i,j,k for interface centered quantities
  int nIInt=i+grid.nCenIntOffset[0];
  int nJInt=j+grid.nCenIntOffset[1];
  int nKInt=k+grid.nCenIntOffset[2];
    
  //Calculate interpolated quantities
  double dU_ijk_np1half=(grid.dLocalGridNew[grid.nU][nIInt][j][k]+grid.dLocalGridNew[grid.nU][nIInt-1][j][k])*0.5;
  double dU0_i_np1half=(grid.dLocalGridNew[grid.nU0][nIInt][0][0]+grid.dLocalGridNew[grid.nU0][nIInt-1][0][0])*0.5;
  double dR_i_n=(grid.dLocalGridOld[grid.nR][nIInt][0][0]+grid.dLocalGridOld[grid.nR][nIInt-1][0][0])*0.5;
  double dR_im1half_n=grid.dLocalGridOld[grid.nR][nIInt-1][0][0];
  double dR_ip1half_n=grid.dLocalGridOld[grid.nR][nIInt][0][0];
  double dRSq_i_n=dR_i_n*dR_i_n;
  double dRSq_ip1half=dR_ip1half_n*dR_ip1half_n;
  double dR4_ip1half=dRSq_ip1half*dRSq_ip1half;
  double dR_im1half_sq=dR_im1half_n*dR_im1half_n;
  double dR_im1half_4=dR_im1half_sq*dR_im1half_sq;
  double dV_ijk_np1half=(grid.dLocalGridNew[grid.nV][i][nJInt][k]+grid.dLocalGridNew[grid.nV][i][nJInt-1][k])*0.5;
  double dW_ijk_np1half=(grid.dLocalGridNew[grid.nW][i][j][nKInt]+grid.dLocalGridNew[grid.nW][i][j][nKInt-1])*0.5;
  double dW_ijkp1half_np1half=(grid.dLocalGridNew[grid.nW][i][j][nKInt]);
  double dW_ijkm1half_np1half=(grid.dLocalGridNew[grid.nW][i][j][nKInt-1]);
  double dVSinTheta_ijp1halfk_np1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt][0]*grid.dLocalGridNew[grid.nV][i][nJInt][k];
  double dVSinTheta_ijm1halfk_np1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt-1][0]*grid.dLocalGridNew[grid.nV][i][nJInt-1][k];
  double dRhoAve_ip1half=(grid.dLocalGridOld[grid.nDenAve][i+1][0][0]+grid.dLocalGridOld[grid.nDenAve][i][0][0])*0.5;
  double dRhoAve_im1half=(grid.dLocalGridOld[grid.nDenAve][i][0][0]+grid.dLocalGridOld[grid.nDenAve][i-1][0][0])*0.5;
  double dRho_ip1halfjk=(grid.dLocalGridOld[grid.nD][i+1][j][k]+grid.dLocalGridOld[grid.nD][i][j][k])*0.5;
  double dRho_im1halfjk=(grid.dLocalGridOld[grid.nD][i][j][k]+grid.dLocalGridOld[grid.nD][i-1][j][k])*0.5;
  double dRho_ijp1halfk=(grid.dLocalGridOld[grid.nD][i][j+1][k]+grid.dLocalGridOld[grid.nD][i][j][k])*0.5;
  double dRho_ijm1halfk=(grid.dLocalGridOld[grid.nD][i][j][k]+grid.dLocalGridOld[grid.nD][i][j-1][k])*0.5;
  double dRho_ijkp1half=(grid.dLocalGridOld[grid.nD][i][j][k+1]+grid.dLocalGridOld[grid.nD][i][j][k])*0.5;
  double dRho_ijkm1half=(grid.dLocalGridOld[grid.nD][i][j][k]+grid.dLocalGridOld[grid.nD][i][j][k-1])*0.5;
  
  double dT_ip1jk_np1half   =(dT_ip1jk_np1+grid.dLocalGridOld[grid.nT][i+1][j][k])*0.5;
  double dT_ip1jk_np1half_sq= dT_ip1jk_np1half   *dT_ip1jk_np1half;
  double dT_ip1jk_np1half_4 = dT_ip1jk_np1half_sq*dT_ip1jk_np1half_sq;
  
  double dT_ijk_np1half   =(dT_ijk_np1+grid.dLocalGridOld[grid.nT][i][j][k])*0.5;
  double dT_ijk_np1half_sq= dT_ijk_np1half   *dT_ijk_np1half;
  double dT_ijk_np1half_4 = dT_ijk_np1half_sq*dT_ijk_np1half_sq;
  
  double dT_im1jk_np1half   =(dT_im1jk_np1+grid.dLocalGridOld[grid.nT][i-1][j][k])*0.5;
  double dT_im1jk_np1half_sq= dT_im1jk_np1half   *dT_im1jk_np1half;
  double dT_im1jk_np1half_4 = dT_im1jk_np1half_sq*dT_im1jk_np1half_sq;
  
  double dT_ijp1k_np1half   =(dT_ijp1k_np1+grid.dLocalGridOld[grid.nT][i][j+1][k])*0.5;
  double dT_ijp1k_np1half_sq= dT_ijp1k_np1half   *dT_ijp1k_np1half;
  double dT_ijp1k_np1half_4 = dT_ijp1k_np1half_sq*dT_ijp1k_np1half_sq;
  
  double dT_ijm1k_np1half   =(dT_ijm1k_np1+grid.dLocalGridOld[grid.nT][i][j-1][k])*0.5;
  double dT_ijm1k_np1half_sq= dT_ijm1k_np1half   *dT_ijm1k_np1half;
  double dT_ijm1k_np1half_4 = dT_ijm1k_np1half_sq*dT_ijm1k_np1half_sq;
  
  double dT_ijkp1_np1half   =(dT_ijkp1_np1+grid.dLocalGridOld[grid.nT][i][j][k+1])*0.5;
  double dT_ijkp1_np1half_sq= dT_ijkp1_np1half   *dT_ijkp1_np1half;
  double dT_ijkp1_np1half_4 = dT_ijkp1_np1half_sq*dT_ijkp1_np1half_sq;
  
  double dT_ijkm1_np1half   =(dT_ijkm1_np1+grid.dLocalGridOld[grid.nT][i][j][k-1])*0.5;
  double dT_ijkm1_np1half_sq= dT_ijkm1_np1half   *dT_ijkm1_np1half;
  double dT_ijkm1_np1half_4 = dT_ijkm1_np1half_sq*dT_ijkm1_np1half_sq;
  
  double dE_ijk_np1=parameters.eosTable.dGetEnergy(dT_ijk_np1,grid.dLocalGridNew[grid.nD][i][j][k]);
  double dE_ip1jk_np1half=parameters.eosTable.dGetEnergy(dT_ip1jk_np1half,grid.dLocalGridOld[grid.nD][i+1][j][k]);
  double dE_ijk_np1half=parameters.eosTable.dGetEnergy(dT_ijk_np1half,grid.dLocalGridOld[grid.nD][i][j][k]);
  double dE_im1jk_np1half=parameters.eosTable.dGetEnergy(dT_im1jk_np1half,grid.dLocalGridOld[grid.nD][i-1][j][k]);
  double dE_ijp1k_np1half=parameters.eosTable.dGetEnergy(dT_ijp1k_np1half,grid.dLocalGridOld[grid.nD][i][j+1][k]);
  double dE_ijm1k_np1half=parameters.eosTable.dGetEnergy(dT_ijm1k_np1half,grid.dLocalGridOld[grid.nD][i][j-1][k]);
  double dE_ijkp1_np1half=parameters.eosTable.dGetEnergy(dT_ijkp1_np1half,grid.dLocalGridOld[grid.nD][i][j][k+1]);
  double dE_ijkm1_np1half=parameters.eosTable.dGetEnergy(dT_ijkm1_np1half,grid.dLocalGridOld[grid.nD][i][j][k-1]);
  
  double dE_ip1halfjk_np1half=(dE_ip1jk_np1half+dE_ijk_np1half)*0.5;
  double dE_im1halfjk_np1half=(dE_im1jk_np1half+dE_ijk_np1half)*0.5;
  double dE_ijp1halfk_np1half=(dE_ijp1k_np1half+dE_ijk_np1half)*0.5;
  double dE_ijm1halfk_np1half=(dE_ijm1k_np1half+dE_ijk_np1half)*0.5;
  double dE_ijkp1half_np1half=(dE_ijkp1_np1half+dE_ijk_np1half)*0.5;
  double dE_ijkm1half_np1half=(dE_ijkm1_np1half+dE_ijk_np1half)*0.5;
  
  double dP_ijk_np1half=parameters.eosTable.dGetPressure(dT_ijk_np1half,grid.dLocalGridOld[grid.nD][i][j][k]);
  double dPi_ijk_np1half=dP_ijk_np1half+grid.dLocalGridOld[grid.nQ0][i][j][k];
  double dPj_ijk_np1half=dP_ijk_np1half+grid.dLocalGridOld[grid.nQ1][i][j][k];
  double dPk_ijk_np1half=dP_ijk_np1half+grid.dLocalGridOld[grid.nQ2][i][j][k];
  
  double dKappa_ip1jk_np1half=parameters.eosTable.dGetOpacity(dT_ip1jk_np1half,grid.dLocalGridOld[grid.nD][i+1][j][k]);
  double dKappa_ijk_np1half=parameters.eosTable.dGetOpacity(dT_ijk_np1half,grid.dLocalGridOld[grid.nD][i][j][k]);
  double dKappa_im1jk_np1half=parameters.eosTable.dGetOpacity(dT_im1jk_np1half,grid.dLocalGridOld[grid.nD][i-1][j][k]);
  double dKappa_ijp1k_np1half=parameters.eosTable.dGetOpacity(dT_ijp1k_np1half,grid.dLocalGridOld[grid.nD][i][j+1][k]);
  double dKappa_ijm1k_np1half=parameters.eosTable.dGetOpacity(dT_ijm1k_np1half,grid.dLocalGridOld[grid.nD][i][j-1][k]);
  double dKappa_ijkp1_np1half=parameters.eosTable.dGetOpacity(dT_ijkp1_np1half,grid.dLocalGridOld[grid.nD][i][j][k+1]);
  double dKappa_ijkm1_np1half=parameters.eosTable.dGetOpacity(dT_ijkm1_np1half,grid.dLocalGridOld[grid.nD][i][j][k-1]);
  
  double dKappa_ip1halfjk_n_np1half=(dT_ip1jk_np1half_4+dT_ijk_np1half_4)/(dT_ijk_np1half_4/dKappa_ijk_np1half+dT_ip1jk_np1half_4/dKappa_ip1jk_np1half);
  double dKappa_im1halfjk_n_np1half=(dT_im1jk_np1half_4+dT_ijk_np1half_4)/(dT_ijk_np1half_4/dKappa_ijk_np1half+dT_im1jk_np1half_4/dKappa_im1jk_np1half);
  double dKappa_ijp1halfk_n_np1half=(dT_ijp1k_np1half_4+dT_ijk_np1half_4)/(dT_ijk_np1half_4/dKappa_ijk_np1half+dT_ijp1k_np1half_4/dKappa_ijp1k_np1half);
  double dKappa_ijm1halfk_n_np1half=(dT_ijm1k_np1half_4+dT_ijk_np1half_4)/(dT_ijk_np1half_4/dKappa_ijk_np1half+dT_ijm1k_np1half_4/dKappa_ijm1k_np1half);
  double dKappa_ijkp1half_np1half=(dT_ijkp1_np1half_4+dT_ijk_np1half_4)/(dT_ijk_np1half_4/dKappa_ijk_np1half+dT_ijkp1_np1half_4/dKappa_ijkp1_np1half);
  double dKappa_ijkm1half_np1half=(dT_ijkm1_np1half_4+dT_ijk_np1half_4)/(dT_ijk_np1half_4/dKappa_ijk_np1half+dT_ijkm1_np1half_4/dKappa_ijkm1_np1half);
  
  //Calcuate dA1
  double dA1CenGrad=(dE_ip1halfjk_np1half-dE_im1halfjk_np1half)/grid.dLocalGridOld[grid.nDM][i][0][0];
  double dA1UpWindGrad=0.0;
  double dU_U0_Diff=(dU_ijk_np1half-dU0_i_np1half);
  if(dU_U0_Diff<0.0){//moving in the negative direction
    dA1UpWindGrad=(dE_ip1jk_np1half-dE_ijk_np1half)/(grid.dLocalGridOld[grid.nDM][i+1][0][0]
      +grid.dLocalGridOld[grid.nDM][i][0][0])*2.0;
  }
  else{//moving in the postive direction
    dA1UpWindGrad=(dE_ijk_np1half-dE_im1jk_np1half)/(grid.dLocalGridOld[grid.nDM][i][0][0]
      +grid.dLocalGridOld[grid.nDM][i-1][0][0])*2.0;
  }
  double dA1=dU_U0_Diff*dRSq_i_n*((1.0-parameters.dDonorFrac)*dA1CenGrad
    +parameters.dDonorFrac*dA1UpWindGrad);
  
  //calculate dS1
  double dUR2_im1half_np1half=grid.dLocalGridNew[grid.nU][nIInt-1][j][k]
    *dR_im1half_n*dR_im1half_n;
  double dUR2_ip1half_np1half=grid.dLocalGridNew[grid.nU][nIInt][j][k]*dR_ip1half_n*dR_ip1half_n;
  double dS1=dPi_ijk_np1half/grid.dLocalGridOld[grid.nD][i][j][k]
    *(dUR2_ip1half_np1half-dUR2_im1half_np1half)/grid.dLocalGridOld[grid.nDM][i][0][0];
  
  //Calcualte dA2
  double dA2CenGrad=(dE_ijp1halfk_np1half-dE_ijm1halfk_np1half)/grid.dLocalGridOld[grid.nDTheta][0][j][0];
  double dA2UpWindGrad=0.0;
  if(dV_ijk_np1half<0.0){//moving in the negative direction
    dA2UpWindGrad=(dE_ijp1k_np1half-dE_ijk_np1half)/(grid.dLocalGridOld[grid.nDTheta][0][j+1][0]
      +grid.dLocalGridOld[grid.nDTheta][0][j][0])*2.0;
  }
  else{//moving in the positive direction
    dA2UpWindGrad=(dE_ijk_np1half-dE_ijm1k_np1half)/(grid.dLocalGridOld[grid.nDTheta][0][j][0]
      +grid.dLocalGridOld[grid.nDTheta][0][j-1][0])*2.0;
  }
  double dA2=dV_ijk_np1half/dR_i_n*((1.0-parameters.dDonorFrac)*dA2CenGrad
    +parameters.dDonorFrac*dA2UpWindGrad);
    
  //Calcualte dS2
  double dS2=dPj_ijk_np1half/(grid.dLocalGridOld[grid.nD][i][j][k]*dR_i_n
    *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]*grid.dLocalGridOld[grid.nDTheta][0][j][0])
    *(dVSinTheta_ijp1halfk_np1half-dVSinTheta_ijm1halfk_np1half);
  
  //Calcualte dA3
  double dA3CenGrad=(dE_ijkp1half_np1half-dE_ijkm1half_np1half)/grid.dLocalGridOld[grid.nDPhi][0][0][k];
  double dA3UpWindGrad=0.0;
  if(dW_ijk_np1half<0.0){//moving in the negative direction
    dA3UpWindGrad=(dE_ijkp1_np1half-dE_ijk_np1half)/(grid.dLocalGridOld[grid.nDPhi][0][0][k+1]
      +grid.dLocalGridOld[grid.nDPhi][0][0][k])*2.0;
  }
  else{//moving in the positive direction
    dA3UpWindGrad=(dE_ijk_np1half-dE_ijkm1_np1half)/(grid.dLocalGridOld[grid.nDPhi][0][0][k]
      +grid.dLocalGridOld[grid.nDPhi][0][0][k-1])*2.0;
  }
  double dA3=dW_ijk_np1half/(dR_i_n*grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0])*
    ((1.0-parameters.dDonorFrac)*dA3CenGrad+parameters.dDonorFrac*dA3UpWindGrad);
  
  //Calcualte dS3
  double dS3=dPk_ijk_np1half/(grid.dLocalGridOld[grid.nD][i][j][k]*dR_i_n
    *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]*grid.dLocalGridOld[grid.nDPhi][0][0][k])
    *(dW_ijkp1half_np1half-dW_ijkm1half_np1half);
  
  //Calculate dS4
  double dTGrad_ip1half_np1half=(dT_ip1jk_np1half_4-dT_ijk_np1half_4)/(grid.dLocalGridOld[grid.nDM][i+1][0][0]+grid.dLocalGridOld[grid.nDM][i][0][0])*2.0;
  double dTGrad_im1half_np1half=(dT_ijk_np1half_4-dT_im1jk_np1half_4)/(grid.dLocalGridOld[grid.nDM][i][0][0]+grid.dLocalGridOld[grid.nDM][i-1][0][0])*2.0;
  double dGrad_ip1half_np1half=dRhoAve_ip1half*dR4_ip1half/(dKappa_ip1halfjk_n_np1half*dRho_ip1halfjk)*dTGrad_ip1half_np1half;
  double dGrad_im1half_np1half=dRhoAve_im1half*dR_im1half_4/(dKappa_im1halfjk_n_np1half*dRho_im1halfjk)*dTGrad_im1half_np1half;
  double dS4=16.0*parameters.dPi*parameters.dPi*grid.dLocalGridOld[grid.nDenAve][i][0][0]
    *(dGrad_ip1half_np1half-dGrad_im1half_np1half)/grid.dLocalGridOld[grid.nDM][i][0][0];
  
  //Calculate dS5
  double dTGrad_jp1half_np1half=(dT_ijp1k_np1half_4-dT_ijk_np1half_4)/(grid.dLocalGridOld[grid.nDTheta][0][j+1][0]+grid.dLocalGridOld[grid.nDTheta][0][j][0])*2.0;
  double dTGrad_jm1half_np1half=(dT_ijk_np1half_4-dT_ijm1k_np1half_4)/(grid.dLocalGridOld[grid.nDTheta][0][j][0]+grid.dLocalGridOld[grid.nDTheta][0][j-1][0])*2.0;
  double dGrad_jp1half_np1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt][0]/(dKappa_ijp1halfk_n_np1half*dRho_ijp1halfk*dR_i_n)*dTGrad_jp1half_np1half;
  double dGrad_jm1half_np1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt-1][0]/(dKappa_ijm1halfk_n_np1half*dRho_ijm1halfk*dR_i_n)*dTGrad_jm1half_np1half;
  double dS5=(dGrad_jp1half_np1half-dGrad_jm1half_np1half)/(grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]
    *dR_i_n*grid.dLocalGridOld[grid.nDTheta][0][j][0]);
  
  //Calculate dS6
  double dTGrad_kp1half_np1half=(dT_ijkp1_np1half_4-dT_ijk_np1half_4)/(grid.dLocalGridOld[grid.nDPhi][0][0][k+1]+grid.dLocalGridOld[grid.nDPhi][0][0][k])*2.0;
  double dTGrad_km1half_np1half=(dT_ijk_np1half_4-dT_ijkm1_np1half_4)/(grid.dLocalGridOld[grid.nDPhi][0][0][k]+grid.dLocalGridOld[grid.nDPhi][0][0][k-1])*2.0;
  double dGrad_kp1half_np1half=dTGrad_kp1half_np1half/(dKappa_ijkp1half_np1half*dRho_ijkp1half*dR_i_n);
  double dGrad_km1half_np1half=dTGrad_km1half_np1half/(dKappa_ijkm1half_np1half*dRho_ijkm1half*dR_i_n);
  double dS6=(dGrad_kp1half_np1half-dGrad_km1half_np1half)/(dR_i_n*grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]*grid.dLocalGridOld[grid.nDPhi][0][0][k]);
  
  //calculate new energy
  return (dE_ijk_np1-grid.dLocalGridOld[grid.nE][i][j][k])/time.dDeltat_n
    +4.0*parameters.dPi*grid.dLocalGridOld[grid.nDenAve][i][0][0]*(dA1+dS1)+dA2+dS2+dA3+dS3
    -4.0*parameters.dSigma/(3.0*grid.dLocalGridOld[grid.nD][i][j][k])*(dS4+dS5+dS6);
}
double dImplicitEnergyFunction_RTP_SB(Grid &grid,Parameters &parameters,Time &time,double dTemps[]
  ,int i,int j,int k){
  
  double dT_ijk_np1=dTemps[0];
  double dT_im1jk_np1=dTemps[1];
  double dT_ijp1k_np1=dTemps[2];
  double dT_ijm1k_np1=dTemps[3];
  double dT_ijkp1_np1=dTemps[4];
  double dT_ijkm1_np1=dTemps[5];
  
  //calculate i,j,k for interface centered quantities
  int nIInt=i+grid.nCenIntOffset[0];
  int nJInt=j+grid.nCenIntOffset[1];
  int nKInt=k+grid.nCenIntOffset[2];
    
  //Calculate interpolated quantities
  double dU_ijk_np1half=(grid.dLocalGridNew[grid.nU][nIInt][j][k]+grid.dLocalGridNew[grid.nU][nIInt-1][j][k])*0.5;
  double dU0_i_np1half=(grid.dLocalGridNew[grid.nU0][nIInt][0][0]+grid.dLocalGridNew[grid.nU0][nIInt-1][0][0])*0.5;
  double dR_i_n=(grid.dLocalGridOld[grid.nR][nIInt][0][0]+grid.dLocalGridOld[grid.nR][nIInt-1][0][0])*0.5;
  double dR_im1half_n=grid.dLocalGridOld[grid.nR][nIInt-1][0][0];
  double dR_ip1half_n=grid.dLocalGridOld[grid.nR][nIInt][0][0];
  double dRSq_i_n=dR_i_n*dR_i_n;
  double dRSq_ip1half=dR_ip1half_n*dR_ip1half_n;
  double dR_im1half_sq=dR_im1half_n*dR_im1half_n;
  double dR_im1half_4=dR_im1half_sq*dR_im1half_sq;
  double dV_ijk_np1half=(grid.dLocalGridNew[grid.nV][i][nJInt][k]+grid.dLocalGridNew[grid.nV][i][nJInt-1][k])*0.5;
  double dW_ijk_np1half=(grid.dLocalGridNew[grid.nW][i][j][nKInt]+grid.dLocalGridNew[grid.nW][i][j][nKInt-1])*0.5;
  double dW_ijkp1half_np1half=(grid.dLocalGridNew[grid.nW][i][j][nKInt]);
  double dW_ijkm1half_np1half=(grid.dLocalGridNew[grid.nW][i][j][nKInt-1]);
  double dVSinTheta_ijp1halfk_np1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt][0]*grid.dLocalGridNew[grid.nV][i][nJInt][k];
  double dVSinTheta_ijm1halfk_np1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt-1][0]*grid.dLocalGridNew[grid.nV][i][nJInt-1][k];
  double dRhoAve_im1half=(grid.dLocalGridOld[grid.nDenAve][i][0][0]+grid.dLocalGridOld[grid.nDenAve][i-1][0][0])*0.5;
  double dRho_im1halfjk=(grid.dLocalGridOld[grid.nD][i][j][k]+grid.dLocalGridOld[grid.nD][i-1][j][k])*0.5;
  double dRho_ijp1halfk=(grid.dLocalGridOld[grid.nD][i][j+1][k]+grid.dLocalGridOld[grid.nD][i][j][k])*0.5;
  double dRho_ijm1halfk=(grid.dLocalGridOld[grid.nD][i][j][k]+grid.dLocalGridOld[grid.nD][i][j-1][k])*0.5;
  double dRho_ijkp1half=(grid.dLocalGridOld[grid.nD][i][j][k+1]+grid.dLocalGridOld[grid.nD][i][j][k])*0.5;
  double dRho_ijkm1half=(grid.dLocalGridOld[grid.nD][i][j][k]+grid.dLocalGridOld[grid.nD][i][j][k-1])*0.5;
  
  double dT_ijk_np1half   =(dT_ijk_np1+grid.dLocalGridOld[grid.nT][i][j][k])*0.5;
  double dT_ijk_np1half_sq= dT_ijk_np1half   *dT_ijk_np1half;
  double dT_ijk_np1half_4 = dT_ijk_np1half_sq*dT_ijk_np1half_sq;
  
  double dT_im1jk_np1half   =(dT_im1jk_np1+grid.dLocalGridOld[grid.nT][i-1][j][k])*0.5;
  double dT_im1jk_np1half_sq= dT_im1jk_np1half   *dT_im1jk_np1half;
  double dT_im1jk_np1half_4 = dT_im1jk_np1half_sq*dT_im1jk_np1half_sq;
  
  double dT_ijp1k_np1half   =(dT_ijp1k_np1+grid.dLocalGridOld[grid.nT][i][j+1][k])*0.5;
  double dT_ijp1k_np1half_sq= dT_ijp1k_np1half   *dT_ijp1k_np1half;
  double dT_ijp1k_np1half_4 = dT_ijp1k_np1half_sq*dT_ijp1k_np1half_sq;
  
  double dT_ijm1k_np1half   =(dT_ijm1k_np1+grid.dLocalGridOld[grid.nT][i][j-1][k])*0.5;
  double dT_ijm1k_np1half_sq= dT_ijm1k_np1half   *dT_ijm1k_np1half;
  double dT_ijm1k_np1half_4 = dT_ijm1k_np1half_sq*dT_ijm1k_np1half_sq;
  
  double dT_ijkp1_np1half   =(dT_ijkp1_np1+grid.dLocalGridOld[grid.nT][i][j][k+1])*0.5;
  double dT_ijkp1_np1half_sq= dT_ijkp1_np1half   *dT_ijkp1_np1half;
  double dT_ijkp1_np1half_4 = dT_ijkp1_np1half_sq*dT_ijkp1_np1half_sq;
  
  double dT_ijkm1_np1half   =(dT_ijkm1_np1+grid.dLocalGridOld[grid.nT][i][j][k-1])*0.5;
  double dT_ijkm1_np1half_sq= dT_ijkm1_np1half   *dT_ijkm1_np1half;
  double dT_ijkm1_np1half_4 = dT_ijkm1_np1half_sq*dT_ijkm1_np1half_sq;
  
  double dE_ijk_np1=parameters.eosTable.dGetEnergy(dT_ijk_np1,grid.dLocalGridNew[grid.nD][i][j][k]);
  double dE_ijk_np1half=parameters.eosTable.dGetEnergy(dT_ijk_np1half,grid.dLocalGridOld[grid.nD][i][j][k]);
  double dE_im1jk_np1half=parameters.eosTable.dGetEnergy(dT_im1jk_np1half,grid.dLocalGridOld[grid.nD][i-1][j][k]);
  double dE_ijp1k_np1half=parameters.eosTable.dGetEnergy(dT_ijp1k_np1half,grid.dLocalGridOld[grid.nD][i][j+1][k]);
  double dE_ijm1k_np1half=parameters.eosTable.dGetEnergy(dT_ijm1k_np1half,grid.dLocalGridOld[grid.nD][i][j-1][k]);
  double dE_ijkp1_np1half=parameters.eosTable.dGetEnergy(dT_ijkp1_np1half,grid.dLocalGridOld[grid.nD][i][j][k+1]);
  double dE_ijkm1_np1half=parameters.eosTable.dGetEnergy(dT_ijkm1_np1half,grid.dLocalGridOld[grid.nD][i][j][k-1]);
  
  double dE_ip1halfjk_np1half=dE_ijk_np1half;/**\BC Using $E_{i,j,k}^{n+1/2}$ for $E_{i+1/2,j,k}^{n+1/2}$*/
  double dE_im1halfjk_np1half=(dE_im1jk_np1half+dE_ijk_np1half)*0.5;
  double dE_ijp1halfk_np1half=(dE_ijp1k_np1half+dE_ijk_np1half)*0.5;
  double dE_ijm1halfk_np1half=(dE_ijm1k_np1half+dE_ijk_np1half)*0.5;
  double dE_ijkp1half_np1half=(dE_ijkp1_np1half+dE_ijk_np1half)*0.5;
  double dE_ijkm1half_np1half=(dE_ijkm1_np1half+dE_ijk_np1half)*0.5;
  
  double dP_ijk_np1half=parameters.eosTable.dGetPressure(dT_ijk_np1half,grid.dLocalGridOld[grid.nD][i][j][k]);
  double dPi_ijk_np1half=dP_ijk_np1half+grid.dLocalGridOld[grid.nQ0][i][j][k];
  double dPj_ijk_np1half=dP_ijk_np1half+grid.dLocalGridOld[grid.nQ1][i][j][k];
  double dPk_ijk_np1half=dP_ijk_np1half+grid.dLocalGridOld[grid.nQ2][i][j][k];
  
  double dKappa_ijk_np1half=parameters.eosTable.dGetOpacity(dT_ijk_np1half,grid.dLocalGridOld[grid.nD][i][j][k]);
  double dKappa_im1jk_np1half=parameters.eosTable.dGetOpacity(dT_im1jk_np1half,grid.dLocalGridOld[grid.nD][i-1][j][k]);
  double dKappa_ijp1k_np1half=parameters.eosTable.dGetOpacity(dT_ijp1k_np1half,grid.dLocalGridOld[grid.nD][i][j+1][k]);
  double dKappa_ijm1k_np1half=parameters.eosTable.dGetOpacity(dT_ijm1k_np1half,grid.dLocalGridOld[grid.nD][i][j-1][k]);
  double dKappa_ijkp1_np1half=parameters.eosTable.dGetOpacity(dT_ijkp1_np1half,grid.dLocalGridOld[grid.nD][i][j][k+1]);
  double dKappa_ijkm1_np1half=parameters.eosTable.dGetOpacity(dT_ijkm1_np1half,grid.dLocalGridOld[grid.nD][i][j][k-1]);
  
  double dKappa_im1halfjk_n_np1half=(dT_im1jk_np1half_4+dT_ijk_np1half_4)/(dT_ijk_np1half_4/dKappa_ijk_np1half+dT_im1jk_np1half_4/dKappa_im1jk_np1half);
  double dKappa_ijp1halfk_n_np1half=(dT_ijp1k_np1half_4+dT_ijk_np1half_4)/(dT_ijk_np1half_4/dKappa_ijk_np1half+dT_ijp1k_np1half_4/dKappa_ijp1k_np1half);
  double dKappa_ijm1halfk_n_np1half=(dT_ijm1k_np1half_4+dT_ijk_np1half_4)/(dT_ijk_np1half_4/dKappa_ijk_np1half+dT_ijm1k_np1half_4/dKappa_ijm1k_np1half);
  double dKappa_ijkp1half_np1half=(dT_ijkp1_np1half_4+dT_ijk_np1half_4)/(dT_ijk_np1half_4/dKappa_ijk_np1half+dT_ijkp1_np1half_4/dKappa_ijkp1_np1half);
  double dKappa_ijkm1half_np1half=(dT_ijkm1_np1half_4+dT_ijk_np1half_4)/(dT_ijk_np1half_4/dKappa_ijk_np1half+dT_ijkm1_np1half_4/dKappa_ijkm1_np1half);
  
  //Calcuate dA1
  double dA1CenGrad=(dE_ip1halfjk_np1half-dE_im1halfjk_np1half)/grid.dLocalGridOld[grid.nDM][i][0][0];
  double dA1UpWindGrad=0.0;
  double dU_U0_Diff=(dU_ijk_np1half-dU0_i_np1half);
  if(dU_U0_Diff<0.0){//moving in the negative direction
    dA1UpWindGrad=dA1UpWindGrad;/**\BC Using centered gradient for upwind gradient when motion is 
      into the star at the surface*/
  }
  else{//moving in the postive direction
    dA1UpWindGrad=(dE_ijk_np1half-dE_im1jk_np1half)/(grid.dLocalGridOld[grid.nDM][i][0][0]
      +grid.dLocalGridOld[grid.nDM][i-1][0][0])*2.0;
  }
  double dA1=dU_U0_Diff*dRSq_i_n*((1.0-parameters.dDonorFrac)*dA1CenGrad
    +parameters.dDonorFrac*dA1UpWindGrad);
  
  //calculate dS1
  double dUR2_im1half_np1half=grid.dLocalGridNew[grid.nU][nIInt-1][j][k]
    *dR_im1half_n*dR_im1half_n;
  double dUR2_ip1half_np1half=grid.dLocalGridNew[grid.nU][nIInt][j][k]*dR_ip1half_n*dR_ip1half_n;
  double dS1=dPi_ijk_np1half/grid.dLocalGridOld[grid.nD][i][j][k]
    *(dUR2_ip1half_np1half-dUR2_im1half_np1half)/grid.dLocalGridOld[grid.nDM][i][0][0];
  
  //Calcualte dA2
  double dA2CenGrad=(dE_ijp1halfk_np1half-dE_ijm1halfk_np1half)/grid.dLocalGridOld[grid.nDTheta][0][j][0];
  double dA2UpWindGrad=0.0;
  if(dV_ijk_np1half<0.0){//moving in the negative direction
    dA2UpWindGrad=(dE_ijp1k_np1half-dE_ijk_np1half)/(grid.dLocalGridOld[grid.nDTheta][0][j+1][0]
      +grid.dLocalGridOld[grid.nDTheta][0][j][0])*2.0;
  }
  else{//moving in the positive direction
    dA2UpWindGrad=(dE_ijk_np1half-dE_ijm1k_np1half)/(grid.dLocalGridOld[grid.nDTheta][0][j][0]
      +grid.dLocalGridOld[grid.nDTheta][0][j-1][0])*2.0;
  }
  double dA2=dV_ijk_np1half/dR_i_n*((1.0-parameters.dDonorFrac)*dA2CenGrad
    +parameters.dDonorFrac*dA2UpWindGrad);
    
  //Calcualte dS2
  double dS2=dPj_ijk_np1half/(grid.dLocalGridOld[grid.nD][i][j][k]*dR_i_n
    *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]*grid.dLocalGridOld[grid.nDTheta][0][j][0])
    *(dVSinTheta_ijp1halfk_np1half-dVSinTheta_ijm1halfk_np1half);
  
  //Calcualte dA3
  double dA3CenGrad=(dE_ijkp1half_np1half-dE_ijkm1half_np1half)/grid.dLocalGridOld[grid.nDPhi][0][0][k];
  double dA3UpWindGrad=0.0;
  if(dW_ijk_np1half<0.0){//moving in the negative direction
    dA3UpWindGrad=(dE_ijkp1_np1half-dE_ijk_np1half)/(grid.dLocalGridOld[grid.nDPhi][0][0][k+1]
      +grid.dLocalGridOld[grid.nDPhi][0][0][k])*2.0;
  }
  else{//moving in the positive direction
    dA3UpWindGrad=(dE_ijk_np1half-dE_ijkm1_np1half)/(grid.dLocalGridOld[grid.nDPhi][0][0][k]
      +grid.dLocalGridOld[grid.nDPhi][0][0][k-1])*2.0;
  }
  double dA3=dW_ijk_np1half/(dR_i_n*grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0])*
    ((1.0-parameters.dDonorFrac)*dA3CenGrad+parameters.dDonorFrac*dA3UpWindGrad);
  
  //Calcualte dS3
  double dS3=dPk_ijk_np1half/(grid.dLocalGridOld[grid.nD][i][j][k]*dR_i_n
    *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]*grid.dLocalGridOld[grid.nDPhi][0][0][k])
    *(dW_ijkp1half_np1half-dW_ijkm1half_np1half);
  
  //Calculate dS4
  double dTGrad_im1half_np1half=(dT_ijk_np1half_4-dT_im1jk_np1half_4)/(grid.dLocalGridOld[grid.nDM][i][0][0]+grid.dLocalGridOld[grid.nDM][i-1][0][0])*2.0;
  double dGrad_ip1half_np1half=-3.0*dRSq_ip1half*dT_ijk_np1half_4/(8.0*parameters.dPi);/**\BC 
    Missing grid.dLocalGridOld[grid.nT][i+1][0][0] using flux equals \f$2\sigma T^4\f$ at surface.*/
  double dGrad_im1half_np1half=dRhoAve_im1half*dR_im1half_4/(dKappa_im1halfjk_n_np1half*dRho_im1halfjk)*dTGrad_im1half_np1half;
  double dS4=16.0*parameters.dPi*parameters.dPi*grid.dLocalGridOld[grid.nDenAve][i][0][0]
    *(dGrad_ip1half_np1half-dGrad_im1half_np1half)/grid.dLocalGridOld[grid.nDM][i][0][0];
  
  //Calculate dS5
  double dTGrad_jp1half_np1half=(dT_ijp1k_np1half_4-dT_ijk_np1half_4)/(grid.dLocalGridOld[grid.nDTheta][0][j+1][0]+grid.dLocalGridOld[grid.nDTheta][0][j][0])*2.0;
  double dTGrad_jm1half_np1half=(dT_ijk_np1half_4-dT_ijm1k_np1half_4)/(grid.dLocalGridOld[grid.nDTheta][0][j][0]+grid.dLocalGridOld[grid.nDTheta][0][j-1][0])*2.0;
  double dGrad_jp1half_np1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt][0]/(dKappa_ijp1halfk_n_np1half*dRho_ijp1halfk*dR_i_n)*dTGrad_jp1half_np1half;
  double dGrad_jm1half_np1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt-1][0]/(dKappa_ijm1halfk_n_np1half*dRho_ijm1halfk*dR_i_n)*dTGrad_jm1half_np1half;
  double dS5=(dGrad_jp1half_np1half-dGrad_jm1half_np1half)/(grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]
    *dR_i_n*grid.dLocalGridOld[grid.nDTheta][0][j][0]);
  
  //Calculate dS6
  double dTGrad_kp1half_np1half=(dT_ijkp1_np1half_4-dT_ijk_np1half_4)/(grid.dLocalGridOld[grid.nDPhi][0][0][k+1]+grid.dLocalGridOld[grid.nDPhi][0][0][k])*2.0;
  double dTGrad_km1half_np1half=(dT_ijk_np1half_4-dT_ijkm1_np1half_4)/(grid.dLocalGridOld[grid.nDPhi][0][0][k]+grid.dLocalGridOld[grid.nDPhi][0][0][k-1])*2.0;
  double dGrad_kp1half_np1half=dTGrad_kp1half_np1half/(dKappa_ijkp1half_np1half*dRho_ijkp1half*dR_i_n);
  double dGrad_km1half_np1half=dTGrad_km1half_np1half/(dKappa_ijkm1half_np1half*dRho_ijkm1half*dR_i_n);
  double dS6=(dGrad_kp1half_np1half-dGrad_km1half_np1half)/(dR_i_n*grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]*grid.dLocalGridOld[grid.nDPhi][0][0][k]);
  
  //calculate new energy
  return (dE_ijk_np1-grid.dLocalGridOld[grid.nE][i][j][k])/time.dDeltat_n
    +4.0*parameters.dPi*grid.dLocalGridOld[grid.nDenAve][i][0][0]*(dA1+dS1)+dA2+dS2+dA3+dS3
    -4.0*parameters.dSigma/(3.0*grid.dLocalGridOld[grid.nD][i][j][k])*(dS4+dS5+dS6);
}
double dImplicitEnergyFunction_R_LES(Grid &grid,Parameters &parameters,Time &time,double dTemps[]
  ,int i,int j,int k){
  
  double dT_ijk_np1=dTemps[0];
  double dT_ip1jk_np1=dTemps[1];
  double dT_im1jk_np1=dTemps[2];
  
  //Calculate interpolated quantities
  int nIInt=i+grid.nCenIntOffset[0];
  double dU_ijk_np1half=(grid.dLocalGridNew[grid.nU][nIInt][j][k]
    +grid.dLocalGridNew[grid.nU][nIInt-1][j][k])*0.5;
  double dU_im1jk_np1half=(grid.dLocalGridNew[grid.nU][nIInt-1][j][k]
    +grid.dLocalGridNew[grid.nU][nIInt-2][j][k])*0.5;
  double dU0_i_np1half=(grid.dLocalGridNew[grid.nU0][nIInt][0][0]
    +grid.dLocalGridNew[grid.nU0][nIInt-1][0][0])*0.5;
  double dR_i_np1half=(grid.dLocalGridOld[grid.nR][nIInt][0][0]
    +grid.dLocalGridOld[grid.nR][nIInt-1][0][0]+grid.dLocalGridNew[grid.nR][nIInt][0][0]
    +grid.dLocalGridNew[grid.nR][nIInt-1][0][0])*0.25;
  double dR_im1_np1half=(grid.dLocalGridOld[grid.nR][nIInt-1][0][0]
    +grid.dLocalGridOld[grid.nR][nIInt-2][0][0]+grid.dLocalGridNew[grid.nR][nIInt-1][0][0]
    +grid.dLocalGridNew[grid.nR][nIInt-2][0][0])*0.25;
  double dRSq_im1_np1half=dR_im1_np1half*dR_im1_np1half;
  double dR_im1half_np1half=(grid.dLocalGridOld[grid.nR][nIInt-1][0][0]
    +grid.dLocalGridNew[grid.nR][nIInt-1][0][0])*0.5;
  double dR_ip1half_np1half=(grid.dLocalGridOld[grid.nR][nIInt][0][0]
    +grid.dLocalGridNew[grid.nR][nIInt][0][0])*0.5;
  double dRSq_i_np1half=dR_i_np1half*dR_i_np1half;
  double dRSq_ip1half_np1half=dR_ip1half_np1half*dR_ip1half_np1half;
  double dR4_ip1half_np1half=dRSq_ip1half_np1half*dRSq_ip1half_np1half;
  double dRSq_im1half_np1half=dR_im1half_np1half*dR_im1half_np1half;
  double dR4_im1half_np1half=dRSq_im1half_np1half*dRSq_im1half_np1half;
  double dRhoAve_ip1half_np1half=(grid.dLocalGridOld[grid.nD][i+1][0][0]
    +grid.dLocalGridOld[grid.nD][i][0][0])*0.5;
  double dRhoAve_im1half_np1half=(grid.dLocalGridOld[grid.nD][i][0][0]
    +grid.dLocalGridOld[grid.nD][i-1][0][0])*0.5;
  double dRho_ip1halfjk_np1half=(grid.dLocalGridOld[grid.nD][i+1][j][k]
    +grid.dLocalGridOld[grid.nD][i][j][k])*0.5;
  double dRho_im1halfjk_np1half=(grid.dLocalGridOld[grid.nD][i][j][k]
    +grid.dLocalGridOld[grid.nD][i-1][j][k])*0.5;
  double dDM_ip1half=(grid.dLocalGridOld[grid.nDM][i+1][0][0]+grid.dLocalGridOld[grid.nDM][i][0][0])*0.5;
  double dDM_im1half=(grid.dLocalGridOld[grid.nDM][i-1][0][0]+grid.dLocalGridOld[grid.nDM][i][0][0])*0.5;
        
  double dEddyVisc_ip1half_np1half=(grid.dLocalGridNew[grid.nEddyVisc][i][j][k]+grid.dLocalGridNew[grid.nEddyVisc][i+1][j][k])*0.5;
  double dEddyVisc_im1half_np1half=(grid.dLocalGridNew[grid.nEddyVisc][i][j][k]
    +grid.dLocalGridNew[grid.nEddyVisc][i-1][j][k])*0.5;
  
  double dT_ip1jk_np1half=(dT_ip1jk_np1+grid.dLocalGridOld[grid.nT][i+1][j][k])*0.5;
  double dT_ip1jk_np1half_sq=dT_ip1jk_np1half*dT_ip1jk_np1half;
  double dT_ip1jk_np1half_4=dT_ip1jk_np1half_sq*dT_ip1jk_np1half_sq;
  
  double dT_ijk_np1half=(dT_ijk_np1+grid.dLocalGridOld[grid.nT][i][j][k])*0.5;
  double dT_ijk_np1half_sq=dT_ijk_np1half*dT_ijk_np1half;
  double dT_ijk_np1half_4=dT_ijk_np1half_sq*dT_ijk_np1half_sq;
  
  double dT_im1jk_np1half=(dT_im1jk_np1+grid.dLocalGridOld[grid.nT][i-1][j][k])*0.5;
  double dT_im1jk_np1half_sq=dT_im1jk_np1half*dT_im1jk_np1half;
  double dT_im1jk_np1half_4=dT_im1jk_np1half_sq*dT_im1jk_np1half_sq;
  
  double dE_ijk_np1=parameters.eosTable.dGetEnergy(dT_ijk_np1
    ,grid.dLocalGridNew[grid.nD][i][j][k]);
  double dE_ip1jk_np1half=parameters.eosTable.dGetEnergy(dT_ip1jk_np1half
    ,grid.dLocalGridOld[grid.nD][i+1][j][k]);
  double dE_ijk_np1half=parameters.eosTable.dGetEnergy(dT_ijk_np1half
    ,grid.dLocalGridOld[grid.nD][i][j][k]);
  double dE_im1jk_np1half=parameters.eosTable.dGetEnergy(dT_im1jk_np1half
    ,grid.dLocalGridOld[grid.nD][i-1][j][k]);
  
  double dE_ip1halfjk_np1half=(dE_ip1jk_np1half+dE_ijk_np1half)*0.5;
  double dE_im1halfjk_np1half=(dE_ijk_np1half+dE_im1jk_np1half)*0.5;
  
  double dPi_ijk_np1half=parameters.eosTable.dGetPressure(dT_ijk_np1half
    ,grid.dLocalGridOld[grid.nD][i][j][k])+grid.dLocalGridOld[grid.nQ0][i][j][k];
  
  double dKappa_ip1jk_np1half=parameters.eosTable.dGetOpacity(dT_ip1jk_np1half
    ,grid.dLocalGridOld[grid.nD][i+1][j][k]);
  double dKappa_ijk_np1half=parameters.eosTable.dGetOpacity(dT_ijk_np1half
    ,grid.dLocalGridOld[grid.nD][i][j][k]);
  double dKappa_im1jk_np1half=parameters.eosTable.dGetOpacity(dT_im1jk_np1half
    ,grid.dLocalGridOld[grid.nD][i-1][j][k]);
  
  double dKappa_ip1halfjk_n_np1half=(dT_ip1jk_np1half_4+dT_ijk_np1half_4)
    /(dT_ijk_np1half_4/dKappa_ijk_np1half+dT_ip1jk_np1half_4/dKappa_ip1jk_np1half);
  double dKappa_im1halfjk_n_np1half=(dT_im1jk_np1half_4+dT_ijk_np1half_4)
    /(dT_ijk_np1half_4/dKappa_ijk_np1half+dT_im1jk_np1half_4/dKappa_im1jk_np1half);
    
  //Calcuate dA1
  double dA1CenGrad=(dE_ip1halfjk_np1half-dE_im1halfjk_np1half)
    /grid.dLocalGridOld[grid.nDM][i][0][0];
  double dA1UpWindGrad=0.0;
  double dU_U0_Diff=(dU_ijk_np1half-dU0_i_np1half);
  if(dU_U0_Diff<0.0){//moving in the negative direction
    dA1UpWindGrad=(dE_ip1jk_np1half-dE_ijk_np1half)/(grid.dLocalGridOld[grid.nDM][i+1][0][0]
      +grid.dLocalGridOld[grid.nDM][i][0][0])*2.0;
  }
  else{//moving in the postive direction
    dA1UpWindGrad=(dE_ijk_np1half-dE_im1jk_np1half)/(grid.dLocalGridOld[grid.nDM][i][0][0]
      +grid.dLocalGridOld[grid.nDM][i-1][0][0])*2.0;
  }
  double dA1=dU_U0_Diff*dRSq_i_np1half*((1.0-parameters.dDonorFrac)*dA1CenGrad
    +parameters.dDonorFrac*dA1UpWindGrad);
  
  //calculate dUR2_im1half_np1half
  double dUR2_im1halfjk_np1half=grid.dLocalGridNew[grid.nU][nIInt-1][j][k]*dRSq_im1half_np1half;
  
  //calculate dR2_ip1half_np1half
  double dUR2_ip1halfjk_np1half=grid.dLocalGridNew[grid.nU][nIInt][j][k]*dRSq_ip1half_np1half;
  
  //calculate dURsq_ijk_np1half
  double dUR2_ijk_np1half=dU_ijk_np1half*dRSq_i_np1half;
  
  //calculate dURsq_im1jk_np1half
  double dUR2_im1jk_np1half=dU_im1jk_np1half*dRSq_im1_np1half;
  
  //calculate dS1
  double dS1=dPi_ijk_np1half/grid.dLocalGridOld[grid.nD][i][j][k]
    *(dUR2_ip1halfjk_np1half-dUR2_im1halfjk_np1half)/grid.dLocalGridOld[grid.nDM][i][0][0];
  
  //Calculate dS4
  double dTGrad_ip1half_np1half=(dT_ip1jk_np1half_4-dT_ijk_np1half_4)
    /(grid.dLocalGridOld[grid.nDM][i+1][0][0]+grid.dLocalGridOld[grid.nDM][i][0][0])*2.0;
  double dTGrad_im1half_np1half=(dT_ijk_np1half_4-dT_im1jk_np1half_4)
    /(grid.dLocalGridOld[grid.nDM][i][0][0]+grid.dLocalGridOld[grid.nDM][i-1][0][0])*2.0;
  double dGrad_ip1half_np1half=dRhoAve_ip1half_np1half*dR4_ip1half_np1half
    /(dKappa_ip1halfjk_n_np1half*dRho_ip1halfjk_np1half)*dTGrad_ip1half_np1half;
  double dGrad_im1half_np1half=dRhoAve_im1half_np1half*dR4_im1half_np1half
    /(dKappa_im1halfjk_n_np1half*dRho_im1halfjk_np1half)*dTGrad_im1half_np1half;
  double dS4=16.0*parameters.dPi*parameters.dPi*grid.dLocalGridOld[grid.nD][i][0][0]
    *(dGrad_ip1half_np1half-dGrad_im1half_np1half)/grid.dLocalGridOld[grid.nDM][i][0][0];
       
  //calculate dDivU_ip1halfjk_np1half
  double dDivU_ip1halfjk_np1half=4.0*parameters.dPi*dRhoAve_ip1half_np1half*(dUR2_ip1halfjk_np1half
    -dUR2_ijk_np1half)/dDM_ip1half;
  
  //calculate dDivU_im1halfjk_np1half
  double dDivU_im1halfjk_np1half=4.0*parameters.dPi*dRhoAve_im1half_np1half*(dUR2_ijk_np1half
    -dUR2_im1jk_np1half)/dDM_im1half;
  
  //calculate dTau_rr_ip1halfjk_np1half
  double dTau_rr_ip1halfjk_np1half=2.0*dEddyVisc_ip1half_np1half*(4.0*parameters.dPi
    *dRSq_ip1half_np1half*dRhoAve_ip1half_np1half*(grid.dLocalGridNew[grid.nU][nIInt][j][k]
    -dU_ijk_np1half)/dDM_ip1half-0.333333333333333*dDivU_ip1halfjk_np1half);
  
  //calculate dTau_rr_im1halfjk_np1half
  double dTau_rr_im1halfjk_np1half=2.0*dEddyVisc_im1half_np1half*(4.0*parameters.dPi
    *dRSq_im1half_np1half*dRhoAve_im1half_np1half*(dU_ijk_np1half-dU_im1jk_np1half)
    /dDM_im1half-0.333333333333333*dDivU_im1halfjk_np1half);
  
  //calculate dTauVR2_ip1halfjk_np1half
  double dTauVR2_ip1halfjk_np1half=dRSq_ip1half_np1half*dTau_rr_ip1halfjk_np1half
    *grid.dLocalGridNew[grid.nU][nIInt][j][k];
  
  //calculate dTauVR2_im1halfjk_np1half
  double dTauVR2_im1halfjk_np1half=dRSq_im1half_np1half*dTau_rr_im1halfjk_np1half
    *grid.dLocalGridNew[grid.nU][nIInt-1][j][k];
  
  //calculate dT1
  double dT1=(dTauVR2_ip1halfjk_np1half-dTauVR2_im1halfjk_np1half)
    /(grid.dLocalGridOld[grid.nDM][i][0][0]*grid.dLocalGridOld[grid.nD][i][j][k]);
          
  //calculate new energy
  return (dE_ijk_np1-grid.dLocalGridOld[grid.nE][i][j][k])/time.dDeltat_n
    +4.0*parameters.dPi*grid.dLocalGridOld[grid.nD][i][0][0]*(dA1+dS1-dT1)
    -4.0*parameters.dSigma/(3.0*grid.dLocalGridOld[grid.nD][i][j][k])*(dS4);
}
double dImplicitEnergyFunction_R_LES_SB(Grid &grid,Parameters &parameters,Time &time,double dTemps[]
  ,int i,int j,int k){
  
  double dT_ijk_np1=dTemps[0];
  double dT_im1jk_np1=dTemps[1];
  int nIInt=i+grid.nCenIntOffset[0];
  
  //Calculate interpolated quantities
  double dU_ijk_np1half=(grid.dLocalGridNew[grid.nU][nIInt][j][k]
    +grid.dLocalGridNew[grid.nU][nIInt-1][j][k])*0.5;
  double dU_im1jk_np1half=(grid.dLocalGridNew[grid.nU][nIInt-2][j][k]
    +grid.dLocalGridNew[grid.nU][nIInt-1][j][k])*0.5;
  double dU0_i_np1half=(grid.dLocalGridNew[grid.nU0][nIInt][0][0]
    +grid.dLocalGridNew[grid.nU0][nIInt-1][0][0])*0.5;
    
  double dR_ip1half_np1half=(grid.dLocalGridOld[grid.nR][nIInt][0][0]+grid.dLocalGridNew[grid.nR][nIInt][0][0])*0.5;
  double dRSq_ip1half_np1half=dR_ip1half_np1half*dR_ip1half_np1half;
  double dR_im1_np1half=(grid.dLocalGridOld[grid.nR][nIInt-1][0][0]+grid.dLocalGridOld[grid.nR][nIInt-2][0][0]+grid.dLocalGridNew[grid.nR][nIInt-1][0][0]+grid.dLocalGridNew[grid.nR][nIInt-2][0][0])*0.25;
  double dRSq_im1_np1half=dR_im1_np1half*dR_im1_np1half;
  double dR_im1half_np1half=(grid.dLocalGridOld[grid.nR][nIInt-1][0][0]+grid.dLocalGridNew[grid.nR][nIInt-1][0][0])*0.5;
  double dRSq_im1half_np1half=dR_im1half_np1half*dR_im1half_np1half;
  double dR4_im1half_np1half=dRSq_im1half_np1half*dRSq_im1half_np1half;
  double dR_i_np1half=(dR_ip1half_np1half+dR_im1half_np1half)*0.5;
  double dRSq_i_np1half=dR_i_np1half*dR_i_np1half;
  double dDM_ip1half=(0.0+grid.dLocalGridOld[grid.nDM][i][0][0])*0.5;
  double dDM_im1half=(grid.dLocalGridOld[grid.nDM][i][0][0]+grid.dLocalGridOld[grid.nDM][i-1][0][0])*0.5;
  double dRho_im1halfjk=(grid.dLocalGridOld[grid.nD][i][j][k]
    +grid.dLocalGridOld[grid.nD][i-1][j][k])*0.5;
  double dRhoAve_i=grid.dLocalGridOld[grid.nD][i][0][0];
  double dRhoAve_ip1=0.0;
  double dRhoAve_im1=grid.dLocalGridOld[grid.nD][i-1][0][0];
  double dRhoAve_ip1half=(dRhoAve_i+dRhoAve_ip1)*0.5;
  double dRhoAve_im1half=(dRhoAve_i+dRhoAve_im1)*0.5;
  
  double dT_ijk_np1half=(dT_ijk_np1+grid.dLocalGridOld[grid.nT][i][j][k])*0.5;
  double dT_ijk_np1half_sq=dT_ijk_np1half*dT_ijk_np1half;
  double dT_ijk_np1half_4=dT_ijk_np1half_sq*dT_ijk_np1half_sq;
  
  double dT_im1jk_np1half=(dT_im1jk_np1+grid.dLocalGridOld[grid.nT][i-1][j][k])*0.5;
  double dT_im1jk_np1half_sq=dT_im1jk_np1half*dT_im1jk_np1half;
  double dT_im1jk_np1half_4=dT_im1jk_np1half_sq*dT_im1jk_np1half_sq;
  
  double dE_ijk_np1=parameters.eosTable.dGetEnergy(dT_ijk_np1
    ,grid.dLocalGridNew[grid.nD][i][j][k]);
  double dE_ijk_np1half=parameters.eosTable.dGetEnergy(dT_ijk_np1half
    ,grid.dLocalGridOld[grid.nD][i][j][k]);
  double dE_im1jk_np1half=parameters.eosTable.dGetEnergy(dT_im1jk_np1half
    ,grid.dLocalGridOld[grid.nD][i][j][k]);
  
  double dE_ip1halfjk_np1half=dE_ijk_np1half;/**\BC Missing
    grid.dLocalGridOld[grid.nE][i+1][j][k] in calculation of \f$E_{i+1/2,j,k}\f$ 
    setting it equal to value at i.*/
  double dE_im1halfjk_np1half=(dE_ijk_np1half+dE_im1jk_np1half)*0.5;
        
  double dEddyVisc_ip1half_np1half=(grid.dLocalGridNew[grid.nEddyVisc][i][j][k])*0.5;
  double dEddyVisc_im1half_np1half=(grid.dLocalGridNew[grid.nEddyVisc][i][j][k]
    +grid.dLocalGridNew[grid.nEddyVisc][i-1][j][k])*0.5;
  double dKappa_ijk_np1half=parameters.eosTable.dGetOpacity(dT_ijk_np1half
    ,grid.dLocalGridOld[grid.nD][i][j][k]);
  double dKappa_im1jk_np1half=parameters.eosTable.dGetOpacity(dT_im1jk_np1half
    ,grid.dLocalGridOld[grid.nD][i-1][j][k]);
  
  double dKappa_im1halfjk_n_np1half=(dT_im1jk_np1half_4+dT_ijk_np1half_4)
    /(dT_ijk_np1half_4/dKappa_ijk_np1half+dT_im1jk_np1half_4/dKappa_im1jk_np1half);
    
  double dP_ijk_np1half=parameters.eosTable.dGetPressure(dT_ijk_np1half
    ,grid.dLocalGridOld[grid.nD][i][j][k]);
  
  //Calcuate dA1
  double dA1CenGrad=(dE_ip1halfjk_np1half-dE_im1halfjk_np1half)
    /grid.dLocalGridOld[grid.nDM][i][0][0];
  double dA1UpWindGrad=0.0;
  double dU_U0_Diff=(dU_ijk_np1half-dU0_i_np1half);
  if(dU_U0_Diff<0.0){//moving in the negative direction
    dA1UpWindGrad=dA1CenGrad;/**\BC grid.dLocalGridOld[grid.nDM][i+1][0][0] and 
      grid.dLocalGridOld[grid.nE][i+1][j][k] missing in the calculation of upwind gradient in 
      dA1. Using the centered gradient instead.*/
  }
  else{//moving in the postive direction*/
    dA1UpWindGrad=(dE_ijk_np1half-dE_im1jk_np1half)/(grid.dLocalGridOld[grid.nDM][i][0][0]
      +grid.dLocalGridOld[grid.nDM][i-1][0][0])*2.0;
  }
  double dA1=dU_U0_Diff*dRSq_i_np1half*((1.0-parameters.dDonorFrac)*dA1CenGrad
    +parameters.dDonorFrac*dA1UpWindGrad);
  
  //calculate dUR2_im1half_np1half
  double dUR2_im1halfjk_np1half=grid.dLocalGridNew[grid.nU][nIInt-1][j][k]*dRSq_im1half_np1half;
  
  //calculate dR2_ip1half_np1half
  double dUR2_ip1halfjk_np1half=grid.dLocalGridNew[grid.nU][nIInt][j][k]*dRSq_ip1half_np1half;
  
  //calculate dURsq_ijk_np1half
  double dUR2_ijk_np1half=dU_ijk_np1half*dRSq_i_np1half;
  
  //calculate dURsq_im1jk_np1half
  double dUR2_im1jk_np1half=dU_im1jk_np1half*dRSq_im1_np1half;
  
  //calculate dS1
  double dS1=dP_ijk_np1half/grid.dLocalGridOld[grid.nD][i][j][k]
    *(dUR2_ip1halfjk_np1half-dUR2_im1halfjk_np1half)/grid.dLocalGridOld[grid.nDM][i][0][0];
  
  //Calculate dS4
  double dTGrad_im1half_np1half=(dT_ijk_np1half_4-dT_im1jk_np1half_4)
    /(grid.dLocalGridOld[grid.nDM][i][0][0]+grid.dLocalGridOld[grid.nDM][i-1][0][0])*2.0;
  double dGrad_ip1half_np1half=-3.0*dRSq_ip1half_np1half*dT_ijk_np1half_4/(8.0*parameters.dPi);/**\BC
    Missing grid.dLocalGridOld[grid.nT][i+1][0][0]*/
  double dGrad_im1half_np1half=dRhoAve_im1half*dR4_im1half_np1half/(dKappa_im1halfjk_n_np1half
    *dRho_im1halfjk)*dTGrad_im1half_np1half;
  double dS4=16.0*parameters.dPi*parameters.dPi*grid.dLocalGridOld[grid.nD][i][0][0]
    *(dGrad_ip1half_np1half-dGrad_im1half_np1half)/grid.dLocalGridOld[grid.nDM][i][0][0];
  
  //calculate dDivU_ip1halfjk_np1half
  double dDivU_ip1halfjk_np1half=4.0*parameters.dPi*dRhoAve_ip1half*(dUR2_ip1halfjk_np1half
    -dUR2_ijk_np1half)/dDM_ip1half;
  
  //calculate dDivU_im1halfjk_np1half
  double dDivU_im1halfjk_np1half=4.0*parameters.dPi*dRhoAve_im1half*(dUR2_ijk_np1half
    -dUR2_im1jk_np1half)/dDM_im1half;
  
  //calculate dTau_rr_ip1halfjk_np1half
  double dTau_rr_ip1halfjk_np1half=2.0*dEddyVisc_ip1half_np1half*(4.0*parameters.dPi
    *dRSq_ip1half_np1half*dRhoAve_ip1half*(grid.dLocalGridNew[grid.nU][nIInt][j][k]
    -dU_ijk_np1half)/dDM_ip1half-0.333333333333333*dDivU_ip1halfjk_np1half);
  
  //calculate dTau_rr_im1halfjk_np1half
  double dTau_rr_im1halfjk_np1half=2.0*dEddyVisc_im1half_np1half*(4.0*parameters.dPi
    *dRSq_im1half_np1half*dRhoAve_im1half*(dU_ijk_np1half-dU_im1jk_np1half)
    /dDM_im1half-0.333333333333333*dDivU_im1halfjk_np1half);
  
  //calculate dTauVR2_ip1halfjk_np1half
  double dTauVR2_ip1halfjk_np1half=dRSq_ip1half_np1half*dTau_rr_ip1halfjk_np1half
    *grid.dLocalGridNew[grid.nU][nIInt][j][k];
  
  //calculate dTauVR2_im1halfjk_np1half
  double dTauVR2_im1halfjk_np1half=dRSq_im1half_np1half*dTau_rr_im1halfjk_np1half
    *grid.dLocalGridNew[grid.nU][nIInt-1][j][k];
  
  //calculate dT1
  double dT1=(dTauVR2_ip1halfjk_np1half-dTauVR2_im1halfjk_np1half)
    /(grid.dLocalGridOld[grid.nDM][i][0][0]*grid.dLocalGridOld[grid.nD][i][j][k]);
  
  //calculate new energy
  return (dE_ijk_np1-grid.dLocalGridOld[grid.nE][i][j][k])/time.dDeltat_n
    +(4.0*parameters.dPi*grid.dLocalGridOld[grid.nD][i][0][0]*(dA1+dS1-dT1)
    -4.0*parameters.dSigma/(3.0*grid.dLocalGridOld[grid.nD][i][j][k])*(dS4));
  
}
double dImplicitEnergyFunction_RT_LES(Grid &grid,Parameters &parameters,Time &time,double dTemps[]
  ,int i,int j,int k){
  
  double dT_ijk_np1=dTemps[0];
  double dT_ip1jk_np1=dTemps[1];
  double dT_im1jk_np1=dTemps[2];
  double dT_ijp1k_np1=dTemps[3];
  double dT_ijm1k_np1=dTemps[4];
  
  double dPiSq=parameters.dPi*parameters.dPi;
  
  //calculate i,j,k for interface centered quantities
  int nIInt=i+grid.nCenIntOffset[0];
  int nJInt=j+grid.nCenIntOffset[1];
  
  //Calculate interpolated quantities
  double dDM_ip1half=(grid.dLocalGridOld[grid.nDM][i][0][0]+grid.dLocalGridOld[grid.nDM][i+1][0][0])
    *0.5;
  double dDM_im1half=(grid.dLocalGridOld[grid.nDM][i][0][0]+grid.dLocalGridOld[grid.nDM][i-1][0][0])
    *0.5;
  double dDelTheta_jp1half=(grid.dLocalGridOld[grid.nDTheta][0][j][0]
    +grid.dLocalGridOld[grid.nDTheta][0][j+1][0])*0.5;
  double dDelTheta_jm1half=(grid.dLocalGridOld[grid.nDTheta][0][j][0]
    +grid.dLocalGridOld[grid.nDTheta][0][j-1][0])*0.5;
  double dU0_i_np1half=(grid.dLocalGridNew[grid.nU0][nIInt][0][0]
    +grid.dLocalGridNew[grid.nU0][nIInt-1][0][0])*0.5;
  double dR_ip1half_np1half=(grid.dLocalGridOld[grid.nR][nIInt][0][0]
    +grid.dLocalGridNew[grid.nR][nIInt][0][0])*0.5;
  double dR_im1half_np1half=(grid.dLocalGridOld[grid.nR][nIInt-1][0][0]
    +grid.dLocalGridNew[grid.nR][nIInt-1][0][0])*0.5;
  double dR_i_np1half=(dR_ip1half_np1half+dR_im1half_np1half)*0.5;
  double dRSq_i_np1half=dR_i_np1half*dR_i_np1half;
  double dRSq_ip1half_np1half=dR_ip1half_np1half*dR_ip1half_np1half;
  double dR4_ip1half_np1half=dRSq_ip1half_np1half*dRSq_ip1half_np1half;
  double dRSq_im1half_np1half=dR_im1half_np1half*dR_im1half_np1half;
  double dR4_im1half_np1half=dRSq_im1half_np1half*dRSq_im1half_np1half;
  double dRhoAve_ip1half_n=(grid.dLocalGridOld[grid.nDenAve][i+1][0][0]
    +grid.dLocalGridOld[grid.nDenAve][i][0][0])*0.5;
  double dRhoAve_im1half_n=(grid.dLocalGridOld[grid.nDenAve][i][0][0]
    +grid.dLocalGridOld[grid.nDenAve][i-1][0][0])*0.5;
  double dRho_ip1halfjk_n=(grid.dLocalGridOld[grid.nD][i+1][j][k]
    +grid.dLocalGridOld[grid.nD][i][j][k])*0.5;
  double dRho_im1halfjk_n=(grid.dLocalGridOld[grid.nD][i][j][k]
    +grid.dLocalGridOld[grid.nD][i-1][j][k])*0.5;
  double dRho_ijp1halfk_n=(grid.dLocalGridOld[grid.nD][i][j+1][k]
    +grid.dLocalGridOld[grid.nD][i][j][k])*0.5;
  double dRho_ijm1halfk_n=(grid.dLocalGridOld[grid.nD][i][j][k]
    +grid.dLocalGridOld[grid.nD][i][j-1][k])*0.5;
  double dU_ijk_np1half=(grid.dLocalGridNew[grid.nU][nIInt][j][k]
    +grid.dLocalGridNew[grid.nU][nIInt-1][j][k])*0.5;
  double dU_ijp1halfk_np1half=(grid.dLocalGridNew[grid.nU][nIInt][j+1][k]
    +grid.dLocalGridNew[grid.nU][nIInt-1][j+1][k]+grid.dLocalGridNew[grid.nU][nIInt][j][k]
    +grid.dLocalGridNew[grid.nU][nIInt-1][j][k])*0.25;
  double dU_ijm1halfk_np1half=(grid.dLocalGridNew[grid.nU][nIInt][j-1][k]
    +grid.dLocalGridNew[grid.nU][nIInt-1][j-1][k]+grid.dLocalGridNew[grid.nU][nIInt][j][k]
    +grid.dLocalGridNew[grid.nU][nIInt-1][j][k])*0.25;
  double dV_ijk_np1half=(grid.dLocalGridNew[grid.nV][i][nJInt][k]
    +grid.dLocalGridNew[grid.nV][i][nJInt-1][k])*0.5;
  double dV_ip1halfjk_np1half=(grid.dLocalGridNew[grid.nV][i+1][nJInt][k]
    +grid.dLocalGridNew[grid.nV][i+1][nJInt-1][k]+grid.dLocalGridNew[grid.nV][i][nJInt][k]
    +grid.dLocalGridNew[grid.nV][i][nJInt-1][k])*0.25;
  double dV_im1halfjk_np1half=(grid.dLocalGridNew[grid.nV][i][nJInt][k]
    +grid.dLocalGridNew[grid.nV][i][nJInt-1][k]+grid.dLocalGridNew[grid.nV][i-1][nJInt][k]
    +grid.dLocalGridNew[grid.nV][i-1][nJInt-1][k])*0.25;
  double dVSinTheta_ijp1halfk_np1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt][0]
    *grid.dLocalGridNew[grid.nV][i][nJInt][k];
  double dVSinTheta_ijm1halfk_np1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt-1][0]
    *grid.dLocalGridNew[grid.nV][i][nJInt-1][k];
  double dEddyVisc_ip1halfjk_n=(grid.dLocalGridNew[grid.nEddyVisc][i+1][j][k]
    +grid.dLocalGridNew[grid.nEddyVisc][i][j][k])*0.5;
  double dEddyVisc_im1halfjk_n=(grid.dLocalGridNew[grid.nEddyVisc][i-1][j][k]
    +grid.dLocalGridNew[grid.nEddyVisc][i][j][k])*0.5;
  double dEddyVisc_ijp1halfk_n=(grid.dLocalGridNew[grid.nEddyVisc][i][j+1][k]
  +grid.dLocalGridNew[grid.nEddyVisc][i][j][k])*0.5;
  double dEddyVisc_ijm1halfk_n=(grid.dLocalGridNew[grid.nEddyVisc][i][j-1][k]
    +grid.dLocalGridNew[grid.nEddyVisc][i][j][k])*0.5;
  
  double dT_ip1jk_np1half=(dT_ip1jk_np1+grid.dLocalGridOld[grid.nT][i+1][j][k])*0.5;
  double dTSq_ip1jk_np1half=dT_ip1jk_np1half*dT_ip1jk_np1half;
  double dT4_ip1jk_np1half=dTSq_ip1jk_np1half*dTSq_ip1jk_np1half;
  
  double dT_ijk_np1half=(dT_ijk_np1+grid.dLocalGridOld[grid.nT][i][j][k])*0.5;
  double dTSq_ijk_np1half=dT_ijk_np1half*dT_ijk_np1half;
  double dT4_ijk_np1half=dTSq_ijk_np1half*dTSq_ijk_np1half;
  
  double dT_im1jk_np1half=(dT_im1jk_np1+grid.dLocalGridOld[grid.nT][i-1][j][k])*0.5;
  double dTSq_im1jk_np1half=dT_im1jk_np1half*dT_im1jk_np1half;
  double dT4_im1jk_np1half = dTSq_im1jk_np1half*dTSq_im1jk_np1half;
  
  double dT_ijp1k_np1half=(dT_ijp1k_np1+grid.dLocalGridOld[grid.nT][i][j+1][k])*0.5;
  double dTSq_ijp1k_np1half=dT_ijp1k_np1half*dT_ijp1k_np1half;
  double dT4_ijp1k_np1half=dTSq_ijp1k_np1half*dTSq_ijp1k_np1half;
  
  double dT_ijm1k_np1half=(dT_ijm1k_np1+grid.dLocalGridOld[grid.nT][i][j-1][k])*0.5;
  double dTSq_ijm1k_np1half=dT_ijm1k_np1half*dT_ijm1k_np1half;
  double dT4_ijm1k_np1half=dTSq_ijm1k_np1half*dTSq_ijm1k_np1half;
  
  double dE_ijk_np1=parameters.eosTable.dGetEnergy(dT_ijk_np1
    ,grid.dLocalGridNew[grid.nD][i][j][k]);
  double dE_ip1jk_np1half=parameters.eosTable.dGetEnergy(dT_ip1jk_np1half
    ,grid.dLocalGridOld[grid.nD][i+1][j][k]);
  double dE_ijk_np1half=parameters.eosTable.dGetEnergy(dT_ijk_np1half
    ,grid.dLocalGridOld[grid.nD][i][j][k]);
  double dE_im1jk_np1half=parameters.eosTable.dGetEnergy(dT_im1jk_np1half
    ,grid.dLocalGridOld[grid.nD][i-1][j][k]);
  double dE_ijp1k_np1half=parameters.eosTable.dGetEnergy(dT_ijp1k_np1half
    ,grid.dLocalGridOld[grid.nD][i][j+1][k]);
  double dE_ijm1k_np1half=parameters.eosTable.dGetEnergy(dT_ijm1k_np1half
    ,grid.dLocalGridOld[grid.nD][i][j-1][k]);
  
  double dE_ip1halfjk_np1half=(dE_ip1jk_np1half+dE_ijk_np1half)*0.5;
  double dE_im1halfjk_np1half=(dE_im1jk_np1half+dE_ijk_np1half)*0.5;
  double dE_ijp1halfk_np1half=(dE_ijp1k_np1half+dE_ijk_np1half)*0.5;
  double dE_ijm1halfk_np1half=(dE_ijm1k_np1half+dE_ijk_np1half)*0.5;
  
  double dP_ijk_np1half=parameters.eosTable.dGetPressure(dT_ijk_np1half
    ,grid.dLocalGridOld[grid.nD][i][j][k]);
  #if VISCOUS_ENERGY_EQ==1
    dP_ijk_np1half=dP_ijk_np1half+grid.dLocalGridOld[grid.nQ0][i][j][k]
      +grid.dLocalGridOld[grid.nQ1][i][j][k];
  #endif
  
  double dKappa_ip1jk_np1half=parameters.eosTable.dGetOpacity(dT_ip1jk_np1half
    ,grid.dLocalGridOld[grid.nD][i+1][j][k]);
  double dKappa_ijk_np1half=parameters.eosTable.dGetOpacity(dT_ijk_np1half
    ,grid.dLocalGridOld[grid.nD][i][j][k]);
  double dKappa_im1jk_np1half=parameters.eosTable.dGetOpacity(dT_im1jk_np1half
    ,grid.dLocalGridOld[grid.nD][i-1][j][k]);
  double dKappa_ijp1k_np1half=parameters.eosTable.dGetOpacity(dT_ijp1k_np1half
    ,grid.dLocalGridOld[grid.nD][i][j+1][k]);
  double dKappa_ijm1k_np1half=parameters.eosTable.dGetOpacity(dT_ijm1k_np1half
    ,grid.dLocalGridOld[grid.nD][i][j-1][k]);
  
  double dKappa_ip1halfjk_np1half=(dT4_ip1jk_np1half+dT4_ijk_np1half)/(dT4_ijk_np1half
    /dKappa_ijk_np1half+dT4_ip1jk_np1half/dKappa_ip1jk_np1half);
  double dKappa_im1halfjk_np1half=(dT4_im1jk_np1half+dT4_ijk_np1half)/(dT4_ijk_np1half
    /dKappa_ijk_np1half+dT4_im1jk_np1half/dKappa_im1jk_np1half);
  double dKappa_ijp1halfk_np1half=(dT4_ijp1k_np1half+dT4_ijk_np1half)/(dT4_ijk_np1half
    /dKappa_ijk_np1half+dT4_ijp1k_np1half/dKappa_ijp1k_np1half);
  double dKappa_ijm1halfk_np1half=(dT4_ijm1k_np1half+dT4_ijk_np1half)/(dT4_ijk_np1half
    /dKappa_ijk_np1half+dT4_ijm1k_np1half/dKappa_ijm1k_np1half);
  
  //Calcuate dA1
  double dA1CenGrad=(dE_ip1halfjk_np1half-dE_im1halfjk_np1half)
    /grid.dLocalGridOld[grid.nDM][i][0][0];
  double dA1UpWindGrad=0.0;
  double dU_U0_Diff=(dU_ijk_np1half-dU0_i_np1half);
  if(dU_U0_Diff<0.0){//moving in the negative direction
    dA1UpWindGrad=(dE_ip1jk_np1half-dE_ijk_np1half)/(grid.dLocalGridOld[grid.nDM][i+1][0][0]
      +grid.dLocalGridOld[grid.nDM][i][0][0])*2.0;
  }
  else{//moving in the postive direction
    dA1UpWindGrad=(dE_ijk_np1half-dE_im1jk_np1half)/(grid.dLocalGridOld[grid.nDM][i][0][0]
      +grid.dLocalGridOld[grid.nDM][i-1][0][0])*2.0;
  }
  double dA1=dU_U0_Diff*dRSq_i_np1half*((1.0-parameters.dDonorFrac)*dA1CenGrad
    +parameters.dDonorFrac*dA1UpWindGrad);
  
  //calculate dS1
  double dUR2_im1halfjk_np1half=grid.dLocalGridNew[grid.nU][nIInt-1][j][k]*dRSq_im1half_np1half;
  double dUR2_ip1halfjk_np1half=grid.dLocalGridNew[grid.nU][nIInt][j][k]*dRSq_ip1half_np1half;
  double dS1=dP_ijk_np1half/grid.dLocalGridOld[grid.nD][i][j][k]
    *(dUR2_ip1halfjk_np1half-dUR2_im1halfjk_np1half)/grid.dLocalGridOld[grid.nDM][i][0][0];
  
  //Calcualte dA2
  double dA2CenGrad=(dE_ijp1halfk_np1half-dE_ijm1halfk_np1half)
    /grid.dLocalGridOld[grid.nDTheta][0][j][0];
  double dA2UpWindGrad=0.0;
  if(dV_ijk_np1half<0.0){//moving in the negative direction
    dA2UpWindGrad=(dE_ijp1k_np1half-dE_ijk_np1half)/(grid.dLocalGridOld[grid.nDTheta][0][j+1][0]
      +grid.dLocalGridOld[grid.nDTheta][0][j][0])*2.0;
  }
  else{//moving in the positive direction
    dA2UpWindGrad=(dE_ijk_np1half-dE_ijm1k_np1half)/(grid.dLocalGridOld[grid.nDTheta][0][j][0]
      +grid.dLocalGridOld[grid.nDTheta][0][j-1][0])*2.0;
  }
  double dA2=dV_ijk_np1half/dR_i_np1half*((1.0-parameters.dDonorFrac)*dA2CenGrad
    +parameters.dDonorFrac*dA2UpWindGrad);
    
  //Calcualte dS2
  double dS2=dP_ijk_np1half/(grid.dLocalGridOld[grid.nD][i][j][k]*dR_i_np1half
    *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]*grid.dLocalGridOld[grid.nDTheta][0][j][0])
    *(dVSinTheta_ijp1halfk_np1half-dVSinTheta_ijm1halfk_np1half);
  
  //Calculate dS4
  double dTGrad_ip1half_np1half=(dT4_ip1jk_np1half-dT4_ijk_np1half)
    /(grid.dLocalGridOld[grid.nDM][i+1][0][0]+grid.dLocalGridOld[grid.nDM][i][0][0])*2.0;
  double dTGrad_im1half_np1half=(dT4_ijk_np1half-dT4_im1jk_np1half)
    /(grid.dLocalGridOld[grid.nDM][i][0][0]+grid.dLocalGridOld[grid.nDM][i-1][0][0])*2.0;
  double dGrad_ip1half_np1half=dRhoAve_ip1half_n*dR4_ip1half_np1half/(dKappa_ip1halfjk_np1half
    *dRho_ip1halfjk_n)*dTGrad_ip1half_np1half;
  double dGrad_im1half_np1half=dRhoAve_im1half_n*dR4_im1half_np1half/(dKappa_im1halfjk_np1half
    *dRho_im1halfjk_n)*dTGrad_im1half_np1half;
  double dS4=16.0*parameters.dPi*parameters.dPi*grid.dLocalGridOld[grid.nDenAve][i][0][0]
    *(dGrad_ip1half_np1half-dGrad_im1half_np1half)/grid.dLocalGridOld[grid.nDM][i][0][0];
  
  //Calculate dS5
  double dTGrad_jp1half_np1half=(dT4_ijp1k_np1half-dT4_ijk_np1half)
    /(grid.dLocalGridOld[grid.nDTheta][0][j+1][0]+grid.dLocalGridOld[grid.nDTheta][0][j][0])*2.0;
  double dTGrad_jm1half_np1half=(dT4_ijk_np1half-dT4_ijm1k_np1half)
    /(grid.dLocalGridOld[grid.nDTheta][0][j][0]+grid.dLocalGridOld[grid.nDTheta][0][j-1][0])*2.0;
  double dGrad_jp1half_np1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt][0]
    /(dKappa_ijp1halfk_np1half*dRho_ijp1halfk_n)*dTGrad_jp1half_np1half;
  double dGrad_jm1half_np1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt-1][0]
    /(dKappa_ijm1halfk_np1half*dRho_ijm1halfk_n)*dTGrad_jm1half_np1half;
  double dS5=(dGrad_jp1half_np1half-dGrad_jm1half_np1half)
    /(grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]
    *dRSq_i_np1half*grid.dLocalGridOld[grid.nDTheta][0][j][0]);
  /*
  //calculate DUDM_ijk_np1half
  double dDUDM_ijk_np1half=4.0*parameters.dPi*grid.dLocalGridOld[grid.nDenAve][i][0][0]
    *dRSq_i_np1half*((grid.dLocalGridNew[grid.nU][nIInt][j][k]
    -grid.dLocalGridNew[grid.nU0][nIInt][0][0])-(grid.dLocalGridNew[grid.nU][nIInt-1][j][k]
    -grid.dLocalGridNew[grid.nU0][nIInt-1][0][0]))/grid.dLocalGridOld[grid.nDM][i][0][0];
  
  //calculate DVDM_ijk_np1half
  double dDVDM_ijk_np1half=4.0*parameters.dPi*grid.dLocalGridOld[grid.nDenAve][i][0][0]
    *dRSq_i_np1half*(dV_ip1halfjk_np1half-dV_im1halfjk_np1half)
    /grid.dLocalGridOld[grid.nDM][i][0][0];
  
  //calculate DUDTheta_ijk_np1half
  double dDUDTheta_ijk_np1half=((dU_ijp1halfk_np1half-dU0_i_np1half)-(dU_ijm1halfk_np1half
    -dU0_i_np1half))/(dR_i_np1half*grid.dLocalGridOld[grid.nDTheta][0][j][0]);
  
  //calculate DVDTheta_ijk_np1half
  double dDVDTheta_ijk_np1half=(grid.dLocalGridNew[grid.nV][i][nJInt][k]
    -grid.dLocalGridNew[grid.nV][i][nJInt-1][k])/(dR_i_np1half
    *grid.dLocalGridOld[grid.nDTheta][0][j][0]);
  
  //cal DivU_ijk_np1half
  double dDivU_ijk_np1half=dDUDM_ijk_np1half+dDVDTheta_ijk_np1half;
  
  //cal Tau_rr_ijk_np1half
  double dTau_rr_ijk_np1half=2.0*grid.dLocalGridNew[grid.nEddyVisc][i][j][k]*(dDUDM_ijk_np1half
    -0.3333333333333333*dDivU_ijk_np1half);
  
  //calculate Tau_tt_ijk_np1half
  double dTau_tt_ijk_np1half=2.0*grid.dLocalGridNew[grid.nEddyVisc][i][j][k]*(dDVDTheta_ijk_np1half
    -0.333333333333333*dDivU_ijk_np1half);
  
  //calculate Tau_rt_ijk_np1half
  double dTau_rt_ijk_np1half=grid.dLocalGridNew[grid.nEddyVisc][i][j][k]*(dDVDM_ijk_np1half
    +dDUDTheta_ijk_np1half);
  
  //eddy viscosity terms
  double dEddyViscosityTerms=(dTau_rr_ijk_np1half*dDUDM_ijk_np1half+dTau_tt_ijk_np1half
    *dDVDTheta_ijk_np1half+dTau_rt_ijk_np1half*(dDUDTheta_ijk_np1half+dDVDM_ijk_np1half))
    /grid.dLocalGridOld[grid.nD][i][j][k];*/
  
  //calculate dT1
  double dEGrad_ip1halfjk_np1half=dR4_ip1half_np1half*dEddyVisc_ip1halfjk_n*dRhoAve_ip1half_n
    *(dE_ip1jk_np1half-dE_ijk_np1half)/(dRho_ip1halfjk_n*dDM_ip1half);
  double dEGrad_im1halfjk_np1half=dR4_im1half_np1half*dEddyVisc_im1halfjk_n*dRhoAve_im1half_n
    *(dE_ijk_np1half-dE_im1jk_np1half)/(dRho_im1halfjk_n*dDM_im1half);
  double dT1=16.0*dPiSq*grid.dLocalGridOld[grid.nDenAve][i][0][0]*(dEGrad_ip1halfjk_np1half
    -dEGrad_im1halfjk_np1half)/grid.dLocalGridOld[grid.nDM][i][0][0];
  
  //calculate dT2
  double dEGrad_ijp1halfk_np1half=dEddyVisc_ijp1halfk_n
    *grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt][0]
    *(dE_ijp1k_np1half-dE_ijk_np1half)/(dRho_ijp1halfk_n*dR_i_np1half*dDelTheta_jp1half);
  double dEGrad_ijm1halfk_np1half=dEddyVisc_ijm1halfk_n
    *grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt-1][0]
    *(dE_ijk_np1half-dE_ijm1k_np1half)/(dRho_ijm1halfk_n*dR_i_np1half*dDelTheta_jm1half);
  double dT2=(dEGrad_ijp1halfk_np1half-dEGrad_ijm1halfk_np1half)/(dR_i_np1half
    *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]
    *grid.dLocalGridOld[grid.nDTheta][0][j][0]);
  
  //eddy viscosity terms
  double dEddyViscosityTerms=(dT1+dT2)/parameters.dPrt;
  
  //calculate energy equation discrepancy
  return (dE_ijk_np1-grid.dLocalGridOld[grid.nE][i][j][k])/time.dDeltat_n
    +4.0*parameters.dPi*grid.dLocalGridOld[grid.nDenAve][i][0][0]*(dA1+dS1)+dA2+dS2
    -4.0*parameters.dSigma/(3.0*grid.dLocalGridOld[grid.nD][i][j][k])*(dS4+dS5)-dEddyViscosityTerms;
}
double dImplicitEnergyFunction_RT_LES_SB(Grid &grid,Parameters &parameters,Time &time
  ,double dTemps[],int i,int j,int k){
  
  double dT_ijk_np1=dTemps[0];
  double dT_im1jk_np1=dTemps[1];
  double dT_ijp1k_np1=dTemps[2];
  double dT_ijm1k_np1=dTemps[3];
  
  double dPiSq=parameters.dPi*parameters.dPi;
  
  //calculate i,j,k for interface centered quantities
  int nIInt=i+grid.nCenIntOffset[0];
  int nJInt=j+grid.nCenIntOffset[1];
  
  //Calculate interpolated quantities
  double dDM_ip1half=(grid.dLocalGridOld[grid.nDM][i][0][0])*(0.5+parameters.dAlpha
    +parameters.dAlphaExtra);/**\BC Missing \f$\Delta M_r\f$ outside model using 
    \ref Parameters.dAlpha times \f$\Delta M_r\f$ in the last zone instead.*/
  double dDM_im1half=(grid.dLocalGridOld[grid.nDM][i][0][0]+grid.dLocalGridOld[grid.nDM][i-1][0][0])
    *0.5;
  double dDelTheta_jp1half=(grid.dLocalGridOld[grid.nDTheta][0][j][0]
    +grid.dLocalGridOld[grid.nDTheta][0][j+1][0])*0.5;
  double dDelTheta_jm1half=(grid.dLocalGridOld[grid.nDTheta][0][j][0]
    +grid.dLocalGridOld[grid.nDTheta][0][j-1][0])*0.5;
  double dR_ip1half_np1half=(grid.dLocalGridOld[grid.nR][nIInt][0][0]
    +grid.dLocalGridNew[grid.nR][nIInt][0][0])*0.5;
  double dR_im1half_np1half=(grid.dLocalGridOld[grid.nR][nIInt-1][0][0]
    +grid.dLocalGridNew[grid.nR][nIInt-1][0][0])*0.5;
  double dR_i_np1half=(dR_ip1half_np1half+dR_im1half_np1half)*0.5;
  double dRSq_i_np1half=dR_i_np1half*dR_i_np1half;
  double dRSq_ip1half_np1half=dR_ip1half_np1half*dR_ip1half_np1half;
  double dRSq_im1half_np1half=dR_im1half_np1half*dR_im1half_np1half;
  double dR4_ip1half_np1half=dRSq_ip1half_np1half*dRSq_ip1half_np1half;
  double dR4_im1half_np1half=dRSq_im1half_np1half*dRSq_im1half_np1half;
  double dRhoAve_ip1half_n=(grid.dLocalGridOld[grid.nDenAve][i][0][0])*0.5;/**\BC missing density
    outside model assuming it is zero*/
  double dRhoAve_im1half_n=(grid.dLocalGridOld[grid.nDenAve][i][0][0]
    +grid.dLocalGridOld[grid.nDenAve][i-1][0][0])*0.5;
  double dRho_ip1halfjk_n=(grid.dLocalGridOld[grid.nD][i][j][k])*0.5;/**\BC missing desnity outside
    model assuming it is zero*/
  double dRho_im1halfjk_n=(grid.dLocalGridOld[grid.nD][i][j][k]
    +grid.dLocalGridOld[grid.nD][i-1][j][k])*0.5;
  double dRho_ijp1halfk_n=(grid.dLocalGridOld[grid.nD][i][j+1][k]
    +grid.dLocalGridOld[grid.nD][i][j][k])*0.5;
  double dRho_ijm1halfk_n=(grid.dLocalGridOld[grid.nD][i][j][k]
    +grid.dLocalGridOld[grid.nD][i][j-1][k])*0.5;
  double dU0_i_np1half=(grid.dLocalGridNew[grid.nU0][nIInt][0][0]
    +grid.dLocalGridNew[grid.nU0][nIInt-1][0][0])*0.5;
  double dU_ijk_np1half=(grid.dLocalGridNew[grid.nU][nIInt][j][k]
    +grid.dLocalGridNew[grid.nU][nIInt-1][j][k])*0.5;
  double dU_ijp1halfk_np1half=(grid.dLocalGridNew[grid.nU][nIInt][j+1][k]
    +grid.dLocalGridNew[grid.nU][nIInt-1][j+1][k]+grid.dLocalGridNew[grid.nU][nIInt][j][k]
    +grid.dLocalGridNew[grid.nU][nIInt-1][j][k])*0.25;
  double dU_ijm1halfk_np1half=(grid.dLocalGridNew[grid.nU][nIInt][j-1][k]
    +grid.dLocalGridNew[grid.nU][nIInt-1][j-1][k]+grid.dLocalGridNew[grid.nU][nIInt][j][k]
    +grid.dLocalGridNew[grid.nU][nIInt-1][j][k])*0.25;
  double dV_ijk_np1half=(grid.dLocalGridNew[grid.nV][i][nJInt][k]
    +grid.dLocalGridNew[grid.nV][i][nJInt-1][k])*0.5;
  double dV_ip1halfjk_np1half=(grid.dLocalGridNew[grid.nV][i][nJInt][k]
    +grid.dLocalGridNew[grid.nV][i][nJInt-1][k])*0.5;/**\BC assuming V at ip1half is the same as V 
    at i*/
  double dV_im1halfjk_np1half=(grid.dLocalGridNew[grid.nV][i][nJInt][k]
    +grid.dLocalGridNew[grid.nV][i][nJInt-1][k]+grid.dLocalGridNew[grid.nV][i-1][nJInt][k]
    +grid.dLocalGridNew[grid.nV][i-1][nJInt-1][k])*0.25;
  double dEddyVisc_ip1halfjk_n=(grid.dLocalGridNew[grid.nEddyVisc][i][j][k])*0.5;
  double dEddyVisc_im1halfjk_n=(grid.dLocalGridNew[grid.nEddyVisc][i-1][j][k]
    +grid.dLocalGridNew[grid.nEddyVisc][i][j][k])*0.5;
  double dEddyVisc_ijp1halfk_n=(grid.dLocalGridNew[grid.nEddyVisc][i][j+1][k]
  +grid.dLocalGridNew[grid.nEddyVisc][i][j][k])*0.5;
  double dEddyVisc_ijm1halfk_n=(grid.dLocalGridNew[grid.nEddyVisc][i][j-1][k]
    +grid.dLocalGridNew[grid.nEddyVisc][i][j][k])*0.5;
  
  double dT_ijk_np1half=(dT_ijk_np1+grid.dLocalGridOld[grid.nT][i][j][k])*0.5;
  double dTSq_ijk_np1half=dT_ijk_np1half*dT_ijk_np1half;
  double dT4_ijk_np1half=dTSq_ijk_np1half*dTSq_ijk_np1half;
  
  double dT_im1jk_np1half=(dT_im1jk_np1+grid.dLocalGridOld[grid.nT][i-1][j][k])*0.5;
  double dTSq_im1jk_np1half=dT_im1jk_np1half*dT_im1jk_np1half;
  double dT4_im1jk_np1half=dTSq_im1jk_np1half*dTSq_im1jk_np1half;
  
  double dT_ijp1k_np1half=(dT_ijp1k_np1+grid.dLocalGridOld[grid.nT][i][j+1][k])*0.5;
  double dTSq_ijp1k_np1half=dT_ijp1k_np1half*dT_ijp1k_np1half;
  double dT4_ijp1k_np1half=dTSq_ijp1k_np1half*dTSq_ijp1k_np1half;
  
  double dT_ijm1k_np1half=(dT_ijm1k_np1+grid.dLocalGridOld[grid.nT][i][j-1][k])*0.5;
  double dTSq_ijm1k_np1half=dT_ijm1k_np1half*dT_ijm1k_np1half;
  double dT4_ijm1k_np1half=dTSq_ijm1k_np1half*dTSq_ijm1k_np1half;
  
  double dE_ijk_np1=parameters.eosTable.dGetEnergy(dT_ijk_np1
    ,grid.dLocalGridNew[grid.nD][i][j][k]);
  double dE_ijk_np1half=parameters.eosTable.dGetEnergy(dT_ijk_np1half
    ,grid.dLocalGridOld[grid.nD][i][j][k]);
  double dE_im1jk_np1half=parameters.eosTable.dGetEnergy(dT_im1jk_np1half
    ,grid.dLocalGridOld[grid.nD][i-1][j][k]);
  double dE_ijp1k_np1half=parameters.eosTable.dGetEnergy(dT_ijp1k_np1half
    ,grid.dLocalGridOld[grid.nD][i][j+1][k]);
  double dE_ijm1k_np1half=parameters.eosTable.dGetEnergy(dT_ijm1k_np1half
    ,grid.dLocalGridOld[grid.nD][i][j-1][k]);
  double dE_ip1jk_np1half=dE_ijk_np1half;/**\BC Assuming energy outside model is the same as
    the energy in the last zone inside the model.*/
  double dE_ip1halfjk_np1half=dE_ijk_np1half;/**\BC Assuming energy outside model is the same as
    the energy in the last zone inside the model.*/
  double dE_im1halfjk_np1half=(dE_im1jk_np1half+dE_ijk_np1half)*0.5;
  double dE_ijp1halfk_np1half=(dE_ijp1k_np1half+dE_ijk_np1half)*0.5;
  double dE_ijm1halfk_np1half=(dE_ijm1k_np1half+dE_ijk_np1half)*0.5;
  
  double dP_ijk_np1half=parameters.eosTable.dGetPressure(dT_ijk_np1half
    ,grid.dLocalGridOld[grid.nD][i][j][k]);
  #if VISCOUS_ENERGY_EQ==1
    dP_ijk_np1half=dP_ijk_np1half+grid.dLocalGridOld[grid.nQ0][i][j][k]
      +grid.dLocalGridOld[grid.nQ1][i][j][k];
  #endif
  
  double dKappa_ijk_np1half=parameters.eosTable.dGetOpacity(dT_ijk_np1half
    ,grid.dLocalGridOld[grid.nD][i][j][k]);
  double dKappa_im1jk_np1half=parameters.eosTable.dGetOpacity(dT_im1jk_np1half
    ,grid.dLocalGridOld[grid.nD][i-1][j][k]);
  double dKappa_ijp1k_np1half=parameters.eosTable.dGetOpacity(dT_ijp1k_np1half
    ,grid.dLocalGridOld[grid.nD][i][j+1][k]);
  double dKappa_ijm1k_np1half=parameters.eosTable.dGetOpacity(dT_ijm1k_np1half
    ,grid.dLocalGridOld[grid.nD][i][j-1][k]);
  double dKappa_im1halfjk_np1half=(dT4_im1jk_np1half+dT4_ijk_np1half)/(dT4_ijk_np1half
    /dKappa_ijk_np1half+dT4_im1jk_np1half/dKappa_im1jk_np1half);
  double dKappa_ijp1halfk_np1half=(dT4_ijp1k_np1half+dT4_ijk_np1half)/(dT4_ijk_np1half
    /dKappa_ijk_np1half+dT4_ijp1k_np1half/dKappa_ijp1k_np1half);
  double dKappa_ijm1halfk_np1half=(dT4_ijm1k_np1half+dT4_ijk_np1half)/(dT4_ijk_np1half
    /dKappa_ijk_np1half+dT4_ijm1k_np1half/dKappa_ijm1k_np1half);
  
  //Calcuate dA1
  double dA1CenGrad=(dE_ip1halfjk_np1half-dE_im1halfjk_np1half)
    /grid.dLocalGridOld[grid.nDM][i][0][0];
  double dA1UpWindGrad=0.0;
  double dU_U0_Diff=(dU_ijk_np1half-dU0_i_np1half);
  if(dU_U0_Diff<0.0){//moving in the negative radial direction
    dA1UpWindGrad=dA1UpWindGrad;/**\BC Using centered gradient for upwind gradient when motion is 
      into the star at the surface*/
  }
  else{//moving in the postive radial direction
    dA1UpWindGrad=(dE_ijk_np1half-dE_im1jk_np1half)/(grid.dLocalGridOld[grid.nDM][i][0][0]
      +grid.dLocalGridOld[grid.nDM][i-1][0][0])*2.0;
  }
  double dA1=dU_U0_Diff*dRSq_i_np1half*((1.0-parameters.dDonorFrac)*dA1CenGrad
    +parameters.dDonorFrac*dA1UpWindGrad);
  
  //calculate dS1
  double dUR2_im1halfjk_np1half=grid.dLocalGridNew[grid.nU][nIInt-1][j][k]*dRSq_im1half_np1half;
  double dUR2_ip1halfjk_np1half=grid.dLocalGridNew[grid.nU][nIInt][j][k]*dRSq_ip1half_np1half;
  double dS1=dP_ijk_np1half/grid.dLocalGridOld[grid.nD][i][j][k]
    *(dUR2_ip1halfjk_np1half-dUR2_im1halfjk_np1half)/grid.dLocalGridOld[grid.nDM][i][0][0];
  
  //Calcualte dA2
  double dA2CenGrad=(dE_ijp1halfk_np1half-dE_ijm1halfk_np1half)
    /grid.dLocalGridOld[grid.nDTheta][0][j][0];
  double dA2UpWindGrad=0.0;
  if(dV_ijk_np1half<0.0){//moving in the negative theta direction
    dA2UpWindGrad=(dE_ijp1k_np1half-dE_ijk_np1half)/(grid.dLocalGridOld[grid.nDTheta][0][j+1][0]
      +grid.dLocalGridOld[grid.nDTheta][0][j][0])*2.0;
  }
  else{//moving in the positive theta direction
    dA2UpWindGrad=(dE_ijk_np1half-dE_ijm1k_np1half)/(grid.dLocalGridOld[grid.nDTheta][0][j][0]
      +grid.dLocalGridOld[grid.nDTheta][0][j-1][0])*2.0;
  }
  double dA2=dV_ijk_np1half/dR_i_np1half*((1.0-parameters.dDonorFrac)*dA2CenGrad
    +parameters.dDonorFrac*dA2UpWindGrad);
    
  //Calcualte dS2
  double dVSinTheta_ijp1halfk_np1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt][0]
    *grid.dLocalGridNew[grid.nV][i][nJInt][k];
  double dVSinTheta_ijm1halfk_np1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt-1][0]
    *grid.dLocalGridNew[grid.nV][i][nJInt-1][k];
  double dS2=dP_ijk_np1half/(grid.dLocalGridOld[grid.nD][i][j][k]*dR_i_np1half
    *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]*grid.dLocalGridOld[grid.nDTheta][0][j][0])
    *(dVSinTheta_ijp1halfk_np1half-dVSinTheta_ijm1halfk_np1half);
  
  //Calculate dS4
  double dTGrad_im1half_np1half=(dT4_ijk_np1half-dT4_im1jk_np1half)
    /(grid.dLocalGridOld[grid.nDM][i][0][0]+grid.dLocalGridOld[grid.nDM][i-1][0][0])*2.0;
  double dGrad_ip1half_np1half=-3.0*dRSq_ip1half_np1half*dT4_ijk_np1half/(8.0*parameters.dPi);/**\BC 
    Missing grid.dLocalGridOld[grid.nT][i+1][0][0] using flux equals \f$2\sigma T^4\f$ at surface.*/
  double dGrad_im1half_np1half=dRhoAve_im1half_n*dR4_im1half_np1half/(dKappa_im1halfjk_np1half
    *dRho_im1halfjk_n)*dTGrad_im1half_np1half;
  double dS4=16.0*parameters.dPi*parameters.dPi*grid.dLocalGridOld[grid.nDenAve][i][0][0]
    *(dGrad_ip1half_np1half-dGrad_im1half_np1half)/grid.dLocalGridOld[grid.nDM][i][0][0];
  
  //Calculate dS5
  double dTGrad_jp1half_np1half=(dT4_ijp1k_np1half-dT4_ijk_np1half)
    /(grid.dLocalGridOld[grid.nDTheta][0][j+1][0]+grid.dLocalGridOld[grid.nDTheta][0][j][0])*2.0;
  double dTGrad_jm1half_np1half=(dT4_ijk_np1half-dT4_ijm1k_np1half)
    /(grid.dLocalGridOld[grid.nDTheta][0][j][0]+grid.dLocalGridOld[grid.nDTheta][0][j-1][0])*2.0;
  double dGrad_jp1half_np1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt][0]
    /(dKappa_ijp1halfk_np1half*dRho_ijp1halfk_n)*dTGrad_jp1half_np1half;
  double dGrad_jm1half_np1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt-1][0]
    /(dKappa_ijm1halfk_np1half*dRho_ijm1halfk_n)*dTGrad_jm1half_np1half;
  double dS5=(dGrad_jp1half_np1half-dGrad_jm1half_np1half)
    /(grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]
    *dRSq_i_np1half*grid.dLocalGridOld[grid.nDTheta][0][j][0]);
  /*
  //calculate DUDM_ijk_np1half
  double dDUDM_ijk_np1half=4.0*parameters.dPi*grid.dLocalGridOld[grid.nDenAve][i][0][0]
    *dRSq_i_np1half*((grid.dLocalGridNew[grid.nU][nIInt][j][k]
    -grid.dLocalGridNew[grid.nU0][nIInt][0][0])-(grid.dLocalGridNew[grid.nU][nIInt-1][j][k]
    -grid.dLocalGridNew[grid.nU0][nIInt-1][0][0]))/grid.dLocalGridOld[grid.nDM][i][0][0];
  
  //calculate DVDM_ijk_np1half
  double dDVDM_ijk_np1half=4.0*parameters.dPi*grid.dLocalGridOld[grid.nDenAve][i][0][0]
    *dRSq_i_np1half*(dV_ip1halfjk_np1half-dV_im1halfjk_np1half)
    /grid.dLocalGridOld[grid.nDM][i][0][0];
  
  //calculate DUDTheta_ijk_np1half
  double dDUDTheta_ijk_np1half=((dU_ijp1halfk_np1half-dU0_i_np1half)-(dU_ijm1halfk_np1half
    -dU0_i_np1half))/(dR_i_np1half*grid.dLocalGridOld[grid.nDTheta][0][j][0]);
  
  //calculate DVDTheta_ijk_np1half
  double dDVDTheta_ijk_np1half=(grid.dLocalGridNew[grid.nV][i][nJInt][k]
    -grid.dLocalGridNew[grid.nV][i][nJInt-1][k])/(dR_i_np1half
    *grid.dLocalGridOld[grid.nDTheta][0][j][0]);
  
  //cal DivU_ijk_np1half
  double dDivU_ijk_np1half=dDUDM_ijk_np1half+dDVDTheta_ijk_np1half;
  
  //cal Tau_rr_ijk_np1half
  double dTau_rr_ijk_np1half=2.0*grid.dLocalGridNew[grid.nEddyVisc][i][j][k]*(dDUDM_ijk_np1half
    -0.3333333333333333*dDivU_ijk_np1half);
  
  //calculate Tau_tt_ijk_np1half
  double dTau_tt_ijk_np1half=2.0*grid.dLocalGridNew[grid.nEddyVisc][i][j][k]*(dDVDTheta_ijk_np1half
    -0.333333333333333*dDivU_ijk_np1half);
  
  //calculate Tau_rt_ijk_np1half
  double dTau_rt_ijk_np1half=grid.dLocalGridNew[grid.nEddyVisc][i][j][k]*(dDVDM_ijk_np1half
    +dDUDTheta_ijk_np1half);
  
  //eddy viscosity terms
  double dEddyViscosityTerms=(dTau_rr_ijk_np1half*dDUDM_ijk_np1half+dTau_tt_ijk_np1half
    *dDVDTheta_ijk_np1half+dTau_rt_ijk_np1half*(dDUDTheta_ijk_np1half+dDVDM_ijk_np1half))
    /grid.dLocalGridOld[grid.nD][i][j][k];*/
  
  //calculate dT1
  double dEGrad_ip1halfjk_np1half=dR4_ip1half_np1half*dEddyVisc_ip1halfjk_n*dRhoAve_ip1half_n
    *(dE_ip1jk_np1half-dE_ijk_np1half)/(dRho_ip1halfjk_n*dDM_ip1half);
  double dEGrad_im1halfjk_np1half=dR4_im1half_np1half*dEddyVisc_im1halfjk_n*dRhoAve_im1half_n
    *(dE_ijk_np1half-dE_im1jk_np1half)/(dRho_im1halfjk_n*dDM_im1half);
  double dT1=16.0*dPiSq*grid.dLocalGridOld[grid.nDenAve][i][0][0]*(dEGrad_ip1halfjk_np1half
    -dEGrad_im1halfjk_np1half)/grid.dLocalGridOld[grid.nDM][i][0][0];
  
  //calculate dT2
  double dEGrad_ijp1halfk_np1half=dEddyVisc_ijp1halfk_n
    *grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt][0]
    *(dE_ijp1k_np1half-dE_ijk_np1half)/(dRho_ijp1halfk_n*dR_i_np1half*dDelTheta_jp1half);
  double dEGrad_ijm1halfk_np1half=dEddyVisc_ijm1halfk_n
    *grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt-1][0]
    *(dE_ijk_np1half-dE_ijm1k_np1half)/(dRho_ijm1halfk_n*dR_i_np1half*dDelTheta_jm1half);
  double dT2=(dEGrad_ijp1halfk_np1half-dEGrad_ijm1halfk_np1half)/(dR_i_np1half
    *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]
    *grid.dLocalGridOld[grid.nDTheta][0][j][0]);
  
  //eddy viscosity terms
  double dEddyViscosityTerms=(dT1+dT2)/parameters.dPrt;
  
  //calculate energy equation discrepancy
  return (dE_ijk_np1-grid.dLocalGridOld[grid.nE][i][j][k])/time.dDeltat_n
    +4.0*parameters.dPi*grid.dLocalGridOld[grid.nDenAve][i][0][0]*(dA1+dS1)+dA2+dS2
    -4.0*parameters.dSigma/(3.0*grid.dLocalGridOld[grid.nD][i][j][k])*(dS4+dS5)-dEddyViscosityTerms;
}
double dImplicitEnergyFunction_RTP_LES(Grid &grid,Parameters &parameters,Time &time,double dTemps[]
  ,int i,int j,int k){
  
  double dT_ijk_np1=dTemps[0];
  double dT_ip1jk_np1=dTemps[1];
  double dT_im1jk_np1=dTemps[2];
  double dT_ijp1k_np1=dTemps[3];
  double dT_ijm1k_np1=dTemps[4];
  double dT_ijkp1_np1=dTemps[5];
  double dT_ijkm1_np1=dTemps[6];
  
  double dPiSq=parameters.dPi*parameters.dPi;
  
  //calculate i,j,k for interface centered quantities
  int nIInt=i+grid.nCenIntOffset[0];
  int nJInt=j+grid.nCenIntOffset[1];
  int nKInt=k+grid.nCenIntOffset[2];
    
  //Calculate interpolated quantities
  double dDM_ip1half=(grid.dLocalGridOld[grid.nDM][i][0][0]+grid.dLocalGridOld[grid.nDM][i+1][0][0])
    *0.5;
  double dDM_im1half=(grid.dLocalGridOld[grid.nDM][i][0][0]+grid.dLocalGridOld[grid.nDM][i-1][0][0])
    *0.5;
  double dDelTheta_jp1half=(grid.dLocalGridOld[grid.nDTheta][0][j][0]
    +grid.dLocalGridOld[grid.nDTheta][0][j+1][0])*0.5;
  double dDelTheta_jm1half=(grid.dLocalGridOld[grid.nDTheta][0][j][0]
    +grid.dLocalGridOld[grid.nDTheta][0][j-1][0])*0.5;
  double dDelPhi_kp1half=(grid.dLocalGridOld[grid.nDPhi][0][0][k]
    +grid.dLocalGridOld[grid.nDPhi][0][0][k+1])*0.5;
  double dDelPhi_km1half=(grid.dLocalGridOld[grid.nDPhi][0][0][k]
    +grid.dLocalGridOld[grid.nDPhi][0][0][k-1])*0.5;
  double dU0_i_np1half=(grid.dLocalGridNew[grid.nU0][nIInt][0][0]
    +grid.dLocalGridNew[grid.nU0][nIInt-1][0][0])*0.5;
  double dR_ip1half_np1half=(grid.dLocalGridOld[grid.nR][nIInt][0][0]
    +grid.dLocalGridNew[grid.nR][nIInt][0][0])*0.5;
  double dR_im1half_np1half=(grid.dLocalGridOld[grid.nR][nIInt-1][0][0]
    +grid.dLocalGridNew[grid.nR][nIInt-1][0][0])*0.5;
  double dR_i_np1half=(dR_ip1half_np1half+dR_im1half_np1half)*0.5;
  double dRSq_i_np1half=dR_i_np1half*dR_i_np1half;
  double dRSq_ip1half_np1half=dR_ip1half_np1half*dR_ip1half_np1half;
  double dR4_ip1half_np1half=dRSq_ip1half_np1half*dRSq_ip1half_np1half;
  double dRSq_im1half_np1half=dR_im1half_np1half*dR_im1half_np1half;
  double dR4_im1half_np1half=dRSq_im1half_np1half*dRSq_im1half_np1half;
  double dRhoAve_ip1half_n=(grid.dLocalGridOld[grid.nDenAve][i+1][0][0]
    +grid.dLocalGridOld[grid.nDenAve][i][0][0])*0.5;
  double dRhoAve_im1half_n=(grid.dLocalGridOld[grid.nDenAve][i][0][0]
    +grid.dLocalGridOld[grid.nDenAve][i-1][0][0])*0.5;
  double dRho_ip1halfjk_n=(grid.dLocalGridOld[grid.nD][i+1][j][k]
    +grid.dLocalGridOld[grid.nD][i][j][k])*0.5;
  double dRho_im1halfjk_n=(grid.dLocalGridOld[grid.nD][i][j][k]
    +grid.dLocalGridOld[grid.nD][i-1][j][k])*0.5;
  double dRho_ijp1halfk_n=(grid.dLocalGridOld[grid.nD][i][j+1][k]
    +grid.dLocalGridOld[grid.nD][i][j][k])*0.5;
  double dRho_ijm1halfk_n=(grid.dLocalGridOld[grid.nD][i][j][k]
    +grid.dLocalGridOld[grid.nD][i][j-1][k])*0.5;
  double dRho_ijkp1half_n=(grid.dLocalGridOld[grid.nD][i][j][k+1]
    +grid.dLocalGridOld[grid.nD][i][j][k])*0.5;
  double dRho_ijkm1half_n=(grid.dLocalGridOld[grid.nD][i][j][k]
    +grid.dLocalGridOld[grid.nD][i][j][k-1])*0.5;
  double dU_ijk_np1half=(grid.dLocalGridNew[grid.nU][nIInt][j][k]
    +grid.dLocalGridNew[grid.nU][nIInt-1][j][k])*0.5;
  double dU_ijp1halfk_np1half=(grid.dLocalGridNew[grid.nU][nIInt][j+1][k]
    +grid.dLocalGridNew[grid.nU][nIInt-1][j+1][k]+grid.dLocalGridNew[grid.nU][nIInt][j][k]
    +grid.dLocalGridNew[grid.nU][nIInt-1][j][k])*0.25;
  double dU_ijm1halfk_np1half=(grid.dLocalGridNew[grid.nU][nIInt][j-1][k]
    +grid.dLocalGridNew[grid.nU][nIInt-1][j-1][k]+grid.dLocalGridNew[grid.nU][nIInt][j][k]
    +grid.dLocalGridNew[grid.nU][nIInt-1][j][k])*0.25;
  double dU_ijkp1half_np1half=(grid.dLocalGridNew[grid.nU][nIInt][j][k]
    +grid.dLocalGridNew[grid.nU][nIInt][j][k+1]+grid.dLocalGridNew[grid.nU][nIInt-1][j][k]
    +grid.dLocalGridNew[grid.nU][nIInt-1][j][k+1])*0.25;
  double dU_ijkm1half_np1half=(grid.dLocalGridNew[grid.nU][nIInt][j][k]
    +grid.dLocalGridNew[grid.nU][nIInt][j][k-1]+grid.dLocalGridNew[grid.nU][nIInt-1][j][k]
    +grid.dLocalGridNew[grid.nU][nIInt-1][j][k-1])*0.25;
  double dV_ijk_np1half=(grid.dLocalGridNew[grid.nV][i][nJInt][k]
    +grid.dLocalGridNew[grid.nV][i][nJInt-1][k])*0.5;
  double dV_ip1halfjk_np1half=(grid.dLocalGridNew[grid.nV][i+1][nJInt][k]
    +grid.dLocalGridNew[grid.nV][i+1][nJInt-1][k]+grid.dLocalGridNew[grid.nV][i][nJInt][k]
    +grid.dLocalGridNew[grid.nV][i][nJInt-1][k])*0.25;
  double dV_im1halfjk_np1half=(grid.dLocalGridNew[grid.nV][i][nJInt][k]
    +grid.dLocalGridNew[grid.nV][i][nJInt-1][k]+grid.dLocalGridNew[grid.nV][i-1][nJInt][k]
    +grid.dLocalGridNew[grid.nV][i-1][nJInt-1][k])*0.25;
  double dV_ijkp1half_np1half=(grid.dLocalGridNew[grid.nV][i][nJInt][k+1]
    +grid.dLocalGridNew[grid.nV][i][nJInt-1][k+1]+grid.dLocalGridNew[grid.nV][i][nJInt][k]
    +grid.dLocalGridNew[grid.nV][i][nJInt-1][k])*0.25;
  double dV_ijkm1half_np1half=(grid.dLocalGridNew[grid.nV][i][nJInt][k]
    +grid.dLocalGridNew[grid.nV][i][nJInt-1][k]+grid.dLocalGridNew[grid.nV][i][nJInt][k-1]
    +grid.dLocalGridNew[grid.nV][i][nJInt-1][k-1])*0.25;
  double dW_ijk_np1half=(grid.dLocalGridNew[grid.nW][i][j][nKInt]
    +grid.dLocalGridNew[grid.nW][i][j][nKInt-1])*0.5;
  double dW_ijkp1half_np1half=(grid.dLocalGridNew[grid.nW][i][j][nKInt]);
  double dW_ijkm1half_np1half=(grid.dLocalGridNew[grid.nW][i][j][nKInt-1]);
  double dW_ip1halfjk_np1half=(grid.dLocalGridNew[grid.nW][i+1][j][nKInt]
    +grid.dLocalGridNew[grid.nW][i+1][j][nKInt-1]+grid.dLocalGridNew[grid.nW][i][j][nKInt]
    +grid.dLocalGridNew[grid.nW][i][j][nKInt-1])*0.25;
  double dW_im1halfjk_np1half=(grid.dLocalGridNew[grid.nW][i][j][nKInt]
    +grid.dLocalGridNew[grid.nW][i][j][nKInt-1]+grid.dLocalGridNew[grid.nW][i-1][j][nKInt]
    +grid.dLocalGridNew[grid.nW][i-1][j][nKInt-1])*0.25;
  double dW_ijp1halfk_np1half=(grid.dLocalGridNew[grid.nW][i][j+1][nKInt]
    +grid.dLocalGridNew[grid.nW][i][j+1][nKInt-1]+grid.dLocalGridNew[grid.nW][i][j][nKInt]
    +grid.dLocalGridNew[grid.nW][i][j][nKInt-1])*0.25;
  double dW_ijm1halfk_np1half=(grid.dLocalGridNew[grid.nW][i][j][nKInt]
    +grid.dLocalGridNew[grid.nW][i][j][nKInt-1]+grid.dLocalGridNew[grid.nW][i][j-1][nKInt]
    +grid.dLocalGridNew[grid.nW][i][j-1][nKInt-1])*0.25;
  double dEddyVisc_ip1halfjk_n=(grid.dLocalGridOld[grid.nEddyVisc][i+1][j][k]
    +grid.dLocalGridOld[grid.nEddyVisc][i][j][k])*0.5;
  double dEddyVisc_im1halfjk_n=(grid.dLocalGridOld[grid.nEddyVisc][i-1][j][k]
    +grid.dLocalGridOld[grid.nEddyVisc][i][j][k])*0.5;
  double dEddyVisc_ijp1halfk_n=(grid.dLocalGridOld[grid.nEddyVisc][i][j+1][k]
  +grid.dLocalGridOld[grid.nEddyVisc][i][j][k])*0.5;
  double dEddyVisc_ijm1halfk_n=(grid.dLocalGridOld[grid.nEddyVisc][i][j-1][k]
    +grid.dLocalGridOld[grid.nEddyVisc][i][j][k])*0.5;
  double dEddyVisc_ijkp1half_n=(grid.dLocalGridOld[grid.nEddyVisc][i][j][k+1]
    +grid.dLocalGridOld[grid.nEddyVisc][i][j][k])*0.5;
  double dEddyVisc_ijkm1half_n=(grid.dLocalGridOld[grid.nEddyVisc][i][j][k-1]
    +grid.dLocalGridOld[grid.nEddyVisc][i][j][k])*0.5;
  
  double dVSinTheta_ijp1halfk_np1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt][0]
    *grid.dLocalGridNew[grid.nV][i][nJInt][k];
  double dVSinTheta_ijm1halfk_np1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt-1][0]
    *grid.dLocalGridNew[grid.nV][i][nJInt-1][k];
  
  double dT_ip1jk_np1half=(dT_ip1jk_np1+grid.dLocalGridOld[grid.nT][i+1][j][k])*0.5;
  double dTSq_ip1jk_np1half=dT_ip1jk_np1half*dT_ip1jk_np1half;
  double dT4_ip1jk_np1half=dTSq_ip1jk_np1half*dTSq_ip1jk_np1half;
  
  double dT_ijk_np1half=(dT_ijk_np1+grid.dLocalGridOld[grid.nT][i][j][k])*0.5;
  double dTSq_ijk_np1half=dT_ijk_np1half*dT_ijk_np1half;
  double dT4_ijk_np1half=dTSq_ijk_np1half*dTSq_ijk_np1half;
  
  double dT_im1jk_np1half=(dT_im1jk_np1+grid.dLocalGridOld[grid.nT][i-1][j][k])*0.5;
  double dTSq_im1jk_np1half=dT_im1jk_np1half*dT_im1jk_np1half;
  double dT4_im1jk_np1half = dTSq_im1jk_np1half*dTSq_im1jk_np1half;
  
  double dT_ijp1k_np1half=(dT_ijp1k_np1+grid.dLocalGridOld[grid.nT][i][j+1][k])*0.5;
  double dTSq_ijp1k_np1half=dT_ijp1k_np1half*dT_ijp1k_np1half;
  double dT4_ijp1k_np1half=dTSq_ijp1k_np1half*dTSq_ijp1k_np1half;
  
  double dT_ijm1k_np1half=(dT_ijm1k_np1+grid.dLocalGridOld[grid.nT][i][j-1][k])*0.5;
  double dTSq_ijm1k_np1half=dT_ijm1k_np1half*dT_ijm1k_np1half;
  double dT4_ijm1k_np1half=dTSq_ijm1k_np1half*dTSq_ijm1k_np1half;
  
  double dT_ijkp1_np1half=(dT_ijkp1_np1+grid.dLocalGridOld[grid.nT][i][j][k+1])*0.5;
  double dTSq_ijkp1_np1half=dT_ijkp1_np1half*dT_ijkp1_np1half;
  double dT4_ijkp1_np1half=dTSq_ijkp1_np1half*dTSq_ijkp1_np1half;
  
  double dT_ijkm1_np1half=(dT_ijkm1_np1+grid.dLocalGridOld[grid.nT][i][j][k-1])*0.5;
  double dTSq_ijkm1_np1half=dT_ijkm1_np1half*dT_ijkm1_np1half;
  double dT4_ijkm1_np1half=dTSq_ijkm1_np1half*dTSq_ijkm1_np1half;
  
  double dE_ijk_np1=parameters.eosTable.dGetEnergy(dT_ijk_np1
    ,grid.dLocalGridNew[grid.nD][i][j][k]);
  double dE_ip1jk_np1half=parameters.eosTable.dGetEnergy(dT_ip1jk_np1half
    ,grid.dLocalGridOld[grid.nD][i+1][j][k]);
  double dE_ijk_np1half=parameters.eosTable.dGetEnergy(dT_ijk_np1half
    ,grid.dLocalGridOld[grid.nD][i][j][k]);
  double dE_im1jk_np1half=parameters.eosTable.dGetEnergy(dT_im1jk_np1half
    ,grid.dLocalGridOld[grid.nD][i-1][j][k]);
  double dE_ijp1k_np1half=parameters.eosTable.dGetEnergy(dT_ijp1k_np1half
    ,grid.dLocalGridOld[grid.nD][i][j+1][k]);
  double dE_ijm1k_np1half=parameters.eosTable.dGetEnergy(dT_ijm1k_np1half
    ,grid.dLocalGridOld[grid.nD][i][j-1][k]);
  double dE_ijkp1_np1half=parameters.eosTable.dGetEnergy(dT_ijkp1_np1half
    ,grid.dLocalGridOld[grid.nD][i][j][k+1]);
  double dE_ijkm1_np1half=parameters.eosTable.dGetEnergy(dT_ijkm1_np1half
    ,grid.dLocalGridOld[grid.nD][i][j][k-1]);
  
  double dE_ip1halfjk_np1half=(dE_ip1jk_np1half+dE_ijk_np1half)*0.5;
  double dE_im1halfjk_np1half=(dE_im1jk_np1half+dE_ijk_np1half)*0.5;
  double dE_ijp1halfk_np1half=(dE_ijp1k_np1half+dE_ijk_np1half)*0.5;
  double dE_ijm1halfk_np1half=(dE_ijm1k_np1half+dE_ijk_np1half)*0.5;
  double dE_ijkp1half_np1half=(dE_ijkp1_np1half+dE_ijk_np1half)*0.5;
  double dE_ijkm1half_np1half=(dE_ijkm1_np1half+dE_ijk_np1half)*0.5;
  
  double dP_ijk_np1half=parameters.eosTable.dGetPressure(dT_ijk_np1half
    ,grid.dLocalGridOld[grid.nD][i][j][k]);
  #if VISCOUS_ENERGY_EQ==1
  dP_ijk_np1half=dP_ijk_np1half+grid.dLocalGridOld[grid.nQ0][i][j][k]
    +grid.dLocalGridOld[grid.nQ1][i][j][k]+grid.dLocalGridOld[grid.nQ2][i][j][k];
  #endif
  
  double dKappa_ip1jk_np1half=parameters.eosTable.dGetOpacity(dT_ip1jk_np1half
    ,grid.dLocalGridOld[grid.nD][i+1][j][k]);
  double dKappa_ijk_np1half=parameters.eosTable.dGetOpacity(dT_ijk_np1half
    ,grid.dLocalGridOld[grid.nD][i][j][k]);
  double dKappa_im1jk_np1half=parameters.eosTable.dGetOpacity(dT_im1jk_np1half
    ,grid.dLocalGridOld[grid.nD][i-1][j][k]);
  double dKappa_ijp1k_np1half=parameters.eosTable.dGetOpacity(dT_ijp1k_np1half
    ,grid.dLocalGridOld[grid.nD][i][j+1][k]);
  double dKappa_ijm1k_np1half=parameters.eosTable.dGetOpacity(dT_ijm1k_np1half
    ,grid.dLocalGridOld[grid.nD][i][j-1][k]);
  double dKappa_ijkp1_np1half=parameters.eosTable.dGetOpacity(dT_ijkp1_np1half
    ,grid.dLocalGridOld[grid.nD][i][j][k+1]);
  double dKappa_ijkm1_np1half=parameters.eosTable.dGetOpacity(dT_ijkm1_np1half
    ,grid.dLocalGridOld[grid.nD][i][j][k-1]);
  
  double dKappa_ip1halfjk_np1half=(dT4_ip1jk_np1half+dT4_ijk_np1half)/(dT4_ijk_np1half
    /dKappa_ijk_np1half+dT4_ip1jk_np1half/dKappa_ip1jk_np1half);
  double dKappa_im1halfjk_np1half=(dT4_im1jk_np1half+dT4_ijk_np1half)/(dT4_ijk_np1half
    /dKappa_ijk_np1half+dT4_im1jk_np1half/dKappa_im1jk_np1half);
  double dKappa_ijp1halfk_np1half=(dT4_ijp1k_np1half+dT4_ijk_np1half)/(dT4_ijk_np1half
    /dKappa_ijk_np1half+dT4_ijp1k_np1half/dKappa_ijp1k_np1half);
  double dKappa_ijm1halfk_np1half=(dT4_ijm1k_np1half+dT4_ijk_np1half)/(dT4_ijk_np1half
    /dKappa_ijk_np1half+dT4_ijm1k_np1half/dKappa_ijm1k_np1half);
  double dKappa_ijkp1half_np1half=(dT4_ijkp1_np1half+dT4_ijk_np1half)/(dT4_ijk_np1half
    /dKappa_ijk_np1half+dT4_ijkp1_np1half/dKappa_ijkp1_np1half);
  double dKappa_ijkm1half_np1half=(dT4_ijkm1_np1half+dT4_ijk_np1half)/(dT4_ijk_np1half
    /dKappa_ijk_np1half+dT4_ijkm1_np1half/dKappa_ijkm1_np1half);
  
  //Calcuate dA1
  double dA1CenGrad=(dE_ip1halfjk_np1half-dE_im1halfjk_np1half)
    /grid.dLocalGridOld[grid.nDM][i][0][0];
  double dA1UpWindGrad=0.0;
  double dU_U0_Diff=(dU_ijk_np1half-dU0_i_np1half);
  if(dU_U0_Diff<0.0){//moving in the negative direction
    dA1UpWindGrad=(dE_ip1jk_np1half-dE_ijk_np1half)/(grid.dLocalGridOld[grid.nDM][i+1][0][0]
      +grid.dLocalGridOld[grid.nDM][i][0][0])*2.0;
  }
  else{//moving in the postive direction
    dA1UpWindGrad=(dE_ijk_np1half-dE_im1jk_np1half)/(grid.dLocalGridOld[grid.nDM][i][0][0]
      +grid.dLocalGridOld[grid.nDM][i-1][0][0])*2.0;
  }
  double dA1=dU_U0_Diff*dRSq_i_np1half*((1.0-parameters.dDonorFrac)*dA1CenGrad
    +parameters.dDonorFrac*dA1UpWindGrad);
  
  //calculate dS1
  double dUR2_im1halfjk_np1half=grid.dLocalGridNew[grid.nU][nIInt-1][j][k]*dRSq_im1half_np1half;
  double dUR2_ip1halfjk_np1half=grid.dLocalGridNew[grid.nU][nIInt][j][k]*dRSq_ip1half_np1half;
  double dS1=dP_ijk_np1half/grid.dLocalGridOld[grid.nD][i][j][k]
    *(dUR2_ip1halfjk_np1half-dUR2_im1halfjk_np1half)/grid.dLocalGridOld[grid.nDM][i][0][0];
  
  //Calcualte dA2
  double dA2CenGrad=(dE_ijp1halfk_np1half-dE_ijm1halfk_np1half)
    /grid.dLocalGridOld[grid.nDTheta][0][j][0];
  double dA2UpWindGrad=0.0;
  if(dV_ijk_np1half<0.0){//moving in the negative direction
    dA2UpWindGrad=(dE_ijp1k_np1half-dE_ijk_np1half)/(grid.dLocalGridOld[grid.nDTheta][0][j+1][0]
      +grid.dLocalGridOld[grid.nDTheta][0][j][0])*2.0;
  }
  else{//moving in the positive direction
    dA2UpWindGrad=(dE_ijk_np1half-dE_ijm1k_np1half)/(grid.dLocalGridOld[grid.nDTheta][0][j][0]
      +grid.dLocalGridOld[grid.nDTheta][0][j-1][0])*2.0;
  }
  double dA2=dV_ijk_np1half/dR_i_np1half*((1.0-parameters.dDonorFrac)*dA2CenGrad
    +parameters.dDonorFrac*dA2UpWindGrad);
    
  //Calcualte dS2
  double dS2=dP_ijk_np1half/(grid.dLocalGridOld[grid.nD][i][j][k]*dR_i_np1half
    *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]*grid.dLocalGridOld[grid.nDTheta][0][j][0])
    *(dVSinTheta_ijp1halfk_np1half-dVSinTheta_ijm1halfk_np1half);
  
  //Calcualte dA3
  double dA3CenGrad=(dE_ijkp1half_np1half-dE_ijkm1half_np1half)
    /grid.dLocalGridOld[grid.nDPhi][0][0][k];
  double dA3UpWindGrad=0.0;
  if(dW_ijk_np1half<0.0){//moving in the negative direction
    dA3UpWindGrad=(dE_ijkp1_np1half-dE_ijk_np1half)/(grid.dLocalGridOld[grid.nDPhi][0][0][k+1]
      +grid.dLocalGridOld[grid.nDPhi][0][0][k])*2.0;
  }
  else{//moving in the positive direction
    dA3UpWindGrad=(dE_ijk_np1half-dE_ijkm1_np1half)/(grid.dLocalGridOld[grid.nDPhi][0][0][k]
      +grid.dLocalGridOld[grid.nDPhi][0][0][k-1])*2.0;
  }
  double dA3=dW_ijk_np1half/(dR_i_np1half*grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0])*
    ((1.0-parameters.dDonorFrac)*dA3CenGrad+parameters.dDonorFrac*dA3UpWindGrad);
  
  //Calcualte dS3
  double dS3=dP_ijk_np1half/(grid.dLocalGridOld[grid.nD][i][j][k]*dR_i_np1half
    *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]*grid.dLocalGridOld[grid.nDPhi][0][0][k])
    *(dW_ijkp1half_np1half-dW_ijkm1half_np1half);
  
  //Calculate dS4
  double dTGrad_ip1half_np1half=(dT4_ip1jk_np1half-dT4_ijk_np1half)
    /(grid.dLocalGridOld[grid.nDM][i+1][0][0]+grid.dLocalGridOld[grid.nDM][i][0][0])*2.0;
  double dTGrad_im1half_np1half=(dT4_ijk_np1half-dT4_im1jk_np1half)
    /(grid.dLocalGridOld[grid.nDM][i][0][0]+grid.dLocalGridOld[grid.nDM][i-1][0][0])*2.0;
  double dGrad_ip1half_np1half=dRhoAve_ip1half_n*dR4_ip1half_np1half/(dKappa_ip1halfjk_np1half
    *dRho_ip1halfjk_n)*dTGrad_ip1half_np1half;
  double dGrad_im1half_np1half=dRhoAve_im1half_n*dR4_im1half_np1half/(dKappa_im1halfjk_np1half
    *dRho_im1halfjk_n)*dTGrad_im1half_np1half;
  double dS4=16.0*dPiSq*grid.dLocalGridOld[grid.nDenAve][i][0][0]
    *(dGrad_ip1half_np1half-dGrad_im1half_np1half)/grid.dLocalGridOld[grid.nDM][i][0][0];
  
  //Calculate dS5
  double dTGrad_jp1half_np1half=(dT4_ijp1k_np1half-dT4_ijk_np1half)
    /(grid.dLocalGridOld[grid.nDTheta][0][j+1][0]+grid.dLocalGridOld[grid.nDTheta][0][j][0])*2.0;
  double dTGrad_jm1half_np1half=(dT4_ijk_np1half-dT4_ijm1k_np1half)
    /(grid.dLocalGridOld[grid.nDTheta][0][j][0]+grid.dLocalGridOld[grid.nDTheta][0][j-1][0])*2.0;
  double dGrad_jp1half_np1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt][0]
    /(dKappa_ijp1halfk_np1half*dRho_ijp1halfk_n)*dTGrad_jp1half_np1half;
  double dGrad_jm1half_np1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt-1][0]
    /(dKappa_ijm1halfk_np1half*dRho_ijm1halfk_n)*dTGrad_jm1half_np1half;
  double dS5=(dGrad_jp1half_np1half-dGrad_jm1half_np1half)
    /(grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]
    *dRSq_i_np1half*grid.dLocalGridOld[grid.nDTheta][0][j][0]);
  
  //Calculate dS6
  double dTGrad_kp1half_np1half=(dT4_ijkp1_np1half-dT4_ijk_np1half)
    /(grid.dLocalGridOld[grid.nDPhi][0][0][k+1]+grid.dLocalGridOld[grid.nDPhi][0][0][k])*2.0;
  double dTGrad_km1half_np1half=(dT4_ijk_np1half-dT4_ijkm1_np1half)
    /(grid.dLocalGridOld[grid.nDPhi][0][0][k]+grid.dLocalGridOld[grid.nDPhi][0][0][k-1])*2.0;
  double dGrad_kp1half_np1half=dTGrad_kp1half_np1half/(dKappa_ijkp1half_np1half*dRho_ijkp1half_n);
  double dGrad_km1half_np1half=dTGrad_km1half_np1half/(dKappa_ijkm1half_np1half*dRho_ijkm1half_n);
  double dS6=(dGrad_kp1half_np1half-dGrad_km1half_np1half)/(dRSq_i_np1half
    *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]*grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]
    *grid.dLocalGridOld[grid.nDPhi][0][0][k]);
  /*
  //calculate DUDM_ijk_np1half
  double dDUDM_ijk_np1half=4.0*parameters.dPi*grid.dLocalGridOld[grid.nDenAve][i][0][0]
    *dRSq_i_np1half*((grid.dLocalGridNew[grid.nU][nIInt][j][k]
    -grid.dLocalGridNew[grid.nU0][nIInt][0][0])-(grid.dLocalGridNew[grid.nU][nIInt-1][j][k]
    -grid.dLocalGridNew[grid.nU0][nIInt-1][0][0]))/grid.dLocalGridOld[grid.nDM][i][0][0];
  
  //calculate DVDM_ijk_np1half
  double dDVDM_ijk_np1half=4.0*parameters.dPi*grid.dLocalGridOld[grid.nDenAve][i][0][0]
    *dRSq_i_np1half*(dV_ip1halfjk_np1half-dV_im1halfjk_np1half)
    /grid.dLocalGridOld[grid.nDM][i][0][0];
  
  //calculate DUDTheta_ijk_np1half
  double dDUDTheta_ijk_np1half=((dU_ijp1halfk_np1half-dU0_i_np1half)-(dU_ijm1halfk_np1half
    -dU0_i_np1half))/(dR_i_np1half*grid.dLocalGridOld[grid.nDTheta][0][j][0]);
  
  //calculate DVDTheta_ijk_np1half
  double dDVDTheta_ijk_np1half=(grid.dLocalGridNew[grid.nV][i][nJInt][k]
    -grid.dLocalGridNew[grid.nV][i][nJInt-1][k])/(dR_i_np1half
    *grid.dLocalGridOld[grid.nDTheta][0][j][0]);
  
  //calculate DUDPhi_ijk_np1half
  double dDUDPhi_ijk_np1half=((dU_ijkp1half_np1half-dU0_i_np1half)-(dU_ijkm1half_np1half
    -dU0_i_np1half))/(dR_i_np1half*grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]
    *grid.dLocalGridOld[grid.nDPhi][0][0][k]);
  
  //calculate DVDPhi_ijk_np1half
  double dDVDPhi_ijk_np1half=(dV_ijkp1half_np1half-dV_ijkm1half_np1half)/(dR_i_np1half
    *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]*grid.dLocalGridOld[grid.nDPhi][0][0][k]);
  
  //calculate DWDM_ijk_np1half
  double dDWDM_ijk_np1half=4.0*parameters.dPi*grid.dLocalGridOld[grid.nDenAve][i][0][0]
    *dRSq_i_np1half*(dW_ip1halfjk_np1half-dW_im1halfjk_np1half)
    /grid.dLocalGridOld[grid.nDM][i][0][0];
  
  //calculate DWDTheta_ijk_np1half
  double dDWDTheta_ijk_np1half=(dW_ijp1halfk_np1half-dW_ijm1halfk_np1half)/(dR_i_np1half
    *grid.dLocalGridOld[grid.nDTheta][0][j][0]);
  
  //calculate DWDPhi_ijk_np1half
  double dDWDPhi_ijk_np1half=(grid.dLocalGridNew[grid.nW][i][j][nKInt]
    -grid.dLocalGridNew[grid.nW][i][j][nKInt-1])/(dR_i_np1half
    *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]*grid.dLocalGridOld[grid.nDPhi][0][0][k]);
  
  //cal DivU_ijk_np1half
  double dDivU_ijk_np1half=dDUDM_ijk_np1half+dDVDTheta_ijk_np1half+dDWDPhi_ijk_np1half;
  
  //cal Tau_rr_ijk_np1half
  double dTau_rr_ijk_np1half=2.0*grid.dLocalGridNew[grid.nEddyVisc][i][j][k]*(dDUDM_ijk_np1half
    -0.3333333333333333*dDivU_ijk_np1half);
  
  //calculate Tau_tt_ijk_np1half
  double dTau_tt_ijk_np1half=2.0*grid.dLocalGridNew[grid.nEddyVisc][i][j][k]*(dDVDTheta_ijk_np1half
    -0.333333333333333*dDivU_ijk_np1half);
  
  //calculate dTau_pp_ijk_np1half
  double dTau_pp_ijk_np1half=2.0*grid.dLocalGridNew[grid.nEddyVisc][i][j][k]*(dDWDPhi_ijk_np1half
    -0.3333333333333333*dDivU_ijk_np1half);
  
  //calculate Tau_rt_ijk_np1half
  double dTau_rt_ijk_np1half=grid.dLocalGridNew[grid.nEddyVisc][i][j][k]*(dDVDM_ijk_np1half
    +dDUDTheta_ijk_np1half);
  
  //calculate dTau_rp_ijk_np1half
  double dTau_rp_ijk_np1half=grid.dLocalGridNew[grid.nEddyVisc][i][j][k]*(dDUDPhi_ijk_np1half
    +dDWDM_ijk_np1half);
  
  //calculate dTau_tp_ijk_np1half
  double dTau_tp_ijk_np1half=grid.dLocalGridNew[grid.nEddyVisc][i][j][k]*(dDVDPhi_ijk_np1half
    +dDWDTheta_ijk_np1half);
  
  //eddy viscosity terms
  double dEddyViscosityTerms=(dTau_rr_ijk_np1half*dDUDM_ijk_np1half+dTau_tt_ijk_np1half
    *dDVDTheta_ijk_np1half+dTau_pp_ijk_np1half*dDWDPhi_ijk_np1half+dTau_rt_ijk_np1half
    *(dDUDTheta_ijk_np1half+dDVDM_ijk_np1half)+dTau_rp_ijk_np1half*(dDUDPhi_ijk_np1half
    +dDWDM_ijk_np1half)+dTau_tp_ijk_np1half*(dDVDPhi_ijk_np1half+dDWDTheta_ijk_np1half))
    /grid.dLocalGridOld[grid.nD][i][j][k];*/
  
  //calculate dT1
  double dEGrad_ip1halfjk_np1half=dR4_ip1half_np1half*dEddyVisc_ip1halfjk_n*dRhoAve_ip1half_n
    *(dE_ip1jk_np1half-dE_ijk_np1half)/(dRho_ip1halfjk_n*dDM_ip1half);
  double dEGrad_im1halfjk_np1half=dR4_im1half_np1half*dEddyVisc_im1halfjk_n*dRhoAve_im1half_n
    *(dE_ijk_np1half-dE_im1jk_np1half)/(dRho_im1halfjk_n*dDM_im1half);
  double dT1=16.0*dPiSq*grid.dLocalGridOld[grid.nDenAve][i][0][0]*(dEGrad_ip1halfjk_np1half
    -dEGrad_im1halfjk_np1half)/grid.dLocalGridOld[grid.nDM][i][0][0];
  
  //calculate dT2
  double dEGrad_ijp1halfk_np1half=dEddyVisc_ijp1halfk_n
    *grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt][0]
    *(dE_ijp1k_np1half-dE_ijk_np1half)/(dRho_ijp1halfk_n*dR_i_np1half*dDelTheta_jp1half);
  double dEGrad_ijm1halfk_np1half=dEddyVisc_ijm1halfk_n
    *grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt-1][0]
    *(dE_ijk_np1half-dE_ijm1k_np1half)/(dRho_ijm1halfk_n*dR_i_np1half*dDelTheta_jm1half);
  double dT2=(dEGrad_ijp1halfk_np1half-dEGrad_ijm1halfk_np1half)/(dR_i_np1half
    *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]
    *grid.dLocalGridOld[grid.nDTheta][0][j][0]);
  
  //calculate dT3
  double dEGrad_ijkp1half_np1half=dEddyVisc_ijkp1half_n*(dE_ijkp1_np1half-dE_ijk_np1half)
    /(dRho_ijkp1half_n*grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]*dR_i_np1half*dDelPhi_kp1half);
  double dEGrad_ijkm1half_np1half=dEddyVisc_ijkm1half_n*(dE_ijk_np1half-dE_ijm1k_np1half)
    /(dRho_ijkm1half_n*grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]*dR_i_np1half*dDelPhi_km1half);
  double dT3=(dEGrad_ijkp1half_np1half-dEGrad_ijkm1half_np1half)/(dR_i_np1half
    *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]
    *grid.dLocalGridOld[grid.nDPhi][0][0][k]);
  
  //eddy viscosity terms
  double dEddyViscosityTerms=(dT1+dT2+dT3)/parameters.dPrt;
  
  //calculate energy equation discrepancy
  return (dE_ijk_np1-grid.dLocalGridOld[grid.nE][i][j][k])/time.dDeltat_n
    +4.0*parameters.dPi*grid.dLocalGridOld[grid.nDenAve][i][0][0]*(dA1+dS1)+dA2+dS2+dA3+dS3
    -4.0*parameters.dSigma/(3.0*grid.dLocalGridOld[grid.nD][i][j][k])*(dS4+dS5+dS6)
    -dEddyViscosityTerms;
}
double dImplicitEnergyFunction_RTP_LES_SB(Grid &grid,Parameters &parameters,Time &time
  ,double dTemps[],int i,int j,int k){//needs terms
  double dT_ijk_np1=dTemps[0];
  double dT_im1jk_np1=dTemps[1];
  double dT_ijp1k_np1=dTemps[2];
  double dT_ijm1k_np1=dTemps[3];
  double dT_ijkp1_np1=dTemps[4];
  double dT_ijkm1_np1=dTemps[5];
  
  double dPiSq=parameters.dPi*parameters.dPi;
  
  //calculate i,j,k for interface centered quantities
  int nIInt=i+grid.nCenIntOffset[0];
  int nJInt=j+grid.nCenIntOffset[1];
  int nKInt=k+grid.nCenIntOffset[2];
    
  //Calculate interpolated quantities
  double dDM_ip1half=(grid.dLocalGridOld[grid.nDM][i][0][0])*(0.5+parameters.dAlpha
    +parameters.dAlphaExtra);/**\BC Missing \f$\Delta M_r\f$ outside model using 
    \ref Parameters.dAlpha times \f$\Delta M_r\f$ in the last zone instead.*/
  double dDM_im1half=(grid.dLocalGridOld[grid.nDM][i][0][0]+grid.dLocalGridOld[grid.nDM][i-1][0][0])
    *0.5;
  double dDelTheta_jp1half=(grid.dLocalGridOld[grid.nDTheta][0][j][0]
    +grid.dLocalGridOld[grid.nDTheta][0][j+1][0])*0.5;
  double dDelTheta_jm1half=(grid.dLocalGridOld[grid.nDTheta][0][j][0]
    +grid.dLocalGridOld[grid.nDTheta][0][j-1][0])*0.5;
  double dDelPhi_kp1half=(grid.dLocalGridOld[grid.nDPhi][0][0][k]
    +grid.dLocalGridOld[grid.nDPhi][0][0][k+1])*0.5;
  double dDelPhi_km1half=(grid.dLocalGridOld[grid.nDPhi][0][0][k]
    +grid.dLocalGridOld[grid.nDPhi][0][0][k-1])*0.5;
  double dR_ip1half_np1half=(grid.dLocalGridOld[grid.nR][nIInt][0][0]
    +grid.dLocalGridNew[grid.nR][nIInt][0][0])*0.5;
  double dR_im1half_np1half=(grid.dLocalGridOld[grid.nR][nIInt-1][0][0]
    +grid.dLocalGridNew[grid.nR][nIInt-1][0][0])*0.5;
  double dR_i_np1half=(dR_ip1half_np1half+dR_im1half_np1half)*0.5;
  double dRSq_i_np1half=dR_i_np1half*dR_i_np1half;
  double dRSq_ip1half_np1half=dR_ip1half_np1half*dR_ip1half_np1half;
  double dRSq_im1half_np1half=dR_im1half_np1half*dR_im1half_np1half;
  double dR4_ip1half_np1half=dRSq_ip1half_np1half*dRSq_ip1half_np1half;
  double dR4_im1half_np1half=dRSq_im1half_np1half*dRSq_im1half_np1half;
  double dRhoAve_ip1half_n=(grid.dLocalGridOld[grid.nDenAve][i][0][0])*0.5;/**\BC missing density
    outside model assuming it is zero*/
  double dRhoAve_im1half_n=(grid.dLocalGridOld[grid.nDenAve][i][0][0]
    +grid.dLocalGridOld[grid.nDenAve][i-1][0][0])*0.5;
  double dRho_ip1halfjk_n=(grid.dLocalGridOld[grid.nD][i][j][k])*0.5;/**\BC missing desnity outside
    model assuming it is zero*/
  double dRho_im1halfjk_n=(grid.dLocalGridOld[grid.nD][i][j][k]
    +grid.dLocalGridOld[grid.nD][i-1][j][k])*0.5;
  double dRho_ijp1halfk_n=(grid.dLocalGridOld[grid.nD][i][j+1][k]
    +grid.dLocalGridOld[grid.nD][i][j][k])*0.5;
  double dRho_ijm1halfk_n=(grid.dLocalGridOld[grid.nD][i][j][k]
    +grid.dLocalGridOld[grid.nD][i][j-1][k])*0.5;
  double dRho_ijkp1half_n=(grid.dLocalGridOld[grid.nD][i][j][k+1]
    +grid.dLocalGridOld[grid.nD][i][j][k])*0.5;
  double dRho_ijkm1half_n=(grid.dLocalGridOld[grid.nD][i][j][k]
    +grid.dLocalGridOld[grid.nD][i][j][k-1])*0.5;
  double dU0_i_np1half=(grid.dLocalGridNew[grid.nU0][nIInt][0][0]
    +grid.dLocalGridNew[grid.nU0][nIInt-1][0][0])*0.5;
  double dU_ijk_np1half=(grid.dLocalGridNew[grid.nU][nIInt][j][k]
    +grid.dLocalGridNew[grid.nU][nIInt-1][j][k])*0.5;
  double dU_ijp1halfk_np1half=(grid.dLocalGridNew[grid.nU][nIInt][j+1][k]
    +grid.dLocalGridNew[grid.nU][nIInt-1][j+1][k]+grid.dLocalGridNew[grid.nU][nIInt][j][k]
    +grid.dLocalGridNew[grid.nU][nIInt-1][j][k])*0.25;
  double dU_ijm1halfk_np1half=(grid.dLocalGridNew[grid.nU][nIInt][j-1][k]
    +grid.dLocalGridNew[grid.nU][nIInt-1][j-1][k]+grid.dLocalGridNew[grid.nU][nIInt][j][k]
    +grid.dLocalGridNew[grid.nU][nIInt-1][j][k])*0.25;
  double dU_ijkp1half_np1half=(grid.dLocalGridNew[grid.nU][nIInt][j][k]
    +grid.dLocalGridNew[grid.nU][nIInt][j][k+1]+grid.dLocalGridNew[grid.nU][nIInt-1][j][k]
    +grid.dLocalGridNew[grid.nU][nIInt-1][j][k+1])*0.25;
  double dU_ijkm1half_np1half=(grid.dLocalGridNew[grid.nU][nIInt][j][k]
    +grid.dLocalGridNew[grid.nU][nIInt][j][k-1]+grid.dLocalGridNew[grid.nU][nIInt-1][j][k]
    +grid.dLocalGridNew[grid.nU][nIInt-1][j][k-1])*0.25;
  double dV_ijk_np1half=(grid.dLocalGridNew[grid.nV][i][nJInt][k]
    +grid.dLocalGridNew[grid.nV][i][nJInt-1][k])*0.5;
  double dV_ip1halfjk_np1half=(grid.dLocalGridNew[grid.nV][i][nJInt][k]
    +grid.dLocalGridNew[grid.nV][i][nJInt-1][k])*0.5;/**\BC assuming V at ip1half is the same as V 
    at i*/
  double dV_im1halfjk_np1half=(grid.dLocalGridNew[grid.nV][i][nJInt][k]
    +grid.dLocalGridNew[grid.nV][i][nJInt-1][k]+grid.dLocalGridNew[grid.nV][i-1][nJInt][k]
    +grid.dLocalGridNew[grid.nV][i-1][nJInt-1][k])*0.25;
  double dV_ijkp1half_np1half=(grid.dLocalGridNew[grid.nV][i][nJInt][k+1]
    +grid.dLocalGridNew[grid.nV][i][nJInt-1][k+1]+grid.dLocalGridNew[grid.nV][i][nJInt][k]
    +grid.dLocalGridNew[grid.nV][i][nJInt-1][k])*0.25;
  double dV_ijkm1half_np1half=(grid.dLocalGridNew[grid.nV][i][nJInt][k]
    +grid.dLocalGridNew[grid.nV][i][nJInt-1][k]+grid.dLocalGridNew[grid.nV][i][nJInt][k-1]
    +grid.dLocalGridNew[grid.nV][i][nJInt-1][k-1])*0.25;
  double dW_ijk_np1half=(grid.dLocalGridNew[grid.nW][i][j][nKInt]
    +grid.dLocalGridNew[grid.nW][i][j][nKInt-1])*0.5;
  double dW_ijkp1half_np1half=(grid.dLocalGridNew[grid.nW][i][j][nKInt]);
  double dW_ijkm1half_np1half=(grid.dLocalGridNew[grid.nW][i][j][nKInt-1]);
  double dW_ip1halfjk_np1half=(grid.dLocalGridNew[grid.nW][i][j][nKInt]
    +grid.dLocalGridNew[grid.nW][i][j][nKInt-1])*0.5;/**\BC assuming W at ip1half is the same as W 
    at i*/
  double dW_im1halfjk_np1half=(grid.dLocalGridNew[grid.nW][i][j][nKInt]
    +grid.dLocalGridNew[grid.nW][i][j][nKInt-1]+grid.dLocalGridNew[grid.nW][i-1][j][nKInt]
    +grid.dLocalGridNew[grid.nW][i-1][j][nKInt-1])*0.25;
  double dW_ijp1halfk_np1half=(grid.dLocalGridNew[grid.nW][i][j+1][nKInt]
    +grid.dLocalGridNew[grid.nW][i][j+1][nKInt-1]+grid.dLocalGridNew[grid.nW][i][j][nKInt]
    +grid.dLocalGridNew[grid.nW][i][j][nKInt-1])*0.25;
  double dW_ijm1halfk_np1half=(grid.dLocalGridNew[grid.nW][i][j][nKInt]
    +grid.dLocalGridNew[grid.nW][i][j][nKInt-1]+grid.dLocalGridNew[grid.nW][i][j-1][nKInt]
    +grid.dLocalGridNew[grid.nW][i][j-1][nKInt-1])*0.25;
  double dEddyVisc_ip1halfjk_n=(grid.dLocalGridOld[grid.nEddyVisc][i][j][k])*0.5;
  double dEddyVisc_im1halfjk_n=(grid.dLocalGridOld[grid.nEddyVisc][i-1][j][k]
    +grid.dLocalGridOld[grid.nEddyVisc][i][j][k])*0.5;
  double dEddyVisc_ijp1halfk_n=(grid.dLocalGridOld[grid.nEddyVisc][i][j+1][k]
  +grid.dLocalGridOld[grid.nEddyVisc][i][j][k])*0.5;
  double dEddyVisc_ijm1halfk_n=(grid.dLocalGridOld[grid.nEddyVisc][i][j-1][k]
    +grid.dLocalGridOld[grid.nEddyVisc][i][j][k])*0.5;
  double dEddyVisc_ijkp1half_n=(grid.dLocalGridOld[grid.nEddyVisc][i][j][k+1]
    +grid.dLocalGridOld[grid.nEddyVisc][i][j][k])*0.5;
  double dEddyVisc_ijkm1half_n=(grid.dLocalGridOld[grid.nEddyVisc][i][j][k-1]
    +grid.dLocalGridOld[grid.nEddyVisc][i][j][k])*0.5;
  
  double dT_ijk_np1half=(dT_ijk_np1+grid.dLocalGridOld[grid.nT][i][j][k])*0.5;
  double dTSq_ijk_np1half=dT_ijk_np1half*dT_ijk_np1half;
  double dT4_ijk_np1half=dTSq_ijk_np1half*dTSq_ijk_np1half;
  
  double dT_im1jk_np1half=(dT_im1jk_np1+grid.dLocalGridOld[grid.nT][i-1][j][k])*0.5;
  double dTSq_im1jk_np1half=dT_im1jk_np1half*dT_im1jk_np1half;
  double dT4_im1jk_np1half=dTSq_im1jk_np1half*dTSq_im1jk_np1half;
  
  double dT_ijp1k_np1half=(dT_ijp1k_np1+grid.dLocalGridOld[grid.nT][i][j+1][k])*0.5;
  double dTSq_ijp1k_np1half=dT_ijp1k_np1half*dT_ijp1k_np1half;
  double dT4_ijp1k_np1half=dTSq_ijp1k_np1half*dTSq_ijp1k_np1half;
  
  double dT_ijm1k_np1half=(dT_ijm1k_np1+grid.dLocalGridOld[grid.nT][i][j-1][k])*0.5;
  double dTSq_ijm1k_np1half=dT_ijm1k_np1half*dT_ijm1k_np1half;
  double dT4_ijm1k_np1half=dTSq_ijm1k_np1half*dTSq_ijm1k_np1half;
  
  double dT_ijkp1_np1half=(dT_ijkp1_np1+grid.dLocalGridOld[grid.nT][i][j][k+1])*0.5;
  double dTSq_ijkp1_np1half=dT_ijkp1_np1half*dT_ijkp1_np1half;
  double dT4_ijkp1_np1half=dTSq_ijkp1_np1half*dTSq_ijkp1_np1half;
  
  double dT_ijkm1_np1half=(dT_ijkm1_np1+grid.dLocalGridOld[grid.nT][i][j][k-1])*0.5;
  double dTSq_ijkm1_np1half=dT_ijkm1_np1half*dT_ijkm1_np1half;
  double dT4_ijkm1_np1half=dTSq_ijkm1_np1half*dTSq_ijkm1_np1half;
  
  double dE_ijk_np1=parameters.eosTable.dGetEnergy(dT_ijk_np1
    ,grid.dLocalGridNew[grid.nD][i][j][k]);
  double dE_ijk_np1half=parameters.eosTable.dGetEnergy(dT_ijk_np1half
    ,grid.dLocalGridOld[grid.nD][i][j][k]);
  double dE_im1jk_np1half=parameters.eosTable.dGetEnergy(dT_im1jk_np1half
    ,grid.dLocalGridOld[grid.nD][i-1][j][k]);
  double dE_ijp1k_np1half=parameters.eosTable.dGetEnergy(dT_ijp1k_np1half
    ,grid.dLocalGridOld[grid.nD][i][j+1][k]);
  double dE_ijm1k_np1half=parameters.eosTable.dGetEnergy(dT_ijm1k_np1half
    ,grid.dLocalGridOld[grid.nD][i][j-1][k]);
  double dE_ijkp1_np1half=parameters.eosTable.dGetEnergy(dT_ijkp1_np1half
    ,grid.dLocalGridOld[grid.nD][i][j][k+1]);
  double dE_ijkm1_np1half=parameters.eosTable.dGetEnergy(dT_ijkm1_np1half
    ,grid.dLocalGridOld[grid.nD][i][j][k-1]);
  double dE_ip1jk_np1half=dE_ijk_np1half;/**\BC Assuming energy outside model is the same as
    the energy in the last zone inside the model.*/
  double dE_ip1halfjk_np1half=dE_ijk_np1half;/**\BC Assuming energy outside model is the same as
    the energy in the last zone inside the model.*/
  double dE_im1halfjk_np1half=(dE_im1jk_np1half+dE_ijk_np1half)*0.5;
  double dE_ijp1halfk_np1half=(dE_ijp1k_np1half+dE_ijk_np1half)*0.5;
  double dE_ijm1halfk_np1half=(dE_ijm1k_np1half+dE_ijk_np1half)*0.5;
  double dE_ijkp1half_np1half=(dE_ijkp1_np1half+dE_ijk_np1half)*0.5;
  double dE_ijkm1half_np1half=(dE_ijkm1_np1half+dE_ijk_np1half)*0.5;
  
  double dP_ijk_np1half=parameters.eosTable.dGetPressure(dT_ijk_np1half
    ,grid.dLocalGridOld[grid.nD][i][j][k]);
  #if VISCOUS_ENERGY_EQ==1
  dP_ijk_np1half=dP_ijk_np1half+grid.dLocalGridOld[grid.nQ0][i][j][k]
    +grid.dLocalGridOld[grid.nQ1][i][j][k]+grid.dLocalGridOld[grid.nQ2][i][j][k];
  #endif
  
  double dKappa_ijk_np1half=parameters.eosTable.dGetOpacity(dT_ijk_np1half
    ,grid.dLocalGridOld[grid.nD][i][j][k]);
  double dKappa_im1jk_np1half=parameters.eosTable.dGetOpacity(dT_im1jk_np1half
    ,grid.dLocalGridOld[grid.nD][i-1][j][k]);
  double dKappa_ijp1k_np1half=parameters.eosTable.dGetOpacity(dT_ijp1k_np1half
    ,grid.dLocalGridOld[grid.nD][i][j+1][k]);
  double dKappa_ijm1k_np1half=parameters.eosTable.dGetOpacity(dT_ijm1k_np1half
    ,grid.dLocalGridOld[grid.nD][i][j-1][k]);
  double dKappa_ijkp1_np1half=parameters.eosTable.dGetOpacity(dT_ijkp1_np1half
    ,grid.dLocalGridOld[grid.nD][i][j][k+1]);
  double dKappa_ijkm1_np1half=parameters.eosTable.dGetOpacity(dT_ijkm1_np1half
    ,grid.dLocalGridOld[grid.nD][i][j][k-1]);
  
  double dKappa_im1halfjk_np1half=(dT4_im1jk_np1half+dT4_ijk_np1half)/(dT4_ijk_np1half
    /dKappa_ijk_np1half+dT4_im1jk_np1half/dKappa_im1jk_np1half);
  double dKappa_ijp1halfk_np1half=(dT4_ijp1k_np1half+dT4_ijk_np1half)/(dT4_ijk_np1half
    /dKappa_ijk_np1half+dT4_ijp1k_np1half/dKappa_ijp1k_np1half);
  double dKappa_ijm1halfk_np1half=(dT4_ijm1k_np1half+dT4_ijk_np1half)/(dT4_ijk_np1half
    /dKappa_ijk_np1half+dT4_ijm1k_np1half/dKappa_ijm1k_np1half);
  double dKappa_ijkp1half_np1half=(dT4_ijkp1_np1half+dT4_ijk_np1half)/(dT4_ijk_np1half
    /dKappa_ijk_np1half+dT4_ijkp1_np1half/dKappa_ijkp1_np1half);
  double dKappa_ijkm1half_np1half=(dT4_ijkm1_np1half+dT4_ijk_np1half)/(dT4_ijk_np1half
    /dKappa_ijk_np1half+dT4_ijkm1_np1half/dKappa_ijkm1_np1half);
  
  //Calcuate dA1
  double dA1CenGrad=(dE_ip1halfjk_np1half-dE_im1halfjk_np1half)
    /grid.dLocalGridOld[grid.nDM][i][0][0];
  double dA1UpWindGrad=0.0;
  double dU_U0_Diff=(dU_ijk_np1half-dU0_i_np1half);
  if(dU_U0_Diff<0.0){//moving in the negative radial direction
    dA1UpWindGrad=dA1UpWindGrad;/**\BC Using centered gradient for upwind gradient when motion is 
      into the star at the surface*/
  }
  else{//moving in the postive radial direction
    dA1UpWindGrad=(dE_ijk_np1half-dE_im1jk_np1half)/(grid.dLocalGridOld[grid.nDM][i][0][0]
      +grid.dLocalGridOld[grid.nDM][i-1][0][0])*2.0;
  }
  double dA1=dU_U0_Diff*dRSq_i_np1half*((1.0-parameters.dDonorFrac)*dA1CenGrad
    +parameters.dDonorFrac*dA1UpWindGrad);
  
  //calculate dS1
  double dUR2_im1halfjk_np1half=grid.dLocalGridNew[grid.nU][nIInt-1][j][k]*dRSq_im1half_np1half;
  double dUR2_ip1halfjk_np1half=grid.dLocalGridNew[grid.nU][nIInt][j][k]*dRSq_ip1half_np1half;
  double dS1=dP_ijk_np1half/grid.dLocalGridOld[grid.nD][i][j][k]
    *(dUR2_ip1halfjk_np1half-dUR2_im1halfjk_np1half)/grid.dLocalGridOld[grid.nDM][i][0][0];
  
  //Calcualte dA2
  double dA2CenGrad=(dE_ijp1halfk_np1half-dE_ijm1halfk_np1half)
    /grid.dLocalGridOld[grid.nDTheta][0][j][0];
  double dA2UpWindGrad=0.0;
  if(dV_ijk_np1half<0.0){//moving in the negative theta direction
    dA2UpWindGrad=(dE_ijp1k_np1half-dE_ijk_np1half)/(grid.dLocalGridOld[grid.nDTheta][0][j+1][0]
      +grid.dLocalGridOld[grid.nDTheta][0][j][0])*2.0;
  }
  else{//moving in the positive theta direction
    dA2UpWindGrad=(dE_ijk_np1half-dE_ijm1k_np1half)/(grid.dLocalGridOld[grid.nDTheta][0][j][0]
      +grid.dLocalGridOld[grid.nDTheta][0][j-1][0])*2.0;
  }
  double dA2=dV_ijk_np1half/dR_i_np1half*((1.0-parameters.dDonorFrac)*dA2CenGrad
    +parameters.dDonorFrac*dA2UpWindGrad);
    
  //Calcualte dS2
  double dVSinTheta_ijp1halfk_np1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt][0]
    *grid.dLocalGridNew[grid.nV][i][nJInt][k];
  double dVSinTheta_ijm1halfk_np1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt-1][0]
    *grid.dLocalGridNew[grid.nV][i][nJInt-1][k];
  double dS2=dP_ijk_np1half/(grid.dLocalGridOld[grid.nD][i][j][k]*dR_i_np1half
    *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]*grid.dLocalGridOld[grid.nDTheta][0][j][0])
    *(dVSinTheta_ijp1halfk_np1half-dVSinTheta_ijm1halfk_np1half);
  
  //Calcualte dA3
  double dA3CenGrad=(dE_ijkp1half_np1half-dE_ijkm1half_np1half)
    /grid.dLocalGridOld[grid.nDPhi][0][0][k];
  double dA3UpWindGrad=0.0;
  if(dW_ijk_np1half<0.0){//moving in the negative phi direction
    dA3UpWindGrad=(dE_ijkp1_np1half-dE_ijk_np1half)/(grid.dLocalGridOld[grid.nDPhi][0][0][k+1]
      +grid.dLocalGridOld[grid.nDPhi][0][0][k])*2.0;
  }
  else{//moving in the positive phi direction
    dA3UpWindGrad=(dE_ijk_np1half-dE_ijkm1_np1half)/(grid.dLocalGridOld[grid.nDPhi][0][0][k]
      +grid.dLocalGridOld[grid.nDPhi][0][0][k-1])*2.0;
  }
  double dA3=dW_ijk_np1half/(dR_i_np1half*grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0])*
    ((1.0-parameters.dDonorFrac)*dA3CenGrad+parameters.dDonorFrac*dA3UpWindGrad);
  
  //Calcualte dS3
  double dS3=dP_ijk_np1half/(grid.dLocalGridOld[grid.nD][i][j][k]*dR_i_np1half
    *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]*grid.dLocalGridOld[grid.nDPhi][0][0][k])
    *(dW_ijkp1half_np1half-dW_ijkm1half_np1half);
  
  //Calculate dS4
  double dTGrad_im1half_np1half=(dT4_ijk_np1half-dT4_im1jk_np1half)
    /(grid.dLocalGridOld[grid.nDM][i][0][0]+grid.dLocalGridOld[grid.nDM][i-1][0][0])*2.0;
  double dGrad_ip1half_np1half=-3.0*dRSq_ip1half_np1half*dT4_ijk_np1half/(8.0*parameters.dPi);/**\BC 
    Missing grid.dLocalGridOld[grid.nT][i+1][0][0] using flux equals \f$2\sigma T^4\f$ at surface.*/
  double dGrad_im1half_np1half=dRhoAve_im1half_n*dR4_im1half_np1half/(dKappa_im1halfjk_np1half
    *dRho_im1halfjk_n)*dTGrad_im1half_np1half;
  double dS4=16.0*parameters.dPi*parameters.dPi*grid.dLocalGridOld[grid.nDenAve][i][0][0]
    *(dGrad_ip1half_np1half-dGrad_im1half_np1half)/grid.dLocalGridOld[grid.nDM][i][0][0];
  
  //Calculate dS5
  double dTGrad_jp1half_np1half=(dT4_ijp1k_np1half-dT4_ijk_np1half)
    /(grid.dLocalGridOld[grid.nDTheta][0][j+1][0]+grid.dLocalGridOld[grid.nDTheta][0][j][0])*2.0;
  double dTGrad_jm1half_np1half=(dT4_ijk_np1half-dT4_ijm1k_np1half)
    /(grid.dLocalGridOld[grid.nDTheta][0][j][0]+grid.dLocalGridOld[grid.nDTheta][0][j-1][0])*2.0;
  double dGrad_jp1half_np1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt][0]
    /(dKappa_ijp1halfk_np1half*dRho_ijp1halfk_n)*dTGrad_jp1half_np1half;
  double dGrad_jm1half_np1half=grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt-1][0]
    /(dKappa_ijm1halfk_np1half*dRho_ijm1halfk_n)*dTGrad_jm1half_np1half;
  double dS5=(dGrad_jp1half_np1half-dGrad_jm1half_np1half)
    /(grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]
    *dRSq_i_np1half*grid.dLocalGridOld[grid.nDTheta][0][j][0]);
  
  //Calculate dS6
  double dTGrad_kp1half_np1half=(dT4_ijkp1_np1half-dT4_ijk_np1half)
    /(grid.dLocalGridOld[grid.nDPhi][0][0][k+1]+grid.dLocalGridOld[grid.nDPhi][0][0][k])*2.0;
  double dTGrad_km1half_np1half=(dT4_ijk_np1half-dT4_ijkm1_np1half)
    /(grid.dLocalGridOld[grid.nDPhi][0][0][k]+grid.dLocalGridOld[grid.nDPhi][0][0][k-1])*2.0;
  double dGrad_kp1half_np1half=dTGrad_kp1half_np1half/(dKappa_ijkp1half_np1half*dRho_ijkp1half_n);
  double dGrad_km1half_np1half=dTGrad_km1half_np1half/(dKappa_ijkm1half_np1half*dRho_ijkm1half_n);
  double dS6=(dGrad_kp1half_np1half-dGrad_km1half_np1half)/(dRSq_i_np1half
    *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]*grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]
    *grid.dLocalGridOld[grid.nDPhi][0][0][k]);
  /*
  //calculate DUDM_ijk_np1half
  double dDUDM_ijk_np1half=4.0*parameters.dPi*grid.dLocalGridOld[grid.nDenAve][i][0][0]
    *dRSq_i_np1half*((grid.dLocalGridNew[grid.nU][nIInt][j][k]
    -grid.dLocalGridNew[grid.nU0][nIInt][0][0])-(grid.dLocalGridNew[grid.nU][nIInt-1][j][k]
    -grid.dLocalGridNew[grid.nU0][nIInt-1][0][0]))/grid.dLocalGridOld[grid.nDM][i][0][0];
  
  //calculate DVDM_ijk_np1half
  double dDVDM_ijk_np1half=4.0*parameters.dPi*grid.dLocalGridOld[grid.nDenAve][i][0][0]
    *dRSq_i_np1half*(dV_ip1halfjk_np1half-dV_im1halfjk_np1half)
    /grid.dLocalGridOld[grid.nDM][i][0][0];
  
  //calculate DUDTheta_ijk_np1half
  double dDUDTheta_ijk_np1half=((dU_ijp1halfk_np1half-dU0_i_np1half)-(dU_ijm1halfk_np1half
    -dU0_i_np1half))/(dR_i_np1half*grid.dLocalGridOld[grid.nDTheta][0][j][0]);
  
  //calculate DVDTheta_ijk_np1half
  double dDVDTheta_ijk_np1half=(grid.dLocalGridNew[grid.nV][i][nJInt][k]
    -grid.dLocalGridNew[grid.nV][i][nJInt-1][k])/(dR_i_np1half
    *grid.dLocalGridOld[grid.nDTheta][0][j][0]);
  
  //calculate DUDPhi_ijk_np1half
  double dDUDPhi_ijk_np1half=((dU_ijkp1half_np1half-dU0_i_np1half)-(dU_ijkm1half_np1half
    -dU0_i_np1half))/(dR_i_np1half*grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]
    *grid.dLocalGridOld[grid.nDPhi][0][0][k]);
  
  //calculate DVDPhi_ijk_np1half
  double dDVDPhi_ijk_np1half=(dV_ijkp1half_np1half-dV_ijkm1half_np1half)/(dR_i_np1half
    *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]*grid.dLocalGridOld[grid.nDPhi][0][0][k]);
  
  //calculate DWDM_ijk_np1half
  double dDWDM_ijk_np1half=4.0*parameters.dPi*grid.dLocalGridOld[grid.nDenAve][i][0][0]
    *dRSq_i_np1half*(dW_ip1halfjk_np1half-dW_im1halfjk_np1half)
    /grid.dLocalGridOld[grid.nDM][i][0][0];
  
  //calculate DWDTheta_ijk_np1half
  double dDWDTheta_ijk_np1half=(dW_ijp1halfk_np1half-dW_ijm1halfk_np1half)/(dR_i_np1half
    *grid.dLocalGridOld[grid.nDTheta][0][j][0]);
  
  //calculate DWDPhi_ijk_np1half
  double dDWDPhi_ijk_np1half=(grid.dLocalGridNew[grid.nW][i][j][nKInt]
    -grid.dLocalGridNew[grid.nW][i][j][nKInt-1])/(dR_i_np1half
    *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]*grid.dLocalGridOld[grid.nDPhi][0][0][k]);
  
  //cal DivU_ijk_np1half
  double dDivU_ijk_np1half=dDUDM_ijk_np1half+dDVDTheta_ijk_np1half+dDWDPhi_ijk_np1half;
  
  //cal Tau_rr_ijk_np1half
  double dTau_rr_ijk_np1half=2.0*grid.dLocalGridNew[grid.nEddyVisc][i][j][k]*(dDUDM_ijk_np1half
    -0.3333333333333333*dDivU_ijk_np1half);
  
  //calculate Tau_tt_ijk_np1half
  double dTau_tt_ijk_np1half=2.0*grid.dLocalGridNew[grid.nEddyVisc][i][j][k]*(dDVDTheta_ijk_np1half
    -0.333333333333333*dDivU_ijk_np1half);
  
  //calculate dTau_pp_ijk_np1half
  double dTau_pp_ijk_np1half=2.0*grid.dLocalGridNew[grid.nEddyVisc][i][j][k]*(dDWDPhi_ijk_np1half
    -0.3333333333333333*dDivU_ijk_np1half);
  
  //calculate Tau_rt_ijk_np1half
  double dTau_rt_ijk_np1half=grid.dLocalGridNew[grid.nEddyVisc][i][j][k]*(dDVDM_ijk_np1half
    +dDUDTheta_ijk_np1half);
  
  //calculate dTau_rp_ijk_np1half
  double dTau_rp_ijk_np1half=grid.dLocalGridNew[grid.nEddyVisc][i][j][k]*(dDUDPhi_ijk_np1half
    +dDWDM_ijk_np1half);
  
  //calculate dTau_tp_ijk_np1half
  double dTau_tp_ijk_np1half=grid.dLocalGridNew[grid.nEddyVisc][i][j][k]*(dDVDPhi_ijk_np1half
    +dDWDTheta_ijk_np1half);
  
  //eddy viscosity terms
  double dEddyViscosityTerms=(dTau_rr_ijk_np1half*dDUDM_ijk_np1half+dTau_tt_ijk_np1half
    *dDVDTheta_ijk_np1half+dTau_pp_ijk_np1half*dDWDPhi_ijk_np1half+dTau_rt_ijk_np1half
    *(dDUDTheta_ijk_np1half+dDVDM_ijk_np1half)+dTau_rp_ijk_np1half*(dDUDPhi_ijk_np1half
    +dDWDM_ijk_np1half)+dTau_tp_ijk_np1half*(dDVDPhi_ijk_np1half+dDWDTheta_ijk_np1half))
    /grid.dLocalGridOld[grid.nD][i][j][k];*/
  
  //calculate dT1
  double dEGrad_ip1halfjk_np1half=dR4_ip1half_np1half*dEddyVisc_ip1halfjk_n*dRhoAve_ip1half_n
    *(dE_ip1jk_np1half-dE_ijk_np1half)/(dRho_ip1halfjk_n*dDM_ip1half);
  double dEGrad_im1halfjk_np1half=dR4_im1half_np1half*dEddyVisc_im1halfjk_n*dRhoAve_im1half_n
    *(dE_ijk_np1half-dE_im1jk_np1half)/(dRho_im1halfjk_n*dDM_im1half);
  double dT1=16.0*dPiSq*grid.dLocalGridOld[grid.nDenAve][i][0][0]*(dEGrad_ip1halfjk_np1half
    -dEGrad_im1halfjk_np1half)/grid.dLocalGridOld[grid.nDM][i][0][0];
  
  //calculate dT2
  double dEGrad_ijp1halfk_np1half=dEddyVisc_ijp1halfk_n
    *grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt][0]
    *(dE_ijp1k_np1half-dE_ijk_np1half)/(dRho_ijp1halfk_n*dR_i_np1half*dDelTheta_jp1half);
  double dEGrad_ijm1halfk_np1half=dEddyVisc_ijm1halfk_n
    *grid.dLocalGridOld[grid.nSinThetaIJp1halfK][0][nJInt-1][0]
    *(dE_ijk_np1half-dE_ijm1k_np1half)/(dRho_ijm1halfk_n*dR_i_np1half*dDelTheta_jm1half);
  double dT2=(dEGrad_ijp1halfk_np1half-dEGrad_ijm1halfk_np1half)/(dR_i_np1half
    *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]
    *grid.dLocalGridOld[grid.nDTheta][0][j][0]);
  
  //calculate dT3
  double dEGrad_ijkp1half_np1half=dEddyVisc_ijkp1half_n*(dE_ijkp1_np1half-dE_ijk_np1half)
    /(dRho_ijkp1half_n*grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]*dR_i_np1half*dDelPhi_kp1half);
  double dEGrad_ijkm1half_np1half=dEddyVisc_ijkm1half_n*(dE_ijk_np1half-dE_ijm1k_np1half)
    /(dRho_ijkm1half_n*grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]*dR_i_np1half*dDelPhi_km1half);
  double dT3=(dEGrad_ijkp1half_np1half-dEGrad_ijkm1half_np1half)/(dR_i_np1half
    *grid.dLocalGridOld[grid.nSinThetaIJK][0][j][0]
    *grid.dLocalGridOld[grid.nDPhi][0][0][k]);
  
  //eddy viscosity terms
  double dEddyViscosityTerms=(dT1+dT2+dT3)/parameters.dPrt;
  
  //calculate energy equation discrepancy
  return (dE_ijk_np1-grid.dLocalGridOld[grid.nE][i][j][k])/time.dDeltat_n
    +4.0*parameters.dPi*grid.dLocalGridOld[grid.nDenAve][i][0][0]*(dA1+dS1)+dA2+dS2+dA3+dS3
    -4.0*parameters.dSigma/(3.0*grid.dLocalGridOld[grid.nD][i][j][k])*(dS4+dS5+dS6)
    -dEddyViscosityTerms;
}
double dEOS_GL(double dRho, double dE, Parameters parameters){
  return dRho*(parameters.dGamma-1.0)*dE;
}
void initDonorFracAndMaxConVel_R_GL(Grid &grid, Parameters &parameters){
  
  int nEndCalc=std::max(grid.nEndGhostUpdateExplicit[grid.nD][0][0],grid.nEndUpdateExplicit[grid.nD][0]);
  int i;
  int j;
  int k;
  int nIInt;
  double dC;
  double dTest_ConVelOverSoundSpeed_R;
  double dTest_ConVelOverSoundSpeed=0.0;
  double dTest_ConVel_R;
  double dTest_ConVel=0.0;
  
  for(i=grid.nStartUpdateExplicit[grid.nD][0];i<nEndCalc;i++){
    
    //calculate i for interface centered quantities
    nIInt=i+grid.nCenIntOffset[0];
    
    for(j=grid.nStartUpdateExplicit[grid.nD][1];j<grid.nEndUpdateExplicit[grid.nD][1];j++){
      for(k=grid.nStartUpdateExplicit[grid.nD][2];k<grid.nEndUpdateExplicit[grid.nD][2];k++){
        
        dC=sqrt(parameters.dGamma*(grid.dLocalGridOld[grid.nP][i][j][k]
          +grid.dLocalGridOld[grid.nQ0][i][j][k])/grid.dLocalGridOld[grid.nD][i][j][k]);
        dTest_ConVelOverSoundSpeed_R=fabs(grid.dLocalGridOld[grid.nU][nIInt][j][k]
          -grid.dLocalGridOld[grid.nU0][nIInt][0][0])/dC;
        dTest_ConVel_R=fabs(grid.dLocalGridOld[grid.nU][nIInt][j][k]
          -grid.dLocalGridOld[grid.nU0][nIInt][0][0]);
        
        //keep largest convective velocity over sound speed
        if(dTest_ConVelOverSoundSpeed_R>dTest_ConVelOverSoundSpeed){
          dTest_ConVelOverSoundSpeed=dTest_ConVelOverSoundSpeed_R;
        }
        
        //keep largest convective velocity
        if(dTest_ConVel_R>dTest_ConVel){
          dTest_ConVel=dTest_ConVel_R;
        }
      }
    }
  }
  
  //keep largest convective velocity
  double dTest_ConVel2;
  MPI::COMM_WORLD.Allreduce(&dTest_ConVel,&dTest_ConVel2,1,MPI::DOUBLE,MPI_MAX);
  parameters.dMaxConvectiveVelocity=dTest_ConVel2;
  
  //set donnor fraction
  double dTest_ConVelOverSoundSpeed2;
  MPI::COMM_WORLD.Allreduce(&dTest_ConVelOverSoundSpeed,&dTest_ConVelOverSoundSpeed2,1
    ,MPI::DOUBLE,MPI_MAX);
  parameters.dMaxConvectiveVelocity_c=dTest_ConVelOverSoundSpeed2;
  if(dTest_ConVelOverSoundSpeed2>1.0){
    parameters.dDonorFrac=1.0;
  }
  else if(dTest_ConVelOverSoundSpeed2<0.1){
    parameters.dDonorFrac=0.1;
  }
  else{
    parameters.dDonorFrac=dTest_ConVelOverSoundSpeed2;
  }
}
void initDonorFracAndMaxConVel_R_TEOS(Grid &grid, Parameters &parameters){
  
  int nEndCalc=std::max(grid.nEndGhostUpdateExplicit[grid.nD][0][0]
    ,grid.nEndUpdateExplicit[grid.nD][0]);
  int i;
  int j;
  int k;
  int nIInt;
  double dC;
  double dTest_ConVelOverSoundSpeed_R;
  double dTest_ConVelOverSoundSpeed=0.0;
  double dTest_ConVel_R;
  double dTest_ConVel=0.0;
  
  for(i=grid.nStartUpdateExplicit[grid.nD][0];i<nEndCalc;i++){
    
    //calculate i for interface centered quantities
    nIInt=i+grid.nCenIntOffset[0];
    
    for(j=grid.nStartUpdateExplicit[grid.nD][1];j<grid.nEndUpdateExplicit[grid.nD][1];j++){
      for(k=grid.nStartUpdateExplicit[grid.nD][2];k<grid.nEndUpdateExplicit[grid.nD][2];k++){
        
        dC=sqrt(grid.dLocalGridOld[grid.nGamma][i][j][k]*(grid.dLocalGridOld[grid.nP][i][j][k]
          +grid.dLocalGridOld[grid.nQ0][i][j][k])/grid.dLocalGridOld[grid.nD][i][j][k]);
        dTest_ConVelOverSoundSpeed_R=fabs(grid.dLocalGridOld[grid.nU][nIInt][j][k]
          -grid.dLocalGridOld[grid.nU0][nIInt][0][0])/dC;
        dTest_ConVel_R=fabs(grid.dLocalGridOld[grid.nU][nIInt][j][k]
          -grid.dLocalGridOld[grid.nU0][nIInt][0][0]);
        
        //keep largest convective velocity over sound speed
        if(dTest_ConVelOverSoundSpeed_R>dTest_ConVelOverSoundSpeed){
          dTest_ConVelOverSoundSpeed=dTest_ConVelOverSoundSpeed_R;
        }
        
        //keep largest convective velocity
        if(dTest_ConVel_R>dTest_ConVel){
          dTest_ConVel=dTest_ConVel_R;
        }
      }
    }
  }
  
  //keep largest convective velocity
  double dTest_ConVel2;
  MPI::COMM_WORLD.Allreduce(&dTest_ConVel,&dTest_ConVel2,1,MPI::DOUBLE,MPI_MAX);
  parameters.dMaxConvectiveVelocity=dTest_ConVel2;
  
  //set donnor fraction
  double dTest_ConVelOverSoundSpeed2;
  MPI::COMM_WORLD.Allreduce(&dTest_ConVelOverSoundSpeed,&dTest_ConVelOverSoundSpeed2,1
    ,MPI::DOUBLE,MPI_MAX);
  parameters.dMaxConvectiveVelocity_c=dTest_ConVelOverSoundSpeed2;
  if(dTest_ConVelOverSoundSpeed2>1.0){
    parameters.dDonorFrac=1.0;
  }
  else if(dTest_ConVelOverSoundSpeed2<0.1){
    parameters.dDonorFrac=0.1;
  }
  else{
    parameters.dDonorFrac=dTest_ConVelOverSoundSpeed2;
  }
}
void initDonorFracAndMaxConVel_RT_GL(Grid &grid, Parameters &parameters){
  int nEndCalc=std::max(grid.nEndGhostUpdateExplicit[grid.nD][0][0]
    ,grid.nEndUpdateExplicit[grid.nD][0]);
  int i;
  int j;
  int k;
  int nIInt;
  int nJInt;
  double dC;
  double dTest_ConVelOverSoundSpeed_R;
  double dTest_ConVelOverSoundSpeed_T;
  double dTest_ConVelOverSoundSpeed=0.0;
  double dTest_ConVel_R;
  double dTest_ConVel_T;
  double dTest_ConVel=0.0;
  
  for(i=grid.nStartUpdateExplicit[grid.nD][0];i<nEndCalc;i++){
    
    //calculate i for interface centered quantities
    nIInt=i+grid.nCenIntOffset[0];
    
    for(j=grid.nStartUpdateExplicit[grid.nD][1];j<grid.nEndUpdateExplicit[grid.nD][1];j++){
      nJInt=j+grid.nCenIntOffset[1];
      for(k=grid.nStartUpdateExplicit[grid.nD][2];k<grid.nEndUpdateExplicit[grid.nD][2];k++){
        dC=sqrt(parameters.dGamma*(grid.dLocalGridOld[grid.nP][i][j][k]
          +grid.dLocalGridOld[grid.nQ0][i][j][k]+grid.dLocalGridOld[grid.nQ1][i][j][k])
          /grid.dLocalGridOld[grid.nD][i][j][k]);
        dTest_ConVelOverSoundSpeed_R=fabs(grid.dLocalGridOld[grid.nU][nIInt][j][k]
          -grid.dLocalGridOld[grid.nU0][nIInt][0][0])/dC;
        dTest_ConVelOverSoundSpeed_T=fabs(grid.dLocalGridOld[grid.nV][i][nJInt][k])/dC;
        dTest_ConVel_R=fabs(grid.dLocalGridOld[grid.nU][nIInt][j][k]
          -grid.dLocalGridOld[grid.nU0][nIInt][0][0]);
        dTest_ConVel_T=fabs(grid.dLocalGridOld[grid.nV][i][nJInt][k]);
        
        //keep largest convective velocity over sound speed
        if(dTest_ConVelOverSoundSpeed_R>dTest_ConVelOverSoundSpeed){
          dTest_ConVelOverSoundSpeed=dTest_ConVelOverSoundSpeed_R;
        }
        if(dTest_ConVelOverSoundSpeed_T>dTest_ConVelOverSoundSpeed){
          dTest_ConVelOverSoundSpeed=dTest_ConVelOverSoundSpeed_T;
        }
        
        //keep largest convective velocity
        if(dTest_ConVel_R>dTest_ConVel){
          dTest_ConVel=dTest_ConVel_R;
        }
        if(dTest_ConVel_T>dTest_ConVel){
          dTest_ConVel=dTest_ConVel_T;
        }
      }
    }
  }
  
  //keep largest convective velocity
  double dTest_ConVel2;
  MPI::COMM_WORLD.Allreduce(&dTest_ConVel,&dTest_ConVel2,1,MPI::DOUBLE,MPI_MAX);
  parameters.dMaxConvectiveVelocity=dTest_ConVel2;
  
  //set donnor fraction
  double dTest_ConVelOverSoundSpeed2;
  MPI::COMM_WORLD.Allreduce(&dTest_ConVelOverSoundSpeed,&dTest_ConVelOverSoundSpeed2,1
    ,MPI::DOUBLE,MPI_MAX);
  parameters.dMaxConvectiveVelocity_c=dTest_ConVelOverSoundSpeed2;
  if(dTest_ConVelOverSoundSpeed2>1.0){
    parameters.dDonorFrac=1.0;
  }
  else if(dTest_ConVelOverSoundSpeed2<0.1){
    parameters.dDonorFrac=0.1;
  }
  else{
    parameters.dDonorFrac=dTest_ConVelOverSoundSpeed2;
  }
}
void initDonorFracAndMaxConVel_RT_TEOS(Grid &grid, Parameters &parameters){
  int nEndCalc=std::max(grid.nEndGhostUpdateExplicit[grid.nD][0][0]
    ,grid.nEndUpdateExplicit[grid.nD][0]);
  int i;
  int j;
  int k;
  int nIInt;
  int nJInt;
  double dC;
  double dTest_ConVelOverSoundSpeed_R;
  double dTest_ConVelOverSoundSpeed_T;
  double dTest_ConVelOverSoundSpeed=0.0;
  double dTest_ConVel_R;
  double dTest_ConVel_T;
  double dTest_ConVel=0.0;
  
  for(i=grid.nStartUpdateExplicit[grid.nD][0];i<nEndCalc;i++){
    
    //calculate i for interface centered quantities
    nIInt=i+grid.nCenIntOffset[0];
    
    for(j=grid.nStartUpdateExplicit[grid.nD][1];j<grid.nEndUpdateExplicit[grid.nD][1];j++){
      nJInt=j+grid.nCenIntOffset[1];
      for(k=grid.nStartUpdateExplicit[grid.nD][2];k<grid.nEndUpdateExplicit[grid.nD][2];k++){
        
        dC=sqrt(grid.dLocalGridOld[grid.nGamma][i][j][k]
          *(grid.dLocalGridOld[grid.nP][i][j][k]+grid.dLocalGridOld[grid.nQ0][i][j][k]
          +grid.dLocalGridOld[grid.nQ1][i][j][k])
          /grid.dLocalGridOld[grid.nD][i][j][k]);
        
        dTest_ConVelOverSoundSpeed_R=fabs(grid.dLocalGridOld[grid.nU][nIInt][j][k]
          -grid.dLocalGridOld[grid.nU0][nIInt][0][0])/dC;
        dTest_ConVelOverSoundSpeed_T=fabs(grid.dLocalGridOld[grid.nV][i][nJInt][k])/dC;
        dTest_ConVel_R=fabs(grid.dLocalGridOld[grid.nU][nIInt][j][k]
          -grid.dLocalGridOld[grid.nU0][nIInt][0][0]);
        dTest_ConVel_T=fabs(grid.dLocalGridOld[grid.nV][i][nJInt][k]);
        
        //keep largest convective velocity over sound speed
        if(dTest_ConVelOverSoundSpeed_R>dTest_ConVelOverSoundSpeed){
          dTest_ConVelOverSoundSpeed=dTest_ConVelOverSoundSpeed_R;
        }
        if(dTest_ConVelOverSoundSpeed_T>dTest_ConVelOverSoundSpeed){
          dTest_ConVelOverSoundSpeed=dTest_ConVelOverSoundSpeed_T;
        }
        
        //keep largest convective velocity
        if(dTest_ConVel_R>dTest_ConVel){
          dTest_ConVel=dTest_ConVel_R;
        }
        if(dTest_ConVel_T>dTest_ConVel){
          dTest_ConVel=dTest_ConVel_T;
        }
      }
    }
  }
  
  //keep largest convective velocity
  double dTest_ConVel2;
  MPI::COMM_WORLD.Allreduce(&dTest_ConVel,&dTest_ConVel2,1,MPI::DOUBLE,MPI_MAX);
  parameters.dMaxConvectiveVelocity=dTest_ConVel2;
  
  //set donnor fraction
  double dTest_ConVelOverSoundSpeed2;
  MPI::COMM_WORLD.Allreduce(&dTest_ConVelOverSoundSpeed,&dTest_ConVelOverSoundSpeed2,1
    ,MPI::DOUBLE,MPI_MAX);
  parameters.dMaxConvectiveVelocity_c=dTest_ConVelOverSoundSpeed2;
  if(dTest_ConVelOverSoundSpeed2>1.0){
    parameters.dDonorFrac=1.0;
  }
  else if(dTest_ConVelOverSoundSpeed2<0.1){
    parameters.dDonorFrac=0.1;
  }
  else{
    parameters.dDonorFrac=dTest_ConVelOverSoundSpeed2;
  }
}
void initDonorFracAndMaxConVel_RTP_GL(Grid &grid, Parameters &parameters){
  int nEndCalc=std::max(grid.nEndGhostUpdateExplicit[grid.nD][0][0]
    ,grid.nEndUpdateExplicit[grid.nD][0]);
  int i;
  int j;
  int k;
  int nIInt;
  int nJInt;
  int nKInt;
  double dC;
  double dTest_ConVelOverSoundSpeed_R;
  double dTest_ConVelOverSoundSpeed_T;
  double dTest_ConVelOverSoundSpeed_P;
  double dTest_ConVelOverSoundSpeed=0.0;
  double dTest_ConVel_R;
  double dTest_ConVel_T;
  double dTest_ConVel_P;
  double dTest_ConVel=0.0;
  
  for(i=grid.nStartUpdateExplicit[grid.nD][0];i<nEndCalc;i++){
    
    //calculate i for interface centered quantities
    nIInt=i+grid.nCenIntOffset[0];
    
    for(j=grid.nStartUpdateExplicit[grid.nD][1];j<grid.nEndUpdateExplicit[grid.nD][1];j++){
      nJInt=j+grid.nCenIntOffset[1];
      for(k=grid.nStartUpdateExplicit[grid.nD][2];k<grid.nEndUpdateExplicit[grid.nD][2];k++){
        nKInt=k+grid.nCenIntOffset[2];
        dC=sqrt(parameters.dGamma
          *(grid.dLocalGridOld[grid.nP][i][j][k]+grid.dLocalGridOld[grid.nQ0][i][j][k]
          +grid.dLocalGridOld[grid.nQ1][i][j][k]+grid.dLocalGridOld[grid.nQ2][i][j][k])
          /grid.dLocalGridOld[grid.nD][i][j][k]);
        
        dTest_ConVelOverSoundSpeed_R=fabs(grid.dLocalGridOld[grid.nU][nIInt][j][k]
          -grid.dLocalGridOld[grid.nU0][nIInt][0][0])/dC;
        dTest_ConVelOverSoundSpeed_T=fabs(grid.dLocalGridOld[grid.nV][i][nJInt][k])/dC;
        dTest_ConVelOverSoundSpeed_P=fabs(grid.dLocalGridOld[grid.nW][i][j][nKInt])/dC;
        dTest_ConVel_R=fabs(grid.dLocalGridOld[grid.nU][nIInt][j][k]
          -grid.dLocalGridOld[grid.nU0][nIInt][0][0]);
        dTest_ConVel_T=fabs(grid.dLocalGridOld[grid.nV][i][nJInt][k]);
        dTest_ConVel_P=fabs(grid.dLocalGridOld[grid.nW][i][j][nKInt]);
        
        //keep largest convective velocity over sound speed
        if(dTest_ConVelOverSoundSpeed_R>dTest_ConVelOverSoundSpeed){
          dTest_ConVelOverSoundSpeed=dTest_ConVelOverSoundSpeed_R;
        }
        if(dTest_ConVelOverSoundSpeed_T>dTest_ConVelOverSoundSpeed){
          dTest_ConVelOverSoundSpeed=dTest_ConVelOverSoundSpeed_T;
        }
        if(dTest_ConVelOverSoundSpeed_P>dTest_ConVelOverSoundSpeed){
          dTest_ConVelOverSoundSpeed=dTest_ConVelOverSoundSpeed_P;
        }
        
        //keep largest convective velocity
        if(dTest_ConVel_R>dTest_ConVel){
          dTest_ConVel=dTest_ConVel_R;
        }
        if(dTest_ConVel_T>dTest_ConVel){
          dTest_ConVel=dTest_ConVel_T;
        }
        if(dTest_ConVel_P>dTest_ConVel){
          dTest_ConVel=dTest_ConVel_P;
        }
      }
    }
  }
  
  //set donnor fraction
  double dTest_ConVelOverSoundSpeed2;
  MPI::COMM_WORLD.Allreduce(&dTest_ConVelOverSoundSpeed,&dTest_ConVelOverSoundSpeed2,1
    ,MPI::DOUBLE,MPI_MAX);
  if(dTest_ConVelOverSoundSpeed2>1.0){
    parameters.dDonorFrac=1.0;
  }
  else if(dTest_ConVelOverSoundSpeed2<0.1){
    parameters.dDonorFrac=0.1;
  }
  else{
    parameters.dDonorFrac=dTest_ConVelOverSoundSpeed2;
  }
  
  //keep largest convective velocity
  double dTest_ConVel2;
  MPI::COMM_WORLD.Allreduce(&dTest_ConVel,&dTest_ConVel2,1,MPI::DOUBLE,MPI_MAX);
  parameters.dMaxConvectiveVelocity_c=dTest_ConVelOverSoundSpeed2;
  parameters.dMaxConvectiveVelocity=dTest_ConVel2;
}
void initDonorFracAndMaxConVel_RTP_TEOS(Grid &grid, Parameters &parameters){
  int nEndCalc=std::max(grid.nEndGhostUpdateExplicit[grid.nD][0][0]
    ,grid.nEndUpdateExplicit[grid.nD][0]);
  int i;
  int j;
  int k;
  int nIInt;
  int nJInt;
  int nKInt;
  double dC;
  double dTest_ConVelOverSoundSpeed_R;
  double dTest_ConVelOverSoundSpeed_T;
  double dTest_ConVelOverSoundSpeed_P;
  double dTest_ConVelOverSoundSpeed=0.0;
  double dTest_ConVel_R;
  double dTest_ConVel_T;
  double dTest_ConVel_P;
  double dTest_ConVel=0.0;
  
  for(i=grid.nStartUpdateExplicit[grid.nD][0];i<nEndCalc;i++){
    
    //calculate i for interface centered quantities
    nIInt=i+grid.nCenIntOffset[0];
    
    for(j=grid.nStartUpdateExplicit[grid.nD][1];j<grid.nEndUpdateExplicit[grid.nD][1];j++){
      nJInt=j+grid.nCenIntOffset[1];
      for(k=grid.nStartUpdateExplicit[grid.nD][2];k<grid.nEndUpdateExplicit[grid.nD][2];k++){
        nKInt=k+grid.nCenIntOffset[2];
        dC=sqrt(grid.dLocalGridOld[grid.nGamma][i][j][k]
          *(grid.dLocalGridOld[grid.nP][i][j][k]+grid.dLocalGridOld[grid.nQ0][i][j][k]
          +grid.dLocalGridOld[grid.nQ1][i][j][k]+grid.dLocalGridOld[grid.nQ2][i][j][k])
          /grid.dLocalGridOld[grid.nD][i][j][k]);
        
        dTest_ConVelOverSoundSpeed_R=fabs(grid.dLocalGridOld[grid.nU][nIInt][j][k]
          -grid.dLocalGridOld[grid.nU0][nIInt][0][0])/dC;
        dTest_ConVelOverSoundSpeed_T=fabs(grid.dLocalGridOld[grid.nV][i][nJInt][k])/dC;
        dTest_ConVelOverSoundSpeed_P=fabs(grid.dLocalGridOld[grid.nW][i][j][nKInt])/dC;
        dTest_ConVel_R=fabs(grid.dLocalGridOld[grid.nU][nIInt][j][k]
          -grid.dLocalGridOld[grid.nU0][nIInt][0][0]);
        dTest_ConVel_T=fabs(grid.dLocalGridOld[grid.nV][i][nJInt][k]);
        dTest_ConVel_P=fabs(grid.dLocalGridOld[grid.nW][i][j][nKInt]);
        
        //keep largest convective velocity over sound speed
        if(dTest_ConVelOverSoundSpeed_R>dTest_ConVelOverSoundSpeed){
          dTest_ConVelOverSoundSpeed=dTest_ConVelOverSoundSpeed_R;
        }
        if(dTest_ConVelOverSoundSpeed_T>dTest_ConVelOverSoundSpeed){
          dTest_ConVelOverSoundSpeed=dTest_ConVelOverSoundSpeed_T;
        }
        if(dTest_ConVelOverSoundSpeed_P>dTest_ConVelOverSoundSpeed){
          dTest_ConVelOverSoundSpeed=dTest_ConVelOverSoundSpeed_P;
        }
        
        //keep largest convective velocity
        if(dTest_ConVel_R>dTest_ConVel){
          dTest_ConVel=dTest_ConVel_R;
        }
        if(dTest_ConVel_T>dTest_ConVel){
          dTest_ConVel=dTest_ConVel_T;
        }
        if(dTest_ConVel_P>dTest_ConVel){
          dTest_ConVel=dTest_ConVel_P;
        }
      }
    }
  }
  
  //set donnor fraction
  double dTest_ConVelOverSoundSpeed2;
  MPI::COMM_WORLD.Allreduce(&dTest_ConVelOverSoundSpeed,&dTest_ConVelOverSoundSpeed2,1
    ,MPI::DOUBLE,MPI_MAX);
  if(dTest_ConVelOverSoundSpeed2>1.0){
    parameters.dDonorFrac=1.0;
  }
  else if(dTest_ConVelOverSoundSpeed2<0.1){
    parameters.dDonorFrac=0.1;
  }
  else{
    parameters.dDonorFrac=dTest_ConVelOverSoundSpeed2;
  }
  
  //keep largest convective velocity
  double dTest_ConVel2;
  MPI::COMM_WORLD.Allreduce(&dTest_ConVel,&dTest_ConVel2,1,MPI::DOUBLE,MPI_MAX);
  parameters.dMaxConvectiveVelocity_c=dTest_ConVelOverSoundSpeed2;
  parameters.dMaxConvectiveVelocity=dTest_ConVel2;
}
