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
    dTest_ConVelOverSoundSpeed=0.0;
    
    for(j=grid.nStartUpdateExplicit[grid.nD][1];j<grid.nEndUpdateExplicit[grid.nD][1];j++){
      for(k=grid.nStartUpdateExplicit[grid.nD][2];k<grid.nEndUpdateExplicit[grid.nD][2];k++){
        
        dC=sqrt(grid.dLocalGridNew[grid.nGamma][i][j][k]*(grid.dLocalGridNew[grid.nP][i][j][k]
          +grid.dLocalGridNew[grid.nQ0][i][j][k])/grid.dLocalGridNew[grid.nD][i][j][k]);
        dUmdU0_ijk_nm1half=((grid.dLocalGridNew[grid.nU][nIInt][j][k]
          -grid.dLocalGridNew[grid.nU0][nIInt][0][0])+(grid.dLocalGridNew[grid.nU][nIInt-1][j][k]
          -grid.dLocalGridNew[grid.nU0][nIInt-1][0][0]))*0.5;
        dVelToSetTimeStep=sqrt(dC*dC+dUmdU0_ijk_nm1half*dUmdU0_ijk_nm1half);
        dTTestR=dDelR/dVelToSetTimeStep;
        
        dTest_ConVel_R=fabs(grid.dLocalGridNew[grid.nU][nIInt][j][k]
          -grid.dLocalGridNew[grid.nU0][nIInt][0][0]);
        
        dTest_ConVelOverSoundSpeed_R=dTest_ConVel_R/dC;
        
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
    
    //set donnor fraction
    double dTest_ConVelOverSoundSpeed2=parameters.dDonorCellMultiplier*dTest_ConVelOverSoundSpeed;
    if(dTest_ConVelOverSoundSpeed2>1.0){
      grid.dLocalGridNew[grid.nDonorCellFrac][i][0][0]=1.0;
    }
    else if(dTest_ConVelOverSoundSpeed2<parameters.dDonorCellMin){
      grid.dLocalGridNew[grid.nDonorCellFrac][i][0][0]=parameters.dDonorCellMin;
    }
    else{
      grid.dLocalGridNew[grid.nDonorCellFrac][i][0][0]=dTest_ConVelOverSoundSpeed2;
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
}
