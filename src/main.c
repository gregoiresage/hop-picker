#include <pebble.h>
#include "enamel.h"
#include "gbitmap_color_palette_manipulator.h"
#include "health.h"
#include <pebble-events/pebble-events.h>

static Window *window = NULL;
static Layer  *layer = NULL;

static GBitmap *bt_disconnected = NULL;

static GFont custom_font;
static GFont small_font;
static GFont medium_font;

static GPath *hour_arrow;
static const GPathInfo LINE_HAND_POINTS =  {4,(GPoint []) {{-3, 0},{-3, -300},{3, -300},{3, 0}}};
static const GPathInfo BIG_LINE_HAND_POINTS =  {4,(GPoint []) {{-5, 0},{-5, -300},{5, -300},{5, 0}}};
static const GPathInfo ARROW_HAND_POINTS = {4,(GPoint []) {{-9, 0},{-2, -175},{2, -175},{9, 0}}};

static const GPathInfo LINE_HAND_24_POINTS =  {4,(GPoint []) {{-3, 0},{-3, -300-150},{3, -300-150},{3, 0}}};
static const GPathInfo BIG_LINE_HAND_24_POINTS =  {4,(GPoint []) {{-5, 0},{-5, -300-150},{5, -300-150},{5, 0}}};
static const GPathInfo ARROW_HAND_24_POINTS = {4,(GPoint []) {{-9*2, 0},{-2, -175-150},{2, -175-150},{9*2, 0}}};

static char* txt[] = {"0","1","2","3","4","5","6","7","8","9","10","11","12","13","14","15","16","17","18","19","20","21","22","23"};
static char info_text[9] = "00000000";

static Animation* animation;
static AnimationImplementation animImpl;

static bool isAnimating = false;
static uint8_t percent = 100;

static bool btConnected = false;

static GColor bg_color;
static GColor bg_circle_color;
static GColor text_and_dots_color;
static GColor dots_outline_color;

static GColor hand_color;
static GColor hand_outline_color;

static int hours = 0;
static int minutes = 0;
static int day = 0;

static int layer_update_count = 0;

static AppTimer *secondary_display_timer;

#define DATE_POSITION_FROM_CENTER 110
#define SECOND_HAND_LENGTH_A 150
#define SECOND_HAND_LENGTH_B 200
#define SECOND_HAND_LENGTH_C 180

#define SMALL_DOT_RADIUS 3
#define BIG_DOT_RADIUS 6
static const GPathInfo SMALL_LINE_MARK_POINTS =  {4,(GPoint []) {{-3, 2},{-3, -6},{3, -6},{3, 2}}};
static const GPathInfo BIG_LINE_MARK_POINTS   =  {4,(GPoint []) {{-3, 4},{-3, -10},{3, -10},{3, 4}}};

static GPath *small_line_mark_path;
static GPath *big_line_mark_path;

static bool containsCircle(GPoint p, int radius){
	GRect bounds = layer_get_bounds(layer);
#ifdef PBL_ROUND
	GPoint center = grect_center_point(&bounds);
	uint32_t dist_x = center.x - p.x;
	uint32_t dist_y = center.y - p.y;
	uint32_t dist_max = 86 - radius;
	return dist_x * dist_x + dist_y * dist_y < dist_max * dist_max;
#else
	return p.x - radius > 0 && p.x + radius < bounds.size.w && p.y - radius > 0 && p.y + radius < bounds.size.h;
#endif
}

static bool contains_grect(const GRect * other){
#ifdef PBL_ROUND
	GPoint p = other->origin;
	if(!containsCircle(p,0))
		return false;
	p.x += other->size.w;
	if(!containsCircle(p,0))
		return false;
	p.y += other->size.h;
	if(!containsCircle(p,0))
		return false;
	p.x -= other->size.w;
	if(!containsCircle(p,0))
		return false;
	return true;
#else
	GRect bounds = layer_get_bounds(layer);
	GPoint bottom = {other->origin.x + other->size.w, other->origin.y + other->size.h};
	return 
		grect_contains_point(&bounds, &other->origin) && grect_contains_point(&bounds, &bottom);
#endif
}

static void handle_minute_tick(struct tm *tick_time, TimeUnits units_changed) {
	hours = tick_time->tm_hour;
	minutes = tick_time->tm_min;
	day = tick_time->tm_mday;
	if(layer)
		layer_mark_dirty(layer);
}

