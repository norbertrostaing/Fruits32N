#include "_wifi.h"

TaskHandle_t TaskForWifi;
wifi_mode_t desiredMode = wifi_mode_t::WIFI_MODE_APSTA;

void printMode(wifi_mode_t m) {
    if (m == WIFI_MODE_NULL) Serial.println("WIFI_MODE_NULL");
    if (m == WIFI_MODE_STA) Serial.println("WIFI_MODE_STA");
    if (m == WIFI_MODE_AP) Serial.println("WIFI_MODE_AP");
    if (m == WIFI_MODE_APSTA) Serial.println("WIFI_MODE_APSTA");
    if (m == WIFI_MODE_MAX) Serial.println("WIFI_MODE_MAX");

}

void loopWifi() {	
	wifi_mode_t neededMode = WIFI_MODE_NULL;
	wifi_mode_t currentMode = WiFi.getMode();
	
	// Serial.println("Current mode : ");
	// printMode(currentMode);
	// Serial.println("desired mode : ");
	// printMode(desiredMode);
	// Serial.println(ethernetIsConnected? "eth OK" : "eth KO");

	bool currentlyConnectedToWifi = currentMode == WIFI_MODE_APSTA || currentMode == WIFI_MODE_STA;
	bool currentlyHaveAccessPoint = currentMode == WIFI_MODE_APSTA || currentMode == WIFI_MODE_AP;
	bool shouldConnectToWifi = desiredMode == WIFI_MODE_APSTA || desiredMode == WIFI_MODE_STA;
	bool shouldHaveAccessPoint = desiredMode == WIFI_MODE_APSTA || desiredMode == WIFI_MODE_AP;

	if (!shouldHaveAccessPoint && !ethernetIsConnected && WiFi.status() != WL_CONNECTED && millis() > 15000) {
		shouldHaveAccessPoint = true;
	}

	if (shouldConnectToWifi && shouldHaveAccessPoint) neededMode = WIFI_MODE_APSTA;
	else if (shouldConnectToWifi) neededMode = WIFI_MODE_STA;
	else if (shouldHaveAccessPoint) neededMode = WIFI_MODE_AP;

	// Serial.println("Needed mode : ");
	// printMode(neededMode);
	bool shouldRestartOSC = false;
	if (currentMode != neededMode) {
		WiFi.mode(neededMode);
		shouldRestartOSC = true;
	}

	if (shouldHaveAccessPoint && !currentlyHaveAccessPoint) {
		WiFi.softAP(chipName+" "+String(chipId), "raclette");
		WiFi.enableAP(true);
	} else if (!shouldHaveAccessPoint && currentlyHaveAccessPoint) {
		WiFi.softAPdisconnect();
		WiFi.enableAP(false);
	}

	if (shouldConnectToWifi) {
		if (WiFi.status() == WL_CONNECTED && WiFi.SSID() != config["wifi/name"].as<String>()) {
			WiFi.disconnect();
			info["wifi/IP"] = "not connected";
			info["wifi/SM"] = "not connected";
		} else {
			info["wifi/IP"] = WiFi.localIP();
			info["wifi/SM"] = WiFi.subnetMask();
		}
		if (config["wifi/name"] != "" && WiFi.status() != WL_CONNECTED) {
			WiFi.begin(config["wifi/name"].as<String>(), config["wifi/pass"].as<String>());

			int count = 0;
			while(WiFi.status() != WL_CONNECTED && count < 10){
					count ++;
					vTaskDelay(pdMS_TO_TICKS(500));
			}

			if (WiFi.status() == WL_CONNECTED) {
					info["wifi/IP"] = WiFi.localIP();
					info["wifi/SM"] = WiFi.subnetMask();
			} else {
					info["wifi/IP"] = "not connected";
					info["wifi/SM"] = "not connected";
			}
		}
	}

	if (shouldRestartOSC) {
		//oscNeedReboot = true;
	}

}

void setDesiredMode() {
	String mode = config["wifi/mode"].as<String>();
	if (mode == "Access point and client") {
		desiredMode = wifi_mode_t::WIFI_MODE_APSTA;
	} else if (mode == "Access point") {
		desiredMode = wifi_mode_t::WIFI_MODE_AP;
	} else if (mode == "Client") {
		desiredMode = wifi_mode_t::WIFI_MODE_STA;
	} else { // Off  
		desiredMode = wifi_mode_t::WIFI_MODE_NULL;
	}
}

void TaskForWifiCode( void * pvParameters ){
	for(;;){
		loopWifi();
		vTaskDelay(pdMS_TO_TICKS(5000));
	} 
}

void setupWifi() {
	addEnumConfig("wifi/mode", "Access point and client");
	addEnumOption("wifi/mode", "Access point");
	addEnumOption("wifi/mode", "Client");
	addEnumOption("wifi/mode", "Off");
	addStringConfig("wifi/name", "");
	addStringConfig("wifi/pass", "");
	info["wifi/IP"] = "not connected";
	info["wifi/SM"] = "not connected";

	WiFi.mode(WIFI_MODE_NULL);
	setDesiredMode();

	xTaskCreatePinnedToCore(
		TaskForWifiCode,   /* Task function. */
		"TaskForWifi",     /* name of task. */
		2048,       /* Stack size of task */
		NULL,        /* parameter of the task */
		1,           /* priority of the task */
		&TaskForWifi,      /* Task handle to keep track of created task */
		1);          /* pin task to core 0 */                  
}

void configUpdatedWifi(String k) {
	if (k == "wifi/mode") {
		setDesiredMode();
	}
}
