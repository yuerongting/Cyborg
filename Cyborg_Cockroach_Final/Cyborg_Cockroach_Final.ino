#include "esp_camera.h"
#include <WiFi.h>
#include <ArduinoWebsockets.h>
#include <SPI.h>
//
// WARNING!!! Make sure that you have either selected ESP32 Wrover Module,
//            or another board which has PSRAM enabled
//

// Select camera model
//#define CAMERA_MODEL_WROVER_KIT
//#define CAMERA_MODEL_ESP_EYE
//#define CAMERA_MODEL_M5STACK_PSRAM
//#define CAMERA_MODEL_M5STACK_WIDE
#define CAMERA_MODEL_AI_THINKER

#include "camera_pins.h"

const char* ssid = "SM-G981U78e";     //Internet Hotspot connection information
const char* password = "0529557722";
//const char* ssid = "Roy";
//const char* password = "meiliuliangle";
const char* websocket_server_host = "34.82.137.37"; //IP address for WebSocket
const uint16_t websocket_server_port = 65080;

// Initiate stimulus pin definitions 
int rightState = LOW;   // Initial state of right stimulus is LOW
int leftState = LOW;    // Initial state of left stimulus is LOW 
const byte csPot           = 15;      // Chip select pin for digital pot
const byte pot0            = 0x11;    // pot0 addr corresponding to Right Antenna Stimulus
const byte pot1            = 0x12;    // pot1 addr corresponding to Left Antenna Stimulus

SPIClass * hspi = NULL;      // uninitalised pointer to SPI object
long level = 62;             // defining "level" to be 62. This value corresponds to the 62nd level out of the 255 included in the MCP42100 digital pot. This was found to correspond to 1.2V output  

// Initiate timing variables 
long previousTimeL = 0;        // will store last time stimulus was sent (LED was flashed)
unsigned long currentTimeL;
long previousTimeR = 0;
unsigned long currentTimeR; 
long interval = 50;  // (milliseconds)

using namespace websockets;
WebsocketsClient client;

// Based on a given string, sets the appropriate GPIO to HIGH for stim_dur
void initStim(String msg){
  if(msg == "Right stimulus button activated"){
    Serial.println("Send right GPIO HIGH");
    previousTimeR = millis(); // Save the time the LED was turned on at 
    rightState = HIGH;
    //digitalWrite(rightStimPin, HIGH);
  }else{
    Serial.println("Send left GPIO HIGH");
    previousTimeL = millis(); // Save the time the LED was turned on at 
    leftState = HIGH;
    //digitalWrite(leftStimPin, HIGH);
  }
}

//void stimTimer(int pin, int state, long previousTime, unsigned long currentTime){
//  if((currentTime - previousTime > interval) && state == HIGH) {  
//    state = LOW;
//    Serial.println("turning left LED back off");
//    digitalWrite(pin, state);
//    return; 
//  }
//}

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 10000000; // 10MHz frequency
  config.pixel_format = PIXFORMAT_JPEG;

  // Set up output GPIOs for stimulus
//  pinMode(33, OUTPUT); // Integrated LED GPIO output LED 
  
 // pinMode(rightStimPin, OUTPUT); // Right stimulus GPIO
 //pinMode(leftStimPin, OUTPUT); // Left stimulus GPIO
  
  //init with high specs to pre-allocate larger buffers
  if(psramFound()){
    config.frame_size = FRAMESIZE_VGA;
    config.jpeg_quality = 40;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }


  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

 
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");

  Serial.print("Camera Ready! Use 'http://");
  Serial.print(WiFi.localIP());
  Serial.println("' to connect");

  /////////////
  /// Websockets block 
  client.onMessage([](WebsocketsMessage msg){ 
      Serial.println("Got Message: " + msg.data());
      // Check for stimulus messages 
      if(msg.data() == "Right stimulus button activated" || msg.data()== "Left stimulus button activated"){ // If the message is stimulus message, call initStim
        initStim(msg.data());
      }
  });
  /////////////
  
  while(!client.connect(websocket_server_host, websocket_server_port, "/")){ //ASK RONGTING
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("Websocket Connected!");
  digitalWrite(csPot, HIGH);        // chip select default to de-selected
  pinMode(csPot, OUTPUT);
  hspi = new SPIClass(HSPI);        // Initialize HSPI instance of SPIClass
  hspi->begin();                    // Initialize HSPI with default IO pins: MOSI 13, SCLK 14
}

void setPotWiper(int addr, int pos) {

  pos = constrain(pos, 0, 100);            // limit wiper setting to range of 0 to 100
  digitalWrite(csPot, LOW);                // select chip
  hspi->transfer(addr);                    // configure target pot 
  hspi->transfer(pos);                     // configure wiper position 
  digitalWrite(csPot, HIGH);               // de-select chip
  
}
  
void loop() { 
  /////////////
  /// Websockets block 
  client.poll();  
  /////////////

  ///////////
  // Stimulus timing block
  
  currentTimeL = millis();
  currentTimeR = millis();
 
  if((currentTimeL - previousTimeL > interval) && leftState == HIGH) {  
    
    leftState = LOW;
    Serial.println("turning left LED back off");
    for (int i = 0; i <= 27; i++) {     // for loop with mini delays to generate monopole train pulse
      setPotWiper(pot1, level);
      delay(9);
      setPotWiper(pot1, 0);
      delay(9);
    }
    delay(50);
    setPotWiper(pot1, 0);
  }
  if((currentTimeR - previousTimeR > interval) && rightState == HIGH) {  
    rightState = LOW;
    Serial.println("turning right LED back off");
    //digitalWrite(rightStimPin, rightState);
    
    for (int i = 0; i <= 27; i++) {
      setPotWiper(pot0, level);
      delay(9);
      setPotWiper(pot0, 0);
      delay(9);
    }
    delay(50);  
    setPotWiper(pot0, 0);
  }
  /////////////
  
  camera_fb_t *fb = esp_camera_fb_get(); 
  if(!fb){
    Serial.println("Camera capture failed");
    esp_camera_fb_return(fb);
    return;
  }

  if(fb->format != PIXFORMAT_JPEG){
    Serial.println("Non-JPEG data not implemented");
    return;
  }

  client.sendBinary((const char*) fb->buf, fb->len);
  esp_camera_fb_return(fb);
}
