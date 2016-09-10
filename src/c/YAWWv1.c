#include <pebble.h>
#define DEBUG 0
#define KEY_SUNRISE 0
#define KEY_SUNSET 1
#define KEY_TEMPERATURE 2
#define KEY_MAX 3
#define KEY_MIN 4
#define KEY_WIND 5
#define KEY_HUMIDITY 6
#define KEY_CLOUDS 7
#define KEY_FAILED_COUNT 8
#define KEY_WUPDATED_TIME 9
#define KEY_CONDITION 10
#define KEY_WUPDATED_TIME_STRING 20
#define KEY_CONNECTION_STATUS 21
#define KEY_DATA 30
#define KEY_TIMEOUT 99

static Window *s_main_window; // Main window
static TextLayer *s_bt_status_layer; // BT status layer
static TextLayer *s_battery_status_layer;	// Battery status layer
static TextLayer *s_time_layer;	// Time layer
static TextLayer *s_sun_times_layer; // Sunrise and sunset layer
static TextLayer *s_today_date_layer; // Date layer
static TextLayer *s_today_date_underscore_layer; // Underscore below the date
static TextLayer *s_wupdated_layer; // When was the weather last updated
static TextLayer *s_temperature_layer;	// Weather layer
static TextLayer *s_high_low_layer; // High-low temperature layer
static TextLayer *s_whc_layer; // Wind, Humidity, Cloudiness layer
static TextLayer *s_condition_layer; // Condition layer
static TextLayer *s_calendar_back_layer; // Calendar back layer
static TextLayer *s_calendar_days_layer[7]; // Calendar days layer

//static int s_fail_retry = 0; // How many times retried get_weather() if it failed to send app_msg
//static int s_timeouts = 0; // How many timeouts happened while trying to get current weather
static char s_wupdated_time_string[] = "Never.   "; // Time stamp (string) of when the weather was last updated
static bool s_connection_status = true; // Connection status the last time it was checked
static char s_condition[] = "Nothing yet."; // Weather condition string
static long data[] = {0,0,0,0,0,0,0,0,1,0}; // sunrise, sunset, s_temperature, s_max, s_min, s_wind, s_humidity, s_clouds,s_failed_count, s_wupdated_time

// "Settings"
int VIBRATE_C = 1; // Vibrate on connect
int VIBRATE_DC = 1;	// Vibrate on disconnect
int VIBRATE_ERR = 0; // Vibrate on weather fetch error
int VIBRATE_CHIME = 0; // Vibrate every X minutes
int TEMPERATURE_C = 1; // Show temperature in celsius
int WIND_S = 0; // Wind speed , 0 - m/s, 1 - k/h , 2 - f/s , 4 - m/h
int WEATHER_UPDATE_INTERVAL = 30; // How often to update the weather in minutes 

void update_bt_status(bool status); // Update bluetooth connection status 
bool check_bt_status(); // Checks wheter we have a connection via update_bt_status and returns the status
void update_battery_status(BatteryChargeState charge_state); // Update battery level and status
void update_time(); // Update the time
void update_sun_times(long sunrise,long sunset); // Update sunrise and sunset 
void update_date(); // Update the date line
void inbox_received_callback(DictionaryIterator *iterator, void *context); // Self explanatory
void inbox_dropped_callback(AppMessageResult reason, void *context); // Self explanatory
void outbox_failed_callback(DictionaryIterator *iterator, AppMessageResult reason, void *context); // Self explanatory
void outbox_sent_callback(DictionaryIterator *iterator, void *context); // Self explanatory
void update_weather(); // Request to update the weather
void update_temperature(int temp); // Update the temperature
void update_high_low(int high,int low); // Update the high and low temperatures
void update_whc(int wind,int humidity, int clouds); // Update wind speed, humidity %, clouds %
void update_condition(char condition[]); // Update weather condition
void update_calendar(); // Update the calendar
void tick_handler(struct tm *tick_time, TimeUnits units_changed); // Handle minute ticks
void main_window_load(Window *window); // Load window with layers
void main_window_unload(Window *window); // Unload window of layers

