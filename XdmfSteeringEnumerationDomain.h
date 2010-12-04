/*=========================================================================

  Project                 : Icarus
  Module                  : XdmfSteeringEnumerationDomain.h

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

#ifndef __XdmfSteeringEnumerationDomain_h
#define __XdmfSteeringEnumerationDomain_h

#include "XdmfSteeringDomain.h"

//BTX
struct XdmfSteeringEnumerationDomainInternals;
//ETX

class XDMF_EXPORT XdmfSteeringEnumerationDomain : public XdmfSteeringDomain
{
public:
  XdmfSteeringEnumerationDomain();
  ~XdmfSteeringEnumerationDomain();

  // Description:
  // Returns true if the int is in the domain. If value is
  // in domain, it's index is return in idx.
  int IsInDomain(int val, unsigned int& idx);

  // Description:
  // Returns the number of entries in the enumeration.
  unsigned int GetNumberOfEntries();

  // Description:
  // Returns the integer value of an enumeration entry.
  int GetEntryValue(unsigned int idx);

  // Description:
  // Returns the descriptive string of an enumeration entry.
  const char* GetEntryText(unsigned int idx);

  // Description:
  // Returns the text for an enumeration value.
  const char* GetEntryTextForValue(int value);

  // Description:
  // Return 1 is the text is present in the enumeration, otherwise 0.
  int HasEntryText(const char* text);

  // Description:
  // Get the value for an enumeration text. The return value is valid only is
  // HasEntryText() returns 1.
  int GetEntryValueForText(const char* text);
//BTX
  // Description:
  // Given an entry text, return the integer value.
  // Valid is set to 1 if text is defined, otherwise 0.
  // If valid=0, return value is undefined.
  int GetEntryValue(const char* text, int& valid);
//ETX

  // Description:
  // Add a new enumeration entry. text cannot be null.
  void AddEntry(const char* text, int value);

  // Description:
  // Clear all entries.
  void RemoveAllEntries();

protected:

  XdmfSteeringEnumerationDomainInternals* EInternals;
};

#endif
