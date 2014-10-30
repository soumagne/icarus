#pragma once

#include <string>
#include "IDType.h"
#ifdef BONSAI_CATALYST_STDLIB
 #include <boost/lexical_cast.hpp>
 #define bonsaistd boost
 #define to_string boost::lexical_cast<std::string>
#else
 #define to_string std::to_string
#endif

struct BonsaiSharedHeader
{
  float tCurrent;
  size_t nBodies;
  char fileName[256];
  bool handshake;
  bool done_writing;
};
  
struct BonsaiSharedData
{
  IDType ID;
  float x,y,z,mass;
  float vx,vy,vz,vw;
  float rho,h;
};

struct BonsaiSharedQuickHeader : public BonsaiSharedHeader
{
  static const char* sharedFile(const int rank)
  {
    const std::string fn = "/BonsaiQuickHeader-"+to_string(rank);
    return fn.c_str();
  }
};

struct BonsaiSharedQuickData : public BonsaiSharedData
{
  static const char* sharedFile(const int rank)
  {
    const std::string fn = "/BonsaiQuickData-"+to_string(rank);
    return fn.c_str();
  }
};

struct BonsaiSharedSnapHeader : public BonsaiSharedHeader
{
  static const char* sharedFile(const int rank)
  {
    const std::string fn = "/BonsaiSnapHeader-"+to_string(rank);
    return fn.c_str();
  }
};

struct BonsaiSharedSnapData : public BonsaiSharedData
{
  static const char* sharedFile(const int rank)
  {
    const std::string fn = "/BonsaiSnapData-"+to_string(rank);
    return fn.c_str();
  }
};
