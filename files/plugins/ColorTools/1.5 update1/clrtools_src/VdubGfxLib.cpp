/*
varoius routines for use in VirtualDub filters.
Copyright (C) 2001  Thomas Hargrove <ciagon@yourmom.dhs.org>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include <windows.h>
#include <stdio.h>

#include "resource.h"

#include <vd2/plugin/vdvideofilt.h>
#include "VdubGfxLib.h"

///////////////////////////////////////////////////////////////////////////
//
// This will combine two colors and produce a third.
// color1 = background color
// color2 = the color to be added.  This is the source
//   of the alpha channel

Pixel32 VGL_Blend2Colors(Pixel32 color1, Pixel32 color2) {
	int r1, g1, b1, r2, g2, b2, r3, g3, b3;
	float Achannel = (float) ((color2 >> 24) & 0xFF);

	if (!Achannel) {
		return color1;
	} else {
		r1 = color1 & 0xFF;
		g1 = (color1 >> 8) & 0xFF;
		b1 = (color1 >> 16) & 0xFF;

		r2 = color2 & 0xFF;
		g2 = (color2 >> 8) & 0xFF;
		b2 = (color2 >> 16) & 0xFF;
		
		int r_dif, g_dif, b_dif;

		r_dif = (int) ( (float) (r2 - r1) * (Achannel / 255.0));
		g_dif = (int) ( (float) (g2 - g1) * (Achannel / 255.0));
		b_dif = (int) ( (float) (b2 - b1) * (Achannel / 255.0));

		r3 = r1 + r_dif;
		g3 = g1 + g_dif;
		b3 = b1 + b_dif;

		return r3 + (g3<<8) + (b3<<16);
	}
}

///////////////////////////////////////////////////////////////////////////
//
// This draws a pixel with the alpha channel.  The color
// mask is aarrggbb.  If aa=0x00, then the pixel is fully
// transparent.  If aa=0xFF, the the pixel is opaque.  0x80
// is right in the middle.  For expample:
// color=0x80FF0000; will produce a %50 transparent red.

void VGL_DrawPixel(const VDXFilterActivation *fa, int x, int y, Pixel32 color) {
	int w = fa->dst.w;
	int h = fa->dst.h;
	ptrdiff_t m = fa->dst.modulo;
	Pixel32 *dst = (Pixel32 *)fa->dst.data;
	dst = dst + x + (w+m) * (h-y);
	// Error checking to keep me in line.
	if ((x<0) || (y<0) || (x>=w) || (y>=h)) {
		//MessageBox(NULL, "Tried to draw a pixel out of bounds", "Error", NULL);
		return;
	}
	// Write the pixel
	*dst = VGL_Blend2Colors(*dst, color);
}

///////////////////////////////////////////////////////////////////////////
//
// Draws a line between two points.  I stole this from somewhere, but I can;t 
// remember where.

void VGL_DrawLine (const VDXFilterActivation *fa, int x1, int y1, int x2, int y2, Pixel32 color)
{
  int   i, deltax, deltay, numpixels,
        d, dinc1, dinc2,
        x, xinc1, xinc2,
        y, yinc1, yinc2;

  //Calculate deltax and deltay for initialisation
  deltax = abs (x2 - x1);
  deltay = abs (y2 - y1);

  //Initialize all vars based on which is the independent variable
  if (deltax >= deltay) {
    //x is independent variable
    numpixels = deltax + 1;
    d = (deltay << 1) - deltax;
    dinc1 = deltay << 1;
    dinc2 = (deltay - deltax) << 1;
    xinc1 = xinc2 = yinc2 = 1;
    yinc1 = 0;
  } else {
    //y is independent variable
    numpixels = deltay + 1;
    d = (deltax << 1) - deltay;
    dinc1 = deltax << 1;
    dinc2 = (deltax - deltay) << 1;
    xinc1 = 0;
    xinc2 = yinc1 = yinc2 = 1;
  }

  //Make sure x and y move in the right directions
  if (x1 > x2) {
    xinc1 = -xinc1;
    xinc2 = -xinc2;
  }
  if (y1 > y2) {
    yinc1 = - yinc1;
    yinc2 = - yinc2;
  }

  //Start drawing at <x1, y1>
  x = x1;
  y = y1;

  //Draw the pixels
  for (i = 0; i < numpixels; i++) {
    VGL_DrawPixel (fa, x, y, color);
    if (d < 0) {
      d += dinc1;
      x += xinc1;
      y += yinc1;
    } else {
      d += dinc2;
      x += xinc2;
      y += yinc2;
    }
  }
}

///////////////////////////////////////////////////////////////////////////
//
// This draws a char.  Pretty simple.
// Remember to include "myfont.h";

void VGL_DrawChar(const VDXFilterActivation *fa, int x, int y, char c, Pixel32 color) {
	unsigned char bits[8]={128,64,32,16,8,4,2,1};
	for (int ty=0; ty<8; ty++) {
		for (int tx=0; tx<8; tx++) {
			if ((fontdata[c*8+ty] & bits[tx])==bits[tx])
				VGL_DrawPixel(fa, x+tx, y+ty, color);
			else {
				// This creates a background for the text.
				// Uncomment it out if you want one
				// VGL_DrawPixelA(fa, x+tx, y+ty, 0xB0000000);
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////
//
// Another simple function.

void VGL_DrawString(const VDXFilterActivation *fa, int x, int y, char *c, Pixel32 color) {
	int t=0;
	int tx = x;
	do {
		VGL_DrawChar(fa, tx, y, *c, color);
		c++;
		tx+=8;
	} while ((t++<1024) && (*c!=0));
}
