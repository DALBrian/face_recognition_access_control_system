#include "WiFi.h"
#include "StreamIO.h"
#include "VideoStream.h"
#include "RTSP.h"
#include "NNFaceDetectionRecognition.h" // For Face Recognition
#include "VideoStreamOverlay.h" //Show Face Recognition Result in Streaming Video
#include <PubSubClient.h> // For MQTT
#include <string.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <time.h>
/*
@author: Small Brian
@date: 20230110
@brief: Use RTSP streaming to view camera live stream remotely. Use NN model for face recognization and upload id/name/image/timestamp to MQTT
@hardware: Realtek AMB82-mini
@version: 1.2
@prev_version mqtt_test5 v 1.1
@add: Network Time Protocol for timestamp.
@todo: Fine tuning on MQTT transmition rate.
@note: AMB82 v4.0.5 board manager doesn't come with NTPClient, manually added to library from previous version.
*/
// Camera channel definition
#define CHANNEL     0
#define CHANNELNN   3
// Resultion for video streaming
#define WIDTH 800
#define HEIGHT 600
// Customised resolution for NN
#define NNWIDTH 576
#define NNHEIGHT 320
// Wi-Fi setting
char* ssid = "Homesweethome";
char* pass = "16291629";
WiFiClient WifiClient; 
WiFiUDP ntpUDP;
// Network time server setting
NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", 28800, 60000);
// Video stream setting
VideoSetting configMQTT(WIDTH, HEIGHT, 10, VIDEO_JPEG, 1); //VIDEO_JPEG for MQTT, VIDEO_H264 for streaming test; snapshop = 1 to enable getImage()
VideoSetting configNN(NNWIDTH, NNHEIGHT, 10, VIDEO_RGB, 0); //Neural Network
StreamIO videoStreamer(1, 1);
StreamIO videoStreamerRGBFD(1,1);
RTSP rtsp;
// MQTT Setting
char *MQTTServer = "192.168.68.103"; //MQTT server IP
int MQTTPort = 1883;
char* MQTTUser = "";
char* MQTTPassword = "";
char* MQTTImgTopic = "Door1/img";
char* MQTTIdTopic = "Door1/id";
char* MQTTNameTopic = "Door1/name";
char* MQTTTimeTopic = "Door1/time";
// MQTT update rate
long MQTTLastPublishTime;
long MQTTPublishInterval = 1000;   
PubSubClient MQTTClient(WifiClient);
// Face recognition setting
NNFaceDetectionRecognition facerecog;
// Others Setting
uint32_t img_addr = 0;
uint32_t img_len = 0;
size_t unknown_times = 0;
/* Main code */
void setup() {
  Serial.begin(115200);
  Serial.println("System starting up!");
  if (WiFi.status() != WL_CONNECTED) WiFiConnect();
  // Configure time server
  timeClient.begin();
  // Configure Camera channel
  configMQTT.setBitrate(2 * 1024 * 1024);
  Camera.configVideoChannel(CHANNEL, configMQTT);
  Camera.configVideoChannel(CHANNELNN, configNN);
  Camera.videoInit();
  // Configure RTSP server
  rtsp.configVideo(configMQTT);
  rtsp.begin();
  videoStreamer.registerInput(Camera.getStream(CHANNEL));
  videoStreamer.registerOutput(rtsp);
  if (videoStreamer.begin() != 0) Serial.println("StreamIO link start failed");
  delay(1000);
  // Configure face recognization
  facerecog.configVideo(configNN);
  facerecog.modelSelect(FACE_RECOGNITION, NA_MODEL, DEFAULT_SCRFD, DEFAULT_MOBILEFACENET);
  facerecog.begin();
  facerecog.setResultCallback(facerecog_callback);
  //
  videoStreamerRGBFD.registerInput(Camera.getStream(CHANNELNN));
  videoStreamerRGBFD.setStackSize();
  videoStreamerRGBFD.setTaskPriority();
  videoStreamerRGBFD.registerOutput(facerecog);
  if (videoStreamerRGBFD.begin() != 0) Serial.println("StreamIO link start failed");
  Camera.channelBegin(CHANNEL);
  Camera.channelBegin(CHANNELNN);
  // Start Over-screen Display(OSD)
  OSD.configVideo(CHANNEL, configMQTT);
  OSD.begin();
  // Print out all information for debugging
  printInfo();
}
void loop() {
  // put your main code here, to run repeatedly:
  if (WiFi.status() != WL_CONNECTED) WiFiConnect();
  if (!MQTTClient.connected()) MQTTConnect();

  if ((millis() - MQTTLastPublishTime) >= MQTTPublishInterval){
    // SendImageMQTT();
    MQTTLastPublishTime = millis();
  }
  if (Serial.available() > 0) kb_command();
  delay(2000);
  OSD.createBitmap(CHANNEL);
  OSD.update(CHANNEL);
}

/* Connection subfunction */
void WiFiConnect(){
  WiFi.begin(ssid, pass);
  delay(500);
  while (WiFi.status() != WL_CONNECTED){
    Serial.print(".");
  }
}
void MQTTConnect(){
  Serial.println("Connecting to MQTT server.");
  MQTTClient.setServer(MQTTServer, MQTTPort);
  while (!MQTTClient.connected()){
    String MQTTClientid = "amb82-" + String(random(1000000, 9999999));
    if (MQTTClient.connect(MQTTClientid.c_str(), MQTTUser, MQTTPassword)){
      Serial.println("MQTT connected");
    }else{
      Serial.print("MQTTConnect fail:");
      Serial.println(MQTTClient.state());
      delay(5000);
    }
  }
}

