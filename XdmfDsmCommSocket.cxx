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

#include "XdmfDsmCommSocket.h"
#include "XdmfDsmMsg.h"

#include <cstring>

//----------------------------------------------------------------------------
XdmfDsmCommSocket::XdmfDsmCommSocket()
{
  this->Comm = MPI_COMM_WORLD;
  this->CommType = XDMF_DSM_COMM_SOCKET;
  this->InterComm = NULL;
  this->DsmMasterSocket = NULL;
  this->CommChannel  = XDMF_DSM_COMM_CHANNEL_LOCAL;
}

//----------------------------------------------------------------------------
XdmfDsmCommSocket::~XdmfDsmCommSocket()
{
  if (this->DsmMasterSocket) delete this->DsmMasterSocket;
  this->DsmMasterSocket = NULL;
}
//----------------------------------------------------------------------------
void
XdmfDsmCommSocket::SetDsmMasterHostName(const char *hostName) {
    strcpy(this->DsmMasterHostName, hostName);
}
//----------------------------------------------------------------------------
XdmfInt32
XdmfDsmCommSocket::DupComm(MPI_Comm Source)
{
  MPI_Comm    NewComm;

  MPI_Comm_dup(Source, &NewComm);
  return(this->SetComm(NewComm));
}
//----------------------------------------------------------------------------
XdmfInt32
XdmfDsmCommSocket::Init()
{
  int size, rank;

  if (MPI_Comm_size(this->Comm, &size) != MPI_SUCCESS) return(XDMF_FAIL);
  if (MPI_Comm_rank(this->Comm, &rank) != MPI_SUCCESS) return(XDMF_FAIL);

  this->SetId(rank);
  this->SetTotalSize(size);

  if (this->Id == 0) {
    this->DsmMasterSocket = new XdmfDsmSocket();
    if (this->DsmMasterSocket->Create() == XDMF_FAIL) {
      XdmfErrorMessage("Creating socket for joining communicators failed");
      return(XDMF_FAIL);
    }
  }
  this->Barrier();

  return(XDMF_SUCCESS);
}
//----------------------------------------------------------------------------
XdmfInt32
XdmfDsmCommSocket::Check(XdmfDsmMsg *Msg)
{
  int         nid, flag=0;
  MPI_Status  Status;

  if (XdmfDsmComm::Check(Msg) != XDMF_SUCCESS) return(XDMF_FAIL);
  if (this->CommChannel == XDMF_DSM_COMM_CHANNEL_LOCAL) {
    MPI_Iprobe(MPI_ANY_SOURCE, Msg->Tag, this->Comm, &flag, &Status);
    if (flag) {
      nid = Status.MPI_SOURCE;
      Msg->SetSource(nid);
      return(XDMF_SUCCESS);
    }
  }
  return(XDMF_FAIL);
}
//----------------------------------------------------------------------------
XdmfInt32
XdmfDsmCommSocket::Receive(XdmfDsmMsg *Msg)
{
  int         MessageLength;
  XdmfInt32   status;
  XdmfInt32   source = MPI_ANY_SOURCE;
  MPI_Status  SendRecvStatus;

  if (XdmfDsmComm::Receive(Msg) != XDMF_SUCCESS) return(XDMF_FAIL);
  if (Msg->Source >= 0) source = Msg->Source;

  if (this->CommChannel == XDMF_DSM_COMM_CHANNEL_REMOTE) {
    XdmfDebug("::::: (" << this->Id << ") Receiving from remote DSM " << Msg->Length << " bytes from " << source << " Tag = " << Msg->Tag);
    this->InterComm[source]->Receive(Msg->Data, Msg->Length);
    // if ANY_SOURCE use select on the whole list of sockets descriptors
  }
  else {
    XdmfDebug("::::: (" << this->Id << ") Receiving " << Msg->Length << " bytes from " << source << " Tag = " << Msg->Tag);
    status = MPI_Recv(Msg->Data, Msg->Length, MPI_UNSIGNED_CHAR, source, Msg->Tag, this->Comm, &SendRecvStatus);

    if (status != MPI_SUCCESS) {
      XdmfErrorMessage("Id = " << this->Id << " MPI_Recv failed to receive " << Msg->Length << " Bytes from " << Msg->Source);
      XdmfErrorMessage("MPI Error Code = " << SendRecvStatus.MPI_ERROR);
      return(XDMF_FAIL);
    }
    status = MPI_Get_count(&SendRecvStatus, MPI_UNSIGNED_CHAR, &MessageLength);
    XdmfDebug("::::: (" << this->Id << ") Received " << MessageLength << " bytes from " << SendRecvStatus.MPI_SOURCE);

    Msg->SetSource(SendRecvStatus.MPI_SOURCE);
    Msg->SetLength(MessageLength);
    if (status != MPI_SUCCESS) {
      XdmfErrorMessage("MPI_Get_count failed ");
      return(XDMF_FAIL);
    }
  }
  return(XDMF_SUCCESS);
}
//----------------------------------------------------------------------------
XdmfInt32
XdmfDsmCommSocket::Send(XdmfDsmMsg *Msg)
{
  XdmfInt32   status;

  if (XdmfDsmComm::Send(Msg) != XDMF_SUCCESS) return(XDMF_FAIL);

  if (this->CommChannel == XDMF_DSM_COMM_CHANNEL_REMOTE) {
    XdmfDebug("::::: (" << this->Id << ") Sending to remote DSM " << Msg->Length << " bytes to " << Msg->Dest << " Tag = " << Msg->Tag);
    this->InterComm[Msg->Dest]->Send(Msg->Data, Msg->Length);
  }
  else {
    XdmfDebug("::::: (" << this->Id << ") Sending " << Msg->Length << " bytes to " << Msg->Dest << " Tag = " << Msg->Tag);
    status = MPI_Send(Msg->Data, Msg->Length, MPI_UNSIGNED_CHAR, Msg->Dest, Msg->Tag, this->Comm);

    if (status != MPI_SUCCESS) {
      XdmfErrorMessage("Id = " << this->Id << " MPI_Send failed to send " << Msg->Length << " Bytes to " << Msg->Dest);
      return(XDMF_FAIL);
    }
    XdmfDebug("::::: (" << this->Id << ") Sent " << Msg->Length << " bytes to " << Msg->Dest);
  }
  return(XDMF_SUCCESS);
}
//----------------------------------------------------------------------------
XdmfInt32
XdmfDsmCommSocket::Barrier()
{
  if (XdmfDsmComm::Barrier() != XDMF_SUCCESS) return(XDMF_FAIL);
  MPI_Barrier(this->Comm);

  return(XDMF_SUCCESS);
}
//----------------------------------------------------------------------------
XdmfInt32
XdmfDsmCommSocket::OpenPort()
{
  if (XdmfDsmComm::OpenPort() != XDMF_SUCCESS) return(XDMF_FAIL);
  if (this->Id == 0) {
    if (this->DsmMasterSocket->Bind(this->DsmMasterPort, this->DsmMasterHostName) == XDMF_FAIL) {
      XdmfErrorMessage("Binding socket for joining communicators failed");
      return(XDMF_FAIL);
    }
    this->DsmMasterSocket->Listen();
  }
  this->Barrier();

  return(XDMF_SUCCESS);
}
//----------------------------------------------------------------------------
XdmfInt32
XdmfDsmCommSocket::ClosePort()
{
  if (XdmfDsmComm::ClosePort() != XDMF_SUCCESS) return(XDMF_FAIL);
  return(XDMF_SUCCESS);
}
//----------------------------------------------------------------------------
XdmfInt32
XdmfDsmCommSocket::RemoteCommAccept()
{
  if (XdmfDsmComm::RemoteCommAccept() != XDMF_SUCCESS) return(XDMF_FAIL);

  // Needed if we want to insert a timeout
  // if (this->MasterSocket->Select(100) <= 0 ) return(XDMF_FAIL);

  if (this->Id == 0) {
    if (this->DsmMasterSocket->Accept() == XDMF_FAIL) {
      XdmfErrorMessage("Accept socket failed");
      return(XDMF_FAIL);
    }
  }
  this->Barrier();

  return(XDMF_SUCCESS);
}
//----------------------------------------------------------------------------
XdmfInt32
XdmfDsmCommSocket::RemoteCommReset()
{
  if (XdmfDsmComm::RemoteCommReset() != XDMF_SUCCESS) return(XDMF_FAIL);
  // needed ??

  return(XDMF_SUCCESS);
}
//----------------------------------------------------------------------------
XdmfInt32
XdmfDsmCommSocket::RemoteCommConnect()
{
  if (XdmfDsmComm::RemoteCommConnect() != XDMF_SUCCESS) return(XDMF_FAIL);

  if (this->Id == 0) {
    if (this->DsmMasterSocket->Connect(this->DsmMasterHostName, this->DsmMasterPort) == XDMF_FAIL) {
      XdmfErrorMessage("Socket connection failed");
      return(XDMF_FAIL);
    }
  }
  this->Barrier();

  return(XDMF_SUCCESS);
}
//----------------------------------------------------------------------------
XdmfInt32
XdmfDsmCommSocket::RemoteCommDisconnect()
{
  if (XdmfDsmComm::RemoteCommDisconnect() != XDMF_SUCCESS) return(XDMF_FAIL);
  if (this->DsmMasterSocket) delete this->DsmMasterSocket;
  this->DsmMasterSocket = NULL;

  return(XDMF_SUCCESS);
}
//----------------------------------------------------------------------------
XdmfInt32
XdmfDsmCommSocket::RemoteCommRecvInfo(XdmfInt64 *length, XdmfInt64 *totalLength,
    XdmfInt32 *startServerId, XdmfInt32 *endServerId)
{
  if (XdmfDsmComm::RemoteCommRecvInfo(length, totalLength, startServerId, endServerId) != XDMF_SUCCESS) return(XDMF_FAIL);

  if (this->Id == 0) {
    this->InterComm[0]->Receive(length, sizeof(XdmfInt64));
    XdmfDebug("Recv DSM length: " << *length);
  }
  if (MPI_Bcast(length, 1, MPI_LONG_LONG, 0, this->Comm) != MPI_SUCCESS) {
    XdmfErrorMessage("Id = " << this->Id << " MPI_Bcast of length failed");
    return(XDMF_FAIL);
  }

  if (this->Id == 0) {
    this->InterComm[0]->Receive(totalLength, sizeof(XdmfInt64));
    XdmfDebug("Recv DSM totalLength: " << *totalLength);
  }
  if (MPI_Bcast(totalLength, 1, MPI_LONG_LONG, 0, this->Comm) != MPI_SUCCESS) {
    XdmfErrorMessage("Id = " << this->Id << " MPI_Bcast of totalLength failed");
    return(XDMF_FAIL);
  }

  if (this->Id == 0) {
    this->InterComm[0]->Receive(startServerId, sizeof(XdmfInt32));
    XdmfDebug("Recv DSM startServerId: " << *startServerId);
  }
  if (MPI_Bcast(startServerId, 1, MPI_INT, 0, this->Comm) != MPI_SUCCESS) {
    XdmfErrorMessage("Id = " << this->Id << " MPI_Bcast of startServerId failed");
    return(XDMF_FAIL);
  }

  if (this->Id == 0) {
    this->InterComm[0]->Receive(endServerId, sizeof(XdmfInt32));
    XdmfDebug("Recv DSM endServerId: " << *endServerId);
  }
  if (MPI_Bcast(endServerId, 1, MPI_INT, 0, this->Comm) != MPI_SUCCESS) {
    XdmfErrorMessage("Id = " << this->Id << " MPI_Bcast of endServerId failed");
    return(XDMF_FAIL);
  }
  XdmfDebug("Recv DSM Info Completed");

  return(XDMF_SUCCESS);
}
//----------------------------------------------------------------------------
XdmfInt32
XdmfDsmCommSocket::RemoteCommSendInfo(XdmfInt64 *length, XdmfInt64 *totalLength,
    XdmfInt32 *startServerId, XdmfInt32 *endServerId)
{
  if (XdmfDsmComm::RemoteCommSendInfo(length, totalLength, startServerId, endServerId) != XDMF_SUCCESS) return(XDMF_FAIL);

  if (this->Id == 0) {
    // Length
    XdmfDebug("Send DSM length: " << *length);
    this->InterComm[0]->Send(length, sizeof(XdmfInt64));

    // TotalLength
    XdmfDebug("Send DSM totalLength: " << *totalLength);
    this->InterComm[0]->Send(totalLength, sizeof(XdmfInt64));

    // StartServerId
    XdmfDebug("Send DSM startServerId: " << *startServerId);
    this->InterComm[0]->Send(startServerId, sizeof(XdmfInt32));

    // EndServerId
    XdmfDebug("Send DSM endServerId: " << *endServerId);
    this->InterComm[0]->Send(endServerId, sizeof(XdmfInt32));
    XdmfDebug("Send DSM Info Completed");
  }
  this->Barrier();
  return(XDMF_SUCCESS);
}
//----------------------------------------------------------------------------
XdmfInt32
XdmfDsmCommSocket::RemoteCommSendXML(XdmfString file)
{
  if (XdmfDsmComm::RemoteCommSendXML(file) != XDMF_SUCCESS) return(XDMF_FAIL);

  XdmfInt32 length = strlen(file) + 1;
  XdmfDebug("Send XML to DSM Buffer object - FilePath: " << file);
  this->InterComm[0]->Send(&length, sizeof(XdmfInt32));
  this->InterComm[0]->Send(file, sizeof(XdmfString)*length);
  XdmfDebug("Send DSM XML Completed");
  return(XDMF_SUCCESS);
}
//----------------------------------------------------------------------------
XdmfInt32
XdmfDsmCommSocket::RemoteCommRecvXML(XdmfString *file)
{
  if (XdmfDsmComm::RemoteCommRecvXML(file) != XDMF_SUCCESS) return(XDMF_FAIL);

  XdmfInt32 length;
  this->InterComm[0]->Receive(&length, sizeof(XdmfInt32));
  *file = new char[length];
  this->InterComm[0]->Receive(file, sizeof(XdmfString)*length);
  XdmfDebug("Recv DSM XML: " << *file);
  return(XDMF_SUCCESS);
}
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
