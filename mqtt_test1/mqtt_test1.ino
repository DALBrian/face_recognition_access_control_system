#include "WiFi.h"
#include "StreamIO.h"
#include "VideoStream.h"
#include "RTSP.h"
#include "NNFaceDetectionRecognition.h" // For Face Recognition
#include "VideoStreamOverlay.h"
#include <PubSubClient.h> // For MQTT

#define CHANNEL     0
#define CHANNELNN   3

// Customised resolution for NN
#define NNWIDTH     576
#define NNHEIGHT    320
// VideoSetting configV(VIDEO_HD, 30, VIDEO_H264, 0);
// VideoSetting configV(VIDEO_FHD, CAM_FPS, VIDEO_H264, 0);
VideoSetting configV(CHANNEL);
VideoSetting configNN(NNWIDTH, NNHEIGHT, 10, VIDEO_RGB, 0);
NNFaceDetectionRecognition facerecog;
RTSP rtsp;
StreamIO videoStreamer(1, 1);
StreamIO videoStreamerFDFR(1,1);
StreamIO videoStreamerRGBFD(1,1);
// Wi-Fi Setting
char ssid[] = "Homesweethome";
char pass[] = "16291629";
WiFiClient WiFiClient;
// MQTT Setting
char* MQTTServer = "mqttgo.io"; //My server IP
int MQTTPort = 1883;
// char* MQTTUser = "Brian";
// char* MQTTPassword = "a33769900";
char* MQTTUser = "";
char* MQTTPassword = "";
char* MQTTPubTopic1 = "你的主題名稱/class205/pic222";
long MQTTLastPublishTime;
long MQTTPublishInterval = 100;    
PubSubClient MQTTClient(WiFiClient);
// Others Setting
int PicID = 0;
uint32_t img_addr = 0;
uint32_t img_len = 0;


void setup() {
  // System startup
  Serial.begin(115200);
  Serial.println("System starting up!");
  WiFiConnect();
  MQTTClient.connected();
  delay(500);
  Camera.configVideoChannel(0, configV);
  Camera.videoInit();
  rtsp.configVideo(configV);
  rtsp.begin();
  Serial.println("Video streaming started!");
  videoStreamer.registerInput(Camera.getStream(0));
    videoStreamer.registerOutput(rtsp);
    if (videoStreamer.begin() != 0) Serial.println("StreamIO link start failed");
  Camera.channelBegin(0);
  delay(1000);
  printInfo();
}

void loop() {
  // put your main code here, to run repeatedly:
  if (WiFi.status() != WL_CONNECTED){
    WiFiConnect();
  }
  if (!MQTTClient.connected()){
    MQTTConnecte();
  }
  if ((millis() - MQTTLastPublishTime) >= MQTTPublishInterval){
    String payload = SendImageMQTT();
    Serial.println(payload);
    MQTTLastPublishTime = millis();
    delay(10);
  }
}
void printInfo(void) {
  Serial.println("------------------------------");
  Serial.println("- Summary of Streaming -");
  Serial.println("------------------------------");
  Camera.printInfo();
  IPAddress ip = WiFi.localIP();
  Serial.println("------------------------------");
  Serial.println("- RTSP Information -");
  Serial.print("rtsp://");
  Serial.print(ip);
  Serial.print(":");
  rtsp.printInfo();
  Serial.println("------------------------------");
}

// String SendImageMQTT(){
//   Serial.println("Transmitting image to MQTT broker");
//   // Initialization
//   int buf = 8192; //Transmition size every time
//   Camera.getImage(1, &img_addr, &img_len);
//   MQTTClient.beginPublish(MQTTPubTopic1, img_len, false);
//   uint8_t* fbBuf = (uint8_t*) img_addr;
//   size_t fbLen = img_len; //Image size
//   //Transmit piece by piece
//   for (size_t n = 0; n < fbLen; n+= buf){
//     if (n+buf < fbLen){
//       MQTTClient.write(fbBuf, buf);
//       fbBuf += buf;
//     }else if (fbLen % buf > 0){
//       size_t remainder = fbLen % buf;
//       MQTTClient.write(fbBuf, remainder);
//     }
//   }
//   boolean isPublished = MQTTClient.endPublish();
//   if (isPublished) return "MQTT transmition successful!";
//   else return "MQTT trasmition fail!";
// }

// String SendImageMQTT() {
//   int buf = 8192;
//   Camera.getImage(1, &img_addr, &img_len);
//   //int ps = 512;
//   //開始傳遞影像檔，批次傳檔案
//   MQTTClient.beginPublish(MQTTPubTopic1, img_len, false);

//   uint8_t* fbBuf = (uint8_t*)img_addr;
//   size_t fbLen = img_len;
//   for (size_t n = 0; n < fbLen; n = n + buf) {
//     if (n + buf < fbLen) {
//       MQTTClient.write(fbBuf, buf);
//       fbBuf += buf;
//     } else if (fbLen % buf > 0) {
//       size_t remainder = fbLen % buf;
//       MQTTClient.write(fbBuf, remainder);
//     }
//   }
//   boolean isPublished = MQTTClient.endPublish();
//   if (isPublished) return "MQTT傳輸成功";
//   else return "MQTT傳輸失敗，請檢查網路設定";
// }
String SendImageMQTT() {
  MQTTClient.publish(MQTTPubTopic1, "Hello world");
  boolean isPublished = MQTTClient.endPublish();
  if (isPublished) return "MQTT傳輸成功";
  else return "MQTT傳輸失敗，請檢查網路設定";
}

void WiFiConnect(){
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED){
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("WiFi connected,IP address:");
  Serial.println(WiFi.localIP());
}
// void MQTTConnect(){
//   Serial.println("Connecting to MQTT server.");
//   MQTTClient.setServer(MQTTServer, MQTTPort);
//   while (!MQTTClient.connected()){
//     String MQTTClientid = "amb82-" + String(random(1000000, 9999999));
//     if (MQTTClient.connect(MQTTClientid.c_str(), MQTTUser, MQTTPassword)){
//       Serial.println("MQTT connected");
//     }else{
//       Serial.print("MQTTConnect fail:");
//       Serial.println(MQTTClient.state());
//       delay(5000);
//     }
//   }
// }
void MQTTConnecte() {
  MQTTClient.setServer(MQTTServer, MQTTPort);
  //MQTTClient.setCallback(MQTTCallback);
  while (!MQTTClient.connected()) {
    //以亂數為ClientID
    String MQTTClientid = "esp32-" + String(random(1000000, 9999999));
    if (MQTTClient.connect(MQTTClientid.c_str(), MQTTUser, MQTTPassword)) {
      //連結成功，顯示「已連線」。
      Serial.println("MQTT已連線");
      //訂閱SubTopic1主題
      //MQTTClient.subscribe(MQTTSubTopic1);
    } else {
      //若連線不成功，則顯示錯誤訊息，並重新連線
      Serial.print("MQTT連線失敗,狀態碼=");
      Serial.println(MQTTClient.state());
      Serial.println("五秒後重新連線");
      delay(5000);
    }
  }
}