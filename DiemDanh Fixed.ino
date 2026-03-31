/*
 * =====================================================
 *  DỰ ÁN: ĐIỂM DANH HỌC SINH RFID + GOOGLE SHEET
 *  學生出席系統 RFID + Google 試算表
 * ─────────────────────────────────────────────────────
 *  Phần cứng / 硬體:
 *    ESP32
 *    PN532 NFC/RFID V3  → SPI (DIP: SEL0=OFF, SEL1=ON)
 *      SCK   → GPIO 18
 *      MISO  → GPIO 19
 *      MOSI  → GPIO 23
 *      SS    → GPIO 16
 *    Buzzer (Active)       → GPIO 5
 *    LED RGB (共陰極):
 *      R     → GPIO 25   (+ điện trở 220Ω)
 *      G     → GPIO 26   (+ điện trở 220Ω)
 *      B     → GPIO 27   (+ điện trở 220Ω)
 *      GND   → GND
 *    Nút ấn VÀO/RA         → GPIO 15 (chân còn lại → GND)
 *    Nút ấn ĐĂNG KÝ THẺ   → GPIO 14 (chân còn lại → GND)
 *    LED onboard ESP32     → GPIO 2
 * ─────────────────────────────────────────────────────
 *  Thư viện cần cài (Library Manager):
 *    - Adafruit PN532
 *    - Adafruit BusIO  (自動安裝)
 * ─────────────────────────────────────────────────────
 *  HƯỚNG DẪN SỬ DỤNG / 使用說明:
 *
 *  【Nút GPIO 14 — Nút ĐĂNG KÝ THẺ / 登記按鈕】
 *    Nhấn 1 lần → Vào chế độ ĐĂNG KÝ (LED ⚪ trắng)
 *      → Quẹt thẻ mới → RFID tự lên cột B của 學生名單
 *      → Lên Sheet điền thêm 姓名(cột C) và 學號(cột D)
 *    Nhấn thêm 1 lần → Thoát ĐĂNG KÝ → về ĐIỂM DANH
 *      → Tự động tải lại danh sách học sinh mới nhất
 *
 *  【Nút GPIO 15 — Nút VÀO/RA / 進入離開切換】
 *    Chỉ hoạt động ở chế độ ĐIỂM DANH
 *    Nhấn để chuyển: 進入(VÀO) ↔ 離開(RA)
 *
 * ─────────────────────────────────────────────────────
 *  Google試算表結構 / Cấu trúc Google Sheet:
 *  工作表「學生名單」:
 *    A欄:編號 | B欄:RFID卡號 | C欄:姓名 | D欄:學號
 *    ↑ Khi đăng ký thẻ mới: cột B tự điền, C+D thầy điền tay
 *  工作表「出席記錄」:
 *    A欄:日期 | B欄:時間 | C欄:RFID卡號 | D欄:姓名 | E欄:狀態
 * ─────────────────────────────────────────────────────
 *  LED RGB 狀態:
 *    🔵 Xanh dương nhấp nháy → 正在啟動
 *    🟢 Xanh lá sáng         → 出席模式：進入 等待刷卡
 *    🟡 Vàng sáng            → 出席模式：離開 等待刷卡
 *    ⚪ Trắng sáng            → 登記模式：等待刷新卡
 *    🔵 Xanh dương sáng      → 正在傳送資料
 *    🟢 Xanh lá nháy 2x      → 出席成功
 *    ⚪ Trắng nháy 3x         → 登記成功
 *    🔴 Đỏ nháy 3x           → 卡片未登記 / 傳送失敗
 *    🔴 Đỏ sáng liên tục     → WiFi錯誤
 *    🟣 Tím nhấp nháy        → PN532錯誤
 * =====================================================
 */

#include <SPI.h>
#include <Adafruit_PN532.h>
#include "WiFi.h"
#include <HTTPClient.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// ─────────────────────────────────────────
//  PN532 - SPI
// ─────────────────────────────────────────
#define PN532_SS  16
Adafruit_PN532 nfc(PN532_SS);
String uidString = "";

