#pragma once

#include <cassert>
#include <cmath>
#ifdef BONSAI_CATALYST_STDLIB
 #include <boost/array.hpp>
 #include <boost/function.hpp>
 #define bonsaistd boost
#else
 #include <array>
 #define bonsaistd std
#endif
#include <algorithm>
#include <iostream>
#include <vector>
#include <stdint.h>
#include "IDType.h"
#ifdef BONSAI_CATALYST_CLANG
 #include <tmmintrin.h>
#endif
#include <mpi.h>

#include "vtkSmartPointer.h"
#include "vtkFloatArray.h"
#include "vtkIdTypeArray.h"

class RendererData
{
  public:
    typedef unsigned long long long_t;
    enum Attribute_t {
      MASS,
      VEL,
      RHO,
      H,
      NPROP};
  public:
    const int rank, nrank;
    const MPI_Comm &comm;
/*
    struct particle_t
    {
      float posx, posy, posz;
      IDType ID;
      float attribute[NPROP];
    };
    std::vector<particle_t> data;
*/
    vtkSmartPointer<vtkIdTypeArray> Id;
    vtkSmartPointer<vtkFloatArray>  coords;
    vtkSmartPointer<vtkFloatArray>  mass;
    vtkSmartPointer<vtkFloatArray>  vel;
    vtkSmartPointer<vtkFloatArray>  rho;
    vtkSmartPointer<vtkFloatArray>  Hval;
    bool new_data;

  public:
    int  getMaster() const { return 0; }
    bool isMaster() const { return getMaster() == rank; }

    void resize(vtkIdType N) {
      vtkDataArray *arr[6] = { Id, coords, mass, vel, rho, Hval };
      for (auto i : arr ) {
        i->SetNumberOfTuples(N);
      }
    }

  public:
    RendererData(const int rank, const int nrank, const MPI_Comm &comm) : 
      rank(rank), nrank(nrank), comm(comm)
  {
    assert(rank < nrank);
    new_data = false;
    Id = vtkSmartPointer<vtkIdTypeArray>::New();
    coords = vtkSmartPointer<vtkFloatArray>::New();
    mass = vtkSmartPointer<vtkFloatArray>::New();
    vel = vtkSmartPointer<vtkFloatArray>::New();
    rho = vtkSmartPointer<vtkFloatArray>::New();
    Hval = vtkSmartPointer<vtkFloatArray>::New();
    coords->SetNumberOfComponents(3);
    // if we want a tuple for velocity, use this
    // vel->SetNumberOfComponents(3);
  }

    void setNewData() {new_data = true;}
    void unsetNewData() { new_data = false; }
    bool isNewData() const {return new_data;}

    int n() const { return coords->GetNumberOfTuples(); }
    ~RendererData() {}

    // virtual methods

    virtual bool  isDistributed() const { return false; }
    virtual void  setNMAXSAMPLE(const int n) {};
    virtual void  distribute() {}
};
