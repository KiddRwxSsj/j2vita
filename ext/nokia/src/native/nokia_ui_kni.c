#include <kni.h>
#include <sni.h>
#include <commonKNIMacros.h>
#include <midpError.h>
#include <gxapi_graphics.h>
#include <imgapi_image.h>
#include <gxj_putpixel.h>
#include <stdlib.h>

/*
 * These putpixel conversion helpers are implemented in gxj_graphics.c but
 * are not exposed by the legacy MIDP headers.  Provide their exact
 * prototypes here so modern GCC/C23 does not treat the calls as implicit.
 */
void gx_draw_pixels_4444_to_565(const java_imagedata *imgDest,
        int nXOriginDest, int nYOriginDest, jshort *dataSrc,
        int nScanLen, int nXOriginSrc, int nYOriginSrc,
        int nWidth, int nHeight, int transform, jboolean processAlpha);

void gx_draw_pixels_8888_to_565(const java_imagedata *imgDest,
        int nXOriginDest, int nYOriginDest, jint *dataSrc,
        int nScanLen, int nXOriginSrc, int nYOriginSrc,
        int nWidth, int nHeight, int transform, jboolean processAlpha);




/////////////////////////////////////////////////////////////////////////////
// below lines are copied from gxj_graphics.c
/**
 * Get a C structure representing the given <tt>ImageData</tt> class.
 */
#define GET_IMAGEDATA_PTR_FROM_GRAPHICS(handle) \
  GXAPI_GET_GRAPHICS_PTR(handle)->img != NULL ? \
  GXAPI_GET_GRAPHICS_PTR(handle)->img->imageData : \
  (java_imagedata*)NULL

/**
 * Gets the clipping region of the given graphics object.
 *
 * @param G handle to the <tt>Graphics</tt> object
 * @param ARRAY native <tt>jshort</tt> array to save the clip data
 */
#define GET_CLIP(G, ARRAY) \
    ARRAY[0] = GXAPI_GET_GRAPHICS_PTR(G)->clipX1, \
    ARRAY[1] = GXAPI_GET_GRAPHICS_PTR(G)->clipY1, \
    ARRAY[2] = GXAPI_GET_GRAPHICS_PTR(G)->clipX2, \
    ARRAY[3] = GXAPI_GET_GRAPHICS_PTR(G)->clipY2


/**
 * Translate the pixel location according to the translation of
 * the given graphics object.
 *
 * @param G handle to the <tt>Graphics</tt> object
 * @param X variable representing the <tt>x</tt> coordinate to be translated;
 *        this macro sets the value of X
 * @param Y variable representing the <tt>y</tt> coordinate to be translated;
 *        this macro sets the value of Y
 */
#define TRANSLATE(G, X, Y)  \
    (X) += GXAPI_GET_GRAPHICS_PTR((G))->transX, \
    (Y) += GXAPI_GET_GRAPHICS_PTR((G))->transY
/* -------------------------------------------------------------------------
 * PS Vita polygon rasterizer for Nokia DirectGraphics.
 * Bounce Tales relies heavily on fillPolygon(), which was a no-op in the
 * original PSPKVM extension.  Keep this implementation generic and clipped.
 * ------------------------------------------------------------------------- */
#ifdef __vita__
static gxj_screen_buffer *vita_nokia_get_screen(jobject graphics,
                                                 gxj_screen_buffer *tmp) {
    java_imagedata *img = GET_IMAGEDATA_PTR_FROM_GRAPHICS(graphics);
    if (img == NULL) {
        return &gxj_system_screen_buffer;
    }
    return gxj_get_image_screen_buffer_impl(img, tmp, NULL);
}

static gxj_pixel_type vita_nokia_blend_pixel(gxj_pixel_type dst,
                                              jint argbColor) {
    int a = (argbColor >> 24) & 0xff;
    int sr = (argbColor >> 16) & 0xff;
    int sg = (argbColor >> 8) & 0xff;
    int sb = argbColor & 0xff;
    int dr, dg, db, rr, rg, rb;

    if (a <= 0) return dst;
    if (a >= 255) return GXJ_RGB24TORGB16(argbColor & 0x00ffffff);

    dr = GXJ_GET_RED_FROM_PIXEL(dst);
    dg = GXJ_GET_GREEN_FROM_PIXEL(dst);
    db = GXJ_GET_BLUE_FROM_PIXEL(dst);

    rr = (sr * a + dr * (255 - a) + 127) / 255;
    rg = (sg * a + dg * (255 - a) + 127) / 255;
    rb = (sb * a + db * (255 - a) + 127) / 255;

    return GXJ_RGB24TORGB16((rr << 16) | (rg << 8) | rb);
}

