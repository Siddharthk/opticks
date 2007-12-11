/*
 * The information in this file is
 * Copyright(c) 2007 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "WizardNodeImp.h"
#include "DateTimeImp.h"
#include "DynamicObjectAdapter.h"
#include "FilenameImp.h"
#include "StringUtilities.h"
#include "xmlreader.h"
#include "xmlwriter.h"

#include <sstream>
using namespace std;
XERCES_CPP_NAMESPACE_USE

string WizardNodeImp::mVersion = "Wizard Node Version 3.0";

WizardNodeImp::WizardNodeImp(WizardItem* pItem, string name, string type)
{
   mpItem = pItem;
   mName = name;
   mType = type;
   mOriginalType = type;
   mpShallowCopyValue = NULL;

   if (type.empty() == false)
   {
      mValidTypes.push_back(type);
   }
}

WizardNodeImp::~WizardNodeImp()
{
   notify(SIGNAL_NAME(Subject, Deleted));   // Call before clearing the connected nodes so that
                                       // connected objects can have access to them
   
   clearConnectedNodes();
   deleteValue();
}

WizardNodeImp& WizardNodeImp::operator =(const WizardNodeImp& node)
{
   if (this != &node)
   {
      mName = node.mName.c_str();
      mType = node.mType.c_str();
      mOriginalType = node.mOriginalType.c_str();
      setValidTypes(node.mValidTypes);
      setValue(node.mpShallowCopyValue);
   }

   return *this;
}

WizardItem* WizardNodeImp::getItem() const
{
   return mpItem;
}

void WizardNodeImp::setName(const string& nodeName)
{
   if (nodeName != mName)
   {
      mName = "" + nodeName;
      WizardNodeChangeType eChange = NodeName;
      notify(SIGNAL_NAME(Subject, Modified), boost::any(&eChange));
   }
}

const string& WizardNodeImp::getName() const
{
   return mName;
}

void WizardNodeImp::setType(const string& nodeType)
{
   if (nodeType != mType)
   {
      if (mpShallowCopyValue != NULL)
      {
         deleteValue();
      }

      mType = "" + nodeType;
      WizardNodeChangeType eChange = NodeType;
      notify(SIGNAL_NAME(Subject, Modified), boost::any(&eChange));
   }
}

const string& WizardNodeImp::getType() const
{
   return mType;
}

void WizardNodeImp::setOriginalType(const string& originalType)
{
   if (originalType != mOriginalType)
   {
      mOriginalType = "" + originalType;
   }
}

const string& WizardNodeImp::getOriginalType() const
{
   return mOriginalType;
}

void WizardNodeImp::setValidTypes(const vector<string>& validTypes)
{
   mValidTypes.clear();

   vector<string>::const_iterator iter;
   for (iter = validTypes.begin(); iter != validTypes.end(); iter++)
   {
      string type = (*iter).c_str();
      mValidTypes.push_back(type);
   }

   if (mValidTypes.empty() == true)
   {
      mValidTypes.push_back(mOriginalType);
   }
}

const vector<string>& WizardNodeImp::getValidTypes() const
{
   return mValidTypes;
}

void WizardNodeImp::setValue(void* pValue)
{
   if (pValue != mpShallowCopyValue)
   {
      if (mpShallowCopyValue != NULL)
      {
         deleteValue();
      }

      if (pValue != NULL)
      {
         mDeepCopyValue = DataVariant(mType, pValue, false);
         if (mDeepCopyValue.isValid())
         {
            mpShallowCopyValue = mDeepCopyValue.getPointerToValueAsVoid();
         }
         else
         {
            mpShallowCopyValue = pValue;
         }
      }
      WizardNodeChangeType eChange = NodeValue;
      notify(SIGNAL_NAME(Subject, Modified), boost::any(&eChange));
   }
}

void* WizardNodeImp::getValue() const
{
   return mpShallowCopyValue;
}

void WizardNodeImp::deleteValue()
{
   if (mDeepCopyValue.isValid())
   {
      mDeepCopyValue = DataVariant();
   }
   mpShallowCopyValue = NULL;
}

bool WizardNodeImp::addConnectedNode(WizardNode* pNode)
{
   if (pNode == NULL)
   {
      return false;
   }

   if (isNodeConnected(pNode) == true)
   {
      return false;
   }

   string type = "";
   type = pNode->getType();
   if (type != mType)
   {
      return false;
   }

   mConnectedNodes.push_back(pNode);
   WizardNodeChangeType eChange = ConnectedNodeAdded;
   notify(SIGNAL_NAME(Subject, Modified), boost::any(&eChange));
   ((WizardNodeImp*) pNode)->addConnectedNode(this);
   return true;
}

int WizardNodeImp::getNumConnectedNodes() const
{
   int iCount = 0;
   iCount = mConnectedNodes.size();

   return iCount;
}

const vector<WizardNode*>& WizardNodeImp::getConnectedNodes() const
{
   return mConnectedNodes;
}

bool WizardNodeImp::removeConnectedNode(WizardNode* pNode)
{
   if (pNode == NULL)
   {
      return false;
   }

   vector<WizardNode*>::iterator iter;
   iter = mConnectedNodes.begin();
   while (iter != mConnectedNodes.end())
   {
      WizardNode* pCurrentNode = NULL;
      pCurrentNode = *iter;
      if (pCurrentNode == pNode)
      {
         mConnectedNodes.erase(iter);
         WizardNodeChangeType eChange = ConnectedNodeRemoved;
         notify(SIGNAL_NAME(Subject, Modified), boost::any(&eChange));
         ((WizardNodeImp*) pNode)->removeConnectedNode(this);
         return true;
      }

      iter++;
   }

   return false;
}

void WizardNodeImp::clearConnectedNodes()
{
   while (mConnectedNodes.empty() == false)
   {
      WizardNode* pNode = mConnectedNodes.front();
      if (pNode != NULL)
      {
         bool bSuccess = removeConnectedNode(pNode);
         if (bSuccess == false)
         {
            break;
         }
      }
   }
}

bool WizardNodeImp::isNodeConnected(WizardNode* pNode) const
{
   if (pNode == NULL)
   {
      return false;
   }

   vector<WizardNode*>::const_iterator iter;
   iter = mConnectedNodes.begin();
   while (iter != mConnectedNodes.end())
   {
      WizardNode* pCurrentNode = NULL;
      pCurrentNode = *iter;
      if (pCurrentNode == pNode)
      {
         return true;
      }

      iter++;
   }

   return false;
}

bool WizardNodeImp::toXml(XMLWriter* xml) const
{
   xml->addAttr("version", mVersion);
   xml->addAttr("name", mName);
   xml->addAttr("originalType", mOriginalType);
   xml->addAttr("type", mType);

   vector<string>::const_iterator iter;
   for(iter = mValidTypes.begin(); iter != mValidTypes.end(); iter++)
      xml->addText(*iter, xml->addElement("validType"));

   if (mpShallowCopyValue != NULL)
   {
      DataVariant var(mType, mpShallowCopyValue, false);
      if (!var.isValid())
      {
         return false;
      }
      DataVariant::Status status;
      string value = var.toXmlString(&status);
      if (status == DataVariant::FAILURE)
      {
         return false;
      }
      xml->addText(value, xml->addElement("value"));
   }

   return true;
}

bool WizardNodeImp::fromXml(DOMNode* document, unsigned int version)
{
   DOMElement *elmnt(static_cast<DOMElement*>(document));

   mVersion = A(elmnt->getAttribute(X("version")));
   mName = A(elmnt->getAttribute(X("name")));
   mOriginalType = A(elmnt->getAttribute(X("originalType")));
   mType = A(elmnt->getAttribute(X("type")));
   deleteValue();
   mValidTypes.clear();

   for(DOMNode *chld = document->getFirstChild();
                chld != NULL;
                chld = chld->getNextSibling())
   {
      if(XMLString::equals(chld->getNodeName(), X("validType")))
      {
         string vt(A(chld->getTextContent()));
         mValidTypes.push_back(vt);
      }
      else if(XMLString::equals(chld->getNodeName(), X("value")))
      {
         string value(A(chld->getTextContent()));

         DataVariant parsedValue(mType, NULL, false);
         if (!parsedValue.isValid())
         {
            return false;
         }
         DataVariant::Status status = parsedValue.fromXmlString(mType, value);
         if (status != DataVariant::SUCCESS)
         {
            return false;
         }
         mDeepCopyValue = parsedValue;
         mpShallowCopyValue = mDeepCopyValue.getPointerToValueAsVoid();
      }
   }

   notify(SIGNAL_NAME(Subject, Modified));
   return true;
}

const string& WizardNodeImp::getObjectType() const
{
   static string type("WizardNodeImp");
   return type;
}

bool WizardNodeImp::isKindOf(const string& className) const
{
   if ((className == getType()) || (className == "WizardNode"))
   {
      return true;
   }

   return SubjectImp::isKindOf(className);
}
