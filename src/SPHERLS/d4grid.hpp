#ifndef D4GRID
#define D4GRID

#include <vector>
#include <algorithm>
#include "exception2.h"

typedef std::vector<int> IntVec; 


class d3Grid
{
    public:
    
    virtual double& getElement(const int i, const  int j,const  int k) = 0;
    virtual int getDimZ(int i) = 0;
    
};


class NormalGrid : public d3Grid
{
    std::vector<double> dGrid;
    int ndimYZ;
    int ndimZ;
    //int ndimZ;
    inline size_t calcindex(int i, int j, int k)
    {
        return static_cast<size_t>(i*ndimYZ+j*ndimZ+k);
    }

    public:
    NormalGrid() : d3Grid() {}
    NormalGrid(int nX, int nY, int nZ) : d3Grid(), dGrid(std::vector<double>(nX*nY*nZ)), ndimYZ(nY*nZ),ndimZ(nZ) {}
    inline double& getElement(const int i, const int j, const int k)
    {
    return this->dGrid[calcindex(i,j,k)];
    }
    int getDimZ(int i);
};

class SurfElement 
{
    std::vector<double> dGrid;
    int ndimZ;
public:
    SurfElement() {}
    SurfElement(int nY, int nZ) : dGrid(std::vector<double>(nY*nZ)), ndimZ(nZ) {}
    inline double& getElement(const int j,const int k)
    {
    return dGrid[j*ndimZ+k];
    }
    int getDimZ();

};

class RadialGrid : public d3Grid
{
    std::vector<SurfElement> dGrid;

public:
    RadialGrid() : d3Grid() {}
    inline double& getElement(const int i,const int j,const int k)
    {
    return dGrid[i].getElement(j,k);
    }
    int getDimZ(int i);

    static d3Grid* buildIt(int nX, int nY, int nZ, int nGhostX, int nGhostY, int nGhostZ);
};

class d4grid
{
    std::vector<d3Grid*> pGridObj;
    int nDimGridObj;
    std::vector<int> ndimensions;

    public:
    d4grid() {}
    ~d4grid();
    d4grid(const d4grid& other) : pGridObj(other.pGridObj), nDimGridObj(other.nDimGridObj),
    ndimensions(other.ndimensions) {}
    d4grid& operator=(const d4grid& other);
    inline virtual double& getElement(const int i,const  int j,const int k,const int l)
    {
    return pGridObj[i]->getElement(j,k,l);
    }
    //virtual double* getElement(int i, int j, int k);
    //static d4grid* buildIt(int nVarNum, int nRadialElementNum, int nThetaNum, int nPhiNum);
    static d4grid* buildNormal(int nVarNum, int** nDimensions);
    //static d4grid* buildRadial(int nVarNum, int nRadialElementNum, int nThetaNum, int nPhiNum, int nSizeX2, int nSizeY2, int nSizeZ2);
    static d4grid* buildRadial(int nVarNum, int** nDimensions, IntVec nSizeX, IntVec nSizeY, IntVec nSizeZ);
    void loadline(std::vector<double> line, int i, int j, int k);
    double* writeline(int i, int j, int k);

};



#endif