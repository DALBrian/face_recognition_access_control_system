
#include "WiFi.h"
#include "StreamIO.h"
#include "VideoStream.h"
#include "AudioStream.h"
#include "AudioEncoder.h"
#include "RTSP.h"
#include "AmebaFatFS.h"
#include <PubSubClient.h>  //請先安裝PubSubClient程式庫
AmebaFatFS fs;

VideoSetting configV(VIDEO_FHD, CAM_FPS, VIDEO_H264, 0);
VideoSetting configLine(800, 600, 10, VIDEO_JPEG, 1);
AudioSetting configA(0);
Audio audio;
AAC aac;
RTSP rtsp;
RTSP rtsp2;

StreamIO audioStreamer(1, 1);  // 1 Input Audio -> 1 Output AAC
StreamIO avMixStreamer(2, 1);  // 2 Input Video + Audio -> 1 Output RTSP

char ssid[] = "Homesweethome";         // your network SSID (name)
char pass[] = "16291629";  // your network password
String Linetoken = "你的Line密碼";  //Line Notify Token. You can set the value of xxxxxxxxxx empty if you don't want to send picture to Linenotify.

char* MQTTServer = "mqttgo.io";                   //免註冊MQTT伺服器
int MQTTPort = 1883;                              //MQTT Port
char* MQTTUser = "";                              //不須帳密
char* MQTTPassword = "";                          //不須帳密
char* MQTTPubTopic1 = "你的主題名稱/class205/pic1";  //推播主題1:即時影像
long MQTTLastPublishTime;                         //此變數用來記錄推播時間
long MQTTPublishInterval = 100;                   //每1秒推撥4-5次影像
WiFiClient WifiClient;
PubSubClient MQTTClient(WifiClient);

int PicID = 0;
uint32_t img_addr = 0;
uint32_t img_len = 0;



void setup() {
  Serial.begin(115200);

  // Configure camera video channel with video format information
  // Adjust the bitrate based on your WiFi network quality
  //configV.setBitrate(2 * 1024 * 1024);     // Recommend to use 2Mbps for RTSP streaming to prevent network congestion
  Camera.configVideoChannel(0, configV);
  Camera.configVideoChannel(1, configLine);
  Camera.videoInit();

  // Configure audio peripheral for audio data output
  audio.configAudio(configA);
  audio.begin();
  // Configure AAC audio encoder
  aac.configAudio(configA);
  aac.begin();

  // Configure RTSP with identical video format information and enable audio streaming
  rtsp.configVideo(configV);
  rtsp.configAudio(configA, CODEC_AAC);
  rtsp.begin();

  // Configure StreamIO object to stream data from audio channel to AAC encoder
  audioStreamer.registerInput(audio);
  audioStreamer.registerOutput(aac);
  if (audioStreamer.begin() != 0) {
    Serial.println("StreamIO link start failed");
  }

  // Configure StreamIO object to stream data from video channel and AAC encoder to rtsp output
  avMixStreamer.registerInput1(Camera.getStream(0));
  avMixStreamer.registerInput2(aac);
  avMixStreamer.registerOutput(rtsp);
  if (avMixStreamer.begin() != 0) {
    Serial.println("StreamIO link start failed");
  }
  // Start data stream from video channel
  Camera.channelBegin(0);
  Camera.channelBegin(1);

  delay(1000);
  printInfo();
  // SD card init
  if (!fs.begin()) Serial.println("記憶卡讀取失敗，請檢查記憶卡是否插入...");
  else Serial.println("記憶卡讀取成功");
}

void loop() {
  //如果Wifi連線中斷，則重啟Wifi連線
  if (WiFi.status() != WL_CONNECTED) WiFiConnect();
  //如果MQTT連線中斷，則重啟MQTT連線
  if (!MQTTClient.connected()) MQTTConnecte();

  if ((millis() - MQTTLastPublishTime) >= MQTTPublishInterval) {
    String payload = SendImageMQTT();
    Serial.println(payload);
    MQTTLastPublishTime = millis();  //更新最後傳輸時間
  }

  //String payload = SendImageLine("這是測試....");
  //Serial.println(payload);
  delay(10);
}