// ─────────────────────────────────────────
//  LCD 1602 I2C
//  SDA → GPIO 21 | SCL → GPIO 22
//  I2C address: 0x27 (phổ biến), nếu không hiển thị thử 0x3F
// ─────────────────────────────────────────
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ─────────────────────────────────────────
//  WIFI  👈 請更改WiFi名稱和密碼
// ─────────────────────────────────────────
const char* ssid     = "TOTOLINK_A3";
const char* password = "Anhcuong140296";

// ─────────────────────────────────────────
//  GOOGLE APPS SCRIPT URL  👈 請更改您的網址
// ─────────────────────────────────────────
String Web_App_URL = "https://script.google.com/macros/s/AKfycbw-s5bgiLkpeLdO7lMvN84yva_kmh_Hc-6IADf4UrQ6C0H4dO14iMfOwDvd71SDObAI/exec";

// ─────────────────────────────────────────
//  CHÂN GPIO / 腳位定義
// ─────────────────────────────────────────
#define On_Board_LED_PIN  2
#define RGB_R             25
#define RGB_G             26
#define RGB_B             27
#define buzzer            5
#define btnIO             15   // Nút VÀO/RA     → 進入/離開 切換
#define btnREG            14   // Nút ĐĂNG KÝ    → 登記模式 切換

// ─────────────────────────────────────────
//  DANH SÁCH HỌC SINH / 學生名單
// ─────────────────────────────────────────
#define MAX_STUDENTS 10
struct Student {
  String id;        // 編號    (cột A)
  char code[15];    // RFID卡號 (cột B)
  char name[40];    // 姓名    (cột C)
  char mssv[20];    // 學號    (cột D)
};
Student students[MAX_STUDENTS];
int studentCount = 0;
int attendCount  = 0;   // Đếm số lượt điểm danh trong phiên hiện tại

// ─────────────────────────────────────────
//  BIẾN TRẠNG THÁI / 狀態變數
// ─────────────────────────────────────────
int  runMode       = 2;    // 1=登記模式 | 2=出席模式
bool InOutState    = 0;    // 0=進入(VÀO) | 1=離開(RA)
bool btnIoPrev     = HIGH;
bool btnRegPrev    = HIGH;
unsigned long lastBtnIoTime  = 0;
unsigned long lastBtnRegTime = 0;
unsigned long lastScanTime   = 0;

// ─────────────────────────────────────────
//  PWM LED RGB — ESP32 Core v3.x
// ─────────────────────────────────────────
#define PWM_FREQ  5000
#define PWM_RES   8

// =====================================================
//  HÀM LED RGB
// =====================================================
void setRGB(int r, int g, int b) {
  ledcWrite(RGB_R, r);
  ledcWrite(RGB_G, g);
  ledcWrite(RGB_B, b);
}
void rgbOff()    { setRGB(0,   0,   0);   }
void rgbRed()    { setRGB(255, 0,   0);   }
void rgbGreen()  { setRGB(0,   255, 0);   }
void rgbBlue()   { setRGB(0,   0,   255); }
void rgbYellow() { setRGB(255, 200, 0);   }
void rgbPurple() { setRGB(180, 0,   255); }
void rgbWhite()  { setRGB(200, 200, 200); }

void rgbBlink(int r, int g, int b, int n, int d) {
  for (int i = 0; i < n; i++) {
    setRGB(r, g, b); delay(d);
    rgbOff();        delay(d);
  }
}

// LED theo chế độ hiện tại
void rgbStandby() {
  if      (runMode == 1)        rgbWhite();   // ⚪ 登記模式
  else if (InOutState == 0)     rgbGreen();   // 🟢 出席：進入
  else                          rgbYellow();  // 🟡 出席：離開
}

// Hiển thị LCD theo chế độ chờ
void lcdStandby() {
  lcd.clear();
  if (runMode == 1) {
    lcd.setCursor(0, 0); lcd.print("** REGISTER  ** ");
    lcd.setCursor(0, 1); lcd.print("Scan new card...");
  } else {
    lcd.setCursor(0, 0); lcd.print(InOutState == 0 ? ">> CHECK IN  << " : ">> CHECK OUT << ");
    lcd.setCursor(0, 1); lcd.print("Scan to attend  ");
  }
}

