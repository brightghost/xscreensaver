/* -*- Mode: C; tab-width: 4 -*- */
/* matrix --- screensaver inspired by the 1999 sci-fi flick "The Matrix" */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)matrix.c	4.15 99/06/30 xlockmore";
#endif

/* Matrix-style screensaver
 *
 * Author: Erik O'Shaughnessy (eriko@xenolab.com)  20 Apr 1999
 * 
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appear in all copies and that
 * both that copyright notice and this permission notice appear in
 * supporting documentation.
 *
 * This file is provided AS IS with no warranties of any kind.  The author
 * shall have no liability with respect to the infringement of copyrights,
 * trade secrets or any patents by this file or any part thereof.  In no
 * event will the author be liable for any lost revenue or profits or
 * other special, indirect and consequential damages.
 *
 * Revision History:
 *
 * 23-Jul-1999: Unleashed on an unsuspecting world
 *
 * 29-Jun-1999: More hacking by Jeremy Buhler (jbuhler@cs.washington.edu)
 *              - fix memory-related bugs found by xlock maintainer
 *              - properly reinitialize screen with new size when init()
 *                is called multiple times
 *              - reinstate multiple final characters (suggested by 
 *                Joan Touzet)
 *              - reduce delay to 1 ms (provisionally)
 *              - decouple pixmap height from screen height, to permit
 *                less memory use and shorter, more frequent updates
 *                (for smoother scrolling on slow boxes, but nothing
 *                will help if the machine is busy.  Also restores
 *                broken display on very small windows).
 *              - nevertheless, certain display parameters such as the
 *                interval between column speed updates should depend on 
 *                the screen height -- enforce this at runtime.
 *              - fix column spacing to use entire window
 *              - remove redundant size fields from matrix_t and column_t
 *                structures -- rely on character size and window size
 *                instead.
 *              - consistently use PICK_MATRIX() instead of passing
 *                mp as a parameter whenever the active screen is
 *                implicitly required.
 *
 * 27-Jun-1999: Overhauled by Jeremy Buhler (jbuhler@cs.washington.edu)
 *              - use screen-to-screen copies as much as possible
 *              - get by with a single pixmap per column instead of two
 *              - try to optimize draw_matrix() and new_column() as much 
 *                as humanly possible
 *              - fixed double-freeing in release_matrix()
 *              - increased delay to 10 ms to prevent hyperfast scrolling
 *              - minor cleanups; removed unused or duplicate code/data
 *
 * 26-Jun-1999: Tweaks by Jeremy Buhler (jbuhler@cs.washington.edu)
 *              - Use larger Katakana font with numbers
 *              - tweak character colors
 *              - change string and gap distributions for lower screen density
 *              - Matrix strings end with a fixed character
 *              - fix scrolling artifacts due to gaps between two column
 *                pixmaps
 *              - minor code cleanups to allow future parameterization
 *                of speed, density
 *
 * 09-Jun-1999: Tweaks by Joan Touzet to look more like the original film's
 *              effects
 *
 * 22-Apr-1999: Initial version
 */

#include <X11/Intrinsic.h>

#ifdef STANDALONE
#define PROGCLASS "Matrix"
#define HACK_INIT init_matrix
#define HACK_DRAW draw_matrix
#define matrix_opts xlockmore_opts
#define DEFAULTS "*delay: 1000\n"
#include "xlockmore.h"		/* in xscreensaver distribution */
#else /* STANDALONE */
#include "xlock.h"		/* in xlockmore distribution */
#endif /* STANDALONE */

ModeSpecOpt matrix_opts = {0, NULL, 0, NULL, NULL};

#ifdef USE_MODULES
ModStruct   matrix_description =
{"matrix", "init_matrix", "draw_matrix", "release_matrix",
 "refresh_matrix", "change_matrix", NULL, &matrix_opts, 
 1000, 1, 1, 1, 64, 1.0, "", "Shows the Matrix", 0, NULL};
#endif


/*
 * state associated with a single column of the waterfall display
 */
typedef struct {
	int yPtr;		/* current offset relative to pixmap origin */
	Pixmap pixmap;
  
	int strLen;		/* remaining length of current string */
	int gapLen;		/* remaining length of current gap    */ 
	Bool endChar;		/* is the next char an ending char?   */

	int speed;		/* pixels downward per update */
	int nextSpeedUpdate;	/* time until next change in speed */
  
} column_t;


/*
 * Matrix display parameters associated with a particular X screen
 */
typedef struct {
	column_t *columns;      /* columns of the Matrix display */
	int num_cols;		/* number of columns of Matrix data
				 * (depends on screen width) */
 
	int charsPerPixmap;	/* height of column pixmaps in characters
				 * (depends on screen height) */
  
	int pixmapHeight;	/* height of column pixmaps in pixels
                                 * (depends on screen height) */
	 
	int speedUpdateInterval; /* number of characters or spaces that
				 * must be written to column pixmap before
				 * the column speed is changed
				 * (depends on screen height) */
  
	Pixmap kana[2];		/* pixmap containing katakana 
				 * [0] is fg on bg
				 * [1] is bold on bg */
  
	Pixel fg;		/* foreground */
	Pixel bg;		/* background */
	Pixel bold;		/* standout foreground color */
} matrix_t;


/* These Katakana font bitmaps were taken from the file 12x24rk.bdf 
 * distributed with XFree86 3.3.3.  The numbers were taken from the file 
 * 10x20.bdf, since the 12x24rk numbers were rather large compared to
 * the other characters.
 */