static void init(){
	// Create main Window element and assign to pointer
	s_main_window = window_create();
	window_set_background_color(s_main_window, GColorBlack);
	// Set handlers to manage the elements inside the Window
	window_set_window_handlers(s_main_window, (WindowHandlers) {.load = main_window_load, .unload = main_window_unload});
	// Show the Window on the watch, with animated=false
	window_stack_push(s_main_window, false);
	tick_timer_service_subscribe(MINUTE_UNIT, tick_handler); // Register with TickTimerService
	bluetooth_connection_service_subscribe(update_bt_status); // Register with BT Connection Service
	battery_state_service_subscribe(update_battery_status); // Register with Battery State Service
	app_message_register_inbox_received(inbox_received_callback); // Self explanatory
	app_message_register_inbox_dropped(inbox_dropped_callback); // Self explanatory
	app_message_register_outbox_failed(outbox_failed_callback); // Self explanatory
	app_message_register_outbox_sent(outbox_sent_callback); // Self explanatory
	app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum()); // Self explanatory?
	
	
	if(persist_exists(KEY_CONNECTION_STATUS)){ // Restore BT connection state if exists
		s_connection_status = persist_read_bool(KEY_CONNECTION_STATUS);
	}
	// Make sure everything is displayed from the start
	check_bt_status();
	update_battery_status(battery_state_service_peek());
	update_time();
	update_date();
	update_calendar();

	if(persist_exists(KEY_DATA)){
		persist_read_data(KEY_DATA, data, sizeof(data));
		persist_read_string(KEY_CONDITION, s_condition, sizeof(s_condition));
		persist_read_string(KEY_WUPDATED_TIME_STRING, s_wupdated_time_string,sizeof(s_wupdated_time_string));
	}

	update_sun_times(data[KEY_SUNRISE], data[KEY_SUNSET]);
	update_temperature(data[KEY_TEMPERATURE]);
	update_high_low(data[KEY_MAX], data[KEY_MIN]);
	update_whc(data[KEY_WIND], data[KEY_HUMIDITY], data[KEY_CLOUDS]);
	update_condition(s_condition);
	text_layer_set_text(s_wupdated_layer, s_wupdated_time_string);
	
	long temp_time = time(NULL); // Used to check how long since last weather update
	// If weather update failed 5 times in a previous instance or if it is longer than update interval
	if((data[KEY_FAILED_COUNT] != 0) || ((temp_time - data[KEY_WUPDATED_TIME]) >= (WEATHER_UPDATE_INTERVAL*60))){
		// Check if we have BT connection 
		if(check_bt_status()){
			data[KEY_FAILED_COUNT] = 1;
			update_weather();
		} else{
//			if(data[KEY_FAILED_COUNT] == -1){
//				data[KEY_FAILED_COUNT] = 1;
//			} else{
				data[KEY_FAILED_COUNT]++;
//			}
//			text_layer_set_background_color(s_wupdated_layer, GColorWhite);
//			text_layer_set_text_color(s_wupdated_layer, GColorBlack);
		}
	}
	light_enable_interaction(); // Turn on the light when the watchface is started

}
static void deinit(){
//	persist_write_data(KEY_DATA, data, sizeof(data));
	persist_write_bool(KEY_CONNECTION_STATUS, s_connection_status);
	battery_state_service_unsubscribe();
	bluetooth_connection_service_unsubscribe();
	tick_timer_service_unsubscribe();
	window_destroy(s_main_window);
}

int main(void){
	init();
	app_event_loop();
	deinit();
}

