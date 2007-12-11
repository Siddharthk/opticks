/*
 * The information in this file is
 * Copyright(c) 2007 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include <QtCore/QCoreApplication>

#include "AppConfig.h"
#include "AppVersion.h"
#include "ArgumentList.h"
#include "BatchApplication.h"
#include "ConfigurationSettingsImp.h"
#include "DateTime.h"
#include "PlugInBranding.h"
#include "StringUtilities.h"
#include "SystemServicesImp.h"

#include <string>
#include <iostream>

using namespace std;

// Defines the entry point for the Batch Processor
int main(int argc, char** argv)
{
   SystemServicesImp::instance()->WriteLogInfo(string(APP_NAME) + " Batch Startup");

   //Need to initialize QCoreApplication so that ConfigSettingsImp
   //can use QCoreApplication::applicationDirPath()
   QCoreApplication qApp(argc, argv);

   // Register the command line options
   ArgumentList* pArgumentList = NULL;
   pArgumentList = ArgumentList::instance();
   if (pArgumentList == NULL)
   {
      SystemServicesImp::instance()->WriteLogInfo(string(APP_NAME) + " Batch shutdown");
      return -1;
   }
   pArgumentList->registerOption("brief");
   pArgumentList->registerOption("configDir");
   pArgumentList->registerOption("defaultDir");
   pArgumentList->registerOption("input");
   pArgumentList->registerOption("generate");
   pArgumentList->registerOption("processors");
   pArgumentList->registerOption("version");
   pArgumentList->registerOption("help");
   pArgumentList->registerOption("h");
   pArgumentList->registerOption("?");
   pArgumentList->set(argc, argv);

   BatchApplication* pApp = new BatchApplication(qApp);

   // Display application information
   bool bProductionRelease = false;
   string releaseType;
   bool configSettingsValid = false;
   string configSettingsErrorMsg = "";

   ConfigurationSettingsImp* pSettings = NULL;
   pSettings = ConfigurationSettingsImp::instance();
   if (pSettings != NULL)
   {
      bProductionRelease = pSettings->isProductionRelease();
      releaseType = StringUtilities::toDisplayString(pSettings->getReleaseType());
      configSettingsValid = pSettings->isInitialized();
      if (pSettings->getInitializationErrorMsg() != NULL)
      {
         configSettingsErrorMsg = pSettings->getInitializationErrorMsg();
      }
   }

   if (!configSettingsValid)
   {
      if (configSettingsErrorMsg.empty())
      {
         configSettingsErrorMsg = "Unable to locate configuration settings";
      }
      cerr << "ERROR: " << configSettingsErrorMsg << endl;
      SystemServicesImp::instance()->WriteLogInfo(string(APP_NAME) + " Batch shutdown");
      return -1;
   }
   else
   {
      if (!configSettingsErrorMsg.empty())
      {
         cerr << "WARNING: " << configSettingsErrorMsg << endl;
      }
   }

   string version = pSettings->getVersion();
   version += " Build " + pSettings->getBuildRevision();
   const DateTime* pDate = pSettings->getReleaseDate();

   string dateString;
   string format = "%d %b %Y";
   dateString = pDate->getFormattedUtc(format);

   cout << endl;
   cout << APP_NAME_LONG << endl;
   cout << APP_NAME << " Batch Processor, Version " << version << ", Release Date " << dateString << endl;
   cout << APP_COPYRIGHT << endl;

   if (bProductionRelease == false)
   {
      cout << releaseType << " - Not for Production Use" << endl;
   }

   const vector<PlugInBranding>& brandings = PlugInBranding::getBrandings();
   if (!brandings.empty())
   {
      cout << "Plug-In Suites:" << endl;
      for (vector<PlugInBranding>::const_iterator brandingIter = brandings.begin();
           brandingIter != brandings.end();
           ++brandingIter)
      {
         cout << "   " << brandingIter->getTitle() << ", Version " << brandingIter->getVersion() << endl;
      }
   }

   // Display help
   if ((pArgumentList->exists("help") == true) || (pArgumentList->exists("h") == true) ||
      (pArgumentList->exists("?") == true))
   {
      string dlm = pArgumentList->getDelimiter();
      cout << endl << "batch " << dlm << "input:filename.batchwiz " << dlm << "input:filename.batchwiz " << dlm << "generate:filename.wiz" << endl;
      cout << "     " << dlm << "input       The batch file to process" << endl;
      cout << "     " << dlm << "generate    Generates a batch file based on the given wizard 'filename.batchwiz'" << endl;
      cout << "     " << dlm << "brief       Displays brief output messages" << endl;
      cout << "     " << dlm << "processors  Sets number of available processors" << endl;
      //cout << "     " << dlm << "test        Runs a set of operational tests" << endl;
      //cout << "     " << dlm << "testAll     Runs the full set of system tests" << endl;
      cout << "     " << dlm << "version     Displays a message listing the version of each Plug-In" << endl;
      cout << "     " << dlm << "help        Displays this help message" << endl;
      SystemServicesImp::instance()->WriteLogInfo(string(APP_NAME) + " Batch shutdown");
      return 0;
   }

   // Run the application
   int iSuccess = -1;

   if (pApp != NULL)
   {
      if (pArgumentList->exists("version")==true) 
      {
         iSuccess = pApp->version(argc,argv);
      }
      else if (pArgumentList->exists("test")==true)
      {
         iSuccess = pApp->test(argc, argv);
      }
      else
      {
         iSuccess = pApp->run(argc, argv);
      }

      delete pApp;
   }

   // Display developer's release information again if necessary
   if (bProductionRelease == false)
   {
      cout << releaseType << " - Not for Production Use" << endl;
   }

   SystemServicesImp::instance()->WriteLogInfo(string(APP_NAME) + " Batch Shutdown");
   return iSuccess;
}
