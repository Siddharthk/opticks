/*
 * The information in this file is
 * Copyright(c) 2007 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "AnimationController.h"
#include "AppConfig.h"
#include "AppVerify.h"
#include "AppVersion.h"
#include "FileDescriptor.h"
#include "MovieExporter.h"
#include "OptionsMovieExporter.h"
#include "PlugInArgList.h"
#include "PlugInManagerServices.h"
#include "Progress.h"
#include "StringUtilities.h"
#include "View.h"

#include <QtCore/QString>
#include <QtGui/QImage>

#include <sstream>
#include <string>
#include <vector>
#include <stdio.h>
using boost::rational;
using boost::rational_cast;
using namespace std;

template<>
Motion_Est_ID StringUtilities::fromXmlString<Motion_Est_ID>(string value, bool* pError)
{
   if (pError != NULL)
   {
      *pError = false;
   }

   if (value == "Zero")
   {
      return ME_ZERO;
   }
   else if (value == "PHODS")
   {
      return ME_PHODS;
   }
   else if (value == "Log")
   {
      return ME_LOG;
   }
   else if (value == "X1")
   {
      return ME_X1;
   }
   else if (value == "EPZS")
   {
      return ME_EPZS;
   }
   else if (value == "FULL")
   {
      return ME_FULL;
   }

   if (pError != NULL)
   {
      *pError = true;
   }

   return Motion_Est_ID();
}

namespace
{
   const unsigned int sDefaultBitrate = 4500;
   QString logBuffer;
};

extern "C" static void av_log_callback(void *pPtr, int level, const char *pFmt, va_list vl)
{
   if (level > av_log_level)
   {
      return;
   }
   logBuffer.vsprintf(pFmt, vl);
}

MovieExporter::MovieExporter() :
   mpStep(NULL),
   mpPicture(NULL),
   mpVideoOutbuf(NULL),
   mFrameCount(0),
   mVideoOutbufSize(0)
{
   logBuffer.clear();
   av_log_set_callback(av_log_callback);
   setCreator("Ball Aerospace & Technologies Corp.");
   setCopyright(APP_COPYRIGHT);
   setVersion(APP_VERSION_NUMBER);
   av_register_all();
   setDescription("Export the contents of a view as it is updated during animation playback.");
   setSubtype(TypeConverter::toString<View>());
   setDescriptorId("{99F399A8-B003-4fdc-99A0-F45261144FF4}");
   allowMultipleInstances(true);
   setAbortSupported(true);
}

MovieExporter::~MovieExporter()
{
}

bool MovieExporter::getInputSpecification(PlugInArgList*& pInArgList)
{
   Service<PlugInManagerServices> pPlugInManager;
   pInArgList = pPlugInManager->getPlugInArgList();

   VERIFY(pInArgList != NULL);
   VERIFY(pInArgList->addArg<Progress>(ProgressArg(), NULL));
   VERIFY(pInArgList->addArg<FileDescriptor>(ExportDescriptorArg()));
   VERIFY(pInArgList->addArg<View>(ExportItemArg()));
   VERIFY(pInArgList->addArg<unsigned int>("Resolution X"));
   VERIFY(pInArgList->addArg<unsigned int>("Resolution Y"));
   VERIFY(pInArgList->addArg<int>("Framerate Numerator"));
   VERIFY(pInArgList->addArg<int>("Framerate Denominator"));
   VERIFY(pInArgList->addArg<unsigned int>("Bitrate"));
   string description("Time for time-based animations. "
      "1-based frame number for frame-based animations, i.e., first frame is 1.");
   VERIFY(pInArgList->addArg<double>("Start Export", description));
   VERIFY(pInArgList->addArg<double>("Stop Export", description));

   return true;
}

bool MovieExporter::getOutputSpecification(PlugInArgList*& pOutArgList)
{
   pOutArgList = NULL;
   return true;
}

void MovieExporter::log_error(const string &msg)
{
   if (!logBuffer.isEmpty())
   {
      MessageResource pMsg("Detailed Message", "app", "E526E292-0713-41E6-92C1-4C9A2FA9C776");
      pMsg->addProperty("Message", logBuffer.toStdString());
   }
   if (mpProgress != NULL)
   {
      string progressMessage = msg;
      if (!logBuffer.isEmpty())
      {
         progressMessage += "\n";
         progressMessage += logBuffer.toStdString();
      }
      mpProgress->updateProgress(progressMessage, 100, ERRORS);
   }
   mpStep->finalize(Message::Failure, msg);
   logBuffer.clear();
}

bool MovieExporter::execute(PlugInArgList *pInArgList, PlugInArgList *pOutArgList)
{
   if (mpOptionWidget.get() != NULL)
   {
      mpOptionWidget->applyChanges();
   }

   FileDescriptor *pFileDescriptor(NULL);
   string filename;
   View *pView(NULL);
   AnimationController *pController(NULL);
   AVOutputFormat *pOutFormat(NULL);
   unsigned int resolutionX(0);
   unsigned int resolutionY(0);
   rational<int> framerate(0);
   unsigned int bitrate(0);
   double startExport(0.0);
   double stopExport(0.0);

   mpProgress = NULL;
   mpStep = StepResource("Export movie", "app", "2233BFC9-9C51-4e31-A8C5-2512925CBE6D");

   // get input arguments and log some useful info about them
   { // scope the MessageResource
      MessageResource pMsg("Input arguments", "app", "4551F478-E182-4b56-B88F-6682F0E3A2CF");

      mpProgress = pInArgList->getPlugInArgValue<Progress>(ProgressArg());
      pMsg->addBooleanProperty("Progress Present", (mpProgress != NULL));

      pFileDescriptor = pInArgList->getPlugInArgValue<FileDescriptor>(ExportDescriptorArg());
      if (pFileDescriptor == NULL)
      {
         log_error("No file specified");
         return false;
      }
      pMsg->addProperty("Destination", pFileDescriptor->getFilename());
      filename = pFileDescriptor->getFilename().getFullPathAndName();

      pView = pInArgList->getPlugInArgValue<View>(ExportItemArg());
      if (pView == NULL)
      {
         log_error("No view specified");
         return false;
      }

      pController = pView->getAnimationController();
      if (pController == NULL)
      {
         log_error("No animation controller specified");
         return false;
      }
      pMsg->addProperty("Animation Controller Name", pController->getName());

      if ((pOutFormat = getOutputFormat()) == NULL)
      {
         log_error("Can't determine output format or format not supported.");
         return false;
      }
      pMsg->addProperty("Format", pOutFormat->long_name);

      bool resolutionFromInputArgs(false);
      if (!pInArgList->getPlugInArgValue("Resolution X", resolutionX) ||
         !pInArgList->getPlugInArgValue("Resolution Y", resolutionY))
      {
         if (mpOptionWidget.get() != NULL)
         {
            mpOptionWidget->getResolution(resolutionX, resolutionY);
         }
      }
      else
      {
         resolutionFromInputArgs = true;
      }

      if (resolutionX <= 0 || resolutionY <= 0)
      {
         QWidget *pWidget = pView->getWidget();
         if (pWidget != NULL)
         {
            resolutionX = pWidget->width();
            resolutionY = pWidget->height();
            resolutionFromInputArgs = false;
         }
         else
         {
            log_error("Can't determine output resolution.");
            return false;
         }
      }

      if (resolutionFromInputArgs && ((resolutionX % 2) != 0 || (resolutionY % 2) != 0))
      {
         stringstream msg;
         msg << "The export resolution must be even numbers. The input arguments were X resolution of "
             << resolutionX << " and Y resolution of " << resolutionY << ".";
         log_error(msg.str());
         return false;
      }

      if ((resolutionX % 2) != 0)
      {
         ++resolutionX;
      }
      if ((resolutionY % 2) != 0)
      {
         ++resolutionY;
      }

      pMsg->addProperty("Resolution", QString("%1 x %2").arg(resolutionX).arg(resolutionY).toStdString());

      int framerateNum, framerateDen;
      // first, get the framerate from the arg list
      // next, try the option widget
      // next, get from the animation controller
      if (pInArgList->getPlugInArgValue("Framerate Numerator", framerateNum) &&
         pInArgList->getPlugInArgValue("Framerate Denominator", framerateDen))
      {
         try
         {
            framerate.assign(framerateNum, framerateDen);
         }
         catch (const boost::bad_rational&)
         {
            // Do nothing; the code below handles this case
         }
      }
      if (framerate == 0)
      {
         if (mpOptionWidget.get() != NULL)
         {
            framerate = mpOptionWidget->getFramerate();
         }
         else
         {
            framerate = pController->getMinimumFrameRate() * pController->getIntervalMultiplier();
         }
      }

      if (framerate == 0)
      {
         log_error("No framerate specified");
         return false;
      }

      // Validate the framerate
      boost::rational<int> validFrameRate = convertToValidFrameRate(framerate);
      if (validFrameRate != framerate)
      {
         QString msg = QString("The selected animation frame rate (%1/%2 fps) can not be represented in the "
                               "selected movie format. A frame rate of %3/%4 fps is being used instead.")
                              .arg(framerate.numerator())
                              .arg(framerate.denominator())
                              .arg(validFrameRate.numerator())
                              .arg(validFrameRate.denominator());
         mpProgress->updateProgress(msg.toStdString(), 0, WARNING);

         framerate = validFrameRate;
      }

      pMsg->addProperty("Framerate",
         QString("%1/%2").arg(framerate.numerator()).arg(framerate.denominator()).toStdString());

      if (!pInArgList->getPlugInArgValue("Bitrate", bitrate))
      {
         if (mpOptionWidget.get() != NULL)
         {
            bitrate = mpOptionWidget->getBitrate();
         }
         else
         {
            bitrate = OptionsMovieExporter::getSettingBitrate();
         }
      }
      pMsg->addProperty("Bitrate", QString::number(bitrate).toStdString());

      FrameType eType = pController->getFrameType();
      if (!pInArgList->getPlugInArgValue("Start Export", startExport))
      {
         if (mpOptionWidget.get() != NULL)
         {
            startExport = mpOptionWidget->getStart();
         }
         else
         {
            startExport = pController->getStartFrame();
         }
      }

      if (!pInArgList->getPlugInArgValue("Stop Export", stopExport))
      {
         if (mpOptionWidget.get() != NULL)
         {
            stopExport = mpOptionWidget->getStop();
         }
         else
         {
            stopExport = pController->getStopFrame();
         }
      }
      string valueType("Time");
      if (eType == FRAME_ID)
      {
         valueType = "Frame";
         --startExport;   // frame id type uses 1-number of frames so subtract 1 for looping
         --stopExport;
      }

      pMsg->addProperty("Start "+valueType, QString::number(startExport).toStdString());
      pMsg->addProperty("Stop "+valueType, QString::number(stopExport).toStdString());
   }

   AvFormatContextResource pFormat(pOutFormat);
   VERIFY(pFormat != NULL);
   snprintf(pFormat->filename, sizeof(pFormat->filename), "%s", filename.c_str());
   AvStreamResource pVideoStream(pFormat, pOutFormat->video_codec);
   if (pVideoStream == NULL)
   {
      log_error("Unable to create video stream.");
      return false;
   }

   /**
    * allow changing of:
    *    dia_size/
    */
   AVCodecContext *pCodecContext = pVideoStream->codec;
   if (!setAvCodecOptions(pCodecContext, pInArgList))
   {
      log_error("Unable to initialize CODEC options");
      return false;
   }
   // set time_base, width, height, and bitrate here since
   // they can be passed in via the input args
   pCodecContext->width = resolutionX;
   pCodecContext->height = resolutionY;
   pCodecContext->bit_rate = bitrate * 1000;
   // the AVCodecContext wants a time_base which is
   // the inverse of fps.
   pCodecContext->time_base.num = framerate.denominator();
   pCodecContext->time_base.den = framerate.numerator();

   if (av_set_parameters(pFormat, NULL) < 0)
   {
      log_error("Invalid output format parameters.");
      return false;
   }
   if (!open_video(pFormat, pVideoStream))
   {
      log_error("Unable to open video.");
      return false;
   }
   if (url_fopen(&pFormat->pb, filename.c_str(), URL_WRONLY) < 0)
   {
      log_error("Could not open the output file.");
      return false;
   }
   av_write_header(pFormat);

   // calculate time interval
   double frameSpeed = pController->getIntervalMultiplier();
   if ((framerate < pController->getMinimumFrameRate() * frameSpeed) && (mpProgress != NULL))
   {
      mpProgress->updateProgress("The selected output frame rate may not encode all the frames in the movie.  "
                                 "Frames may be dropped.", 0, WARNING);
   }

   double interval = rational_cast<double>(frameSpeed / framerate);

   // export the frames
   AVFrame *pTmpPicture = alloc_picture(PIX_FMT_RGBA32, pCodecContext->width, pCodecContext->height);
   QImage image(pTmpPicture->data[0], pCodecContext->width, pCodecContext->height, QImage::Format_ARGB32);
   FrameType eType = pController->getFrameType();

   // For frame id based animation, each band of the data set fills one second of animation. 
   // If the requested frame rate for export is 15 fps, then each band is replicated 15 times. The execution
   // loop uses a pseudo time value, video_pts, to walk through the animation. The interval between 
   // exported frames is the inverse of the frame rate, e.g., for 15 fps the interval is 0.06667.
   // To fully export the last requested frame, we need to add just under an extra pseudo second to
   // the end time - stopExport. If we added a full extra second, we would export one video frame of the band past
   // the last requested frame - could cause crash if the last request frame was the last band.
   if (eType == FRAME_ID)
   {
      stopExport += 0.99;
   }

   for (double video_pts = startExport; video_pts <= stopExport; video_pts += interval)
   {
      if (isAborted() == true)
      {
         // reset resources to close output file so it can be deleted
         pVideoStream = AvStreamResource();
         pFormat = AvFormatContextResource(NULL);
         mpPicture = NULL;
         mpVideoOutbuf = NULL;
         remove(filename.c_str());

         if (mpProgress != NULL)
         {
            mpProgress->updateProgress("Export aborted", 0, ABORT);
         }
         mpStep->finalize(Message::Abort);
         return false;
      }

      // generate the next frame
      pController->setCurrentFrame(video_pts);
      if (mpProgress != NULL)
      {
         double progressValue = (video_pts - startExport) /
                                (stopExport - startExport) * 100.0;
         mpProgress->updateProgress("Saving movie", static_cast<int>(progressValue), NORMAL);
      }
      pView->getCurrentImage(image);
      img_convert(reinterpret_cast<AVPicture*>(mpPicture),
         pCodecContext->pix_fmt,
         reinterpret_cast<AVPicture*>(pTmpPicture),
         PIX_FMT_RGBA32,
         pCodecContext->width,
         pCodecContext->height);
      if (!write_video_frame(pFormat, pVideoStream))
      {
         // reset resources to close output file so it can be deleted
         pVideoStream = AvStreamResource();
         pFormat = AvFormatContextResource(NULL);
         mpPicture = NULL;
         mpVideoOutbuf = NULL;
         remove(filename.c_str());
         string msg = "Can't write frame.";
         log_error(msg.c_str());
         return false;
      }
   }
   for (int frame = 0; frame < pCodecContext->delay; ++frame)
   {
      write_video_frame(pFormat, pVideoStream);
   }

   av_write_trailer(pFormat);

   if (mpProgress != NULL)
   {
      mpProgress->updateProgress("Finished saving movie", 100, NORMAL);
   }
   mpStep->finalize(Message::Success);
   return true;
}