void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
	struct tm tick = *tick_time;
	update_time(tick_time); 

	// Check if it's midnight to update the date and calendar
	if((tick.tm_hour==0) && (tick.tm_min==0)){
		update_date();
		update_calendar();
	}
	
	// Check if needs to update weather
	if((WEATHER_UPDATE_INTERVAL>0) && (((tick.tm_min%WEATHER_UPDATE_INTERVAL)==0) || (data[KEY_FAILED_COUNT] != 0))){
		// Check if we have a connection to update the weather
		if(check_bt_status() ){
			update_weather();
		} else{
			data[KEY_FAILED_COUNT]++;
		}
/*
		if(data[KEY_FAILED_COUNT] == 5){
			text_layer_set_background_color(s_wupdated_layer, GColorWhite);
			text_layer_set_text_color(s_wupdated_layer, GColorBlack);
			text_layer_set_text(s_wupdated_layer, s_wupdated_time_string);
		}
*/
 	}
}

void main_window_load(Window *window){

	// BT status layer
	s_bt_status_layer = text_layer_create(GRect(0, 0, 50, 15));
	text_layer_set_background_color(s_bt_status_layer, GColorClear);
	text_layer_set_text_color(s_bt_status_layer, GColorWhite);
	text_layer_set_font(s_bt_status_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD));
	text_layer_set_text_alignment(s_bt_status_layer, GTextAlignmentLeft);
	layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_bt_status_layer));
	
	// Battery status layer
	s_battery_status_layer = text_layer_create(GRect(100, 0, 44, 15));
	text_layer_set_background_color(s_battery_status_layer, GColorClear);
	text_layer_set_text_color(s_battery_status_layer, GColorWhite);
	text_layer_set_font(s_battery_status_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
	text_layer_set_text_alignment(s_battery_status_layer, GTextAlignmentRight);
	layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_battery_status_layer));
	
	// Time layer
	s_time_layer = text_layer_create(GRect(0, 15, 100, 30));
	text_layer_set_background_color(s_time_layer, GColorClear);
	text_layer_set_text_color(s_time_layer, GColorWhite);
	text_layer_set_font(s_time_layer, fonts_get_system_font(FONT_KEY_BITHAM_30_BLACK));
	text_layer_set_text_alignment(s_time_layer, GTextAlignmentLeft);
	layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_time_layer));
	
	// Sun layer
	s_sun_times_layer = text_layer_create(GRect(100, 15, 44, 30));
	text_layer_set_background_color(s_sun_times_layer, GColorClear);
	text_layer_set_text_color(s_sun_times_layer, GColorWhite);
	text_layer_set_font(s_sun_times_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
	text_layer_set_text_alignment(s_sun_times_layer, GTextAlignmentLeft);
	layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_sun_times_layer));
	
	// Date layer
	s_today_date_layer = text_layer_create(GRect(0, 45, 110, 18));
	text_layer_set_background_color(s_today_date_layer, GColorClear);
	text_layer_set_text_color(s_today_date_layer, GColorWhite);
	text_layer_set_font(s_today_date_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
	text_layer_set_text_alignment(s_today_date_layer, GTextAlignmentLeft);
	layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_today_date_layer));
	
	//Underscore layer
	s_today_date_underscore_layer = text_layer_create(GRect(0, 63, 100, 2));
	text_layer_set_background_color(s_today_date_underscore_layer, GColorWhite);
	layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_today_date_underscore_layer));
	
	// Weather updated layer
	s_wupdated_layer = text_layer_create(GRect(115, 50, 29, 15));
	text_layer_set_background_color(s_wupdated_layer, GColorClear);
	text_layer_set_text_color(s_wupdated_layer, GColorWhite);
	text_layer_set_font(s_wupdated_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
	text_layer_set_text_alignment(s_wupdated_layer, GTextAlignmentRight);
	layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_wupdated_layer));

	// Temperature layer
	s_temperature_layer = text_layer_create(GRect(0, 65, 44, 30));
	text_layer_set_background_color(s_temperature_layer, GColorClear);
	text_layer_set_text_color(s_temperature_layer, GColorWhite);
	text_layer_set_font(s_temperature_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
	text_layer_set_text_alignment(s_temperature_layer, GTextAlignmentLeft);
	layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_temperature_layer));

	// High-low layer
	s_high_low_layer = text_layer_create(GRect(45, 65, 54, 30));
	text_layer_set_background_color(s_high_low_layer, GColorClear);
	text_layer_set_text_color(s_high_low_layer, GColorWhite);
	text_layer_set_font(s_high_low_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
	text_layer_set_text_alignment(s_high_low_layer, GTextAlignmentLeft);
	layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_high_low_layer));
	
	// whc layer
	s_whc_layer = text_layer_create(GRect(100, 65, 44, 45));
	text_layer_set_background_color(s_whc_layer, GColorClear);
	text_layer_set_text_color(s_whc_layer, GColorWhite);
	text_layer_set_font(s_whc_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
	text_layer_set_text_alignment(s_whc_layer, GTextAlignmentLeft);
	layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_whc_layer));
	
	// Condition layer
	s_condition_layer = text_layer_create(GRect(5, 100, 95, 20));
	text_layer_set_background_color(s_condition_layer, GColorClear);
	text_layer_set_text_color(s_condition_layer, GColorWhite);
	text_layer_set_font(s_condition_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD));
	text_layer_set_text_alignment(s_condition_layer, GTextAlignmentLeft);
	layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_condition_layer));
	
	// Calendar back layer
	s_calendar_back_layer = text_layer_create(GRect(0, 121, 144, 47));
	text_layer_set_background_color(s_calendar_back_layer, GColorWhite);
	layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_calendar_back_layer)); 
	
	// Calendar days layers
	for(int i=0;i<7;i++){
		s_calendar_days_layer[i] = text_layer_create(GRect(9+18*i, 121, 18, 47));
		text_layer_set_font(s_calendar_days_layer[i], fonts_get_system_font(FONT_KEY_GOTHIC_14));
		text_layer_set_text_alignment(s_calendar_days_layer[i], GTextAlignmentCenter);
		layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_calendar_days_layer[i])); 
	}
}

