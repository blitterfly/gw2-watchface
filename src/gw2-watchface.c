#include <pebble.h>

#define KEY_TWEET 0

static Window * s_main_window;
static TextLayer * s_time_layer;
static GFont s_time_font;
static BitmapLayer * s_background_layer;
static GBitmap * s_background_bitmap;

static void update_time() {
	// Get a tm structure
	time_t temp = time(NULL); 
	struct tm * tick_time = localtime(&temp);
	
	// Create a long-lived buffer
	static char buffer[] = "00:00";
	
	// Write the current hours and minutes into the buffer
	if (clock_is_24h_style() == true) {
		// Use 24 hour format
		strftime(buffer, sizeof("00:00"), "%k:%M", tick_time);
	} else {
		// Use 12 hour format
		strftime(buffer, sizeof("00:00"), "%l:%M", tick_time);
	}
	
	// Display this time on the TextLayer
	text_layer_set_text(s_time_layer, buffer[0] == ' ' ? (buffer + 1) : buffer);
}

static void tick_handler(struct tm * tick_time, TimeUnits units_changed) {
	update_time();
	
	// Get latest tweet update every 30 minutes
	/*if(tick_time->tm_min % 30 == 0) {
		// Begin dictionary
		DictionaryIterator * iter;
		app_message_outbox_begin(&iter);
		
		// Add a key-value pair
		dict_write_uint8(iter, 0, 0);
		
		// Send the message!
		app_message_outbox_send();
	}*/
}

static void main_window_load(Window * window) {
	// Create GBitmap, then set to created BitmapLayer
	s_background_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BACKGROUND);
	s_background_layer = bitmap_layer_create(GRect(0, 0, 144, 168));
	bitmap_layer_set_bitmap(s_background_layer, s_background_bitmap);
	layer_add_child(window_get_root_layer(window), bitmap_layer_get_layer(s_background_layer));
	
	// Create time TextLayer
	s_time_layer = text_layer_create(GRect(5, 0, 134, 45));
	text_layer_set_background_color(s_time_layer, GColorClear);
	text_layer_set_text_color(s_time_layer, GColorWhite);
	text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);
		
	// Create GFont
	s_time_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_CRONOSPRO_44));
	
	// Improve the layout to be more like a watchface
	text_layer_set_font(s_time_layer, s_time_font);
	text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);
	
	// Add it as a child layer to the Window's root layer
	layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_time_layer));
	
	// Register with TickTimerService
	tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
	
}

static void main_window_unload(Window * window) {
	// Destroy TextLayer
	text_layer_destroy(s_time_layer);
	
	// Destroy GBitmap
	gbitmap_destroy(s_background_bitmap);
	
	// Destroy BitmapLayer
	bitmap_layer_destroy(s_background_layer);
	
	// Unload GFont
	fonts_unload_custom_font(s_time_font);
	
}

static void inbox_received_callback(DictionaryIterator * iterator, void * context) {
	// Store incoming information
	/*static char tweet_buffer[80];
	
	// Read first item
	Tuple * t = dict_read_first(iterator);
	
	// For all items
	while(t != NULL) {
		// Which key was received?
		switch(t->key) {
			case KEY_TWEET:
				snprintf(tweet_buffer, sizeof(tweet_buffer), "%.76s...", t->value->cstring);
				break;
			default:
				APP_LOG(APP_LOG_LEVEL_ERROR, "Key %d not recognized!", (int)t->key);
				break;
		}
		
		// Look for next item
		t = dict_read_next(iterator);
	}
	
	// Assemble full string and display
	text_layer_set_text(s_tweet_layer, tweet_buffer);*/
}

static void inbox_dropped_callback(AppMessageResult reason, void *context) {
	APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped!");
}

static void outbox_failed_callback(DictionaryIterator *iterator, AppMessageResult reason, void *context) {
	APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox send failed!");
}

static void outbox_sent_callback(DictionaryIterator *iterator, void *context) {
	APP_LOG(APP_LOG_LEVEL_INFO, "Outbox send success!");
}

static void init(void) {
	// Create main Window element and assign to pointer
	s_main_window = window_create();
	
	// Set handlers to manage the elements inside the Window
	window_set_window_handlers(s_main_window, (WindowHandlers) {
		.load = main_window_load,
		.unload = main_window_unload
	});
	
	// Show the Window on the watch, with animated=true
	window_stack_push(s_main_window, true);
	
	// Make sure the time is displayed from the start
	update_time();
	
	// Register callbacks
	/*app_message_register_inbox_received(inbox_received_callback);
	app_message_register_inbox_dropped(inbox_dropped_callback);
	app_message_register_outbox_failed(outbox_failed_callback);
	app_message_register_outbox_sent(outbox_sent_callback);
	
	// Open AppMessage
	app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum());*/
}

static void deinit(void) {
	// Destroy Window
	window_destroy(s_main_window);
}

int main(void) {
	init();
	app_event_loop();
	deinit();
}
