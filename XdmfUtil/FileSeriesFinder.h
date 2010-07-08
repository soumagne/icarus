// DGraham Data
// Ascii2H5Part -c D:/code/CSCS/pv-meshless/CSCS/vtkH5Part/Tools/Ascii2H5Part.plymouth.cfg -f H:/ParticleData/DGraham/DAT_t_6_2.ascii 
// Ascii2H5Part -c D:/code/CSCS/pv-meshless/CSCS/vtkH5Part/Tools/Ascii2H5Part.plymouth.cfg -f H:/ParticleData/DGraham/Graham_wave_data_New.ascii 
// Ascii2H5Part -c D:/code/CSCS/pv-meshless/CSCS/vtkH5Part/Tools/Ascii2H5Part.plymouth.cfg -f H:/ParticleData/DGraham/DATA_2.ascii 
//
// EDF Data
// Ascii2H5Part -c C:/code/CSCS/pv-meshless/CSCS/vtkH5Part/Tools/Ascii2H5Part.edf.cfg -f C:/data/ParticleData/EDF/test_case_output95.dat
// Ascii2H5Part -c D:/code/CSCS/pv-meshless/CSCS/vtkH5Part/Tools/Ascii2H5Part.edf.cfg -f H:\ParticleData\EDF\test_case\test_case_output1.dat
//
// UMan Data
// Ascii2H5Part -c C:/cmakebuild/plugins/bin/ASCII2H5Part.manchester.cfg  -f C:/Data/ParticleData/UMan/Case6/PART_0001 -a C:/Data/ParticleData/UMan/Case6/PART_0000_point_attribs.txt   
//
// ECN Data
// Ascii2H5Part -c C:/code/CSCS/pv-meshless/CSCS/vtkH5Part/Tools/Ascii2H5Part.ecn.cfg -f C:/Data/ParticleData/ECN/BGL_SPhere0POSX.dat
//---------------------------------------------------------------------------
#ifndef FILESERIESFINDER_H
#define FILESERIESFINDER_H

#include "XdmfUtilconfig.h"
#include <vtkstd/string>
#include <vtkstd/vector>
#include <vtkstd/map>
#include <vtkstd/algorithm>
#include <vtksys/SystemTools.hxx>
#include <vtksys/Glob.hxx>
#include <vtksys/RegularExpression.hxx>
#include <vtksys/Process.h>
#include <sstream>
#include "vtkSystemIncludes.h"

typedef vtkstd::vector<vtkstd::string> stringlist;
typedef vtkstd::pair< vtkstd::string, vtkstd::string > maptype;
//
class VTK_EXPORT FileSeriesFinder {
  public:
    FileSeriesFinder(vtkstd::string filenamepattern);
    FileSeriesFinder();

    //
    // If loading just one file and no scan is required, use this function
    //
    void SetFileName(const char *filename);

    //
    // These should be called before calling Scan
    //
    void SetPrefixRegEx(const char *prefix_regex);
    void SetTimeRegEx(const char *time_regex);
    void SetBlockRegEx(const char *block_regex);
    void SetBlockSubDirRegEx(const char *block_regex);
    void SetVarRegEx(const char *var_regex);
    void SetText0RegEx(const char *text0_regex);
    void SetExtRegEx(const char *ext_regex);
    //
    // Scan will search for files
    void Scan(const char *inputfile);
    // print out all the files found
    void TestFilenameGeneration();
    // return the filename for a given time.block,var
    vtkstd::string GenerateFileName(int T, int B, int V);
    // assume block/var are zero and just use time
    vtkstd::string GenerateFileName(int T);
    //
    int GetNumberOfTimeSteps();
    int GetNumberOfBlocks();
    int GetNumberOfVars();

    void GetTimeValues(vtkstd::vector<double> &values);
    double GetTimeValue(int index);

  protected:
    vtkstd::string GenerateNumericPattern(vtkstd::string match);
    vtkstd::string GenerateGlobString(unsigned int index, stringlist &patterns, bool regexmode);
    void FindGlobbedSegments(vtkstd::string &pattern, stringlist &files, bool numeric);


  public:
    vtkstd::string FileNamePattern; 
    vtkstd::string PrefixRegEx;
    vtkstd::string BlockRegEx;
    vtkstd::string BlockSubDirRegEx;
    vtkstd::string VarRegEx; 
    vtkstd::string ExtRegEx;
    vtkstd::string TimeRegEx;
    vtkstd::string Text0RegEx;
    //
    vtkstd::map<vtkstd::string, vtkstd::string> RegExMap;
    stringlist patterns, regexmatches, Tstrings, Bstrings, Vstrings;
    vtkstd::string regex;
    int TimeIndex, NumberOfTimeSteps;
    int BlockIndex, NumberOfBlocks;
    int VarIndex, NumberOfVars;
    vtkstd::string timeform, blockform, varform, filepattern;
};

#endif // FILESERIESFINDER_H
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
