/*
 * The information in this file is
 * Copyright(c) 2007 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include <math.h>
#include <stdio.h>

#include "AppConfig.h"
#include "GeoPoint.h"
#include "MgrsEngine.h"

using namespace std;

#pragma message(__FILE__ "(" STRING(__LINE__) ") : warning : This should really be a Location<double,3> with methods to convert/access as DMS, LatLon, UTM, and GPRS (tclarke)")

///////////////
//  DmsPoint //
///////////////

DmsPoint::DmsPoint(DmsType eType, double dValue)
{
   mType = eType;
   mValue = dValue;
}

DmsPoint::DmsPoint(DmsType eType, const std::string& valueText)
{
   mType = eType;
   mValue = 0.0;

   setValue(valueText);
}

DmsPoint::~DmsPoint()
{
}

DmsPoint::DmsType DmsPoint::getType() const
{
   return mType;
}

void DmsPoint::setValue(double dValue)
{
   mValue = dValue;
}

void DmsPoint::setValue(const string& valueText)
{
   mValue = 0.0;
   bool finished(false);

   if (valueText.empty() == true)
   {
      return;
   }

   double degValue=0.0, minValue=0.0, secValue=0.0;
   char dirDelimiter[100]="N", degDelimiter[20]="d", minDelimiter[20]="m", secDelimiter[20]="s";
   char posCapIndicator[2]="N", negCapIndicator[2]="S";
   char posLcIndicator[2]="n", negLcIndicator[2]="s";
   int fieldCount=0;
   const char *format="%lg";

   if (mType == DMS_DECIMAL)
   {
      posCapIndicator[0] = ' ';
      negCapIndicator[0] = '-';
   }
   else if (mType == DMS_LATITUDE)
   {
      posCapIndicator[0] = 'N';
      negCapIndicator[0] = 'S';
   }
   else if (mType == DMS_LONGITUDE)
   {
      posCapIndicator[0] = 'E';
      negCapIndicator[0] = 'W';
   }

   posLcIndicator[0] = posCapIndicator[0]-'A'+'a';
   negLcIndicator[0] = negCapIndicator[0]-'A'+'a';

   string formatStr = string("%[") +
      posCapIndicator + negCapIndicator +
      posLcIndicator + negLcIndicator +
      "]%lg%[Dd�*\x20]%lg%[Mm'\x20]%lg%[Ss\"\x20]";

   format = formatStr.c_str();

   fieldCount = sscanf(valueText.c_str(), format, &dirDelimiter, 
      &degValue, degDelimiter,
      &minValue, minDelimiter,
      &secValue, secDelimiter);

   if (fieldCount < 2)
   {
      format = "%lg%[Dd�*\x20]%lg%[Mm'\x20]%lg%[Ss\"\x20]";

      fieldCount = sscanf(valueText.c_str(), format, 
         &degValue, degDelimiter,
         &minValue, minDelimiter,
         &secValue, secDelimiter);

      if (fieldCount <= 1)
      {
         mValue = degValue;
         finished = true;
      }
      else
      {
         fieldCount++;
         if (degValue >= 0.0)
         {
            dirDelimiter[0] = posCapIndicator[0];
         }
         else
         {
            dirDelimiter[0] = negCapIndicator[0];
         }
      }
   }

   if (!finished)
   {
      mValue = fabs(degValue);
      if (fieldCount >= 4)
         mValue += minValue / 60.0;
      if (fieldCount >= 6)
         mValue += secValue / 3600.0;

      if ((dirDelimiter[0] == negCapIndicator[0] || dirDelimiter[0] == negLcIndicator[0]))
         mValue = -mValue;
   }

   // check for concatenated format
   if (fabs(mValue) > 1000.0) // concatenated format parsed as decimal degrees
   {
      string strVal = valueText;
      int firstLoc = strVal.find_first_of('.');
      int lastLoc = strVal.find_last_of('.');

      if (firstLoc != lastLoc)  // bad entry, should only be one decimal point
      {
         mValue = 0.0;
         return;
      }

      if (lastLoc == string::npos) // no decimal point
      {
         lastLoc = strVal.length();
      }

      lastLoc -= 2; // back up to tens of seconds digit;
      strVal.insert(lastLoc,"m");
      lastLoc -= 2; // back up to tens of minutes digit
      strVal.insert(lastLoc,"d");
      setValue(strVal);
   }
}

double DmsPoint::getValue() const
{
   return mValue;
}

string DmsPoint::getValueText(DmsFormatType format) const
{
   bool bPositive = true;
   double dPositiveValue = mValue;

   if (mValue < 0.0)
   {
      bPositive = false;
      dPositiveValue = -mValue;
   }

   int iDegrees = (int) dPositiveValue;
   int iMinutes = (int) (60.0 * (dPositiveValue - iDegrees));
   double dSeconds = fabs((dPositiveValue - iDegrees - iMinutes / 60.0) * 3600.0);

   dSeconds = floor(1000.0 * dSeconds + 0.45) / 1000.0;

   if (dSeconds > 59.5)
   {
      iMinutes += 1;
      dSeconds = 0.0;
   }

   if (iMinutes == 60)
   {
      iDegrees += 1;
      iMinutes = 0;
   }

   char direction;
   if (bPositive == true)
   {
      if (mType == DmsPoint::DMS_DECIMAL) direction = ' ';
      else if (mType == DmsPoint::DMS_LATITUDE) direction = 'N';
      else if (mType == DmsPoint::DMS_LONGITUDE) direction = 'E';
   }
   else
   {
      if (mType == DmsPoint::DMS_DECIMAL) direction = '-';
      else if (mType == DmsPoint::DMS_LATITUDE) direction = 'S';
      else if (mType == DmsPoint::DMS_LONGITUDE) direction = 'W';
   }

   // put switch statement here to test various formatting of values

   char buffer[1024];
   if (dSeconds > 0.0)
   {
      // write out everything in full: degrees, minutes, and seconds
      switch(format)
      {
      case DMS_FULL:
         sprintf(buffer, "%c%d%s%d'%g\"", direction, iDegrees, DEG_CHAR.c_str(), iMinutes, dSeconds);
         break;
      case DMS_FULL_DECIMAL:
         sprintf(buffer, "%c%.3lf%s", direction, dPositiveValue, DEG_CHAR.c_str());
         break;
      case DMS_MINUTES_DECIMAL:
         sprintf(buffer, "%c%d%s%2.2lf'", direction, iDegrees, DEG_CHAR.c_str(), (60.0 * (dPositiveValue - iDegrees)) );
         break;
      default:
         return string("");
      }
   }
   else
   {
      // need to test if the minutes field needs to be written - seconds field doesn't need to be written, however
      switch(format)
      {
      case DMS_FULL:
         if (iMinutes > 0)
         {
            sprintf(buffer, "%c%d%s%d'", direction, iDegrees, DEG_CHAR.c_str(), iMinutes);
         }
         else
         {
            sprintf(buffer, "%c%d%s", direction, iDegrees, DEG_CHAR.c_str());
         }
         break;
      case DMS_FULL_DECIMAL:
         if (iMinutes > 0)
         {
            sprintf(buffer, "%c%.3f%s", direction, dPositiveValue, DEG_CHAR.c_str());
         }
         else
         {
            sprintf(buffer, "%c%.0lf%s", direction, dPositiveValue, DEG_CHAR.c_str());
         }
         break;
      case DMS_MINUTES_DECIMAL:
         if (iMinutes > 0)
         {
            sprintf(buffer, "%c%d%s%2.2lf'", direction, iDegrees, DEG_CHAR.c_str(), (60.0 * (dPositiveValue - iDegrees)) );
         }
         else
         {
            sprintf(buffer, "%c%.0lf%s", direction, dPositiveValue, DEG_CHAR.c_str());
         }
         break;
      default:
         return string("");
      }
   }

   return string(buffer);
}

DmsPoint& DmsPoint::operator =(const DmsPoint &original)
{
   if (this != &original)
   {
      this->mType = original.mType;
      this->mValue = original.mValue;
   }
   return *this;
}

bool DmsPoint::operator ==(const DmsPoint &rhs)
{
   return (this->mType == rhs.mType) && (this->mValue == rhs.mValue);
}

//////////////////
//  LatLonPoint //
//////////////////

LatLonPoint::LatLonPoint() :
   mLatitude(DmsPoint::DMS_LATITUDE),
   mLongitude(DmsPoint::DMS_LONGITUDE)
{
}

LatLonPoint::LatLonPoint(LocationType latLon) :
   mLatitude(DmsPoint::DMS_LATITUDE, latLon.mX),
   mLongitude(DmsPoint::DMS_LONGITUDE, latLon.mY)
{
}

LatLonPoint::LatLonPoint(const string& latitudeText, const string& longitudeText) :
   mLatitude(DmsPoint::DMS_LATITUDE, latitudeText),
   mLongitude(DmsPoint::DMS_LONGITUDE, longitudeText)
{
}

LatLonPoint::LatLonPoint(const string& latLonText) :
   mLatitude(DmsPoint::DMS_LATITUDE),
   mLongitude(DmsPoint::DMS_LONGITUDE)
{
   if (latLonText.empty() == false)
   {
      int iPos = -1;
      iPos = latLonText.find(", ");
      if (iPos != -1)
      {
         string latitudeText = latLonText.substr(0, iPos);
         string longitudeText = latLonText.substr(iPos + 2);

         mLatitude.setValue(latitudeText);
         mLongitude.setValue(longitudeText);
      }
   }
}

LatLonPoint::~LatLonPoint()
{
}

LocationType LatLonPoint::getCoordinates() const
{
   double dLatitude = mLatitude.getValue();
   double dLongitude = mLongitude.getValue();

   LocationType coords(dLatitude, dLongitude);
   return coords;
}

const DmsPoint& LatLonPoint::getLatitude() const
{
   return mLatitude;
}

const DmsPoint& LatLonPoint::getLongitude() const
{
   return mLongitude;
}

string LatLonPoint::getText(DmsFormatType format) const
{
   string latitudeText = getLatitudeText(format);
   string longitudeText = getLongitudeText(format);

   string latLonText = latitudeText + ", " + longitudeText;
   return latLonText;
}

string LatLonPoint::getLatitudeText(DmsFormatType format) const
{
   string latitudeText = mLatitude.getValueText(format);
   return latitudeText;
}

string LatLonPoint::getLongitudeText(DmsFormatType format) const
{
   string longitudeText = mLongitude.getValueText(format);
   return longitudeText;
}

LatLonPoint& LatLonPoint::operator =(const LatLonPoint &original)
{
   if (this != &original)
   {
      this->mLatitude = original.mLatitude;
      this->mLongitude = original.mLongitude;
   }
   return *this;
}

bool LatLonPoint::operator ==(const LatLonPoint &rhs)
{
   return (this->mLatitude == rhs.mLatitude) && (this->mLongitude == rhs.mLatitude);
}

///////////////
//  UtmPoint //
///////////////

UtmPoint::UtmPoint(LatLonPoint latLon)
{
   mEasting = 0.0;
   mNorthing = 0.0;
   mZone = 0;
   mHemisphere = 'N';

   MgrsEngine* pMgrsEngine = NULL;
   pMgrsEngine = MgrsEngine::Instance();
   if (pMgrsEngine != NULL)
   {
      pMgrsEngine->Initialize_Engine();

      // Setup inputs for the conversion
      MgrsEngine::Coordinate_Tuple inputCoords;
      MgrsEngine::Parameter_Tuple inputParams;

      inputCoords.Geodetic.longitude = latLon.getLongitude().getValue() * PI / 180.0;
      inputCoords.Geodetic.latitude = latLon.getLatitude().getValue() * PI / 180.0;
      inputCoords.Geodetic.height = 0;
      inputParams.Geodetic.height_type = MgrsEngine::No_Height;

      int datum_index = 0;
      pMgrsEngine->Get_Datum_Index("WGE", &datum_index);
      pMgrsEngine->Set_Datum(MgrsEngine::Interactive, MgrsEngine::Input, datum_index);
      pMgrsEngine->Set_Coordinate_System(MgrsEngine::Interactive, MgrsEngine::Input, MgrsEngine::Geodetic);
      pMgrsEngine->Set_Geodetic_Params(MgrsEngine::Interactive, MgrsEngine::Input, inputParams.Geodetic);
      pMgrsEngine->Set_Geodetic_Coordinates(MgrsEngine::Interactive, MgrsEngine::Input, inputCoords.Geodetic);

      // Setup outputs
      MgrsEngine::Coordinate_Tuple outputCoords;
      MgrsEngine::Parameter_Tuple outputParams;

      outputParams.UTM.zone = 0;
      outputParams.UTM.override = 0;

      pMgrsEngine->Set_Datum(pMgrsEngine->Interactive, pMgrsEngine->Output, datum_index);
      pMgrsEngine->Set_Coordinate_System(pMgrsEngine->Interactive, pMgrsEngine->Output, pMgrsEngine->UTM);
      pMgrsEngine->Set_UTM_Params(pMgrsEngine->Interactive, pMgrsEngine->Output, outputParams.UTM);

      // Convert to UTM
      pMgrsEngine->Convert();
      pMgrsEngine->Get_UTM_Coordinates(pMgrsEngine->Interactive, pMgrsEngine->Output, &outputCoords.UTM);

      // Set the member values
      mEasting = outputCoords.UTM.easting;
      mNorthing = outputCoords.UTM.northing;
      mZone = outputCoords.UTM.zone;
      mHemisphere = outputCoords.UTM.hemisphere;
   }
}

UtmPoint::UtmPoint(double dEasting, double dNorthing, int iZone, char hemisphere)
{
   mEasting = dEasting;
   mNorthing = dNorthing;
   mZone = iZone;
   mHemisphere = hemisphere;
}

UtmPoint::~UtmPoint()
{
}

LocationType UtmPoint::getCoordinates() const
{
   LocationType coords(mEasting, mNorthing);
   return coords;
}

LatLonPoint UtmPoint::getLatLonCoordinates() const
{
   MgrsEngine* pMgrsEngine = NULL;
   pMgrsEngine = MgrsEngine::Instance();
   if (pMgrsEngine != NULL)
   {
      pMgrsEngine->Initialize_Engine();

      // Setup inputs for the conversion
      MgrsEngine::Coordinate_Tuple inputCoords;
      MgrsEngine::Parameter_Tuple inputParams;

      inputCoords.UTM.easting = mEasting;
      inputCoords.UTM.northing = mNorthing;
      inputCoords.UTM.zone = mZone;
      inputCoords.UTM.hemisphere = mHemisphere;
      inputParams.UTM.zone = mZone;
      inputParams.UTM.override = 0;

      int datum_index = 0;
      pMgrsEngine->Get_Datum_Index("WGE", &datum_index);
      pMgrsEngine->Set_Datum(MgrsEngine::Interactive, MgrsEngine::Input, datum_index);
      pMgrsEngine->Set_Coordinate_System(MgrsEngine::Interactive, MgrsEngine::Input, MgrsEngine::UTM);
      pMgrsEngine->Set_UTM_Params(MgrsEngine::Interactive, MgrsEngine::Input, inputParams.UTM);
      pMgrsEngine->Set_UTM_Coordinates(MgrsEngine::Interactive, MgrsEngine::Input, inputCoords.UTM);

      // Setup outputs
      MgrsEngine::Coordinate_Tuple outputCoords;
      MgrsEngine::Parameter_Tuple outputParams;

      outputParams.Geodetic.height_type = MgrsEngine::No_Height;

      pMgrsEngine->Set_Datum(MgrsEngine::Interactive, MgrsEngine::Output, datum_index);
      pMgrsEngine->Set_Coordinate_System(MgrsEngine::Interactive, MgrsEngine::Output, MgrsEngine::Geodetic);
      pMgrsEngine->Set_Geodetic_Params(MgrsEngine::Interactive, MgrsEngine::Output, outputParams.Geodetic);

      // Convert to latitude/longitude
      pMgrsEngine->Convert();
      pMgrsEngine->Get_Geodetic_Coordinates(MgrsEngine::Interactive, MgrsEngine::Output, &outputCoords.Geodetic);

      LatLonPoint latLon(LocationType(outputCoords.Geodetic.latitude * 180.0 / PI,
         outputCoords.Geodetic.longitude * 180.0 / PI));

      return latLon;
   }

   return LatLonPoint(LocationType());
}

double UtmPoint::getEasting() const
{
   return mEasting;
}

double UtmPoint::getNorthing() const
{
   return mNorthing;
}

int UtmPoint::getZone() const
{
   return mZone;
}

const char UtmPoint::getHemisphere() const
{
   return mHemisphere;
}

string UtmPoint::getText() const
{
   string eastingText = getEastingText();
   string northingText = getNorthingText();
   string zoneText = getZoneText();

   string utmText = eastingText + " " + zoneText + ", " + northingText + " " + mHemisphere;
   return utmText;
}

string UtmPoint::getEastingText() const
{
   string eastingText = "";

   char buffer[1024];
   if (sprintf(buffer, "E%.0f", mEasting) > 0)
   {
      eastingText = buffer;
   }

   return eastingText;
}

string UtmPoint::getNorthingText() const
{
   string northingText = "";

   char buffer[1024];
   if (sprintf(buffer, "N%.0f", mNorthing) > 0)
   {
      northingText = buffer;
   }

   return northingText;
}

string UtmPoint::getZoneText() const
{
   string zoneText = "";

   char buffer[1024];
   if (sprintf(buffer, "Zone %d", mZone) > 0)
   {
      zoneText = buffer;
   }

   return zoneText;
}

////////////////
//  MgrsPoint //
////////////////

MgrsPoint::MgrsPoint(LatLonPoint latLon)
{
   MgrsEngine* pMgrsEngine = NULL;
   pMgrsEngine = MgrsEngine::Instance();
   if (pMgrsEngine != NULL)
   {
      pMgrsEngine->Initialize_Engine();

      // Setup inputs for the conversion
      MgrsEngine::Coordinate_Tuple inputCoords;
      MgrsEngine::Parameter_Tuple inputParams;

      inputCoords.Geodetic.longitude = latLon.getLongitude().getValue() * PI / 180.0;
      inputCoords.Geodetic.latitude = latLon.getLatitude().getValue() * PI / 180.0;
      inputCoords.Geodetic.height = 0;
      inputParams.Geodetic.height_type = MgrsEngine::No_Height;

      int datum_index = 0;
      pMgrsEngine->Get_Datum_Index("WGE", &datum_index);
      pMgrsEngine->Set_Datum(MgrsEngine::Interactive, MgrsEngine::Input, datum_index);
      pMgrsEngine->Set_Coordinate_System(MgrsEngine::Interactive, MgrsEngine::Input, MgrsEngine::Geodetic);
      pMgrsEngine->Set_Geodetic_Params(MgrsEngine::Interactive, MgrsEngine::Input, inputParams.Geodetic);
      pMgrsEngine->Set_Geodetic_Coordinates(MgrsEngine::Interactive, MgrsEngine::Input, inputCoords.Geodetic);

      // Setup outputs
      MgrsEngine::Coordinate_Tuple outputCoords;

      pMgrsEngine->Set_Datum(pMgrsEngine->Interactive, pMgrsEngine->Output, datum_index);
      pMgrsEngine->Set_Coordinate_System(pMgrsEngine->Interactive, pMgrsEngine->Output, pMgrsEngine->MGRS);

      // Convert to MGRS
      pMgrsEngine->Convert();
      pMgrsEngine->Get_MGRS_Coordinates(pMgrsEngine->Interactive, pMgrsEngine->Output, &outputCoords.MGRS);

      // Set the text value
      mText = outputCoords.MGRS.string;
   }
}

MgrsPoint::MgrsPoint(const string& mgrsText)
{
   mText = mgrsText;
}

MgrsPoint::~MgrsPoint()
{
}

LocationType MgrsPoint::getCoordinates() const
{
   LocationType coords(getEasting(), getNorthing());
   return coords;
}

LatLonPoint MgrsPoint::getLatLonCoordinates() const
{
   MgrsEngine* pMgrsEngine = NULL;
   pMgrsEngine = MgrsEngine::Instance();
   if (pMgrsEngine != NULL)
   {
      pMgrsEngine->Initialize_Engine();

      // Setup inputs for the conversion
      MgrsEngine::Coordinate_Tuple inputCoords;

      strcpy(inputCoords.MGRS.string, mText.c_str());

      pMgrsEngine->Set_Coordinate_System(MgrsEngine::Interactive, MgrsEngine::Input, MgrsEngine::MGRS);
      pMgrsEngine->Set_MGRS_Coordinates(MgrsEngine::Interactive, MgrsEngine::Input, inputCoords.MGRS);

      // Setup outputs
      MgrsEngine::Coordinate_Tuple outputCoords;
      MgrsEngine::Parameter_Tuple outputParams;

      outputParams.Geodetic.height_type = MgrsEngine::No_Height;

      pMgrsEngine->Set_Coordinate_System(MgrsEngine::Interactive, MgrsEngine::Output, MgrsEngine::Geodetic);
      pMgrsEngine->Set_Geodetic_Params(MgrsEngine::Interactive, MgrsEngine::Output, outputParams.Geodetic);

      // Convert to latitude/longitude
      pMgrsEngine->Convert();
      pMgrsEngine->Get_Geodetic_Coordinates(MgrsEngine::Interactive, MgrsEngine::Output, &outputCoords.Geodetic);

      LatLonPoint latLon(LocationType(outputCoords.Geodetic.latitude * 180.0 / PI,
         outputCoords.Geodetic.longitude * 180.0 / PI));

      return latLon;
   }

   return LatLonPoint(LocationType());
}

double MgrsPoint::getEasting() const
{
   double dEasting = 0.0;

   if (mText.empty() == false)
   {
      string scrCode = getScrCodeText();
      string coordsText = "";

      int iLength = 0;
      iLength = scrCode.length();
      if ((iLength % 2) == 0)
      {
         coordsText = mText.substr(4);
      }
      else
      {
         coordsText = mText.substr(5);
      }

      iLength = coordsText.length() / 2;

      string eastingText = coordsText.substr(0, iLength);
      dEasting = atof(eastingText.c_str());
   }

   return dEasting;
}

double MgrsPoint::getNorthing() const
{
   double dNorthing = 0.0;

   if (mText.empty() == false)
   {
      string scrCode = getScrCodeText();
      string coordsText = "";

      int iLength = 0;
      iLength = scrCode.length();
      if ((iLength % 2) == 0)
      {
         coordsText = mText.substr(4);
      }
      else
      {
         coordsText = mText.substr(5);
      }

      iLength = coordsText.length() / 2;

      string northingText = coordsText.substr(iLength);
      dNorthing = atof(northingText.c_str());
   }

   return dNorthing;
}

int MgrsPoint::getZone() const
{
   string zoneText = getZoneText();

   int iZone = 0;
   sscanf(zoneText.c_str(), "%d", &iZone);

   return iZone;
}

string MgrsPoint::getText() const
{
   return mText;
}

string MgrsPoint::getZoneText() const
{
   string zoneText = "";
   if (mText.empty() == false)
   {
      zoneText = mText.substr(0, 2);
   }

   return zoneText;
}

string MgrsPoint::getScrCodeText() const
{
   string scrCodeText = "";
   if (mText.empty() == false)
   {
      int iLength = 0;
      iLength = mText.length();
      if ((iLength % 2) == 0)
      {
         scrCodeText = mText.substr(2, 2);
      }
      else
      {
         scrCodeText = mText.substr(2, 3);
      }
   }

   return scrCodeText;
}
