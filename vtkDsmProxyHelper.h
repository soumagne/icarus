/*=========================================================================

  Project                 : Icarus
  Module                  : vtkDsmProxyHelper.h

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
// .NAME vtkDsmProxyHelper - Manipulate Proxy objects created from steering XML
// .SECTION Description
// Create/Expose a proxies via the steering XML

#ifndef __vtkDsmProxyHelper_h
#define __vtkDsmProxyHelper_h

#include "vtkDataObjectAlgorithm.h"    // Base class
#include <map>

// BTX
class vtkDsmManager;
class vtkSteeringWriter;
class vtkClientServerInterpreter;
extern "C" void VTK_EXPORT DsmProxyHelperInit(vtkClientServerInterpreter *csi);
//ETX

class VTK_EXPORT vtkDsmProxyHelper : public vtkDataObjectAlgorithm
{
public:
  static vtkDsmProxyHelper *New();
  vtkTypeMacro(vtkDsmProxyHelper,vtkDataObjectAlgorithm);

  virtual void SetDsmManager(vtkDsmManager*);
  vtkGetObjectMacro(DsmManager, vtkDsmManager)

  virtual void SetSteeringWriter(vtkSteeringWriter*);
  vtkGetObjectMacro(SteeringWriter, vtkSteeringWriter)

  void WriteDataSetArrayData(const char *desc);

  int                BlockTraffic;

protected:
  vtkDsmProxyHelper();
  virtual ~vtkDsmProxyHelper();

  int FillInputPortInformation(int port, vtkInformation* info);

  vtkDsmManager     *DsmManager;
  vtkSteeringWriter *SteeringWriter;

private:
    vtkDsmProxyHelper(const vtkDsmProxyHelper&);  // Not implemented.
    void operator=(const vtkDsmProxyHelper&);  // Not implemented.
};

#endif
