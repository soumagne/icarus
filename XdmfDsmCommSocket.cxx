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
  for (int i=0; i<XDMF_DSM_MAX_SOCKET; i++) this->InterComm[i] = NULL;
  this->DsmMasterSocket = NULL;
  this->CommChannel  = XDMF_DSM_COMM_CHANNEL_LOCAL;
  this->DsmMasterPort = 0;
}

//----------------------------------------------------------------------------
XdmfDsmCommSocket::~XdmfDsmCommSocket()
{
  for (int i=0; i<XDMF_DSM_MAX_SOCKET ;i++) {
     if (this->InterComm[i]) delete this->InterComm[i];
     this->InterComm[i] = NULL;
   }
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
      XdmfErrorMessage("Create DsmMasterSocket failed");
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
    if (source >= 0) {
      this->InterComm[source]->Receive(Msg->Data, Msg->Length);
    } else {
      // TODO when modifying then dynamically the socket array, should be careful not to change it
      // while doing a select on it
      int selectedIndex;
      int *socketsToSelect = new int[this->InterSize];
      for (int i=0; i<this->InterSize; i++) {
        socketsToSelect[i] = this->InterComm[i]->GetClientSocketDescriptor();
      }
      // if ANY_SOURCE use select on the whole list of sockets descriptors
      this->InterComm[0]->SelectSockets(socketsToSelect, this->InterSize, 0, &selectedIndex);
      this->InterComm[selectedIndex]->Receive(Msg->Data, Msg->Length);
      delete []socketsToSelect;
    }
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
    // Take this port as default for now and let Bind decide about hostname
    if (this->DsmMasterSocket->Bind(22000) == XDMF_FAIL) {
      XdmfErrorMessage("Bind DsmMasterSocket failed");
      return(XDMF_FAIL);
    }
    this->DsmMasterPort = this->DsmMasterSocket->GetPort();
    strcpy(this->DsmMasterHostName, this->DsmMasterSocket->GetHostName());
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
  // close MasterSocket
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
    this->DsmMasterSocket->Receive(&this->InterSize, sizeof(int));
    this->DsmMasterSocket->Send(&this->TotalSize, sizeof(int));
  }
  if (MPI_Bcast(&this->InterSize, 1, MPI_INT, 0, this->Comm) != MPI_SUCCESS) {
    XdmfErrorMessage("Id = " << this->Id << " MPI_Bcast of InterSize failed");
    return(XDMF_FAIL);
  }

  if (this->InterCommServerConnect() != XDMF_SUCCESS) {
    XdmfErrorMessage("Id = " << this->Id << " Error in InterCommServerConnect");
    return(XDMF_FAIL);
  }

  this->CommChannel = XDMF_DSM_COMM_CHANNEL_REMOTE;

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
    this->DsmMasterSocket->Send(&this->TotalSize, sizeof(int));
    this->DsmMasterSocket->Receive(&this->InterSize, sizeof(int));
  }
  if (MPI_Bcast(&this->InterSize, 1, MPI_INT, 0, this->Comm) != MPI_SUCCESS){
    XdmfErrorMessage("Id = " << this->Id << " MPI_Bcast of InterSize failed");
    return(XDMF_FAIL);
  }

  if (this->InterCommClientConnect() != XDMF_SUCCESS) {
    XdmfErrorMessage("Id = " << this->Id << " Error in InterCommClientConnect");
    return(XDMF_FAIL);
  }

  this->CommChannel = XDMF_DSM_COMM_CHANNEL_REMOTE;
  return(XDMF_SUCCESS);
}
//----------------------------------------------------------------------------
XdmfInt32
XdmfDsmCommSocket::RemoteCommDisconnect()
{
  if (XdmfDsmComm::RemoteCommDisconnect() != XDMF_SUCCESS) return(XDMF_FAIL);
  for (int i=0; i<XDMF_DSM_MAX_SOCKET; i++) {
    if (this->InterComm[i]) delete this->InterComm[i];
    this->InterComm[i] = NULL;
  }
  this->CommChannel = XDMF_DSM_COMM_CHANNEL_LOCAL;
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
XdmfInt32
XdmfDsmCommSocket::InterCommServerConnect()
{
  char *interCommHostName = NULL;
  char *masterInterCommHostName = NULL;
  int  *interCommPort = NULL;
  int  *masterInterCommPort = NULL;

  interCommHostName = new char[this->InterSize*MPI_MAX_PORT_NAME];
  masterInterCommHostName = new char[(this->InterSize)*(this->TotalSize)*MPI_MAX_PORT_NAME];
  interCommPort = new int[this->InterSize];
  masterInterCommPort = new int[(this->InterSize)*(this->TotalSize)];

  XdmfDebug("(" << this->Id << ") Creating " << this->InterSize << " sockets for intercomm");
  for(int sock=0; sock<(this->InterSize); sock++) {
    int bindPort = 23000 + sock * this->Id;
    this->InterComm[sock] = new XdmfDsmSocket();
    this->InterComm[sock]->Create();
    while (1) {
      if (this->InterComm[sock]->Bind(bindPort) < 0) {
        bindPort++;
      } else {
        break;
      }
    }
    strcpy(&interCommHostName[sock*MPI_MAX_PORT_NAME], this->InterComm[sock]->GetHostName());
    interCommPort[sock] = this->InterComm[sock]->GetPort();
    this->InterComm[sock]->Listen();
  }

  // Gather socket info
  MPI_Gather(interCommHostName, this->InterSize*MPI_MAX_PORT_NAME, MPI_CHAR,
      masterInterCommHostName, this->InterSize*MPI_MAX_PORT_NAME, MPI_CHAR, 0, this->Comm);
  MPI_Gather(interCommPort, this->InterSize, MPI_INT,
        masterInterCommPort, this->InterSize, MPI_INT, 0, this->Comm);

  // Send it then to interComm 0
  if (this->Id == 0) {
    for (int i=0; i<((this->InterSize)*(this->TotalSize)); i++) {
      char tmpHost[MPI_MAX_PORT_NAME];
      int tmpPort = masterInterCommPort[i];
      strncpy(tmpHost, masterInterCommHostName+i*MPI_MAX_PORT_NAME, MPI_MAX_PORT_NAME);
      XdmfDebug("Send info for intercomm socket on " << i << " to " << tmpHost << ":" << tmpPort);
    }
    // Send masterInterCommHostName
    this->DsmMasterSocket->Send(masterInterCommHostName,
        (this->InterSize)*(this->TotalSize)*MPI_MAX_PORT_NAME*sizeof(char));
    // Send masterInterCommPort
    this->DsmMasterSocket->Send(masterInterCommPort,
            (this->InterSize)*(this->TotalSize)*sizeof(int));
  }
  //
  // Accept
  for(int sock=0; sock<(this->InterSize); sock++) {
    this->InterComm[sock]->Accept();
  }
  this->Barrier();
  //
  delete[] interCommHostName;
  delete[] masterInterCommHostName;
  delete[] interCommPort;
  delete[] masterInterCommPort;

  XdmfDebug("Cleaned well interCommHostName/masterInterCommHostName");
  return XDMF_SUCCESS;
}
//----------------------------------------------------------------------------
XdmfInt32
XdmfDsmCommSocket::InterCommClientConnect()
{
  char *interCommHostName = NULL;
  char *masterInterCommHostName = NULL;
  int  *interCommPort = NULL;
  int  *masterInterCommPort = NULL;

  interCommHostName = new char[this->InterSize*MPI_MAX_PORT_NAME];
  masterInterCommHostName = new char[(this->InterSize)*(this->TotalSize)*MPI_MAX_PORT_NAME];
  interCommPort = new int[this->InterSize];
  masterInterCommPort = new int[(this->InterSize)*(this->TotalSize)];

  if (this->Id == 0) {
    // Send masterInterCommHostName
    this->DsmMasterSocket->Receive(masterInterCommHostName,
        (this->InterSize)*(this->TotalSize)*MPI_MAX_PORT_NAME*sizeof(char));
    // Send masterInterCommPort
    this->DsmMasterSocket->Receive(masterInterCommPort,
            (this->InterSize)*(this->TotalSize)*sizeof(int));

    for (int i=0; i<((this->InterSize)*(this->TotalSize)); i++) {
      if (i%(this->TotalSize) == 0) {
        strncpy(interCommHostName+(i/this->TotalSize)*MPI_MAX_PORT_NAME,
            masterInterCommHostName+i*MPI_MAX_PORT_NAME, MPI_MAX_PORT_NAME);
        interCommPort[i/this->TotalSize] = masterInterCommPort[i];
      } else {
        char tmpHost[MPI_MAX_PORT_NAME];
        int tmpPort = masterInterCommPort[i];
        strncpy(tmpHost, masterInterCommHostName+i*MPI_MAX_PORT_NAME, MPI_MAX_PORT_NAME);
        //XdmfDebug("Receive info for intercomm socket on " << i << " to " << tmpHost << ":" << tmpPort);
        MPI_Send(tmpHost, MPI_MAX_PORT_NAME, MPI_CHAR, i%(this->TotalSize), 0, this->Comm);
        MPI_Send(&tmpPort, 1, MPI_INT, i%(this->TotalSize), 0, this->Comm);
      }
    }
  } else {
    for (int i=0; i<this->InterSize; i++) {
      MPI_Recv((interCommHostName+i*MPI_MAX_PORT_NAME), MPI_MAX_PORT_NAME, MPI_CHAR, 0, 0, this->Comm, MPI_STATUS_IGNORE);
      MPI_Recv((interCommPort+i), 1, MPI_INT, 0, 0, this->Comm, MPI_STATUS_IGNORE);
    }
  }
  this->Barrier();

  XdmfDebug("(" << this->Id << ") Creating " << this->InterSize << " sockets for intercomm");
  for(int sock=0; sock<(this->InterSize); sock++) {
    char tmpHost[MPI_MAX_PORT_NAME];
    int tmpPort = interCommPort[sock];
    strncpy(tmpHost, interCommHostName+sock*MPI_MAX_PORT_NAME, MPI_MAX_PORT_NAME);
    this->InterComm[sock] = new XdmfDsmSocket();
    this->InterComm[sock]->Create();
    // Connect
    XdmfDebug("Connecting intercomm socket " << sock << " on " << this->Id << " to " << tmpHost << ":" << tmpPort);
    this->InterComm[sock]->Connect(tmpHost, tmpPort);
  }
  this->Barrier();
  //
  delete[] interCommHostName;
  delete[] masterInterCommHostName;
  delete[] interCommPort;
  delete[] masterInterCommPort;

  XdmfDebug("Cleaned well interCommHostName/masterInterCommHostName");

  return XDMF_SUCCESS;
}
//----------------------------------------------------------------------------