static void vita_nokia_span(gxj_screen_buffer *sbuf, const jshort *clip,
                            int y, int x1, int x2, jint argbColor) {
    int x;
    gxj_pixel_type *dst;
    int a = (argbColor >> 24) & 0xff;
    gxj_pixel_type solid;

    if (a == 0 || y < clip[1] || y >= clip[3] ||
        y < 0 || y >= sbuf->height) return;
    if (x1 > x2) { int t = x1; x1 = x2; x2 = t; }
    if (x1 < clip[0]) x1 = clip[0];
    if (x2 >= clip[2]) x2 = clip[2] - 1;
    if (x1 < 0) x1 = 0;
    if (x2 >= sbuf->width) x2 = sbuf->width - 1;
    if (x1 > x2) return;

    dst = sbuf->pixelData + y * sbuf->width + x1;
    if (a >= 255) {
        solid = GXJ_RGB24TORGB16(argbColor & 0x00ffffff);
        for (x = x1; x <= x2; ++x) *dst++ = solid;
    } else {
        for (x = x1; x <= x2; ++x, ++dst) {
            *dst = vita_nokia_blend_pixel(*dst, argbColor);
        }
    }
}

static void vita_nokia_fill_polygon(gxj_screen_buffer *sbuf, const jshort *clip,
                                    const jint *xp, const jint *yp,
                                    int n, int tx, int ty, jint argbColor) {
    int minY, maxY, y, i, j;
    int *nodes;

    if (sbuf == NULL || sbuf->pixelData == NULL || n < 3) return;
    if (((argbColor >> 24) & 0xff) == 0) return;

    minY = yp[0] + ty;
    maxY = minY;
    for (i = 1; i < n; ++i) {
        int vy = yp[i] + ty;
        if (vy < minY) minY = vy;
        if (vy > maxY) maxY = vy;
    }
    if (minY < clip[1]) minY = clip[1];
    if (maxY >= clip[3]) maxY = clip[3] - 1;
    if (minY < 0) minY = 0;
    if (maxY >= sbuf->height) maxY = sbuf->height - 1;
    if (minY > maxY) return;

    nodes = (int *)malloc((size_t)n * sizeof(int));
    if (nodes == NULL) return;

    for (y = minY; y <= maxY; ++y) {
        int count = 0;
        j = n - 1;
        for (i = 0; i < n; ++i) {
            int xi = xp[i] + tx;
            int yi = yp[i] + ty;
            int xj = xp[j] + tx;
            int yj = yp[j] + ty;

            /* Half-open edge rule prevents duplicate intersections at vertices. */
            if ((yi <= y && yj > y) || (yj <= y && yi > y)) {
                long long num = (long long)(y - yi) * (long long)(xj - xi);
                nodes[count++] = xi + (int)(num / (long long)(yj - yi));
            }
            j = i;
        }

        /* n is small in Nokia games; insertion sort is cheaper than qsort setup. */
        for (i = 1; i < count; ++i) {
            int key = nodes[i];
            int k = i - 1;
            while (k >= 0 && nodes[k] > key) {
                nodes[k + 1] = nodes[k];
                --k;
            }
            nodes[k + 1] = key;
        }

        for (i = 0; i + 1 < count; i += 2) {
            vita_nokia_span(sbuf, clip, y, nodes[i], nodes[i + 1], argbColor);
        }
    }

    free(nodes);
}

static void vita_nokia_draw_line_alpha(gxj_screen_buffer *sbuf,
                                        const jshort *clip,
                                        int x0, int y0, int x1, int y1,
                                        jint argbColor) {
    int dx = (x1 > x0) ? (x1 - x0) : (x0 - x1);
    int sx = (x0 < x1) ? 1 : -1;
    int dy = -((y1 > y0) ? (y1 - y0) : (y0 - y1));
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx + dy;

    for (;;) {
        if (x0 >= clip[0] && x0 < clip[2] && y0 >= clip[1] && y0 < clip[3] &&
            x0 >= 0 && x0 < sbuf->width && y0 >= 0 && y0 < sbuf->height) {
            gxj_pixel_type *p = sbuf->pixelData + y0 * sbuf->width + x0;
            *p = vita_nokia_blend_pixel(*p, argbColor);
        }
        if (x0 == x1 && y0 == y1) break;
        {
            int e2 = err << 1;
            if (e2 >= dy) { err += dy; x0 += sx; }
            if (e2 <= dx) { err += dx; y0 += sy; }
        }
    }
}
#endif

