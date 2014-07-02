#ifndef _tgl_zblit_h_
#define _tgl_zblit_h_

#include "graphics/pixelbuffer.h"

namespace TinyGL {


	void tglUploadBlitTexture(int* textureHandle, Graphics::PixelBuffer &buffer, int colorKey);
	void tglDisposeBlitTexture(int textureHandle);

	template <bool disableBlending = false, bool disableColoring = false, bool disableTransform = false>
	void tglBlit(int blitTextureHandle, int dstX, int dstY, int width, int height, int srcX, int srcY, int srcWidth, int srcHeight, float rotation, float rTint, float gTint, float bTint, float aTint);

	// Disables blending, coloring and transform.
	void tglBlitFast(int blitTextureHandle, int x, int y, int width, int height);
}

template <bool disableBlending = false, bool disableColoring = false, bool disableTransform = false>
void TinyGL<disableBlending>::tglBlit(int blitTextureHandle, int dstX, int dstY, int width, int height, int srcX, int srcY, int srcWidth, int srcHeight, float rotation, float rTint, float gTint, float bTint, float aTint)
{

}

#endif