// =====================================================
//  BUZZER
// =====================================================
void beep(int n, int d) {
  for (int i = 0; i < n; i++) {
    digitalWrite(buzzer, HIGH); delay(d);
    digitalWrite(buzzer, LOW);  delay(d);
  }
}

// =====================================================
//  SETUP
// =====================================================
void setup() {
  Serial.begin(115200);
  delay(300);

  pinMode(buzzer,           OUTPUT); digitalWrite(buzzer,           LOW);
  pinMode(On_Board_LED_PIN, OUTPUT); digitalWrite(On_Board_LED_PIN, LOW);
  pinMode(btnIO,  INPUT_PULLUP);
  pinMode(btnREG, INPUT_PULLUP);

  ledcAttach(RGB_R, PWM_FREQ, PWM_RES);
  ledcAttach(RGB_G, PWM_FREQ, PWM_RES);
  ledcAttach(RGB_B, PWM_FREQ, PWM_RES);

  // ── Khởi tạo LCD 1602 I2C ──
  Wire.begin(21, 22);       // SDA=21, SCL=22
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("RFID System");
  lcd.setCursor(0, 1); lcd.print("Initializing... ");

  Serial.println("\n=== 學生出席系統 RFID ===");
  rgbBlink(0, 0, 255, 4, 150);

  // WiFi
  Serial.print("正在連接WiFi: "); Serial.println(ssid);
  rgbBlue();
  WiFi.begin(ssid, password);
  int wt = 0;
  while (WiFi.status() != WL_CONNECTED && wt < 20) { delay(500); Serial.print("."); wt++; }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ WiFi連線成功 — IP: " + WiFi.localIP().toString());
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print("WiFi: OK");
    lcd.setCursor(0, 1); lcd.print(WiFi.localIP().toString());
    rgbBlink(0, 255, 0, 2, 150); beep(2, 100);
    // ── Đồng bộ thời gian NTP (UTC+7 = Việt Nam / Đài Loan +8) ──
    configTime(7 * 3600, 0, "pool.ntp.org", "time.nist.gov");
    Serial.print("Dang dong bo NTP...");
    struct tm ti;
    int ntpWait = 0;
    while (!getLocalTime(&ti) && ntpWait < 20) { delay(500); Serial.print("."); ntpWait++; }
    if (getLocalTime(&ti)) {
      char buf[20];
      strftime(buf, sizeof(buf), "%H:%M:%S", &ti);
      Serial.println(" OK → " + String(buf));
    } else {
      Serial.println(" NTP that bai, dung millis()");
    }
  } else {
    Serial.println("\n❌ WiFi連線失敗！");
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print("WiFi: FAILED!");
    lcd.setCursor(0, 1); lcd.print("Check settings..");
    rgbRed(); beep(5, 300);
  }

  // PN532
  Serial.println("正在初始化PN532...");
  nfc.begin(); delay(500);
  uint32_t ver = nfc.getFirmwareVersion();
  if (!ver) {
    Serial.println("❌ 找不到PN532！請確認DIP: SEL0=OFF, SEL1=ON");
    while (1) { rgbBlink(180, 0, 255, 1, 300); beep(1, 200); }
  }
  Serial.print("✅ PN532就緒 — 韌體: ");
  Serial.print((ver >> 16) & 0xFF); Serial.print("."); Serial.println((ver >> 8) & 0xFF);
  nfc.setPassiveActivationRetries(0x03);
  nfc.SAMConfig();

  // Đọc danh sách
  Serial.println("正在讀取學生名單...");
  rgbBlue();
  if (readDataSheet()) {
    Serial.println("✅ 讀取成功 — 共 " + String(studentCount) + " 位學生");
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print("Student list: OK");
    lcd.setCursor(0, 1); lcd.print(String(studentCount) + " students");
    delay(1000);
    rgbBlink(0, 255, 0, 3, 80); beep(3, 80);
  } else {
    Serial.println("⚠️  無法讀取名單！");
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print("List load error!");
    lcd.setCursor(0, 1); lcd.print("Please retry... ");
    rgbBlink(255, 200, 0, 3, 300); beep(2, 500);
  }

  rgbStandby();
  lcdStandby();
  printStatus();
}