/////////////////////////////////////////////////////////////////////////////

#define NO_IMAGE       0
#define COMPLETE_IMAGE 1
#define PARTIAL_IMAGE  2

/* reference point locations, from Graphics.java */
#define HCENTER   1
#define VCENTER   2
#define LEFT      4
#define RIGHT     8
#define TOP      16
#define BOTTOM   32
#define BASELINE 64

#define TRANSFORM_INVERTED_AXES 0x4
#define TRANSFORM_X_FLIP 0x2
#define TRANSFORM_Y_FLIP 0x1
/**
 * Accepts image information and adjusts the values so that only those
 * parts of the image that are visible in the clip will be drawn
 *
 * returns NO_IMAGE       if the image is completely outside of the clip
 * returns COMPLETE_IMAGE if the image is completely inside of the clip
 * returns PARTIAL_IMAGE  if the image is partially inside of the clip
 */

jbyte
adjustToViewableRegion(jint *x_dest, jint *y_dest, jint *x_src, jint *y_src,
                       jint *width, jint *height,
                       jshort *clip, jint anchor, jint transform) {
 
    int ret = NO_IMAGE;

    /*
     * use these temportary variables so we aren't deferencing all the time
     */
    jint t_x_dest = *x_dest, 
         t_y_dest = *y_dest,
         t_x_src,  t_y_src,
         t_width,  t_height;

    /* original values */
    jint ox_src, oy_src;
    jint ox_dest, oy_dest;
    jint oWidth, oHeight;

    jshort cx = clip[0],
           cy = clip[1],
           cw = clip[2],
           ch = clip[3];

    if (transform & TRANSFORM_INVERTED_AXES) {
        t_width  = *height;
        t_height = *width;
        t_x_src  = *y_src;
        t_y_src  = *x_src;
        ox_src   = *y_src;
        oy_src   = *x_src;
        oWidth  = *height;
        oHeight  = *width;
    } else {
        t_width  = *width;
        t_height = *height;
        t_x_src  = *x_src;
        t_y_src  = *y_src;
        ox_src   = *x_src;
        oy_src   = *y_src;
        oWidth  = *width;
        oHeight  = *height;
    }

    ox_dest = *x_dest;
    oy_dest = *y_dest;

    switch (anchor & (LEFT | RIGHT | HCENTER)) {
        default:
        case LEFT:
            break;

        case RIGHT:
            t_x_dest -= t_width;
            break;

        case HCENTER:
            t_x_dest -= t_width >> 1;
            break;
    }

    switch (anchor & (TOP | BOTTOM | VCENTER)) {
        default:
        case TOP:
            break;

        case BOTTOM:
            t_y_dest -= t_height;
            break;

        case VCENTER:
            t_y_dest -= t_height >> 1;
            break;
    }

    /* image will be past the left/right side of the clip */
    if (((t_x_dest + t_width) < cx) || ((t_x_dest >= (cx + cw)))) {
        /* this will leave the parameters unchanged; that is ok. */
        return ret;
    }

    /* image will be past the top/bottom side of the clip */
    if (((t_y_dest + t_height) < cy) || ((t_y_dest >= (cy + ch)))) {
        /* this will leave the parameters unchanged; that is ok. */
        return ret;
    }

    ret = COMPLETE_IMAGE;

    /* left side of image is past the left side of clip */
    if (t_x_dest < cx) {
        t_x_src += cx - t_x_dest;
        t_width -= cx - t_x_dest;

        t_x_dest = cx;
        ret = PARTIAL_IMAGE;
    }

    /* top of image is past the top of clip */
    if (t_y_dest < cy) {
        t_y_src  += cy - t_y_dest;
        t_height -= cy - t_y_dest;

        t_y_dest = cy;
        ret = PARTIAL_IMAGE;
    }

    /* right side of image is past the right side of clip */
    if ((t_x_dest + t_width) > (cx + cw)) {
        t_width = (cx + cw) - t_x_dest;
        ret = PARTIAL_IMAGE;
    }

    /* bottom of image is past the bottom of clip */
    if ((t_y_dest + t_height) > (cy + ch)) {
        t_height = (cy + ch) - t_y_dest;
        ret = PARTIAL_IMAGE;
    }


    /* check to see if we have dimensions that do not 
     * represent an image.
     * if width == 0 or height == 0,
     * return NO_IMAGE.
     * x, y, width, height values are invalid and
     * should not be used.
     */
    if (t_width == 0 || t_height == 0) {
      return NO_IMAGE;
    }
  
    /* these conditions will adjust the visible rectangle to cover
     * the part of the image that will actually be drawn if
     * a transform has been applied
     *
     * if the transform including inverting the axes and flipping one of the 
     * axes then we need to reverse (not undo) the visible rectangle, so that
     * whatever was adjusted one way needs to be adjusted the other.
     * this is because there is an implicit axes flip when the axes
     * are inverted.
     */

#define ADJUST_X \
t_x_src = ox_src + ((ox_src + oWidth) - (t_x_src + t_width))

#define ADJUST_Y \
t_y_src = oy_src + ((oy_src + oHeight) - (t_y_src + t_height))

    if (transform & TRANSFORM_X_FLIP) {
        ADJUST_X;
    }

    if (transform & TRANSFORM_Y_FLIP) {
        ADJUST_Y;
    }

    if ( (transform == (TRANSFORM_INVERTED_AXES | TRANSFORM_X_FLIP))
        || (transform == (TRANSFORM_INVERTED_AXES | TRANSFORM_Y_FLIP))) {

            ADJUST_X;
            ADJUST_Y;
    } 

#undef ADJUST_X
#undef ADJUST_Y

    if (transform & TRANSFORM_INVERTED_AXES) {
        *width  = t_height;
        *height = t_width;
        *x_src  = t_y_src;
        *y_src  = t_x_src;
    } else {
        *width  = t_width;
        *height = t_height;
        *x_src  = t_x_src;
        *y_src  = t_y_src;
    }

    *x_dest = t_x_dest;
    *y_dest = t_y_dest;

#if 0
    printf("sxy(%ld %ld) osxy(%i %i) wh(%ld %ld) owh(%i %i) "
           "dxy(%ld %ld) odxy(%i %i) t(%ld)\n", 
           *x_src, *y_src, ox_src, oy_src, *width, *height, 
           oWidth, oHeight, *x_dest, *y_dest, ox_dest, oy_dest, transform);
#endif

    return ret;
}

