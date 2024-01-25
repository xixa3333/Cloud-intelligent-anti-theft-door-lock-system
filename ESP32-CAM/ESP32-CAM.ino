#include "esp_camera.h"//引入ESP32-CAM開發版的相機庫之標題檔
#include <WiFi.h>//引入ESP32-CAM開發版的WiFi庫之標題檔
#include <WiFiClientSecure.h>//引入建立與網站的HTTPS通訊函式庫之標題檔
#include "soc/soc.h"//引入客戶端與服務器程序函式庫之標題檔
#include "soc/rtc_cntl_reg.h"//引入操作實時時鐘控制寄存器函式庫之標題檔
#define CAMERA_MODEL_AI_THINKER
//定義CAMERA_MODEL_AI_THINKER 的符號常量，可利用這個符號常量表示相機型號
#include "camera_pins.h"
//將此標頭檔案引入進程式碼中，此標頭檔案定義了與相機引腳相關的常量和設置
const char* ssid = "xixa3333";//WiFi帳號
const char* password = "asdfghjkl";//WiFi密碼
String myLineNotifyToken = "70giismZ85IZ3FZ00VDrYNmZUwDCT5dEL3ups1Y6EhZ";
//LINE權杖

String sendImage2LineNotify(String msg) {//傳遞LINE訊息與照片之副程式
  Serial.println(msg);
    camera_fb_t* fb = NULL;//宣告指針變量為fb，類型為camera_fb_t*，並初始化為NULL
    fb = esp_camera_fb_get();//取得相機影像放置fb
    if (!fb) {//判斷沒取得相機影像
        delay(100);//延遲0.1秒
        Serial.println("Camera capture failed, Reset");
        ESP.restart();//重置ESP開發板
    }

    WiFiClientSecure client_tcp;//啟動SSL wificlient
    Serial.println("Connect to notify-api.line.me");
    if (client_tcp.connect("notify-api.line.me", 443)) {//判斷LINE與TCP在443端口連接成功
        Serial.println("Connection successful");
        String head = "--Taiwan\r\nContent-Disposition: form-data; name=\"message\"; \r\n\r\n" + msg + "\r\n--Taiwan\r\nContent-Disposition: form-data; name=\"imageFile\"; filename=\"esp32-cam.jpg\"\r\nContent-Type: image/jpeg\r\n\r\n";
        //宣告字串用於與HTTP協議進行文件上傳或表單提交
        String tail = "\r\n--Taiwan--\r\n";//宣告字串用於與HTTP協議要求的消息結束標示符
        uint16_t imageLen = fb->len;//獲取fb指針指向len屬性的值，表示為圖像文件數據長度
        uint16_t extraLen = head.length() + tail.length();//獲取頭部和尾部字符串的長度之和
        uint16_t totalLen = imageLen + extraLen;//獲取圖像文件數據與附加長度之和
        //開始POST傳送訊息
        client_tcp.println("POST /api/notify HTTP/1.1");
        client_tcp.println("Connection: close");
        client_tcp.println("Host: notify-api.line.me");
        client_tcp.println("Authorization: Bearer " + myLineNotifyToken);
        client_tcp.println("Content-Length: " + String(totalLen));
        client_tcp.println("Content-Type: multipart/form-data; boundary=Taiwan");
        client_tcp.println();
        client_tcp.print(head);
        uint8_t* fbBuf = fb->buf;//獲取指向圖像文件數據的指針
        size_t fbLen = fb->len;//獲取圖像文件數據長度
        Serial.println("Data Sending....");
        //照片，分段傳送

        for (size_t n = 0; n < fbLen; n = n + 2048) {//將圖像文件數據分包發送給服務器
            if (n + 2048 < fbLen) {//判斷數據包無超過2048字節
                client_tcp.write(fbBuf, 2048);//傳送2048字節的數據包給服務器
                fbBuf += 2048;//將指針移動到下個數據包起始位址
            }
            else if (fbLen % 2048 > 0) {//判斷還有剩下的數據包
                size_t remainder = fbLen % 2048;//取得剩下的數據包長度
                client_tcp.write(fbBuf, remainder);//將剩下的數據包發送給服務器
            }
        }
        client_tcp.print(tail);
        client_tcp.println();

        String getResponse = "", Feedback = "";//宣告字符串
        boolean state = false;//將state宣告類型為布林，並初始化為false
        int waitTime = 3000;   // 依據網路調整等候時間，3000代表，最多等3秒
        long startTime = millis();//獲取當前程序的運行時間
        delay(1000);//延遲1秒
        Serial.print("Get Response");

        while ((startTime + waitTime) > millis()) {//等待TCP客戶端響應
            Serial.print(".");
            delay(100);//延遲0.1秒
            bool jobdone = false;//將jobdone宣告類型為布林，並初始化為false
            while (client_tcp.available())
            {//當有收到回覆資料時
                jobdone = true;//可退出迴圈之標記為true
                char c = client_tcp.read();//取得客戶端發送的數據
                if (c == '\n')//判斷字符讀取到換行符時
                {
                    if (getResponse.length() == 0) state = true;//如果字符串長度為0則標記為true
                    getResponse = "";//將字符串清空
                }
                else if (c != '\r')//判斷讀取到的字符不為回車符
                    getResponse += String(c);//將客戶端傳送之資料丟給字符串
                if (state == true) Feedback += String(c);//判斷標記為true則將客戶端傳送之資料丟給字符串
                startTime = millis();//獲取當前程序的運行時間
            }
            if (jobdone) break;//有收到回覆資料即可退出迴圈
        }
        client_tcp.stop();//關閉與TCP服務器的連接
        esp_camera_fb_return(fb);//清除緩衝區
        return Feedback;//回傳客戶端傳送之資料
    }

    else {//否則連接失敗時
        esp_camera_fb_return(fb);//釋放相機緩存區內存
        return "Send failed.";//回傳傳送失敗之字符串
    }
}