#define katakana_cell_width  12
#define katakana_cell_height 24
#define katakana_num_cells   65

#define katakana_width 780
#define katakana_height 24

static const unsigned char katakana_bits[] = {
 0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
 0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
 0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
 0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
 0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
 0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
 0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x0f,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
 0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
 0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xdf,0xff,0xff,0xff,
 0xff,0xfc,0xff,0xff,0xff,0xef,0xff,0xff,0x7f,0xff,0xff,0xff,0xff,0xff,0xff,
 0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xfe,0xff,
 0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x3f,0xff,0xff,0xff,0xff,0xff,
 0x7f,0xfe,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
 0x0f,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
 0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
 0xff,0xff,0xff,0xef,0x9f,0xff,0xff,0x9f,0xff,0xfd,0xef,0xff,0xf9,0xcf,0xff,
 0xff,0x7f,0xfe,0xff,0xff,0xff,0xff,0xff,0xff,0xfd,0xff,0xff,0xfe,0xfb,0xfc,
 0xfc,0x9f,0xff,0xff,0xff,0xff,0xfc,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
 0xfc,0xff,0x3f,0xf8,0xcf,0xff,0xe7,0xff,0xff,0xfe,0xff,0xff,0xff,0x03,0x3c,
 0xc7,0xdf,0xff,0xff,0xff,0xff,0xff,0xff,0x0f,0xff,0xff,0xff,0xff,0xff,0xff,
 0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xfd,0xff,0xff,0xff,0xff,
 0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xcf,0x9f,0xff,0xff,
 0xbf,0xff,0xf9,0xcf,0xff,0xf9,0xcf,0xff,0xff,0x73,0x3e,0xff,0x7b,0xfc,0xfe,
 0xfb,0xfc,0xfd,0xff,0xdc,0x9c,0x03,0xfc,0xfc,0x9f,0xff,0xff,0xff,0xff,0xf9,
 0x7f,0xfe,0xfb,0xff,0xff,0xdf,0xff,0xff,0xf9,0xff,0x7f,0xf0,0x9f,0xff,0xef,
 0xff,0xff,0xfe,0xff,0x9f,0x9f,0x07,0x7c,0xcf,0x9f,0xff,0xff,0xff,0x9f,0xdf,
 0xff,0x0f,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
 0xff,0xff,0x01,0xf8,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
 0xff,0xff,0x1f,0xf8,0xcf,0x9f,0xff,0xff,0x3f,0xff,0xf9,0xdf,0xff,0xf9,0xcf,
 0x9f,0xdf,0x67,0x7e,0xff,0x03,0xf8,0xfc,0xfb,0xf9,0xcd,0x0f,0xdc,0x9d,0x07,
 0xff,0xfc,0xbf,0xff,0xff,0x1b,0xfc,0xf9,0xff,0x7c,0xf2,0xf9,0xdf,0x80,0xff,
 0xff,0xf9,0xff,0xff,0xc3,0x9f,0xff,0xcf,0xff,0xfe,0xfe,0xff,0x3f,0x80,0xff,
 0x7f,0xcf,0x93,0xbf,0xff,0xff,0x1f,0x80,0xf9,0x0f,0x9f,0xff,0xf9,0x0f,0xff,
 0xf0,0xff,0x3e,0xc0,0x0f,0x3f,0xc0,0x0f,0xff,0xf0,0x01,0xf8,0xff,0xff,0xff,
 0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x05,0xf8,0xcf,0x9d,0xbd,
 0xc3,0x3f,0xff,0xf9,0xdf,0xff,0xdd,0xef,0x3f,0x80,0x67,0x7e,0xfe,0x87,0xfd,
 0xfc,0xf3,0xfd,0xc1,0x03,0x9f,0x9d,0xff,0xff,0xfc,0xbf,0xff,0xff,0x03,0xf8,
 0xef,0xff,0xfc,0xf4,0xf3,0x1f,0x80,0xcf,0xff,0xf9,0x3f,0xf8,0x8f,0x9f,0xff,
 0xcf,0x03,0xfc,0xfe,0xff,0x3f,0x80,0xff,0x7f,0xcf,0x87,0x3f,0xff,0xfb,0x3d,
 0x80,0xfb,0x0f,0x0f,0xff,0xf8,0x67,0x7e,0xe6,0x7f,0x3e,0xff,0x67,0xfe,0xcf,
 0x67,0x7e,0xe6,0xff,0xfc,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
 0xff,0xff,0xff,0xc1,0xf9,0xe7,0x01,0x38,0xc0,0x3f,0xff,0x81,0xdf,0xff,0x80,
 0xef,0x3f,0x80,0x67,0xfe,0xfe,0xff,0xfd,0xfc,0xf7,0xfd,0xc0,0xa1,0xbf,0x99,
 0xff,0xff,0xfc,0xbf,0xff,0xff,0xc7,0xfd,0xe1,0xff,0xfc,0xe4,0xfb,0x3f,0xcf,
 0xcf,0xdf,0x01,0x01,0xf0,0x9f,0xdf,0xff,0xcf,0x03,0xff,0x0c,0x7f,0xfe,0x9f,
 0xff,0x7f,0xcf,0x97,0x3f,0xff,0x03,0xbc,0xcf,0xf3,0x0f,0x67,0x7e,0xf8,0xf3,
 0x3c,0xcf,0x3f,0x3e,0xff,0xf3,0xfe,0xcf,0xf3,0x3c,0xcf,0xff,0xfc,0xff,0x7f,
 0xff,0xfd,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xfe,0xfb,0xfc,0xe7,0x03,
 0x3c,0xf8,0x3f,0xdf,0x80,0x1f,0xfc,0x80,0x67,0xf0,0xdf,0x67,0xfe,0x7f,0xff,
 0xfc,0xbc,0xe7,0xfc,0xce,0xbf,0x3f,0x99,0xff,0xff,0xfc,0xbf,0x3f,0xc0,0xff,
 0xdd,0xe0,0xff,0xfe,0xee,0xfb,0xff,0xcf,0x87,0x1f,0x00,0x81,0xf1,0xff,0xdf,
 0xff,0xcf,0xdf,0xff,0x00,0x01,0xfe,0x9f,0xff,0x7f,0xcf,0x97,0x3f,0xff,0x03,
 0xbc,0xcf,0xf7,0x0f,0x67,0x3e,0xf9,0xf3,0x3c,0xcf,0x1f,0x3e,0xff,0xf3,0xff,
 0xe7,0xf3,0x3c,0xcf,0xff,0xfc,0xc0,0x7f,0xfe,0xf9,0xff,0xff,0xfb,0xef,0xff,
 0xff,0xff,0xbf,0xfc,0x5f,0xfe,0xf3,0xfb,0xfc,0xf9,0x3d,0x10,0x98,0x05,0xfc,
 0xde,0x07,0xf0,0xdf,0x67,0xd8,0x3f,0xff,0xfe,0x00,0xe7,0x7c,0xee,0xbf,0x3f,
 0x99,0x04,0xf0,0xfc,0x3d,0x38,0xe0,0xff,0x1c,0xe4,0x7f,0xfe,0xce,0xfb,0xfc,
 0xcf,0xa7,0x1f,0xf8,0xf9,0x39,0xfe,0xcf,0x7f,0xee,0xdf,0xff,0xa0,0x03,0xfe,
 0x9f,0x0c,0x78,0xcf,0x97,0x3f,0x7f,0xf3,0xbc,0xcf,0xe7,0x0b,0xf3,0xfc,0xf9,
 0xff,0xfc,0xcf,0x4f,0x3e,0xff,0xf3,0xff,0xe7,0xf3,0x3c,0xcf,0xff,0x3c,0xc0,
 0xff,0xfe,0xf9,0xff,0xff,0xf3,0xcf,0xff,0xff,0xff,0xbc,0xcd,0x1f,0xfe,0xf3,
 0xfb,0xfc,0xf9,0x01,0x10,0x99,0x81,0x7f,0xce,0x07,0xff,0xdf,0x01,0x90,0xbf,
 0xff,0x6e,0x80,0xe7,0x7c,0xe6,0xbf,0x3f,0x99,0x01,0xf0,0xf0,0x01,0xf0,0xff,
 0xff,0x3c,0xf3,0x7f,0x7e,0xce,0x7b,0xfc,0xcf,0x37,0xff,0xf9,0xff,0x7c,0xf8,
 0xcf,0xff,0xec,0xdf,0x1f,0x9c,0x7f,0xfe,0x9f,0x01,0x78,0xcf,0x97,0x3f,0x3f,
 0xf3,0xbc,0xcf,0xe7,0x0b,0xf3,0xfc,0xf9,0xff,0xfc,0xe7,0x67,0x3e,0xf1,0x13,
 0xff,0xf3,0x67,0x3e,0xcf,0x03,0x3c,0xce,0x7f,0xbe,0xe9,0xff,0xff,0xf3,0xcf,
 0xfe,0xff,0x03,0x3c,0xc9,0x1f,0xff,0xf9,0xfb,0xfc,0xf9,0x01,0xff,0x99,0x9b,
 0x7f,0xef,0x33,0xff,0xdf,0x01,0x3e,0x9f,0x7f,0x0e,0xdc,0xe7,0x3e,0xe6,0x3f,
 0x38,0xdb,0xb9,0xff,0xe0,0x81,0xff,0xff,0xe7,0xfe,0xf3,0x7f,0x7f,0xde,0x1b,
 0xff,0xcf,0x73,0x7f,0xf1,0xff,0xfc,0xf0,0x6f,0xff,0xe0,0xdf,0x1f,0xdc,0x7f,
 0xfe,0x9f,0xf1,0x79,0xcf,0x97,0x3f,0xbf,0xf3,0xbc,0xcf,0xe7,0x09,0xf3,0xfc,
 0xf9,0x7f,0xfe,0xf1,0x73,0x3e,0xe6,0x63,0xfe,0xf3,0x0f,0x7f,0xc6,0x03,0xfc,
 0xef,0x3f,0x3f,0xe0,0xff,0xff,0xf3,0x0f,0xfc,0xff,0x07,0x7c,0xcb,0x9f,0xff,
 0xf9,0xfb,0xfc,0xf9,0x3f,0xff,0x9d,0x9f,0x3f,0xef,0x3b,0xff,0xdf,0x67,0x7e,
 0xde,0x7f,0x9f,0xcc,0xff,0xbe,0xf4,0x05,0xf8,0xcf,0xbf,0xff,0xc4,0x9f,0xff,
 0xff,0x4f,0xfe,0xf9,0x3f,0x7f,0x9e,0x83,0xff,0xcf,0x7b,0x7e,0xe9,0x7f,0xfe,
 0xe3,0x6f,0xff,0xf1,0x1f,0x98,0xdd,0x7f,0xfe,0x9f,0xff,0x79,0xcf,0x97,0x37,
 0x9f,0xf3,0xbc,0xcf,0xff,0x0d,0xf3,0xfc,0xf9,0x1f,0xff,0xe7,0x73,0xfe,0xcf,
 0xf3,0xfc,0xf9,0x67,0xfe,0xc8,0xff,0xfc,0xe1,0x3f,0x3f,0xe0,0x03,0x3e,0xc0,
 0x0f,0xfc,0xe7,0xff,0x7c,0xcb,0x9f,0xff,0xf8,0xfb,0xfc,0xf9,0x3f,0xff,0x9d,
 0x9f,0xbf,0xe7,0x39,0xff,0xdf,0x67,0x7e,0xce,0x3f,0xff,0xec,0x7f,0x9e,0xf0,
 0x81,0xff,0xcf,0xbf,0xff,0xcc,0x9f,0xff,0xff,0x4f,0xff,0xf1,0x3f,0x7f,0x9f,
 0xc3,0xff,0xe7,0xf9,0x7e,0xc9,0x7b,0xfe,0xc7,0x67,0xfe,0xf3,0x04,0xf8,0xcd,
 0x7f,0x3e,0x80,0xff,0x7c,0xcf,0x97,0x33,0xdf,0xf3,0xbc,0xcf,0xff,0x0c,0xf3,
 0xfc,0xf9,0xcf,0xff,0xcf,0x03,0xfc,0xcf,0xf3,0xfc,0xf9,0xf3,0xfc,0xcf,0x7f,
 0xfe,0xf1,0x1f,0x3f,0xef,0x03,0x3e,0xc0,0xc1,0x7c,0xe0,0xff,0x7c,0xca,0x9f,
 0x7f,0xfa,0xfb,0xfc,0xf9,0x1f,0xff,0x9d,0x9f,0x90,0xf7,0x3d,0xff,0xdf,0x67,
 0xfe,0xef,0xbf,0xff,0xe4,0x7f,0xdf,0xf1,0x99,0xff,0xcf,0xbf,0xff,0xdc,0x9f,
 0xff,0xff,0x1f,0xff,0xe0,0xbf,0x7f,0xbf,0xfb,0xff,0xe7,0xfd,0x3c,0xd9,0x73,
 0xff,0xcf,0xe7,0xfe,0xe7,0xc1,0xff,0xed,0x7f,0x7e,0xc0,0xff,0x7c,0xcf,0x97,
 0x3b,0xcf,0xf3,0xfc,0xcf,0xff,0x0e,0x67,0xfe,0xf9,0xe7,0x3f,0xcf,0x7f,0xfe,
 0xcf,0xf3,0xfc,0xfc,0xf3,0xfc,0xcf,0x7f,0xfe,0xf9,0x0f,0x3f,0xef,0x9f,0xff,
 0xf1,0xc3,0x7e,0xe0,0xff,0xfc,0xee,0x9f,0x3f,0xfb,0xfb,0xfe,0xf9,0x5f,0xff,
 0x9d,0x0f,0xd0,0xf7,0x3d,0xff,0xcf,0x67,0xfe,0xe7,0x1f,0xfe,0xf4,0x3f,0xff,
 0xf3,0x9f,0xff,0xef,0x9f,0xff,0xfc,0x9f,0xff,0xff,0x1f,0x7f,0xc0,0x9f,0x3f,
 0xbf,0xfb,0xff,0xf7,0xff,0x3d,0x99,0x27,0xff,0xff,0xe7,0xfc,0xe3,0xd9,0xff,
 0xe5,0x7f,0xff,0xdf,0x7f,0x7e,0xcf,0x97,0x39,0xef,0xf3,0xfc,0xef,0x7f,0x0e,
 0x67,0xfe,0xf9,0xf3,0x3f,0xcf,0x7f,0x3e,0xcf,0xf3,0xfc,0xfc,0xf3,0x7c,0xcf,
 0x7f,0xfe,0xf9,0x0f,0x3f,0xef,0x9f,0xff,0xf1,0x5f,0xfe,0xe7,0x07,0xfc,0xef,
 0x9f,0x9f,0xfb,0x7b,0xfe,0xf9,0x4f,0xff,0x9c,0x00,0xff,0xf3,0xbf,0xff,0xcf,
 0x67,0xfe,0xf7,0x5f,0xfe,0xf0,0xbf,0xff,0xf1,0x9f,0xff,0xe7,0x9f,0xff,0xfc,
 0x9f,0xff,0xff,0x1f,0x3e,0x88,0xdf,0x3f,0xbf,0xfb,0xff,0xf3,0xff,0xb9,0x99,
 0x8f,0xff,0xff,0xf7,0xfc,0xcb,0xdf,0xff,0xf1,0x7f,0xff,0xdf,0x7f,0x7e,0xef,
 0x97,0x3d,0xe7,0xf3,0xfc,0xe7,0x7f,0x0f,0x0f,0xff,0xf9,0xf3,0x7f,0xe6,0x7f,
 0x7e,0xe6,0x67,0x7e,0xfe,0x67,0x7e,0xe6,0x3f,0xff,0xfd,0x27,0x3f,0xe7,0x9f,
 0xff,0xf0,0x5f,0xff,0xe7,0x07,0xfe,0xe7,0xdf,0xdf,0xfb,0x7f,0xfe,0xb9,0x6f,
 0xff,0xdc,0xa1,0xff,0xfb,0xbf,0xff,0xcf,0x67,0xfe,0xf3,0xcf,0xfc,0xfc,0x9f,
 0xff,0xf5,0x9f,0xff,0xf7,0xdf,0xff,0xfc,0xdf,0xff,0xff,0xdf,0x1e,0x99,0xcf,
 0xbf,0xbf,0xfb,0xff,0xfb,0xff,0x99,0xb9,0x9f,0x3f,0xfe,0xf3,0xf9,0xd9,0xdf,
 0xff,0xf9,0x7f,0xff,0xdf,0x3f,0xff,0xe7,0x97,0x3c,0xf7,0xf3,0xfc,0xe7,0x3f,
 0x0f,0x9f,0x3f,0xc0,0x03,0xfc,0xf0,0x7f,0xfe,0xf0,0x0f,0x7f,0xfe,0x0f,0xff,
 0xf0,0x3f,0xff,0xfd,0x37,0xff,0xf7,0x9f,0xff,0xf2,0xdf,0xff,0xe7,0xff,0xfe,
 0xf7,0xcf,0xff,0xfb,0x3f,0xcf,0x00,0x67,0xff,0xde,0xbf,0xff,0xf9,0x9f,0x1f,
 0xc0,0x67,0xff,0xf9,0xef,0xfd,0xfc,0xdf,0xff,0xfc,0xdf,0xff,0xf3,0xcf,0xff,
 0xfc,0xdf,0xdf,0x00,0xcf,0x8c,0x39,0xef,0xbf,0xbf,0xfb,0xff,0xf9,0xff,0xc3,
 0xb9,0x3f,0x3f,0xf8,0x33,0xf8,0x9d,0xdf,0xff,0xf9,0x7c,0xff,0xdf,0xbf,0xff,
 0xe7,0x93,0x3e,0xf3,0x03,0xfc,0xf7,0x9f,0x0f,0xff,0xff,0xff,0xff,0xff,0xff,
 0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x9f,0xff,0xfd,0x3f,0xff,0xf3,
 0x9f,0x7f,0xf2,0xdf,0xff,0xe7,0xff,0xfe,0xf3,0xef,0xff,0xfb,0x3f,0x1f,0x00,
 0x77,0x7f,0xda,0xbf,0xff,0xfc,0xdf,0x3f,0xc0,0x7f,0xbf,0xfc,0xe7,0xf9,0xfc,
 0xcf,0xff,0xfe,0xcf,0xff,0xf9,0xef,0xff,0xfc,0xcf,0x1f,0x00,0xef,0xfd,0x39,
 0xe7,0x9f,0x3f,0xfb,0xff,0xfd,0xff,0x43,0xb8,0x3f,0xff,0xf1,0x01,0xf8,0xfc,
 0xdf,0xff,0xfb,0x01,0xf0,0xdf,0x9f,0xff,0xe7,0x13,0x3e,0xf9,0x03,0xfc,0xf3,
 0x8f,0x0f,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
 0xff,0xff,0x9f,0xff,0xfc,0x3f,0xff,0xfb,0x9f,0x7f,0xf3,0x9f,0xff,0xe7,0xff,
 0xfe,0xfb,0xe7,0xff,0xfb,0x9f,0x1f,0xff,0x53,0x7f,0xc3,0xbf,0xff,0xfc,0xcf,
 0xff,0xff,0x3f,0x3f,0xfc,0xf3,0xf9,0xfc,0xef,0x7f,0xfe,0xef,0xff,0xf9,0xe7,
 0xff,0xfc,0xef,0x1f,0xff,0xe7,0xf9,0xf9,0xf7,0xdf,0x3f,0xfb,0xff,0xfc,0xff,
 0xf3,0xf8,0x7f,0xfe,0xe3,0xc1,0xf3,0xfe,0xdf,0xff,0xfb,0x01,0x10,0x80,0xdf,
 0xff,0xf7,0x1b,0x3e,0xfd,0xf3,0xff,0xfb,0xcb,0x0f,0xff,0xff,0xff,0xff,0xff,
 0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xcf,0xff,0xfe,0x3f,0xff,
 0xf9,0x9f,0x3f,0xf3,0x9f,0xff,0xe7,0xff,0xfe,0xf9,0xf7,0xff,0xfb,0x9f,0xff,
 0xff,0x19,0x3f,0xe7,0xbf,0x7f,0xfe,0xef,0xff,0xff,0xbf,0x3f,0xfe,0xf9,0xfb,
 0x01,0xe7,0x3f,0xff,0xe7,0xff,0xfc,0xf3,0xff,0xfc,0xe7,0xff,0xff,0xf3,0xf3,
 0xf9,0xf3,0xcf,0xff,0xf3,0xf9,0xfe,0xff,0xff,0xf8,0x7f,0xfe,0xc7,0xf9,0x73,
 0xfe,0x1f,0xf0,0xfb,0xff,0x1f,0x80,0xcf,0xff,0xf3,0x19,0x3f,0xfc,0xf3,0xff,
 0xf9,0xe3,0x0f,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
 0xff,0xff,0xff,0xe7,0x7f,0xfe,0x3f,0xff,0xfd,0x03,0xb8,0xf1,0x9f,0x1f,0xc0,
 0x03,0xfc,0xfc,0xf3,0xff,0xfb,0xcf,0xff,0xff,0x19,0x9f,0xe7,0xbf,0x3f,0xff,
 0xe7,0xff,0xff,0x9f,0x7f,0xfe,0xf9,0xff,0x01,0xf7,0x1f,0xff,0xf3,0x7f,0xfe,
 0xf3,0xff,0xfc,0xf3,0xff,0xff,0xf9,0xf3,0xf9,0xfb,0xef,0xff,0x03,0x78,0xfe,
 0xff,0xff,0xf9,0xff,0xff,0xcf,0xff,0x33,0xff,0x1f,0xf0,0xfb,0xff,0xff,0xff,
 0xe7,0xff,0xf8,0x1d,0x3f,0xfe,0xff,0xff,0xfd,0xe3,0x0f,0xff,0xff,0xff,0xff,
 0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xe7,0x7f,0xff,0x3f,
 0xff,0xfc,0x03,0xf8,0xfb,0x9f,0x1f,0x80,0x07,0x7c,0xfc,0xfb,0xff,0xfb,0xe7,
 0xff,0xff,0x3c,0x9f,0xff,0xbf,0x9f,0xff,0xf7,0xff,0xff,0xcf,0xff,0xff,0xff,
 0xff,0xff,0xff,0x9f,0xff,0xf3,0x3f,0xff,0xf9,0xff,0xfc,0xff,0xff,0xff,0xf9,
 0xff,0xf9,0xf9,0xff,0xff,0x07,0x38,0xff,0xff,0xff,0xfd,0xff,0xff,0xdf,0xff,
 0x33,0xff,0xff,0xff,0xfb,0xff,0xff,0xff,0xf3,0xff,0xfc,0xbd,0x7f,0xff,0xff,
 0xff,0xfc,0xf3,0x0f,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
 0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
 0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
 0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
 0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
 0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
 0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x0f
};

