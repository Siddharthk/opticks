/*
 * The information in this file is
 * Copyright(c) 2007 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef GPUTILE_H
#define GPUTILE_H

#include "Tile.h"
#include "ImageFilterDescriptor.h"
#include "glCommon.h"
#include "TypesFile.h"

#include <vector>

class ColorBuffer;
class ImageFilter;
class ImageLoader;

class GpuTile : public Tile
{
public:
   GpuTile();
   ~GpuTile();

   void setupTile(void* pData, EncodingType encodingType, unsigned int index);
   void draw(CGparameter outputCgTextureParam, GLint textureMode);

   ImageFilter *createFilter(ImageFilterDescriptor *pDescriptor);
   ImageFilter *getFilter(ImageFilterDescriptor *pDescriptor) const;
   bool hasFilter(ImageFilterDescriptor *pDescriptor) const;
   void destroyFilter(ImageFilterDescriptor *pDescriptor);
   void resetFilter(ImageFilterDescriptor *pDescriptor);
   void freezeFilter(ImageFilterDescriptor *pDescriptor, bool toggle = true);
   unsigned int readFilterBuffer(GLint xCoord, GLint yCoord, GLsizei width, GLsizei height, GLvoid *pValues);

   bool isTextureReady(unsigned int index) const;

   std::vector<ImageFilterDescriptor*> getFilters() const;

protected:
   void applyFilters();

private:
   ImageLoader *mpImageLoader;
   ImageLoader *mpImageReader;
   ColorBuffer *mpOutputColorBuffer;

   bool mbInitialized;

   std::vector<ImageFilter*> mFilters;
};

#endif