ValidationResultType MovieExporter::validate(const PlugInArgList* pArgList, string& errorMessage) const
{
   /* validate some arguments */
   ValidationResultType result = ExporterShell::validate(pArgList, errorMessage);
   if (result != VALIDATE_SUCCESS)
   {
      return result;
   }
   View *pView = pArgList->getPlugInArgValue<View>(ExportItemArg());
   if (pView == NULL)
   {
      errorMessage = "No view specified. Nothing to export.";
      return VALIDATE_FAILURE;
   }
   AnimationController *pController = pView->getAnimationController();
   if (pController == NULL)
   {
      errorMessage = "No animation is associated with this view. Nothing to export.";
      return VALIDATE_FAILURE;
   }

   /* validate the framerate */
   boost::rational<int> expectedFrameRate;

   // first, get the framerate from the arg list
   // next, try the option widget
   // next, get from the animation controller
   // finally, default to the config settings
   int framerateNum, framerateDen;
   if (pArgList->getPlugInArgValue("Framerate Numerator", framerateNum) &&
      pArgList->getPlugInArgValue("Framerate Denominator", framerateDen))
   {
      expectedFrameRate.assign(framerateNum, framerateDen);
   }
   if (expectedFrameRate == 0)
   {
      if (mpOptionWidget.get() != NULL)
      {
         expectedFrameRate = mpOptionWidget->getFramerate();
      }
      if (expectedFrameRate == 0)
      {
         expectedFrameRate = pController->getMinimumFrameRate() * pController->getIntervalMultiplier();
      }
      if (expectedFrameRate == 0)
      {
         errorMessage = "No framerate specified";
         return VALIDATE_FAILURE;
      }
   }

   boost::rational<int> actualFrameRate = convertToValidFrameRate(expectedFrameRate);
   if (actualFrameRate != 0)
   {
      if (actualFrameRate < pController->getMinimumFrameRate() * pController->getIntervalMultiplier())
      {
         errorMessage = "The selected output frame rate may not encode all the frames in the movie.  "
                        "Frames may be dropped.";
         result = VALIDATE_INFO;
      }
      if (actualFrameRate != expectedFrameRate)
      {
         QString msg = QString("The selected animation frame rate (%1/%2 fps) can not be represented in the "
            "selected movie format. A frame rate of %3/%4 fps is being used instead.")
            .arg(expectedFrameRate.numerator())
            .arg(expectedFrameRate.denominator())
            .arg(actualFrameRate.numerator())
            .arg(actualFrameRate.denominator());
         if (!errorMessage.empty())
         {
            errorMessage += "\n\n";
         }
         errorMessage += msg.toStdString();
         result = VALIDATE_INFO;
      }
   }

   return result;
}