sensor_t* x;
void startCameraServer();
//啟動一個相機伺服器，讓客戶端通過網路連接到相機，獲取即時影像或者訪問相機的設置
void setup() {
    Serial.begin(115200);//序列埠初始化
    Serial.setDebugOutput(true);
    /*啟用Serial物件的調試輸出模式，Serial物件可輸出調試信息到所有已註冊的串口，
    這些信息可以用於調試程序或查找錯誤*/
    Serial.println();
    camera_config_t config;//宣告相機的配置參數
    config.ledc_channel = LEDC_CHANNEL_0;//將LEDC通道設置為0
    config.ledc_timer = LEDC_TIMER_0;//將LEDC計時器設置為0
    config.pin_d0 = Y2_GPIO_NUM;//宣告腳位
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
    config.xclk_freq_hz = 20000000;//設置相機模塊的時鐘頻率為20MHz
    config.pixel_format = PIXFORMAT_JPEG;//設置相機輸出圖像格式為JPEG格式
    
    if (psramFound()) {//判斷開發版是否搭載PSRAM外部記憶體
        config.frame_size = FRAMESIZE_UXGA;//設置相機輸出圖像分辨率為UXGA
        config.jpeg_quality = 10;//設置輸出的圖像質量
        config.fb_count = 2;//分配相機圖像緩存數量
    }
    else {
        config.frame_size = FRAMESIZE_SVGA;
        config.jpeg_quality = 12;
        config.fb_count = 1;
    }
    esp_err_t err = esp_camera_init(&config);
    /*esp_err_t類型是一個枚舉類型的錯誤碼，用於指示操作的成功或失敗
    如果相機初始化成功，則該函數返回ESP_OK。反之則函數返回相應的錯誤碼*/
    if (err != ESP_OK) {//判斷初始化失敗
        Serial.printf("Camera init failed with error 0x%x", err);
        return;//退出此程式
    }
    sensor_t* s = esp_camera_sensor_get();//獲取相機模組的傳感器物件指針
    if (s->id.PID == OV3660_PID) {//判斷傳感器裝置ID是否為OV3660_PID
        s->set_vflip(s, 1);//垂直翻轉
        s->set_brightness(s, 1);//調整亮度
        s->set_saturation(s, -2);//調整飽和度
    }
    x=s;
    s->set_framesize(s, FRAMESIZE_QVGA);//設置傳感器的幀大小為FRAMESIZE_QVGA
    
    WiFi.mode(WIFI_STA);//WiFi作為客戶端連接到指定的WiFi網路
    long int StartTime = millis();
    WiFi.begin(ssid, password);//嘗試連接WiFi網路
    while (WiFi.status() != WL_CONNECTED) {//開始迴圈直到WiFi連接成功
        delay(500);//延遲0.5秒
        Serial.print(".");
        if ((StartTime + 5000) < millis()) break;//超過5秒也會退出迴圈
    }
    if (WiFi.status() != WL_CONNECTED) {//判斷WiFi連接失敗
      Serial.println("Reset");
      delay(1000);
      ESP.restart();//連線不成功，則重新開機
    }
    Serial.println("");
    Serial.println("WiFi connected");
    startCameraServer();
    Serial.print("Camera Ready! Use 'http://");
    Serial.print(WiFi.localIP());//即時影像IP位址
    Serial.println("' to connect");
    pinMode(14, INPUT);//14接4
    pinMode(15, OUTPUT);//15接8
    pinMode(13, INPUT);//13接光敏
    digitalWrite(15, HIGH);
}

void loop() {
    if (digitalRead(14) == true) {//高電位時，傳圖片
        Serial.println("starting to Line");
        String payload = sendImage2LineNotify("照片傳送中，即時影像網址:http://"+WiFi.localIP().toString());
        //呼叫傳遞訊息與圖片之副程式，並將回傳值丟至字串payload
        Serial.println(payload);
        
        delay(1500);//延遲1.5秒
    }
     int LightValue = analogRead(13); //讀取的GPIO 13讀取的數值放在LightValue
  if (LightValue < 500) {
    //小於500，亮度充足
    x->set_brightness(x, -1);
  }
  else if (LightValue < 1000) {
    //500-1000之間，亮度中等
    x->set_brightness(x, 0);
  }
  else if (LightValue < 2000) {
    //1000-2000之間，亮度不足
    x->set_brightness(x, 1);
  }
  else if (LightValue > 2000) {
    //2000以上，完全無燈光
    x->set_brightness(x, 2);
  }
    delay(100);
}