#define PIXMAP_WIDTH katakana_cell_width

#define PICK_MATRIX(mi) (&matrix[MI_SCREEN(mi)])
#define MATRIX_RANDOM(min, max) (NRAND((max) - (min)) + (min))

/*************************************************************************
 * CONFIGURABLE DISPLAY OPTIONS
 *************************************************************************/

/* Defining RANDOMIZE_COLUMNS causes each column pixmap to be regenerated
 * after scrolling off the screen rather than just repeating
 * infinitely.  This is a better effect but uses more CPU and will
 * cause update hesitations (especially on multi-headed machines or
 * slower hardware).  However, it can be done in relatively little memory
 * because the pixmaps can be kept short and regenerated frequently.
 *
 * Undefining RANDOMIZE_COLUMNS will cause the pixmaps to be generated only 
 * once, which should save a lot of horsepower for whatever else your machine
 * might be doing.  However, the tradeoff is that the pixmaps are longer
 * to avoid a really boring display.
 */
#define RANDOMIZE_COLUMNS

/*
 * Speed at which columns move, in pixels per screen update.
 * Must be <= katakana_cell_height; interesting values range
 * from one (crawl) to about fifteen (zoom!).
 * (Note: the distribution of speeds implemented in new_column()
 *  is slightly biased against very slow scrolling.) 
 */
