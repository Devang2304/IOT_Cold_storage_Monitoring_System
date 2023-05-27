#include <ESP_Mail_Client.h>

#include <ThingSpeak.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include<ESP8266WiFi.h>  // wifi library

// code for connecting it to wifi
// the SSID (name) of the wifi network
const char* ssid = "PUT_WIFI_NAME_HERE";
const char* password = "PUT_PASSWORD_HERE";  // wifi password
WiFiClient client;
#define relayPin 12


// thingspeak api
unsigned long myChannelNumber = 1;
const char* myWriteAPIKey = "PUT_YOUR_THINGSPEAK_API_KEY";

// gmail notification details
#define SMTP_HOST "smtp.gmail.com" // SMTP_HOST
#define SMTP_PORT 465 // SMTP_PORT

// sign in credentials
// the account which NODEMCU will be using for sending email to owner
#define AUTHOR_EMAIL "PUT_GMAIL_ID_HERE"
// you will be using app password here which is created by google 
// 2 step verification so that you need to verify during sign in through nodemcu
#define AUTHOR_PASSWORD "PUT_YOUR_APP_PASSWORD_HERE" 

// the SMTP Session object used for Enail sending
SMTPSession smtp;

// declare the session config data
ESP_Mail_Session session;

// declare the message class
SMTP_Message message;

// variables declaration of temperature threshold for sending email
bool trigger_Send=true;
float temp_Threshold_above = 31.0;
float temp_Threshold_below = 29.0;

float t,h;
#define VIN 3.3 // V power voltage, 3.3v in case of NodeMCU
#define R 10000 // Voltage devider resistor value
float LDRvalue,luminance;

String textMsg; // variable to store the data that will be sen to email

// ESPTimeHelper ETH; // ESPTimeHelper declaration this is used to get time data from the server



#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

#define DHTPIN 14     // Digital pin connected to the DHT sensor

#define DHTTYPE    DHT11     // DHT 11


DHT dht(DHTPIN, DHTTYPE);

void setup() {
  Serial.begin(115200);
  //pinMode(relay,OUTPUT);
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, LOW);
  dht.begin();

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  delay(2000);
  display.clearDisplay();
  display.setTextColor(WHITE);

  connectToWiFi();
  ThingSpeak.begin(client);  // Initialize ThingSpeak

  sendEmail();
}

void loop() {
  delay(5000);

  LDRvalue = analogRead(A0);// read the input on analog pin 0
  luminance = conversion(LDRvalue); 
  Serial.print("Iluminance value = ");
  Serial.print(luminance); // Converted value
  Serial.println(" Lux"); 
  delay(500);

  //read temperature and humidity
   t = dht.readTemperature();
   h = dht.readHumidity();
  if (isnan(h) || isnan(t)) {
    Serial.println("Failed to read from DHT sensor!");
  }
  // clear display
  display.clearDisplay();
  
  // display temperature
  display.setTextSize(1);
  display.setCursor(0,0);
  display.print("Temperature: ");
  display.setTextSize(2);
  display.setCursor(0,10);
  display.print(t);
  display.print(" ");
  display.setTextSize(1);
  display.cp437(true);
  display.write(167);
  display.setTextSize(2);
  display.print("C");
  
  // display humidity
  display.setTextSize(1);
  display.setCursor(0, 35);
  display.print("Humidity: ");
  display.setTextSize(2);
  display.setCursor(0, 45);
  display.print(h);
  display.print(" %"); 
  
  display.display(); 

  int x = ThingSpeak.writeField(myChannelNumber, 1, t, myWriteAPIKey);
  int y = ThingSpeak.writeField(myChannelNumber, 2, h, myWriteAPIKey);
  int z = ThingSpeak.writeField(myChannelNumber, 5, luminance, myWriteAPIKey);

  if(x==200){
    Serial.println("Channel 1 update successful!");
  }else{
    Serial.println("problem updating channel. HTTP error code " + String(x));
  }

  

  if(y==200){
    Serial.println("Channel 2 update successful!");
  }else{
    Serial.println("problem updating field 2 of the channel. HTTP error code " + String(y));
  }

  
  if(z==200){
    Serial.println("Channel 3 update successful!");
  }else{
    Serial.println("problem updating field 3 of the channel. HTTP error code " + String(z));
  }


  // Serial.printf("Date/Time: %02d/%02d/%d %02d:%02d:%02d", ETH.getDay(), ETH.getMonth(), ETH.getYear(), ETH.getHour(), ETH.getMin(), ETH.getSec());
  Serial.println(F(" | Temperature: "));
  Serial.print(t);
  Serial.println(F(" *C"));
  Serial.println(F(" | Humidity: "));
  Serial.print(h);
  Serial.println(" %");
  Serial.println(F(" | luminance: "));
  Serial.print(luminance);
  Serial.println(" lux");

  if(t>temp_Threshold_above){
    pinMode(relayPin, OUTPUT);

  }
  else {
    pinMode(relayPin,INPUT); // Deactivate relay
  }

  // Conditions for sending email messages
  // when above the threshold value
  if(t>temp_Threshold_above && trigger_Send == true) {
    textMsg+= "Temperature above threshold value : " + String(temp_Threshold_above) + " *C" + "\n";
    Serial.print(textMsg);

    Serial.println("Send DHT11 sensor data via email");
    
    setTextMsg();
    sendTextMsg();
    textMsg = "";
    trigger_Send = false;
  }

  //when temperature value  is below the threshold value
  if(t>temp_Threshold_below && trigger_Send == false) {
    textMsg=textMsg+ "Temperature below threshold value : " + String(temp_Threshold_below) + " *C" + "\n";
    Serial.println("Send DHT11 sensor data via email");
    setTextMsg();
    sendTextMsg();
    textMsg = "";
    trigger_Send = true;
  }
  
  


  delay(3000);
}