static void animationUpdate(Animation *animation, const AnimationProgress progress) {
	percent = progress * 100 / ANIMATION_NORMALIZED_MAX;
	layer_mark_dirty(layer);
}

static void animation_started(Animation *animation, void *data) {
	percent = 0;
	isAnimating = true;
}

static void animation_stopped(Animation *animation, bool finished, void *data) {
	percent = 100;
	isAnimating = false;
	layer_mark_dirty(layer);
}

static void drawClock(GPoint center, GContext *ctx){
	GPoint segA;
    GPoint segB; // outer position for second timezone
	GPoint segC;

	GRect bounds = layer_get_bounds(layer);

	uint16_t segA_length = enamel_get_full_hour_mode() ? SECOND_HAND_LENGTH_A + 150 : SECOND_HAND_LENGTH_A;
	uint16_t segB_length = enamel_get_full_hour_mode() ? SECOND_HAND_LENGTH_B + 150 : SECOND_HAND_LENGTH_B;
	uint16_t segC_length = enamel_get_full_hour_mode() ? SECOND_HAND_LENGTH_C + 150 : SECOND_HAND_LENGTH_C;
	
	graphics_context_set_fill_color(ctx, bg_circle_color);
	graphics_fill_circle(ctx, center, segC_length);

	graphics_context_set_text_color(ctx, text_and_dots_color);
	graphics_context_set_fill_color(ctx, text_and_dots_color);
	graphics_context_set_stroke_color(ctx, dots_outline_color);
 
	int minhour = hours + 24 - 2;
	int maxhour = hours + 24 + 3;

	int mark_space = 1;
	switch(enamel_get_mark_space()){
		case MARK_SPACE_10 : mark_space = 6; break;
		case MARK_SPACE_15 : mark_space = 4; break;
		case MARK_SPACE_30 : mark_space = 2; break;
		case MARK_SPACE_60 : mark_space = 1; break;
	}
	
	for(int i=minhour*mark_space; i<maxhour*mark_space; i++){
		int32_t angle = 
			enamel_get_full_hour_mode() ?
				TRIG_MAX_ANGLE * (i % (24*mark_space)) / (24*mark_space):
				TRIG_MAX_ANGLE * (i % (12*mark_space)) / (12*mark_space);
				;
		if(enamel_get_full_hour_mode()){
			angle += TRIG_MAX_ANGLE / 2;
			angle = angle % TRIG_MAX_ANGLE;
		}

		if(isAnimating){
			segC.y = (int16_t)((-cos_lookup(angle) * segC_length / TRIG_MAX_RATIO) + center.y - bounds.size.h/2) * percent / 100 + bounds.size.h/2;
			segC.x = (int16_t)((sin_lookup(angle) * segC_length / TRIG_MAX_RATIO) + center.x - bounds.size.w/2) * percent / 100 + bounds.size.w/2;
			segA.x = segA.y = 0;
			segB.x = segB.y = 0;
		}
		else {
			segA.y = (int16_t)(-cos_lookup(angle) * segA_length / TRIG_MAX_RATIO) + center.y;
			segA.x = (int16_t)(sin_lookup(angle) * segA_length / TRIG_MAX_RATIO) + center.x;
            segB.y = (int16_t)(-cos_lookup(angle) * segB_length / TRIG_MAX_RATIO) + center.y;
            segB.x = (int16_t)(sin_lookup(angle) * segB_length / TRIG_MAX_RATIO) + center.x;
			segC.y = (int16_t)(-cos_lookup(angle) * segC_length / TRIG_MAX_RATIO) + center.y;
			segC.x = (int16_t)(sin_lookup(angle) * segC_length / TRIG_MAX_RATIO) + center.x;
		}
		
		bool hour_mark = (i % mark_space) == 0;
		uint8_t radius = hour_mark ? BIG_DOT_RADIUS : SMALL_DOT_RADIUS ;

		if(enamel_get_mark_style() == MARK_STYLE_DOTS) {
			graphics_fill_circle(ctx, segC, radius);
			graphics_draw_circle(ctx, segC, radius);
		}
		else {
			GPath* mark_path = hour_mark ? big_line_mark_path : small_line_mark_path;
			gpath_move_to(mark_path, segC);
			gpath_rotate_to(mark_path, angle);
			gpath_draw_filled(ctx, mark_path);
			// don't draw the outline for 'monochrome' themes
			if(enamel_get_color_theme() == COLOR_THEME_BLACK_ON_WHITE || enamel_get_color_theme() == COLOR_THEME_WHITE_ON_BLACK)
				gpath_draw_outline(ctx, mark_path);
		}
		
		if(!isAnimating && hour_mark) {	
			if((clock_is_24h_style() && enamel_get_force_hour_format() != FORCE_HOUR_FORMAT_12_H)
				|| enamel_get_force_hour_format() == FORCE_HOUR_FORMAT_24_H
				|| enamel_get_full_hour_mode()){
				graphics_draw_text(ctx,
					txt[(i % (24*mark_space))/mark_space],
					custom_font,
					GRect(segA.x-25, segA.y-25, 50, 50),
					GTextOverflowModeWordWrap,
					GTextAlignmentCenter,
					NULL);
                if(enamel_get_second_tz_offset() != 0) {
                    graphics_draw_text(ctx,
                        txt[((i + enamel_get_second_tz_offset() * mark_space) % (24*mark_space))/mark_space],
                        small_font,
                        GRect(segB.x-11, segB.y-11, 22, 22),
                        GTextOverflowModeWordWrap,
                        GTextAlignmentCenter,
                        NULL);
                }
			}
			else {
				graphics_draw_text(ctx,
					(i % (12*mark_space))/mark_space == 0 ? "12" : txt[(i % (12*mark_space))/mark_space],
					custom_font,
					GRect(segA.x-25, segA.y-25, 50, 50),
					GTextOverflowModeWordWrap,
					GTextAlignmentCenter,
					NULL);
                if(enamel_get_second_tz_offset() != 0) {
                    graphics_draw_text(ctx,
                        ((i + enamel_get_second_tz_offset() * mark_space) % (12*mark_space))/mark_space == 0 ? "12" : txt[((i + enamel_get_second_tz_offset() * mark_space) % (12*mark_space))/mark_space],
                        small_font,
                        GRect(segB.x-11, segB.y-11, 22, 22),
                        GTextOverflowModeWordWrap,
                        GTextAlignmentCenter,
                        NULL);
                }
			}
		}
	}
}

