MESSAGE("pv include dirs  ${PARAVIEW_INCLUDE_DIRS}")

INCLUDE_DIRECTORIES(
  ${PARAVIEW_INCLUDE_DIRS}
  ${VTK_INCLUDE_DIR}
  ${MPI_INCLUDE_PATH}
  ${ParaView_SOURCE_DIR}/VTK/Utilities/vtklibxml2/include
  ${ParaView_BINARY_DIR}/VTK/Utilities/vtklibxml2/libxml
  ${DSMManager_SOURCE_DIR}/..
)

#--------------------------------------------------
# Find and Use Boost
#--------------------------------------------------
IF(VTK_USE_BOOST)
  # should already be setup
ELSE (VTK_USE_BOOST)
  SET(Boost_DEBUG ON)
  SET(Boost_ADDITIONAL_VERSIONS "1.39" "1.39.0" "1.40" "1.40.0")
  SET(BOOST_ROOT "C:/Program Files/Boost")
  SET(Boost_USE_STATIC_LIBS   ON)
  SET(Boost_USE_MULTITHREADED ON)
  FIND_PACKAGE(Boost 1.38.0 COMPONENTS thread) 
ENDIF(VTK_USE_BOOST)

IF(Boost_FOUND)
  INCLUDE_DIRECTORIES(${Boost_INCLUDE_DIRS})
  MESSAGE(${Boost_INCLUDE_DIRS})
  ADD_DEFINITIONS(-DHAVE_BOOST_THREADS)
  LINK_DIRECTORIES(${Boost_LIBRARY_DIRS})
ENDIF(Boost_FOUND)

#--------------------------------------------------
# Set project include directories 
#--------------------------------------------------
INCLUDE_DIRECTORIES(${DSMManager_SOURCE_DIR})

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

INCLUDE(${DSMManager_SOURCE_DIR}/CMakeSources.txt)

# invoke this macro to add link libraries to PV
PARAVIEW_LINK_LIBRARIES("${PARAVIEW_HDF5_LIBRARIES};Xdmf;${Boost_LIBRARIES}")

# invoke this macro to add the sources to paraview and wrap them for the
# client server 
PARAVIEW_INCLUDE_WRAPPED_SOURCES("${DSMManager_WRAPPED_SRCS}")

# invoke this macro to add the sources to the build (but not wrap them into
# the client server
PARAVIEW_INCLUDE_SOURCES("${DSMManager_SRCS}")

# invoke this macro to add sources into the client 
PARAVIEW_INCLUDE_CLIENT_SOURCES("${DSMManager_GUI_SOURCES}")

PARAVIEW_INCLUDE_GUI_RESOURCES("${DSMManager_GUI_RESOURCES}")

PARAVIEW_INCLUDE_SERVERMANAGER_RESOURCES("${DSMManager_SERVER_XML}")

#--------------------------------------------------
# Converter
#--------------------------------------------------

SET(Converter_LIBS vtkPVFilters)

ADD_EXECUTABLE(
  ConvertToXdmf
  ${DSMManager_SOURCE_DIR}/../Tools/vtkSubdivideUnstructuredGrid.cxx
  ${DSMManager_SOURCE_DIR}/../Tools/vtkSplitBlocksFilter.cxx
  ${DSMManager_SOURCE_DIR}/../Tools/vtkRedistributeBlocksFilter.cxx
  ${DSMManager_SOURCE_DIR}/../Tools/ConvertToXdmf.cxx
)

TARGET_LINK_LIBRARIES(ConvertToXdmf ${Converter_LIBS})

#--------------------------------------------------
# Install
#--------------------------------------------------
SET(INSTALL_PATH "${CMAKE_INSTALL_PREFIX}/lib/paraview-${PARAVIEW_VERSION_MAJOR}.${PARAVIEW_VERSION_MINOR}")

INSTALL(
  TARGETS ConvertToXdmf
  DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
)