#define MATRIX_SPEED  MATRIX_RANDOM(1, 8)

/*
 * Number of characters in each generated string
 * (Note: this is not limited by the pixmap size!)
 */
#define MATRIX_STRLEN MATRIX_RANDOM(15, 20)

/*
 * Number of character-heights in each generated gap
 * (Note: this is not limited by the pixmap size!)
 */
#define MATRIX_GAPLEN MATRIX_RANDOM(2, 6)

/* colors used for Matrix characters */
#define BRIGHTGREEN (28 * MI_NPIXELS(mi) / 64)
#define GREEN       (26 * MI_NPIXELS(mi) / 64)

/*************************************************************************/


/*
 * state of the Matrix
 */
static matrix_t *matrix = NULL;


/* local prototypes */
static void init_active_screen(const ModeInfo *mi);
static void free_screen(const ModeInfo *mi, matrix_t *mp);
static void new_column(const ModeInfo *mi, column_t *);


/* NAME: init_matrix
 *
 * FUNCTION: allocate space for global matrix array 
 *           initialize colors
 *           initialize dimensions
 *           allocate a pair of pixmaps containing character set 
 *               (0 is green on black, 1 is bold on black)
 *           create columns of "matrix" data
 */

void init_matrix(ModeInfo *mi)
{
  matrix_t *mp;
  
  if (matrix == NULL)
	if((matrix = (matrix_t *)
		calloc(MI_NUM_SCREENS(mi), sizeof(matrix_t))) == NULL)
	  {
		perror("init_matrix:calloc");
		return;
	  }
  
  /* don't want any exposure events from XCopyArea */
  XSetGraphicsExposures(MI_DISPLAY(mi), MI_GC(mi), False);
  
  mp = PICK_MATRIX(mi);
  
  /* screen may have changed size/depth/who-knows-what */
  free_screen(mi, mp);
  init_active_screen(mi);
}


