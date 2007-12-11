/*
 * The information in this file is
 * Copyright(c) 2007 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */
 


#ifndef SCRIPTPLUGIN_H
#define SCRIPTPLUGIN_H

#include "AlgorithmShell.h"
#include "Descriptors.h"

class ScriptPlugIn : public AlgorithmShell
{
public:
   ScriptPlugIn(int scriptIndex);
   ~ScriptPlugIn();

   bool isInputValid(PlugInArgList *);
   bool setBatch();
   bool setInteractive();
   bool getInputSpecification(PlugInArgList *&);
   bool getOutputSpecification(PlugInArgList *&);
   bool execute(PlugInArgList *, PlugInArgList *);
   bool initialize();
   bool hasAbort() { return false; };
   bool abort();

private:
   bool populateOutputArgList(PlugInArgList *pOutArgList);

   Descriptor mDescriptor;
};

#endif   // SCRIPTPLUGIN_H

 
