#include "d4grid.hpp"


inline int d4grid::calcindex(int i, int j, int k, int l)
{
    return i*nBlockSize + j*nSurfaceSize + k* nRowSize + l;
}

double& d4grid::getElement(int i, int j, int k, int l)
{
    return this->pGridobj[this->calcindex(i,j,k,l)];
}

double* d4grid::getElement(int i, int j, int k)
{
    double* output = &pGridobj[calcindex(i,j,k,0)];
    return output;
}


d4grid* d4grid::buildIt(int nVarNum, int nRadialElementNum, int nThetaNum, int nPhiNum)
{
    d4grid* d4NewGrid = new d4grid();

    d4NewGrid->ndim1=nVarNum;
    d4NewGrid->ndim2=nRadialElementNum;
    d4NewGrid->ndim3=nThetaNum;
    d4NewGrid->ndim4=nPhiNum;
    d4NewGrid->nBlockSize=nRadialElementNum*nThetaNum*nPhiNum;
    d4NewGrid->nSurfaceSize=nThetaNum*nPhiNum;
    d4NewGrid->nRowSize=nPhiNum;
    d4NewGrid->pGridobj= new double[nVarNum*nRadialElementNum*nThetaNum*nPhiNum];

    return d4NewGrid;
}

d4grid::~d4grid()
{
    delete this->pGridobj;
}

d4grid* d4grid::buildRadial(int nVarNum, int nRadialElementNum, int nThetaNum, int nPhiNum, int nSizeX2, int nSizeY2, int nSizeZ2)
{
    d4grid* d4NewGrid = new d4gridRadial();
    d4NewGrid->ndim1=nVarNum;
    d4NewGrid->ndim2=nRadialElementNum;
    d4NewGrid->ndim3=nThetaNum;
    d4NewGrid->ndim4=nPhiNum;
    d4NewGrid->nBlockSize=nRadialElementNum*nThetaNum*nPhiNum;
    d4NewGrid->nSurfaceSize=nThetaNum*nPhiNum;
    d4NewGrid->nRowSize=nPhiNum;
    d4NewGrid->pGridobj= new double[nVarNum*nRadialElementNum*nThetaNum*nPhiNum];
    static_cast<d4gridRadial*>(d4NewGrid)->buildGhost(nSizeX2,nSizeY2,nSizeZ2);

    return d4NewGrid;

}

void d4gridRadial::buildGhost(int nSizeX2, int nSizeY2, int nSizeZ2)
{
    this->ghostGrid=d4grid::buildIt(this->ndim1,nSizeX2,nSizeY2,nSizeZ2);
}

double& d4gridRadial::getElement(int i, int j, int k, int l)
{
    if( j >= ndim2)
    {
        return this->ghostGrid->getElement(i, j-ndim2, k ,l);
    }
    else
    {
        return d4grid::getElement(i,j,k,l);
    }
}

d4gridRadial::~d4gridRadial()
{
    delete this->ghostGrid;
}

double* d4gridRadial::getElement(int i, int j, int k)
{
    if( j >= ndim2)
    {
        return this->ghostGrid->getElement(i, j-ndim2, k);
    }
    else
    {
        return d4grid::getElement(i,j,k);
    }
}