/* NAME: draw_matrix
 *
 * FUNCTION: update the matrix waterfall display
 */
void draw_matrix(ModeInfo *mi)
{
  matrix_t *mp = PICK_MATRIX(mi);
  double xDistance = (double) MI_WIDTH(mi) / (double) mp->num_cols;
  int i;
  
  MI_IS_DRAWN(mi) = True;
  
  /*
   * THEORY OF OPERATION
   *
   * Screen-to-screen blits occur within the video RAM and are usually
   * hardware-accelerated, while pixmap-to-screen blits require DMA
   * between main memory and vRAM.  To make updates as fast as possible,
   * we scroll down the screen contents in each column by <speed> pixels,
   * then fill only the top <speed> rows of the column with pixmap data.
   *
   * There are two tricky bits to this scheme.  First, we must keep a
   * pointer (yPtr) to the pixmap contents which should be used to
   * fill in the top rows of each column.  yPtr is decremented on each
   * call to draw_matrix(); when it reaches zero, the column's pixmap
   * may be redrawn to generate new characters for the column.
   *
   * Second, the scroll increment corresponding to <speed> need not be a 
   * multiple of the pixmap height. We must special-case the handling of the 
   * last decrement that makes yPtr <= 0 to ensure that the last few pixels
   * of the old data AND the first few pixels of the new data are both drawn.
   * It turns out to be quite reasonable to do the pixmap update inline, 
   * between drawing the last few old pixels and the first few new pixels.
   * Hence, we can achieve smooth scrolling with only one pixmap per column.
   */
  
  for (i = 0; i < mp->num_cols; i++) 
	{
	  column_t *c = &(mp->columns[i]);
	  int xOffset = (int) (i * xDistance);
	  int yDelta  = c->speed;
	  
	  /* screen-screen blit takes care of most of the column */
	  XCopyArea(MI_DISPLAY(mi), MI_WINDOW(mi), 
				MI_WINDOW(mi), MI_GC(mi),
				xOffset, 0,
				PIXMAP_WIDTH, MI_HEIGHT(mi) - yDelta,
				xOffset, yDelta);
	  
	  c->yPtr -= yDelta;
	  
	  if (c->yPtr <= 0) /* we're about to finish the current pixmap */
		{
		  /* exhaust the pixels in the current pixmap */
		  XCopyArea(MI_DISPLAY(mi), c->pixmap,
					MI_WINDOW(mi), MI_GC(mi),
					0, 0, 
					PIXMAP_WIDTH, yDelta + c->yPtr,
					xOffset, -(c->yPtr));
		  
#ifdef RANDOMIZE_COLUMNS
		  /* regenerate the column pixmap with new text */
		  new_column(mi, c);
#else
		  /* speed is normally updated in new_column(), which isn't called,
		   * so take this opportunity to update the column speed.
		   */
		  c->speed = MATRIX_SPEED;
#endif	
		  
		  /* fill in the remainder of the column from the new pixmap */
		  XCopyArea(MI_DISPLAY(mi), c->pixmap,
					MI_WINDOW(mi), MI_GC(mi),
					0, mp->pixmapHeight + c->yPtr, 
					PIXMAP_WIDTH, -(c->yPtr),
					xOffset, 0);
		  
		  c->yPtr += mp->pixmapHeight;
		}
	  else
		{
		  /* fill in the remainder of the column from the pixmap */
		  XCopyArea(MI_DISPLAY(mi), c->pixmap,
					MI_WINDOW(mi), MI_GC(mi),
					0, c->yPtr, 
					PIXMAP_WIDTH, yDelta,
					xOffset, 0);
		}
	}
}