static void drawInfo(GPoint center, int angle, GContext *ctx, char* text){
	GPoint segA;

	uint16_t outer_radius = 0;
	uint16_t inner_radius = 0;
	GFont font = small_font;

	switch(enamel_get_info_text_size()){
		case INFO_TEXT_SIZE_DEFAULT : 
			outer_radius = 17;
			inner_radius = 14;
			font = small_font;
			break;
		case INFO_TEXT_SIZE_LARGER : 
			outer_radius = 23;
			inner_radius = 20;
			font = medium_font;
			break;
	}

	GRect text_bounds = (GRect){.origin={0,0},.size={120,120}};
	GSize text_size = graphics_text_layout_get_content_size(text,font,text_bounds,GTextOverflowModeWordWrap,GTextAlignmentCenter);

	bool drawCircle = strlen(text) < 3;

	int32_t posFromCenter = enamel_get_full_hour_mode() ? DATE_POSITION_FROM_CENTER + 150 : DATE_POSITION_FROM_CENTER;
	GRect outside = grect_inset(text_bounds, GEdgeInsets4(-6, -8, -8, -8));
	text_bounds.size = text_size;

	do{
		segA.y = (int16_t)(-cos_lookup(angle) * posFromCenter / TRIG_MAX_RATIO) + center.y;
		segA.x = (int16_t)(sin_lookup(angle) * posFromCenter / TRIG_MAX_RATIO) + center.x;
		posFromCenter--;

		text_bounds.origin = segA;
		text_bounds.origin.x -= text_size.w/2;
		text_bounds.origin.y -= text_size.h/2;
		
		outside = grect_inset(text_bounds, GEdgeInsets4(-6, -8, -8, -8));
	}
	while((drawCircle && containsCircle(segA, outer_radius + 3)) || (!drawCircle && contains_grect(&outside)));

	graphics_context_set_text_color(ctx, text_and_dots_color);
	graphics_context_set_stroke_color(ctx, hand_outline_color);
	graphics_context_set_fill_color(ctx, hand_color);

	if(drawCircle){
		graphics_fill_circle(ctx, segA, outer_radius);
		if(!gcolor_equal(hand_outline_color,GColorClear)){
			graphics_draw_circle(ctx, segA, outer_radius);
		}
	}
	else
		graphics_fill_rect(ctx, grect_inset(text_bounds, GEdgeInsets4(-6, -8, -8, -8)), 8, GCornersAll);
	
	
	graphics_context_set_fill_color(ctx, bg_circle_color);

	if(drawCircle){
		graphics_fill_circle(ctx, segA, inner_radius);
		if(!gcolor_equal(hand_outline_color,GColorClear)){
			graphics_draw_circle(ctx, segA, inner_radius);
		}
	}
	else
		graphics_fill_rect(ctx, grect_inset(text_bounds, GEdgeInsets4(-3, -5, -5, -5)), 8, GCornersAll);
	
	
	graphics_draw_text(ctx,
					text,
					font,
					text_bounds,
					GTextOverflowModeWordWrap,
					GTextAlignmentCenter,
					NULL);
}

