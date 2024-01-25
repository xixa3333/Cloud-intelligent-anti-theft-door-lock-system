#include <LWiFi.h>//引入WiFi連線函式庫標題檔
#include "MCS.h"//引入MCS雲端連線函式庫標題檔
#include <SPI.h>//引入序列通訊函式庫標題檔
#include <MFRC522.h>//引入RFID函式庫標題檔
#include <Ultrasonic.h>//引入超音波函式庫標題檔

String lineToken = "70giismZ85IZ3FZ00VDrYNmZUwDCT5dEL3ups1Y6EhZ";//設置LINE權杖
String RfidNo, rfids[100]{};//宣告RFID磁扣名稱與磁扣陣列
int loss = 0, lock = 0, tweet = 0, AutoLock = 0, SensFlag = 0, SensDelay = 0, rfid_quan = 1;
//宣告磁扣連續感應失敗的累積次數與開鎖旗標與蜂鳴器計時器與自動鎖定計時器與測距旗標與測距延遲秒數計時與新增磁扣數量
float dist;//宣告超音波測量距離
Ultrasonic ultrasonic_6_7(6, 7);//宣告超音波接腳
char _lwifi_ssid[] = "xixa3333";//WiFi帳號
char _lwifi_pass[] = "asdfghjkl";//WiFi密碼
MCSLiteDevice mcs("S1qcQJ8Ri", "b02ec897df624871fc9d0b090b9674f38e60266e6647223dcd1183bfe6800998", "192.168.148.9", 3000);
//取得MCS雲端ID與Key與IP位址和埠做連線
MCSControllerOnOff switchs("switchs");//取得雲端開關MCS控制通道
MCSControllerOnOff rfidadd("rfidadd");//取得新增使用者MCS控制通道
MCSControllerOnOff RfidDel("RfidDel");//取得刪除使用者MCS控制通道
MFRC522 rfid(/*SS_PIN*/ 10, /*RST_PIN*/ UINT8_MAX);//宣告RFID腳位

String mfrc522_readID()//RFID讀取
{
    String ret;
    if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial())
        //判斷有感應到新的磁扣與有讀取磁扣資料
    {
        MFRC522::PICC_Type piccType = rfid.PICC_GetType(rfid.uid.sak);
        // 根據卡片回應的SAK值判斷磁扣類型
        for (byte i = 0; i < rfid.uid.size; i++) { // 逐一取得UID值
            ret += (rfid.uid.uidByte[i] < 0x10 ? "0" : "");
            ret += String(rfid.uid.uidByte[i], HEX); // 取得UID值
        }
    }
    rfid.PICC_HaltA();// 讓磁扣進入停止模式
    rfid.PCD_StopCrypto1();//可重複感應磁扣
    return ret;//回傳磁扣UID值
}

void sendLineMsg(String myMsg) {//LINE連接與傳遞訊息
    static TLSClient line_client;//創建LINE的區域變量
    myMsg.replace("%", "%25");//轉換字符
    myMsg.replace("&", "%26");
    myMsg.replace("§", "&");
    myMsg.replace("\\n", "\n");
    if (line_client.connect("notify-api.line.me", 443)) {//LINE是否連接成功
        line_client.println("POST /api/notify HTTP/1.1");//傳遞給LINE資料
        line_client.println("Connection: close");
        line_client.println("Host: notify-api.line.me");
        line_client.println("Authorization: Bearer " + lineToken);
        line_client.println("Content-Type: application/x-www-form-urlencoded");
        line_client.println("Content-Length: " + String(myMsg.length()));
        line_client.println();
        line_client.println(myMsg);
        line_client.println();
        line_client.stop();//停止傳遞
    }
    else Serial.println("Line Notify failed");
}

void LockSub() {//門鎖鎖定副程式
    lock = 0;//開鎖旗標為0
    digitalWrite(5, LOW);//門鎖自動鎖定
    Serial.println("已鎖定");
    sendLineMsg(String("message=\n") + "智慧門鎖已鎖定");
}