/////////////////////////////////////////////////////////////////////////////

#ifndef NULL
#define NULL 0
#endif

#define FLIP_HORIZONTAL       0x2000
#define FLIP_VERTICAL         0x4000
    
#define ROTATE_90             90
#define ROTATE_180            180
#define ROTATE_270            270
static int MIDP2Manip2NokiaManip[8][2] = 
{ 
	{ 0,                        FLIP_VERTICAL|FLIP_HORIZONTAL|ROTATE_180 },         //Sprite.TRANS_NONE
	{ FLIP_VERTICAL,            FLIP_HORIZONTAL|ROTATE_180 },                       //Sprite.TRANS_MIRROR_ROT180
	{ FLIP_HORIZONTAL,          FLIP_VERTICAL|ROTATE_180 },                         //Sprite.TRANS_MIRROR
	{ ROTATE_180,               FLIP_VERTICAL|FLIP_HORIZONTAL },                    //Sprite.TRANS_ROT180
	{ FLIP_VERTICAL|ROTATE_90,  FLIP_HORIZONTAL|ROTATE_270 },                       //Sprite.TRANS_MIRROR_ROT270
	{ ROTATE_90,                FLIP_VERTICAL|FLIP_HORIZONTAL|ROTATE_270 },         //Sprite.TRANS_ROT90
	{ ROTATE_270,               FLIP_VERTICAL|FLIP_HORIZONTAL|ROTATE_90 },          //Sprite.TRANS_ROT270
	{ FLIP_HORIZONTAL|ROTATE_90,FLIP_VERTICAL|ROTATE_270 }                          //Sprite.TRANS_MIRROR_ROT90
};

int nokiaManip2MIDP2Manip(int manip)
{
	int i, j;
	for(i = 0; i < 8; i++)
	{
		for(j = 0; j < 2; j++)
			if(MIDP2Manip2NokiaManip[i][j] == manip)
				return i;

	}
	return 0;
}

