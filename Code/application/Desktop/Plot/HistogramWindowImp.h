/*
 * The information in this file is
 * Copyright(c) 2007 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef HISTOGRAMWINDOWIMP_H
#define HISTOGRAMWINDOWIMP_H

#include "AttachmentPtr.h"
#include "PlotWindowImp.h"
#include "RasterLayer.h"
#include "SessionExplorer.h"

#include <boost/shared_ptr.hpp>
#include <set>

class Layer;
class PlotWidget;

class HistogramWindowImp : public PlotWindowImp
{
   Q_OBJECT

public:
   HistogramWindowImp(const std::string& id, const std::string& windowName, QWidget* pParent = 0);
   ~HistogramWindowImp();

   using SessionItemImp::setIcon;
   void updateContextMenu(Subject& subject, const std::string& signal, const boost::any& value);

   PlotSet* createPlotSet(const QString& strPlotSet);
   bool deletePlotSet(PlotSet* pPlotSet);

   PlotWidget* getPlot(Layer* pLayer) const;
   PlotWidget* getPlot(Layer* pLayer, const RasterChannelType& eColor) const;    // Deprecated
   PlotWidget* getPlot(RasterLayer* pLayer, RasterChannelType channel) const;
   using PlotWindowImp::setCurrentPlot;

public slots:
   PlotWidget* createPlot(Layer* pLayer);
   PlotWidget* createPlot(Layer* pLayer, PlotSet* pPlotSet);
   PlotWidget* createPlot(RasterLayer* pLayer, RasterChannelType channel);
   PlotWidget* createPlot(RasterLayer* pLayer, RasterChannelType channel, PlotSet* pPlotSet);
   void setCurrentPlot(Layer* pLayer);
   bool setCurrentPlot(Layer* pLayer, const RasterChannelType& eColor);          // Deprecated
   bool setCurrentPlot(RasterLayer* pLayer, RasterChannelType channel);
   void deletePlot(Layer* pLayer);
   void deletePlot(RasterLayer* pLayer, RasterChannelType channel);

signals:
   void plotActivated(Layer* pLayer, const RasterChannelType& color);

protected:
   bool event(QEvent* pEvent);
   void showEvent(QShowEvent * pEvent);

   void createPlots(RasterLayer* pLayer, DisplayMode displayMode);
   void createPlots(RasterLayer* pLayer, DisplayMode displayMode, PlotSet* pPlotSet);
   void deletePlots(RasterLayer* pLayer, DisplayMode displayMode);
   void updatePlotInfo(RasterLayer* pLayer, RasterChannelType channel);

protected slots:
   void setCurrentPlot(const DisplayMode& displayMode);
   void activateLayer(PlotWidget* pPlot);
   void updatePlotInfo(RasterChannelType channel);

private:
   AttachmentPtr<SessionExplorer> mpExplorer;
   bool mDisplayModeChanging;

   class HistogramUpdater
   {
   public:
      HistogramUpdater(HistogramWindowImp *pWindow);

      void initialize(RasterLayer* pLayer, RasterChannelType channel);
      void update();
   private:
      class UpdateMomento
      {
      public:
         UpdateMomento(HistogramWindowImp *pWindow, RasterLayer* pLayer, RasterChannelType channel);
         bool operator<(const UpdateMomento &rhs) const;
         void update();
      private:
         HistogramWindowImp *mpWindow;
         boost::shared_ptr<AttachmentPtr<RasterLayer> > mpRasterLayer;
         RasterChannelType mChannel;
      };
      std::set<UpdateMomento> mUpdatesPending;
      HistogramWindowImp *mpWindow;
   } mUpdater;
};

#define HISTOGRAMWINDOWADAPTER_METHODS(impClass) \
   PLOTWINDOWADAPTER_METHODS(impClass) \
   PlotWidget* createPlot(Layer* pLayer, PlotSet* pPlotSet) \
   { \
      return impClass::createPlot(pLayer, pPlotSet); \
   } \
   PlotWidget* getPlot(Layer* pLayer) const \
   { \
      return impClass::getPlot(pLayer); \
   } \
   PlotWidget* getPlot(Layer* pLayer, const RasterChannelType& eColor) const \
   { \
      return impClass::getPlot(pLayer, eColor); \
   } \
   void setCurrentPlot(Layer* pLayer) \
   { \
      impClass::setCurrentPlot(pLayer); \
   } \
   bool setCurrentPlot(Layer* pLayer, const RasterChannelType& eColor) \
   { \
      return impClass::setCurrentPlot(pLayer, eColor); \
   } \
   void deletePlot(Layer* pLayer) \
   { \
      impClass::deletePlot(pLayer); \
   }

#endif
