/*
 * The information in this file is
 * Copyright(c) 2007 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "ApplicationServices.h"
#include "AppConfig.h"
#include "AppAssert.h"
#include "DateTimeImp.h"
#include "DynamicObjectAdapter.h"
#include "FilenameImp.h"
#include "MessageLogAdapter.h"
#include "xmlwriter.h"

#include <sstream>
#include <iostream>

#include <QtCore/QString>
#include <QtCore/QTemporaryFile>

using namespace std;
XERCES_CPP_NAMESPACE_USE

MessageLogImp::MessageLogImp(const char* name, const char* path, QFile *journal) :
         mpLogName(name),
         mpCurrentStep(NULL),
         mpJournal(journal),
         mpWriter(NULL)
{
   mpFilename = new FilenameImp(path);
   //determine the path + filename of the log
   string fname(name);
   string jname;
   int lth = fname.length();
   int x = fname.find(":");
   while(x >= 0)
   {
      fname.replace(x, 1, "_");
      x = fname.find(":", x + 1);
   }

   x = fname.find("\\", 0);
   while(x >= 0)
   {
      fname.replace(x, 1, "_");
      x = fname.find("\\", x + 1);
   }

   x = fname.find("/", 0);
   while(x >= 0)
   {
      fname.replace(x, 1, "_");
      x = fname.find("/", x + 1);
   }
 
   string extension = ".log";
   if ((path[strlen(path)-1] != string("\\").at(0)) && (path[strlen(path)-1] != string("/").at(0)))
   {
      fname = (string)path + SLASH + fname + extension;
   }
   else
   {
      fname = (string)path + fname + extension;
   }
   mpJournalWriter = new QTextStream(mpJournal);

   QTemporaryFile *pTempFile = new QTemporaryFile(QString::fromStdString(fname));
   if(pTempFile != NULL)
   {
      pTempFile->setAutoRemove(false);
      mpLogFile = pTempFile;
   }
   if((mpLogFile == NULL) || !mpLogFile->open(QIODevice::WriteOnly | QIODevice::Append))
   {
      Service<ApplicationServices> pApplication;
      if (pApplication->isBatch() == true)
      {
         string msg("Unable to open log file ");
         msg += mpLogFile->fileName().toStdString();
         msg += ": ";
         msg += mpLogFile->errorString().toStdString();
         // if we are in batch mode, output a message to stderr
         cerr << msg << endl;
      }

      delete mpLogFile;
      mpLogFile = NULL;
   }

   mpWriter = new XMLWriter("messagelog");
   Message *pOpen(createMessage("Log Opened", "app", "EC355E3E-03CA-4081-9006-5F45D6A488B3"));
   pOpen->finalize();
}

MessageLogImp::~MessageLogImp()
{
   MessageAdapter *pClosed(new MessageAdapter("Log Closed", "app", "3620CAD7-3535-4716-9686-E024E201481F"));
   pClosed->finalize();
   pClosed->getId()(size()+1);
   if((mpWriter != NULL) && (mpLogFile != NULL))
   {
      QTextStream stream(mpLogFile);
      stream << serialize().c_str();
   }
   if(mpWriter != NULL)
   {
      delete mpWriter;
      mpWriter = NULL;
   }

   if(mpLogFile != NULL)
   {
      mpLogFile->close();
      delete mpLogFile;
      mpLogFile = NULL;
   }
   if(mpJournalWriter != NULL)
   {
      delete mpJournalWriter;
      mpJournalWriter = NULL;
   }

   if(mpFilename != NULL)
   {
      delete dynamic_cast<FilenameImp*>(mpFilename);
      mpFilename = NULL;
   }

   //clean-up all of the underlying messages
   vector<Message*>::iterator messageIter;
   for (messageIter = mMessageList.begin(); messageIter != mMessageList.end(); ++messageIter)
   {
      Message* pCurMsg = *messageIter;
      if (pCurMsg != NULL)
      {
         MessageImp* pCurMsgImp = dynamic_cast<MessageImp*>(pCurMsg);
         if (pCurMsgImp != NULL)
         {
            delete pCurMsgImp;
         }
      }
   }
   mMessageList.clear();

   notify(SIGNAL_NAME(Subject, Deleted));
}

Message *MessageLogImp::createMessage(const string &action,
                                      const string &component,
                                      const string &key,
                                      bool finalizeOnCreate,
                                      bool recurse)
{
   Message *msg(NULL);
   if(recurse && mpCurrentStep != NULL)
   {
      msg = mpCurrentStep->addMessage(action,component,key,finalizeOnCreate,true);
      if(msg == NULL)
      {
         mpCurrentStep = NULL;
         msg = createMessage(action,component,key,finalizeOnCreate,false);
      }
   }
   else
   {
      MessageAdapter *pMsgAdapter(new MessageAdapter(action,component,key));
      msg = pMsgAdapter;
      if(finalizeOnCreate)
         pMsgAdapter->finalize();
      mMessageList.push_back(msg);
      pMsgAdapter->getId()(size());
      pMsgAdapter->attach(SIGNAL_NAME(Message, MessageModified), Slot(this, &MessageLogImp::messageModified));
      pMsgAdapter->attach(SIGNAL_NAME(Message, Hidden), Slot(this, &MessageLogImp::messageHidden));
      // The next line of code must be as it is. It cannot be dynamic_cast<Subject*>(this) because
      // we can get here from MessageLogImp's ctor where the d_c to Subject will fail.
      Subject *pSubject = static_cast<MessageLogAdapter*>(this);
      VERIFYRV(pSubject != NULL, NULL);
      messageAdded(*pSubject, SIGNAL_NAME(MessageLog, MessageAdded), boost::any(msg));
   }
   return msg;
}

Step *MessageLogImp::createStep(const string &action,
                                const string &component,
                                const string &key,
                                bool recurse)
{
   Step *stp(NULL);
   if(recurse && mpCurrentStep != NULL)
   {
      stp = static_cast<StepAdapter*>(mpCurrentStep->addStep(action,component,key,true));
      if(stp == NULL)
      {
         mpCurrentStep = NULL;
         return createStep(action,component,key,false);
      }
   }
   else
   {
      StepAdapter *pStpAdapter(new StepAdapter(action, component, key));
      stp = pStpAdapter;
      mMessageList.push_back(static_cast<Message*>(stp));
      mpCurrentStep = stp;
      pStpAdapter->getId()(size());
      pStpAdapter->attach(SIGNAL_NAME(Message, MessageModified), Slot(this, &MessageLogImp::messageModified));
      pStpAdapter->attach(SIGNAL_NAME(Message, Hidden), Slot(this, &MessageLogImp::messageHidden));
      pStpAdapter->attach(SIGNAL_NAME(Step, MessageAdded), Slot(this, &MessageLogImp::messageAdded));
      pStpAdapter->attach(SIGNAL_NAME(Step, MessageHidden), Slot(this, &MessageLogImp::messageHidden));
      pStpAdapter->attach(SIGNAL_NAME(Step, MessageDeleted), Slot(this, &MessageLogImp::messageDeleted));
      // The next line of code must be as it is. It cannot be dynamic_cast<Subject*>(this) because
      // we can get here from MessageLogImp's ctor where the d_c to Subject will fail.
      Subject *pSubject = static_cast<MessageLogAdapter*>(this);
      VERIFYRV(pSubject != NULL, NULL);
      messageAdded(*pSubject, SIGNAL_NAME(MessageLog, MessageAdded), boost::any(static_cast<Message*>(stp)));
   }
   return stp;
}

MessageLog::size_t MessageLogImp::size() const
{
   return mMessageList.size();
}

MessageLog::iterator MessageLogImp::begin()
{
   return mMessageList.begin();
}

MessageLog::const_iterator MessageLogImp::begin() const
{
   return mMessageList.begin();
}

MessageLog::iterator MessageLogImp::end()
{
   return mMessageList.end();
}

MessageLog::const_iterator MessageLogImp::end() const
{
   return mMessageList.end();
}

Message *MessageLogImp::operator[](MessageLog::size_t i)
{
   return mMessageList[i];
}

const Message *MessageLogImp::operator[](MessageLog::size_t i) const
{
   return mMessageList[i];
}

const string& MessageLogImp::getObjectType() const
{
   static string type("MessageLogImp");
   return type;
}

bool MessageLogImp::isKindOf(const string& name) const
{
   if ((name == getObjectType()) || (name == "MessageLog"))
   {
      return true;
   }

   return SubjectImp::isKindOf(name);
}

const string& MessageLogImp::getLogName()
{
   return mpLogName;
}

void MessageLogImp::messageAdded(Subject &subject, const string &signal, const boost::any &v)
{
   Message *pMsg(boost::any_cast<Message*>(v));
   if(mpJournalWriter == NULL || pMsg == NULL)
      return;
   Step *pStp(dynamic_cast<Step*>(pMsg));
   StepImp *pStpImp(dynamic_cast<StepImp*>(pStp));
   MessageImp *pMsgImp(dynamic_cast<MessageImp*>(pMsg));
   *mpJournalWriter << mpLogName.c_str() << " - ADDED "
      << ((pStpImp != NULL) ? "Step" : "Message")
      << "[" << ((pStpImp != NULL) ? pStpImp : pMsgImp)->getStringId().c_str() << "] "
      << pMsg->getAction().c_str() << endl << flush;
   notify(SIGNAL_NAME(MessageLog, MessageAdded), v);
}

void MessageLogImp::messageModified(Subject &subject, const string &signal, const boost::any &v)
{
   Message *pMsg(boost::any_cast<Message*>(v));
   if(mpJournalWriter == NULL || pMsg == NULL)
      return;
   Step *pStp(dynamic_cast<Step*>(pMsg));
   StepImp *pStpImp(dynamic_cast<StepImp*>(pStp));
   MessageImp *pMsgImp(dynamic_cast<MessageImp*>(pMsg));
   *mpJournalWriter << mpLogName.c_str() << " - PROPERTY ADDED "
      << ((pStpImp != NULL) ? "Step" : "Message")
      << "[" << ((pStpImp != NULL) ? pStpImp : pMsgImp)->getStringId().c_str() << "."
      << pMsg->getProperties()->getNumAttributes() << "] "
      << endl << flush;
   notify(SIGNAL_NAME(MessageLog, MessageModified), v);
}

void MessageLogImp::messageHidden(Subject &subject, const string &signal, const boost::any &v)
{
   Message *pMsg(boost::any_cast<Message*>(v));
   if(mpJournalWriter == NULL || pMsg == NULL)
      return;
   Step *pStp(dynamic_cast<Step*>(pMsg));
   StepImp *pStpImp(dynamic_cast<StepImp*>(pStp));
   MessageImp *pMsgImp(dynamic_cast<MessageImp*>(pMsg));
   *mpJournalWriter << mpLogName.c_str() << " - FINALIZED "
      << ((pStpImp != NULL) ? "Step" : "Message")
      << "[" << ((pStpImp != NULL) ? pStpImp : pMsgImp)->getStringId().c_str() << "] ";
   if(pStp != NULL)
   {
      switch(pStp->getResult())
      {
      case Message::Success:
         *mpJournalWriter << "Success";
         break;
      case Message::Failure:
         *mpJournalWriter << "Failure[" << pStp->getFailureMessage().c_str() << "]";
         break;
      case Message::Abort:
         *mpJournalWriter << "Abort";
         break;
      }
   }
   *mpJournalWriter << endl << flush;
   notify(SIGNAL_NAME(MessageLog, MessageHidden), v);
}

void MessageLogImp::messageDetached(Subject &subject, const string &signal, const boost::any &v)
{
   notify(SIGNAL_NAME(MessageLog, MessageDetached), v);
}

void MessageLogImp::messageDeleted(Subject &subject, const string &signal, const boost::any &v)
{
   notify(SIGNAL_NAME(MessageLog, MessageDeleted), v);
}

string MessageLogImp::serialize()
{
   mpWriter->pushAddPoint(); // add messages as children
   for(vector<Message*>::const_iterator it=mMessageList.begin();
                                          it!=mMessageList.end();
                                          ++it)
   {
      MessageImp *mi(dynamic_cast<MessageImp*>(*it));
      mi->toXml(mpWriter);
   }
   mpWriter->popAddPoint();
   return mpWriter->writeToString();
}

/////////// MessageImp

MessageImp::MessageImp(const string &action,
                       const string &component,
                       const string &key,
                       DateTime *timestamp,
                       const Step *pParent) :
      mAction(action),
      id(new NumChain()),
      mpTimeStamp(timestamp),
      mFinalized(false),
      mComponent(component),
      mKey(key),
      mDomNode(NULL),
      mpParent(pParent)
{
   if(mpTimeStamp == NULL)
   {
      mpTimeStamp = new DateTimeImp();
      mpTimeStamp->setToCurrentTime();
   }
   getId()(1);
   mpProperties = new DynamicObjectAdapter;
   ENSURE(mpProperties != NULL);
}

MessageImp::~MessageImp()
{
   delete dynamic_cast<DateTimeImp*>(mpTimeStamp);
   delete id;
   delete dynamic_cast<DynamicObjectAdapter*>(mpProperties);
}

bool MessageImp::addProperty(const string &name, const char *value)
{
   return addProperty(name, string(value));
}

bool MessageImp::addProperty(const string &name, char value)
{
   if(mFinalized) return false;
   mpProperties->setAttribute(name, value);
   notify(SIGNAL_NAME(Message, MessageModified), boost::any(dynamic_cast<Message*>(this)));
   return true;
}

bool MessageImp::addProperty(const string &name, unsigned char value)
{
   if(mFinalized) return false;
   mpProperties->setAttribute(name, value);
   notify(SIGNAL_NAME(Message, MessageModified), boost::any(dynamic_cast<Message*>(this)));
   return true;
}

bool MessageImp::addProperty(const string &name, short value)
{
   if(mFinalized) return false;
   mpProperties->setAttribute(name, value);
   notify(SIGNAL_NAME(Message, MessageModified), boost::any(dynamic_cast<Message*>(this)));
   return true;
}

bool MessageImp::addProperty(const string &name, unsigned short value)
{
   if(mFinalized) return false;
   mpProperties->setAttribute(name, value);
   notify(SIGNAL_NAME(Message, MessageModified), boost::any(dynamic_cast<Message*>(this)));
   return true;
}

bool MessageImp::addProperty(const string &name, int value)
{
   if(mFinalized) return false;
   mpProperties->setAttribute(name, value);
   notify(SIGNAL_NAME(Message, MessageModified), boost::any(dynamic_cast<Message*>(this)));
   return true;
}

bool MessageImp::addProperty(const string &name, unsigned int value)
{
   if(mFinalized) return false;
   mpProperties->setAttribute(name, value);
   notify(SIGNAL_NAME(Message, MessageModified), boost::any(dynamic_cast<Message*>(this)));
   return true;
}

bool MessageImp::addProperty(const string &name, long value)
{
   if(mFinalized) return false;
   mpProperties->setAttribute(name, value);
   notify(SIGNAL_NAME(Message, MessageModified), boost::any(dynamic_cast<Message*>(this)));
   return true;
}

bool MessageImp::addProperty(const string &name, unsigned long value)
{
   if(mFinalized) return false;
   mpProperties->setAttribute(name, value);
   notify(SIGNAL_NAME(Message, MessageModified), boost::any(dynamic_cast<Message*>(this)));
   return true;
}

bool MessageImp::addBooleanProperty(const string &name, bool value)
{
   if(mFinalized) return false;
   mpProperties->setAttribute(name, value);
   notify(SIGNAL_NAME(Message, MessageModified), boost::any(dynamic_cast<Message*>(this)));
   return true;
}

bool MessageImp::addProperty(const string &name, float value)
{
   if(mFinalized) return false;
   mpProperties->setAttribute(name, value);
   notify(SIGNAL_NAME(Message, MessageModified), boost::any(dynamic_cast<Message*>(this)));
   return true;
}

bool MessageImp::addProperty(const string &name, double value)
{
   if(mFinalized) return false;
   mpProperties->setAttribute(name, value);
   notify(SIGNAL_NAME(Message, MessageModified), boost::any(dynamic_cast<Message*>(this)));
   return true;
}

bool MessageImp::addProperty(const string &name, const string &value)
{
   if(mFinalized) return false;
   mpProperties->setAttribute(name, value);
   notify(SIGNAL_NAME(Message, MessageModified), boost::any(dynamic_cast<Message*>(this)));
   return true;
}

bool MessageImp::addProperty(const string &name, const Filename *pValue)
{
   if(mFinalized) return false;
   mpProperties->setAttribute(name, *pValue);
   notify(SIGNAL_NAME(Message, MessageModified), boost::any(dynamic_cast<Message*>(this)));
   return true;
}

bool MessageImp::addProperty(const string &name, const DateTime *pValue)
{
   if(mFinalized) return false;
   mpProperties->setAttribute(name, *pValue);
   notify(SIGNAL_NAME(Message, MessageModified), boost::any(dynamic_cast<Message*>(this)));
   return true;
}

bool MessageImp::addProperty(const string &name, const vector<char>& value)
{
   if(mFinalized) return false;
   mpProperties->setAttribute(name, value);
   notify(SIGNAL_NAME(Message, MessageModified), boost::any(dynamic_cast<Message*>(this)));
   return true;
}

bool MessageImp::addProperty(const string &name, const vector<unsigned char>& value)
{
   if(mFinalized) return false;
   mpProperties->setAttribute(name, value);
   notify(SIGNAL_NAME(Message, MessageModified), boost::any(dynamic_cast<Message*>(this)));
   return true;
}

bool MessageImp::addProperty(const string &name, const vector<short>& value)
{
   if(mFinalized) return false;
   mpProperties->setAttribute(name, value);
   notify(SIGNAL_NAME(Message, MessageModified), boost::any(dynamic_cast<Message*>(this)));
   return true;
}

bool MessageImp::addProperty(const string &name, const vector<unsigned short>& value)
{
   if(mFinalized) return false;
   mpProperties->setAttribute(name, value);
   notify(SIGNAL_NAME(Message, MessageModified), boost::any(dynamic_cast<Message*>(this)));
   return true;
}

bool MessageImp::addProperty(const string &name, const vector<int>& value)
{
   if(mFinalized) return false;
   mpProperties->setAttribute(name, value);
   notify(SIGNAL_NAME(Message, MessageModified), boost::any(dynamic_cast<Message*>(this)));
   return true;
}

bool MessageImp::addProperty(const string &name, const vector<unsigned int>& value)
{
   if(mFinalized) return false;
   mpProperties->setAttribute(name, value);
   notify(SIGNAL_NAME(Message, MessageModified), boost::any(dynamic_cast<Message*>(this)));
   return true;
}

bool MessageImp::addProperty(const string &name, const vector<long>& value)
{
   if(mFinalized) return false;
   mpProperties->setAttribute(name, value);
   notify(SIGNAL_NAME(Message, MessageModified), boost::any(dynamic_cast<Message*>(this)));
   return true;
}

bool MessageImp::addProperty(const string &name, const vector<unsigned long>& value)
{
   if(mFinalized) return false;
   mpProperties->setAttribute(name, value);
   notify(SIGNAL_NAME(Message, MessageModified), boost::any(dynamic_cast<Message*>(this)));
   return true;
}

bool MessageImp::addProperty(const string &name, const vector<float>& value)
{
   if(mFinalized) return false;
   mpProperties->setAttribute(name, value);
   notify(SIGNAL_NAME(Message, MessageModified), boost::any(dynamic_cast<Message*>(this)));
   return true;
}

bool MessageImp::addProperty(const string &name, const vector<double>& value)
{
   if(mFinalized) return false;
   mpProperties->setAttribute(name, value);
   notify(SIGNAL_NAME(Message, MessageModified), boost::any(dynamic_cast<Message*>(this)));
   return true;
}

bool MessageImp::addProperty(const string &name, const vector<bool>& value)
{
   if(mFinalized) return false;
   mpProperties->setAttribute(name, value);
   notify(SIGNAL_NAME(Message, MessageModified), boost::any(dynamic_cast<Message*>(this)));
   return true;
}

bool MessageImp::addProperty(const string &name, const vector<string>& value)
{
   if(mFinalized) return false;
   mpProperties->setAttribute(name, value);
   notify(SIGNAL_NAME(Message, MessageModified), boost::any(dynamic_cast<Message*>(this)));
   return true;
}

bool MessageImp::addProperty(const string &name, const vector<const Filename*>& value)
{
   if(mFinalized) return false;
   mpProperties->setAttribute(name, value);
   notify(SIGNAL_NAME(Message, MessageModified), boost::any(dynamic_cast<Message*>(this)));
   return true;
}

bool MessageImp::finalize(Message::Result result)
{
   if(mFinalized) return false;
   mFinalized = true;
   notify(SIGNAL_NAME(Message, Hidden), boost::any(dynamic_cast<Message*>(this)));
   return true;
}

const DynamicObject *MessageImp::getProperties() const
{
   return mpProperties;
}

string MessageImp::getAction() const
{
   return mAction;
}

Message::Result MessageImp::getResult() const
{
   if(mFinalized)
      return Message::Success;
   return Message::Unresolved;
}

string MessageImp::getComponent() const
{
   return mComponent;
}

string MessageImp::getKey() const
{
   return mKey;
}

const string& MessageImp::getObjectType() const
{
   static string type("MessageImp");
   return type;
}

bool MessageImp::isKindOf(const string& name) const
{
   if ((name == getObjectType()) || (name == "Message"))
   {
      return true;
   }

   return false;
}

bool MessageImp::toXml(XMLWriter* xml) const
{
   if(xml == NULL)
      return false;
   if(mDomNode == NULL)
   {
      if((mDomNode = xml->addElement("message")) == NULL)
         return false;
   }
   xml->pushAddPoint(mDomNode);
   serializeReal(xml);
   xml->popAddPoint();
   return true;
}

void MessageImp::serializeDate(string &date, string &time) const
{
   string format = "%d %b %Y";
   date = mpTimeStamp->getFormattedUtc(format);

   format = "%H:%M:%S UTC";
   time = mpTimeStamp->getFormattedUtc(format);
}

void MessageImp::serializeReal(XMLWriter* xml) const
{
   xml->addAttr("id", id->toString(), mDomNode);
   if (!mComponent.empty())
   {
      xml->addAttr("component", mComponent, mDomNode);
   }
   if (!mKey.empty())
   {
      xml->addAttr("key", mKey, mDomNode);
   }

   string date, time;
   serializeDate(date, time);
   xml->addAttr("date", date, mDomNode);
   xml->addAttr("time", time, mDomNode);

   xml->addAttr("name", getAction(), mDomNode);

   vector<string> propertyNames;
   mpProperties->getAttributeNames(propertyNames);
   for(vector<string>::iterator it = propertyNames.begin(); it != propertyNames.end(); ++it)
   {
      if(mPropertyNodes.find(*it) == mPropertyNodes.end())
      {
         // the property node does not exist
         mPropertyNodes[*it] = xml->addElement("property", mDomNode);
      }
      xml->pushAddPoint(mPropertyNodes[*it]);
      xml->addAttr("name", *it);
      const DataVariant& attrValue = mpProperties->getAttribute(*it);
      string value = attrValue.toXmlString();
      string type = attrValue.getTypeName();
      xml->addAttr("type", type);
      xml->addAttr("value", value);
      xml->popAddPoint();
   }
}

string MessageImp::propertyToString(const string &type, void *pValue) const
{
   stringstream buf;
   if(type == "char")
   {
      buf << (char *)pValue;
   }
   else if(type == "unsigned char")
   {
      buf << *(unsigned char *)pValue;
   }
   else if(type == "short")
   {
      buf << *(short *)pValue;
   }
   else if(type == "unsigned short")
   {
      buf << *(unsigned short *)pValue;
   }
   else if(type == "int")
   {
      buf << *(int *)pValue;
   }
   else if(type == "unsigned int")
   {
      buf << *(unsigned int *)pValue;
   }
   else if(type == "long")
   {
      buf << *(long *)pValue;
   }
   else if(type == "unsigned long")
   {
      buf << *(unsigned long *)pValue;
   }
   else if(type == "bool")
   {
      buf << *(bool *)pValue;
   }
   else if(type == "float")
   {
      buf << *(float *)pValue;
   }
   else if(type == "double")
   {
      buf.precision(16);
      buf << *(double *)pValue;
   }
   else if(type == "string")
   {
      buf << *(string*)pValue;
   }
   else if(type == "Filename")
   {
      buf << (Filename *)pValue;
   }
   else if(type == "DateTime")
   {
      string date, time;
      serializeDate(date, time);
      buf << date;
   }
   else if(type == "vector<char>")
   {
      vector<char> *v((vector<char>*)pValue);
      QString s("");
      for(vector<char>::iterator i=v->begin(); i!=v->end(); ++i)
         s += (*i) + " ";
      if(s.right(1) == " ")
         s = s.left(s.length()-2);
      buf << s.toStdString();
   }
   else if(type == "vector<unsigned char>")
   {
      vector<unsigned char> *v((vector<unsigned char>*)pValue);
      QString s("");
      for(vector<unsigned char>::iterator i=v->begin(); i!=v->end(); ++i)
         s += (*i) + " ";
      if(s.right(1) == " ")
         s = s.left(s.length()-2);
      buf << s.toStdString();
   }
   else if(type == "vector<short>")
   {
      vector<short> *v((vector<short>*)pValue);
      QString s("");
      for(vector<short>::iterator i=v->begin(); i!=v->end(); ++i)
         s += QString::number(*i) + " ";
      if(s.right(1) == " ")
         s = s.left(s.length()-2);
      buf << s.toStdString();
   }
   else if(type == "vector<unsigned short>")
   {
      vector<unsigned short> *v((vector<unsigned short>*)pValue);
      QString s("");
      for(vector<unsigned short>::iterator i=v->begin(); i!=v->end(); ++i)
         s += QString::number(*i) + " ";
      if(s.right(1) == " ")
         s = s.left(s.length()-2);
      buf << s.toStdString();
   }
   else if(type == "vector<int>")
   {
      vector<int> *v((vector<int>*)pValue);
      QString s("");
      for(vector<int>::iterator i=v->begin(); i!=v->end(); ++i)
         s += QString::number(*i) + " ";
      if(s.right(1) == " ")
         s = s.left(s.length()-2);
      buf << s.toStdString();
   }
   else if(type == "vector<unsigned int>")
   {
      vector<unsigned int> *v((vector<unsigned int>*)pValue);
      QString s("");
      for(vector<unsigned int>::iterator i=v->begin(); i!=v->end(); ++i)
         s += QString::number(*i) + " ";
      if(s.right(1) == " ")
         s = s.left(s.length()-2);
      buf << s.toStdString();
   }
   else if(type == "vector<long>")
   {
      vector<long> *v((vector<long>*)pValue);
      QString s("");
      for(vector<long>::iterator i=v->begin(); i!=v->end(); ++i)
         s += QString::number(*i) + " ";
      if(s.right(1) == " ")
         s = s.left(s.length()-2);
      buf << s.toStdString();
   }
   else if(type == "vector<unsigned long>")
   {
      vector<unsigned long> *v((vector<unsigned long>*)pValue);
      QString s("");
      for(vector<unsigned long>::iterator i=v->begin(); i!=v->end(); ++i)
         s += QString::number(*i) + " ";
      if(s.right(1) == " ")
         s = s.left(s.length()-2);
      buf << s.toStdString();
   }
   else if(type == "vector<float>")
   {
      vector<float> *v((vector<float>*)pValue);
      QString s("");
      for(vector<float>::iterator i=v->begin(); i!=v->end(); ++i)
         s += QString::number(*i) + " ";
      if(s.right(1) == " ")
         s = s.left(s.length()-2);
      buf << s.toStdString();
   }
   else if(type == "vector<double>")
   {
      vector<double> *v((vector<double>*)pValue);
      QString s("");
      for(vector<double>::iterator i=v->begin(); i!=v->end(); ++i)
         s += QString::number(*i, 'g', 16) + " ";
      if(s.right(1) == " ")
         s = s.left(s.length()-2);
      buf << s.toStdString();
   }
   else if(type == "vector<bool>")
   {
      vector<bool> *v((vector<bool>*)pValue);
      QString s("");
      for(vector<bool>::iterator i=v->begin(); i!=v->end(); ++i)
      {
         if(*i) s += "1 ";
         else s += "0 ";
      }
      if(s.right(1) == " ")
         s = s.left(s.length()-2);
      buf << s.toStdString();
   }
   else if(type == "vector<string>")
   {
      vector<string> *v((vector<string>*)pValue);
      QString s("");
      for(vector<string>::iterator i=v->begin(); i!=v->end(); ++i)
      {
         // this may seem a little strange, but representing a string
         // as a file:// URL will automatically escape invalid characters
         // although at the expense of slightly less readable encodings
         // until we change the XML to accomodate arbitrary CDATA, this
         // will work well
         QString ss(XmlBase::PathToURL(*i).c_str());
         ss.replace('"',"\\\"");
         s += '"' + ss + "\" ";
      }
      if(s.right(1) == " ")
         s = s.left(s.length()-2);
      buf << s.toStdString();
   }
   else if(type == "vector<Filename*>")
   {
      vector<Filename*> *v((vector<Filename*>*)pValue);
      QString s("");
      for(vector<Filename*>::iterator i=v->begin(); i!=v->end(); ++i)
         s += QString(XmlBase::PathToURL(((*i)->getFullPathAndName())).c_str()) + " ";
      if(s.right(1) == " ")
         s = s.left(s.length()-2);
      buf << s.toStdString();
   }

   return buf.str();
}

/////////// StepImp

StepImp::StepImp(const string &action,
                 const string &component,
                 const string &key,
                 DateTime *timestamp,
                 const Step *pParent) :
   MessageImp(action,component,key,timestamp,pParent),
   mResult(Message::Unresolved),
   mpCurrentStep(NULL)
{
}

StepImp::~StepImp()
{
   //clean-up all of the underlying messages
   vector<Message*>::iterator messageIter;
   for (messageIter = mMessageList.begin(); messageIter != mMessageList.end(); ++messageIter)
   {
      Message* pCurMsg = *messageIter;
      if (pCurMsg != NULL)
      {
         MessageImp* pCurMsgImp = dynamic_cast<MessageImp*>(pCurMsg);
         if (pCurMsgImp != NULL)
         {
            delete pCurMsgImp;
         }
      }
   }
   mMessageList.clear();
}

Step *StepImp::addStep(const string &action,
                       const string &component,
                       const string &key,
                       bool recurse)
{
   if(mFinalized) return NULL;
   Step *stp(NULL);
   if(recurse && mpCurrentStep != NULL)
   {
      stp = mpCurrentStep->addStep(action,component,key,true);
      if(stp == NULL)
      {
         mpCurrentStep = NULL;
         return addStep(action,component,key,false);
      }
   }
   else
   {
      StepAdapter *pStpAdapter(new StepAdapter(action, component, key, NULL, dynamic_cast<Step*>(this)));
      stp = pStpAdapter;
      mMessageList.push_back(static_cast<Message*>(stp));
      mpCurrentStep = stp;
      NumChain *nid(new NumChain(getId()));
      nid->append(size());
      pStpAdapter->setId(nid);
      notify(SIGNAL_NAME(Step, MessageAdded), boost::any(static_cast<Message*>(stp)));
   }
   return stp;
}

Message *StepImp::addMessage(const string &action,
                             const string &component,
                             const string &key,
                             bool finalizeOnCreate,
                             bool recurse)
{
   if(mFinalized) return NULL;
   Message *pMsg(NULL);
   if(recurse && mpCurrentStep != NULL)
   {
      pMsg = mpCurrentStep->addMessage(action,component,key,finalizeOnCreate,true);
      if(pMsg == NULL)
      {
         mpCurrentStep = NULL;
         pMsg = addMessage(action,component,key,finalizeOnCreate,false);
      }
   }
   else
   {
      MessageAdapter *pMsgAdapter(new MessageAdapter(action, component, key, NULL, dynamic_cast<Step*>(this)));
      pMsg = pMsgAdapter;
      if(pMsg == NULL)
         return NULL;
      if(finalizeOnCreate)
         pMsg->finalize();
      mMessageList.push_back(pMsg);
      NumChain *nid(new NumChain(getId()));
      nid->append(size());
      pMsgAdapter->setId(nid);
      notify(SIGNAL_NAME(Step, MessageAdded), boost::any(pMsg));
   }
   return pMsg;
}

bool StepImp::finalize(Message::Result result)
{
   return finalize(result, "");
}

bool StepImp::finalize(Message::Result result, const string& failureReason)
{
   if(mFinalized) return false;
   mResult = result;
   mFailureReason = failureReason;
   for(std::vector<Message*>::iterator it=mMessageList.begin();
                                     it!=mMessageList.end();
                                     ++it)
   {
      MessageAdapter *m(static_cast<MessageAdapter*>(*it));
      StepAdapter *pStpAdapter(dynamic_cast<StepAdapter*>(m));
      if (pStpAdapter != NULL)
      {
         pStpAdapter->finalize(result, failureReason);
      }
      else
      {
         m->finalize(result);
      }
   }
   return MessageImp::finalize();
}

const std::string& StepImp::getFailureMessage() const
{
   return mFailureReason;
}

Step::size_t StepImp::size() const
{
   return mMessageList.size();
}

Step::iterator StepImp::begin()
{
   return mMessageList.begin();
}

Step::const_iterator StepImp::begin() const
{
   return mMessageList.begin();
}

Step::iterator StepImp::end()
{
   return mMessageList.end();
}

Step::const_iterator StepImp::end() const
{
   return mMessageList.end();
}

Message *StepImp::operator[](Step::size_t i)
{
   return mMessageList[i];
}

const Message *StepImp::operator[](Step::size_t i) const
{
   return mMessageList[i];
}

Message::Result StepImp::getResult() const
{
   return mResult;
}

const string& StepImp::getObjectType() const
{
   static string type("StepImp");
   return type;
}

bool StepImp::isKindOf(const string& name) const
{
   if ((name == getObjectType()) || (name == "Step"))
   {
      return true;
   }

   return false;
}

void StepImp::messageAdded(Subject &subject, const string &signal, const boost::any &v)
{
   if(!mFinalized)
   {
      notify(SIGNAL_NAME(Step, MessageAdded), v);
   }
}

void StepImp::messageModified(Subject &subject, const string &signal, const boost::any &v)
{
   if(!mFinalized)
   {
      notify(SIGNAL_NAME(Step, MessageModified), v);
   }
}

void StepImp::messageHidden(Subject &subject, const string &signal, const boost::any &v)
{
   if(!mFinalized)
   {
      notify(SIGNAL_NAME(Step, MessageHidden), v);
   }
}

void StepImp::messageDeleted(Subject &subject, const string &signal, const boost::any &v)
{
   if(!mFinalized)
   {
      notify(SIGNAL_NAME(Step, MessageDeleted), v);
   }
}

void StepImp::messageDetached(Subject &subject, const string &signal, const boost::any &v)
{
   if(!mFinalized)
   {
      notify(SIGNAL_NAME(Step, MessageDetached), v);
   }
}

bool StepImp::toXml(XMLWriter* xml) const
{
   if(xml == NULL)
      return false;
   if(mDomNode == NULL)
   {
      if((mDomNode = xml->addElement("step")) == NULL)
         return false;
   }
   switch(getResult())
   {
      case Message::Success:
         xml->addAttr("result", "Success", mDomNode);
         break;
      case Message::Failure:
         xml->addAttr("result", "Failure", mDomNode);
         break;
      case Message::Abort:
         xml->addAttr("result", "Abort", mDomNode);
         break;
      case Message::Unresolved:
         xml->addAttr("result", "Unresolved", mDomNode);
         break;
   }
   if (mFailureReason.size() > 0)
   {
      xml->addAttr("failureMessage", mFailureReason);
   }
   serializeReal(xml);
   xml->pushAddPoint(mDomNode);
   for(vector<Message*>::const_iterator it=mMessageList.begin();
                                      it!=mMessageList.end();
                                      ++it)
   {
      MessageImp *mi(dynamic_cast<MessageImp*>(*it));
      mi->toXml(xml);
   }
   xml->popAddPoint();
   return true;
}
