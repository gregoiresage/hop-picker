#include <pebble.h>
#include "autoconfig.h"
#include "gbitmap_color_palette_manipulator.h"

static Window *window = NULL;
static Layer  *layer = NULL;

static GBitmap *bt_disconnected = NULL;

GFont custom_font;
GFont small_font;

static GPath *hour_arrow;
static const GPathInfo LINE_HAND_POINTS =  {4,(GPoint []) {{-4, 0},{-4, -300},{4, -300},{4, 0}}};
static const GPathInfo ARROW_HAND_POINTS = {4,(GPoint []) {{-9, 0},{-2, -175},{2, -175},{9, 0}}};

static char* txt[] = {"0","1","2","3","4","5","6","7","8","9","10","11","12","13","14","15","16","17","18","19","20","21","22","23"};

static char date_text[3] = "31";

static AppTimer *timer;

static int anim_minutes = 0;
static bool isAnimating = false;
static int increaseAnimation = 20;
static uint8_t percent = 100;

static bool btConnected = false;

static GColor fg_color;
static GColor bg_color;
static GColor hand_color;

static int hours = 0;
static int minutes = 0;
static int day = 0;

#define DATE_POSITION_FROM_CENTER 100
#define SECOND_HAND_LENGTH_A 150
#define SECOND_HAND_LENGTH_C 180

static bool containsCircle(GPoint center, int radius){
	return center.x - radius > 0 && center.x + radius < 144 && center.y - radius > 0 && center.y + radius < 168;
}

static void handle_minute_tick(struct tm *tick_time, TimeUnits units_changed) {
	hours = tick_time->tm_hour;
	minutes = tick_time->tm_min;
	day = tick_time->tm_mday;
	if(layer)
		layer_mark_dirty(layer);
}

static void animation_timer_callback(void *data) {
	timer = NULL;
	layer_mark_dirty(layer);
  	if(isAnimating)
  		timer = app_timer_register(20 /* milliseconds */, animation_timer_callback, NULL);
  	percent += 8;
  	percent = percent > 100 ? 100 : percent;
}

static void drawClock(GPoint center, GContext *ctx){
	GPoint segA;
	GPoint segC;
	
	graphics_context_set_text_color(ctx, fg_color);
	graphics_context_set_fill_color(ctx, fg_color);

	int minhour=(hours+24)-2;
	int maxhour=hours+24+3;
	
	for(int i=minhour*4; i<maxhour*4; i++){
		int32_t angle = TRIG_MAX_ANGLE * (i % (12*4)) / (12*4);
		if(isAnimating){
			segC.y = (int16_t)((-cos_lookup(angle) * SECOND_HAND_LENGTH_C / TRIG_MAX_RATIO) + center.y - 84) * percent / 100 + 84;
			segC.x = (int16_t)((sin_lookup(angle) * SECOND_HAND_LENGTH_C / TRIG_MAX_RATIO) + center.x - 72) * percent / 100 + 72;
			segA.x = segA.y = 0;
		}
		else {
			segA.y = (int16_t)(-cos_lookup(angle) * SECOND_HAND_LENGTH_A / TRIG_MAX_RATIO) + center.y;
			segA.x = (int16_t)(sin_lookup(angle) * SECOND_HAND_LENGTH_A / TRIG_MAX_RATIO) + center.x;
			segC.y = (int16_t)(-cos_lookup(angle) * SECOND_HAND_LENGTH_C / TRIG_MAX_RATIO) + center.y;
			segC.x = (int16_t)(sin_lookup(angle) * SECOND_HAND_LENGTH_C / TRIG_MAX_RATIO) + center.x;
		}
		
		uint8_t radius = i % 4  ? 3 : 6;

		if(radius == 6 || containsCircle(segC, radius))
		{
			graphics_fill_circle(ctx, segC, radius);

			if(!isAnimating && (i % 4 == 0)) {			
				if(clock_is_24h_style()){
					graphics_draw_text(ctx,
						txt[(i % (24*4))/4],
						custom_font,
						GRect(segA.x-25, segA.y-25, 50, 50),
						GTextOverflowModeWordWrap,
						GTextAlignmentCenter,
						NULL);	
				}
				else {
					graphics_draw_text(ctx,
						(i % (12*4))/4 == 0 ? "12" : txt[(i % (12*4))/4],
						custom_font,
						GRect(segA.x-25, segA.y-25, 50, 50),
						GTextOverflowModeWordWrap,
						GTextAlignmentCenter,
						NULL);
				}
			}
		}
		
	}
}

