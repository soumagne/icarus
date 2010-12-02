/*=========================================================================

  Project                 : Icarus
  Module                  : XdmfSteeringIntRangeDomain.cxx

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
#include "XdmfSteeringIntRangeDomain.h"

#include <vector>

struct XdmfSteeringIntRangeDomainInternals
{
  struct EntryType
  {
    int Min;
    int Max;
    int Resolution;
    int MinSet;
    int MaxSet;
    int ResolutionSet;

    EntryType() : Min(0), Max(0), Resolution(0), MinSet(0), MaxSet(0), ResolutionSet(0) {}
  };
  std::vector<EntryType> Entries;
};

//---------------------------------------------------------------------------
XdmfSteeringIntRangeDomain::XdmfSteeringIntRangeDomain()
{
  this->IRInternals = new XdmfSteeringIntRangeDomainInternals;
  this->DomainType = XDMF_STEERING_INT_RANGE_DOMAIN;
}

//---------------------------------------------------------------------------
XdmfSteeringIntRangeDomain::~XdmfSteeringIntRangeDomain()
{
  delete this->IRInternals;
}

//---------------------------------------------------------------------------
int XdmfSteeringIntRangeDomain::IsInDomain(unsigned int idx, int val)
{
  // User has not put any condition so domains is always valid
  if (idx >= this->IRInternals->Entries.size())
    {
    return 1;
    }
  if ( this->IRInternals->Entries[idx].MinSet &&
       val < this->IRInternals->Entries[idx].Min )
    {
    return 0;
    }

  if ( this->IRInternals->Entries[idx].MaxSet &&
       val > this->IRInternals->Entries[idx].Max )
    {
    return 0;
    }

  if ( this->IRInternals->Entries[idx].ResolutionSet )
    {
    // check if value is a multiple of resolution + min:
    int exists;
    int min = this->GetMinimum(idx,exists); //set to 0 if necesseary
    int res = this->IRInternals->Entries[idx].Resolution;
    int multi = (int)((val - min) / res);
    return (multi*res + min - val) == 0.;
    }
  //else the resolution is not taken into account

  return 1;
}

//---------------------------------------------------------------------------
unsigned int XdmfSteeringIntRangeDomain::GetNumberOfEntries()
{
  return this->IRInternals->Entries.size();
}

//---------------------------------------------------------------------------
void XdmfSteeringIntRangeDomain::SetNumberOfEntries(unsigned int size)
{
  this->IRInternals->Entries.resize(size);
}

//---------------------------------------------------------------------------
int XdmfSteeringIntRangeDomain::GetMinimum(unsigned int idx, int& exists)
{
  exists = 0;
  if (idx >= this->IRInternals->Entries.size())
    {
    return 0;
    }
  if (this->IRInternals->Entries[idx].MinSet)
    {
    exists=1;
    return this->IRInternals->Entries[idx].Min;
    }
  return 0;
}

//---------------------------------------------------------------------------
int XdmfSteeringIntRangeDomain::GetMaximum(unsigned int idx, int& exists)
{
  exists = 0;
  if (idx >= this->IRInternals->Entries.size())
    {
    return 0;
    }
  if (this->IRInternals->Entries[idx].MaxSet)
    {
    exists=1;
    return this->IRInternals->Entries[idx].Max;
    }
  return 0;
}

//---------------------------------------------------------------------------
int XdmfSteeringIntRangeDomain::GetMinimumExists(unsigned int idx)
{
  if (idx >= this->IRInternals->Entries.size())
    {
    return 0;
    }
  return this->IRInternals->Entries[idx].MinSet;
}

//---------------------------------------------------------------------------
int XdmfSteeringIntRangeDomain::GetMaximumExists(unsigned int idx)
{
  if (idx >= this->IRInternals->Entries.size())
    {
    return 0;
    }
  return this->IRInternals->Entries[idx].MaxSet;
}


//---------------------------------------------------------------------------
int XdmfSteeringIntRangeDomain::GetMaximum(unsigned int idx)
{
  if (!this->GetMaximumExists(idx))
    {
    return 0;
    }
  return this->IRInternals->Entries[idx].Max;
}

//---------------------------------------------------------------------------
int XdmfSteeringIntRangeDomain::GetMinimum(unsigned int idx)
{
  if (!this->GetMinimumExists(idx))
    {
    return 0;
    }
  return this->IRInternals->Entries[idx].Min;
}
//---------------------------------------------------------------------------
int XdmfSteeringIntRangeDomain::GetResolution(unsigned int idx, int& exists)
{
  exists = 0;
  if (idx >= this->IRInternals->Entries.size())
    {
    return 0;
    }
  if (this->IRInternals->Entries[idx].ResolutionSet)
    {
    exists=1;
    return this->IRInternals->Entries[idx].Resolution;
    }
  return 0;
}

//---------------------------------------------------------------------------
int XdmfSteeringIntRangeDomain::GetResolutionExists(unsigned int idx)
{
  if (idx >= this->IRInternals->Entries.size())
    {
    return 0;
    }
  return this->IRInternals->Entries[idx].ResolutionSet;
}

//---------------------------------------------------------------------------
int XdmfSteeringIntRangeDomain::GetResolution(unsigned int idx)
{
  if (!this->GetResolutionExists(idx))
    {
    return 0;
    }
  return this->IRInternals->Entries[idx].Resolution;
}

//---------------------------------------------------------------------------
void XdmfSteeringIntRangeDomain::AddMinimum(unsigned int idx, int val)
{
  this->SetEntry(idx, XdmfSteeringIntRangeDomain::MIN, 1, val);
}

//---------------------------------------------------------------------------
void XdmfSteeringIntRangeDomain::RemoveMinimum(unsigned int idx)
{
  this->SetEntry(idx, XdmfSteeringIntRangeDomain::MIN, 0, 0);
}

//---------------------------------------------------------------------------
void XdmfSteeringIntRangeDomain::RemoveAllMinima()
{
  unsigned int numEntries = this->GetNumberOfEntries();
  for(unsigned int idx=0; idx<numEntries; idx++)
    {
    this->SetEntry(idx, XdmfSteeringIntRangeDomain::MIN, 0, 0);
    }
}

//---------------------------------------------------------------------------
void XdmfSteeringIntRangeDomain::AddMaximum(unsigned int idx, int val)
{
  this->SetEntry(idx, XdmfSteeringIntRangeDomain::MAX, 1, val);
}

//---------------------------------------------------------------------------
void XdmfSteeringIntRangeDomain::RemoveMaximum(unsigned int idx)
{
  this->SetEntry(idx, XdmfSteeringIntRangeDomain::MAX, 0, 0);
}

//---------------------------------------------------------------------------
void XdmfSteeringIntRangeDomain::RemoveAllMaxima()
{
  unsigned int numEntries = this->GetNumberOfEntries();
  for(unsigned int idx=0; idx<numEntries; idx++)
    {
    this->SetEntry(idx, XdmfSteeringIntRangeDomain::MAX, 0, 0);
    }
}

//---------------------------------------------------------------------------
void XdmfSteeringIntRangeDomain::AddResolution(unsigned int idx, int val)
{
  this->SetEntry(idx, XdmfSteeringIntRangeDomain::RESOLUTION, 1, val);
}

//---------------------------------------------------------------------------
void XdmfSteeringIntRangeDomain::RemoveResolution(unsigned int idx)
{
  this->SetEntry(idx, XdmfSteeringIntRangeDomain::RESOLUTION, 0, 0);
}

//---------------------------------------------------------------------------
void XdmfSteeringIntRangeDomain::RemoveAllResolutions()
{
  unsigned int numEntries = this->GetNumberOfEntries();
  for(unsigned int idx=0; idx<numEntries; idx++)
    {
    this->SetEntry(idx, XdmfSteeringIntRangeDomain::RESOLUTION, 0, 0);
    }
}

//---------------------------------------------------------------------------
void XdmfSteeringIntRangeDomain::SetEntry(
  unsigned int idx, int minOrMaxOrRes, int set, int value)
{
  if (idx >= this->IRInternals->Entries.size())
    {
    this->IRInternals->Entries.resize(idx+1);
    }
  if (minOrMaxOrRes == MIN)
    {
    if (set)
      {
      this->IRInternals->Entries[idx].MinSet = 1;
      this->IRInternals->Entries[idx].Min = value;
      }
    else
      {
      this->IRInternals->Entries[idx].MinSet = 0;
      }
    }
  else if(minOrMaxOrRes == MAX)
    {
    if (set)
      {
      this->IRInternals->Entries[idx].MaxSet = 1;
      this->IRInternals->Entries[idx].Max = value;
      }
    else
      {
      this->IRInternals->Entries[idx].MaxSet = 0;
      }
    }
  else //if (minOrMaxOrRes == RESOLUTION)
    {
    if (set)
      {
      this->IRInternals->Entries[idx].ResolutionSet = 1;
      this->IRInternals->Entries[idx].Resolution = value;
      }
    else
      {
      this->IRInternals->Entries[idx].ResolutionSet = 0;
      }
    }
}
