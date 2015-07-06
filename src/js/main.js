// CONSUMER_KEY, CONSUMER_SECRET, ACCESS_TOKEN, ACCESS_SECRET defined in secrets.js

/*
function getTweets() {
	// Construct URL
	var url = 'https://api.twitter.com/1.1/statuses/user_timeline.json';
	console.log('URL ' + url);
	
	// OAuth signature
	var nonce = Date.now(), timestamp = Math.floor(Date.now() / 1000);
	var base_signature = 'GET&' + encodeURIComponent(url) + 
		'&count%3D1' +
		'%26oauth_consumer_key%3D' +  encodeURIComponent(CONSUMER_KEY) +
		'%26oauth_nonce%3D'+ nonce +
		'%26oauth_signature_method%3DHMAC-SHA1' +
		'%26oauth_timestamp%3D' + timestamp +
		'%26oauth_token%3D' + encodeURIComponent(ACCESS_TOKEN) +
		'%26oauth_version%3D1.0' +
		'%26screen_name%3DHalo';
	var signing_secret = encodeURIComponent(CONSUMER_SECRET) + '&' + encodeURIComponent(ACCESS_SECRET);
	var signature = CryptoJS.HmacSHA1(base_signature, signing_secret).toString(CryptoJS.enc.Base64);
	
	// Send request to Twitter
	var xhr = new XMLHttpRequest();
	xhr.onload = function() {
		// responseText contains a JSON object with weather info
		var json = JSON.parse(this.responseText);
		console.log('Got tweet: ' + json[0].text);
			
		// Assemble dictionary using our keys
		var dictionary = { 'KEY_TWEET': json[0].text };
			
		// Send to Pebble
		Pebble.sendAppMessage(dictionary,
			function(e) {
				onsole.log('Tweet sent to Pebble successfully!');
			},
			function(e) {
				console.log('Error sending tweet to Pebble!');
			}
		);
	};
	xhr.onerror = function() {
		console.log('Error! ' + this.status + ' ' + this.statusText);
	};
	xhr.open('GET', url + '?screen_name=Halo&count=1');
	xhr.setRequestHeader('Authorization',
		'OAuth oauth_consumer_key="' + CONSUMER_KEY + '", ' +
		'oauth_nonce="' + nonce + '", ' + 
		'oauth_signature="' + encodeURIComponent(signature) + '", ' +
		'oauth_signature_method="HMAC-SHA1", ' +
		'oauth_timestamp="' + timestamp + '", ' +
		'oauth_token="' + ACCESS_TOKEN + '", ' +
		'oauth_version="1.0"');
	
	xhr.send();
}
*/

// Listen for when the watchface is opened
Pebble.addEventListener('ready', 
	function(e) {
		console.log('PebbleKit JS ready!');
		//getTweets();
	}
);

// Listen for when an AppMessage is received
Pebble.addEventListener('appmessage',
	function(e) {
		console.log('AppMessage received!');
		//getTweets();
	}                     
);