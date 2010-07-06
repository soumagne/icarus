PROJECT("DSMMANAGER_PLUGIN")

CMAKE_MINIMUM_REQUIRED (VERSION 2.6)

IF(COMMAND CMAKE_POLICY)
  CMAKE_POLICY(SET CMP0003 NEW)
ENDIF(COMMAND CMAKE_POLICY)

#--------------------------------------------------
# Find and Use ParaView
#--------------------------------------------------
IF(NOT PARAVIEW_LOADED)
  FIND_PACKAGE(ParaView REQUIRED)
  INCLUDE(${PARAVIEW_USE_FILE})
  IF (ParaView_BINARY_DIR)
  INCLUDE_DIRECTORIES(
    ${ParaView_CMAKE_DIR}/../Utilities/H5FDdsm
    ${ParaView_BINARY_DIR}/Utilities/H5FDdsm
  )
  ENDIF(ParaView_BINARY_DIR)
  SET(PARAVIEW_LOADED 1)
ELSE(NOT PARAVIEW_LOADED)
  IF (PARAVIEW_BUILD_QT_GUI)
    INCLUDE(${QT_USE_FILE})
  ENDIF (PARAVIEW_BUILD_QT_GUI)
  INCLUDE_DIRECTORIES(
    ${PARAVIEW_INCLUDE_DIRS}
    ${ParaView_SOURCE_DIR}/VTK/Geovis
    ${PARAVIEW_GUI_INCLUDE_DIRS}
    ${PARAVIEW_KWSYS_INCLUDE_DIRS}
    ${VTK_INCLUDE_DIR}
    ${MPI_INCLUDE_PATH}
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
  INCLUDE_DIRECTORIES(
    ${ParaView_CMAKE_DIR}/../Utilities/H5FDdsm
    ${ParaView_BINARY_DIR}/Utilities/H5FDdsm
  )
ENDIF(NOT PARAVIEW_LOADED)

#--------------------------------------------------
# Find and Use Boost
#--------------------------------------------------
SET(Boost_DEBUG ON)
SET(Boost_ADDITIONAL_VERSIONS "1.39" "1.39.0" "1.40" "1.40.0")
SET(BOOST_ROOT "C:/Program Files/Boost")
SET(Boost_USE_STATIC_LIBS   ON)
SET(Boost_USE_MULTITHREADED ON)
FIND_PACKAGE(Boost 1.38.0 COMPONENTS thread) 

IF(Boost_FOUND)
  INCLUDE_DIRECTORIES(${Boost_INCLUDE_DIRS})
  #MESSAGE(${Boost_INCLUDE_DIRS})
  ADD_DEFINITIONS(-DHAVE_BOOST_THREADS)
  LINK_DIRECTORIES(${Boost_LIBRARY_DIRS})
ENDIF(Boost_FOUND)

#--------------------------------------------------
# Set project include directories 
#--------------------------------------------------
INCLUDE_DIRECTORIES(
  ${CMAKE_CURRENT_SOURCE_DIR}/XdmfUtil
  ${CMAKE_CURRENT_BINARY_DIR}/XdmfUtil
  )

#--------------------------------------------------
# Set Definitions 
#--------------------------------------------------
ADD_DEFINITIONS(-DMPICH_SKIP_MPICXX)
ADD_DEFINITIONS(-DCSCS_PARAVIEW_INTERNAL)
ADD_DEFINITIONS(-D_CRT_SECURE_NO_WARNINGS)

#-----------------------------------------------
# Setup some convenience flags depending on machine
#-----------------------------------------------
SET(COMPUTERNAME $ENV{COMPUTERNAME})
IF(COMPUTERNAME)
  MESSAGE("Setting computername #DEFINE to MACHINE_$ENV{COMPUTERNAME}")
  ADD_DEFINITIONS(-DMACHINE_$ENV{COMPUTERNAME})
ENDIF(COMPUTERNAME)

#--------------------------------------------------
# Setup GUI custom Qt panel sources and wrapping
#--------------------------------------------------
IF(PARAVIEW_BUILD_QT_GUI)
  OPTION(CSCS_DSM_BUILD_TESTING "Build testing DSM viewer panel" OFF)
  IF(CSCS_DSM_BUILD_TESTING)
    SET(DSM_PANEL_CLASS pqDSMViewerPanel_testing)
  ELSE(CSCS_DSM_BUILD_TESTING)
    SET(DSM_PANEL_CLASS pqDSMViewerPanel)
  ENDIF(CSCS_DSM_BUILD_TESTING)

  QT4_WRAP_CPP(
    DSM_MOC_SRCS 
      ${CMAKE_CURRENT_SOURCE_DIR}/${DSM_PANEL_CLASS}.h
  )
  # invoke macro that adds the refinement and display controls panel
  ADD_PARAVIEW_DOCK_WINDOW(
    DSM_IFACE 
    DSM_IFACE_SRCS 
    CLASS_NAME 
      ${DSM_PANEL_CLASS}
    DOCK_AREA 
      Left Right Top Bottom
  )
  QT4_WRAP_UI(
    DSM_UI_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/${DSM_PANEL_CLASS}.ui
  )
ENDIF(PARAVIEW_BUILD_QT_GUI)

#--------------------------------------------------
# Define Plugin
#--------------------------------------------------
ADD_PARAVIEW_PLUGIN(
  CSCS_DSM
  "1.0" 

  SERVER_MANAGER_XML 
    ${CMAKE_CURRENT_SOURCE_DIR}/CSCS_DSMManager_Server.xml
  SERVER_MANAGER_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/vtkDSMManager.cxx
    ${CMAKE_CURRENT_SOURCE_DIR}/vtkXdmfWriter4.cxx
    ${CMAKE_CURRENT_SOURCE_DIR}/vtkXdmfReader3.cxx
    ${CMAKE_CURRENT_SOURCE_DIR}/vtkXdmfReader4.cxx
  SERVER_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/H5MButil.cxx
    ${CMAKE_CURRENT_SOURCE_DIR}/XdmfH5MBCallback.cxx    
  GUI_INTERFACES 
    ${DSM_IFACE}
  GUI_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/${DSM_PANEL_CLASS}.cxx
    ${DSM_IFACE_SRCS}
    ${DSM_MOC_SRCS}
    ${DSM_UI_SOURCES}
  GUI_RESOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/CSCS_DSMManager.qrc
  GUI_RESOURCE_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/${DSM_PANEL_CLASS}.ui
)

TARGET_LINK_LIBRARIES(
  CSCS_DSM
    H5FDdsm    
    Xdmf
    XdmfUtil
    ${PARAVIEW_HDF5_LIBRARIES} 
    ${Boost_LIBRARIES}
)

#--------------------------------------------------
# XdmfUtil
#--------------------------------------------------
# USE_H5FD_DSM must be defined to enable DSM suppport in XdmfUtil
SET(USE_H5FD_DSM 1)
ADD_SUBDIRECTORY(XdmfUtil)

#--------------------------------------------------
# Testing
#--------------------------------------------------
ADD_SUBDIRECTORY(Testing)

#--------------------------------------------------
# Converter
#--------------------------------------------------

SET(Converter_LIBS CSCS_DSM)

IF(CSCS_PLUGIN_CFX)
  MESSAGE("Xdmf converter supporting CFX " ${CSCS_CFX_DIR})
  SET(HAS_CFX ON)
  ADD_DEFINITIONS(-DHAS_CFX)
  INCLUDE_DIRECTORIES(${CSCS_CFX_DIR})
  LINK_DIRECTORIES(${CSCS_CFX_LINK_DIRS})
  SET(Converter_LIBS ${Converter_LIBS} CSCS_CFX)
ENDIF(CSCS_PLUGIN_CFX)
  
IF(CSCS_PLUGIN_Fluent)
  MESSAGE("Xdmf converter supporting Fluent " ${CSCS_Fluent_DIR})
  SET(HAS_FLUENT ON)
  ADD_DEFINITIONS(-DHAS_FLUENT)
  INCLUDE_DIRECTORIES(${CSCS_Fluent_DIR})
  SET(Converter_LIBS ${Converter_LIBS} CSCS_Fluent)
ENDIF(CSCS_PLUGIN_Fluent)

ADD_EXECUTABLE(
  ConvertToXdmf
  ${CMAKE_CURRENT_SOURCE_DIR}/Tools/vtkSubdivideUnstructuredGrid.cxx
  ${CMAKE_CURRENT_SOURCE_DIR}/Tools/vtkSplitBlocksFilter.cxx
  ${CMAKE_CURRENT_SOURCE_DIR}/Tools/vtkRedistributeBlocksFilter.cxx
  ${CMAKE_CURRENT_SOURCE_DIR}/Tools/ConvertToXdmf.cxx
)

TARGET_LINK_LIBRARIES(ConvertToXdmf ${Converter_LIBS})

ADD_EXECUTABLE(
  TestDSMListener  
  ${CMAKE_CURRENT_SOURCE_DIR}/Testing/TestDSMListener.cxx
)

TARGET_LINK_LIBRARIES(TestDSMListener ${Converter_LIBS})

#ADD_EXECUTABLE(
#  TreeTest
#  ${CMAKE_CURRENT_SOURCE_DIR}/Testing/TreeTest.cpp
#)


#--------------------------------------------------
# Testing, pull in the test from ParaView/Utilities/Xdmf2/vtk/Testing/Cxx
#--------------------------------------------------
#ADD_SUBDIRECTORY(
#  ${ParaView_SOURCE_DIR}/Utilities/Xdmf2/vtk/Testing/Cxx
#  ${CMAKE_CURRENT_BINARY_DIR}/PVTesting
#  )

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
      CSCS_DSM
    DESTINATION ${INSTALL_PATH}
  )
ENDIF(PARAVIEW_BUILD_QT_GUI)
