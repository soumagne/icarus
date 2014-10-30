/*=========================================================================

  Project                 : Icarus
  Module                  : vtkBonsaiDsmManager.cxx

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
  Framework Programme (FP7/2007-2013) under grant agreement 225967 ���NextMuSE���

=========================================================================*/
#include "vtkBonsaiDsmManager.h"
//
#include "vtkObjectFactory.h"
//
#include "vtkProcessModule.h"
#include "vtkSMProxyManager.h"
#include "vtkClientServerInterpreterInitializer.h"
#include "vtkSMProxyDefinitionManager.h"
#include "vtkSMSessionProxyManager.h"
#include "vtkPVSessionServer.h"
#include "vtkPVSessionCore.h"
//
// For PARAVIEW_USE_MPI
#include "vtkPVConfig.h"
#ifdef PARAVIEW_USE_MPI
 #include "vtkMPI.h"
 #include "vtkMPIController.h"
 #include "vtkMPICommunicator.h"
#endif
// Otherwise
#include "vtkMultiProcessController.h"
//
#include "vtkDsmProxyHelper.h"
#include "vtkPVOptions.h"
#include "vtkClientSocket.h"

#include "vtkMultiThreader.h"
#include "vtkConditionVariable.h"

//----------------------------------------------------------------------------
vtkStandardNewMacro(vtkBonsaiDsmManager);
//----------------------------------------------------------------------------

//----------------------------------------------------------------------------
vtkBonsaiDsmManager::vtkBonsaiDsmManager()
{
//  std::cout << "Created a Bonsai DSM manager" << std::endl;
}

//----------------------------------------------------------------------------
vtkBonsaiDsmManager::~vtkBonsaiDsmManager()
{ 
}
//----------------------------------------------------------------------------



