PROJECT("ICARUS")

CMAKE_MINIMUM_REQUIRED (VERSION 2.6)

IF(COMMAND CMAKE_POLICY)
  CMAKE_POLICY(SET CMP0003 NEW)
ENDIF(COMMAND CMAKE_POLICY)

IF(WIN32 AND MSVC)
  ADD_DEFINITIONS(-D_CRT_SECURE_NO_WARNINGS)
ENDIF(WIN32 AND MSVC)

SET(PLUGIN_NAME ${PROJECT_NAME})
#--------------------------------------------------
# Find and Use ParaView
#--------------------------------------------------
SET(QT_USE_QTNETWORK TRUE)
IF (ParaView_SOURCE_DIR)
  IF(PARAVIEW_BUILD_QT_GUI)
    INCLUDE(${QT_USE_FILE})
  ENDIF(PARAVIEW_BUILD_QT_GUI)
  INCLUDE_DIRECTORIES(
    ${HDF5_INCLUDE_DIR}
    ${PARAVIEW_INCLUDE_DIRS}
    ${PARAVIEW_GUI_INCLUDE_DIRS}
    ${VTK_INCLUDE_DIR}
  )
ELSE (ParaView_SOURCE_DIR)
  FIND_PACKAGE(ParaView REQUIRED)
  INCLUDE(${PARAVIEW_USE_FILE})
ENDIF (ParaView_SOURCE_DIR)
#INCLUDE(${ParaView_BINARY_DIR}/Utilities/Xdmf2/XDMFConfig.cmake)
#INCLUDE_DIRECTORIES(${XDMF_MPI_INCLUDE_PATH} ${XDMF_LIBXML2_INCLUDE})

#--------------------------------------------------
# stdlibc++
#--------------------------------------------------
OPTION(ICARUS_NO_LIBCXX "Use boost instead of libc++" OFF)
IF(ICARUS_NO_LIBCXX)
  add_definitions(-DICARUS_NO_LIBCXX)
  add_definitions(-DDSM_CXX_STDLIB)
  find_package(Boost REQUIRED)
  include_directories(${Boost_INCLUDE_DIRS})
ENDIF()

#--------------------------------------------------
# Timers
#--------------------------------------------------
OPTION(ICARUS_ENABLE_TIMERS "Enable timing of operations." OFF)
IF(ICARUS_ENABLE_TIMERS)
  SET(ENABLE_TIMERS 1)
ENDIF(ICARUS_ENABLE_TIMERS)

option(ICARUS_USE_H5FDDSM "Build H5FDdsm support into plugin" OFF)
option(ICARUS_USE_BONSAI "Build Bonsai support into plugin" OFF)

#--------------------------------------------------
# Find and Use System libs (only needed on WIN32)
#--------------------------------------------------
#IF(WIN32 AND MSVC)
  IF (NOT TARGET "hdf5")
    FIND_PACKAGE(HDF5 REQUIRED NO_MODULE PATHS "$ENV{ProgramFiles}/HDF5")
    INCLUDE_DIRECTORIES(${HDF5_INCLUDE_DIR})
  ENDIF (NOT TARGET "hdf5")
  #--------------------------------------------------
  IF (NOT TARGET "H5FDdsm")
    FIND_PACKAGE(H5FDdsm REQUIRED NO_MODULE PATHS "$ENV{ProgramFiles}/H5FDdsm")
    INCLUDE_DIRECTORIES(${H5FDdsm_INCLUDE_DIR})
  ENDIF (NOT TARGET "H5FDdsm")
#--------------------------------------------------
#ENDIF(WIN32 AND MSVC)

