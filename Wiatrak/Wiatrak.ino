#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include "DHT.h"
#include <ESP_Mail_Client.h>

#define DHTPIN_B D3
#define DHTPIN_H D4
#define FANPIN D5
#define SWITCHPIN D6

// Pin 15 can work but DHT must be disconnected during program upload.

#define DHTTYPE DHT11

#define SMTP_HOST "smtp.gmail.com"
#define SMTP_PORT esp_mail_smtp_port_465
#define AUTHOR_EMAIL "mik.arduino.mik@gmail.com"
#define AUTHOR_PASSWORD "ArduinO2Mik"

const char *ssid = "ElaJan";       // Enter SSID here
const char *password = "39396T50"; // Enter Password here

const long T1000 = 1000;
const long TFanStart = 10000;
const long TFanStop = 30000;

const int fanTreshold = 6;

ESP8266WebServer server(80);
SMTPSession smtp;

DHT bath(DHTPIN_B, DHTTYPE);
DHT house(DHTPIN_H, DHTTYPE);

bool webSwitch = false;
bool isFanWork = false;
bool switchStatus = false;
float bathT = 0;
float bathH = 0;
float houseT = 0;
float houseH = 0;

unsigned long timer1 = 0;
unsigned long fanStartTimer = 0;
unsigned long fanStopTimer = 0;
bool stopTimerStarted = false;
bool startTimerStarted = false;

void setup()
{
  Serial.begin(9600);
  delay(1000);

  Serial.println("Wiatrak");
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(FANPIN, OUTPUT);
  pinMode(SWITCHPIN, INPUT);

  ledOff(false);
  fanWork(false);

  bath.begin();
  house.begin();

  Serial.print("Connecting to: ");
  Serial.println(ssid);

  // connect to your local wi-fi network
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected..!");
  Serial.print("Got IP: ");
  Serial.println(WiFi.localIP());

  sendMailWithIP();

  server.on("/", handle_OnConnect);
  server.on("/webSwitchOn", handle_switchOn);
  server.on("/webSwitchOff", handle_switchOff);

  server.onNotFound(handle_NotFound);

  server.begin();
  Serial.println("HTTP server started");
}

void loop()
{
  server.handleClient();

  readSensors();
  readPins();

  if (millis() > timer1 + T1000)
  {
    timer1 = millis();
    bool off = !digitalRead(LED_BUILTIN);
    ledOff(off);
  }
  fanControl();
}

void fanControl()
{
  bool oldFan = isFanWork;

  if (isFanWork)
  {
    if (bathH - houseH <= fanTreshold)
    {
      if (stopTimerStarted)
      {
        if (millis() > fanStopTimer + TFanStop)
        {
          isFanWork = false;
          startTimerStarted = false;
          Serial.println("StartTimrer - stoped");
        }
      }
      else
      {
        fanStopTimer = millis();
        stopTimerStarted = true;
        Serial.println("StopTimer - started");
      }
    }
  }
  else
  {
    if (startTimerStarted)
    {
      if (millis() > fanStartTimer + TFanStart)
      {
        if (bathH - houseH > fanTreshold)
        {
          isFanWork = true;
          stopTimerStarted = false;
          Serial.println("StopTimre - stoped");
        }
      }
    }
    else
    {
      if (switchStatus)
      {
        fanStartTimer = millis();
        startTimerStarted = true;
        Serial.println("StartTimer - started");
      }
    }
  }

  if (webSwitch)
  {
    if (isFanWork != oldFan)
    {
      fanWork(isFanWork);
    }
  }
  else
  {
    fanWork(false);
  }
}

void handle_switchOn()
{
  Serial.println("Web Switch ON");
  // switchStatus = true;
  // digitalWrite(FANPIN, LOW);
  webSwitch = true;
  handle_OnConnect();
}

void handle_switchOff()
{
  Serial.println("Web Switch OFF");
  // digitalWrite(FANPIN, HIGH);
  // switchStatus = false;
  webSwitch = false;
  handle_OnConnect();
}

void handle_OnConnect()
{
  server.send(200, "text/html", SendHTML(bathT, bathH, houseT, houseH, isFanWork, switchStatus, webSwitch));
}

void handle_NotFound()
{
  server.send(404, "text/plain", "Not found");
}