static void drawHand(GPoint center, int angle, GContext *ctx){
	
	graphics_context_set_fill_color(ctx, hand_color);
	
	gpath_move_to(hour_arrow, center);
	gpath_rotate_to(hour_arrow, angle);
	gpath_draw_filled(ctx, hour_arrow);

	if(!gcolor_equal(hand_outline_color,GColorClear)){
		graphics_context_set_stroke_color(ctx, hand_outline_color);
		gpath_draw_outline(ctx, hour_arrow);
	}
}

static void layer_update_proc(Layer *layer, GContext *ctx) {
	GRect bounds = layer_get_bounds(layer);
	GPoint center = grect_center_point(&bounds);

	if(isAnimating){
		if(layer_update_count == 0){
			layer_mark_dirty(layer);
		}
		else if(layer_update_count == 1){
			animation = animation_create();
			animImpl.update = animationUpdate;
			animation_set_handlers(animation, (AnimationHandlers) {
				.started = (AnimationStartedHandler) animation_started,
				.stopped = (AnimationStoppedHandler) animation_stopped,
			}, NULL);
			animation_set_duration(animation, 500);
			animation_set_implementation(animation, &animImpl);
			animation_schedule(animation);
		}
		layer_update_count++;
	}
	
	int32_t angle = 
		enamel_get_full_hour_mode() ?
			TRIG_MAX_ANGLE * ((hours % 24) * 60 + minutes) / (24 * 60):
			TRIG_MAX_ANGLE * ((hours % 12) * 60 + minutes) / (12 * 60);
			
	if(enamel_get_full_hour_mode()){
		angle += TRIG_MAX_ANGLE / 2;
		angle = angle % TRIG_MAX_ANGLE;
	}

	uint16_t segA_length = enamel_get_full_hour_mode() ? SECOND_HAND_LENGTH_A + 150 : SECOND_HAND_LENGTH_A;

	GPoint centerClock;
	centerClock.y = (int16_t)(cos_lookup(angle) * segA_length / TRIG_MAX_RATIO) + center.y;
	centerClock.x = (int16_t)(-sin_lookup(angle) * segA_length / TRIG_MAX_RATIO) + center.x;

	drawClock(centerClock, ctx);

	if(!isAnimating){
		drawHand(centerClock, angle, ctx);

		if(enamel_get_display_bt_icon() && !btConnected){
  			graphics_context_set_compositing_mode(ctx,GCompOpSet);

#ifdef PBL_ROUND
  			GPoint btPos;
  			uint16_t radiusFromCenter = (enamel_get_full_hour_mode() ? SECOND_HAND_LENGTH_C + 150 : SECOND_HAND_LENGTH_C) + 35;
  			btPos.y = (int16_t)(-cos_lookup(angle) * radiusFromCenter / TRIG_MAX_RATIO) + centerClock.y;
			btPos.x = (int16_t)(sin_lookup(angle) * radiusFromCenter / TRIG_MAX_RATIO) + centerClock.x;
  			graphics_draw_bitmap_in_rect(ctx, bt_disconnected, GRect(btPos.x-16,btPos.y-16,32,32));
#else
  			if(enamel_get_full_hour_mode()){
				if(hours < 6)
					graphics_draw_bitmap_in_rect(ctx, bt_disconnected, GRect(0,160-32,32,32));
				else if(hours < 12)
					graphics_draw_bitmap_in_rect(ctx, bt_disconnected, GRect(0,0,32,32));
				else if(hours < 18)
					graphics_draw_bitmap_in_rect(ctx, bt_disconnected, GRect(bounds.size.w-32,0,32,32));
				else 
					graphics_draw_bitmap_in_rect(ctx, bt_disconnected, GRect(bounds.size.w-32,160-32,32,32));
			}
  			else {
				if(hours % 12 < 3)
					graphics_draw_bitmap_in_rect(ctx, bt_disconnected, GRect(bounds.size.w-32,0,32,32));
				else if(hours % 12 < 6)
					graphics_draw_bitmap_in_rect(ctx, bt_disconnected, GRect(bounds.size.w-32,160-32,32,32));
				else if(hours % 12 < 9)
					graphics_draw_bitmap_in_rect(ctx, bt_disconnected, GRect(0,160-32,32,32));
				else 
					graphics_draw_bitmap_in_rect(ctx, bt_disconnected, GRect(0,0,32,32));
  			}
#endif
		}

		if(secondary_display_timer != NULL && enamel_get_secondary_display() != SECONDARY_DISPLAY_NOTHING){
			switch(enamel_get_secondary_display()) {
				case SECONDARY_DISPLAY_DATE :
					snprintf(info_text, sizeof(info_text), "%d", day); 
					break;
				case SECONDARY_DISPLAY_MINUTES :
					snprintf(info_text, sizeof(info_text), "%02d", minutes); 
					break;
				case SECONDARY_DISPLAY_BATTERY :
					snprintf(info_text, sizeof(info_text), "%02d%%", battery_state_service_peek().charge_percent); 
					break;
				case SECONDARY_DISPLAY_STEP_COUNT :
					snprintf(info_text, sizeof(info_text), "%d", health_get_metric_sum(HealthMetricStepCount)); 
					break;
				// case SECONDARY_DISPLAY_PERCENTDAILYGOAL : ;
				// 	int steps = health_get_metric_sum(HealthMetricStepCount);
				// 	snprintf(info_text, sizeof(info_text), "%02ld%%", steps * 100 / getStep_day_goal()); 
				// 	break;
				default :
					break;
			}
			drawInfo(centerClock, angle, ctx, info_text);
		}
		else if(enamel_get_info_display() != INFO_DISPLAY_NOTHING){
			switch(enamel_get_info_display()) {
				case INFO_DISPLAY_DATE :
					snprintf(info_text, sizeof(info_text), "%d", day); 
					break;
				case INFO_DISPLAY_MINUTES :
					snprintf(info_text, sizeof(info_text), "%02d", minutes); 
					break;
				case INFO_DISPLAY_BATTERY :
					snprintf(info_text, sizeof(info_text), "%02d%%", battery_state_service_peek().charge_percent); 
					break;
				case INFO_DISPLAY_STEP_COUNT :
					snprintf(info_text, sizeof(info_text), "%d", health_get_metric_sum(HealthMetricStepCount)); 
					break;
				// case INFO_DISPLAY_PERCENTDAILYGOAL : ;
				// 	int steps = health_get_metric_sum(HealthMetricStepCount);
				// 	snprintf(info_text, sizeof(info_text), "%02ld%%", steps * 100 / getStep_day_goal()); 
				// 	break;
				default :
					break;
			}
			drawInfo(centerClock, angle, ctx, info_text);
		}
	}	
}

