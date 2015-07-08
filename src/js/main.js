var GW2_REALMS = {};
var GW2_MATCHES = [];
var match_next = 0;

var xhrRequest = function(url, type, callback) {
	var xhr = new XMLHttpRequest();
	xhr.onload = function() {
		callback(JSON.parse(this.responseText));
	};
	xhr.onerror = function() {
		console.log('XMLHttpRequest error! ' + this.status + ' ' + this.statusText);
	};
	xhr.open(type, url);
	xhr.send();
};

function getRealmsAndMatches() {
	xhrRequest('https://api.guildwars2.com/v1/world_names.json?lang=en', 'GET', function(json) {
		for (var i = 0; i < json.length; i++) {
			GW2_REALMS[ json[i].id ] = json[i].name;
		}
		getMatches();
	});
};

function getMatches() {
	xhrRequest('https://api.guildwars2.com/v1/wvw/matches.json', 'GET', function(json) {
		GW2_MATCHES = [];
		for (var i = 0; i < json.wvw_matches.length; i++) {
			var match = json.wvw_matches[i];
			GW2_MATCHES.push({
				'red_realm': GW2_REALMS[match.red_world_id],
				'blue_realm': GW2_REALMS[match.blue_world_id],
				'green_realm': GW2_REALMS[match.green_world_id]
			});
			getMatchDetails(match.wvw_match_id, GW2_MATCHES[GW2_MATCHES.length - 1]);
		}
	});
};

function getMatchDetails(match_id, match_obj) {
	xhrRequest('https://api.guildwars2.com/v1/wvw/match_details.json?match_id=' + match_id, 'GET', function(json) {
		var total_score = json.scores[0] + json.scores[1] + json.scores[2];
		match_obj['red_score'] = Math.floor(json.scores[0] / total_score * 100);
		match_obj['blue_score'] = Math.floor(json.scores[1] / total_score * 100);
		match_obj['green_score'] = Math.floor(json.scores[2] / total_score * 100);
	});	
}

function getNextMatch() {
	if (match_next >= GW2_MATCHES.length) {
		match_next = 0;
	}
	
	var match = GW2_MATCHES[match_next];
	if (match && match.red_score) {
		console.log('Got next match: ' + JSON.stringify(match));
		// Assemble dictionary using our keys
		var dictionary = {
			'KEY_RED_NAME': match.red_realm,
			'KEY_RED_SCORE': match.red_score,
			'KEY_BLUE_NAME': match.blue_realm,
			'KEY_BLUE_SCORE': match.blue_score,
			'KEY_GREEN_NAME': match.green_realm,
			'KEY_GREEN_SCORE': match.green_score,
			'KEY_THIS_MATCH': (match_next + 1),
			'KEY_MATCHES': GW2_MATCHES.length
		};
		
		// Send to Pebble
		Pebble.sendAppMessage(dictionary,
			function(e) {
				console.log('Data sent to Pebble successfully!');
				match_next++;
			},
			function(e) {
				console.log('Error sending data to Pebble!');
			}
		);
	}
};

// Listen for when the watchface is opened
Pebble.addEventListener('ready', 
	function(e) {
		console.log('PebbleKit JS ready!');
		getRealmsAndMatches();
	}
);

// Listen for when an AppMessage is received
Pebble.addEventListener('appmessage',
	function(e) {
		console.log('Received message: ' + JSON.stringify(e.payload));
		if (e.payload['KEY_UPDATEMODE'] === 0) {
			// reload super-data
			getRealmsAndMatches();
		} else {
			// return the next match
			getNextMatch();
		}
	}                     
);