/* MQTT upload function */
void SendImageMQTT(){
  // Serial.println("Transmitting image to MQTT broker");
  // Initialization
  int buf = 8192; //Transmition size every time
  Camera.getImage(CHANNEL, &img_addr, &img_len);
  printf("img_len: %i \n", img_len);
  if (img_len > 0){
    MQTTClient.beginPublish(MQTTImgTopic, img_len, false);
    uint8_t* fbBuf = (uint8_t*) img_addr;
    size_t fbLen = img_len; //Image size
    //Transmit piece by piece
    for (size_t n = 0; n < fbLen; n+= buf){
      if (n+buf < fbLen){
        MQTTClient.write(fbBuf, buf);
        fbBuf += buf;
      }else if (fbLen % buf > 0){
        size_t remainder = fbLen % buf;
        MQTTClient.write(fbBuf, remainder);
      }
    }
    boolean isPublished = MQTTClient.endPublish();
    if (isPublished) Serial.println("MQTT transmition successful!");
    else Serial.println("MQTT trasmition fail!");
  }
  else{
    Serial.print("img_len: ");
    Serial.println(img_len);
  }
}
void SendTextMQTT(const char *id, const char *name, const char *timestamp){
  // if(!MQTTClient.publish(MQTTIdTopic, id) && !MQTTClient.publish(MQTTNameTopic, name) && !MQTTClient.publish(MQTTTimeTopic, timestamp)){
  //   Serial.println("MQTT text publish fail.");
  // }
  // else Serial.println("MQTT text publish successful.");
  MQTTClient.publish(MQTTIdTopic, id);
  MQTTClient.publish(MQTTNameTopic, name);
  MQTTClient.publish(MQTTTimeTopic, timestamp);
  Serial.println("Text sended.");
}

/*  NN modle callback */
void facerecog_callback(std::vector<FaceRecognitionResult> results){
  uint16_t image_width = configMQTT.width();
  uint16_t image_height = configMQTT.height();
  uint16_t result_count = facerecog.getResultCount();
  OSD.createBitmap(CHANNEL);
  if (result_count > 0){
    for (size_t i = 0; i < (int) result_count; i++){
      FaceRecognitionResult obj = results[i];
      int xmin = (int)(obj.xMin() * image_width);
      int xmax = (int)(obj.xMax() * image_width);
      int ymin = (int)(obj.yMin() * image_height);
      int ymax = (int)(obj.yMax() * image_height);

      uint32_t osd_color;
      printf("The name is: %s \n", obj.name());
      if (String(obj.name()) == String("unknown")){
        osd_color = OSD_COLOR_RED;
        unknown_times += 1;
      }else{
        osd_color = OSD_COLOR_GREEN;
        unknown_times = 0;
      }
      printf("Unknown times: %i \n", unknown_times);
      if (unknown_times % 5 == 1) {
        SendImageMQTT();
        String id = "1";
        SendTextMQTT(id.c_str(), obj.name(), get_datetime().c_str());
      }
      OSD.drawRect(CHANNEL, xmin, ymin, xmax, ymax, 3, osd_color);
      char name[64];
      char unknown_times_[64];
      snprintf(name, sizeof(name), "Face: %s", obj.name());
      sprintf(unknown_times_, "%zu", unknown_times);
      OSD.drawText(CHANNEL,  xmin,  ymin - OSD.getTextHeight(CHANNEL), name, osd_color); 
      // char charValue = static_cast<char>(unknown_times);
      OSD.drawText(CHANNEL, 0, 0, unknown_times_, osd_color);
    }
  }
  OSD.update(CHANNEL);
}

/* keyboard input */
void kb_command(){
    String input = Serial.readString();
    input.trim();
    if (input.startsWith(String("REG="))){
      String name = input.substring(4);
      facerecog.registerFace(name);
    }else if(input.startsWith(String("DEL="))){
      String name = input.substring(4);
      facerecog.removeFace(name);
    }else if(input.startsWith(String("RESET=")))facerecog.resetRegisteredFace();
    else if(input.startsWith(String("BACKUP=")))facerecog.backupRegisteredFace();
    else if(input.startsWith(String("RESTORE=")))facerecog.restoreRegisteredFace();
}

/* Get timestamp and date */
String get_datetime(){
  timeClient.update();
  time_t epochTime = timeClient.getEpochTime();
  int currentHour = timeClient.getHours();
  int currentMinute = timeClient.getMinutes();
  int currentSecond = timeClient.getSeconds();
  struct tm *ptm = gmtime ((time_t *)&epochTime); 
  int currentYear = ptm->tm_year+1900;
  int currentMonth = ptm->tm_mon+1;
  int monthDay = ptm->tm_mday;
  String datetime = String(currentYear) + "-" + String(currentMonth) + "-" + String(monthDay) +
  "_"+ String(currentHour) + ":" + String(currentMinute) + ":" + String(currentSecond);
  Serial.print("Current datetime: ");
  Serial.println(datetime);
  return datetime;
}

/* Print essential information */
void printInfo() {
  Serial.println("------------------------------");
  Serial.println("- Summary of Streaming -");
  Serial.println("------------------------------");
  Camera.printInfo();
  Serial.println("------------------------------");
  Serial.println("- RTSP Information -");
  Serial.print("rtsp://");
  Serial.print(WiFi.localIP());
  Serial.print(":");
  Serial.println(rtsp.getPort());
  Serial.println("------------------------------");
  delay(2000);
}