/* NAME: release_matrix
 *
 * FUNCTION: frees all allocated resources
 */

void release_matrix(ModeInfo *mi)
{
  int screen;
  
  /* If the matrix exists, free all data associated with all screens.
   * free_screen() does no harm if given nonexistent screens.
   */
  if (matrix != NULL)
	{
	  for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++) 
		free_screen(mi, &matrix[screen]);
	  
	  (void) free((void *) matrix);
	  matrix = NULL;
	}
}


/* NAME: refresh_matrix
 *
 * FUNCTION: refresh the screen
 */

void refresh_matrix(ModeInfo *mi)
{
  matrix_t *mp = PICK_MATRIX(mi);
  int c;
  
  /* We simply clear the whole window and restart the scrolling operation.
   * In principle, it is possible to restore at least some of the
   * column pixmap data to the screen, but it's not worth the trouble.
   */
  MI_CLEARWINDOW(mi);
  
  for (c = 0; c < mp->num_cols; c++)
	mp->columns[c].yPtr = mp->pixmapHeight;
}


/* NAME: change_matrix
 *
 * FUNCTION: resets column offsets and speeds and generates new pixmaps
 */

void change_matrix(ModeInfo *mi)
{
  matrix_t *mp = PICK_MATRIX(mi);
  int c;
  
  MI_CLEARWINDOW(mi);
  
  for (c = 0; c < mp->num_cols; c++)
	{
	  column_t *column = &(mp->columns[c]);
	  
	  column->yPtr = mp->pixmapHeight;
	  
	  column->nextSpeedUpdate = 0; /* will generate new speed  */
	  column->strLen  = 0;         /* will generate new string */
	  
	  new_column(mi, column);
	}
}

