#ifndef D4GRID
#define D4GRID


class d4grid {

protected:
    double* pGridobj;

    int ndim1;
    int ndim2;
    int ndim3;
    int ndim4;

    int nBlockSize;
    int nSurfaceSize;
    int nRowSize;


    inline int calcindex(int i, int j, int k, int l);

public:
    d4grid() = default;
    ~d4grid();
    virtual double& getElement(int i, int j, int k, int l);
    virtual double* getElement(int i, int j, int k);
    static d4grid* buildIt(int nVarNum, int nRadialElementNum, int nThetaNum, int nPhiNum);
    static d4grid* buildRadial(int nVarNum, int nRadialElementNum, int nThetaNum, int nPhiNum, int nSizeX2, int nSizeY2, int nSizeZ2);

};

class d4gridRadial : public d4grid
{
private:
    
    d4grid *ghostGrid;


public:
    d4gridRadial() : d4grid() {};
    ~d4gridRadial();
    void buildGhost(int nSizeX2, int nSizeY2, int nSizeZ2);
    double& getElement(int i, int j, int k, int l);
    double* getElement(int i, int j, int k);


};

#endif