// =====================================================
//  LOOP
// =====================================================
void loop() {

  // ── Nút GPIO14: chuyển ĐĂNG KÝ ↔ ĐIỂM DANH ──
  bool btnRegNow = digitalRead(btnREG);
  if (btnRegNow == LOW && btnRegPrev == HIGH) {
    if (millis() - lastBtnRegTime > 300) {
      if (runMode == 2) {
        // ► Vào chế độ ĐĂNG KÝ
        runMode = 1;
        Serial.println("\n>>> 進入【登記模式】⚪");
        Serial.println("    請刷新卡，RFID卡號將自動寫入試算表B欄");
        beep(2, 80);
        rgbBlink(200, 200, 200, 3, 100);
        rgbWhite();
        lcdStandby();
      } else {
        // ► Thoát ĐĂNG KÝ, tải lại danh sách
        runMode = 2;
        Serial.println("\n>>> 退出登記，切換到【出席模式】");
        Serial.println("    正在重新讀取學生名單...");
        beep(1, 300);
        rgbBlue();
        if (readDataSheet()) {
          Serial.println("✅ 名單更新成功 — " + String(studentCount) + " 位學生");
          rgbBlink(0, 255, 0, 2, 100); beep(2, 80);
        } else {
          Serial.println("⚠️  名單更新失敗！");
          rgbBlink(255, 200, 0, 2, 200);
        }
        rgbStandby();
        lcdStandby();
      }
      printStatus();
      lastBtnRegTime = millis();
    }
  }
  btnRegPrev = btnRegNow;

  // ── Nút GPIO15: chuyển VÀO/RA (chỉ ở chế độ điểm danh) ──
  if (runMode == 2) {
    bool btnIoNow = digitalRead(btnIO);
    if (btnIoNow == LOW && btnIoPrev == HIGH) {
      if (millis() - lastBtnIoTime > 300) {
        InOutState = !InOutState;
        Serial.println(">>> 切換: " + String(InOutState ? "離開 🟡" : "進入 🟢"));
        beep(1, 150);
        rgbStandby();
        lcdStandby();
        lastBtnIoTime = millis();
      }
    }
    btnIoPrev = btnIoNow;
  }

  // ── Quét thẻ mỗi 600ms ──
  if (millis() - lastScanTime > 600) {
    readUID();
    lastScanTime = millis();
  }
}

// =====================================================
//  ĐỌC UID THẺ
// =====================================================
void readUID() {
  uint8_t uid[7] = {0};
  uint8_t uidLength = 0;
  bool found = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength, 300);
  delay(1);
  if (!found) return;

  uidString = "";
  for (uint8_t i = 0; i < uidLength; i++) {
    if (uid[i] < 0x10) uidString += "0";
    uidString += String(uid[i], HEX);
  }
  uidString.toUpperCase();
  Serial.println("\n📡 RFID卡號: " + uidString);
  beep(1, 80);

  if      (runMode == 1) registerNewCard();
  else if (runMode == 2) writeLogSheet();
}