static void window_load(Window *window) {
	Layer *window_layer = window_get_root_layer(window);
	GRect bounds = layer_get_bounds(window_layer);
	
	layer = layer_create(bounds);
	layer_set_update_proc(layer, layer_update_proc);
	layer_add_child(window_layer, layer);
}

static void window_unload(Window *window) {
	layer_destroy(layer);
}

static void secondary_timer_cb(void *data) {
	secondary_display_timer = NULL;
	layer_mark_dirty(layer);
}

static void tap_handler(AccelAxisType axis, int32_t direction) {
	if(enamel_get_secondary_display() != SECONDARY_DISPLAY_NOTHING){
		secondary_display_timer = app_timer_register(9 * 1000, secondary_timer_cb, NULL);
		layer_mark_dirty(layer);
	}
}

static void updateSettings(){
	gpath_destroy(hour_arrow);
	switch(enamel_get_hand()){
		case HAND_LINE : hour_arrow = gpath_create(enamel_get_full_hour_mode() ? &LINE_HAND_24_POINTS : &LINE_HAND_POINTS); break;
		case HAND_BIG_LINE : hour_arrow = gpath_create(enamel_get_full_hour_mode() ? &BIG_LINE_HAND_24_POINTS : &BIG_LINE_HAND_POINTS); break;
		case HAND_ARROW : 
		default : hour_arrow = gpath_create(enamel_get_full_hour_mode() ? &ARROW_HAND_24_POINTS : &ARROW_HAND_POINTS); break;
	}

	switch(enamel_get_color_theme()){
		case COLOR_THEME_DARK : 
			bg_color = GColorBlack;
			bg_circle_color = GColorBlack;
			text_and_dots_color = GColorWhite;
			dots_outline_color = GColorWhite;
			hand_color = GColorWhite;
			break;
		case COLOR_THEME_CLEAR :
			bg_color = GColorWhite;
			bg_circle_color = GColorWhite;
			text_and_dots_color = GColorBlack;
			dots_outline_color = GColorBlack;
			hand_color = GColorBlack;
			break;
		case COLOR_THEME_BLACK_ON_WHITE :
			bg_color = GColorWhite;
			bg_circle_color = GColorBlack;
			text_and_dots_color = GColorWhite;
			dots_outline_color = GColorBlack;
			hand_color = GColorWhite;
			break;
		case COLOR_THEME_WHITE_ON_BLACK :
			bg_color = GColorBlack;
			bg_circle_color = GColorWhite;
			text_and_dots_color = GColorBlack;
			dots_outline_color = GColorWhite;
			hand_color = GColorBlack;
			break;
	}

#ifdef PBL_COLOR
	hand_color = enamel_get_hand_color();
#endif
	hand_outline_color = GColorClear;
	if(gcolor_equal(bg_color,hand_color) || gcolor_equal(bg_circle_color,hand_color))
		hand_outline_color = gcolor_equal(hand_color,GColorWhite) ? GColorBlack : GColorWhite;

	if(bt_disconnected)
		gbitmap_destroy(bt_disconnected);
	bt_disconnected = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BT_DISCONNECTED);
	if(dots_outline_color.argb == GColorWhite.argb) {
		replace_gbitmap_color(GColorBlack, GColorRed, bt_disconnected);
		replace_gbitmap_color(GColorWhite, GColorBlack, bt_disconnected);
		replace_gbitmap_color(GColorRed, GColorWhite, bt_disconnected);
	}

	switch(enamel_get_secondary_display()) {
		case SECONDARY_DISPLAY_NOTHING :
			accel_tap_service_unsubscribe();
			break;
		default :
			accel_tap_service_subscribe(tap_handler);
			break;
	}

	window_set_background_color(window, bg_color);
}

