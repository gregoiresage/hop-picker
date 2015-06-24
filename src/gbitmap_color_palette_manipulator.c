
#include "gbitmap_color_palette_manipulator.h"


#ifdef PBL_COLOR
static int get_num_palette_colors(GBitmap *b){

	GBitmapFormat format = gbitmap_get_format(b);

	switch (format) {
		case GBitmapFormat1Bit: return 0;
		case GBitmapFormat8Bit: return 0;
		case GBitmapFormat1BitPalette: return 2;
		case GBitmapFormat2BitPalette: return 4;
		case GBitmapFormat4BitPalette: return 16;

		default: return 0;
	}
}
#endif

void replace_gbitmap_color(GColor color_to_replace, GColor replace_with_color, GBitmap *im){
#ifdef PBL_COLOR
	//First determine what the number of colors in the palette
	int num_palette_items = get_num_palette_colors(im);

	//Get the gbitmap's current palette
	GColor *current_palette = gbitmap_get_palette(im);

	//Iterate through the palette finding the color we want to replace and replacing 
	//it with the new color
	for(int i = 0; i < num_palette_items; i++){
		if ((color_to_replace.argb & 0x3F)==(current_palette[i].argb & 0x3F)){
			current_palette[i].argb = (current_palette[i].argb & 0xC0)| (replace_with_color.argb & 0x3F);
			break;
		}
	}
#endif
}