#define TYPE_BYTE_1_GRAY              1
#define TYPE_BYTE_1_GRAY_VERTICAL     (-1)
#define TYPE_BYTE_2_GRAY              2
#define TYPE_BYTE_4_GRAY              4
#define TYPE_BYTE_8_GRAY              8
#define TYPE_BYTE_332_RGB             332
#define TYPE_USHORT_4444_ARGB         4444
#define TYPE_USHORT_444_RGB           444
#define TYPE_USHORT_555_RGB           555
#define TYPE_USHORT_1555_ARGB         1555
#define TYPE_USHORT_565_RGB           565
#define TYPE_INT_888_RGB              888
#define TYPE_INT_8888_ARGB            8888

/**
 * Draws the specified pixels from the given data array. The array
 * consists of values in the form of 0xAARRGGBB.
 * <p>
 * Java declaration:
 * <pre>
 *     drawRGB([IIIIIIIZ)V
 * </pre>
 *
 * @param rgbData The array of argb pixels to draw
 * @param offset Zero-based index of first argb pixel to be drawn
 * @param scanlen Number of intervening pixels between pixels in
 *                the same column but in adjacent rows
 * @param x The x coordinate of the upper left corner of the
 *          region to draw
 * @param y The y coordinate of the upper left corner of the
 *          region to draw
 * @param width The width of the target region
 * @param height The height of the target region
 * @param processAlpha If <tt>true</tt>, alpha channel bytes will
 *                     be used, otherwise, alpha channel bytes will
 *                     be ignored
 */

//KNIEXPORT KNI_RETURNTYPE_VOID
//KNIDECL(javax_microedition_lcdui_Graphics_drawRGB) {
//    jboolean processAlpha = KNI_GetParameterAsBoolean(8);
//    jint height = KNI_GetParameterAsInt(7);
//    jint width = KNI_GetParameterAsInt(6);
//    jint y = KNI_GetParameterAsInt(5);
//    jint x = KNI_GetParameterAsInt(4);
//    jint scanlen = KNI_GetParameterAsInt(3);
//    jint offset = KNI_GetParameterAsInt(2);
//    jint buflen;
//    jint *rgbBuffer;
//    long min, max, l_scanlen, l_height, l_tmpexp;
//    
//    KNI_StartHandles(2);
//    KNI_DeclareHandle(rgbData);
//    KNI_DeclareHandle(thisObject);
//
//    KNI_GetParameterAsObject(1, rgbData);
//    KNI_GetThisPointer(thisObject);
//
//    if (GRAPHICS_OP_IS_ALLOWED(thisObject)) {
//        if (KNI_IsNullHandle(rgbData)) {
//            KNI_ThrowNew(midpNullPointerException, NULL);
//        } else {
//	
//            buflen = KNI_GetArrayLength(rgbData);
//	
//            /* According to the spec., this function can be
//             * defined as operation P(a,b) = rgbData[ offset +
//             * (a-x) + (b-y)* scanlength] where x <= a < x + width
//             * AND y <= b < y + height.
//             *
//             * We do not need to check every index value and its
//             * corresponding array access violation. We only need
//             * to check for the min/max case. Detail explanation
//             * can be found in the design doc.
//             *
//             * - To translate "<" to "<=", we minus one from height
//             * and width (the ceiling operation), for all cases
//             * except when height or width is zero.
//             * - To avoid overflow (or underflow), we cast the
//             * variables scanlen and height to long first */
//
//            l_scanlen = (long) scanlen;
//            l_height  = (long) height - 1;
//            l_tmpexp  = (height == 0) ? 0 : l_height * l_scanlen ;
//	
//            /* Find the max/min of the index for rgbData array */
//            max = offset + ((width==0) ? 0 : (width-1)) 
//                + ((scanlen<0) ? 0 : l_tmpexp);
//            min = offset + ((scanlen<0) ? l_tmpexp : 0);
//	
//            if ((max >= buflen) || (min < 0) || (max < 0) || (min >= buflen)) {
//                KNI_ThrowNew(midpArrayIndexOutOfBoundsException, NULL);
//            } else {
//	  
//                if ((0 == scanlen || 
//                     0 == width   || 
//                     0 == height)) {
//	    
//                    /* Valid values, but nothing to render. */
//	    
//                } else {
//
//                    jshort clip[4]; /* Defined in Graphics.java as 4 shorts */
//            
//                    rgbBuffer = JavaIntArray(rgbData);
//            
//                    TRANSLATE(thisObject, x, y);
//
//                    GET_CLIP(thisObject, clip);
//            
//                    gx_draw_rgb(clip,
//                        GET_IMAGEDATA_PTR_FROM_GRAPHICS(thisObject), 
//                        rgbBuffer, offset, scanlen, x, y, width, 
//                        height, processAlpha);
//                }
//
//            }
//        }
//    }
//    
//    KNI_EndHandles();
//    KNI_ReturnVoid();
//}