static void enamel_register_settings_received_cb() {
	updateSettings();
	layer_mark_dirty(layer);
}

static void bluetooth_connection_handler(bool connected){
	btConnected = connected;
	if(enamel_get_vibrate_on_bt_lost()){
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
	enamel_init();
	enamel_register_settings_received(enamel_register_settings_received_cb);

	events_app_message_open();

	window = window_create();
	window_set_window_handlers(window, (WindowHandlers) {
		.load = window_load,
		.unload = window_unload,
	});

	custom_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_COMFORTAA_40));
	small_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_COMFORTAA_18));
	medium_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_COMFORTAA_26));
	bt_disconnected = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BT_DISCONNECTED);

	small_line_mark_path = gpath_create(&SMALL_LINE_MARK_POINTS);
	big_line_mark_path = gpath_create(&BIG_LINE_MARK_POINTS);

	btConnected = bluetooth_connection_service_peek();
	bluetooth_connection_service_subscribe(bluetooth_connection_handler);

	tick_timer_service_subscribe(MINUTE_UNIT, handle_minute_tick);

	time_t now = time(NULL);
	struct tm *t = localtime(&now);
	handle_minute_tick(t, MINUTE_UNIT);

	updateSettings();

	isAnimating = enamel_get_animated();
	if(isAnimating)
		percent = 0;

	window_stack_push(window, false);
}

static void deinit(void) {
	enamel_deinit();
	fonts_unload_custom_font(custom_font);
	fonts_unload_custom_font(small_font);
	fonts_unload_custom_font(medium_font);
	gbitmap_destroy(bt_disconnected);
	gpath_destroy(hour_arrow);
	gpath_destroy(small_line_mark_path);
	gpath_destroy(big_line_mark_path);
	window_destroy(window);
	bluetooth_connection_service_unsubscribe();
}

int main(void) {
	init();
	app_event_loop();
	deinit();
}