QWidget* MovieExporter::getExportOptionsWidget(const PlugInArgList* pInArgList)
{
   if (mpOptionWidget.get() == NULL)
   {
      mpOptionWidget = auto_ptr<OptionsMovieExporter>(new OptionsMovieExporter());
      VERIFYRV(mpOptionWidget.get() != NULL, NULL);
      mpOptionWidget->setPromptUserToSaveSettings(true);

      AVOutputFormat* pOutFormat = getOutputFormat();
      VERIFYRV(pOutFormat, NULL);
      AVCodec* pCodec = avcodec_find_encoder(pOutFormat->video_codec);
      VERIFYRV(pCodec, NULL);
      if (pCodec->supported_framerates != NULL)
      {
         vector<boost::rational<int> > frameRates;
         try
         {
            for (int idx = 0;; ++idx)
            {
               boost::rational<int> frameRate(pCodec->supported_framerates[idx].num,
                  pCodec->supported_framerates[idx].den);
               if (frameRate == 0)
               {
                  break;
               }

               frameRates.push_back(frameRate);
            }
         }
         catch (const boost::bad_rational&)
         {
            // intentionally left blank
         }

         mpOptionWidget->setFramerates(frameRates);
      }

      View* pView = pInArgList->getPlugInArgValue<View>(ExportItemArg());
      if (pView != NULL)
      {
         QWidget* pWidget = pView->getWidget();
         if (pWidget != NULL)
         {
            mpOptionWidget->setResolution(pWidget->width(), pWidget->height());
         }

         AnimationController* pController = pView->getAnimationController();
         if (pController != NULL)
         {
            // Frame type
            FrameType eType = pController->getFrameType();
            mpOptionWidget->setFrameType(eType);

            // Start and stop values
            double start = pController->getStartFrame();
            double stop = pController->getStopFrame();
            if (eType == FRAME_ID) // values are frame numbers so add 1 so first frame is 1 and not 0
            {
               ++start;
               ++stop;
            }

            mpOptionWidget->setRange(start, stop);
            mpOptionWidget->setStart(start);
            mpOptionWidget->setStop(stop);

            // Frame rate
            rational<int> frameRate = pController->getMinimumFrameRate() * pController->getIntervalMultiplier();
            frameRate = convertToValidFrameRate(frameRate);
            mpOptionWidget->setFramerate(frameRate);
         }
      }
   }

   return mpOptionWidget.get();
}

