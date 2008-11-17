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

WizardNodeImp::WizardNodeImp(WizardItem* pItem, string name, string type, string description) : 
   mpItem(pItem),
   mName(name),
   mType(type),
   mDescription(description),
   mOriginalType(type),
   mpShallowCopyValue(NULL)
{
   if (type.empty() == false)
   {
      mValidTypes.push_back(type);
   }
}

WizardNodeImp::~WizardNodeImp()
{
   notify(SIGNAL_NAME(Subject, Deleted));    // Call before clearing the connected nodes so that
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
      mDescription = node.mDescription.c_str();
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
      mName = nodeName;
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

      mType = nodeType;
      WizardNodeChangeType eChange = NodeType;
      notify(SIGNAL_NAME(Subject, Modified), boost::any(&eChange));
   }
}

const string& WizardNodeImp::getType() const
{
   return mType;
}

void WizardNodeImp::setDescription(const string& nodeDescription)
{
   if (nodeDescription != mDescription)
   {
      mDescription = nodeDescription;
      WizardNodeChangeType eChange = NodeDescription;
      notify(SIGNAL_NAME(Subject, Modified), boost::any(&eChange));
   }
}

const string& WizardNodeImp::getDescription() const
{
   return mDescription;
}

void WizardNodeImp::setOriginalType(const string& originalType)
{
   if (originalType != mOriginalType)
   {
      mOriginalType = originalType;
   }
}

const string& WizardNodeImp::getOriginalType() const
{
   return mOriginalType;
}

void WizardNodeImp::setValidTypes(const vector<string>& validTypes)
{
   mValidTypes.clear();

   for (vector<string>::const_iterator iter = validTypes.begin(); iter != validTypes.end(); ++iter)
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

   string type = pNode->getType();
   if (type != mType)
   {
      return false;
   }

   mConnectedNodes.push_back(pNode);
   WizardNodeChangeType eChange = ConnectedNodeAdded;
   notify(SIGNAL_NAME(Subject, Modified), boost::any(&eChange));

   WizardNodeImp* pNodeImp = static_cast<WizardNodeImp*>(pNode);
   if (pNodeImp != NULL)
   {
      pNodeImp->addConnectedNode(this);
   }

   return true;
}

int WizardNodeImp::getNumConnectedNodes() const
{
   int iCount = static_cast<int>(mConnectedNodes.size());
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

   vector<WizardNode*>::iterator iter = mConnectedNodes.begin();
   while (iter != mConnectedNodes.end())
   {
      WizardNode* pCurrentNode = *iter;
      if (pCurrentNode == pNode)
      {
         mConnectedNodes.erase(iter);
         WizardNodeChangeType eChange = ConnectedNodeRemoved;
         notify(SIGNAL_NAME(Subject, Modified), boost::any(&eChange));

         WizardNodeImp* pNodeImp = static_cast<WizardNodeImp*>(pNode);
         if (pNodeImp != NULL)
         {
            pNodeImp->removeConnectedNode(this);
         }

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

   vector<WizardNode*>::const_iterator iter = mConnectedNodes.begin();
   while (iter != mConnectedNodes.end())
   {
      WizardNode* pCurrentNode = *iter;
      if (pCurrentNode == pNode)
      {
         return true;
      }

      iter++;
   }

   return false;
}

bool WizardNodeImp::toXml(XMLWriter* pXml) const
{
   // This will get all of the infomation for the wizard node
   // except the description.
   pXml->addAttr("version", mVersion);
   pXml->addAttr("name", mName);
   pXml->addAttr("originalType", mOriginalType);
   pXml->addAttr("type", mType);

   for (vector<string>::const_iterator iter = mValidTypes.begin(); iter != mValidTypes.end(); ++iter)
   {
      pXml->addText(*iter, pXml->addElement("validType"));
   }

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
      pXml->addText(value, pXml->addElement("value"));
   }

   return true;
}

bool WizardNodeImp::fromXml(DOMNode* pDocument, unsigned int version)
{
   DOMElement* elmnt(static_cast<DOMElement*>(pDocument));

   mVersion = A(elmnt->getAttribute(X("version")));
   mName = A(elmnt->getAttribute(X("name")));
   mOriginalType = A(elmnt->getAttribute(X("originalType")));
   mType = A(elmnt->getAttribute(X("type")));
   deleteValue();
   mValidTypes.clear();

   for (DOMNode* pChld = pDocument->getFirstChild(); pChld != NULL; pChld = pChld->getNextSibling())
   {
      if (XMLString::equals(pChld->getNodeName(), X("validType")))
      {
         string vt(A(pChld->getTextContent()));
         mValidTypes.push_back(vt);
      }
      else if (XMLString::equals(pChld->getNodeName(), X("value")))
      {
         string value(A(pChld->getTextContent()));

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
   static string sType("WizardNodeImp");
   return sType;
}

bool WizardNodeImp::isKindOf(const string& className) const
{
   if ((className == getType()) || (className == "WizardNode"))
   {
      return true;
   }

   return SubjectImp::isKindOf(className);
}