/**
 * Draws the specified pixels from the given data array. The array
 * consists of values in the form of 0xAARRGGBB.
 * <p>
 * Java declaration:
 * <pre>
 *     drawPixels(3SZIIIIIIII)V
 * </pre>
 */
//drawPixels(byte pixels[], byte transparencyMask[], int offset, int scanlength, int x, int y, int width, 
//            int height, int manipulation, int format);
KNIEXPORT KNI_RETURNTYPE_VOID
KNIDECL(com_nokia_mid_ui_DirectGraphicsImpl_drawPixels___3B_3BIIIIIIII) {
	KNI_ReturnVoid();
}

//drawPixels(short pixels[], boolean transparency, int offset, int scanlength, int x, int y, int width, 
//            int height, int manipulation, int format);
KNIEXPORT KNI_RETURNTYPE_VOID
KNIDECL(com_nokia_mid_ui_DirectGraphicsImpl_drawPixels___3SZIIIIIIII) {
	jint format = KNI_GetParameterAsInt(10);
	jint manipulation = KNI_GetParameterAsInt(9);
    jint height = KNI_GetParameterAsInt(8);
    jint width = KNI_GetParameterAsInt(7);
    jint y = KNI_GetParameterAsInt(6);
    jint x = KNI_GetParameterAsInt(5);
    jint scanlength = KNI_GetParameterAsInt(4);
    jint offset = KNI_GetParameterAsInt(3);
    jboolean transparency = KNI_GetParameterAsBoolean(2);
    //jint buflen;
    //jint *rgbBuffer;
	int transform;

    KNI_StartHandles(4);
    KNI_DeclareHandle(pixels);
    KNI_DeclareHandle(thisObject);
    KNI_DeclareHandle(gHandle);
    //KNI_DeclareHandle(cHandle);
    //KNI_DeclareHandle(iHandle);
	KNI_DeclareHandle(graphics);

    KNI_GetParameterAsObject(1, pixels);
    KNI_GetParameterAsObject(0, thisObject);

	KNI_GetObjectClass(thisObject, gHandle);
	KNI_GetObjectField(thisObject, KNI_GetFieldID(gHandle, "graphics", "Ljavax/microedition/lcdui/Graphics;"), graphics);

    if (KNI_IsNullHandle(pixels)) {
        //KNI_ThrowNew("java/lang/NullPointerException", "");
    } else {
		if (y < 0) {
			offset += -y * scanlength;
			height += y;
			y = 0;
		}
		if (x < 0) {
			offset += -x;
			width += x;
			x = 0;
		}
		if ((height <= 0) || (width <= 0) || (scanlength <= 0)){

		} else if ((offset < 0) || ((offset + ((height-1) * scanlength)+width) > KNI_GetArrayLength(pixels))) {
			//KNI_ThrowNew("java/lang/ArrayIndexOutOfBoundsException", "");
		} else {
            jshort clip[4]; /* Defined in Graphics.java as 4 shorts */
    
            jshort * pixBuffer = JavaShortArray(pixels) + offset;
            
			jint x_src = 0,	y_src = 0;
			
            jbyte ret;
            
            TRANSLATE(graphics, x, y);

            GET_CLIP(graphics, clip);

			transform = nokiaManip2MIDP2Manip(manipulation);
			ret = adjustToViewableRegion(&x, &y, &x_src, &y_src,
				&width, &height, clip, 0, transform);
			
			if (ret != NO_IMAGE) {
				gx_draw_pixels_4444_to_565(GET_IMAGEDATA_PTR_FROM_GRAPHICS(graphics), x, y, 
	            		pixBuffer, scanlength, x_src, y_src,  
	            		width, height, transform, transparency);
			} 
        }
    }

    KNI_EndHandles();
    KNI_ReturnVoid();
}