bool MovieExporter::setAvCodecOptions(AVCodecContext *pContext, PlugInArgList *pInArgList)
{
   if (pContext == NULL || pInArgList == NULL)
   {
      return false;
   }
   string meMethod = OptionsMovieExporter::getSettingMeMethod();
   pContext->gop_size = OptionsMovieExporter::getSettingGopSize();
   pContext->qcompress = OptionsMovieExporter::getSettingQCompress();
   pContext->qblur = OptionsMovieExporter::getSettingQBlur();
   pContext->qmin = OptionsMovieExporter::getSettingQMinimum();
   pContext->qmax = OptionsMovieExporter::getSettingQMaximum();
   pContext->max_qdiff = OptionsMovieExporter::getSettingQDiffMaximum();
   pContext->max_b_frames = OptionsMovieExporter::getSettingMaxBFrames();
   pContext->b_quant_factor = OptionsMovieExporter::getSettingBQuantFactor();
   pContext->b_quant_offset = OptionsMovieExporter::getSettingBQuantOffset();
   pContext->dia_size = OptionsMovieExporter::getSettingDiaSize();
   int newFlags = OptionsMovieExporter::getSettingFlags();
   if (mpOptionWidget.get() != NULL)
   {
      meMethod = mpOptionWidget->getMeMethod();
      pContext->gop_size= mpOptionWidget->getGopSize();
      pContext->qcompress = mpOptionWidget->getQCompress();
      pContext->qblur = mpOptionWidget->getQBlur();
      pContext->qmin = mpOptionWidget->getQMinimum();
      pContext->qmax = mpOptionWidget->getQMaximum();
      pContext->max_qdiff = mpOptionWidget->getQDiffMaximum();
      pContext->max_b_frames = mpOptionWidget->getMaxBFrames();
      pContext->b_quant_factor = mpOptionWidget->getBQuantFactor();
      pContext->b_quant_offset = mpOptionWidget->getBQuantOffset();
      pContext->dia_size = mpOptionWidget->getDiaSize();
      newFlags = mpOptionWidget->getFlags();
   }
   pContext->me_method = StringUtilities::fromXmlString<Motion_Est_ID>(meMethod);
   pContext->flags |= newFlags;
   if (pContext->flags & CODEC_FLAG_TRELLIS_QUANT)
   {
      pContext->trellis = 1;
   }
   else
   {
      pContext->trellis = 0;
   }

   return true;
}