void main_window_unload(Window *window){
	for(int i=6;i>=0;i--){
		text_layer_destroy(s_calendar_days_layer[i]);
	}
	text_layer_destroy(s_calendar_back_layer);
	text_layer_destroy(s_condition_layer);	
	text_layer_destroy(s_whc_layer);
	text_layer_destroy(s_high_low_layer);
	text_layer_destroy(s_temperature_layer);
	text_layer_destroy(s_wupdated_layer);
	text_layer_destroy(s_today_date_underscore_layer);
	text_layer_destroy(s_today_date_layer);
	text_layer_destroy(s_sun_times_layer);
	text_layer_destroy(s_time_layer);
	text_layer_destroy(s_battery_status_layer);
	text_layer_destroy(s_bt_status_layer);
}


void update_bt_status(bool status){
	if(status == true){
		text_layer_set_background_color(s_bt_status_layer, GColorClear);
		text_layer_set_text_color(s_bt_status_layer, GColorWhite);
		text_layer_set_text(s_bt_status_layer, "BT");
		if(s_connection_status == false){
			if(VIBRATE_C == 1){
				vibes_short_pulse();	
			}
//			s_fail_retry = 0;
			if(data[KEY_FAILED_COUNT] != 0){
//				if(data[KEY_FAILED_COUNT] == -1){
					data[KEY_FAILED_COUNT]++;
//				}
//				update_weather();
			}
		}
		s_connection_status = true;
	} else{
		text_layer_set_background_color(s_bt_status_layer, GColorWhite);
		text_layer_set_text_color(s_bt_status_layer, GColorBlack);
		text_layer_set_text(s_bt_status_layer, "NO-BT");
		if((VIBRATE_DC == 1) && (s_connection_status == true)){
			vibes_double_pulse();
		}
		s_connection_status = false;
	}
}

bool check_bt_status(){
	update_bt_status(bluetooth_connection_service_peek());
	return s_connection_status;
}

