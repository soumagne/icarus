/*=========================================================================

  Project                 : Icarus
  Module                  : XdmfSteeringIntVectorProperty.cxx

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
#include "XdmfSteeringIntVectorProperty.h"

#include <vtkstd/vector>

struct XdmfIntVectorPropertyInternals
{
  vtkstd::vector<int> Values;
  vtkstd::vector<int> LastPushedValues; // These are the values that
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
XdmfSteeringIntVectorProperty::XdmfSteeringIntVectorProperty()
{
  this->Internals = new XdmfIntVectorPropertyInternals;
}
//---------------------------------------------------------------------------
XdmfSteeringIntVectorProperty::~XdmfSteeringIntVectorProperty()
{
  delete this->Internals;
}
//---------------------------------------------------------------------------
void XdmfSteeringIntVectorProperty::SetNumberOfElements(unsigned int num)
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
unsigned int XdmfSteeringIntVectorProperty::GetNumberOfElements()
{
  return static_cast<unsigned int>(this->Internals->Values.size());
}
//---------------------------------------------------------------------------
int XdmfSteeringIntVectorProperty::GetElement(unsigned int idx)
{
  return this->Internals->Values[idx];
}
//---------------------------------------------------------------------------
int *XdmfSteeringIntVectorProperty::GetElements()
{
  return (this->Internals->Values.size() > 0)?
    &this->Internals->Values[0] : NULL;
}
//---------------------------------------------------------------------------
int XdmfSteeringIntVectorProperty::SetElement(unsigned int idx, int value)
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
int XdmfSteeringIntVectorProperty::SetElements1(int value0)
{
  return this->SetElement(0, value0);
}

//---------------------------------------------------------------------------
int XdmfSteeringIntVectorProperty::SetElements2(int value0, int value1)
{
  int retVal1 = this->SetElement(0, value0);
  int retVal2 = this->SetElement(1, value1);
  return (retVal1 && retVal2);
}

//---------------------------------------------------------------------------
int XdmfSteeringIntVectorProperty::SetElements3(int value0,
                                          int value1,
                                          int value2)
{
  int retVal1 = this->SetElement(0, value0);
  int retVal2 = this->SetElement(1, value1);
  int retVal3 = this->SetElement(2, value2);
  return (retVal1 && retVal2 && retVal3);
}
//---------------------------------------------------------------------------
int XdmfSteeringIntVectorProperty::SetElements(const int* values)
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

  memcpy(&this->Internals->Values[0], values, numArgs*sizeof(int));
  this->Initialized = true;
  return 1;
}

