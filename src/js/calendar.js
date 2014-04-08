// Timeout for (any) http requests, in milliseconds
var g_xhr_timeout = 10000;

var diffTime = 0;

/**
 * XHR wrapper
 * Usage:
 * ask({
 *   url: ...,
 *   method: null, // default depends on data: GET or POST
 *   data: null, // null => default GET
 *   headers: additional request headers (eg: {'Content-Type': 'text/plain'}
 *   success: function(text, event, xhr){...},
 *   failure: function(code, text, event, xhr){...},
 * });
 */
function ask(o) {
	function p(name, def) {
		return o[name] || def;
	}
	var req = new XMLHttpRequest();
	req.open(p('method', (o.data?'POST':'GET')), o.url, true); // open async
	headers = p('headers', {});
	for(h in headers)
		req.setRequestHeader(h, headers[h]);
	req.onload = function(e) {
		if(req.readyState == 4) {
			clearTimeout(xhrTimeout); // got response, no more need in timeout
			text = req.responseText;
			if(req.status == 200) {
				console.log("xhr:success");
				if(o.success)
					o.success(text, e, req);
			} else {
				console.log("xhr:error "+req.status+"\n"+text);
				if(o.failure)
					o.failure(req.status, text, e, req);
			}
		}
	};
	req.send(p('data', null));
	var xhrTimeout = setTimeout(function() {
		req.abort();
		displayError("Request timed out");
	}, g_xhr_timeout);
}
/**
 * Usage:
 * getJson(url, success_func, failure_func)
 * or
 * getJson(url, success_func, true) to use the same function
 *
 * success(data, event, xhr)
 * failure(err_code, data, event, xhf)
 */
function getJson(url, success, failure, headers, method, data) {
	var o = 
	{
			url: url,
			headers: headers,
			method: method,
			data: data,
			success: function(text, e, xhr) {
				if(success)
					success(JSON.parse(text), e, xhr);
			},
			failure: function(code, text, e, xhr) {
				if(failure === true) {
					if(success)
						success(JSON.parse(text), e, xhr);
				} else if(failure) // function
					failure(code, JSON.parse(text), e, xhr);
			}
	};
	//DEBUG: console.log("asking:"+JSON.stringify(o));
	ask(o);
}

/**
 * send query to googleCalendar api.
 * Will automatically send error message to Pebble
 * if anything goes wrong.
 *
 * endpoint: e.g. users/@me/lists or lists/.../tasks
 * params: {} with all args
 * success: callback(json)
 * method: get or post or something else (defaults to Get)
 * data: n/a for get method
 */
function queryEvents(endpoint, params, success, method, send_data) {
	var url = "https://www.googleapis.com/calendar/v3/" + endpoint;
	var sep = "?";
	if(params) {
		for(p in params){
			url += sep + encodeURIComponent(p) + "=" + encodeURIComponent(params[p]);
			sep = "&";
		}
	}
	var access_token=JSON.parse(localStorage['HopPicker']).access_token;
	var headers = {"Authorization": "Bearer "+access_token,
		"Content-Type": "application/json"};
	getJson(url, success, function(code, data) {
		if(code == 401) { // Invalid Credentials
			console.log("Renewing token and retrying...");
			renewToken(function() { // renew, and on success -
				var access_token=JSON.parse(localStorage['HopPicker']).access_token;
				headers["Authorization"] = "Bearer "+access_token; // the new one
				getJson(url, success, function(code, data) {
					console.log("Renewal didn't help! "+code+": "+data.error.message);
					displayError(data.error.message, code);
				}, headers, method, send_data);
			});
		} else {
			displayError(data.error.message, code);
		}
	}, headers, method, send_data);
}

/**
 * Checks current g_access_token for validity (TODO)
 * and requests new if neccessary,
 * after which calls Success callback.
 * [Now just requests new token and saves it]
 * In case of error it will call displayError.
 */
