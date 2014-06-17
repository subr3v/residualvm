
#include "common/endian.h"
#include "graphics/tinygl/zbuffer.h"

namespace TinyGL {

#define ZCMP(z, zpix) ((z) >= (zpix))

static const int DRAW_DEPTH_ONLY = 0;
static const int DRAW_FLAT = 1;
static const int DRAW_SMOOTH = 2;
static const int DRAW_MAPPING = 3;
static const int DRAW_MAPPING_PERSPECTIVE = 4;
static const int DRAW_SHADOW_MASK = 5;
static const int DRAW_SHADOW = 6;

static const int NB_INTERP = 8;

#define SAR_RND_TO_ZERO(v,n) (v / (1 << n))

FORCEINLINE static void putPixelMapping(PIXEL *pp, unsigned int *pz, Graphics::PixelBuffer &texture, int _a, unsigned int &z,  unsigned int &t, unsigned int &s, int &dzdx, int &dsdx, int &dtdx) {
	if (ZCMP(z, pz[_a])) {
		pp[_a] = texture.getRawBuffer()[((t & 0x3FC00000) | s) >> 14];
		pz[_a] = z;
	}
	z += dzdx;
	s += dsdx;
	t += dtdx;
}

FORCEINLINE static void putPixelFlat(PIXEL *pp, unsigned int *pz, int _a, unsigned int &z, int color, int &dzdx) {
	if (ZCMP(z, pz[_a])) {
		pp[_a] = color;
		pz[_a] = z;
	}
	z += dzdx;
}

FORCEINLINE static void putPixelDepth(unsigned int *pz, int _a, unsigned int &z, int &dzdx) {
	if (ZCMP(z, pz[_a])) {
		pz[_a] = z;
	}
	z += dzdx;
}

FORCEINLINE static void putPixelSmooth(Graphics::PixelBuffer &buf, unsigned int *pz, int _a, unsigned int &z, int &tmp, unsigned int &rgb, int &dzdx, unsigned int &drgbdx) {
	if (ZCMP(z, pz[_a])) {
		tmp = rgb & 0xF81F07E0;
		buf.setPixelAt(_a, tmp | (tmp >> 16));
		pz[_a] = z;
	}
	z += dzdx;
	rgb = (rgb + drgbdx) & (~0x00200800);
}

FORCEINLINE static void putPixelMappingPerspective(Graphics::PixelBuffer &buf, Graphics::PixelFormat &textureFormat, Graphics::PixelBuffer &texture, unsigned int *pz, int _a, unsigned int &z, unsigned int &t, unsigned int &s, int &tmp, unsigned int &rgb, int &dzdx, int &dsdx, int &dtdx, unsigned int &drgbdx) {
	if (ZCMP(z, pz[_a])) {
		unsigned ttt = (t & 0x003FC000) >> (9 - PSZSH);
		unsigned sss = (s & 0x003FC000) >> (17 - PSZSH);
		int pixel = ((ttt | sss) >> 1);
		uint8 alpha, c_r, c_g, c_b;
		uint32 *textureBuffer = (uint32 *)texture.getRawBuffer(pixel);
		uint32 col = *textureBuffer;
		alpha = (col >> textureFormat.aShift) & 0xFF;
		c_r = (col >> textureFormat.rShift) & 0xFF;
		c_g = (col >> textureFormat.gShift) & 0xFF;
		c_b = (col >> textureFormat.bShift) & 0xFF;
		if (alpha == 0xFF) {
			tmp = rgb & 0xF81F07E0;
			unsigned int light = tmp | (tmp >> 16);
			unsigned int l_r = (light & 0xF800) >> 8;
			unsigned int l_g = (light & 0x07E0) >> 3;
			unsigned int l_b = (light & 0x001F) << 3;
			c_r = (c_r * l_r) / 256;
			c_g = (c_g * l_g) / 256;
			c_b = (c_b * l_b) / 256;
			buf.setPixelAt(_a, c_r, c_g, c_b);
			pz[_a] = z;
		}
	}
	z += dzdx;
	s += dsdx;
	t += dtdx;
	rgb = (rgb + drgbdx) & (~0x00200800);
}

template <bool interpRGB, bool interpZ, bool interpST, bool interpSTZ, int drawLogic>
void FrameBuffer::fillTriangle(ZBufferPoint *p0, ZBufferPoint *p1, ZBufferPoint *p2) {
	Graphics::PixelBuffer texture;
	float fdzdx, fndzdx, ndszdx, ndtzdx;
	int _drgbdx;

	ZBufferPoint *tp, *pr1 = 0, *pr2 = 0, *l1 = 0, *l2 = 0;
	float fdx1, fdx2, fdy1, fdy2, fz0, d1, d2;
	unsigned int *pz1;
	unsigned char *pm1;
	int part, update_left, update_right;
	int color;

	int nb_lines, dx1, dy1, tmp, dx2, dy2;

	int error = 0, derror = 0;
	int x1 = 0, dxdy_min = 0, dxdy_max = 0;
	// warning: x2 is multiplied by 2^16
	int x2 = 0, dx2dy2 = 0;

	int z1 = 0, dzdx = 0, dzdy = 0, dzdl_min = 0, dzdl_max = 0;

	int r1 = 0, drdx = 0, drdy = 0, drdl_min = 0, drdl_max = 0;
	int g1 = 0, dgdx = 0, dgdy = 0, dgdl_min = 0, dgdl_max = 0;
	int b1 = 0, dbdx = 0, dbdy = 0, dbdl_min = 0, dbdl_max = 0;

	int s1 = 0, dsdx = 0, dsdy = 0, dsdl_min = 0, dsdl_max = 0;
	int t1 = 0, dtdx = 0, dtdy = 0, dtdl_min = 0, dtdl_max = 0;

	float sz1 = 0.0, dszdx = 0, dszdy = 0, dszdl_min = 0.0, dszdl_max = 0.0;
	float tz1 = 0.0, dtzdx = 0, dtzdy = 0, dtzdl_min = 0.0, dtzdl_max = 0.0;

	// we sort the vertex with increasing y
	if (p1->y < p0->y) {
		tp = p0;
		p0 = p1;
		p1 = tp;
	}
	if (p2->y < p0->y) {
		tp = p2;
		p2 = p1;
		p1 = p0;
		p0 = tp;
	} else if (p2->y < p1->y) {
		tp = p1;
		p1 = p2;
		p2 = tp;
	}

	// we compute dXdx and dXdy for all interpolated values

	fdx1 = (float)(p1->x - p0->x);
	fdy1 = (float)(p1->y - p0->y);

	fdx2 = (float)(p2->x - p0->x);
	fdy2 = (float)(p2->y - p0->y);

	fz0 = fdx1 * fdy2 - fdx2 * fdy1;
	if (fz0 == 0)
		return;
	fz0 = (float)(1.0 / fz0);

	fdx1 *= fz0;
	fdy1 *= fz0;
	fdx2 *= fz0;
	fdy2 *= fz0;

	if (interpZ) {
		d1 = (float)(p1->z - p0->z);
		d2 = (float)(p2->z - p0->z);
		dzdx = (int)(fdy2 * d1 - fdy1 * d2);
		dzdy = (int)(fdx1 * d2 - fdx2 * d1);
	}

	if (interpRGB) {
		d1 = (float)(p1->r - p0->r);
		d2 = (float)(p2->r - p0->r);
		drdx = (int)(fdy2 * d1 - fdy1 * d2);
		drdy = (int)(fdx1 * d2 - fdx2 * d1);

		d1 = (float)(p1->g - p0->g);
		d2 = (float)(p2->g - p0->g);
		dgdx = (int)(fdy2 * d1 - fdy1 * d2);
		dgdy = (int)(fdx1 * d2 - fdx2 * d1);

		d1 = (float)(p1->b - p0->b);
		d2 = (float)(p2->b - p0->b);
		dbdx = (int)(fdy2 * d1 - fdy1 * d2);
		dbdy = (int)(fdx1 * d2 - fdx2 * d1);
	}

	if (interpST) {
		d1 = (float)(p1->s - p0->s);
		d2 = (float)(p2->s - p0->s);
		dsdx = (int)(fdy2 * d1 - fdy1 * d2);
		dsdy = (int)(fdx1 * d2 - fdx2 * d1);

		d1 = (float)(p1->t - p0->t);
		d2 = (float)(p2->t - p0->t);
		dtdx = (int)(fdy2 * d1 - fdy1 * d2);
		dtdy = (int)(fdx1 * d2 - fdx2 * d1);
	}

	if (interpSTZ) {
		float zz;
		zz = (float)p0->z;
		p0->sz = (float)p0->s * zz;
		p0->tz = (float)p0->t * zz;
		zz = (float)p1->z;
		p1->sz = (float)p1->s * zz;
		p1->tz = (float)p1->t * zz;
		zz = (float)p2->z;
		p2->sz = (float)p2->s * zz;
		p2->tz = (float)p2->t * zz;

		d1 = p1->sz - p0->sz;
		d2 = p2->sz - p0->sz;
		dszdx = (fdy2 * d1 - fdy1 * d2);
		dszdy = (fdx1 * d2 - fdx2 * d1);

		d1 = p1->tz - p0->tz;
		d2 = p2->tz - p0->tz;
		dtzdx = (fdy2 * d1 - fdy1 * d2);
		dtzdy = (fdx1 * d2 - fdx2 * d1);
	}

	// screen coordinates

	byte *pp1 = pbuf.getRawBuffer() + linesize * p0->y;
	pz1 = zbuf + p0->y * xsize;

	switch (drawLogic) {
	case DRAW_SHADOW_MASK:
		pm1 = shadow_mask_buf + p0->y * xsize;
		break;
	case DRAW_SHADOW:
		pm1 = shadow_mask_buf + p0->y * xsize;
		color = RGB_TO_PIXEL(shadow_color_r, shadow_color_g, shadow_color_b);
		break;
	case DRAW_DEPTH_ONLY:
		break;
	case DRAW_FLAT:
		color = RGB_TO_PIXEL(p2->r, p2->g, p2->b);
		break;
	case DRAW_SMOOTH:
		_drgbdx = (SAR_RND_TO_ZERO(drdx, 6) << 22) & 0xFFC00000;
		_drgbdx |= SAR_RND_TO_ZERO(dgdx, 5) & 0x000007FF;
		_drgbdx |= (SAR_RND_TO_ZERO(dbdx, 7) << 12) & 0x001FF000;
		break;
	case DRAW_MAPPING:
		texture = current_texture;
		break;
	case DRAW_MAPPING_PERSPECTIVE:
		texture = current_texture;
		assert(texture.getFormat().bytesPerPixel == 4);
		fdzdx = (float)dzdx;
		fndzdx = NB_INTERP * fdzdx;
		ndszdx = NB_INTERP * dszdx;
		ndtzdx = NB_INTERP * dtzdx;
		_drgbdx = ((drdx / (1 << 6)) << 22) & 0xFFC00000;
		_drgbdx |= (dgdx / (1 << 5)) & 0x000007FF;
		_drgbdx |= ((dbdx / (1 << 7)) << 12) & 0x001FF000;
		break;
	default:
		break;
	}

	Graphics::PixelFormat textureFormat = texture.getFormat();

	for (part = 0; part < 2; part++) {
		if (part == 0) {
			if (fz0 > 0) {
				update_left = 1;
				update_right = 1;
				l1 = p0;
				l2 = p2;
				pr1 = p0;
				pr2 = p1;
			} else {
				update_left = 1;
				update_right = 1;
				l1 = p0;
				l2 = p1;
				pr1 = p0;
				pr2 = p2;
			}
			nb_lines = p1->y - p0->y;
		} else {
			// second part
			if (fz0 > 0) {
				update_left = 0;
				update_right = 1;
				pr1 = p1;
				pr2 = p2;
			} else {
				update_left = 1;
				update_right = 0;
				l1 = p1;
				l2 = p2;
			}
			nb_lines = p2->y - p1->y + 1;
		}

		// compute the values for the left edge

		if (update_left) {
			dy1 = l2->y - l1->y;
			dx1 = l2->x - l1->x;
			if (dy1 > 0)
				tmp = (dx1 << 16) / dy1;
			else
				tmp = 0;
			x1 = l1->x;
			error = 0;
			derror = tmp & 0x0000ffff;
			dxdy_min = tmp >> 16;
			dxdy_max = dxdy_min + 1;

			if (interpZ) {
				z1 = l1->z;
				dzdl_min = (dzdy + dzdx * dxdy_min);
				dzdl_max = dzdl_min + dzdx;
			}

			if (interpRGB) {
				r1 = l1->r;
				drdl_min = (drdy + drdx * dxdy_min);
				drdl_max = drdl_min + drdx;

				g1 = l1->g;
				dgdl_min = (dgdy + dgdx * dxdy_min);
				dgdl_max = dgdl_min + dgdx;

				b1 = l1->b;
				dbdl_min = (dbdy + dbdx * dxdy_min);
				dbdl_max = dbdl_min + dbdx;
			}

			if (interpST) {
				s1 = l1->s;
				dsdl_min = (dsdy + dsdx * dxdy_min);
				dsdl_max = dsdl_min + dsdx;

				t1 = l1->t;
				dtdl_min = (dtdy + dtdx * dxdy_min);
				dtdl_max = dtdl_min + dtdx;
			}

			if (interpSTZ) {
				sz1 = l1->sz;
				dszdl_min = (dszdy + dszdx * dxdy_min);
				dszdl_max = dszdl_min + dszdx;

				tz1 = l1->tz;
				dtzdl_min = (dtzdy + dtzdx * dxdy_min);
				dtzdl_max = dtzdl_min + dtzdx;
			}
		}

		// compute values for the right edge

		if (update_right) {
			dx2 = (pr2->x - pr1->x);
			dy2 = (pr2->y - pr1->y);
			if (dy2 > 0)
				dx2dy2 = (dx2 << 16) / dy2;
			else
				dx2dy2 = 0;
			x2 = pr1->x << 16;
		}

		// we draw all the scan line of the part
		while (nb_lines > 0) {
			nb_lines--;
			{
				switch (drawLogic) {
				case DRAW_DEPTH_ONLY: {
					register PIXEL *pp;
					register int n;
					register unsigned int *pz;
					register unsigned int z;
					register unsigned int or1, og1, ob1;
					register unsigned int s, t;
					float sz, tz;
					n = (x2 >> 16) - x1;
					pp = (PIXEL *)((char *)pp1 + x1 * PSZB);
					if (interpZ) {
						pz = pz1 + x1;
						z = z1;
					}
					if (interpRGB) {
						or1 = r1;
						og1 = g1;
						ob1 = b1;
					}
					if (interpST) {
						s = s1;
						t = t1;
					}
					if (interpSTZ) {
						sz = sz1;
						tz = tz1;
					}
					while (n >= 3) {
						if (drawLogic == DRAW_DEPTH_ONLY) {
							putPixelDepth(pz,0,z,dzdx);
							putPixelDepth(pz,1,z,dzdx);
							putPixelDepth(pz,2,z,dzdx);
							putPixelDepth(pz,3,z,dzdx);
						}
						if (drawLogic == DRAW_FLAT) {
							putPixelFlat(pp,pz,0,z,color,dzdx);
							putPixelFlat(pp,pz,1,z,color,dzdx);
							putPixelFlat(pp,pz,2,z,color,dzdx);
							putPixelFlat(pp,pz,3,z,color,dzdx);
						}
						if (DRAW_MAPPING) {
							putPixelMapping(pp,pz,texture,0,z,t,s,dzdx,dsdx,dtdx);
							putPixelMapping(pp,pz,texture,1,z,t,s,dzdx,dsdx,dtdx);
							putPixelMapping(pp,pz,texture,2,z,t,s,dzdx,dsdx,dtdx);
							putPixelMapping(pp,pz,texture,3,z,t,s,dzdx,dsdx,dtdx);
						}
						if (interpZ) {
							pz += 4;
						}
						pp = (PIXEL *)((char *)pp + 4 * PSZB);
						n -= 4;
					}
					while (n >= 0) {
						if (drawLogic == DRAW_DEPTH_ONLY) {
							putPixelDepth(pz,0,z,dzdx);
						}
						if (drawLogic == DRAW_FLAT) {
							putPixelFlat(pp,pz,0,z,color,dzdx);
						}
						if (DRAW_MAPPING) {
							putPixelMapping(pp,pz,texture,0,z,t,s,dzdx,dsdx,dtdx);
						}
						if (interpZ) {
							pz += 1;
						}
						pp = (PIXEL *)((char *)pp + PSZB);
						n -= 1;
					}
				}
				break;
				case DRAW_SHADOW_MASK: {
					register unsigned char *pm;
					register int n;

					n = (x2 >> 16) - x1;
					pm = pm1 + x1;
					while (n >= 3) {
						for (int a = 0; a <= 3; a++) {
							pm[a] = 0xff;
						}
						pm += 4;
						n -= 4;
					}
					while (n >= 0) {
						pm[0] = 0xff;
						pm += 1;
						n -= 1;
					}
				}
				break;
				case DRAW_SHADOW: {
					register unsigned char *pm;
					register int n;
					register unsigned int *pz;
					register unsigned int z;

					n = (x2 >> 16) - x1;

					Graphics::PixelBuffer buf = pbuf;
					buf = pp1 + x1 * PSZB;

					pm = pm1 + x1;
					pz = pz1 + x1;
					z = z1;
					while (n >= 3) {
						for (int a = 0; a < 4; a++) {
							if (ZCMP(z, pz[a]) && pm[0]) {
								buf.setPixelAt(a, color);
								pz[a] = z;
							}
							z += dzdx;
						}
						pz += 4;
						pm += 4;
						buf.shiftBy(4);
						n -= 4;
					}
					while (n >= 0) {
						if (ZCMP(z, pz[0]) && pm[0]) {
							buf.setPixelAt(0, color);
							pz[0] = z;
						}
						pz += 1;
						pm += 1;
						buf.shiftBy(1);
						n -= 1;
					}
				}
				break;
				case DRAW_SMOOTH: {
					register unsigned int *pz;
					Graphics::PixelBuffer buf = pbuf;
					register unsigned int z, rgb, drgbdx;
					register int n;
					n = (x2 >> 16) - x1;
					int bpp = buf.getFormat().bytesPerPixel;
					buf = (byte *)pp1 + x1 * bpp;
					pz = pz1 + x1;
					z = z1;
					rgb =(r1 << 16) & 0xFFC00000;
					rgb |= (g1 >> 5) & 0x000007FF;
					rgb |= (b1 << 5) & 0x001FF000;
					drgbdx = _drgbdx;
					while (n >= 3) {
						putPixelSmooth(buf, pz, 0, z, tmp, rgb, dzdx, drgbdx);
						putPixelSmooth(buf, pz, 1, z, tmp, rgb, dzdx, drgbdx);
						putPixelSmooth(buf, pz, 2, z, tmp, rgb, dzdx, drgbdx);
						putPixelSmooth(buf, pz, 3, z, tmp, rgb, dzdx, drgbdx);
						pz += 4;
						buf.shiftBy(4);
						n -= 4;
					}
					while (n >= 0) {
						putPixelSmooth(buf, pz, 0, z, tmp, rgb, dzdx, drgbdx);
						buf.shiftBy(1);
						pz += 1;
						n -= 1;
					}
				}
				break;
				case DRAW_MAPPING_PERSPECTIVE: {
					register unsigned int *pz;
					register unsigned int s, t, z, rgb, drgbdx;
					register int n, dsdx, dtdx;
					float sz, tz, fz, zinv;
					n = (x2 >> 16) - x1;
					fz = (float)z1;
					zinv = (float)(1.0 / fz);

					Graphics::PixelBuffer buf = pbuf;
					buf = pp1 + x1 * PSZB;

					pz = pz1 + x1;
					z = z1;
					sz = sz1;
					tz = tz1;
					rgb = (r1 << 16) & 0xFFC00000;
					rgb |= (g1 >> 5) & 0x000007FF;
					rgb |= (b1 << 5) & 0x001FF000;
					drgbdx = _drgbdx;
					while (n >= (NB_INTERP - 1)) {
						{
							float ss, tt;
							ss = sz * zinv;
							tt = tz * zinv;
							s = (int)ss;
							t = (int)tt;
							dsdx = (int)((dszdx - ss * fdzdx) * zinv);
							dtdx = (int)((dtzdx - tt * fdzdx) * zinv);
							fz += fndzdx;
							zinv = (float)(1.0 / fz);
						}
						for (int _a = 0; _a < 8; _a++) {
							putPixelMappingPerspective(buf, textureFormat, texture, pz, _a, z, t, s, tmp, rgb, dzdx, dsdx, dtdx, drgbdx);
						}
						pz += NB_INTERP;
						buf.shiftBy(NB_INTERP);
						n -= NB_INTERP;
						sz += ndszdx;
						tz += ndtzdx;
					}

					{
						float ss, tt;
						ss = sz * zinv;
						tt = tz * zinv;
						s = (int)ss;
						t = (int)tt;
						dsdx = (int)((dszdx - ss * fdzdx) * zinv);
						dtdx = (int)((dtzdx - tt * fdzdx) * zinv);
					}

					int bytePerPixel = texture.getFormat().bytesPerPixel;

					while (n >= 0) {
						putPixelMappingPerspective(buf, textureFormat, texture, pz, 0, z, t, s, tmp, rgb, dzdx, dsdx, dtdx, drgbdx);
						pz += 1;
						buf.shiftBy(1);
						n -= 1;
					}
				}
				break;
				default:
					break;
				}
			}

			// left edge
			error += derror;
			if (error > 0) {
				error -= 0x10000;
				x1 += dxdy_max;
				if (interpZ) {
					z1 += dzdl_max;
				}

				if (interpRGB) {
					r1 += drdl_max;
					g1 += dgdl_max;
					b1 += dbdl_max;
				}

				if (interpST) {
					s1 += dsdl_max;
					t1 += dtdl_max;
				}

				if (interpSTZ) {
					sz1 += dszdl_max;
					tz1 += dtzdl_max;
				}
			} else {
				x1 += dxdy_min;
				if (interpZ) {
					z1 += dzdl_min;
				}
				if (interpRGB) {
					r1 += drdl_min;
					g1 += dgdl_min;
					b1 += dbdl_min;
				}
				if (interpST) {
					s1 += dsdl_min;
					t1 += dtdl_min;
				}
				if (interpSTZ) {
					sz1 += dszdl_min;
					tz1 += dtzdl_min;
				}
			}

			// right edge
			x2 += dx2dy2;

			// screen coordinates
			pp1 += linesize;
			pz1 += xsize;

			if (drawLogic == DRAW_SHADOW || drawLogic == DRAW_SHADOW_MASK)
				pm1 = pm1 + xsize;
		}
	}
}

void FrameBuffer::fillTriangleDepthOnly(ZBufferPoint *p0, ZBufferPoint *p1, ZBufferPoint *p2) {
	const bool interpZ = true;
	const bool interpRGB = false;
	const bool interpST = false;
	const bool interpSTZ = false;
	fillTriangle<interpRGB,interpZ,interpST,interpSTZ,DRAW_DEPTH_ONLY>(p0, p1, p2);
}


void FrameBuffer::fillTriangleFlat(ZBufferPoint *p0, ZBufferPoint *p1, ZBufferPoint *p2) {
	const bool interpZ = true;
	const bool interpRGB = false;
	const bool interpST = false;
	const bool interpSTZ = false;
	fillTriangle<interpRGB,interpZ,interpST,interpSTZ,DRAW_FLAT>(p0, p1, p2);
}

// Smooth filled triangle.
// The code below is very tricky :) -- Not anymore :P
void FrameBuffer::fillTriangleSmooth(ZBufferPoint *p0, ZBufferPoint *p1, ZBufferPoint *p2) {
	const bool interpZ = true;
	const bool interpRGB = true;
	const bool interpST = false;
	const bool interpSTZ = false;
	fillTriangle<interpRGB,interpZ,interpST,interpSTZ,DRAW_SMOOTH>(p0, p1, p2);
}

void FrameBuffer::fillTriangleMapping(ZBufferPoint *p0, ZBufferPoint *p1, ZBufferPoint *p2) {
	const bool interpZ = true;
	const bool interpRGB = false;
	const bool interpST = true;
	const bool interpSTZ = false;
	fillTriangle<interpRGB,interpZ,interpST,interpSTZ,DRAW_MAPPING>(p0, p1, p2);
}

void FrameBuffer::fillTriangleMappingPerspective(ZBufferPoint *p0, ZBufferPoint *p1, ZBufferPoint *p2) {
	const bool interpZ = true;
	const bool interpRGB = true;
	const bool interpST = false;
	const bool interpSTZ = true;
	fillTriangle<interpRGB,interpZ,interpST,interpSTZ,DRAW_MAPPING_PERSPECTIVE>(p0, p1, p2);
}

void FrameBuffer::fillTriangleFlatShadowMask(ZBufferPoint *p0, ZBufferPoint *p1, ZBufferPoint *p2) {
	const bool interpZ = true;
	const bool interpRGB = false;
	const bool interpST = false;
	const bool interpSTZ = false;
	fillTriangle<interpRGB,interpZ,interpST,interpSTZ,DRAW_SHADOW_MASK>(p0, p1, p2);
}

void FrameBuffer::fillTriangleFlatShadow(ZBufferPoint *p0, ZBufferPoint *p1, ZBufferPoint *p2) {
	const bool interpZ = true;
	const bool interpRGB = false;
	const bool interpST = false;
	const bool interpSTZ = false;
	fillTriangle<interpRGB,interpZ,interpST,interpSTZ,DRAW_SHADOW>(p0, p1, p2);
}


} // end of namespace TinyGL