void update_battery_status(BatteryChargeState charge_state){ 
	static char buffer_battery[6];
	if(charge_state.is_charging == true){
		snprintf(buffer_battery, sizeof(buffer_battery), "+%d%%", charge_state.charge_percent);
	} else{
		snprintf(buffer_battery, sizeof(buffer_battery), "%d%%", charge_state.charge_percent);
	}
	text_layer_set_text(s_battery_status_layer, buffer_battery);
}

void update_time(){
	time_t temp_time = time(NULL);
	struct tm *tick = localtime(&temp_time);
	static char buffer_time[6];

	if(clock_is_24h_style() == true) {
  		strftime(buffer_time, sizeof(buffer_time), "%H:%M", tick);
 	} else{
  		strftime(buffer_time, sizeof(buffer_time), "%I:%M", tick);
	}
	text_layer_set_text(s_time_layer, buffer_time);
	
	if((VIBRATE_CHIME > 0) && (tick->tm_min%VIBRATE_CHIME == 0)){
		vibes_short_pulse();
	}
}

void update_sun_times(long sunrise,long sunset){
	static char buffer_times[16];
	char buffer_rise[8];
	char buffer_set[8];
	time_t tmp_time = sunrise;
	struct tm *tmp = gmtime(&tmp_time);
	if(clock_is_24h_style() == true){
		strftime(buffer_rise, sizeof(buffer_rise), "R:%H:%M", tmp);
		time_t tmp_time = sunset;
		struct tm *tmp = gmtime(&tmp_time);
		strftime(buffer_set, sizeof(buffer_set), "S:%H:%M", tmp);
	} else{
		strftime(buffer_rise, sizeof(buffer_rise), "R:%I:%M", tmp);
		time_t tmp_time = sunset;
		struct tm *tmp = gmtime(&tmp_time);
		strftime(buffer_set, sizeof(buffer_set), "S:%I:%M", tmp);
	}
	snprintf(buffer_times, sizeof(buffer_times), "%s\n%s", buffer_rise, buffer_set);
	text_layer_set_text(s_sun_times_layer, buffer_times);
}

void update_date(){
	time_t temp_time = time(NULL); 
	struct tm *today = localtime(&temp_time);
	static char buffer_date[18];
	strftime(buffer_date, sizeof(buffer_date), "%A %b %d.", today);
	text_layer_set_text(s_today_date_layer, buffer_date);
}

void update_weather(){
	
	DictionaryIterator *iter;
	app_message_outbox_begin(&iter);
	dict_write_uint8(iter, 0, 0);
//	data[KEY_FAILED_COUNT]++;
	app_message_outbox_send();
}

void update_temperature(int temp){
	static char buffer_temperature[] = "°°°°°";
	if(TEMPERATURE_C == 0){
		temp = (int)((temp*1.8) + 32);
	}
	if(temp < 0){
		text_layer_set_font(s_temperature_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28));
	} else{
		text_layer_set_font(s_temperature_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
	}
	snprintf(buffer_temperature, sizeof(buffer_temperature), "%d°", (int)temp);
	text_layer_set_text(s_temperature_layer, buffer_temperature);
}

void update_high_low(int high,int low){
	if(TEMPERATURE_C == 0){
		high = (int)((high*1.8) + 32);
		low = (int)((low*1.8) + 32);
	}
	static char buffer_high_low[26] = "max: °°°\nmin: °°°";
	snprintf(buffer_high_low, sizeof(buffer_high_low), "max:%d°\nmin:%d°", high,low);
	text_layer_set_text(s_high_low_layer, buffer_high_low);
}

