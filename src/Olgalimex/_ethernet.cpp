#include "_ethernet.h"

TaskHandle_t TaskForEthernet;

bool pleaseReconfigureEthernet = false;

void updateIpAddress()
{
	IPAddress noIP(0, 0, 0, 0);

	IPAddress staticIP;
	IPAddress staticSM;

	bool validIP = staticIP.fromString( config["ethernet/staticIP"].as<String>() );
	bool validSM = staticSM.fromString( config["ethernet/staticSM"].as<String>() );

	info["ethernet/IP"] = "connecting";
	info["ethernet/SM"] = "connecting";

	if (validIP && validSM)
	{
		ETH.config(staticIP, noIP, staticSM);
	}
	else
	{
		ETH.config(noIP, noIP, noIP);
	}
}

void loopEthernet() {
		if (pleaseReconfigureEthernet) {
			updateIpAddress();
			pleaseReconfigureEthernet = false;
		}
		vTaskDelay(pdMS_TO_TICKS(2000));
}

void TaskForEthernetCode( void * pvParameters ){
	for(;;){
		loopEthernet();
	} 
}

void onNetworkEvent(WiFiEvent_t event) {
	switch (event)
		{
		case ARDUINO_EVENT_ETH_START:
			Serial.println("Ethernet démarré");
			break;

		case ARDUINO_EVENT_ETH_CONNECTED:
			Serial.println("Câble Ethernet connecté");
			ethernetIsConnected = true;
			break;

		case ARDUINO_EVENT_ETH_GOT_IP:
			ethernetIsConnected = true;

			info["ethernet/IP"] = ETH.localIP().toString();
			info["ethernet/SM"] = ETH.subnetMask().toString();

			Serial.print("Ethernet connecté, IP : ");
			Serial.println(ETH.localIP());
			break;

		case ARDUINO_EVENT_ETH_DISCONNECTED:
			ethernetIsConnected = false;

			info["ethernet/IP"] = "not connected";
			info["ethernet/SM"] = "not connected";

			Serial.println("Câble Ethernet débranché");
			break;

		case ARDUINO_EVENT_ETH_STOP:
			ethernetIsConnected = false;
			Serial.println("Ethernet arrêté");
			break;

		default:
			break;
	}
}

void setupEthernet() {
	addStringConfig("ethernet/staticIP", "");
	addStringConfig("ethernet/staticSM", "");
	info["ethernet/IP"] = "not connected";
	info["ethernet/SM"] = "not connected";

	WiFi.onEvent(onNetworkEvent);
	delay(100);
	ETH.begin();
	updateIpAddress();
	xTaskCreatePinnedToCore(
		TaskForEthernetCode,   /* Task function. */
		"TaskForEthernet",     /* name of task. */
		2048,       /* Stack size of task */
		NULL,        /* parameter of the task */
		1,           /* priority of the task */
		&TaskForEthernet,      /* Task handle to keep track of created task */
		1);          /* pin task to core 0 */                  
	
 
}

void configUpdatedEthernet(String k) {
	if (k == "ethernet/staticIP" || k == "ethernet/staticSM") {
		pleaseReconfigureEthernet = true;
	}
}