if(ICARUS_USE_BONSAI)
  add_definitions(-DICARUS_HAVE_BONSAI)
  add_definitions(-DBONSAI_CATALYST_STDLIB)
  add_definitions(-DBONSAI_CATALYST_CLANG)
  include_directories(${CMAKE_CURRENT_SOURCE_DIR}/bonsai)
  set(ICARUS_H5FDDSM_SM_XML
    ${CMAKE_CURRENT_SOURCE_DIR}/bonsai/vtkBonsaiDsmManager.xml
    ${CMAKE_CURRENT_SOURCE_DIR}/bonsai/vtkBonsaiSharedMemoryReader.xml
  )
  set(ICARUS_H5FDDSM_SM_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/bonsai/vtkBonsaiDsmManager.cxx
    ${CMAKE_CURRENT_SOURCE_DIR}/bonsai/vtkBonsaiSharedMemoryReader.cxx
  )
  set(ICARUS_SERVER_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/bonsai/RendererData.cpp
  )
  set(ICARUS_H5FDDSM_QT_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/bonsai/pqBonsaiDsmPanel.cxx
  )
  set(ICARUS_H5FDDSM_UI_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/bonsai/pqBonsaiDsmPanel.ui
  )
  set(ICARUS_H5FDDSM_MOC_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/bonsai/pqBonsaiDsmPanel.h
  )
  set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/bonsai/RendererData.cpp PROPERTIES COMPILE_FLAGS "-std=c++11")
  set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/bonsai/vtkBonsaiSharedMemoryReader.cxx PROPERTIES COMPILE_FLAGS "-std=c++11")
endif()

if(ICARUS_USE_H5FDDSM)

add_definitions(-DICARUS_HAVE_H5FDDSM)
#--------------------------------------------------
# DSM H5Part Reader needs pv-meshless
#--------------------------------------------------
OPTION(ICARUS_USE_H5PART "Build H5Part DSM reader." OFF)
OPTION(ICARUS_USE_NETCDF "Build NetCDF DSM reader." OFF)

IF(ICARUS_USE_NETCDF)
  LINK_DIRECTORIES(C:/cmakebuild/netcdf/bin)
  INCLUDE_DIRECTORIES(
    "C:/code/netcdf-4.1.3/include"
    "C:/code/netcdf-4.1.3/cxx"
    "C:/code/netcdf-4.1.3/libsrc"
    "C:/code/netcdf-4.1.3/libsrc4"
    )
  SET(ICARUS_NETCDF_XML ${CMAKE_CURRENT_SOURCE_DIR}/vtkNetCDFCFDsmReader.xml)
  SET(ICARUS_NETCDF_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/vtkNetCDFCFDsmReader.cxx)
  SET(ICARUS_NETCDF_LIBS 
     netCDF 
  )
ENDIF(ICARUS_USE_NETCDF)

IF(ICARUS_USE_H5PART)
  IF(NOT pv-meshless_SOURCE_DIR)
    IF(EXISTS ${CSCS_Plugins_SOURCE_DIR}/pv-meshless)
      SET(pv-meshless_SOURCE_DIR ${CSCS_Plugins_SOURCE_DIR}/pv-meshless)
    ELSE(EXISTS ${CSCS_Plugins_SOURCE_DIR}/pv-meshless)
      IF(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../pv-meshless)
        SET(pv-meshless_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../pv-meshless)
      ELSE(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../pv-meshless)
        FIND_PATH(pv-meshless_SOURCE_DIR vtkH5PartReader.h)
      ENDIF(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../pv-meshless)
    ENDIF(EXISTS ${CSCS_Plugins_SOURCE_DIR}/pv-meshless)
  ENDIF(NOT pv-meshless_SOURCE_DIR)

  INCLUDE_DIRECTORIES(${pv-meshless_SOURCE_DIR} ${pv-meshless_SOURCE_DIR}/h5part)

  SET(ICARUS_H5PART_XML ${CMAKE_CURRENT_SOURCE_DIR}/vtkH5PartDsmReader.xml)
  SET(ICARUS_H5PART_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/vtkH5PartDsmReader.cxx)
ENDIF(ICARUS_USE_H5PART)

