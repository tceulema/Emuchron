//*****************************************************************************
// Filename : 'ks0108conf.h'
// Title    : Graphic lcd configuration for hd61202/ks0108 displays
//*****************************************************************************

#ifndef KS0108CONF_H
#define KS0108CONF_H

// Lcd geometry defs
#define GLCD_XPIXELS			128	// Pixel width of entire display
#define GLCD_YPIXELS			64	// Pixel height of entire display

// Lcd defs for a single display controller
#define GLCD_CONTROLLER_XPIXELS		64	// Pixel width
#define GLCD_CONTROLLER_YPIXELS		64	// Pixel height
#define GLCD_CONTROLLER_XPIXMASK	0x3f	// Mask used for x pixels
#define GLCD_CONTROLLER_YPIXMASK	0x3f	// Mask used for y pixels
#define GLCD_CONTROLLER_XPIXBITS	6	// Bits used for x pixels
#define GLCD_CONTROLLER_YPAGES		8	// Pixel height in pages
#define GLCD_CONTROLLER_YPAGEBITS	3	// Bits used for y pixel page
#define GLCD_CONTROLLER_YPAGEMASK	0x07	// Mask used for y pixel page

// Determine the number of controllers (make sure we round up for partial use of
// more than one controller)
#define GLCD_NUM_CONTROLLERS \
  ((GLCD_XPIXELS + GLCD_CONTROLLER_XPIXELS - 1) / GLCD_CONTROLLER_XPIXELS)

// Display text defs when using glcdPutStr()
#define GLCD_TEXT_LINES			8	// Visible lines
#define GLCD_TEXT_LINE_LENGTH		22	// Internal line text length
#endif