static void drawDate(GPoint center, int angle, GContext *ctx){
	GPoint segA;

	int32_t posFromCenter = DATE_POSITION_FROM_CENTER;

	do{
		segA.y = (int16_t)(-cos_lookup(angle) * posFromCenter / TRIG_MAX_RATIO) + center.y;
		segA.x = (int16_t)(sin_lookup(angle) * posFromCenter / TRIG_MAX_RATIO) + center.x;
		posFromCenter--;
	}
	while(containsCircle(segA, 17 + 1));

	snprintf(date_text, sizeof(date_text), "%d", day); 

	graphics_context_set_text_color(ctx, fg_color);
	graphics_context_set_stroke_color(ctx, fg_color);
	graphics_context_set_fill_color(ctx, hand_color);

	graphics_fill_circle(ctx, GPoint(segA.x, segA.y), 17);
	// graphics_draw_circle(ctx, GPoint(segA.x, segA.y), 15);
	
	graphics_context_set_fill_color(ctx, bg_color);

	graphics_fill_circle(ctx, GPoint(segA.x, segA.y), 13);
	graphics_draw_text(ctx,
					date_text,
					small_font,
					GRect(segA.x-11, segA.y-11, 25, 25),
					GTextOverflowModeWordWrap,
					GTextAlignmentCenter,
					NULL);
}

static void drawHand(GPoint secondHand, int angle, GContext *ctx){

	graphics_context_set_stroke_color(ctx, fg_color);
	graphics_context_set_fill_color(ctx, hand_color);

	gpath_move_to(hour_arrow, secondHand);
	gpath_rotate_to(hour_arrow, angle);
	gpath_draw_filled(ctx, hour_arrow);
	// gpath_draw_outline(ctx, hour_arrow);

}

static void layer_update_proc(Layer *layer, GContext *ctx) {
	GRect bounds = layer_get_bounds(layer);
	GPoint center = grect_center_point(&bounds);
	
	GPoint secondHand;

	const int16_t secondHandLength = 150;

	int32_t second_angle = TRIG_MAX_ANGLE * ((hours % 12) * 60 + minutes) / (12 * 60);

	secondHand.y = (int16_t)(cos_lookup(second_angle) * (int32_t)secondHandLength / TRIG_MAX_RATIO) + center.y;
	secondHand.x = (int16_t)(-sin_lookup(second_angle) * (int32_t)secondHandLength / TRIG_MAX_RATIO) + center.x;

	drawClock(secondHand, ctx);

	if(!isAnimating && percent >= 100){
		drawHand(secondHand, second_angle, ctx);

		if(getDisplay_bt_icon() && !btConnected){
#ifdef PBL_COLOR
  			graphics_context_set_compositing_mode(ctx,GCompOpSet);
#else
  			if(gcolor_equal(GColorBlack,bg_color))
  				graphics_context_set_compositing_mode(ctx,GCompOpAssignInverted);
#endif
			if(hours % 12 < 3)
				graphics_draw_bitmap_in_rect(ctx, bt_disconnected, GRect(144-32,0,32,32));
			else if(hours % 12 < 6)
				graphics_draw_bitmap_in_rect(ctx, bt_disconnected, GRect(144-32,160-32,32,32));
			else if(hours % 12 < 9)
				graphics_draw_bitmap_in_rect(ctx, bt_disconnected, GRect(0,160-32,32,32));
			else 
				graphics_draw_bitmap_in_rect(ctx, bt_disconnected, GRect(0,0,32,32));
		}
		
		if(getDate()){
			drawDate(secondHand, second_angle, ctx);
		}
	}
	else if(percent >= 100) {
		isAnimating = false;
	}
	
}