function renewToken(success) {
	console.log("Renewing token!");
	var refresh_token=JSON.parse(localStorage['HopPicker']).refresh_token;
	if(!refresh_token) {
		displayError("No refresh token; please log in!", 401);
		return;
	}
	getJson("https://pebble-hoppicker.appspot.com/v1/auth/refresh?refresh_token="+encodeURIComponent(refresh_token),
		function(data) { // success
			console.log("Renewed. "+JSON.stringify(data));
			if("access_token" in data) {
				var access_token = data.access_token;
				var jsonStorage = JSON.parse(localStorage['HopPicker']);
				jsonStorage.access_token = access_token;
				localStorage['HopPicker'] = JSON.stringify(jsonStorage);
				success(access_token);
			} else if("error" in data) {
				displayError("Auth error: " + data.error + "\nPlease open Pebble app and log in again!");
			} else {
				displayError("No access token received from Google!"); // unlikely...
			}
		},
		function(code, data) { // failure
			displayError(data.error.message, code);
		});
}

var g_msg_buffer = [];
var g_msg_transaction = null;

/**
 * Sends appMessage to pebble; logs errors.
 * failure: may be True to use the same callback as for success.
 */
function sendMessage(data, success, failure) {
	function sendNext() {
		g_msg_transaction = null;
		next = g_msg_buffer.shift();
		if(next) { // have another msg to send
			sendMessage(next);
		}
	}
	if(g_msg_transaction) { // busy
		g_msg_buffer.push(data);
	} else { // free
		g_msg_transaction = Pebble.sendAppMessage(data,
			function(e) {
				console.log("Message sent for transactionId=" + e.data.transactionId);
				if(g_msg_transaction >= 0 && g_msg_transaction != e.data.transactionId) // -1 if unsupported
					console.log("### Confused! Message sent which is not a current message. "+
							"Current="+g_msg_transaction+", sent="+e.data.transactionId);
				if(success)
					success();
				sendNext();
			},
		   	function(e) {
				console.log("Failed to send message for transactionId=" + e.data.transactionId +
						", error is "+("message" in e.error ? e.error.message : "(none)"));
				if(g_msg_transaction >= 0 && g_msg_transaction != e.data.transactionId)
					console.log("### Confused! Message not sent, but it is not a current message. "+
							"Current="+g_msg_transaction+", unsent="+e.data.transactionId);
				if(failure === true) {
					if(success)
						success();
				} else if(failure)
					failure();
				sendNext();
			}
		);
		if(g_msg_transaction === undefined) { // iOS buggy sendAppMessage
			g_msg_transaction = -1; // just a dummy "non-false" value for sendNext and friends
		}
		console.log("transactionId="+g_msg_transaction+" for msg "+JSON.stringify(data));
	}
}

/**
 * Sends an error packet to Pebble
 * code may be null
 */
function displayError(text, code) {
	if(code) text += " (" + code + ")";
	console.log("Sending error msg to Pebble: " + text);
}

function doGetNextEvent() {
	var calendarId = "primary";
	var date = new Date();
	var lowerDate = new Date(date.getTime() - 90*60000);
	var upperDate = new Date(date.getTime() + 90*60000);
	var params = {
		maxResults: 6,
		orderBy: "startTime",
		singleEvents : true,
		timeMax: upperDate.toISOString(),
		timeMin: lowerDate.toISOString(),
	};
	queryEvents("calendars/"+calendarId+"/events", params, function(d) {
		var count = 0;
		
		// var miniDate = new Date();
		// //miniDate = new Date(miniDate.getTime() - 90*60000);

		// for(var id=0; id<d.items.length; id++){
		// 	var startdate = new Date(d.items[id].start.dateTime);
		// 	if(startdate >= miniDate){
		// 		count++;
		// 	}
		// }

		// sendMessage({
		// 	eventsCount: count, // array start/size
		// });

		sendMessage({
			eventsCount: d.items.length, 
		});

		for(var id=0; id<d.items.length; id++){
			var startdate = new Date(d.items[id].start.dateTime);
			var enddate = new Date(d.items[id].end.dateTime);
			// var arrayDate = [ startdate.getDate(), startdate.getHours(), startdate.getMinutes(), enddate.getDate(), enddate.getHours(), enddate.getMinutes() ];
			console.log("Event " + id + " : " + startdate.getTime()/1000 + " --> " + enddate.getTime()/1000);
			// if(startdate >= miniDate){
				sendMessage({
				eventId: id, 
				// eventDates: arrayDate,
				eventStart: parseInt(startdate.getTime()/1000) - diffTime,
				eventEnd: parseInt(enddate.getTime()/1000) - diffTime,
				eventSummary: d.items[id].summary
			});
			// }
			
		}
	});
}