void UnLockSub() {//門鎖開鎖副程式
    tweet = 0;//蜂鳴器計時器停止計時
    loss = 0;//失敗次數重置為0
    digitalWrite(5, HIGH);// 給繼電器高電位使電子鎖開鎖
    digitalWrite(2, LOW);// 蜂鳴器停止鳴叫
    Serial.println("通過");
    sendLineMsg(String("message=\n") + "智慧門鎖已開鎖");
}

void Photograph() {//拍照副程式
    digitalWrite(4, HIGH);//傳遞高電位給ESP32-CAM拍照並傳遞照片
    delay(100);//延遲0.1秒
    digitalWrite(4, LOW);//傳遞低電位給ESP32-CAM停止拍照
}

void setup()//初始化各值
{
    Serial.begin(9600);//序列埠初始化
    mcs.addChannel(switchs);//新增雲端開關MCS控制通道
    mcs.addChannel(rfidadd);//新增新增使用者MCS控制通道
    mcs.addChannel(RfidDel);//取得刪除使用者MCS控制通道
    Serial.println("WIFI連線中");//傳遞訊息至序列埠並換行
    while (WiFi.begin(_lwifi_ssid, _lwifi_pass) != WL_CONNECTED) { delay(1000); }
    //判斷WiFi是否連線成功
    Serial.println("WIFI已連線");
    Serial.println("MCS連線中");
    while (!mcs.connected()) { mcs.connect(); }//判斷MCS是否連線成功
    Serial.println("MCS已連線");
    Serial.begin(9600);//序列埠初始化
    SPI.begin();//序列通訊初始化
    rfid.PCD_Init();//初始化RC522讀卡機模組
    pinMode(2, OUTPUT);//初始化腳位連接蜂鳴器
    pinMode(4, OUTPUT);//初始化腳位連接ESP32-CAM
    pinMode(5, OUTPUT);//初始化腳位連接繼電器
    pinMode(8, INPUT);//人臉辨識傳輸電位
    rfids[0] = "8cc13d63";//將原使用者的磁扣碼存至陣列
}