#--------------------------------------------------
# Set project include directories 
#--------------------------------------------------
INCLUDE_DIRECTORIES(
  ${CMAKE_CURRENT_SOURCE_DIR}/XdmfGenerator
  ${CMAKE_CURRENT_BINARY_DIR}/XdmfGenerator
  )

endif()

#--------------------------------------------------
# Config Files for options
#--------------------------------------------------
CONFIGURE_FILE(
  ${CMAKE_CURRENT_SOURCE_DIR}/IcarusConfig.h.in
  ${CMAKE_CURRENT_BINARY_DIR}/IcarusConfig.h)

if(ICARUS_USE_H5FDDSM)
  set(ICARUS_H5FDDSM_SM_XML
    ${CMAKE_CURRENT_SOURCE_DIR}/vtkHDF5DsmManager.xml
    ${CMAKE_CURRENT_SOURCE_DIR}/vtkXdmfReader4.xml
    ${CMAKE_CURRENT_SOURCE_DIR}/vtkFlattenOneBlock.xml
    ${CMAKE_CURRENT_SOURCE_DIR}/vtkSteeringWriter.xml
    ${CMAKE_CURRENT_SOURCE_DIR}/vtkDsmTableReader.xml
  )
  set(ICARUS_H5FDDSM_SM_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/vtkHDF5DsmManager.cxx
    #${CMAKE_CURRENT_SOURCE_DIR}/vtkXdmfWriter4.cxx
    ${CMAKE_CURRENT_SOURCE_DIR}/vtkXdmfReader4.cxx
    ${CMAKE_CURRENT_SOURCE_DIR}/vtkFlattenOneBlock.cxx
    ${CMAKE_CURRENT_SOURCE_DIR}/vtkSteeringWriter.cxx
    ${CMAKE_CURRENT_SOURCE_DIR}/vtkDsmTableReader.cxx
  )
  set(ICARUS_H5FDDSM_QT_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/pqH5FDdsmPanel.cxx
  )
  set(ICARUS_H5FDDSM_UI_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/pqH5FDdsmPanel.ui
  )
  set(ICARUS_H5FDDSM_MOC_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/pqH5FDdsmPanel.h
  )
endif()

#--------------------------------------------------
# Setup GUI custom Qt panel sources and wrapping
#--------------------------------------------------
IF(PARAVIEW_BUILD_QT_GUI)
  QT4_WRAP_CPP(
    DSM_MOC_SRCS 
      ${CMAKE_CURRENT_SOURCE_DIR}/pqDsmOptions.h
      ${CMAKE_CURRENT_SOURCE_DIR}/pqDsmObjectInspector.h
      ${ICARUS_H5FDDSM_MOC_SRCS}
  )

  # invoke macro that adds the refinement and display controls panel
  ADD_PARAVIEW_DOCK_WINDOW(
    DSM_IFACE 
    DSM_IFACE_SRCS 
    CLASS_NAME
      pqDsmOptions
    DOCK_AREA 
      Left Right Top Bottom
  )
  QT4_WRAP_UI(
    DSM_UI_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/pqDsmOptions.ui
    ${ICARUS_H5FDDSM_UI_SRCS}
  )
ENDIF(PARAVIEW_BUILD_QT_GUI)

encode_files_as_strings(ENCODED_FILTERS
  ${CMAKE_CURRENT_SOURCE_DIR}/CustomFilter_TransformBlock.xml
  ${CMAKE_CURRENT_SOURCE_DIR}/CustomFilter_XdmfReaderBlock.xml
)

#--------------------------------------------------
# XdmfGenerator
#--------------------------------------------------
# USE_H5FD_DSM must be defined to enable DSM suppport in XdmfGenerator
if(ICARUS_USE_H5FDDSM)
  IF(XDMF_BUILD_MPI)
    SET(USE_MPI 1)
  ENDIF(XDMF_BUILD_MPI)
  SET(USE_H5FD_DSM 1)
  ADD_SUBDIRECTORY(XdmfGenerator)