/* Initialization */
// Pebble.addEventListener("ready", function(e) {
// 	console.log("JS is running. Okay.");
// 	g_access_token = localStorage.access_token;
// 	g_refresh_token = localStorage.refresh_token;
// 	console.log("access token (from LS): "+g_access_token);
// 	console.log("refresh token (from LS): "+hideToken(g_refresh_token));

// 	if(g_refresh_token) // check on refresh token, as we can restore/renew access token later with it
// 		ready(); // ready: tell watchapp that we are ready to communicate
// 	else { // try to retrieve it from watchapp
// 		console.log("No refresh token, trying to retrieve");
// 		sendMessage({ code: 41 }, // retrieve token
// 			false, // on success just wait for reply
// 			function() { // on sending failure tell user to login; although error message is unlikely to pass
// 				displayError("No refresh token stored, please log in!", 401); // if no code, tell user to log in
// 			}
// 		);
// 	}
// });

/* Configuration window */
// Pebble.addEventListener("showConfiguration", function(e) {
// 	console.log("Showing config window...");
// 	var url = "http://pebble-notes.appspot.com/v1/notes-config.html#"+
// 		encodeURIComponent(JSON.stringify({"access_token": (g_access_token === undefined ? "" : g_access_token)}));
// 	console.log("URL: "+url);
// 	var result = Pebble.openURL(url);
// 	console.log("Okay. "+result);
// });
// Pebble.addEventListener("webviewclosed", function(e) {
// 	console.log("webview closed: "+e.response);
// 	result = {};
// 	try {
// 		if(('response' in e) && e.response) // we don't want to parse 'undefined'
// 			result = JSON.parse(decodeURIComponent(e.response));
// 	} catch(ex) {
// 		console.log("Parsing failed: "+ex+"\n");
// 	}
// 	if("access_token" in result && "refresh_token" in result) { // assume it was a login session
// 		console.log("Saving tokens");
// 		// save tokens
// 		if(result.access_token) {
// 			localStorage.access_token = g_access_token = result.access_token;
// 			console.log("Access token: " + g_access_token);
// 		}
// 		if(result.refresh_token) {
// 			localStorage.refresh_token = g_refresh_token = result.refresh_token;
// 			console.log("Refresh token saved: " + hideToken(g_refresh_token));
// 		}
// 		// TODO: maybe save expire time for later checks? (now + value)
// 		// now save tokens in watchapp:
// 		sendMessage({
// 				code: 40, // save_token
// 				access_token: g_access_token,
// 				refresh_token: g_refresh_token
// 		});
// 		ready(); // tell watchapp that we are now ready to work
// 	} else if("logout" in result) {
// 		console.log("Logging out");
// 		g_access_token = localStorage.access_token = '';
// 		g_refresh_token = localStorage.refresh_token = '';
// 		sendMessage({ code: 40 }); // remove credentials
// 	}
// });

/* Messages */
Pebble.addEventListener("appmessage", function(e) {
	console.log("Received message: " + JSON.stringify(e.payload));
	if(e.payload.calendar){
		var date = new Date;


		diffTime = parseInt(date.getTime()/1000) - e.payload.calendar;
		console.log("diffTime : " + diffTime);

		doGetNextEvent();
	}
});