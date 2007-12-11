/*
 * The information in this file is
 * Copyright(c) 2007 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "AppVerify.h"
#include "DataElement.h"
#include "GraphicElementImp.h"
#include "GraphicGroupImp.h"
#include "RasterElement.h"

#include <string>
using namespace std;

XERCES_CPP_NAMESPACE_USE

GraphicElementImp::GraphicElementImp(const DataDescriptorImp& descriptor, const string& id) :
   DataElementImp(descriptor, id), mpGroup(GROUP_OBJECT), mInteractive(true), mpGeocentricSource(
   SIGNAL_NAME(RasterElement, GeoreferenceModified), Slot(this, &GraphicElementImp::georeferenceModified))
{
   Subject *pSub = dynamic_cast<Subject*>(mpGroup.get());
   pSub->attach(SIGNAL_NAME(Subject, Modified), Slot(this, &GraphicElementImp::groupModified));
}

GraphicElementImp::~GraphicElementImp()
{
}

bool GraphicElementImp::toXml(XMLWriter* pXml) const
{
   if (!DataElementImp::toXml(pXml))
   {
      return false;
   }
   pXml->addAttr("geocentric", (mpGeocentricSource.get() != NULL) ? "true" : "false");

   pXml->pushAddPoint(pXml->addElement("group"));
   const GraphicGroupImp *pGroup = dynamic_cast<const GraphicGroupImp*>(mpGroup.get());
   VERIFY(pGroup != NULL);

   bool success = pGroup->toXml(pXml);
   pXml->popAddPoint();
   return success;
}

bool GraphicElementImp::fromXml(DOMNode* pDocument, unsigned int version)
{
   if (!DataElementImp::fromXml(pDocument, version))
   {
      return false;
   }

   DOMElement* pElement = static_cast<DOMElement*> (pDocument);
   if (pElement != NULL)
   {
      string geocentric(A(pElement->getAttribute(X("geocentric"))));
      if ((geocentric == "true") || (geocentric == "1"))
      {
         if (!setGeocentric(true))
         {
            return false;
         }
      }
   }

   DOMNode *pGroupNode = NULL;
   for (pGroupNode = pDocument->getFirstChild();
      pGroupNode != NULL; pGroupNode = pGroupNode->getNextSibling())
   {
      if (XMLString::equals(pGroupNode->getNodeName(), X("group")))
      {
         break;
      }
   }
   GraphicGroupImp *pGroup = dynamic_cast<GraphicGroupImp*>(mpGroup.get());
   VERIFY(pGroup != NULL);
   bool success = pGroup->fromXml(pGroupNode, version);
   return success;
}

const string& GraphicElementImp::getObjectType() const
{
   static string type("GraphicElementImp");
   return type;
}

bool GraphicElementImp::isKindOf(const string& className) const
{
   if ((className == getObjectType()) || (className == "GraphicElement"))
   {
      return true;
   }

   return DataElementImp::isKindOf(className);
}

void GraphicElementImp::getElementTypes(vector<string>& classList)
{
   classList.push_back("GraphicElement");
   DataElementImp::getElementTypes(classList);
}

bool GraphicElementImp::isKindOfElement(const string& className)
{
   if ((className == "GraphicElementImp") || (className == "GraphicElement"))
   {
      return true;
   }

   return DataElementImp::isKindOfElement(className);
}

DataElement* GraphicElementImp::copy(const string& name, DataElement* pParent) const
{
   DataElement* pElement = DataElementImp::copy(name, pParent);

   GraphicElementImp* pGraphic = dynamic_cast<GraphicElementImp*>(pElement);
   if (pGraphic != NULL)
   {
      dynamic_cast<GraphicGroupImp*>(pGraphic->mpGroup.get())->operator =(
         *dynamic_cast<const GraphicGroupImp*>(getGroup()));
   }

   return pElement;
}

GraphicGroup *GraphicElementImp::getGroup()
{
   return mpGroup.get();
}

const GraphicGroup *GraphicElementImp::getGroup() const
{
   return mpGroup.get();
}

void GraphicElementImp::groupModified(Subject &subject, const string &signal, const boost::any &data)
{
   if (mInteractive && &subject == dynamic_cast<Subject*>(mpGroup.get()))
   {
      notify(SIGNAL_NAME(Subject, Modified), data);
   }
}

void GraphicElementImp::setInteractive(bool interactive)
{
   bool oldInteractive = mInteractive;
   mInteractive = interactive;
   if (oldInteractive == false && interactive == true)
   {
      notify(SIGNAL_NAME(Subject, Modified), boost::any());
      GraphicGroupImp *pGroup = dynamic_cast<GraphicGroupImp*>(getGroup());
      if (pGroup != NULL)
      {
         pGroup->updateBoundingBox();
      }
   }
}

bool GraphicElementImp::getInteractive() const
{
   return mInteractive;
}

bool GraphicElementImp::setGeocentric(bool geocentric)
{
   if (!geocentric)
   {
      if (mpGeocentricSource.get() != NULL)
      {
         mpGeocentricSource.reset(NULL);
         notify(SIGNAL_NAME(Subject, Modified));
      }
   }
   else
   {
      if (mpGeocentricSource.get() == NULL)
      {
         const RasterElement *pGeocentricSource = getGeoreferenceElement(false);
         if (pGeocentricSource == NULL)
         {
            return false;
         }
         mpGeocentricSource.reset(const_cast<RasterElement*>(pGeocentricSource));
         GraphicGroupImp *pGroup = dynamic_cast<GraphicGroupImp*>(getGroup());
         VERIFY(pGroup != NULL);
         pGroup->enableGeo();
      }
   }

   return true;
}

bool GraphicElementImp::getGeocentric() const
{
   if (mpGeocentricSource.get() != NULL && mpGeocentricSource->isGeoreferenced())
   {
      return true;
   }
   return false;
}

const RasterElement *GraphicElementImp::getGeoreferenceElement(bool onlyIfGeocentric) const
{
   bool geocentric = getGeocentric();
   if (!onlyIfGeocentric || getGeocentric())
   {
      RasterElement *pGeoreferenceElement = dynamic_cast<RasterElement*>(getParent());
      if (pGeoreferenceElement == NULL || !pGeoreferenceElement->isGeoreferenced())
      {
         return NULL;
      }

      return pGeoreferenceElement;
   }

   return NULL;
}

void GraphicElementImp::georeferenceModified(Subject &subject, const std::string &signal, const boost::any &data)
{
   GraphicGroupImp *pGroup = dynamic_cast<GraphicGroupImp*>(getGroup());
   VERIFYNRV(pGroup != NULL);

   bool interactive = getInteractive();
   setInteractive(false);
   pGroup->updateGeo();
   setInteractive(interactive);
}
