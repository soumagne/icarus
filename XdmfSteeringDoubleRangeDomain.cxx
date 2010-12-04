/*=========================================================================

  Project                 : Icarus
  Module                  : XdmfSteeringDoubleRangeDomain.cxx

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
#include "XdmfSteeringDoubleRangeDomain.h"

#include <vector>

struct XdmfSteeringDoubleRangeDomainInternals
{
  struct EntryType
  {
    double Min;
    double Max;
    double Resolution;
    int MinSet;
    int MaxSet;
    int ResolutionSet;

    EntryType() : Min(0), Max(0), Resolution(0), MinSet(0), MaxSet(0), ResolutionSet(0) {}
  };
  std::vector<EntryType> Entries;
};

//---------------------------------------------------------------------------
XdmfSteeringDoubleRangeDomain::XdmfSteeringDoubleRangeDomain()
{
  this->DRInternals = new XdmfSteeringDoubleRangeDomainInternals;
  this->DomainType = XDMF_STEERING_DOUBLE_RANGE_DOMAIN;
}

//---------------------------------------------------------------------------
XdmfSteeringDoubleRangeDomain::~XdmfSteeringDoubleRangeDomain()
{
  delete this->DRInternals;
}

//---------------------------------------------------------------------------
int XdmfSteeringDoubleRangeDomain::IsInDomain(unsigned int idx, double val)
{
  // User has not put any condition so domains is always valid
  if (idx >= this->DRInternals->Entries.size())
    {
    return 1;
    }
  if ( this->DRInternals->Entries[idx].MinSet &&
       val < this->DRInternals->Entries[idx].Min )
    {
    return 0;
    }

  if ( this->DRInternals->Entries[idx].MaxSet &&
       val > this->DRInternals->Entries[idx].Max )
    {
    return 0;
    }
  
  if ( this->DRInternals->Entries[idx].ResolutionSet )
    {
    // check if value is a multiple of resolution + min:
    int exists;
    double min = this->GetMinimum(idx,exists); //set to 0 if necesseary
    double res = this->DRInternals->Entries[idx].Resolution;
    int multi = (int)((val - min) / res);
    return (multi*res + min - val) == 0.;
    }
  //else the resolution is not taken into account

  return 1;
}

//---------------------------------------------------------------------------
unsigned int XdmfSteeringDoubleRangeDomain::GetNumberOfEntries()
{
  return this->DRInternals->Entries.size();
}

//---------------------------------------------------------------------------
void XdmfSteeringDoubleRangeDomain::SetNumberOfEntries(unsigned int size)
{
  this->DRInternals->Entries.resize(size);
}

//---------------------------------------------------------------------------
double XdmfSteeringDoubleRangeDomain::GetMinimum(unsigned int idx, int& exists)
{
  exists = 0;
  if (idx >= this->DRInternals->Entries.size())
    {
    return 0;
    }
  if (this->DRInternals->Entries[idx].MinSet)
    {
    exists=1;
    return this->DRInternals->Entries[idx].Min;
    }
  return 0;
}

//---------------------------------------------------------------------------
double XdmfSteeringDoubleRangeDomain::GetMaximum(unsigned int idx, int& exists)
{
  exists = 0;
  if (idx >= this->DRInternals->Entries.size())
    {
    return 0;
    }
  if (this->DRInternals->Entries[idx].MaxSet)
    {
    exists=1;
    return this->DRInternals->Entries[idx].Max;
    }
  return 0;
}

//---------------------------------------------------------------------------
int XdmfSteeringDoubleRangeDomain::GetMinimumExists(unsigned int idx)
{
  if (idx >= this->DRInternals->Entries.size())
    {
    return 0;
    }
  return this->DRInternals->Entries[idx].MinSet;
}

//---------------------------------------------------------------------------
int XdmfSteeringDoubleRangeDomain::GetMaximumExists(unsigned int idx)
{
  if (idx >= this->DRInternals->Entries.size())
    {
    return 0;
    }
  return this->DRInternals->Entries[idx].MaxSet;
}


//---------------------------------------------------------------------------
double XdmfSteeringDoubleRangeDomain::GetMaximum(unsigned int idx)
{
  if (!this->GetMaximumExists(idx))
    {
    return 0;
    }
  return this->DRInternals->Entries[idx].Max;
}

//---------------------------------------------------------------------------
double XdmfSteeringDoubleRangeDomain::GetMinimum(unsigned int idx)
{
  if (!this->GetMinimumExists(idx))
    {
    return 0;
    }
  return this->DRInternals->Entries[idx].Min;
}

//---------------------------------------------------------------------------
double XdmfSteeringDoubleRangeDomain::GetResolution(unsigned int idx, int& exists)
{
  exists = 0;
  if (idx >= this->DRInternals->Entries.size())
    {
    return 0;
    }
  if (this->DRInternals->Entries[idx].ResolutionSet)
    {
    exists=1;
    return this->DRInternals->Entries[idx].Resolution;
    }
  return 0;
}

//---------------------------------------------------------------------------
int XdmfSteeringDoubleRangeDomain::GetResolutionExists(unsigned int idx)
{
  if (idx >= this->DRInternals->Entries.size())
    {
    return 0;
    }
  return this->DRInternals->Entries[idx].ResolutionSet;
}

//---------------------------------------------------------------------------
double XdmfSteeringDoubleRangeDomain::GetResolution(unsigned int idx)
{
  if (!this->GetResolutionExists(idx))
    {
    return 0;
    }
  return this->DRInternals->Entries[idx].Resolution;
}

//---------------------------------------------------------------------------
void XdmfSteeringDoubleRangeDomain::AddMinimum(unsigned int idx, double val)
{
  this->SetEntry(idx, XdmfSteeringDoubleRangeDomain::MIN, 1, val);
}

//---------------------------------------------------------------------------
void XdmfSteeringDoubleRangeDomain::RemoveMinimum(unsigned int idx)
{
  this->SetEntry(idx, XdmfSteeringDoubleRangeDomain::MIN, 0, 0);
}

//---------------------------------------------------------------------------
void XdmfSteeringDoubleRangeDomain::RemoveAllMinima()
{
  unsigned int numEntries = this->GetNumberOfEntries();
  for(unsigned int idx=0; idx<numEntries; idx++)
    {
    this->SetEntry(idx, XdmfSteeringDoubleRangeDomain::MIN, 0, 0);
    }
}

//---------------------------------------------------------------------------
void XdmfSteeringDoubleRangeDomain::AddMaximum(unsigned int idx, double val)
{
  this->SetEntry(idx, XdmfSteeringDoubleRangeDomain::MAX, 1, val);
}

//---------------------------------------------------------------------------
void XdmfSteeringDoubleRangeDomain::RemoveMaximum(unsigned int idx)
{
  this->SetEntry(idx, XdmfSteeringDoubleRangeDomain::MAX, 0, 0);
}

//---------------------------------------------------------------------------
void XdmfSteeringDoubleRangeDomain::RemoveAllMaxima()
{
  unsigned int numEntries = this->GetNumberOfEntries();
  for(unsigned int idx=0; idx<numEntries; idx++)
    {
    this->SetEntry(idx, XdmfSteeringDoubleRangeDomain::MAX, 0, 0);
    }
}

//---------------------------------------------------------------------------
void XdmfSteeringDoubleRangeDomain::AddResolution(unsigned int idx, double val)
{
  this->SetEntry(idx, XdmfSteeringDoubleRangeDomain::RESOLUTION, 1, val);
}

//---------------------------------------------------------------------------
void XdmfSteeringDoubleRangeDomain::RemoveResolution(unsigned int idx)
{
  this->SetEntry(idx, XdmfSteeringDoubleRangeDomain::RESOLUTION, 0, 0);
}

//---------------------------------------------------------------------------
void XdmfSteeringDoubleRangeDomain::RemoveAllResolutions()
{
  unsigned int numEntries = this->GetNumberOfEntries();
  for(unsigned int idx=0; idx<numEntries; idx++)
    {
    this->SetEntry(idx, XdmfSteeringDoubleRangeDomain::RESOLUTION, 0, 0);
    }
}

//---------------------------------------------------------------------------
void XdmfSteeringDoubleRangeDomain::SetEntry(
  unsigned int idx, int minOrMaxOrRes, int set, double value)
{
  if (idx >= this->DRInternals->Entries.size())
    {
    this->DRInternals->Entries.resize(idx+1);
    }
  if (minOrMaxOrRes == MIN)
    {
    if (set)
      {
      this->DRInternals->Entries[idx].MinSet = 1;
      this->DRInternals->Entries[idx].Min = value;
      }
    else
      {
      this->DRInternals->Entries[idx].MinSet = 0;
      }
    }
  else if(minOrMaxOrRes == MAX)
    {
    if (set)
      {
      this->DRInternals->Entries[idx].MaxSet = 1;
      this->DRInternals->Entries[idx].Max = value;
      }
    else
      {
      this->DRInternals->Entries[idx].MaxSet = 0;
      }
    }
  else //if (minOrMaxOrRes == RESOLUTION)
    {
    if (set)
      {
      this->DRInternals->Entries[idx].ResolutionSet = 1;
      this->DRInternals->Entries[idx].Resolution = value;
      }
    else
      {
      this->DRInternals->Entries[idx].ResolutionSet = 0;
      }
    }
}