// =====================================================
//  ĐĂNG KÝ THẺ MỚI → ghi hàng mới vào 學生名單
//  Script nhận: ?sts=registercard&uid=XXXX
//  Sheet tự điền: cột A(編號 tự tăng), cột B(RFID)
//  Thầy chỉ cần điền thêm: cột C(姓名) và cột D(學號)
// =====================================================
void registerNewCard() {
  Serial.println("📝 登記新卡 → 上傳RFID到試算表...");
  rgbBlue();
  digitalWrite(On_Board_LED_PIN, HIGH);

  String url = Web_App_URL + "?sts=registercard&uid=" + uidString;

  HTTPClient http;
  http.begin(url.c_str());
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setTimeout(10000);

  int httpCode = http.GET();
  if (httpCode == 200) {
    String res = http.getString();
    if (res == "DUPLICATE") {
      Serial.println("⚠️  此卡已登記過: " + uidString);
      rgbBlink(255, 200, 0, 3, 200); // 黃色閃爍 = 重複卡
      beep(2, 300);
    } else {
      Serial.println("✅ 登記成功！請到試算表補填 姓名 和 學號");
      rgbBlink(200, 200, 200, 3, 150); // ⚪ 白色閃爍 = 登記成功
      beep(3, 100);
    }
  } else {
    Serial.println("❌ 上傳失敗 HTTP: " + String(httpCode));
    rgbBlink(255, 0, 0, 3, 300);
    beep(3, 400);
  }
  http.end();
  digitalWrite(On_Board_LED_PIN, LOW);
  rgbWhite();
  lcdStandby(); // Giữ hiển thị chế độ đăng ký
}

// =====================================================
//  GHI LOG ĐIỂM DANH
// =====================================================
void writeLogSheet() {
  char uidChar[uidString.length() + 1];
  uidString.toCharArray(uidChar, sizeof(uidChar));
  char* studentName = getStudentNameByUID(uidChar);
  char* studentMSSV = getStudentMSSVByUID(uidChar);

  if (studentName == nullptr) {
    Serial.println("❌ 找不到此卡號: " + uidString);
    Serial.println("   → 請按GPIO14進入登記模式登記此卡");
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print("Card not found! ");
    lcd.setCursor(0, 1); lcd.print(uidString.substring(0, 16));
    rgbBlink(255, 0, 0, 3, 400);
    beep(3, 400);
    delay(1500);
    rgbStandby();
    lcdStandby();
    return;
  }

  Serial.println("👤 " + String(studentName) + " — " + String(InOutState ? "離開" : "進入"));

  // ── Hiển thị LCD ngay khi quẹt thẻ (trong khi gửi HTTP) ──
  lcd.clear();
  // Hàng 1: Mã số học sinh (學號), ví dụ: s121170906
  String lcdMSSV = (studentMSSV != nullptr) ? String(studentMSSV) : uidString.substring(0, 16);
  if (lcdMSSV.length() > 16) lcdMSSV = lcdMSSV.substring(0, 16);
  lcd.setCursor(0, 0); lcd.print(lcdMSSV);
  // Hàng 2: Trạng thái đang gửi
  lcd.setCursor(0, 1); lcd.print("Sending data... ");

  rgbBlue();
  digitalWrite(On_Board_LED_PIN, HIGH);

  String inoutText = (InOutState == 0) ? "進入" : "離開";
  String url = Web_App_URL + "?sts=writelog";
  url += "&uid="   + uidString;
  url += "&name="  + urlencode(String(studentName));
  url += "&inout=" + urlencode(inoutText);

  HTTPClient http;
  http.begin(url.c_str());
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setTimeout(10000);

  int httpCode = http.GET();
  if (httpCode == 200) {
    Serial.println("✅ 出席記錄成功！");
    attendCount++;

    // Lấy giờ hiện tại từ NTP
    char timeBuf[17] = "                ";
    struct tm ti;
    if (getLocalTime(&ti)) {
      strftime(timeBuf, sizeof(timeBuf), "%H:%M:%S %d/%m", &ti);
    } else {
      unsigned long s = millis() / 1000;
      snprintf(timeBuf, sizeof(timeBuf), "T+%lu sec", s);
    }

    // Dòng 1: Mã số học sinh (學號) — ví dụ: s121170906
    lcd.clear();
    lcd.setCursor(0, 0);
    String dispMSSV = (studentMSSV != nullptr) ? String(studentMSSV) : uidString.substring(0, 16);
    if (dispMSSV.length() > 16) dispMSSV = dispMSSV.substring(0, 16);
    lcd.print(dispMSSV);

    // Dòng 2: Thời gian điểm danh quẹt thẻ
    lcd.setCursor(0, 1);
    lcd.print(timeBuf);

    rgbBlink(0, 255, 0, 2, 150); beep(2, 100);
    delay(2000); // Giữ 2 giây để học sinh đọc
  } else {
    Serial.println("❌ HTTP錯誤: " + String(httpCode));
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print("Send data error!");
    lcd.setCursor(0, 1); lcd.print("HTTP: " + String(httpCode));
    rgbBlink(255, 0, 0, 3, 300); beep(4, 300);
    delay(1500);
  }
  http.end();
  digitalWrite(On_Board_LED_PIN, LOW);
  rgbStandby();
  lcdStandby(); // Về màn hình chờ
}

