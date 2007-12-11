/*
 * The information in this file is
 * Copyright(c) 2007 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "AppVersion.h"
#include "EnviImporter.h"
#include "Classification.h"
#include "DimensionDescriptor.h"
#include "DynamicObject.h"
#include "Endian.h"
#include "FileFinder.h"
#include "ImportDescriptor.h"
#include "ModelServices.h"
#include "ObjectFactory.h"
#include "ObjectResource.h"
#include "RasterDataDescriptor.h"
#include "RasterFileDescriptor.h"
#include "RasterUtilities.h"
#include "SpecialMetadata.h"
#include "StringUtilities.h"
#include "Units.h"

#include <sstream>

using namespace std;

static bool parseDefaultBands(EnviField* pField, vector<unsigned int>* pBandNumbers);
static bool parseWavelengths(EnviField* pField, vector<double>* pWavelengthCenters);
static bool parseFwhm(EnviField* pField, vector<double>* pWavelengthStarts,
                      const vector<double>* pWavelengthCenters, vector<double>* pWavelengthEnds);
static bool parseBbl(EnviField* pField, vector<unsigned int>* pBadBands);

template <class T>
void vectorFromField(EnviField* pField, vector<T> &vec, const char *pFormat)
{
   char *pPtr;
   char *pBuffer = new char[pField->mValue.size()+1];
   strcpy (pBuffer, pField->mValue.c_str());
   pPtr = strtok (pBuffer, ",");
   while (pPtr != NULL)
   {
      T value;
      int count = sscanf (pPtr, pFormat, &value);
      if (count == 1)
         vec.push_back (value);
      pPtr = strtok (NULL, ",");
   }
   delete pBuffer;
}

EnviImporter::EnviImporter()
{
   setName("ENVI Importer");
   setCreator("Ball Aerospace & Technologies Corp.");
   setCopyright(APP_COPYRIGHT);
   setVersion(APP_VERSION_NUMBER);
   setExtensions("ENVI Header Files (*.hdr)");
   setDescriptorId("{811F49A2-3930-4a43-AC69-5A08DAEC93B8}");
   allowMultipleInstances(true);
   setProductionStatus(APP_IS_PRODUCTION_RELEASE);

   mFields.mTag = "envi";
}

EnviImporter::~EnviImporter()
{
}

vector<ImportDescriptor*> EnviImporter::getImportDescriptors(const string& filename)
{
   string headerFile = filename;

   bool bSuccess = parseHeader(headerFile);
   if (bSuccess == false)
   {
      headerFile = findHeaderFile(headerFile);
      if (headerFile.empty() == false)
      {
         bSuccess = parseHeader(headerFile);
      }
   }

   EnviField *pField = NULL;
   vector<ImportDescriptor*> descriptors;
   if (bSuccess == true)
   {
      string dataFile = findDataFile(headerFile);
      if (dataFile.empty() == false)
      {
         ImportDescriptor* pImportDescriptor = mpModel->createImportDescriptor(dataFile, "RasterElement", NULL);
         if (pImportDescriptor != NULL)
         {
            RasterDataDescriptor* pDescriptor =
               dynamic_cast<RasterDataDescriptor*>(pImportDescriptor->getDataDescriptor());
            if (pDescriptor != NULL)
            {
               FactoryResource<RasterFileDescriptor> pFileDescriptor;
               if (pFileDescriptor.get() != NULL)
               {
                  // Filename
                  pFileDescriptor->setFilename(dataFile);

                  // Coordinate offset
                  int columnOffset = 0;
                  int rowOffset = 0;

                  pField = mFields.find("x-start");
                  if (pField != NULL)
                  {
                     columnOffset = atoi(pField->mValue.c_str());
                  }

                  pField = mFields.find("y-start");
                  if (pField != NULL)
                  {
                     rowOffset = atoi(pField->mValue.c_str());
                  }

                  // Rows
                  vector<DimensionDescriptor> rows;
                  pField = mFields.find("lines");
                  if (pField != NULL)
                  {
                     int numRows = atoi(pField->mValue.c_str());
                     for (int i = 0; i < numRows; ++i)
                     {
                        DimensionDescriptor rowDim;
                        rowDim.setOriginalNumber(static_cast<unsigned int>(rowOffset + i));
                        rowDim.setOnDiskNumber(static_cast<unsigned int>(i));
                        rows.push_back(rowDim);
                     }

                     pDescriptor->setRows(rows);
                     pFileDescriptor->setRows(rows);
                  }
                 
                  // Columns
                  vector<DimensionDescriptor> columns;
                  pField = mFields.find("samples");
                  if (pField != NULL)
                  {                  
                     int numColumns = atoi(pField->mValue.c_str());
                     for (int i = 0; i < numColumns; ++i)
                     {
                        DimensionDescriptor columnDim;
                        columnDim.setOriginalNumber(static_cast<unsigned int>(columnOffset + i));
                        columnDim.setOnDiskNumber(static_cast<unsigned int>(i));
                        columns.push_back(columnDim);
                     }

                     pDescriptor->setColumns(columns);
                     pFileDescriptor->setColumns(columns);
                  }

                  // Bands
                  vector<DimensionDescriptor> bands;
                  pField = mFields.find("bands");
                  if (pField != NULL)
                  {
                     int numBands = atoi(pField->mValue.c_str());
                     bands = RasterUtilities::generateDimensionVector(numBands, true, false, true);
                     pDescriptor->setBands(bands);
                     pFileDescriptor->setBands(bands);
                  }

                  // Description
                  list<GcpPoint> gcps;

                  pField = mFields.find("description");
                  if (pField != NULL)
                  {
                     // Metadata
                     if (pField->mChildren.empty() == false)
                     {
                        FactoryResource<DynamicObject> pMetadata;
                        for (unsigned int i = 0; i < pField->mChildren.size(); ++i)
                        {
                           EnviField* pChild = pField->mChildren[i];
                           if (pChild != NULL)
                           {
                              if (pChild->mTag == "filename")
                              {
                                 // Filename
                                 if (pChild->mValue.empty() == false)
                                 {
                                    pFileDescriptor->setFilename(pChild->mValue);
                                 }
                              }
                              else if (pChild->mTag == "classification")
                              {
                                 // Classification
                                 FactoryResource<Classification> pClassification;
                                 if (pClassification.get() != NULL)
                                 {
                                    string classLevel;
                                    classLevel.append(1, *(pChild->mValue.data()));
                                    pClassification->setLevel(classLevel);

                                    pDescriptor->setClassification(pClassification.get());
                                 }
                              }
                              else if ((pChild->mTag == "ll") || (pChild->mTag == "lr") || (pChild->mTag == "ul") ||
                                 (pChild->mTag == "ur") || (pChild->mTag == "center"))
                              {
                                 GcpPoint gcp;
                                 bool dmsFormat = false;
                                 char ns;
                                 char ew;

                                 sscanf(pChild->mValue.c_str(), "%lg%c %lg%c", &gcp.mCoordinate.mY, &ew,
                                    &gcp.mCoordinate.mX, &ns);
                                 if (fabs(gcp.mCoordinate.mY) > 180.0 || fabs(gcp.mCoordinate.mX) > 90.0)
                                 {
                                    dmsFormat = true;
                                 }

                                 double deg, min, sec;
                                 if (dmsFormat == true)
                                 {
                                    deg = (int)(gcp.mCoordinate.mY / 10000.0);
                                    min = (int)((gcp.mCoordinate.mY - 10000.0 * deg) / 100.0);
                                    sec = gcp.mCoordinate.mY - 10000.0 * deg - 100.0 * min;
                                    gcp.mCoordinate.mY = deg + (min / 60.0) + (sec / 3600.0);
                                 }

                                 if (ew == 'W' || ew == 'w')
                                 {
                                    gcp.mCoordinate.mY = -gcp.mCoordinate.mY;
                                 }

                                 if (dmsFormat)
                                 {
                                    deg = (int)(gcp.mCoordinate.mX / 10000.0);
                                    min = (int)((gcp.mCoordinate.mX - 10000.0 * deg) / 100.0);
                                    sec = gcp.mCoordinate.mX - 10000.0 * deg - 100.0 * min;
                                    gcp.mCoordinate.mX = deg + (min / 60.0) + (sec / 3600.0);
                                 }

                                 if (ns == 'S' || ns == 's')
                                 {
                                    gcp.mCoordinate.mX = -gcp.mCoordinate.mX;
                                 }

                                 if (pChild->mTag == "ll")
                                 {
                                    gcp.mPixel.mX = 0.0;
                                    gcp.mPixel.mY = 0.0;
                                 }
                                 else if (pChild->mTag == "lr")
                                 {
                                    gcp.mPixel.mX = columns.size() - 1.0;
                                    gcp.mPixel.mY = 0.0;
                                 }
                                 else if (pChild->mTag == "ul")
                                 {
                                    gcp.mPixel.mX = 0.0;
                                    gcp.mPixel.mY = rows.size() - 1.0;
                                 }
                                 else if (pChild->mTag == "ur")
                                 {
                                    gcp.mPixel.mX = columns.size() - 1.0;
                                    gcp.mPixel.mY = rows.size() - 1.0;
                                 }
                                 else if (pChild->mTag == "center")
                                 {
                                    gcp.mPixel.mX = (columns.size() / 2.0) - 0.5;
                                    gcp.mPixel.mY = (rows.size() / 2.0) - 0.5;
                                 }

                                 gcps.push_back(gcp);
                              }
                              else if (pChild->mTag.empty() == false)
                              {
                                 pMetadata->setAttribute(pChild->mTag, pChild->mValue);
                              }
                           }
                        }

                        if (pMetadata->getNumAttributes() > 0)
                        {
                           pDescriptor->setMetadata(pMetadata.get());
                        }
                     }
                  }

                  // GCPs
                  if (gcps.empty() == false)
                  {
                     pFileDescriptor->setGcps(gcps);
                  }

                  // Header bytes
                  pField = mFields.find("header offset");
                  if (pField != NULL)
                  {
                     int headerBytes = atoi(pField->mValue.c_str());
                     pFileDescriptor->setHeaderBytes(static_cast<unsigned int>(headerBytes));
                  }

                  // Data type
                  pField = mFields.find("data type");
                  if (pField != NULL)
                  {
                     switch (atoi(pField->mValue.c_str()))
                     {
                        case 1:     // char
                           pDescriptor->setDataType(INT1UBYTE);
                           pFileDescriptor->setBitsPerElement(8);
                           break;

                        case 2:     // short
                           pDescriptor->setDataType(INT2SBYTES);
                           pFileDescriptor->setBitsPerElement(16);
                           break;

                        case 3:     // int
                           pDescriptor->setDataType(INT4SBYTES);
                           pFileDescriptor->setBitsPerElement(32);
                           break;

                        case 4:     // float
                           pDescriptor->setDataType(FLT4BYTES);
                           pFileDescriptor->setBitsPerElement(32);
                           break;

                        case 5:     // double
                           pDescriptor->setDataType(FLT8BYTES);
                           pFileDescriptor->setBitsPerElement(64);
                           break;

                        case 6:     // float complex
                           pDescriptor->setDataType(FLT8COMPLEX);
                           pFileDescriptor->setBitsPerElement(64);
                           break;

                        case 9:     // double complex
                           // not supported
                           break;

                        case 12:    // unsigned short
                           pDescriptor->setDataType(INT2UBYTES);
                           pFileDescriptor->setBitsPerElement(16);
                           break;

                        case 13:    // unsigned int
                           pDescriptor->setDataType(INT4UBYTES);
                           pFileDescriptor->setBitsPerElement(32);
                           break;

                        case 14:    // 64-bit int
                        case 15:    // unsigned 64-bit int
                           // not supported
                           break;

                        case 99:    // integer complex (recognized only by this application)
                           pDescriptor->setDataType(INT4SCOMPLEX);
                           pFileDescriptor->setBitsPerElement(32);
                           break;

                        default:
                           break;
                     }

                     // Bad values
                     EncodingType dataType = pDescriptor->getDataType();
                     if ((dataType != FLT4BYTES) && (dataType != FLT8COMPLEX) && (dataType != FLT8BYTES))
                     {
                        vector<int> badValues;
                        badValues.push_back(0);

                        pDescriptor->setBadValues(badValues);
                     }
                  }

                  // Interleave format
                  pField = mFields.find("interleave");
                  if (pField != NULL)
                  {
                     string interleave = StringUtilities::toLower(pField->mValue);
                     if (interleave == "bip")
                     {
                        pDescriptor->setInterleaveFormat(BIP);
                        pFileDescriptor->setInterleaveFormat(BIP);
                     }
                     else if (interleave == "bil")
                     {
                        pDescriptor->setInterleaveFormat(BIL);
                        pFileDescriptor->setInterleaveFormat(BIL);
                     }
                     else if (interleave == "bsq")
                     {
                        pDescriptor->setInterleaveFormat(BSQ);
                        pFileDescriptor->setInterleaveFormat(BSQ);
                     }
                  }

                  // Endian
                  pField = mFields.find("byte order");
                  if (pField != NULL)
                  {
                     int byteOrder = atoi(pField->mValue.c_str());
                     if (byteOrder == 0)
                     {
                        pFileDescriptor->setEndian(LITTLE_ENDIAN);
                     }
                     else if (byteOrder == 1)
                     {
                        pFileDescriptor->setEndian(BIG_ENDIAN);
                     }
                  }

                  // check for scaling factor
                  pField = mFields.find("reflectance scale factor");
                  if (pField !=NULL)
                  {
                     double scalingFactor=0.0;
                     stringstream scaleStream(pField->mValue);
                     scaleStream >> scalingFactor;
                     if (!scaleStream.fail() && scalingFactor != 0.0)
                     {
                        Units *pUnits = pDescriptor->getUnits();
                        if (pUnits != NULL)
                        {
                           pUnits->setScaleFromStandard(1.0 / scalingFactor);
                           pUnits->setUnitName("Reflectance");
                        }
                     }
                  }

                  // Pixel size
                  pField = mFields.find("pixel size");
                  if (pField != NULL)
                  {
                     if (pField->mChildren.size() == 2)
                     {
                        pField = pField->mChildren[0];
                        if (pField != NULL)
                        {
                           double pixelSize = 1.0;
                           if (sscanf(pField->mValue.c_str(), "%g", &pixelSize) == 1)
                           {
                              pDescriptor->setXPixelSize(pixelSize);
                              pFileDescriptor->setXPixelSize(pixelSize);
                           }
                        }

                        pField = pField->mChildren[1];
                        if (pField != NULL)
                        {
                           double pixelSize = 1.0;
                           if (sscanf(pField->mValue.c_str(), "%g", &pixelSize) == 1)
                           {
                              pDescriptor->setYPixelSize(pixelSize);
                              pFileDescriptor->setYPixelSize(pixelSize);
                           }
                        }
                     }
                  }

                  // Default bands
                  pField = mFields.find("default bands");
                  if (pField != NULL)
                  {
                     vector<unsigned int> displayBands;
                     parseDefaultBands(pField, &displayBands);

                     if (displayBands.size() == 1)
                     {
                        DimensionDescriptor grayBand = pFileDescriptor->getOriginalBand(displayBands[0]);

                        pDescriptor->setDisplayBand(GRAY, grayBand);
                        pDescriptor->setDisplayMode(GRAYSCALE_MODE);
                     }
                     else if (displayBands.size() == 3)
                     {
                        DimensionDescriptor redBand = pFileDescriptor->getOriginalBand(displayBands[0]);
                        DimensionDescriptor greenBand = pFileDescriptor->getOriginalBand(displayBands[1]);
                        DimensionDescriptor blueBand = pFileDescriptor->getOriginalBand(displayBands[2]);

                        pDescriptor->setDisplayBand(RED, redBand);
                        pDescriptor->setDisplayBand(GREEN, greenBand);
                        pDescriptor->setDisplayBand(BLUE, blueBand);
                        pDescriptor->setDisplayMode(RGB_MODE);
                     }
                  }

                  // Bad bands
                  pField = mFields.find("bbl");
                  vector<unsigned int> goodBandNumbers;
                  if (pField != NULL)
                  {
                     parseBbl(pField, &goodBandNumbers);

                     vector<DimensionDescriptor> bandsToLoad;
                     for (unsigned int i = 0; i < goodBandNumbers.size(); ++i)
                     {
                        unsigned int onDiskNumber = goodBandNumbers[i];
                        DimensionDescriptor bandDim = pFileDescriptor->getOnDiskBand(onDiskNumber);
                        if (bandDim.isValid())
                        {
                           bandsToLoad.push_back(bandDim);
                        }
                     }

                     pDescriptor->setBands(bandsToLoad);
                  }

                  DynamicObject* pMetadata = pDescriptor->getMetadata();

                  // Band names
                  pField = mFields.find("band names");
                  if (pField != NULL)
                  {
                     vector<string> bandNames;
                     for (unsigned int i = 0; i < pField->mChildren.size(); ++i)
                     {
                        string bandName = pField->mChildren[i]->mValue;
                        if (bandName.empty() == false)
                        {
                           bandNames.push_back(bandName);
                        }
                     }

                     if (pMetadata != NULL)
                     {
                        string pNamesPath[] = { SPECIAL_METADATA_NAME, BAND_METADATA_NAME, 
                           NAMES_METADATA_NAME, END_METADATA_NAME };
                        pMetadata->setAttributeByPath(pNamesPath, bandNames);
                     }
                  }

                  // Wavelengths
                  vector<double> centerWavelengths;
                  pField = mFields.find("wavelength");
                  if (pField != NULL)
                  {
                     parseWavelengths(pField, &centerWavelengths);
                     if (pMetadata != NULL)
                     {
                        string pCenterPath[] = { SPECIAL_METADATA_NAME, BAND_METADATA_NAME, 
                           CENTER_WAVELENGTHS_METADATA_NAME, END_METADATA_NAME };
                        pMetadata->setAttributeByPath(pCenterPath, centerWavelengths);
                     }
                  }

                  // FWHM
                  pField = mFields.find("fwhm");
                  if (pField != NULL)
                  {
                     vector<double> startWavelengths;
                     vector<double> endWavelengths;
                     parseFwhm(pField, &startWavelengths, &centerWavelengths, &endWavelengths);
                     
                     if (pMetadata != NULL)
                     {
                        string pStartPath[] = { SPECIAL_METADATA_NAME, BAND_METADATA_NAME, 
                           START_WAVELENGTHS_METADATA_NAME, END_METADATA_NAME };
                        pMetadata->setAttributeByPath(pStartPath, startWavelengths);
                        string pEndPath[] = { SPECIAL_METADATA_NAME, BAND_METADATA_NAME, 
                           END_WAVELENGTHS_METADATA_NAME, END_METADATA_NAME };
                        pMetadata->setAttributeByPath(pEndPath, endWavelengths);
                     }
                  }

                  // File descriptor
                  pDescriptor->setFileDescriptor(pFileDescriptor.get());
               }
            }

            descriptors.push_back(pImportDescriptor);
         }
      }
   }

   return descriptors;
}

unsigned char EnviImporter::getFileAffinity(const std::string& filename)
{
   if (getImportDescriptors(filename).empty())
   {
      return Importer::CAN_NOT_LOAD;
   }
   else
   {
      return Importer::CAN_LOAD;
   }
}

static bool parseDefaultBands(EnviField* pField, vector<unsigned int>* pBandNumbers)
{
   if ((pField == NULL) || (pBandNumbers == NULL))
   {
      return false;
   }

   for (unsigned int i = 0; i < pField->mChildren.size(); i++)
   {
      vectorFromField(pField->mChildren[i], *pBandNumbers, "%u");
   }

   for (unsigned int i = 0; i < pBandNumbers->size(); i++)
   {
      pBandNumbers->at(i) = pBandNumbers->at(i) - 1;
   }

   return true;
}

static double sWavelengthScaleFactor = 1.0;

static bool parseWavelengths (EnviField *field, 
                              vector<double>* pWavelengthCenters)
{
   unsigned int i;
   double maxWavelength = 0.0;

   for (i=0; i<field->mChildren.size(); i++)
   {
      vectorFromField(field->mChildren.at(i), *pWavelengthCenters, "%lf");
   }
   for (i=0; i<pWavelengthCenters->size(); ++i)
   {
      if (pWavelengthCenters->at(i) > maxWavelength)
      {
         maxWavelength = pWavelengthCenters->at(i);
      }
   }
   if (maxWavelength > 50.0) // wavelengths are in nanometers, convert to microns
   {
      sWavelengthScaleFactor = 1.0 / 1000.0;
      for (i = 0; i < pWavelengthCenters->size(); ++i)
      {
         pWavelengthCenters->at(i) *= (float) sWavelengthScaleFactor;
      }
   }
   return true;
}

static bool parseFwhm(EnviField *field,
                      vector<double>* pWavelengthStarts,
                      const vector<double>* pWavelengthCenters,
                      vector<double> *pWavelengthEnds)
{
   unsigned int i;

   vector<double> fwhmValues;
   for (i = 0; i < field->mChildren.size(); i++)
   {
      vectorFromField(field->mChildren.at(i), fwhmValues, "%lf");
   }

   if (fwhmValues.size() != pWavelengthCenters->size())
   {
      return false;
   }

   for (i = 0; i < fwhmValues.size(); i++)
   {
      pWavelengthStarts->push_back((float) (pWavelengthCenters->at(i) - sWavelengthScaleFactor * fwhmValues[i] / 2.0));
      pWavelengthEnds->push_back((float) (pWavelengthCenters->at(i) + sWavelengthScaleFactor * fwhmValues[i] / 2.0));
   }

   return true;
}

static bool parseBbl (EnviField *field, 
                              vector<unsigned int>* pBadBands)
{
   unsigned int i;
   int band = 0;

   for (i=0; i<field->mChildren.size(); i++)
   {
      int fields = 1;
      char *line, *ptr;
      line = strdup (field->mChildren.at(i)->mValue.c_str());
      ptr = strtok (line, ",");
      float f1 = 0.0;
      while (fields == 1 && ptr != NULL)
      {
         fields = sscanf (ptr, "%f", &f1);
         ptr = strtok (NULL, ",");
         if (fields == 1)
         {
            if (f1 == 1.0f)
            {
               pBadBands->push_back (band);
            }
            band++;
         }
      }

      free (line);
   }
   return true;
}

string EnviImporter::findHeaderFile(const string& dataPath)
{
   string temp = dataPath + ".hdr";

   FILE* pStream = fopen(temp.c_str(), "rb");
   if (pStream != NULL)
   {
      fclose(pStream);
      return temp;
   }
   else
   {
      unsigned int i = dataPath.rfind('.');
      if ((i >= 0) && (i < dataPath.size()))
      {
         temp = dataPath.substr (0, i) + ".hdr";
         pStream = fopen(temp.c_str(), "rb");
         if (pStream != NULL)
         {
            fclose(pStream);
            return temp;
         }
      }
   }
   return "";
}

string EnviImporter::findDataFile(const string& headerPath)
{
   FILE* pStream = NULL;
   string temp;

   // Read the header if necessary
   EnviField* pField = mFields.find("description");
   if (pField == NULL)
   {
      bool bSuccess = parseHeader(headerPath);
      if (bSuccess == false)
      {
         return "";
      }
   }

   // Check for the filename field
   pField = mFields.find("description");
   if (pField != NULL)
   {
      pField = pField->find("filename");
      if (pField != NULL)
      {
         temp = pField->mValue;
         if (!temp.empty())
         {
            pStream = fopen(temp.c_str(), "rb");
         }

         if (pStream != NULL)
         {
            fclose(pStream);
            return temp;
         }
      }
   }

   // Check for files with the same name
   temp = headerPath;
   unsigned int i = temp.rfind('.');
   if ((i >= 0) && (i < temp.size()))
   {
      temp = temp.substr(0, i);

      // Check common extensions
      char *pExtensions[] = {".bip", ".bil", ".bsq", ".dat",
                            "",     ".sio", ".cub", ".img"};
      for (int j = 0; j < sizeof(pExtensions) / sizeof(pExtensions[0]); j++)
      {
         string attempt = temp + pExtensions[j];
         pStream = fopen(attempt.c_str(), "rb");
         if (pStream != NULL)
         {
            fclose(pStream);
            return attempt;
         }
      }

      // Check any extension
      FactoryResource<FileFinder> pFileFinder;
      if (pFileFinder.get() != NULL)
      {
         FactoryResource<Filename> pFileName;
         VERIFYRV(pFileName.get() != NULL, string());
         pFileName->setFullPathAndName(temp);
         if (pFileFinder->findFile(pFileName->getPath(), pFileName->getTitle() + ".*") == true)
         {
            while (pFileFinder->findNextFile() != NULL)
            {
               string dataFile = "";
               pFileFinder->getFullPath(dataFile);
               if (dataFile != headerPath)
               {
                  return dataFile;
               }
            }
         }
      }
   }

   return "";
}

bool EnviImporter::parseHeader(const string& filename)
{
   if (filename.empty() == true)
   {
      return false;
   }

   bool bSuccess = mFields.populateFromHeader(filename);
   return bSuccess;
}
