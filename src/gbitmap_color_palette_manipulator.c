// The MIT License (MIT)
// Copyright (c) 2015 Jonathan LaRocque
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#include "gbitmap_color_palette_manipulator.h"

static int get_num_palette_colors(GBitmap *b){
	GBitmapFormat format = gbitmap_get_format(b);
	switch (format) {
		case GBitmapFormat1BitPalette: return 2;
		case GBitmapFormat2BitPalette: return 4;
		case GBitmapFormat4BitPalette: return 16;
		default: return 0;
	}
}

void replace_gbitmap_color(GColor color_to_replace, GColor replace_with_color, GBitmap *im){
	//First determine what the number of colors in the palette
	int num_palette_items = get_num_palette_colors(im);

	//Get the gbitmap's current palette
	GColor *current_palette = gbitmap_get_palette(im);

	//Iterate through the palette finding the color we want to replace and replacing 
	//it with the new color
	for(int i = 0; i < num_palette_items; i++){
		if ((color_to_replace.argb & 0x3F)==(current_palette[i].argb & 0x3F)){
			current_palette[i].argb = (current_palette[i].argb & 0xC0)| (replace_with_color.argb & 0x3F);
		}
	}
}