// =====================================================
//  ĐỌC DANH SÁCH HỌC SINH
// =====================================================
bool readDataSheet() {
  if (WiFi.status() != WL_CONNECTED) return false;
  digitalWrite(On_Board_LED_PIN, HIGH);

  HTTPClient http;
  http.begin((Web_App_URL + "?sts=read").c_str());
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setTimeout(10000);

  int httpCode = http.GET();
  studentCount = 0;

  if (httpCode == 200) {
    String payload = http.getString();
    Serial.println("回傳資料: " + payload);
    char buf[payload.length() + 1];
    payload.toCharArray(buf, sizeof(buf));
    int total = countElements(buf, ',');
    char* token = strtok(buf, ",");
    while (token != NULL && studentCount < total / 4 && studentCount < MAX_STUDENTS) {
      students[studentCount].id = String(token);
      token = strtok(NULL, ","); if (!token) break;
      strncpy(students[studentCount].code, token, sizeof(students[studentCount].code) - 1);
      token = strtok(NULL, ","); if (!token) break;
      strncpy(students[studentCount].name, token, sizeof(students[studentCount].name) - 1);
      token = strtok(NULL, ","); if (!token) break;
      strncpy(students[studentCount].mssv, token, sizeof(students[studentCount].mssv) - 1);
      studentCount++;
      token = strtok(NULL, ",");
    }
    for (int i = 0; i < studentCount; i++) {
      Serial.println("  [" + students[i].id + "] " + students[i].code +
                     " | " + students[i].name + " | " + students[i].mssv);
    }
  }
  http.end();
  digitalWrite(On_Board_LED_PIN, LOW);
  return studentCount > 0;
}

// =====================================================
//  In trạng thái ra Serial
// =====================================================
void printStatus() {
  Serial.println("────────────────────────────────");
  if (runMode == 1) {
    Serial.println("【登記模式 ⚪】請刷新卡");
    Serial.println("再按GPIO14退出登記模式");
  } else {
    Serial.println("【出席模式】" + String(InOutState ? "🟡 離開" : "🟢 進入"));
    Serial.println("GPIO14=登記模式 | GPIO15=切換進入/離開");
  }
  Serial.println("────────────────────────────────");
}

// =====================================================
//  HÀM TIỆN ÍCH
// =====================================================
char* getStudentNameByUID(char* uid) {
  for (int i = 0; i < studentCount; i++) {
    if (strcmp(students[i].code, uid) == 0) return students[i].name;
  }
  return nullptr;
}

char* getStudentMSSVByUID(char* uid) {
  for (int i = 0; i < studentCount; i++) {
    if (strcmp(students[i].code, uid) == 0) return students[i].mssv;
  }
  return nullptr;
}

int countElements(const char* data, char delimiter) {
  if (strlen(data) == 0) return 0;
  char buf[strlen(data) + 1];
  strcpy(buf, data);
  int count = 0;
  char* token = strtok(buf, &delimiter);
  while (token != NULL) { count++; token = strtok(NULL, &delimiter); }
  return count;
}

String urlencode(String str) {
  String encoded = "";
  for (int i = 0; i < str.length(); i++) {
    char c = str.charAt(i);
    if (c == ' ') encoded += '+';
    else if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') encoded += c;
    else {
      char hi = (c >> 4) & 0xF;
      char lo = c & 0xF;
      encoded += '%';
      encoded += (hi > 9) ? (char)(hi - 10 + 'A') : (char)(hi + '0');
      encoded += (lo > 9) ? (char)(lo - 10 + 'A') : (char)(lo + '0');
    }
  }
  return encoded;
}
