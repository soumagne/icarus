/*=========================================================================

  Project                 : vtkCSCS
  Module                  : XdmfDsmCommSocket.h
  Revision of last commit : $Rev$
  Author of last commit   : $Author$
  Date of last commit     : $Date::                            $

  Copyright (C) CSCS - Swiss National Supercomputing Centre.
  You may use modify and and distribute this code freely providing
  1) This copyright notice appears on all copies of source code
  2) An acknowledgment appears with any substantial usage of the code
  3) If this code is contributed to any other open source project, it
  must not be reformatted such that the indentation, bracketing or
  overall style is modified significantly.

  This software is distributed WITHOUT ANY WARRANTY; without even the
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

=========================================================================*/

#ifndef XDMFDSMCOMMSOCKET_H
#define XDMFDSMCOMMSOCKET_H

#define XDMF_DSM_MAX_SOCKET 256

#include "XdmfDsmComm.h"
#include "XdmfDsmSocket.h"

#include <mpi.h>

class XDMF_EXPORT XdmfDsmCommSocket : public XdmfDsmComm {

public:
  XdmfDsmCommSocket();
  ~XdmfDsmCommSocket();

  XdmfConstString GetClassName() { return ( "XdmfDsmCommSocket" ) ; };

  // Set/Get the Internal MPI Communicator
  XdmfSetValueMacro(Comm, MPI_Comm);
  XdmfGetValueMacro(Comm, MPI_Comm);

  XdmfGetStringMacro(DsmMasterHostName);
  void SetDsmMasterHostName(XdmfConstString hostName);

  XdmfGetValueMacro(DsmMasterPort, XdmfInt32);
  XdmfSetValueMacro(DsmMasterPort, XdmfInt32);

  XdmfInt32   DupComm(MPI_Comm Source);

  XdmfInt32   Init();
  XdmfInt32   Send(XdmfDsmMsg *Msg);
  XdmfInt32   Receive(XdmfDsmMsg *Msg);
  XdmfInt32   Check(XdmfDsmMsg *Msg);
  XdmfInt32   Barrier();

  XdmfInt32   OpenPort();
  XdmfInt32   ClosePort();
  XdmfInt32   RemoteCommAccept();
  XdmfInt32   RemoteCommConnect();
  XdmfInt32   RemoteCommDisconnect();

  XdmfInt32   RemoteCommRecvInfo(XdmfInt64 *length, XdmfInt64 *totalLength,
      XdmfInt32 *startServerId, XdmfInt32 *endServerId);
  XdmfInt32   RemoteCommSendInfo(XdmfInt64 *length, XdmfInt64 *totalLength,
      XdmfInt32 *startServerId, XdmfInt32 *endServerId);

  XdmfInt32   RemoteCommSendXML(XdmfString file);
  XdmfInt32   RemoteCommRecvXML(XdmfString *file);

protected:
  XdmfInt32   InterCommServerConnect();
  XdmfInt32   InterCommClientConnect();

  MPI_Comm             Comm;
  XdmfDsmSocket       *InterComm[XDMF_DSM_MAX_SOCKET]; // Internode Socket Collection for data exchange
  XdmfInt32            InterSize;
  XdmfDsmSocket       *DsmMasterSocket; // Used for initializing connection and send comm orders
  XdmfByte             DsmMasterHostName[MPI_MAX_PORT_NAME];
  XdmfInt32            DsmMasterPort;
};

#endif /* XDMFDSMCOMMSOCKET_H */