/***********************************************************************/


/* NAME: init_active_screen()
 *
 * FUNCTION: Initialize the Matrix state for the current screen.
 *           Based on the width and height of the display,
 *           allocate and initialize column data structures.
 */

static void init_active_screen(const ModeInfo *mi)
{
  matrix_t *mp = PICK_MATRIX(mi);
  int c;
 
  if (MI_NPIXELS(mi) > 2) 
	{
	  mp->fg   = MI_PIXEL(mi, GREEN);
	  mp->bold = MI_PIXEL(mi, BRIGHTGREEN);
	} 
  else 
	mp->fg = mp->bold = MI_WHITE_PIXEL(mi);
  
  mp->bg = MI_BLACK_PIXEL(mi);
  
  mp->kana[0] = 
	XCreatePixmapFromBitmapData(MI_DISPLAY(mi),
								MI_WINDOW(mi), (char *) katakana_bits,
								katakana_width, katakana_height,
								mp->bg, mp->fg, MI_DEPTH(mi));
  
  mp->kana[1] = 
	XCreatePixmapFromBitmapData(MI_DISPLAY(mi),
								MI_WINDOW(mi), (char *) katakana_bits,
								katakana_width, katakana_height,
								mp->bg, mp->bold, MI_DEPTH(mi));
  
  
  /* 
   * Using the width of the kana font, determine how many
   * columns can fit across the current screen (with a little
   * padding).
   */
  mp->num_cols = MI_WIDTH(mi) / (1.3 * katakana_cell_width);
  
  mp->columns = (column_t *) calloc(mp->num_cols, sizeof(column_t));
  if (mp->columns == NULL)
	{
	  perror("init_active_screen:calloc");
	  return;
	}

  /*
   * If we're randomizing columns, keep the column pixmap height small
   * to save memory and minimize time for each column update.  Otherwise,
   * make the height large to maximize variety without updating the pixmaps.
   */
#ifdef RANDOMIZE_COLUMNS
  mp->charsPerPixmap = MI_HEIGHT(mi) / (4 * katakana_cell_height); 
#else
  mp->charsPerPixmap = MI_HEIGHT(mi) / katakana_cell_height; 
#endif
  
  if (!mp->charsPerPixmap)  /* sanity check for tiny windows */
	mp->charsPerPixmap = 1;
  
  mp->pixmapHeight = mp->charsPerPixmap * katakana_cell_height;
  
  for (c = 0; c < mp->num_cols; c++)
	mp->columns[c].pixmap  = XCreatePixmap(MI_DISPLAY(mi), MI_WINDOW(mi),
										   PIXMAP_WIDTH, mp->pixmapHeight,
										   MI_DEPTH(mi));
  
  /* Change the speed of a column after it's covered half the screen */
  mp->speedUpdateInterval = MI_HEIGHT(mi) / (2 * katakana_cell_height);
  
  /* initialize the column data */
  for (c = 0; c < mp->num_cols; c++)
	{
	  column_t *column = &(mp->columns[c]);
	  
	  column->yPtr = mp->pixmapHeight;
	  
	  column->nextSpeedUpdate = 0; /* will generate new speed  */
	  column->strLen  = 0;         /* will generate new string */
	  
	  new_column(mi, column);
	}
}


