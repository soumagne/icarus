PROJECT("XDMF_UTIL")

CMAKE_MINIMUM_REQUIRED (VERSION 2.6)

IF(COMMAND CMAKE_POLICY)
  CMAKE_POLICY(SET CMP0003 NEW)
ENDIF(COMMAND CMAKE_POLICY)
  
#--------------------------------------------------
# Load ParaView env
#--------------------------------------------------
IF(NOT USE_H5FD_DSM)
  IF(NOT PARAVIEW_LOADED)
    FIND_PACKAGE(ParaView REQUIRED)
    INCLUDE(${PARAVIEW_USE_FILE})
    INCLUDE(${PARAVIEW_VTK_DIR}/VTKConfig.cmake)
    IF (ParaView_BINARY_DIR)
      INCLUDE_DIRECTORIES(
        ${ParaView_CMAKE_DIR}/../Utilities/Xdmf2/libsrc
        ${ParaView_BINARY_DIR}/Utilities/Xdmf2/libsrc
      )
    IF (VTK_USE_SYSTEM_LIBXML2)
      INCLUDE_DIRECTORIES(
        ${LIBXML2_INCLUDE_DIR}
      )
    ELSE (VTK_USE_SYSTEM_LIBXML2)
      INCLUDE_DIRECTORIES(
        ${ParaView_BINARY_DIR}/VTK/Utilities/vtklibxml2
      )
    ENDIF (VTK_USE_SYSTEM_LIBXML2)
    ENDIF(ParaView_BINARY_DIR)
    IF(VTK_USE_MPI)
      SET(USE_MPI 1)
    ENDIF(VTK_USE_MPI)
    SET(PARAVIEW_LOADED 1)
  ELSE(NOT PARAVIEW_LOADED)
    IF (PARAVIEW_BUILD_QT_GUI)
      INCLUDE(${QT_USE_FILE})
    ENDIF (PARAVIEW_BUILD_QT_GUI)
    INCLUDE_DIRECTORIES(
      ${PARAVIEW_INCLUDE_DIRS}
      ${PARAVIEW_KWSYS_INCLUDE_DIRS}
      ${VTK_INCLUDE_DIR}
      ${MPI_INCLUDE_PATH}
      ${ParaView_CMAKE_DIR}/../Utilities/Xdmf2/libsrc
      ${ParaView_BINARY_DIR}/Utilities/Xdmf2/libsrc
    )
    IF(VTK_USE_MPI)
      SET(USE_MPI 1)
    ENDIF(VTK_USE_MPI)
    IF (VTK_USE_SYSTEM_LIBXML2)
      INCLUDE_DIRECTORIES(
        ${LIBXML2_INCLUDE_DIR}
      )
    ELSE (VTK_USE_SYSTEM_LIBXML2)
      INCLUDE_DIRECTORIES(
        ${ParaView_BINARY_DIR}/VTK/Utilities/vtklibxml2
      )
    ENDIF (VTK_USE_SYSTEM_LIBXML2)
  ENDIF(NOT PARAVIEW_LOADED)
  # temporary here but will be removed when next versions of ParaView integrate hdf5-1.8
  FIND_PATH(HDF5_SOURCE_DIR "path to the source directory of HDF5")
  INCLUDE_DIRECTORIES(${HDF5_SOURCE_DIR})

SET(XdmfUtil_SRCS
  ${CMAKE_CURRENT_SOURCE_DIR}/h5dump/h5dump.cxx
  ${CMAKE_CURRENT_SOURCE_DIR}/h5dump/h5tools.cxx
  ${CMAKE_CURRENT_SOURCE_DIR}/h5dump/h5tools_str.cxx
  ${CMAKE_CURRENT_SOURCE_DIR}/h5dump/h5tools_utils.cxx
  ${CMAKE_CURRENT_SOURCE_DIR}/h5dump/h5tools_ref.cxx
  ${CMAKE_CURRENT_SOURCE_DIR}/h5dump/h5tools_type.cxx
  ${CMAKE_CURRENT_SOURCE_DIR}/h5dump/h5trav.cxx
  )

ELSE(NOT USE_H5FD_DSM)
  SET(XdmfUtil_SRCS "")
ENDIF(NOT USE_H5FD_DSM)

INCLUDE_DIRECTORIES(
  ${CMAKE_CURRENT_SOURCE_DIR} 
  ${CMAKE_CURRENT_SOURCE_DIR}/h5dump
  ${CMAKE_CURRENT_BINARY_DIR}
  )

SET(XdmfUtil_SRCS
  ${XdmfUtil_SRCS}
  ${CMAKE_CURRENT_SOURCE_DIR}/XdmfGenerator.cxx
  ${CMAKE_CURRENT_SOURCE_DIR}/XdmfHDFDOM.cxx
  ${CMAKE_CURRENT_SOURCE_DIR}/FileSeriesFinder.cxx
  ${CMAKE_CURRENT_SOURCE_DIR}/XdmfDump.cxx
  )

IF(VTK_BUILD_SHARED_LIBS OR BUILD_SHARED_LIBS)
  SET(XdmfUtil_LIBTYPE SHARED)
ELSE(VTK_BUILD_SHARED_LIBS OR BUILD_SHARED_LIBS)
  SET(XdmfUtil_LIBTYPE STATIC)
ENDIF(VTK_BUILD_SHARED_LIBS OR BUILD_SHARED_LIBS)

CONFIGURE_FILE(
  ${CMAKE_CURRENT_SOURCE_DIR}/XdmfUtilconfig.h.in
  ${CMAKE_CURRENT_BINARY_DIR}/XdmfUtilconfig.h)

ADD_LIBRARY(XdmfUtil ${XdmfUtil_LIBTYPE} ${XdmfUtil_SRCS})
TARGET_LINK_LIBRARIES(XdmfUtil Xdmf vtksys)

ADD_EXECUTABLE(XdmfGenerate ${CMAKE_CURRENT_SOURCE_DIR}/XdmfGenerate.cxx)
TARGET_LINK_LIBRARIES(XdmfGenerate XdmfUtil)

#--------------------------------------------------
# Install
#--------------------------------------------------
INSTALL(
  TARGETS
    XdmfGenerate
  DESTINATION 
    "${CMAKE_INSTALL_PREFIX}/bin"
)

INSTALL(
  TARGETS
    XdmfUtil
  DESTINATION 
    "${CMAKE_INSTALL_PREFIX}/lib/paraview-${PARAVIEW_VERSION_MAJOR}.${PARAVIEW_VERSION_MINOR}"
)



