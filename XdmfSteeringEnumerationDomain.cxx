/*=========================================================================

  Project                 : Icarus
  Module                  : XdmfSteeringEnumerationDomain.cxx

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
#include "XdmfSteeringEnumerationDomain.h"

#include <vector>
#include <string>

struct XdmfSteeringEnumerationDomainInternals
{
  struct EntryType
  {
    EntryType(std::string text, int value) : Text(text), Value(value) {}
    std::string Text;
    int Value;
  };

  typedef std::vector<EntryType> EntriesType;
  EntriesType  Entries;
};

//---------------------------------------------------------------------------
XdmfSteeringEnumerationDomain::XdmfSteeringEnumerationDomain()
{
  this->EInternals = new XdmfSteeringEnumerationDomainInternals;
  this->DomainType = XDMF_STEERING_ENUMERATION_DOMAIN;
}

//---------------------------------------------------------------------------
XdmfSteeringEnumerationDomain::~XdmfSteeringEnumerationDomain()
{
  delete this->EInternals;
}

//---------------------------------------------------------------------------
unsigned int XdmfSteeringEnumerationDomain::GetNumberOfEntries()
{
  return static_cast<unsigned int>(this->EInternals->Entries.size());
}

//---------------------------------------------------------------------------
int XdmfSteeringEnumerationDomain::GetEntryValue(unsigned int idx)
{
  if (idx >= static_cast<unsigned int>(this->EInternals->Entries.size()))
    {
    XdmfErrorMessage("Invalid idx: " << idx);
    return 0;
    }
  return this->EInternals->Entries[idx].Value;
}

//---------------------------------------------------------------------------
const char* XdmfSteeringEnumerationDomain::GetEntryText(unsigned int idx)
{
  if (idx >= static_cast<unsigned int>(this->EInternals->Entries.size()))
    {
    XdmfErrorMessage("Invalid idx: " << idx);
    return NULL;
    }
  return this->EInternals->Entries[idx].Text.c_str();
}

//---------------------------------------------------------------------------
const char* XdmfSteeringEnumerationDomain::GetEntryTextForValue(int value)
{
  unsigned int idx = 0;
  if (!this->IsInDomain(value, idx))
    {
    return NULL;
    }
  return this->GetEntryText(idx);
}

//---------------------------------------------------------------------------
int XdmfSteeringEnumerationDomain::HasEntryText(const char* text)
{
  int valid;
  this->GetEntryValue(text, valid);
  return valid;
}

//---------------------------------------------------------------------------
int XdmfSteeringEnumerationDomain::GetEntryValueForText(const char* text)
{
  int valid;
  return this->GetEntryValue(text, valid);
}

//---------------------------------------------------------------------------
int XdmfSteeringEnumerationDomain::GetEntryValue(const char* text, int& valid)
{
  valid = 0;

  if (!text)
    {
    return -1;
    }

  XdmfSteeringEnumerationDomainInternals::EntriesType::iterator iter =
    this->EInternals->Entries.begin();
  for (; iter != this->EInternals->Entries.end(); ++iter)
    {
    if (iter->Text == text)
      {
      valid = 1;
      return iter->Value;
      }
    }

  return -1;
}

//---------------------------------------------------------------------------
int XdmfSteeringEnumerationDomain::IsInDomain(int val, unsigned int& idx)
{
  unsigned int numEntries = this->GetNumberOfEntries();
  if (numEntries == 0)
    {
    return 1;
    }

  for (unsigned int i=0; i<numEntries; i++)
    {
    if (val == this->GetEntryValue(i))
      {
      idx = i;
      return 1;
      }
    }
  return 0;
}

//---------------------------------------------------------------------------
void XdmfSteeringEnumerationDomain::AddEntry(const char* text, int value)
{
  this->EInternals->Entries.push_back(
    XdmfSteeringEnumerationDomainInternals::EntryType(text, value));
}

//---------------------------------------------------------------------------
void XdmfSteeringEnumerationDomain::RemoveAllEntries()
{
  this->EInternals->Entries.erase(
    this->EInternals->Entries.begin(), this->EInternals->Entries.end());
}