void update_whc(int wind,int humidity,int clouds){
	static char buffer_whc[20];
	switch(WIND_S){
		case 0: // m/s - meters/second
			snprintf(buffer_whc, sizeof(buffer_whc), "%dm/s\nh:%d%%\nc:%d%%", wind,humidity,clouds);
		break;
		
		case 1: // k/h - kilometer/hour
			snprintf(buffer_whc, sizeof(buffer_whc), "%dk/s\nh:%d%%\nc:%d%%", (int)(wind*3.6),humidity,clouds);
		break;
		case 2: // f/s - feet/second
			snprintf(buffer_whc, sizeof(buffer_whc), "%df/s\nh:%d%%\nc:%d%%", (int)(3*wind+((3.37*wind)/12)),humidity,clouds);
		break;
		case 3: // m/h - mile/hour
			snprintf(buffer_whc, sizeof(buffer_whc), "%dm/s\nh:%d%%\nc:%d%%", (int)(wind*3.6*0.62137),humidity,clouds);
		break;
		default: // Shouldn't happen but just in case
			snprintf(buffer_whc, sizeof(buffer_whc), "%dm/s\nh:%d%%\nc:%d%%", wind,humidity,clouds);
		break;
		
	}	
	text_layer_set_text(s_whc_layer, buffer_whc);
}

void update_condition(char condition[]){
	static char buffer_condition[15];
	snprintf(buffer_condition, sizeof(buffer_condition), "%s.", condition);
	text_layer_set_text(s_condition_layer, buffer_condition);
}

void update_calendar(){
	time_t temp_time = time(NULL); 
	struct tm *today = localtime(&temp_time);

	for(int i=0;i<7;i++){
		if(i==today->tm_wday){
			text_layer_set_background_color(s_calendar_days_layer[i], GColorBlack);
			text_layer_set_text_color(s_calendar_days_layer[i], GColorWhite);
		} else{
			text_layer_set_background_color(s_calendar_days_layer[i], GColorClear);
			text_layer_set_text_color(s_calendar_days_layer[i], GColorBlack);
		}
	}
	
	temp_time -= today->tm_wday*(60*60*24);
	today = localtime(&temp_time);
	static char buffer_cal[7][9];
	{
		buffer_cal[0][0] = 'S';
		buffer_cal[0][1] = 'u';
		buffer_cal[1][0] = 'M';
		buffer_cal[1][1] = 'o';
		buffer_cal[2][0] = 'T';
		buffer_cal[2][1] = 'u';
		buffer_cal[3][0] = 'W';
		buffer_cal[3][1] = 'e';
		buffer_cal[4][0] = 'T';
		buffer_cal[4][1] = 'h';
		buffer_cal[5][0] = 'F';
		buffer_cal[5][1] = 'r';
		buffer_cal[6][0] = 'S';
		buffer_cal[6][1] = 'a';
	}
	for(int i=0;i<7;i++){
		buffer_cal[i][2] = '\n';
		buffer_cal[i][5] = '\n';
		int day = today->tm_mday;
		if(day > 9){
			buffer_cal[i][3] = (day/10)+48;
		} else{
			buffer_cal[i][3] = '0';
		}
		buffer_cal[i][4] = (day%10)+48;
		temp_time += (7*60*60*24);
		today = localtime(&temp_time);
		day = today->tm_mday;
		if(day > 9){
			buffer_cal[i][6] = (day/10)+48;
		}else{
			buffer_cal[i][6] = '0';
		}
		buffer_cal[i][7] = (day%10)+48; 
		text_layer_set_text(s_calendar_days_layer[i],buffer_cal[i]);
		
		temp_time -= (6*60*60*24);
		today = localtime(&temp_time);
	}
}