//void drawPixels(int pixels[], boolean transparency, int offset, int scanlength, int x, int y, int width, 
//            int height, int manipulation, int format);
KNIEXPORT KNI_RETURNTYPE_VOID
KNIDECL(com_nokia_mid_ui_DirectGraphicsImpl_drawPixels___3IZIIIIIIII) {
	jint format = KNI_GetParameterAsInt(10);
	jint manipulation = KNI_GetParameterAsInt(9);
	jint height = KNI_GetParameterAsInt(8);
	jint width = KNI_GetParameterAsInt(7);
	jint y = KNI_GetParameterAsInt(6);
	jint x = KNI_GetParameterAsInt(5);
	jint scanlength = KNI_GetParameterAsInt(4);
	jint offset = KNI_GetParameterAsInt(3);
	//jint buflen;
	//jint *rgbBuffer;
	int transform;
	jboolean transparency = KNI_GetParameterAsBoolean(2);

	KNI_StartHandles(4);
	KNI_DeclareHandle(pixels);
	KNI_DeclareHandle(thisObject);
	KNI_DeclareHandle(gHandle);
	//KNI_DeclareHandle(cHandle);
	//KNI_DeclareHandle(iHandle);
	KNI_DeclareHandle(graphics);

	KNI_GetParameterAsObject(1, pixels);
	KNI_GetParameterAsObject(0, thisObject);

	KNI_GetObjectClass(thisObject, gHandle);
	KNI_GetObjectField(thisObject, KNI_GetFieldID(gHandle, "graphics", "Ljavax/microedition/lcdui/Graphics;"), graphics);

	if (KNI_IsNullHandle(pixels)) {
		//KNI_ThrowNew("java/lang/NullPointerException", "");
	} else {
		if (y < 0) {
			offset += -y * scanlength;
			height += y;
			y = 0;
		}
		if (x < 0) {
			offset += -x;
			width += x;
			x = 0;
		}
		
		if ((height < 0) || (width < 0) || (scanlength < 0)){

		} else if ((offset < 0) || ((offset + ((height-1) * scanlength)+width) > KNI_GetArrayLength(pixels))) {
			//KNI_ThrowNew("java/lang/ArrayIndexOutOfBoundsException", "");
		} else {
            jshort clip[4]; /* Defined in Graphics.java as 4 shorts */
    
            jint * pixBuffer = JavaIntArray(pixels) + offset;
            
			jint x_src = 0,	y_src = 0;
			
            jbyte ret;
            
            TRANSLATE(graphics, x, y);

            GET_CLIP(graphics, clip);

			transform = nokiaManip2MIDP2Manip(manipulation);
			ret = adjustToViewableRegion(&x, &y, &x_src, &y_src,
				&width, &height, clip, 0, transform);
			
			if (ret != NO_IMAGE) {
	            gx_draw_pixels_8888_to_565(GET_IMAGEDATA_PTR_FROM_GRAPHICS(graphics), x, y, 
	            		pixBuffer, scanlength, x_src, y_src,  
	            		width, height, transform, transparency);
			}        
		}
	}

	KNI_EndHandles();
	KNI_ReturnVoid();
}

//void drawPolygon(int xPoints[], int xOffset, int yPoints[], int yOffset, int nPoints, int argbColor);
KNIEXPORT KNI_RETURNTYPE_VOID
KNIDECL(com_nokia_mid_ui_DirectGraphicsImpl_drawPolygon) {
#ifdef __vita__
    jint xOffset = KNI_GetParameterAsInt(2);
    jint yOffset = KNI_GetParameterAsInt(4);
    jint nPoints = KNI_GetParameterAsInt(5);
    jint argbColor = KNI_GetParameterAsInt(6);

    KNI_StartHandles(5);
    KNI_DeclareHandle(xPoints);
    KNI_DeclareHandle(yPoints);
    KNI_DeclareHandle(thisObject);
    KNI_DeclareHandle(gHandle);
    KNI_DeclareHandle(graphics);
    KNI_GetParameterAsObject(1, xPoints);
    KNI_GetParameterAsObject(3, yPoints);
    KNI_GetParameterAsObject(0, thisObject);

    if (!KNI_IsNullHandle(xPoints) && !KNI_IsNullHandle(yPoints) && nPoints >= 2 &&
        xOffset >= 0 && yOffset >= 0 &&
        xOffset + nPoints <= KNI_GetArrayLength(xPoints) &&
        yOffset + nPoints <= KNI_GetArrayLength(yPoints)) {
        gxj_screen_buffer temp;
        gxj_screen_buffer *sbuf;
        jshort clip[4];
        jint *xs = JavaIntArray(xPoints) + xOffset;
        jint *ys = JavaIntArray(yPoints) + yOffset;
        int tx, ty, i;

        KNI_GetObjectClass(thisObject, gHandle);
        KNI_GetObjectField(thisObject,
            KNI_GetFieldID(gHandle, "graphics", "Ljavax/microedition/lcdui/Graphics;"),
            graphics);
        if (!KNI_IsNullHandle(graphics)) {
            GET_CLIP(graphics, clip);
            tx = GXAPI_GET_GRAPHICS_PTR(graphics)->transX;
            ty = GXAPI_GET_GRAPHICS_PTR(graphics)->transY;
            sbuf = vita_nokia_get_screen(graphics, &temp);
            if (sbuf != NULL) {
                for (i = 0; i < nPoints; ++i) {
                    int j = (i + 1 == nPoints) ? 0 : i + 1;
                    vita_nokia_draw_line_alpha(sbuf, clip,
                        xs[i] + tx, ys[i] + ty,
                        xs[j] + tx, ys[j] + ty, argbColor);
                }
            }
        }
    }
    KNI_EndHandles();
#endif
    KNI_ReturnVoid();
}

