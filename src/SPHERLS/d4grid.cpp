#include "d4grid.hpp"
#include<string>
#include<iostream>


int NormalGrid::getDimZ( [[maybe_unused]] int i)
{
    return ndimZ;
}

/*********RadialGrid and SurfaceElement*********/


int SurfElement::getDimZ()
{
    return ndimZ;
}

int RadialGrid::getDimZ(int i)
{
    return dGrid[i].getDimZ();
}



d3Grid* RadialGrid::buildIt(int nX, int nY, int nZ, int nGhostX, int nGhostY, int nGhostZ)
{
    RadialGrid* newGrid = new RadialGrid;

    newGrid->dGrid = std::vector<SurfElement>(nX+nGhostX);
    std::fill(newGrid->dGrid.begin(),std::next(newGrid->dGrid.begin(),nX),
             SurfElement(nY,nZ));
    if(nGhostX != 0){
        std::fill(std::next(newGrid->dGrid.begin(),nX),newGrid->dGrid.end(),
                SurfElement(nGhostY,nGhostZ));    
    }
    return newGrid;
}


/*********************** d4grid *********************/

d4grid::~d4grid()
{
    for (auto &i : this->pGridObj)
    {
        delete i;
    }
}

d4grid& d4grid::operator=(const d4grid& other)
/*rule of three for the time being */
{
    if (this==&other)
    {
        return *this;
    }
    this->pGridObj = other.pGridObj;
    this->nDimGridObj = other.nDimGridObj;
    this->ndimensions = other.ndimensions;
    return *this;
}


d4grid* d4grid::buildNormal(int nVarNum, int** nDimensions)
{
    d4grid* newObj = new d4grid;

    newObj->nDimGridObj=nVarNum;

    newObj->ndimensions=std::vector<int>(nVarNum*3);

    std::generate(newObj->ndimensions.begin(),std::end(newObj->ndimensions),
    [i=0, &nDimensions]() mutable { int j=i/3; int k=i%3;i++; return nDimensions[j][k];});

    newObj->pGridObj = std::vector<d3Grid*>(nVarNum);
    std::generate(newObj->pGridObj.begin(),newObj->pGridObj.end(),
    [i=0, &nDimensions]() mutable {int* nDims=nDimensions[i++];
      return new NormalGrid(nDims[0],nDims[1],nDims[2]);} );

    return newObj;

}


d4grid* d4grid::buildRadial(int nVarNum, int** nDimensions, IntVec nSizeX, IntVec nSizeY, IntVec nSizeZ)
{
    d4grid* newObj = new d4grid;

    newObj -> nDimGridObj= nVarNum;
    
    newObj->ndimensions=std::vector<int>(nVarNum*3);

    std::generate(newObj->ndimensions.begin(),std::end(newObj->ndimensions),
    [i=0, &nDimensions]() mutable { int j=i/3; int k=i%3;i++; return nDimensions[j][k];});

    newObj->pGridObj = std::vector<d3Grid*>(nVarNum);
    std::generate(newObj->pGridObj.begin(),newObj->pGridObj.end(),
    [i=0, &nDimensions,&nSizeX, &nSizeY, &nSizeZ]() mutable {
        int* nDims=nDimensions[i++];
        return RadialGrid::buildIt(nDims[0],nDims[1],nDims[2],
            nSizeX[i-1],nSizeY[i-1],nSizeZ[i-1]);
        });
    return newObj;
}

void d4grid::loadline(std::vector<double> line,int i, int j, int k)
{
    for(int l=0; l< line.size();l++)
    {
        this->getElement(i,j,k,l) = line[l];
    }
}
double* d4grid::writeline(int i, int j, int k)
{
    d3Grid* pVarElement = this->pGridObj[i];
    int ndimZ=pVarElement->getDimZ(j);
    double* line = new double[ndimZ];
    
    for (int l=0; l<ndimZ;l++)
        {
            line[l] = pVarElement->getElement(j,k,l);
        }
    return line;
}