void printInfo(void) {
  Serial.println("------------------------------");
  Serial.println("- Summary of Streaming -");
  Serial.println("------------------------------");

  Camera.printInfo();

  IPAddress ip = WiFi.localIP();

  Serial.println("- RTSP Information -");
  Serial.print("rtsp://");
  Serial.print(ip);
  Serial.print(":");
  rtsp.printInfo();

  Serial.println("- Audio Information -");
  audio.printInfo();
}


String SendImageMQTT() {
  int buf = 8192;
  Camera.getImage(1, &img_addr, &img_len);
  //int ps = 512;
  //開始傳遞影像檔，批次傳檔案
  MQTTClient.beginPublish(MQTTPubTopic1, img_len, false);

  uint8_t* fbBuf = (uint8_t*)img_addr;
  size_t fbLen = img_len;
  for (size_t n = 0; n < fbLen; n = n + buf) {
    if (n + buf < fbLen) {
      MQTTClient.write(fbBuf, buf);
      fbBuf += buf;
    } else if (fbLen % buf > 0) {
      size_t remainder = fbLen % buf;
      MQTTClient.write(fbBuf, remainder);
    }
  }
  boolean isPublished = MQTTClient.endPublish();
  if (isPublished) return "MQTT傳輸成功";
  else return "MQTT傳輸失敗，請檢查網路設定";
}

void WiFiConnect() {
  Serial.print("開始連線到:");
  Serial.print(ssid);
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("WiFi connected,IP address:");
  Serial.println(WiFi.localIP());
}


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

//傳送照片到Line
String SendImageLine(String msg) {
  int buf = 4096;
  Camera.getImage(1, &img_addr, &img_len);

  WiFiSSLClient client_tcp;
  if (client_tcp.connect("notify-api.line.me", 443)) {
    Serial.println("連線到Line成功");
    //組成HTTP POST表頭
    String head = "--Cusboundary\r\nContent-Disposition: form-data;";
    head += "name=\"message\"; \r\n\r\n" + msg + "\r\n";
    head += "--Cusboundary\r\nContent-Disposition: form-data; ";
    head += "name=\"imageFile\"; filename=\"img.jpg\"";
    head += "\r\nContent-Type: image/jpeg\r\n\r\n";
    String tail = "\r\n--Cusboundary--\r\n";
    uint32_t imageLen = img_len;
    uint16_t extraLen = head.length() + tail.length();
    uint16_t totalLen = imageLen + extraLen;
    //開始POST傳送
    client_tcp.println("POST /api/notify HTTP/1.1");
    client_tcp.println("Connection: close");
    client_tcp.println("Host: notify-api.line.me");
    client_tcp.println("Authorization: Bearer " + Linetoken);
    client_tcp.println("Content-Length: " + String(totalLen));
    client_tcp.println("Content-Type: multipart/form-data; boundary=Cusboundary");

    client_tcp.println();
    client_tcp.print(head);
    uint8_t* fbBuf = (uint8_t*)img_addr;
    size_t fbLen = img_len;
    Serial.println("傳送影像檔...");
    for (size_t n = 0; n < fbLen; n = n + buf) {
      if (n + buf < fbLen) {
        client_tcp.write(fbBuf, buf);
        fbBuf += buf;
      } else if (fbLen % buf > 0) {
        size_t remainder = fbLen % buf;
        client_tcp.write(fbBuf, remainder);
      }
    }
    client_tcp.print(tail);
    client_tcp.println();
    String payload = "";
    boolean state = false;
    int waitTime = 5000;  //等候時間5秒鐘
    long startTime = millis();
    delay(1000);
    Serial.print("等候回應...");
    while ((startTime + waitTime) > millis()) {
      Serial.print(".");
      delay(100);
      while (client_tcp.available() && (startTime + waitTime) > millis()) {
        //已收到回覆，依序讀取內容
        char c = client_tcp.read();
        payload += c;
      }
    }
    client_tcp.stop();
    return payload;
  } else {
    return "傳送失敗，請檢查網路設定";
  }
}

void SaveImgTF(String filePathName) {
  File file = fs.open(filePathName);
  Camera.getImage(1, &img_addr, &img_len);
  boolean a = file.write((uint8_t*)img_addr, img_len);
  file.close();
  if (a) Serial.println(filePathName + " 存檔成功...");
  else Serial.println("存檔失敗，請檢查記憶卡...");
}


