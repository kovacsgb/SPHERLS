#ifndef D4GRID
#define D4GRID


class d4grid {

private:
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
    double& getElement(int i, int j, int k, int l);
    static d4grid buildIt(int nVarNum, int nRadialElementNum, int nThetaNum, int nPhiNum);

};


#endif