#include "d4grid.hpp"


inline int d4grid::calcindex(int i, int j, int k, int l)
{
    return i*nBlockSize + j*nSurfaceSize + k* nRowSize + l;
}

double& d4grid::getElement(int i, int j, int k, int l)
{
    return this->pGridobj[this->calcindex(i,j,k,l)];
}

d4grid d4grid::buildIt(int nVarNum, int nRadialElementNum, int nThetaNum, int nPhiNum)
{
    d4grid d4NewGrid =d4grid();

    d4NewGrid.ndim1=nVarNum;
    d4NewGrid.ndim2=nRadialElementNum;
    d4NewGrid.ndim3=nThetaNum;
    d4NewGrid.ndim4=nPhiNum;
    d4NewGrid.nBlockSize=nRadialElementNum*nThetaNum*nPhiNum;
    d4NewGrid.nSurfaceSize=nThetaNum*nPhiNum;
    d4NewGrid.nRowSize=nPhiNum;
    d4NewGrid.pGridobj= new double[nVarNum*nRadialElementNum*nThetaNum*nPhiNum];

    return d4NewGrid;
}