/* NAME: free_screen
 *
 * FUNCTION: delete the data associated with a single screen.  Be careful
 * not to do spurious free()s of nonexistent data, and to nullify all
 * freed pointers that might be reused past the end of this call.
 */
static void free_screen(const ModeInfo *mi, matrix_t *mp)
{
  if (mp->columns != NULL)
	{
	  int c;
	  
	  for (c = 0; c < mp->num_cols; c++)
		{
		  if (mp->columns[c].pixmap)
			XFreePixmap(MI_DISPLAY(mi), mp->columns[c].pixmap);
		}
	  
	  free(mp->columns);
	  mp->columns = NULL;
	}
  
  if (mp->kana[0])
	{
	  XFreePixmap(MI_DISPLAY(mi), mp->kana[0]);
	  mp->kana[0] = 0;
	}
  
  if (mp->kana[1])
	{
	  XFreePixmap(MI_DISPLAY(mi), mp->kana[1]);
	  mp->kana[1] = 0;
	}
}


/* NAME: new_column
 *
 * FUNCTION: clears and generates a pixmap of "matrix" data
 */

static void new_column(const ModeInfo *mi, column_t *c)
{
  matrix_t *mp = PICK_MATRIX(mi);
  int currChar;
  
  /* 
   * clear the pixmap to get rid of previous data or initialize
   */
  XSetForeground(MI_DISPLAY(mi), MI_GC(mi), mp->bg);
  XFillRectangle(MI_DISPLAY(mi), c->pixmap, MI_GC(mi),
				 0, 0, PIXMAP_WIDTH, mp->pixmapHeight);
  
  /* 
   * Write characters into the column pixmap, starting at the
   * bottom and moving upwards.  Decrement the string length, gap length,
   * and next-speed-update timers for the column and reinitialize them
   * if necessary.
   */
  for (currChar = mp->charsPerPixmap - 1; currChar >= 0; currChar--)
	{
	  if (c->nextSpeedUpdate-- == 0)
		{
		  c->speed = MATRIX_SPEED;
		  c->nextSpeedUpdate = mp->speedUpdateInterval;
		  
		  /* tweak to prevent really slow columns from remaining slow */
		  if (c->speed <= 2) 
			c->nextSpeedUpdate /= 2;
		}
	  
	  if (c->gapLen > 0)
		{
		  c->gapLen--;
		  continue;
		}
	  else if (c->strLen == 0) 
		{
		  /* 
		   * generate a gap of random length in the string, followed
		   * by a random number of characters.  Set the endChar flag
		   * to indicate the end of a new string.
		   */
		  c->gapLen  = MATRIX_GAPLEN;
		  c->strLen  = MATRIX_STRLEN;
		  c->endChar = True;
		}
	  else
		{
		  Pixmap src;
		  int kOffset;
		  
		  if (c->endChar) 
			{  
			  src = mp->kana[1]; /* a bold, non-numeric character */
			  kOffset = MATRIX_RANDOM(10, katakana_num_cells);
			  c->endChar = False;
			} else {             /* inside a string - kana or number */
			  src = mp->kana[0];
			  kOffset = MATRIX_RANDOM(0, katakana_num_cells);
			}
		  
		  XCopyArea(MI_DISPLAY(mi), src, c->pixmap, MI_GC(mi),
					kOffset * katakana_cell_width, 0,
					katakana_cell_width, katakana_cell_height,
					0, currChar * katakana_cell_height);
		  
		  c->strLen--;
		}
	}
}
