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
INCLUDE(${ParaView_BINARY_DIR}/Utilities/Xdmf2/XDMFConfig.cmake)
INCLUDE_DIRECTORIES(${XDMF_MPI_INCLUDE_PATH} ${XDMF_LIBXML2_INCLUDE})

#--------------------------------------------------
# Find and Use System libs (only needed on WIN32)
#--------------------------------------------------
IF(WIN32 AND MSVC)
  IF (NOT TARGET "hdf5")
    FIND_PACKAGE(HDF5 REQUIRED NO_MODULE PATHS "$ENV{ProgramFiles}/HDF5")
    INCLUDE_DIRECTORIES(${HDF5_INCLUDE_DIR})
  ENDIF (NOT TARGET "hdf5")
  #--------------------------------------------------
  IF (NOT TARGET "H5FDdsm")
    FIND_PACKAGE(H5FDdsm REQUIRED NO_MODULE PATHS "$ENV{ProgramFiles}/H5FDdsm")
    INCLUDE_DIRECTORIES(${H5FD_DSM_INCLUDE_DIR})
  ENDIF (NOT TARGET "H5FDdsm")
#--------------------------------------------------
ENDIF(WIN32 AND MSVC)

#--------------------------------------------------
# Timers
#--------------------------------------------------
OPTION(ICARUS_ENABLE_TIMERS "Enable timing of operations." OFF)
IF(ICARUS_ENABLE_TIMERS)
  SET(ENABLE_TIMERS 1)
ENDIF(ICARUS_ENABLE_TIMERS)

#--------------------------------------------------
# DSM H5Part Reader needs pv-meshless
#--------------------------------------------------
OPTION(ICARUS_USE_H5PART "Build H5Part DSM reader." OFF)
IF(ICARUS_USE_H5PART)
  IF(EXISTS ${CSCS_Plugins_SOURCE_DIR}/pv-meshless)
    INCLUDE_DIRECTORIES(
      ${CSCS_Plugins_SOURCE_DIR}/pv-meshless
      ${CSCS_Plugins_SOURCE_DIR}/pv-meshless/h5part
    )
  ENDIF(EXISTS ${CSCS_Plugins_SOURCE_DIR}/pv-meshless)

  IF(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../pv-meshless)
    INCLUDE_DIRECTORIES(
      ${CMAKE_CURRENT_SOURCE_DIR}/../pv-meshless
      ${CMAKE_CURRENT_SOURCE_DIR}/../pv-meshless/h5part
    )
  ENDIF(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../pv-meshless)

  SET(ICARUS_H5PART_XML ${CMAKE_CURRENT_SOURCE_DIR}/vtkH5PartDsmReader.xml)
  SET(ICARUS_H5PART_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/vtkH5PartDsmReader.cxx)
ENDIF(ICARUS_USE_H5PART)

#--------------------------------------------------
# Config Files for options
#--------------------------------------------------
CONFIGURE_FILE(
  ${CMAKE_CURRENT_SOURCE_DIR}/IcarusConfig.h.in
  ${CMAKE_CURRENT_BINARY_DIR}/IcarusConfig.h)

#--------------------------------------------------
# Set project include directories 
#--------------------------------------------------
INCLUDE_DIRECTORIES(
  ${CMAKE_CURRENT_SOURCE_DIR}/XdmfGenerator
  ${CMAKE_CURRENT_BINARY_DIR}/XdmfGenerator
  )

#--------------------------------------------------
# Setup GUI custom Qt panel sources and wrapping
#--------------------------------------------------
IF(PARAVIEW_BUILD_QT_GUI)
  QT4_WRAP_CPP(
    DSM_MOC_SRCS 
      ${CMAKE_CURRENT_SOURCE_DIR}/pqDsmViewerPanel.h
      ${CMAKE_CURRENT_SOURCE_DIR}/pqDsmObjectInspector.h
  )

  # invoke macro that adds the refinement and display controls panel
  ADD_PARAVIEW_DOCK_WINDOW(
    DSM_IFACE 
    DSM_IFACE_SRCS 
    CLASS_NAME
      pqDsmViewerPanel
    DOCK_AREA 
      Left Right Top Bottom
  )
  QT4_WRAP_UI(
    DSM_UI_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/pqDsmViewerPanel.ui
  )
