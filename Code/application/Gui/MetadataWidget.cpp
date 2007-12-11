/*
 * The information in this file is
 * Copyright(c) 2007 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include <QtGui/QHeaderView>
#include <QtGui/QLabel>
#include <QtGui/QLayout>
#include <QtGui/QMessageBox>
#include <QtGui/QSortFilterProxyModel>

#include "AppVersion.h"
#include "DynamicObjectItemModel.h"
#include "DynamicObjectAdapter.h"
#include "MetadataWidget.h"
#include "NameTypeValueDlg.h"

#include <string>
using namespace std;

bool MetadataWidget::mEditWarning = true;

MetadataWidget::MetadataWidget(QWidget* parent) :
   QWidget(parent),
   mpMetadataTree(NULL),
   mpObject(new DynamicObjectAdapter),
   mpMetadata(NULL),
   mModified(false),
   mpModifyButton(NULL),
   mpAddChildButton(NULL),
   mpAddSiblingButton(NULL),
   mpEditButton(NULL),
   mpDeleteButton(NULL),
   mpClearButton(NULL)
{
   // Metadata
   QLabel* pMetadataLabel = new QLabel("Metadata:", this);

   QStringList columnNames;
   columnNames.append("Name");
   columnNames.append("Type");
   columnNames.append("Value");

   mpMetadataModel = new DynamicObjectItemModel(this, mpObject);
   mpMetadataSortingModel = new QSortFilterProxyModel(this);
   mpMetadataSortingModel->setSortCaseSensitivity(Qt::CaseInsensitive);
   mpMetadataSortingModel->setSourceModel(mpMetadataModel);

   mpMetadataTree = new QTreeView(this);
   mpMetadataTree->setSelectionMode(QAbstractItemView::SingleSelection);
   mpMetadataTree->setRootIsDecorated(true);
   mpMetadataTree->setSortingEnabled(true);
   mpMetadataTree->setUniformRowHeights(true);
   mpMetadataTree->setModel(mpMetadataSortingModel);  
   mpMetadataTree->setSelectionBehavior(QAbstractItemView::SelectRows);
   mpMetadataTree->setAllColumnsShowFocus(true);

   QHeaderView* pHeader = mpMetadataTree->header();
   if (pHeader != NULL)
   {
      pHeader->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
      pHeader->setSortIndicatorShown(true);
   }

   // Buttons
   mpModifyButton = new QPushButton("&Modify Values", this);
   mpAddChildButton = new QPushButton("&Add Child...", this);
   mpAddSiblingButton = new QPushButton("Add &Sibling...", this);
   mpEditButton = new QPushButton("&Edit...", this);
   mpDeleteButton = new QPushButton("&Delete", this);
   mpClearButton = new QPushButton("&Clear", this);

   // Layout
   QHBoxLayout* pButtonLayout = new QHBoxLayout();
   pButtonLayout->setMargin(0);
   pButtonLayout->setSpacing(5);
   pButtonLayout->addWidget(mpModifyButton);
   pButtonLayout->addSpacing(15);
   pButtonLayout->addStretch();
   pButtonLayout->addWidget(mpAddChildButton);
   pButtonLayout->addWidget(mpAddSiblingButton);
   pButtonLayout->addWidget(mpEditButton);
   pButtonLayout->addWidget(mpDeleteButton);
   pButtonLayout->addWidget(mpClearButton);

   QGridLayout* pGrid = new QGridLayout(this);
   pGrid->setMargin(0);
   pGrid->setSpacing(5);
   pGrid->addWidget(pMetadataLabel, 0, 0);
   pGrid->addWidget(mpMetadataTree, 1, 0);
   pGrid->setRowMinimumHeight(2, 5);
   pGrid->addLayout(pButtonLayout, 3, 0);
   pGrid->setRowStretch(1, 10);

   // Initialization
   mpModifyButton->setEnabled(mEditWarning);
   mpAddChildButton->setEnabled(!mEditWarning);
   mpAddSiblingButton->setEnabled(false);
   mpEditButton->setEnabled(false);
   mpDeleteButton->setEnabled(false);
   mpClearButton->setEnabled(!mEditWarning);

   // Connections
   if (mEditWarning == false)
   {
      connect(mpMetadataTree->selectionModel(), SIGNAL(currentChanged(const QModelIndex&,const QModelIndex&)), this,
         SLOT(currentChanged(const QModelIndex&,const QModelIndex&)));
      connect(mpMetadataTree, SIGNAL(doubleClicked(const QModelIndex&)), this,
         SLOT(editSelectedValue(const QModelIndex&)));
   }

   connect(mpModifyButton, SIGNAL(clicked()), this, SLOT(modifyValues()));
   connect(mpAddChildButton, SIGNAL(clicked()), this, SLOT(addKey()));
   connect(mpAddSiblingButton, SIGNAL(clicked()), this, SLOT(addKey()));
   connect(mpEditButton,   SIGNAL(clicked()), this, SLOT(editCurrentValue()));
   connect(mpDeleteButton, SIGNAL(clicked()), this, SLOT(deleteKey()));
   connect(mpClearButton, SIGNAL(clicked()), this, SLOT(clearKeys()));
}

MetadataWidget::~MetadataWidget()
{
   if (mpObject != NULL)
   {
      delete mpObject;
   }
}

void MetadataWidget::setMetadata(DynamicObject* pMetadata)
{
   mpMetadata = pMetadata;

   if (mpObject != NULL)
   {
      if (mpMetadata != NULL)
      {
         *(static_cast<DynamicObjectImp*>(mpObject)) = *(dynamic_cast<DynamicObjectImp*>(mpMetadata));
      }
      else
      {
         mpObject->clear();
      }
   }
}

bool MetadataWidget::isModified() const
{
   return mModified;
}

bool MetadataWidget::applyChanges()
{
   if (mModified == false)
   {
      return true;
   }

   if (mpMetadata != NULL)
   {
      *(dynamic_cast<DynamicObjectImp*>(mpMetadata)) = *mpObject;

      mModified = false;
      return true;
   }

   return false;
}

QSize MetadataWidget::sizeHint() const
{
   return QSize(575, 325);
}

void MetadataWidget::modifyValues()
{
   if (mEditWarning == true)
   {
      int iReturn = QMessageBox::warning(this, QString::fromStdString(APP_NAME), "Modifying the metadata "
         "values could have adverse effects since some\nplug-ins may require the existence of specific "
         "metadata key-value pairs.\nDo you want to continue?\n\nClicking the Ignore button will allow the "
         "values to be modified for the\nremainder of the session without displaying this message.",
         QMessageBox::Yes, QMessageBox::No, QMessageBox::Ignore);
      if (iReturn == QMessageBox::No)
      {
         return;
      }
      else if (iReturn == QMessageBox::Ignore)
      {
         mEditWarning = false;
      }
   }

   mpMetadataTree->selectionModel()->clear();
   mpModifyButton->setEnabled(false);
   mpAddChildButton->setEnabled(true);
   mpAddSiblingButton->setEnabled(false);
   mpDeleteButton->setEnabled(false);
   mpClearButton->setEnabled(mpMetadata != NULL);

   connect(mpMetadataTree->selectionModel(), SIGNAL(currentChanged(const QModelIndex&,const QModelIndex&)), this,
      SLOT(currentChanged(const QModelIndex&,const QModelIndex&)));
   connect(mpMetadataTree, SIGNAL(doubleClicked(const QModelIndex&)), this,
      SLOT(editSelectedValue(const QModelIndex&)));
}

void MetadataWidget::addKey()
{
   // Get the dynamic object
   DynamicObject* pDynamicObject = NULL;
   QModelIndex parent;

   QObject* pButton = sender();
   if (pButton == mpAddChildButton)
   {
      parent = mpMetadataSortingModel->mapToSource(mpMetadataTree->currentIndex());
   }
   else if (pButton == mpAddSiblingButton)
   {
      parent = mpMetadataSortingModel->mapToSource(mpMetadataTree->currentIndex().parent());
   }

   if (parent.isValid() == true)
   {
      DataVariant* pValue = parent.data(Qt::UserRole).value<DataVariant*>();
      if ((pValue != NULL) && (pValue->isValid() == true))
      {
         if (pValue->getTypeName() == "DynamicObject")
         {
            pDynamicObject = dv_cast<DynamicObject>(pValue);
         }
      }
   }
   else
   {
      pDynamicObject = mpObject;
   }

   if (pDynamicObject == NULL)
   {
      return;
   }

   // Get the new name and value from the user
   NameTypeValueDlg dlg(this);
   dlg.setWindowTitle("Add Metadata Value");
   if (dlg.exec() == QDialog::Accepted)
   {
      string key = dlg.getName().toStdString();
      const DataVariant& var = dlg.getValue();

      // Update the value
      pDynamicObject->setAttribute(key, var);

      // Expand the parent dynamic object item to see the new child attribute
      if (parent.isValid() == true)
      {
         mpMetadataTree->expand(mpMetadataSortingModel->mapFromSource(parent));
      }

      // Mark the page as modified
      mModified = true;
      emit modified();
   }
}

void MetadataWidget::editCurrentValue()
{
   QModelIndex selectedItem = mpMetadataTree->currentIndex();
   editSelectedValue(selectedItem);
}

void MetadataWidget::deleteKey()
{
   // Get the parent dynamic object
   DynamicObject* pDynamicObject = NULL;

   QModelIndex parent = mpMetadataSortingModel->mapToSource(mpMetadataTree->currentIndex().parent());
   if (parent.isValid() == true)
   {
      DataVariant* pValue = parent.data(Qt::UserRole).value<DataVariant*>();
      if ((pValue != NULL) && (pValue->isValid() == true))
      {
         if (pValue->getTypeName() == "DynamicObject")
         {
            pDynamicObject = dv_cast<DynamicObject>(pValue);
         }
      }
   }
   else
   {
      pDynamicObject = mpObject;
   }

   if (pDynamicObject == NULL)
   {
      return;
   }

   // Remove the attribute
   QModelIndex index = mpMetadataSortingModel->mapToSource(mpMetadataTree->currentIndex());
   index = index.sibling(index.row(), 0);

   string key = index.data(Qt::DisplayRole).toString().toStdString();
   pDynamicObject->removeAttribute(key);

   // Update the modified flag
   mModified = true;
   emit modified();
}

void MetadataWidget::clearKeys()
{
   if (mpObject != NULL)
   {
      mpObject->clear();
      mModified = true;
      emit modified();
   }
}

void MetadataWidget::currentChanged(const QModelIndex& selectedItem, const QModelIndex& deselectedItem)
{
   QModelIndex selected = mpMetadataSortingModel->mapToSource(selectedItem);

   bool bSelectedItem = selected.isValid();
   mpAddChildButton->setEnabled(true);
   mpAddSiblingButton->setEnabled(bSelectedItem);
   mpEditButton->setEnabled(bSelectedItem);
   mpDeleteButton->setEnabled(bSelectedItem);

   if (bSelectedItem == true)
   {
      DataVariant* pValue = selected.data(Qt::UserRole).value<DataVariant*>();
      if ((pValue != NULL) && (pValue->isValid() == true))
      {
         if (pValue->getTypeName() == "DynamicObject")
         {
            mpEditButton->setEnabled(false);
         }
         else
         {
            mpAddChildButton->setEnabled(false);
         }
      }
   }
}

void MetadataWidget::editSelectedValue(const QModelIndex& selectedItem)
{
   // Do not allow editing of a dynamic object item
   QModelIndex index = mpMetadataSortingModel->mapToSource(selectedItem);
   if (index.isValid() == true)
   {
      DataVariant* pValue = index.data(Qt::UserRole).value<DataVariant*>();
      if ((pValue != NULL) && (pValue->isValid() == true))
      {
         if (pValue->getTypeName() == "DynamicObject")
         {
            return;
         }
      }
   }
   else
   {
      return;
   }

   // Get the parent dynamic object
   DynamicObject* pDynamicObject = NULL;

   QModelIndex parent = mpMetadataSortingModel->mapToSource(selectedItem.parent());
   if (parent.isValid() == true)
   {
      DataVariant* pValue = parent.data(Qt::UserRole).value<DataVariant*>();
      if ((pValue != NULL) && (pValue->isValid() == true))
      {
         if (pValue->getTypeName() == "DynamicObject")
         {
            pDynamicObject = dv_cast<DynamicObject>(pValue);
         }
      }
   }
   else
   {
      pDynamicObject = mpObject;
   }

   if (pDynamicObject == NULL)
   {
      return;
   }

   // Get the current name and value
   index = index.sibling(index.row(), 0);

   QString name = index.data(Qt::DisplayRole).toString();
   DataVariant* pValue = index.data(Qt::UserRole).value<DataVariant*>();
   VERIFYNRV(pValue != NULL);

   // Get the new name and value from the user
   NameTypeValueDlg dlg(this);
   dlg.setWindowTitle("Edit Metadata Value");
   dlg.setValue(name, *pValue);
   if (dlg.exec() == QDialog::Accepted)
   {
      QString newName = dlg.getName();
      if (newName.isEmpty() == false)
      {
         // Update the value
         const DataVariant& newValue = dlg.getValue();
         pDynamicObject->setAttribute(newName.toStdString(), newValue);

         // Update the modified flag
         mModified = true;
         emit modified();
      }
   }
}