String SendHTML(float bathT, float bathH, float houseT, float houseH, bool isFanWork, bool switchStatus, bool webSwitch)
{
  String ptr = "<!DOCTYPE html> <html>\n";
  ptr += "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
  ptr += "<title>Sterowanie wiatrakiem</title>\n";
  //  ptr +="<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}\n";
  //  ptr +="body{margin-top: 50px;} h1 {color: #444444;margin: 50px auto 30px;}\n";
  //  ptr +="p {font-size: 24px;color: #444444;margin-bottom: 10px;}\n";
  //  ptr +="</style>\n";
  ptr += "<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}\n";
  ptr += "body{margin-top: 50px;} h1 {color: #444444;margin: 50px auto 30px;} h3 {color: #444444;margin-bottom: 50px;}\n";
  ptr += ".button {display: block;width: 80px;background-color: #1abc9c;border: none;color: white;padding: 13px 30px;text-decoration: none;font-size: 25px;margin: 0px auto 35px;cursor: pointer;border-radius: 4px;}\n";
  ptr += ".button-on {background-color: #1abc9c;}\n";
  ptr += ".button-on:active {background-color: #16a085;}\n";
  ptr += ".button-off {background-color: #34495e;}\n";
  ptr += ".button-off:active {background-color: #2c3e50;}\n";
  ptr += "p {font-size: 14px;color: #888;margin-bottom: 10px;}\n";
  ptr += "</style>\n";
  //  ptr +="<meta http-equiv=\"refresh\"content=\"2\">\n";

  ptr += "<script>\n";
  ptr += "setInterval(loadDoc,1000);\n";
  ptr += "function loadDoc() {\n";
  ptr += "var xhttp = new XMLHttpRequest();\n";
  ptr += "xhttp.onreadystatechange = function() {\n";
  ptr += "if (this.readyState == 4 && this.status == 200) {\n";
  ptr += "document.getElementById(\"webpage\").innerHTML =this.responseText}\n";
  ptr += "};\n";
  ptr += "xhttp.open(\"GET\", \"/\", true);\n";
  ptr += "xhttp.send();\n";
  ptr += "}\n";
  ptr += "</script>\n";

  ptr += "</head>\n";
  ptr += "<body>\n";
  ptr += "<div id=\"webpage\">\n";
  ptr += "<h1>Sterowanie Wiatrakiem</h1>\n";

  ptr += "<h3>ESP8266 NodeMCU Weather Report</h3>\n";

  ptr += "<h2>Lazienka</h2>\n";

  ptr += "<p>Temperatura: ";
  ptr += bathT;
  ptr += "°C</p>";
  ptr += "<p>Wilgotnosc: ";
  ptr += (int)bathH;
  ptr += "%</p>";

  ptr += "<h2>Dom</h2>\n";

  ptr += "<p>Temperatura: ";
  ptr += houseT;
  ptr += "°C</p>";
  ptr += "<p>Wilgotnosc: ";
  ptr += (int)houseH;
  ptr += "%</p>";

  if (webSwitch)
  {
    ptr += "<h2>Web switch: ON</h2><a class=\"button button-off\" href=\"/webSwitchOff\">OFF</a>\n";
  }
  else
  {
    ptr += "<h2>Web switch: OFF</h2><a class=\"button button-on\" href=\"/webSwitchOn\">ON</a>\n";
  }

  if (switchStatus)
  {
    ptr += "<h2>Światło: ON</h2>\n";
  }
  else
  {
    ptr += "<h2>Światło: OFF</h2>\n";
  }

  if (isFanWork)
  {
    ptr += "<h2>Wiatrak: ON</h2>\n";
  }
  else
  {
    ptr += "<h2>Wiatrak: OFF</h2>\n";
  }

  ptr += "</div>\n";
  ptr += "</body>\n";
  ptr += "</html>\n";
  return ptr;
}

void readSensors()
{
  bathT = bath.readTemperature();
  bathH = bath.readHumidity();
  houseT = house.readTemperature();
  houseH = house.readHumidity();
}

void readPins()
{
  bool old = switchStatus;
  switchStatus = digitalRead(SWITCHPIN);
  if (old != switchStatus)
  {
    if (switchStatus)
    {
      Serial.println("Światło: ON");
    }
    else
    {
      Serial.println("Światło OFF");
    }
  }
}

// Function On or Off buildin led
void ledOff(bool off)
{
  if (off)
  {
    digitalWrite(LED_BUILTIN, HIGH);
  }
  else
  {
    digitalWrite(LED_BUILTIN, LOW);
  }
}

void fanWork(bool work)
{
  bool current = !digitalRead(FANPIN);
  if (work)
  {
    if(work != current)
    {
      Serial.println("Wiatrak ON");
    }
    digitalWrite(FANPIN, LOW);
  }
  else
  {
    if(work != current)
    {
      Serial.println("Wiatrak OFF");
    }
    digitalWrite(FANPIN, HIGH);
  }
}

