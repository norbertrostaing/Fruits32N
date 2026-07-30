#include "_config.h"
#include "_osc.h"

int oscPort = 9004; 

TaskHandle_t TaskForOsc;

static bool restartOscUdp()
{
    const auto& udpMap = OscWiFi.getUdpMap();
    auto udpIterator = udpMap.find(oscPort);

    if (udpIterator == udpMap.end())
    {
        Serial.println("OSC: aucun socket UDP pour ce port");
        return false;
    }

    udpIterator->second->stop();
    const bool success = udpIterator->second->begin(oscPort) == 1;

    if (success) Serial.println("OSC: socket UDP redémarré");
    else Serial.println("OSC: échec du redémarrage UDP");

    return success;
}

void loopOsc() {
  OscWiFi.getServer(oscPort).parse();
}

void TaskForOscCode( void * pvParameters ){
  for(;;){
    if (oscNeedReboot) {
      oscNeedReboot = false;
      restartOscUdp();
    }
    loopOsc();
    vTaskDelay(pdMS_TO_TICKS(3));
  } 
}

void sendConfig(String k, String remoteIp) {
  if (configTypes[k] == "b") { 
    OscWiFi.getClient().send(remoteIp, oscPort, "/"+chipName+""+String(chipId)+"/config/"+k, config[k].as<bool>()); 
  }
  else if (configTypes[k] == "f") { 
    OscWiFi.getClient().send(remoteIp, oscPort, "/"+chipName+""+String(chipId)+"/config/"+k, config[k].as<float>()); 
    if (!configOptions[k]["rangeMin"].isNull()) {
      OscWiFi.getClient().send(remoteIp, oscPort, "/"+chipName+""+String(chipId)+"/config/"+k, config[k].as<float>(), "range",configOptions[k]["rangeMin"].as<float>(), configOptions[k]["rangeMax"].as<float>());
    }
  }
  else if (configTypes[k] == "i") { 
    OscWiFi.getClient().send(remoteIp, oscPort, "/"+chipName+""+String(chipId)+"/config/"+k, config[k].as<int>()); 
    if (!configOptions[k]["rangeMin"].isNull()) {
      OscWiFi.getClient().send(remoteIp, oscPort, "/"+chipName+""+String(chipId)+"/config/"+k, config[k].as<int>(), "range",configOptions[k]["rangeMin"].as<int>(), configOptions[k]["rangeMax"].as<int>());
    }
  }
  else if (configTypes[k] == "s") { 
    OscWiFi.getClient().send(remoteIp, oscPort, "/"+chipName+""+String(chipId)+"/config/"+k, config[k].as<String>()); 
  }
  else if (configTypes[k] == "e") { 
    OscWiFi.getClient().send(remoteIp, oscPort, "/"+chipName+""+String(chipId)+"/config/"+k, config[k].as<String>()); 
    OscWiFi.getClient().send(remoteIp, oscPort, "/"+chipName+""+String(chipId)+"/config/"+k, config[k].as<String>(), "options",configOptions[k]["options"].as<String>());
  }

}

void sendInfo(String k, String remoteIp) {
  if (!info[k].isNull() && info[k].is<bool>()) { OscWiFi.getClient().send(remoteIp, oscPort, "/"+chipName+""+String(chipId)+"/info/"+k, info[k].as<bool>()); }
  else if (!info[k].isNull() && info[k].is<float>()) { OscWiFi.getClient().send(remoteIp, oscPort, "/"+chipName+""+String(chipId)+"/info/"+k, info[k].as<float>()); }
  else if (!info[k].isNull() && info[k].is<int>()) { OscWiFi.getClient().send(remoteIp, oscPort, "/"+chipName+""+String(chipId)+"/info/"+k, info[k].as<int>()); }
  else if (!info[k].isNull() && info[k].is<String>()) { OscWiFi.getClient().send(remoteIp, oscPort, "/"+chipName+""+String(chipId)+"/info/"+k, info[k].as<String>()); }
}

void subscribeAll() {
  auto& serv = OscWiFi.getServer(oscPort);
  for (JsonPair kv : config.as<JsonObject>()) {
    String k = kv.key().c_str();
    serv.subscribe("/config/"+k, [k](const OscMessage& m){
      if (m.size()>0) {
        if (m.isBool(0)) {writeConfig(k, m.getArgAsBool(0));}
        else if (m.isFloat(0)) {writeConfig(k, m.getArgAsFloat(0));}
        else if (m.isInt32(0)) {writeConfig(k, m.getArgAsInt32(0));}
        else if (m.isStr(0)) {writeConfig(k, m.getArgAsString(0));}
      } else {
        sendConfig(k, m.remoteIP());
      }
    } );
  }

  serv.subscribe("/config", [](const OscMessage& m){
      for (JsonPair kv : config.as<JsonObject>()) {
        String k = kv.key().c_str();
        sendConfig(k, m.remoteIP());
      }
  });

  for (JsonPair kv : trigger.as<JsonObject>()) {
    String k = kv.key().c_str();
    serv.subscribe("/trigger/"+k, [k](const OscMessage& m){
      triggerTriggered(k);
    } );
  }
  serv.subscribe("/trigger", [](const OscMessage& m){
    for (JsonPair kv : trigger.as<JsonObject>()) {
      String k = kv.key().c_str();
      //OscWiFi.getClient().send(m.remoteIP(), oscPort, "/"+chipName+""+String(chipId)+"/trigger/"+k); 
    }
  });

  for (JsonPair kv : info.as<JsonObject>()) {
    String k = kv.key().c_str();
    serv.subscribe("/info/"+k, [k](const OscMessage& m){
      sendInfo(k, m.remoteIP());
    } );
  }
  serv.subscribe("/info", [](const OscMessage& m){
      for (JsonPair kv : info.as<JsonObject>()) {
        String k = kv.key().c_str();
        sendInfo(k, m.remoteIP());
      }
  });


}

void setupOSC() {
  subscribeAll();
  xTaskCreatePinnedToCore(
                    TaskForOscCode,   /* Task function. */
                    "TaskForOsc",     /* name of task. */
                    4096,       /* Stack size of task */
                    NULL,        /* parameter of the task */
                    1,           /* priority of the task */
                    &TaskForOsc,      /* Task handle to keep track of created task */
                    1);          /* pin task to core 0 */                  

}


