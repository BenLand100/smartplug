#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>

#define RELAY 12
#define STATUS 2
Preferences prefs;

WebServer server(80);

char buffer[4096];
bool configured;
bool state;

#define ENTRY digitalWrite(STATUS, configured ? HIGH : LOW);
#define EXIT digitalWrite(STATUS, configured ? LOW : HIGH);

void root() {
  ENTRY
  Serial.print("Power state: ");
  Serial.println(state ? "ON" : "OFF");
  sprintf(buffer,
    "<!DOCTYPE html><html>"
    "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
    "<link rel=\"icon\" href=\"data:,\">"
    "<style>"
    "html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center; }"
    ".buttonon { background-color: #00FF00; border: none; color: white; padding: 16px 40px; text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer; }"
    ".buttonoff { background-color: #FF0000; border: none; color: white; padding: 16px 40px; text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer; }"
    "</style>"
    "</head>"
    "<body><h1>30A Smart Switch</h1>"
    "<p>Current State: %s</p> %s"
    "</body></html>",
    state ? "ON" : "OFF",
    ( state
      ? "<p><a href=\"/off\"><button class=\"buttonoff\">OFF</button></a></p>"
      : "<p><a href=\"/on\"><button class=\"buttonon\">ON</button></a></p>" ) 
  );  
  server.send(200, "text/html", buffer);
  EXIT
}


void turnOn() {
  ENTRY
  digitalWrite(RELAY, HIGH);
  state = true;  
  server.sendHeader("Location", "/", true);  
  server.send(302, "text/plain", "");
  EXIT
}

void turnOff() {
  ENTRY
  digitalWrite(RELAY, LOW);
  state = false;
  server.sendHeader("Location", "/", true);  
  server.send(302, "text/plain", "");
  EXIT
}

void api() {
  ENTRY
  if (server.method() == HTTP_GET) {
    server.send(200, "text/plain", state ? "ON" : "OFF");
  } else if (server.method() == HTTP_POST) {
    if (server.hasArg("plain")) { //Arduino/ESP32 puts the body in "plain" to keep everyone confused... 
      String new_state = server.arg("plain");
      if (new_state == "ON") {
        digitalWrite(RELAY, HIGH);
        state = true;  
        server.send(200, "text/plain", "ok");
      } else if (new_state == "OFF") {
        digitalWrite(RELAY, LOW);
        state = false;  
        server.send(200, "text/plain", "ok");
      } else {
        server.send(400, "text/plain", "");
      }
    } else {
      server.send(400, "text/plain", "");
    }
  } else {
    server.send(400, "text/plain", "");
  }
  EXIT
}

void configure() {
  ENTRY
  if (server.method() == HTTP_GET) {
    sprintf(buffer,
      "<!DOCTYPE html><html>"
      "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
      "<link rel=\"icon\" href=\"data:,\">"
      "<style>"
      "html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center; }"
      "</style>"
      "</head>"
      "<body><h1>30A Smart Switch</h1>"
      "<p>WiFi Configuration</p>"
      "<form action=\"/configure\" method=\"post\">"
      "<label for=\"fname\">Network SSID:</label><br>"
      "<input type=\"text\" id=\"ssid\" name=\"ssid\"><br>"
      "<label for=\"fname\">Network Password:</label><br>"
      "<input type=\"password\" id=\"pwd\" name=\"pwd\"><br>"
      "<input type=\"submit\" value=\"Submit\">"
      "</form>"
      "</body></html>"
    );  
    server.send(200, "text/html", buffer);
  } else if (server.method() == HTTP_POST) {
    if (server.hasArg("ssid") && server.hasArg("pwd")) {
      String SSID = server.arg("ssid");
      String PWD = server.arg("pwd");
      
      prefs.begin("credentials", false);
      prefs.putString("SSID",SSID);
      prefs.putString("PWD",PWD);
      prefs.end();
      
      sprintf(buffer,
        "<!DOCTYPE html><html>"
        "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
        "<link rel=\"icon\" href=\"data:,\">"
        "<style>"
        "html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center; }"
        "</style>"
        "</head>"
        "<body><h1>30A Smart Switch</h1>"
        "<p>WiFi configuration saved!</p>"
        "<p>Reconnect after device reboots.</p>"
        "</body></html>"
      );  
      server.send(200, "text/html", buffer);
      delay(1000);  
      ESP.restart();
    } else {
      server.send(400, "text/plain", "");
    }
  } else {
    server.send(400, "text/plain", "");
  }
  EXIT
}

void setup() {     
  Serial.begin(115200); 

  pinMode(STATUS, OUTPUT);
  digitalWrite(STATUS, HIGH);
  
  pinMode(RELAY, OUTPUT);
  digitalWrite(RELAY, LOW);
  state = false;

  prefs.begin("credentials", true);
  String SSID = prefs.getString("SSID","");
  String PWD = prefs.getString("PWD","");
  prefs.end();

  configured = SSID != "";

  if (configured) {
    Serial.print("Connecting to ");
    Serial.print(SSID);
    WiFi.begin(SSID.c_str(), PWD.c_str()); 
    for (int i = 0; i < 20; i++) {
      Serial.print(".");
      delay(500);
      if (WiFi.status() == WL_CONNECTED)  break;
    }

    configured = WiFi.status() == WL_CONNECTED;
  }
  
  if (configured) {
    
    Serial.print("\nConnected! IP Address: ");
    Serial.println(WiFi.localIP()); 
  
    server.on("/", root);     
    server.on("/on", turnOn);   
    server.on("/off", turnOff);   
  
    
    server.on("/api", HTTP_GET, api);  
    server.on("/api", HTTP_POST, api); 
    
    server.on("/configure", HTTP_GET, configure);  
    server.on("/configure", HTTP_POST, configure); 
    
    digitalWrite(STATUS, LOW);
     
  } else {
    
    Serial.println("Dropping into AP mode for configuration.");
    
    WiFi.softAP("30AmpSwitch", "30AmpSwitch");
    
    Serial.print("IP address: ");
    Serial.println(WiFi.softAPIP());

    server.on("/", HTTP_GET, configure);  
    server.on("/", HTTP_POST, configure); 
  }
          
  server.begin();     
   
}    
       
void loop() {    
  server.handleClient();
}