bool MovieExporter::open_video(AVFormatContext *pFormat, AVStream *pVideoStream)
{
   VERIFY(pFormat && pVideoStream);
   AVCodecContext *pCodecContext = pVideoStream->codec;
   AVCodec *pCodec = avcodec_find_encoder(pCodecContext->codec_id);
   if (pCodec == NULL)
   {
      return false;
   }

   if ((pCodec = avcodec_find_encoder(pCodecContext->codec_id)) == NULL)
   {
      return false;
   }

   if (avcodec_open(pCodecContext, pCodec) < 0)
   {
      return false;
   }

   mpVideoOutbuf = NULL;
   if (!(pFormat->oformat->flags & AVFMT_RAWPICTURE))
   {
      mVideoOutbufSize = 200000;
      mpVideoOutbuf = reinterpret_cast<uint8_t*>(malloc(mVideoOutbufSize));
   }

   /* allocate the encoded raw picture */
   if ((mpPicture = alloc_picture(pCodecContext->pix_fmt, pCodecContext->width, pCodecContext->height)) == NULL)
   {
      return false;
   }

   return true;
}

AVFrame *MovieExporter::alloc_picture(int pixFmt, int width, int height)
{
   AVFrame *pPicture = avcodec_alloc_frame();
   VERIFYRV(pPicture, NULL);
   int size = avpicture_get_size(pixFmt, width, height);
   uint8_t *pPictureBuf = reinterpret_cast<uint8_t*>(malloc(size));
   if (pPictureBuf == NULL)
   {
      av_free(pPicture);
      return NULL;
   }
   avpicture_fill(reinterpret_cast<AVPicture*>(pPicture), pPictureBuf, pixFmt, width, height);
   return pPicture;
}