static void window_load(Window *window) {
	Layer *window_layer = window_get_root_layer(window);
	GRect bounds = layer_get_bounds(window_layer);
	
	layer = layer_create(bounds);
	layer_set_update_proc(layer, layer_update_proc);
	layer_add_child(window_layer, layer);

	timer = app_timer_register(500 /* milliseconds */, animation_timer_callback, NULL);
}

static void window_unload(Window *window) {
	layer_destroy(layer);
}

static GColor getHandColor(int color){
	switch(color){
		case HAND_COLOR_WHITE	: return GColorWhite;
		case HAND_COLOR_BLACK	: return GColorBlack;
#ifdef PBL_COLOR
		case HAND_COLOR_RED 	: return GColorRed;
		case HAND_COLOR_BLUE 	: return GColorVividCerulean;
		case HAND_COLOR_ORANGE 	: return GColorOrange;
		case HAND_COLOR_MAGENTA	: return GColorMagenta;
		case HAND_COLOR_GREEN	: return GColorMalachite;
#endif
		default				: return GColorWhite;
	}
}

static void update_colors(){
	int theme = getColor_theme();

	switch(theme){
		case COLOR_THEME_DARK : 
			bg_color = GColorBlack;
			fg_color = GColorWhite;
			break;
		case COLOR_THEME_CLEAR :
			bg_color = GColorWhite;
			fg_color = GColorBlack;
			break;
	}

#ifdef PBL_COLOR
	hand_color = getHandColor(getHand_color());
	if(gcolor_equal(hand_color,fg_color))
		hand_color = fg_color;
#else
	hand_color = fg_color;
#endif

	replace_gbitmap_color(GColorBlack, fg_color, bt_disconnected);

	window_set_background_color(window, bg_color);
}

static void in_received_handler(DictionaryIterator *iter, void *context) {
  // call autoconf_in_received_handler
	autoconfig_in_received_handler(iter, context);

	gpath_destroy(hour_arrow);
	if(getHand() == HAND_LINE){
		hour_arrow = gpath_create(&LINE_HAND_POINTS);
	}
	else {
		hour_arrow = gpath_create(&ARROW_HAND_POINTS);
	}

	update_colors();

	layer_mark_dirty(layer);
}

static void bluetooth_connection_handler(bool connected){
	btConnected = connected;
	if(getVibrate_on_bt_lost()){
		vibes_cancel();
		if(connected){
			vibes_long_pulse();
		}
		else {
			vibes_double_pulse();
		}
	}
	layer_mark_dirty(layer);
}

static void init(void) {
	autoconfig_init();

	isAnimating = getAnimated();
	if(isAnimating)
		percent = 0;

	window = window_create();
	
	window_set_window_handlers(window, (WindowHandlers) {
		.load = window_load,
		.unload = window_unload,
	});

	custom_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_COMFORTAA_40));
	small_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_COMFORTAA_18));

	bt_disconnected = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BT_DISCONNECTED);

	if(getHand() == HAND_LINE){
		hour_arrow = gpath_create(&LINE_HAND_POINTS);
	}
	else {
		hour_arrow = gpath_create(&ARROW_HAND_POINTS);
	}

	app_message_register_inbox_received(in_received_handler);

	tick_timer_service_subscribe(MINUTE_UNIT, handle_minute_tick);

	time_t now = time(NULL);
	struct tm *t = localtime(&now);
	handle_minute_tick(t, MINUTE_UNIT);

	btConnected = bluetooth_connection_service_peek();
	bluetooth_connection_service_subscribe(bluetooth_connection_handler);

	update_colors();

	window_stack_push(window, false);
}

static void deinit(void) {
	autoconfig_deinit();
	fonts_unload_custom_font(custom_font);
	fonts_unload_custom_font(small_font);
	gbitmap_destroy(bt_disconnected);
	gpath_destroy(hour_arrow);
	window_destroy(window);
	bluetooth_connection_service_unsubscribe();
}

int main(void) {
	init();
	app_event_loop();
	deinit();
}