ENDIF(PARAVIEW_BUILD_QT_GUI)

encode_files_as_strings(ENCODED_FILTERS
  ${CMAKE_CURRENT_SOURCE_DIR}/CustomFilter_TransformBlock.xml
  ${CMAKE_CURRENT_SOURCE_DIR}/CustomFilter_XdmfReaderBlock.xml
)

#--------------------------------------------------
# Define Plugin
#--------------------------------------------------
ADD_PARAVIEW_PLUGIN(
  ${PLUGIN_NAME}
  "1.0" 

  SERVER_MANAGER_XML 
    ${CMAKE_CURRENT_SOURCE_DIR}/vtkDsmManager.xml
    ${CMAKE_CURRENT_SOURCE_DIR}/vtkXdmfReader4.xml
    #${CMAKE_CURRENT_SOURCE_DIR}/vtkXdmfWriter4.xml
    ${CMAKE_CURRENT_SOURCE_DIR}/vtkFlattenOneBlock.xml
    ${CMAKE_CURRENT_SOURCE_DIR}/vtkSteeringWriter.xml
    ${ICARUS_H5PART_XML}
  SERVER_MANAGER_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/vtkDsmManager.cxx
    #${CMAKE_CURRENT_SOURCE_DIR}/vtkXdmfWriter4.cxx
    ${CMAKE_CURRENT_SOURCE_DIR}/vtkXdmfReader4.cxx
    ${CMAKE_CURRENT_SOURCE_DIR}/vtkFlattenOneBlock.cxx
    ${CMAKE_CURRENT_SOURCE_DIR}/vtkSteeringWriter.cxx
    ${ICARUS_H5PART_SOURCES}
  SERVER_SOURCES
    ${ENCODED_FILTERS}
    ${CMAKE_CURRENT_SOURCE_DIR}/vtkDsmProxyHelper.cxx
    ${CMAKE_CURRENT_SOURCE_DIR}/vtkCustomPipelineHelper.cxx
    #${CMAKE_CURRENT_SOURCE_DIR}/H5MButil.cxx
    #${CMAKE_CURRENT_SOURCE_DIR}/XdmfH5MBCallback.cxx
  GUI_INTERFACES 
    ${DSM_IFACE}
  GUI_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/XdmfSteeringParser.cxx
    ${CMAKE_CURRENT_SOURCE_DIR}/pqDsmViewerPanel.cxx
    ${CMAKE_CURRENT_SOURCE_DIR}/pqDsmObjectInspector.cxx
    ${DSM_IFACE_SRCS}
    ${DSM_MOC_SRCS}
    ${DSM_UI_SOURCES}
  GUI_RESOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/IcarusQtResources.qrc
  GUI_RESOURCE_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/Icarus_Readers.xml
    ${CMAKE_CURRENT_SOURCE_DIR}/Filters.xml
    ${CMAKE_CURRENT_SOURCE_DIR}/pqDsmViewerPanel.ui
)

TARGET_LINK_LIBRARIES(${PLUGIN_NAME} XdmfGenerator)

IF(WIN32 AND MSVC)
  TARGET_LINK_LIBRARIES(${PLUGIN_NAME} H5FDdsm hdf5)
ENDIF(WIN32 AND MSVC)

IF(ICARUS_USE_H5PART)
  TARGET_LINK_LIBRARIES(${PLUGIN_NAME} pv_meshless)
ENDIF(ICARUS_USE_H5PART)


#--------------------------------------------------
# XdmfGenerator
#--------------------------------------------------
# USE_H5FD_DSM must be defined to enable DSM suppport in XdmfGenerator
IF(XDMF_BUILD_MPI)
  SET(USE_MPI 1)
ENDIF(XDMF_BUILD_MPI)
SET(USE_H5FD_DSM 1)
ADD_SUBDIRECTORY(XdmfGenerator)

#--------------------------------------------------
# Converter
#--------------------------------------------------
# ADD_SUBDIRECTORY(Tools)

#--------------------------------------------------
# Testing
#--------------------------------------------------
# ADD_SUBDIRECTORY(Testing)

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
