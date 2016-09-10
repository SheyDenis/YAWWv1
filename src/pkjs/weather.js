// Listen for when an AppMessage is received
Pebble.addEventListener('appmessage',
	function(e){
		console.log('AppMessage received!');
		getWeather();
	}
);

// Listen for when the watchface is opened
Pebble.addEventListener('ready', 
	function(e){
		console.log('PebbleKit JS ready!');
	}
);

var xhrRequest = function(url, type, callback){
	var xhr = new XMLHttpRequest();
	xhr.timeout = 10000;
	xhr.ontimeout = function(){
		console.log('xhrRequest timed out.');
		var dictionary = {
				'KEY_TIMEOUT': 1,
		};
		Pebble.sendAppMessage(dictionary,
				function(e){
//					console.log('Error sent to Pebble successfully!');
				},
				function(e){
					console.log('Error sending error to Pebble!');
				}
		);
	};
	xhr.onload = function () {
		callback(this.responseText);
	};
	xhr.open(type, url, true);
	xhr.send(null);
};

function locationSuccess(pos) {
	
	// Construct URL
	// Remember to change APPID
	var url = 'http://api.openweathermap.org/data/2.5/weather?lat=' + pos.coords.latitude + '&lon=' + pos.coords.longitude + '&type=like&units=metric&APPID=YOURKEYHERE';
	var url2 = 'http://api.openweathermap.org/data/2.5/forecast?lat=' + pos.coords.latitude + '&lon=' + pos.coords.longitude + '&type=like&units=metric&APPID=YOURKEYHERE';
	// Send request to OpenWeatherMap
	xhrRequest(url, 'GET',
		function(responseText) {
			// responseText contains a JSON object with weather info
			var json = JSON.parse(responseText);

			var tzo = new Date().getTimezoneOffset();
			var sunrise = json.sys.sunrise-(tzo*60);
			var sunset = json.sys.sunset-(tzo*60);
			
			var temperature = Math.round(json.main.temp);
			var wind,humidity,clouds;
			
			try{
				wind = json.wind.speed;
			} catch(err){
				wind = 0;
				console.log(err);
			}
			try{
				humidity = json.main.humidity;
			} catch(err){
				humidity = 0;
				console.log(err);
			}
			try{
				clouds = json.clouds.all;
			} catch(err){
				clouds = 0;
				console.log(err);
			}
			var condition = json.weather[0].main;
			

//			console.log('sunrise: ' + sunrise);
//			console.log('sunset: ' + sunset);
//			console.log('Sunrise is ' + sunrise);
//			console.log('Sunset is ' + sunset);
//			console.log('Timezone offset is ' + d.getTimezoneOffset()*60);
//			console.log('Temperature is ' + temperature);
//			console.log('Max is ' + maxtemp);
//			console.log('min is ' + mintemp);
//			console.log('Wind is ' + wind);
//			console.log('Humidity is ' + humidity);
//			console.log('Cloudiness is ' + clouds);
//			console.log('Condition are ' + condition);
			var dictionary = {
				'KEY_SUNRISE': sunrise,
				'KEY_SUNSET': sunset,
				'KEY_TEMPERATURE': temperature,
				'KEY_WIND': wind,
				'KEY_HUMIDITY': humidity,
				'KEY_CLOUDS': clouds,
				'KEY_CONDITION': condition
			};
			// Send to Pebble
			Pebble.sendAppMessage(dictionary,
				function(e){
					console.log('Weather info sent to Pebble successfully!');
				},
				function(e){
					console.log('Error sending weather info to Pebble!');
				}
			);
		}      
	);
	xhrRequest(url2, 'GET',
		function(responseText) {
			// responseText contains a JSON object with weather info
			var json = JSON.parse(responseText);
			var maxtemp = Math.round(json.list[0].main.temp_max);
			var mintemp = Math.round(json.list[0].main.temp_min);
			var dictionary = {
				'KEY_MAX': maxtemp,
				'KEY_MIN': mintemp
			};
//			console.log('Max is ' + maxtemp);
//			console.log('Min is ' + mintemp);
			Pebble.sendAppMessage(dictionary,
				function(e){
					console.log('Weather info sent to Pebble successfully!');
				},
				function(e){
					console.log('Error sending weather info to Pebble!');
				}
			);
		}
	);
	
}

function locationError(err) {
	console.log('Error requesting location!');
	// This is being treated the same as timeout error so this will just stay named like this
		var dictionary = {
				'KEY_TIMEOUT': 0,
		};
		Pebble.sendAppMessage(dictionary,
				function(e){
					console.log('Error sent to Pebble successfully!');
				},
				function(e){
					console.log('Error sending error to Pebble!');
				}
		);
}

function getWeather() {
	navigator.geolocation.getCurrentPosition(locationSuccess, locationError, {maximumAge:10000,timeout:15000});
}

