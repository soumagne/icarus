/*=========================================================================

  Project                 : Icarus
  Module                  : vtkDSMProxyHelper.h

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
// .NAME vtkDSMProxyHelper - Manipulate Proxy objects created from steering XML
// .SECTION Description
// Create/Expose a proxies via the steering XML

#ifndef __vtkDSMProxyHelper_h
#define __vtkDSMProxyHelper_h

#include "vtkDataObjectAlgorithm.h"    // Base class

// BTX
class vtkDSMManager;
class vtkClientServerInterpreter;
extern "C" void VTK_EXPORT DSMProxyHelperInit(vtkClientServerInterpreter *csi);
//ETX

class VTK_EXPORT vtkDSMProxyHelper : public vtkDataObjectAlgorithm
{
public:
  static vtkDSMProxyHelper *New();
  vtkTypeMacro(vtkDSMProxyHelper,vtkDataObjectAlgorithm);

  virtual void SetDSMManager(vtkDSMManager*);
  vtkGetObjectMacro(DSMManager, vtkDSMManager)
protected:
    vtkDSMProxyHelper();
   ~vtkDSMProxyHelper();

  int FillInputPortInformation(int port, vtkInformation* info);

  vtkDSMManager *DSMManager;

private:
    vtkDSMProxyHelper(const vtkDSMProxyHelper&);  // Not implemented.
    void operator=(const vtkDSMProxyHelper&);  // Not implemented.
};

#endif