void loop()
{
    while (!mcs.connected()) {
        mcs.connect();//與MCS同步
        if (mcs.connected()) { Serial.println("MCS Reconnected."); }
        //判斷MCS是否連線成功
    }
    mcs.process(100);
    dist = ultrasonic_6_7.convert(ultrasonic_6_7.timing(), Ultrasonic::CM);
    //取得超音波測量距離
    Serial.println(dist);
    if (lock == 0 && AutoLock == 0) {//判斷為關鎖狀態
        SensDelay++;//測距延遲秒數計時
        if (dist < 60 ) {//判斷超音波測量距離小於60cm
          Serial.println(SensDelay);
          if(SensFlag == 1)SensDelay = 0;//當延遲中途時變換，秒數變0
            if (SensDelay >= 16&& SensFlag == 0) {
              //在門前超過5秒且測距旗標等於0
              Serial.println("60以內2");
                sendLineMsg(String("message=\n") + "有人靠近你家");
                SensFlag = 1;//測距旗標等於1
                SensDelay = 0;//測距延遲秒數重置為0
                Photograph();//呼叫拍照副程式
            }
        }
        else if (dist >= 60) {//超音波測量距離大於等於60cm
          if(SensFlag == 0)SensDelay = 0;//當延遲中途時變換，秒數變0
            if (SensDelay >= 2 && SensFlag == 1) {
            Serial.println("60以外");
              //測距延遲秒數計時超過0.5秒時且測距旗標等於1
                SensFlag = 0;//測距旗標等於0
                SensDelay = 0;//測距延遲秒數重置為0
            }
        }
    }

    if (tweet != 0) {
        tweet -= 1;//蜂鳴器計時器倒數
        if (tweet == 0)digitalWrite(2, LOW);//蜂鳴器停止鳴叫
    }

    if (AutoLock != 0) {
        AutoLock -= 1;//自動鎖定計時器倒數
        if (AutoLock == 0)LockSub();//呼叫鎖定副程式
    }

    RfidNo = mfrc522_readID();//取得RFID磁扣的UID值
    if (RfidNo != "") {//判斷RFID有感應到
      Serial.println(RfidNo);
        if (lock == 0 && AutoLock == 0 && rfidadd.value() == false && RfidDel.value() == false) {
            //判斷門未解鎖且自動鎖定計時器未在計時且為感應模式
            for (int i = 0; i < rfid_quan; i++) {
                Serial.println(i);
                if (RfidNo == rfids[i]) {//利用迴圈判斷磁扣陣列裡有感應之磁扣
                    AutoLock = 50;//自動鎖定計時器開始倒數5秒
                    UnLockSub();//呼叫開鎖副程式
                    break;
                }
                else if (i == rfid_quan - 1) {//判斷磁扣陣列裡無感應之磁扣
                    loss += 1;//失敗次數增加
                    Serial.println("");
                    Serial.println(String() + "read RFID is:" + RfidNo);
                    Serial.println("不通過");
                    sendLineMsg(String("message=\n") + "智慧門鎖感應失敗");
                    //傳遞訊息至LINE
                    if (loss >= 3) {//判斷磁扣感應失敗連續三次或以上
                        tweet = 33;//蜂鳴器計時器開始倒數10秒
                        digitalWrite(2, HIGH);// 蜂鳴器鳴叫
                        sendLineMsg(String("message=\n") + "智慧門鎖連續感應失敗已達" + loss + "次");
                        Photograph();//呼叫拍照副程式
                    }
                }
            }
        }

        else if (rfidadd.value() == false && RfidDel.value() == true) {
            //判斷為刪除使用者模式
            for (int i = 0; i < rfid_quan; i++)if (RfidNo == rfids[i]) {
                //利用迴圈判斷磁扣陣列裡有感應之磁扣
                rfids[i] = "";//將磁扣陣列裡之磁扣刪除
                sendLineMsg(String("message=\n") + "刪除磁扣成功");
            }
        }

        else if (rfidadd.value() == true && RfidDel.value() == false) {
            //判斷為新增使用者模式
            for (int i = 0; i < rfid_quan; i++) {
                if (RfidNo == rfids[i])break;
                //利用迴圈判斷磁扣陣列裡有感應之磁扣則退出
                else if (i == rfid_quan - 1) {
                    //利用迴圈判斷磁扣陣列裡無感應之磁扣
                    for (int j = 0; j < rfid_quan; j++) {
                        if (rfids[j] == "") {
                            //利用迴圈判斷磁扣陣列裡中間有空白之陣列
                            rfids[j] = RfidNo;//將感應之磁扣新增至磁扣陣列
                            break;
                        }
                        else if (j == rfid_quan - 1) {
                            //磁扣陣列裡中間無空白之陣列
                            rfids[rfid_quan] = RfidNo;
                            //將感應之磁扣新增至磁扣陣列
                            rfid_quan++;
                            j++;
                            i++;
                        }
                    }
                    sendLineMsg(String("message=\n") + "新增磁扣成功");
                }
            }
        }
    }

    else if (AutoLock == 0 && lock == 0 && digitalRead(8) == 0) {
        //判斷門未解鎖且自動鎖定計時器未在計時且人臉辨識成功
        AutoLock = 50;//自動鎖定計時器開始倒數15秒
        UnLockSub();//呼叫開鎖副程式
    }

    else if (switchs.value() == true && lock == 0) {//判斷雲端開關為開且門未解鎖
        if (AutoLock == 0)UnLockSub();//判斷自動鎖定計時器未在計時並呼叫開鎖副程式
        lock = 1;//開鎖旗標為1
    }

    else if (switchs.value() == false && lock == 1) {//判斷雲端開關為關且門已解鎖
        AutoLock = 0;//自動鎖定計時器停止計時
        LockSub();//呼叫鎖定副程式
    }
    delay(100);
}
