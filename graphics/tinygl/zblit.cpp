#include "graphics/tinygl/zblit.h"

namespace TinyGL {

	void tglUploadBlitTexture(int* textureHandle, Graphics::PixelBuffer &buffer, int colorKey)
	{

	}

	void tglDisposeBlitTexture(int textureHandle)
	{

	}

	void tglBlitFast(int blitTextureHandle, int x, int y, int width, int height)
	{
		tglBlit<true, true, true>(blitTextureHandle, x, y, width, height, 0, 0, width, height, 0, 255, 255, 255 ,255);
	}

}