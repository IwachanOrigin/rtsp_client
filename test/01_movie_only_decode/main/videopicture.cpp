
#include "videopicture.h"

VideoPicture::VideoPicture()
  : frame(nullptr)
  , width(0)
  , height(0)
  , allocated(0)
  , pts(0.0)
{
}

VideoPicture::~VideoPicture()
{
}