endif()

#--------------------------------------------------
# Define Plugin
#--------------------------------------------------
ADD_PARAVIEW_PLUGIN(
  ${PLUGIN_NAME}
  "1.0" 

  SERVER_MANAGER_XML
    ${CMAKE_CURRENT_SOURCE_DIR}/vtkAbstractDsmManager.xml
    ${ICARUS_H5FDDSM_SM_XML} 
    ${ICARUS_H5PART_XML}
    ${ICARUS_NETCDF_XML}
  SERVER_MANAGER_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/vtkAbstractDsmManager.cxx
    ${ICARUS_H5FDDSM_SM_SRCS} 
    ${ICARUS_H5PART_SOURCES}
    ${ICARUS_NETCDF_SOURCES}
  SERVER_SOURCES
    ${ENCODED_FILTERS}
    ${ICARUS_SERVER_SRCS}
    ${CMAKE_CURRENT_SOURCE_DIR}/vtkDsmProxyHelper.cxx
    ${CMAKE_CURRENT_SOURCE_DIR}/vtkCustomPipelineHelper.cxx
    ${CMAKE_CURRENT_SOURCE_DIR}/XdmfSteeringParser.cxx
  GUI_INTERFACES 
    ${DSM_IFACE}
  GUI_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/pqDsmOptions.cxx
    ${CMAKE_CURRENT_SOURCE_DIR}/pqDsmObjectInspector.cxx
    ${ICARUS_H5FDDSM_QT_SRCS}
    ${DSM_IFACE_SRCS}
    ${DSM_MOC_SRCS}
    ${DSM_UI_SOURCES}
  GUI_RESOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/IcarusQtResources.qrc
  GUI_RESOURCE_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/Icarus_Readers.xml
    ${CMAKE_CURRENT_SOURCE_DIR}/Filters.xml
    ${CMAKE_CURRENT_SOURCE_DIR}/pqDsmOptions.ui
    ${ICARUS_H5FDDSM_UI_SRCS}
)
if(ICARUS_USE_H5FDDSM)
  TARGET_LINK_LIBRARIES(${PLUGIN_NAME}  XdmfGenerator ${ICARUS_NETCDF_LIBS})
  TARGET_LINK_LIBRARIES(${PLUGIN_NAME} H5FDdsmTools H5FDdsm hdf5)
endif()

IF(ICARUS_USE_H5PART)
  TARGET_LINK_LIBRARIES(${PLUGIN_NAME} pv_meshless )
ENDIF(ICARUS_USE_H5PART)

TARGET_LINK_LIBRARIES(${PLUGIN_NAME} vtkIOXdmf2 pqApplicationComponents)
TARGET_LINK_LIBRARIES(${PLUGIN_NAME} vtkxdmf2 H5FDdsm hdf5)

#--------------------------------------------------
# Converter
#--------------------------------------------------
# ADD_SUBDIRECTORY(Tools)

#--------------------------------------------------
# Example DSM driver application
#--------------------------------------------------
if(ICARUS_USE_H5FDDSM)
  ADD_SUBDIRECTORY(example)
endif()
  
IF(BUILD_TESTING)
#  ADD_SUBDIRECTORY(Testing)
ENDIF(BUILD_TESTING)

#--------------------------------------------------
# Install
#--------------------------------------------------
SET(INSTALL_PATH "${CMAKE_INSTALL_PREFIX}/lib/paraview-${PARAVIEW_VERSION_MAJOR}.${PARAVIEW_VERSION_MINOR}")

INSTALL(
  FILES ${TOOL_CFG_FILES}
  DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
)

IF(PARAVIEW_BUILD_QT_GUI)
  INSTALL(
    TARGETS
      ${PLUGIN_NAME}
    DESTINATION ${INSTALL_PATH}
  )
ENDIF(PARAVIEW_BUILD_QT_GUI)