void setTextMsg(){ // message to be send
  textMsg = textMsg + "Temperature : " + String(t) + " *C" + "\n" + "Humidity : " + String(h) + " %" +"\n" + "luminance :" + String(luminance) + "lux";
  message.text.content = textMsg.c_str();

}

void sendTextMsg(){

  // set the custom message header
  message.addHeader("Message-ID: <dht11.send@gmail.com>");

  // connect to server with the session config
  if(!smtp.connect(&session)) return;

  // Start sending  Email and close the session
  if(!MailClient.sendMail(&smtp, &message)) Serial.println("Error sending Email, " + smtp.errorReason());
}

void smtpCallback(SMTP_Status status) {
  // print current info
  Serial.println(status.info());

  // Print the sending result
  if(status.success()){
    Serial.printf("Message send success: %d\n", status.completedCount());
    Serial.printf("Message send failure: %d\n", status.failedCount());

    struct tm dt;

    for(size_t i=0; i<smtp.sendingResult.size(); i++){
      //get the result item

      SMTP_Result result = smtp.sendingResult.getItem(i);
      // localtime_r(&result.timestamp,&dt);

      Serial.printf("Message No: %d\n", i+1);
      Serial.printf("Status: %s\n", result.completed ? "success" : "failed");
      // Serial.printf("Date/Time: %02d/%02d/%d %02d:%02d:%02d\n", dt.tm_mday, dt.tm_mon +1, dt.tm_year + 1900, dt.tm_hour, dt.tm_min, dt.tm_sec);
      Serial.printf("Recipient: %s\n",result.recipients);
      Serial.printf("Subject: %s\n", result.subject);
    }
  }
}

void connectToWiFi() {
  Serial.println();
  Serial.println();
  Serial.print("Connecting  to WiFi");
  Serial.print("...");
  WiFi.begin(ssid,password);
  int retries = 0;
  while((WiFi.status() != WL_CONNECTED) && ( retries < 15 )){
    retries++;
    delay(500);
    Serial.print(".");
  }
  if(retries > 14){
    Serial.println(F("Wifi connection failed"));
  }

  if(WiFi.status()==WL_CONNECTED){
    Serial.println(F("WiFi connected"));
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
  }
  Serial.println(F("Setup Ready"));
}

void sendEmail(){

  // get the time data from the server
  // ETH.TZ =7; // GMT+7

  // Daylight saving time
  // ETH.DST_MN = 0;

  // Serial.println("Getting time data from server. Please wait....");
  // if(!ETH.setClock(ETH.TZ, ETH.DST_MN)){
  //   Serial.println("Can't set clock...");
  //   return;
  // }
  // Serial.println("Successfully get time data.");

  smtp.debug(1); // for debugging

  // set the session config
  session.server.host_name = SMTP_HOST;
  session.server.port = SMTP_PORT;
  session.login.email = AUTHOR_EMAIL;
  session.login.password = AUTHOR_PASSWORD;


  // message header
  message.sender.name = "Thnakyou";
  message.sender.email = AUTHOR_EMAIL;
  message.subject = "DTH11 sensor data report";
  message.addRecipient("PUT SENDER NAME OR ANYTHING HERE", "PUT RECIPIENT email id");

  // the plain text message character set
  message.text.charSet = "us-ascii";

  // the content transfer encoding
  message.text.transfer_encoding = Content_Transfer_Encoding::enc_7bit;

  // the message priority
  message.priority = esp_mail_smtp_priority::esp_mail_smtp_priority_high;

  message.response.notify = esp_mail_smtp_notify_success | esp_mail_smtp_notify_failure | esp_mail_smtp_notify_delay;
}

int conversion(int raw_val){
  // Conversion rule
  float Vout = float(raw_val) * (VIN / float(1023));// Conversion analog to voltage
  float RLDR = (R * (VIN - Vout))/Vout; // Conversion voltage to resistance
  int lux = 500/(RLDR/1000); // Conversion resitance to lumen
  return lux;
}