//void fillPolygon(int xPoints[], int xOffset, int yPoints[], int yOffset, int nPoints, int argbColor);
KNIEXPORT KNI_RETURNTYPE_VOID
KNIDECL(com_nokia_mid_ui_DirectGraphicsImpl_fillPolygon) {
#ifdef __vita__
    jint xOffset = KNI_GetParameterAsInt(2);
    jint yOffset = KNI_GetParameterAsInt(4);
    jint nPoints = KNI_GetParameterAsInt(5);
    jint argbColor = KNI_GetParameterAsInt(6);

    KNI_StartHandles(5);
    KNI_DeclareHandle(xPoints);
    KNI_DeclareHandle(yPoints);
    KNI_DeclareHandle(thisObject);
    KNI_DeclareHandle(gHandle);
    KNI_DeclareHandle(graphics);
    KNI_GetParameterAsObject(1, xPoints);
    KNI_GetParameterAsObject(3, yPoints);
    KNI_GetParameterAsObject(0, thisObject);

    if (!KNI_IsNullHandle(xPoints) && !KNI_IsNullHandle(yPoints) && nPoints >= 3 &&
        xOffset >= 0 && yOffset >= 0 &&
        xOffset + nPoints <= KNI_GetArrayLength(xPoints) &&
        yOffset + nPoints <= KNI_GetArrayLength(yPoints)) {
        gxj_screen_buffer temp;
        gxj_screen_buffer *sbuf;
        jshort clip[4];
        jint *xs = JavaIntArray(xPoints) + xOffset;
        jint *ys = JavaIntArray(yPoints) + yOffset;
        int tx, ty;

        KNI_GetObjectClass(thisObject, gHandle);
        KNI_GetObjectField(thisObject,
            KNI_GetFieldID(gHandle, "graphics", "Ljavax/microedition/lcdui/Graphics;"),
            graphics);
        if (!KNI_IsNullHandle(graphics)) {
            GET_CLIP(graphics, clip);
            tx = GXAPI_GET_GRAPHICS_PTR(graphics)->transX;
            ty = GXAPI_GET_GRAPHICS_PTR(graphics)->transY;
            sbuf = vita_nokia_get_screen(graphics, &temp);
            if (sbuf != NULL) {
                vita_nokia_fill_polygon(sbuf, clip, xs, ys, nPoints, tx, ty, argbColor);
            }
        }
    }
    KNI_EndHandles();
#endif
    KNI_ReturnVoid();
}

//void getPixels(int pixels[], int offset, int scanlength, 
//            int x, int y, int width, int height, int format);
KNIEXPORT KNI_RETURNTYPE_VOID
KNIDECL(com_nokia_mid_ui_DirectGraphicsImpl_getPixels___3IIIIIIII) {
	KNI_ReturnVoid();
}

//void getPixels(short pixels[], int offset, int scanlength, 
//            int x, int y, int width, int height, int format);
KNIEXPORT KNI_RETURNTYPE_VOID
KNIDECL(com_nokia_mid_ui_DirectGraphicsImpl_getPixels___3SIIIIIII) {
	KNI_ReturnVoid();
}

//void getPixels(byte[] pixels, byte[] transparencyMask, int offset, int scanlength, 
//            int x, int y, int width, int height, int format);
KNIEXPORT KNI_RETURNTYPE_VOID
KNIDECL(com_nokia_mid_ui_DirectGraphicsImpl_getPixels___3B_3BIIIIIII) {
	KNI_ReturnVoid();
}
