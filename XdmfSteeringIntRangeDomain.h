/*=========================================================================

  Project                 : Icarus
  Module                  : XdmfSteeringIntRangeDomain.h

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

#ifndef __XdmfSteeringIntRangeDomain_h
#define __XdmfSteeringIntRangeDomain_h

#include "XdmfSteeringDomain.h"

//BTX
struct XdmfSteeringIntRangeDomainInternals;
//ETX

class XDMF_EXPORT XdmfSteeringIntRangeDomain : public XdmfSteeringDomain
{
public:
  XdmfSteeringIntRangeDomain();
  ~XdmfSteeringIntRangeDomain();

  // Description:
  // Returns true if the int is in the domain. If value is
  // in domain, it's index is return in idx.
  // A value is in the domain if it is between (min, max)
  int IsInDomain(unsigned int idx, int val);

  // Description:
  // Return a min. value if it exists. If the min. exists
  // exists is set to 1. Otherwise, it is set to 0.
  // An unspecified min. is equivalent to -inf
  int GetMinimum(unsigned int idx, int& exists);

  // Description:
  // Return a max. value if it exists. If the min. exists
  // exists is set to 1. Otherwise, it is set to 0.
  // An unspecified max. is equivalent to inf
  int GetMaximum(unsigned int idx, int& exists);

  // Description:
  // Returns if minimum/maximum bound is set for the domain.
  int GetMinimumExists(unsigned int idx);
  int GetMaximumExists(unsigned int idx);

  // Description:
  // Returns the minimum/maximum value, is exists, otherwise
  // 0 is returned. Use GetMaximumExists() GetMaximumExists() to make sure that
  // the bound is set.
  int GetMinimum(unsigned int idx);
  int GetMaximum(unsigned int idx);

  // Description:
  // Return a resolution. value if it exists. If the resolution. exists
  // exists is set to 1. Otherwise, it is set to 0.
  // An unspecified max. is equivalent to 1
  int GetResolution(unsigned int idx, int& exists);

  // Description:
  // Returns is the relution value is set for the given index.
  int GetResolutionExists(unsigned int idx);
 
  // Description:
  // Return a resolution. value if it exists, otherwise 0. 
  // Use GetResolutionExists() to make sure that the value exists.
  int GetResolution(unsigned int idx);

  // Description:
  // Set a min. of a given index.
  void AddMinimum(unsigned int idx, int value);

  // Description:
  // Remove a min. of a given index.
  // An unspecified min. is equivalent to -inf
  void RemoveMinimum(unsigned int idx);

  // Description:
  // Clear all minimum values.
  void RemoveAllMinima();

  // Description:
  // Set a max. of a given index.
  void AddMaximum(unsigned int idx, int value);

  // Description:
  // Remove a max. of a given index.
  // An unspecified min. is equivalent to inf
  void RemoveMaximum(unsigned int idx);

  // Description:
  // Clear all maximum values.
  void RemoveAllMaxima();

  // Description:
  // Set a resolution. of a given index.
  void AddResolution(unsigned int idx, int value);

  // Description:
  // Remove a resolution. of a given index.
  // An unspecified resolution. is equivalent to 1
  void RemoveResolution(unsigned int idx);

  // Description:
  // Clear all resolution values.
  void RemoveAllResolutions();

  // Description:
  // Returns the number of entries in the internal
  // maxima/minima list. No maxima/minima exists beyond
  // this index. Maxima/minima below this number may or
  // may not exist.
  unsigned int GetNumberOfEntries();

protected:

  // Description:
  // General purpose method called by both AddMinimum() and AddMaximum()
  void SetEntry(unsigned int idx, int minOrMax, int set, int value);

  // Internal use only.
  // Set/Get the number of min/max entries.
  void SetNumberOfEntries(unsigned int size);

  XdmfSteeringIntRangeDomainInternals* IRInternals;

//BTX
  enum
  {
    MIN = 0,
    MAX = 1,
    RESOLUTION = 2
  };
//ETX
};

#endif