void sendMailWithIP()
{
  smtp.debug(1);

  /* Set the callback function to get the sending results */
  smtp.callback(smtpCallback);

  /* Declare the session config data */
  ESP_Mail_Session session;

  session.server.host_name = SMTP_HOST;
  session.server.port = SMTP_PORT;
  session.login.email = AUTHOR_EMAIL;
  session.login.password = AUTHOR_PASSWORD;
  // session.login.user_domain = "mydomain.net";

  /* Declare the message class */
  SMTP_Message message;

  /* Set the message headers */
  message.sender.name = "Arduino Lazienka";
  message.sender.email = AUTHOR_EMAIL;
  message.subject = "New IP of Arduino";
  message.addRecipient("Owner", "19mik10@gmail.com");

  String textMsg = "Current IP: " + WiFi.localIP().toString();
  message.text.content = textMsg.c_str();

  message.text.charSet = "us-ascii";

  message.text.transfer_encoding = Content_Transfer_Encoding::enc_7bit;

  /** The message priority
   * esp_mail_smtp_priority_high or 1
   * esp_mail_smtp_priority_normal or 3
   * esp_mail_smtp_priority_low or 5
   * The default value is esp_mail_smtp_priority_low
   */
  message.priority = esp_mail_smtp_priority::esp_mail_smtp_priority_high;

  // message.response.reply_to = "someone@somemail.com";
  // message.response.return_path = "someone@somemail.com";

  /** The Delivery Status Notifications e.g.
   * esp_mail_smtp_notify_never
   * esp_mail_smtp_notify_success
   * esp_mail_smtp_notify_failure
   * esp_mail_smtp_notify_delay
   * The default value is esp_mail_smtp_notify_never
   */
  message.response.notify = esp_mail_smtp_notify_success | esp_mail_smtp_notify_failure | esp_mail_smtp_notify_delay;

  /* Set the custom message header */
  message.addHeader("Message-ID: <mik.arduino@gmail.com>");

  // For Root CA certificate verification (ESP8266 and ESP32 only)
  // session.certificate.cert_data = rootCACert;
  // or
  // session.certificate.cert_file = "/path/to/der/file";
  // session.certificate.cert_file_storage_type = esp_mail_file_storage_type_flash; // esp_mail_file_storage_type_sd
  // session.certificate.verify = true;

  // The WiFiNINA firmware the Root CA certification can be added via the option in Firmware update tool in Arduino IDE

  /* Connect to server with the session config */
  if (!smtp.connect(&session))
    return;

  /* Start sending Email and close the session */
  if (!MailClient.sendMail(&smtp, &message))
    Serial.println("Error sending Email, " + smtp.errorReason());

  // to clear sending result log
  // smtp.sendingResult.clear();

  // ESP_MAIL_PRINTF("Free Heap: %d\n", MailClient.getFreeHeap());
}

/* Callback function to get the Email sending status */
void smtpCallback(SMTP_Status status)
{
  /* Print the current status */
  Serial.println(status.info());

  /* Print the sending result */
  if (status.success())
  {
    Serial.println("----------------");
    Serial.printf("Message sent success: %d\n", status.completedCount());
    Serial.printf("Message sent failled: %d\n", status.failedCount());
    Serial.println("----------------\n");
    struct tm dt;

    for (size_t i = 0; i < smtp.sendingResult.size(); i++)
    {
      /* Get the result item */
      SMTP_Result result = smtp.sendingResult.getItem(i);
      // time_t ts = (time_t)result.timestamp;
      // localtime_r(&ts, &dt);

      Serial.printf("Message No: %d\n", i + 1);
      Serial.printf("Status: %s\n", result.completed ? "success" : "failed");
      // Serial.println("Date/Time: %d/%d/%d %d:%d:%d\n", dt.tm_year + 1900, dt.tm_mon + 1, dt.tm_mday, dt.tm_hour, dt.tm_min, dt.tm_sec);
      Serial.printf("Recipient: %s\n", result.recipients);
      Serial.printf("Subject: %s\n", result.subject);
    }
    Serial.println("----------------\n");

    // You need to clear sending result as the memory usage will grow up as it keeps the status, timstamp and
    // pointer to const char of recipients and subject that user assigned to the SMTP_Message object.

    // Because of pointer to const char that stores instead of dynamic string, the subject and recipients value can be
    // a garbage string (pointer points to undefind location) as SMTP_Message was declared as local variable or the value changed.

    // smtp.sendingResult.clear();
  }
}
