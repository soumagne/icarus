/*=========================================================================

  Project                 : Icarus
  Module                  : XdmfSteeringDoubleVectorProperty.cxx

  Authors:
     John Biddiscombe     Jerome Soumagne
     biddisco@cscs.ch     soumagne@cscs.ch

  Copyright (C) CSCS - Swiss National Supercomputing Centre.
  You may use modify and and distribute this code freely providing
  1) This copyright notice appears on all copies of source code
  2) An acknowledgment appears with any substantial usage of the code
  3) If this code is contributed to any other open source project, it
  must not be reformatted such that the indentation, bracketing or
  overall style is modified significantly.

  This software is distributed WITHOUT ANY WARRANTY; without even the
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

  This work has received funding from the European Community's Seventh
  Framework Programme (FP7/2007-2013) under grant agreement 225967 “NextMuSE”

=========================================================================*/
#include "XdmfSteeringDoubleVectorProperty.h"

#include <vtkstd/vector>

struct XdmfIntVectorPropertyInternals
{
  vtkstd::vector<double> Values;
  vtkstd::vector<double> LastPushedValues; // These are the values that
      // were last pushed onto the server. These are used to generate
      // the undo/redo state.
  bool DefaultsValid;

  XdmfIntVectorPropertyInternals() : DefaultsValid(false) {}
  void UpdateLastPushedValues()
    {
    // Update LastPushedValues.
    this->LastPushedValues.clear();
    this->LastPushedValues.insert(this->LastPushedValues.end(),
      this->Values.begin(), this->Values.end());
    }
};
//---------------------------------------------------------------------------
XdmfSteeringDoubleVectorProperty::XdmfSteeringDoubleVectorProperty()
{
  this->Internals = new XdmfIntVectorPropertyInternals;
}
//---------------------------------------------------------------------------
XdmfSteeringDoubleVectorProperty::~XdmfSteeringDoubleVectorProperty()
{
  delete this->Internals;
}
//---------------------------------------------------------------------------
void XdmfSteeringDoubleVectorProperty::SetNumberOfElements(unsigned int num)
{
  if (num == this->Internals->Values.size())
    {
    return;
    }
  this->Internals->Values.resize(num, 0);
  if (num == 0)
    {
    // If num == 0, then we already have the intialized values (so to speak).
    this->Initialized = true;
    }
  else
    {
    this->Initialized = false;
    }
}
//---------------------------------------------------------------------------
unsigned int XdmfSteeringDoubleVectorProperty::GetNumberOfElements()
{
  return static_cast<unsigned int>(this->Internals->Values.size());
}
//---------------------------------------------------------------------------
double XdmfSteeringDoubleVectorProperty::GetElement(unsigned int idx)
{
  return this->Internals->Values[idx];
}
//---------------------------------------------------------------------------
double *XdmfSteeringDoubleVectorProperty::GetElements()
{
  return (this->Internals->Values.size() > 0)?
    &this->Internals->Values[0] : NULL;
}
//---------------------------------------------------------------------------
int XdmfSteeringDoubleVectorProperty::SetElement(unsigned int idx, double value)
{
  unsigned int numElems = this->GetNumberOfElements();

  if (this->Initialized && idx < numElems && value == this->GetElement(idx))
    {
    return 1;
    }

  if (idx >= numElems)
    {
    this->SetNumberOfElements(idx+1);
    }
  this->Internals->Values[idx] = value;
  this->Initialized = true;
  return 1;
}
//---------------------------------------------------------------------------
int XdmfSteeringDoubleVectorProperty::SetElements1(double value0)
{
  return this->SetElement(0, value0);
}

//---------------------------------------------------------------------------
int XdmfSteeringDoubleVectorProperty::SetElements2(double value0, double value1)
{
  int retVal1 = this->SetElement(0, value0);
  int retVal2 = this->SetElement(1, value1);
  return (retVal1 && retVal2);
}

//---------------------------------------------------------------------------
int XdmfSteeringDoubleVectorProperty::SetElements3(double value0,
                                          double value1,
                                          double value2)
{
  int retVal1 = this->SetElement(0, value0);
  int retVal2 = this->SetElement(1, value1);
  int retVal3 = this->SetElement(2, value2);
  return (retVal1 && retVal2 && retVal3);
}
//---------------------------------------------------------------------------
int XdmfSteeringDoubleVectorProperty::SetElements(const double* values)
{
  unsigned int numArgs = this->GetNumberOfElements();

  int modified = 0;
  for (unsigned int i=0; i<numArgs; i++)
    {
    if (this->Internals->Values[i] != values[i])
      {
      modified = 1;
      break;
      }
    }
  if(!modified && this->Initialized)
    {
    return 1;
    }

  memcpy(&this->Internals->Values[0], values, numArgs*sizeof(double));
  this->Initialized = true;
  return 1;
}

