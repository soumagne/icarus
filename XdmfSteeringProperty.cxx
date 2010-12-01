/*=========================================================================

  Project                 : Icarus
  Module                  : XdmfSteeringProperty.cxx

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
#include "XdmfSteeringProperty.h"

//---------------------------------------------------------------------------
XdmfSteeringProperty::XdmfSteeringProperty()
{
  this->Command = 0;
  this->XMLName = 0;
  this->XMLLabel = 0;
  this->Documentation = 0;
}
//---------------------------------------------------------------------------
XdmfSteeringProperty::~XdmfSteeringProperty()
{
  this->SetCommand(0);
  this->SetXMLName(0);
  this->SetXMLLabel(0);
  this->SetDocumentation(0);
}
//-----------------------------------------------------------------------------