# rfid-attendance-system
RFID學生出席系統 — ESP32 × PN532 × LCD × Google Sheets
# 📡 RFID 學生出席系統
### ESP32 × PN532 × LCD 1602 × Google Sheets × Gmail 自動通知

<p align="center">
  <img src="https://img.shields.io/badge/Platform-ESP32-blue?logo=espressif&logoColor=white" />
  <img src="https://img.shields.io/badge/IDE-Arduino-teal?logo=arduino&logoColor=white" />
  <img src="https://img.shields.io/badge/Cloud-Google%20Sheets-green?logo=google-sheets&logoColor=white" />
  <img src="https://img.shields.io/badge/NFC-PN532-orange" />
  <img src="https://img.shields.io/badge/License-MIT-lightgrey" />
</p>

> 利用 RFID 感應卡自動完成學生點名，記錄即時上傳至 Google 試算表，並透過 Gmail 自動發送出席通知給學生。LCD 螢幕即時顯示學號與打卡時間，完全取代傳統人工點名。

---

## 📋 目錄

- [功能特色](#-功能特色)
- [系統架構圖](#-系統架構圖)
- [所需材料](#-所需材料)
- [腳位連接](#-腳位連接)
- [安裝與設定](#-安裝與設定)
  - [1. Google 試算表設定](#1-google-試算表設定)
  - [2. Google Apps Script 部署](#2-google-apps-script-部署)
  - [3. Arduino IDE 設定](#3-arduino-ide-設定)
  - [4. 修改程式碼參數](#4-修改程式碼參數)
  - [5. 燒錄程式](#5-燒錄程式)
- [使用說明](#-使用說明)
- [LCD 顯示畫面](#-lcd-顯示畫面)
- [LED RGB 狀態對照](#-led-rgb-狀態對照)
- [檔案結構](#-檔案結構)
- [常見問題](#-常見問題)
- [授權](#-授權)

---

## ✨ 功能特色

| 功能 | 說明 |
|------|------|
| 🔖 RFID 刷卡點名 | 感應時間 < 1 秒，支援 Mifare 1K 卡片 |
| ☁️ 雲端同步 | 出席記錄自動上傳 Google 試算表 |
| 📺 LCD 即時顯示 | 顯示學號與打卡時間（HH:MM:SS DD/MM）|
| 📧 Gmail 自動通知 | 每次刷卡後自動寄送出席確認信 |
| 🔒 個人查詢頁面 | 學生透過專屬連結只能查看自己的記錄 |
| 💡 LED RGB 狀態燈 | 不同顏色對應不同系統狀態 |
| 🔔 蜂鳴器回饋 | 成功/失敗用不同聲音提示 |
| 📝 刷卡登記模式 | 一鍵切換，新卡自動寫入試算表 |
| ⏰ NTP 時間同步 | WiFi 連線後自動同步真實時間 |
| 🔄 進入 / 離開切換 | 按鈕切換，分別記錄進出時間 |

---

## 🏗️ 系統架構圖

```
┌─────────────────────────────────────────────────────┐
│                     ESP32                           │
│                                                     │
│  PN532 ──SPI──►  讀取 RFID UID                     │
│  LCD 1602 ◄──I2C── 顯示學號 + 時間                 │
│  RGB LED ◄──PWM── 狀態指示                         │
│  Buzzer ◄──GPIO── 聲音回饋                         │
│  Button x2 ──GPIO── 模式切換                       │
│                                                     │
│  WiFi ──HTTPS──► Google Apps Script                │
└─────────────────────────────────────────────────────┘
                          │
                          ▼
         ┌────────────────────────────┐
         │      Google 試算表          │
         │  ┌──────────┐ ┌─────────┐ │
         │  │ 學生名單  │ │出席記錄 │ │
         │  └──────────┘ └─────────┘ │
         └────────────────────────────┘
                          │
                          ▼
                   📧 Gmail 通知
                   🔒 個人查詢頁
```

---

## 🛒 所需材料

| 編號 | 元件 | 數量 | 備註 |
|------|------|------|------|
| 1 | ESP32 開發板 | 1 | 任意 ESP32 型號皆可 |
| 2 | PN532 NFC/RFID 模組 V3 | 1 | DIP 撥碼：SEL0=OFF, SEL1=ON（SPI 模式）|
| 3 | LCD 1602 I2C | 1 | I2C 位址 0x27（若不亮試 0x3F）|
| 4 | Mifare 1K 感應卡 / 鑰匙扣 | 若干 | 每位學生一張 |
| 5 | RGB LED 共陰極 | 1 | — |
| 6 | 電阻 220Ω | 3 | R / G / B 各一顆 |
| 7 | 主動式蜂鳴器 | 1 | — |
| 8 | 按鈕（輕觸開關）| 2 | 進入/離開 切換 & 登記模式 |
| 9 | 麵包板 + 杜邦線 | 若干 | — |
| 10 | Micro USB 線 | 1 | 燒錄與供電 |

---

## 🔌 腳位連接

### PN532 → ESP32（SPI 模式）
> ⚠️ 使用前請確認 PN532 DIP 撥碼：**SEL0 = OFF，SEL1 = ON**

| PN532 | ESP32 |
|-------|-------|
| SCK   | GPIO 18 |
| MISO  | GPIO 19 |
| MOSI  | GPIO 23 |
| SS/CS | GPIO 16 |
| VCC   | 3.3V |
| GND   | GND |

### LCD 1602 I2C → ESP32
| LCD | ESP32 |
|-----|-------|
| SDA | GPIO 21 |
| SCL | GPIO 22 |
| VCC | 5V |
| GND | GND |

### RGB LED（共陰極）→ ESP32
| LED | 電阻 | ESP32 |
|-----|------|-------|
| R   | 220Ω | GPIO 25 |
| G   | 220Ω | GPIO 26 |
| B   | 220Ω | GPIO 27 |
| GND | —    | GND |

### 其他元件
| 元件 | ESP32 |
|------|-------|
| 蜂鳴器 ＋ | GPIO 5 |
| 蜂鳴器 － | GND |
| 按鈕 VÀO/RA（一端） | GPIO 15 |
| 按鈕 ĐĂNG KÝ（一端）| GPIO 14 |
| 兩顆按鈕另一端 | GND（使用內部上拉）|

---

## ⚙️ 安裝與設定

### 1. Google 試算表設定

1. 前往 [Google Sheets](https://sheets.google.com) 建立新試算表
2. 建立兩個工作表（分頁），名稱**完全相同**：

**工作表一：`學生名單`**
| A | B | C | D | E |
|---|---|---|---|---|
| 編號 | RFID卡號 | 姓名 | 學號 | 電子郵件 |

**工作表二：`出席記錄`**
| A | B | C | D | E |
|---|---|---|---|---|
| 日期 | 時間 | RFID卡號 | 姓名 | 狀態 |

3. 複製試算表網址中的 **ID**（`/d/` 和 `/edit` 之間的字串）

---

### 2. Google Apps Script 部署

1. 在試算表中點選 **「擴充功能」→「Apps Script」**
2. 刪除預設內容，將 `GoogleAppsScript_TW_FINAL.gs` 的程式碼全部貼上
3. 修改第 51 行的 `SHEET_ID`：
   ```javascript
   var SHEET_ID = '貼上您的試算表ID';
   ```
4. 修改第 55 行的課程名稱（選填）：
   ```javascript
   var CLASS_NAME = '您的課程名稱';
   ```
5. 點選 **「部署」→「新增部署作業」**
   - 類型：**網頁應用程式**
   - 執行身分：**我**
   - 存取權：**任何人**
6. 授權並複製 **Web App URL**（等一下會用到）
7. 將 Web App URL 貼回第 58 行的 `WEB_APP_URL`，再重新部署一次

---

### 3. Arduino IDE 設定

**安裝 ESP32 開發板支援：**
1. 開啟 Arduino IDE → **「檔案」→「偏好設定」**
2. 在「額外的開發板管理員網址」貼上：
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
3. **「工具」→「開發板管理員」** → 搜尋 `esp32` → 安裝 **ESP32 by Espressif Systems**

**安裝函式庫（Library Manager）：**

按下 `Ctrl + Shift + I` 開啟函式庫管理員，搜尋並安裝：

| 函式庫名稱 | 作者 |
|-----------|------|
| `Adafruit PN532` | Adafruit |
| `Adafruit BusIO` | Adafruit（自動安裝）|
| `LiquidCrystal I2C` | Frank de Brabander |

---

### 4. 修改程式碼參數

開啟 `DiemDanh_Fixed.ino`，找到並修改以下三個地方：

```cpp
// ① WiFi 名稱與密碼
const char* ssid     = "您的WiFi名稱";
const char* password = "您的WiFi密碼";

// ② Google Apps Script URL（從步驟2複製）
String Web_App_URL = "https://script.google.com/macros/s/XXXXXX/exec";

// ③ 時區（越南 UTC+7，台灣請改為 8）
configTime(7 * 3600, 0, "pool.ntp.org", "time.nist.gov");
//         ↑ 台灣用戶請改成 8
```

> **台灣用戶**：請將 `7 * 3600` 改為 `8 * 3600`

---

### 5. 燒錄程式

1. 將 ESP32 透過 USB 連接電腦
2. **「工具」→「開發板」** → 選擇 `ESP32 Dev Module`
3. **「工具」→「序列埠」** → 選擇 ESP32 的 COM 埠
4. 點選 **「上傳」**（→ 箭頭按鈕）
5. 燒錄完成後開啟 **序列埠監控器**（`Ctrl + Shift + M`），設定 `115200 baud` 確認運作正常

---

## 📖 使用說明

### 登記新卡流程
```
① 按下 GPIO14 按鈕一次 → 進入登記模式（LED 白色）
② 將新卡靠近 PN532 → RFID 自動寫入試算表 B 欄
③ 到 Google 試算表手動填入：C欄（姓名）、D欄（學號）、E欄（Gmail）
④ 再按 GPIO14 一次 → 退出登記，自動重新載入名單
```

### 學生點名流程
```
① 系統預設為【進入模式】（LED 綠色）
② 學生將卡片靠近 PN532 → 嗶一聲
③ LCD 顯示學號 + 時間，LED 閃爍綠色 2 次
④ 系統自動上傳記錄至 Google 試算表
⑤ 學生收到 Gmail 出席通知（含個人查詢連結）
```

### 切換進入 / 離開
```
按下 GPIO15 → LED 由綠色變黃色（離開模式）
再按一次 → 回到綠色（進入模式）
```

### 學生查詢個人記錄
在 Gmail 通知信件中點選連結，或直接開啟：
```
https://script.google.com/macros/s/[YOUR_ID]/exec?sts=mystats&studentid=學號
```

---

## 📺 LCD 顯示畫面

```
開機啟動             WiFi 連線成功
┌────────────────┐   ┌────────────────┐
│ RFID System    │   │ WiFi: OK       │
│ Initializing...│   │ 192.168.1.xxx  │
└────────────────┘   └────────────────┘

等待打卡（進入）      等待打卡（離開）
┌────────────────┐   ┌────────────────┐
│ >> CHECK IN << │   │ >> CHECK OUT <<│
│ Scan to attend │   │ Scan to attend │
└────────────────┘   └────────────────┘

打卡成功 ✅           卡片未登記 ❌
┌────────────────┐   ┌────────────────┐
│ s121170906     │   │ Card not found!│
│ 08:35:12 18/03 │   │ A3F2B1C4       │
└────────────────┘   └────────────────┘
```

---

## 💡 LED RGB 狀態對照

| LED 顏色 | 狀態說明 |
|----------|---------|
| 🔵 藍色閃爍 | 系統正在啟動 |
| 🟢 綠色常亮 | 出席模式：等待進入刷卡 |
| 🟡 黃色常亮 | 出席模式：等待離開刷卡 |
| ⚪ 白色常亮 | 登記模式：等待新卡 |
| 🔵 藍色常亮 | 正在上傳資料至雲端 |
| 🟢 綠色閃爍 2次 | 出席記錄成功 |
| ⚪ 白色閃爍 3次 | 新卡登記成功 |
| 🔴 紅色閃爍 3次 | 卡片未登記 / 上傳失敗 |
| 🔴 紅色常亮 | WiFi 連線失敗 |
| 🟣 紫色閃爍 | PN532 模組錯誤 |

---

## 📁 檔案結構

```
rfid-attendance-system/
│
├── DiemDanh_Fixed.ino          # ESP32 主程式（Arduino）
├── GoogleAppsScript_TW_FINAL.gs # Google Apps Script 後端
├── README.md                   # 本說明文件
└── docs/
    └── RFID_出席系統_報告.pptx  # 專案報告簡報
```

---

## ❓ 常見問題

<details>
<summary><b>Q1: LCD 沒有顯示任何文字？</b></summary>

1. 確認 I2C 位址：大多數 LCD 模組為 `0x27`，部分為 `0x3F`
2. 修改程式碼第 81 行：`LiquidCrystal_I2C lcd(0x3F, 16, 2);`
3. 確認 SDA → GPIO21、SCL → GPIO22 連接正確
4. 調整 LCD 背面的藍色電位器直到文字清晰可見

</details>

<details>
<summary><b>Q2: PN532 無法偵測到卡片？</b></summary>

1. 確認 DIP 撥碼設定：**SEL0 = OFF，SEL1 = ON**（SPI 模式）
2. 確認接線：SCK=18, MISO=19, MOSI=23, SS=16
3. 開啟序列埠監控器，確認是否出現 `PN532就緒` 訊息
4. 嘗試將卡片放在模組正上方約 1~3 cm 處

</details>

<details>
<summary><b>Q3: WiFi 連線失敗？</b></summary>

1. 確認 SSID 和密碼是否正確（注意大小寫）
2. ESP32 僅支援 **2.4 GHz** 頻段，不支援 5 GHz
3. 確認路由器未開啟 MAC 位址過濾

</details>

<details>
<summary><b>Q4: 上傳成功但試算表沒有記錄？</b></summary>

1. 確認 Web App URL 正確貼入程式碼
2. 確認部署時「存取權」設定為「**任何人**」
3. 確認工作表名稱完全一致：`學生名單` 和 `出席記錄`
4. 在 Apps Script Editor 執行 `testReadSheet()` 函數測試

</details>

<details>
<summary><b>Q5: 學生沒有收到 Gmail 通知？</b></summary>

1. 確認試算表 E 欄已填入正確的 Gmail 地址
2. 在 Apps Script Editor 執行 `testGmail()` 函數確認 Gmail 授權正常
3. 檢查垃圾郵件資料夾
4. 確認 Apps Script 已取得 Gmail 存取權限

</details>

<details>
<summary><b>Q6: 編譯錯誤 LiquidCrystal_I2C.h: No such file？</b></summary>

開啟 Arduino IDE → `Ctrl + Shift + I` → 搜尋 `LiquidCrystal I2C` → 安裝 **Frank de Brabander** 的版本

</details>

---

## 📊 系統規格

| 項目 | 規格 |
|------|------|
| 最大學生人數 | 30 人（可修改 `MAX_STUDENTS`）|
| RFID 頻率 | 13.56 MHz（Mifare ISO14443A）|
| 通訊方式 | SPI（PN532）/ I2C（LCD）/ WiFi HTTPS |
| 時區設定 | UTC+7（越南）/ 改 8 適用台灣 |
| 回應時間 | 刷卡到記錄完成約 2~4 秒 |
| 供電 | 5V via USB 或外部電源 |

---

## 📜 授權

本專案採用 [MIT License](LICENSE) 授權。  
歡迎 Fork、修改與分享，請保留原始作者資訊。

---

## 🙏 致謝

- [Adafruit PN532 Library](https://github.com/adafruit/Adafruit-PN532)
- [LiquidCrystal I2C by Frank de Brabander](https://github.com/johnrickman/LiquidCrystal_I2C)
- [Arduino ESP32 Core](https://github.com/espressif/arduino-esp32)
- 指導教師：王旭志 教授

---

<p align="center">
  Made with ❤️ for 介面技術實務 — 電資四實
</p>
