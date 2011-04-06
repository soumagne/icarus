/*=========================================================================

  Project                 : Icarus
  Module                  : pqDSMViewerPanel.h

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
#include "pqUpdateThread.h"

#include "pqDSMViewerPanel.h"

pqUpdateThread::pqUpdateThread(pqDSMViewerPanel *p) {
  this->DSMViewerPanel = p;
  this->Terminating = false;
}
//----------------------------------------------------------------------------
pqUpdateThread::~pqUpdateThread() {
  this->Terminating = true;
  wait();
}
//----------------------------------------------------------------------------
void pqUpdateThread::run() {
  while (!this->Terminating) {
    this->DSMViewerPanel->onDSMWaitForUpdate();
    emit dsmUpdate();
  }
}