bool MovieExporter::write_video_frame(AVFormatContext *pFormat, AVStream *pVideoStream)
{
   VERIFY(pFormat && pVideoStream);
   int out_size = 0;
   int ret = 0;
   AVCodecContext *pCodecContext = pVideoStream->codec;

   if (pFormat->oformat->flags & AVFMT_RAWPICTURE)
   {
      /* raw video case. The API will change slightly in the near
      future for that */
      AVPacket pkt;
      av_init_packet(&pkt);

      pkt.flags |= PKT_FLAG_KEY;
      pkt.stream_index = pVideoStream->index;
      pkt.data = reinterpret_cast<uint8_t*>(mpPicture);
      pkt.size = sizeof(AVPicture);

      ret = av_write_frame(pFormat, &pkt);
   }
   else
   {
      /* encode the image */
      out_size = avcodec_encode_video(pCodecContext, mpVideoOutbuf, mVideoOutbufSize, mpPicture);
      /* if zero size, it means the image was buffered */
      if (out_size > 0)
      {
         AVPacket pkt;
         av_init_packet(&pkt);

         pkt.pts= av_rescale_q(pCodecContext->coded_frame->pts, pCodecContext->time_base, pVideoStream->time_base);
         if (pCodecContext->coded_frame->key_frame)
         {
            pkt.flags |= PKT_FLAG_KEY;
         }
         pkt.stream_index = pVideoStream->index;
         pkt.data = mpVideoOutbuf;
         pkt.size = out_size;

         /* write the compressed frame in the media file */
         ret = av_write_frame(pFormat, &pkt);
      }
      else
      {
         ret = 0;
      }
   }
   if (ret != 0)
   {
      return false;
   }
   ++mFrameCount;
   return true;
}