void inbox_received_callback(DictionaryIterator *iterator, void *context) {
//	APP_LOG(APP_LOG_LEVEL_INFO, "Recieved callback.");
	// Read first item
	Tuple *t = dict_read_first(iterator);
	if(t->key == KEY_TIMEOUT){
//		s_timeouts++;
		data[KEY_FAILED_COUNT]++;
		if(data[KEY_FAILED_COUNT <= 10]){
			update_weather();
		}
/*		if(s_timeouts < 10){
			update_weather();
		} else if(s_timeouts == 10){
			text_layer_set_background_color(s_wupdated_layer, GColorWhite);
			text_layer_set_text_color(s_wupdated_layer, GColorBlack);
		}
*/
		return;
	}
	// For all items
	while(t != NULL) {
		switch(t->key) {
			case KEY_SUNRISE:
				data[KEY_SUNRISE] = (int)t->value->int32;
			break;
			case KEY_SUNSET:
				data[KEY_SUNSET] = (int)t->value->int32;
			break;
			case KEY_TEMPERATURE:
				data[KEY_TEMPERATURE] = (int)t->value->int32;
			break;
			case KEY_MAX:
				data[KEY_MAX] = (int)t->value->int32;
			break;
			case KEY_MIN:
				data[KEY_MIN] = (int)t->value->int32;
			break;
			case KEY_WIND:
				data[KEY_WIND] = (int)t->value->int32;
			break;
			case KEY_HUMIDITY:
				data[KEY_HUMIDITY] = (int)t->value->int32;
			break;
			case KEY_CLOUDS:
				data[KEY_CLOUDS] = (int)t->value->int32;
			break;
			case KEY_CONDITION:
				snprintf(s_condition, sizeof(s_condition),"%s", t->value->cstring);
			break;
			default:
				APP_LOG(APP_LOG_LEVEL_ERROR, "Key %d not recognized!", (int)t->key);
			break;
		}
		t = dict_read_next(iterator);
	}
	update_sun_times(data[KEY_SUNRISE], data[KEY_SUNSET]);
	update_temperature(data[KEY_TEMPERATURE]);
	update_high_low(data[KEY_MAX], data[KEY_MIN]);
	update_whc(data[KEY_WIND], data[KEY_HUMIDITY], data[KEY_CLOUDS]);
	update_condition(s_condition);
	
	time_t temp_time = data[KEY_WUPDATED_TIME] = time(NULL);
	struct tm *tick = localtime(&temp_time);
	if(clock_is_24h_style() == true) {
  		strftime(s_wupdated_time_string, sizeof(s_wupdated_time_string), "%H:%M", tick);
	 } else{
  		strftime(s_wupdated_time_string, sizeof(s_wupdated_time_string), "%I:%M", tick);
	}
//	text_layer_set_background_color(s_wupdated_layer, GColorClear);
//	text_layer_set_text_color(s_wupdated_layer, GColorWhite);
//	text_layer_set_text(s_wupdated_layer, s_wupdated_time_string);
	data[KEY_FAILED_COUNT] = 0;
	
	persist_write_data(KEY_DATA, data, sizeof(data));
	persist_write_string(KEY_CONDITION,s_condition);
	persist_write_string(KEY_WUPDATED_TIME_STRING, s_wupdated_time_string);
	
//	s_fail_retry = 0;
//	s_timeouts = 0;
}
void inbox_dropped_callback(AppMessageResult reason, void *context) {
//	APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped!");
	data[KEY_FAILED_COUNT]++;
	if(data[KEY_FAILED_COUNT] <= 10){
		update_weather();
	}
}

void outbox_failed_callback(DictionaryIterator *iterator, AppMessageResult reason, void *context) {
//	APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox send failed!");
	data[KEY_FAILED_COUNT]++;
	if(data[KEY_FAILED_COUNT] <= 10){
		update_weather();
	}
/*
	if(check_bt_status() && (s_fail_retry < 10)){
		s_fail_retry++;
		update_weather();
	} 
	if(data[KEY_FAILED_COUNT] == 5){
		text_layer_set_background_color(s_wupdated_layer, GColorWhite);
		text_layer_set_text_color(s_wupdated_layer, GColorBlack);
		text_layer_set_text(s_wupdated_layer, s_wupdated_time_string);
		if(VIBRATE_ERR == 1){
			vibes_short_pulse();
		}
	}
*/
}

void outbox_sent_callback(DictionaryIterator *iterator, void *context) {
//	s_fail_retry = 0;
//	APP_LOG(APP_LOG_LEVEL_INFO, "Outbox send success!");
}