boost::rational<int> MovieExporter::convertToValidFrameRate(const boost::rational<int>& frameRate) const
{
   boost::rational<int> validFrameRate;
   try
   {
      AVOutputFormat* pOutFormat = getOutputFormat();
      VERIFY(pOutFormat != NULL);
      AVCodec* pCodec = avcodec_find_encoder(pOutFormat->video_codec);
      VERIFY(pCodec != NULL);

      if (pCodec->supported_framerates != NULL)
      {
         for (int idx = 0; ; ++idx)
         {
            boost::rational<int> supportedFrameRate(pCodec->supported_framerates[idx].num,
                                                    pCodec->supported_framerates[idx].den);
            if (supportedFrameRate == 0)
            {
               break;
            }

            if ((supportedFrameRate == frameRate) || // The frame rate matches a supported frame rate
               (validFrameRate == 0) ||              // A valid frame rate has not yet been set
               // The difference between supported FR and the FR is closer than the valid FR difference
               ((supportedFrameRate - frameRate) < (validFrameRate - frameRate)) ||
               // The supported FR is greater than the FR and the valid FR is less than the FR
               ((validFrameRate - frameRate) < 0 && (supportedFrameRate - frameRate) > 0))
            {
               validFrameRate = supportedFrameRate;
            }
         }
      }
      else
      {
         validFrameRate = frameRate;
      }
   }
   catch (const boost::bad_rational&)
   {
      // Intentionally left blank
   }

   return validFrameRate;
}
