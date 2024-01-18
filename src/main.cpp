#include <Arduino.h>
#include <vector>

#include <ArduinoOTA.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiManager.h>

#include <ESP32Time.h>
#include <HX711.h>

#include <U8g2lib.h>

#include <SPIFFS.h>

#include "bitmaps.h"
#include "defines.h"
#include "menu.hpp"
#include "pins.h"
#include "rtcHelper.h"
#include "spiffsHelper.hpp"

using std::vector;

struct Payload
{
  tm time;
  int OverGroundTank;
  int UnderGroundTank;
};

struct Levels
{
  long OverGroundTank;
  long UnderGroundTank;
};

struct Tank
{
  long Tare0;
  long Tare100;

  HX711 scale;
};

WiFiManager wm;
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0);
ESP32Time rtc;
HX711 loadCell;

Tank OverGroundTank;
Tank UnderGroundTank;

vector<Payload> PayloadStack = {};
bool usingPortal = false;

TaskHandle_t WiFiManageHandle = NULL;
TaskHandle_t DisplayManageHandle = NULL;
TaskHandle_t UploadHandle = NULL;
TaskHandle_t ReadDataHandle = NULL;

Levels getLevels()
{
  Levels levels;

  int changeT = OverGroundTank.Tare100 - OverGroundTank.Tare0;

  return {
      // ((A.scale.read_average(5) - (double)A.Tare0) / changeT ) * 100,
      ((OverGroundTank.scale.read_average(7) - (double)OverGroundTank.Tare0) / changeT) * 100,
      0};
}

void fastPrint(int align, int y, String text, bool clear = true,
               bool sendBuffer = true,
               const uint8_t *font = u8g2_font_6x12_tr)
{
  if (clear)
    u8g2.clearBuffer();

  int width = u8g2.getDisplayWidth();

  u8g2.setFont(font);

  switch (align)
  {
  case ALIGN_CENTER:
    u8g2.setCursor((width - u8g2.getUTF8Width(text.c_str())) / 2, y);
    break;
  case ALIGN_LEFT:
    u8g2.setCursor(0, y);
    break;
  case ALIGN_RIGHT:
    u8g2.setCursor(width - u8g2.getUTF8Width(text.c_str()), y);
    break;
  default:
    u8g2.setCursor(align, y);
    break;
  }
  u8g2.print(text.c_str());

  if (sendBuffer)
  {
    u8g2.sendBuffer();
  }
}

void log(String owner, const char *format, ...)
{
  char loc_buf[64];
  char *temp = loc_buf;
  va_list arg;
  va_list copy;
  va_start(arg, format);
  va_copy(copy, arg);
  int len = vsnprintf(temp, sizeof(loc_buf), format, copy);
  va_end(copy);

  if (len < 0)
  {
    va_end(arg);
    return;
  }

  if (len >= (int)sizeof(loc_buf))
  {
    temp = (char *)malloc(len + 1);
    if (temp == NULL)
    {
      va_end(arg);
      return;
    }
    len = vsnprintf(temp, len + 1, format, arg);
  }

  va_end(arg);

  Serial.print("[");
  Serial.print(owner);
  Serial.print("] ");
  Serial.println(temp);

  if (temp != loc_buf)
  {
    free(temp);
  }
}

bool isPressed(short pin, bool useVdelay = true)
{
  auto dl = [useVdelay](int ms)
  {
    if (useVdelay)
      vTaskDelay(ms);
    else
      delay(ms);
  };

  if (digitalRead(pin) == HIGH)
  {
    dl(50);
    if (digitalRead(pin) == HIGH)
      return true;
    else
      return false;
  }

  return false;
}

void tareMode()
{
  if (ReadDataHandle != NULL)  
    vTaskSuspend(ReadDataHandle);
  
  digitalWrite(pins::waterPump, HIGH);

  log("TARE", "hmm");

  auto releaseBtn = []()
  {
    u8g2.clearBuffer();
    fastPrint(ALIGN_LEFT, 7, "Release the button", false);
    while (digitalRead(pins::select) == HIGH)
      ;

    u8g2.clearBuffer();
  };

  fastPrint(0, 7, "Empty Tank A");

  while (!isPressed(pins::select))
  {
    vTaskDelay(5 / portTICK_PERIOD_MS);
  };

  releaseBtn();

  SpiffsHelper::writeFile("/TankA0", String(OverGroundTank.scale.read_average(20)));

  // fastPrint(0, 7, "Empty Tank B");

  // while (!isPressed(pins::select))
  // {
  //   vTaskDelay(50 / portTICK_PERIOD_MS);
  // };
  // while (isPressed(pins::select))
  // {
  //   vTaskDelay(50 / portTICK_PERIOD_MS);
  // };

  // SpiffsHelper::writeFile("/TankB0", String(B.scale.read_average(20)));

  fastPrint(0, 7, "Fill Tank A");

  while (!isPressed(pins::select))
  {
    vTaskDelay(5 / portTICK_PERIOD_MS);
  };
  releaseBtn();

  SpiffsHelper::writeFile("/TankA100", String(OverGroundTank.scale.read_average(20)));

  // fastPrint(0, 7, "Fill Tank B");

  // while (!isPressed(pins::select))
  // {
  //   vTaskDelay(50 / portTICK_PERIOD_MS);
  // };
  // while (isPressed(pins::select))
  // {
  //   vTaskDelay(50 / portTICK_PERIOD_MS);
  // };

  // SpiffsHelper::writeFile("/TankB100", String(B.scale.read_average(20)));

  ESP.restart();
}

void WiFiManageTask(void *parameters)
{
  for (;;)
  {
    if (WiFi.status() == WL_CONNECTED)
    {
      // WiFi is still connected
      // log("WiFi", "OK");
      vTaskDelay(TIMEOUT_MS / portTICK_PERIOD_MS);
      continue;
    }
    // usingPortal = wm.getConfigPortalActive();

    log("WiFi", "disconnected");
    log("WiFi", " Attempting reconnection");

    unsigned long startTime = millis();

    WiFi.mode(WIFI_STA);
    WiFi.begin(wm.getWiFiSSID(true).c_str(), wm.getWiFiPass(true).c_str());

    while (WiFi.status() != WL_CONNECTED && millis() - startTime < TIMEOUT_MS)
    {
      vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    if (WiFi.status() != WL_CONNECTED)
    {
      log("WiFi", "FAIL");
      vTaskDelay(20000 / portTICK_PERIOD_MS);
      continue;
    }

    log("WiFi", "OK");
  }
}

void DisplayManageTask(void *parameters)
{

  auto renderBar = [](bool renderSettings = true, bool renderManager = false)
  {
    u8g2.drawLine(0, 64 - 16 - 3, 128, 64 - 16 - 3);
    fastPrint(ALIGN_LEFT, 59, rtc.getTime("%H:%M"), false, false);

    if (WiFi.status() == WL_CONNECTED)
      u8g2.drawBitmap(128 - 16, 64 - 16, 16 / 8, 16, bmpWiFi);
    else if (wm.getConfigPortalActive())
      u8g2.drawBitmap(128 - 16, 64 - 16, 16 / 8, 16, bmpManager);
    else if (renderManager)
      u8g2.drawBitmap(128 - 16, 64 - 16, 16 / 8, 16, bmpManager);
    else
      u8g2.drawBitmap(128 - 16, 64 - 16, 16 / 8, 16, bmpNoWiFi);

    if (renderSettings)
      u8g2.drawBitmap(128 - 33, 64 - 16, 16 / 8, 16, bmpSettings);
  };

  auto renderMenu = [renderBar](String items[], int numMenuItems)
  {
    bool exit = false;
    // int numMenuItems = sizeof(items) / sizeof(items[0]); // Change this to
    // the actual number of menu items
    int selectedIndex = 0;

    while (!exit)
    {
      u8g2.clear();

      int startIdx = max(0, selectedIndex - 1);
      int endIdx = min(numMenuItems - 1, startIdx + 2);

      for (int i = startIdx; i <= endIdx; i++)
      {
        if (i == selectedIndex)
        {
          u8g2.drawButtonUTF8(3, 13 * (i - startIdx + 1), U8G2_BTN_INV, 0, 2, 2,
                              (String(i + 1) + ". " + items[i]).c_str());
        }
        else
        {
          u8g2.drawButtonUTF8(3, 13 * (i - startIdx + 1), U8G2_BTN_BW0, 0, 2, 2,
                              (String(i + 1) + ". " + items[i]).c_str());
        }
      }

      renderBar();
      u8g2.sendBuffer();

      while (true)
      {
        if (isPressed(pins::up))
        {
          selectedIndex = (selectedIndex - 1 + numMenuItems) % numMenuItems;
          while (digitalRead(pins::up) == HIGH)
            ;
          break;
        }

        if (isPressed(pins::down))
        {
          selectedIndex = (selectedIndex + 1) % numMenuItems;
          while (digitalRead(pins::down) == HIGH)
            ;
          break;
        }

        if (isPressed(pins::select))
        {
          while (digitalRead(pins::select) == HIGH)
            ;
          return items[selectedIndex];
        }

        vTaskDelay(50 / portTICK_PERIOD_MS);
      }
    }
  };

  auto renderMenuTitle = [renderBar](String title, String items[],
                                     int numMenuItems)
  {
    bool exit = false;
    int selectedIndex = 0;

    while (!exit)
    {
      u8g2.clear();

      u8g2.setFont(u8g2_font_6x13_tr);
      u8g2.drawButtonUTF8(0, 13, U8G2_BTN_BW0, 0, 2, 2, title.c_str());
      u8g2.drawLine(0, 17, 128, 17);
      u8g2.setFont(u8g2_font_6x12_tr);

      int startIdx = max(0, selectedIndex - 1);
      int endIdx = min(numMenuItems - 1, startIdx + 2);

      for (int i = startIdx; i <= endIdx; i++)
      {
        if (i == selectedIndex)
        {
          u8g2.drawButtonUTF8(3, 13 * (i - startIdx + 1) + 15, U8G2_BTN_INV, 0,
                              2, 2, (String(i + 1) + ". " + items[i]).c_str());
        }
        else
        {
          u8g2.drawButtonUTF8(3, 13 * (i - startIdx + 1) + 15, U8G2_BTN_BW0, 0,
                              2, 2, (String(i + 1) + ". " + items[i]).c_str());
        }
      }

      renderBar();
      u8g2.sendBuffer();

      while (true)
      {
        if (isPressed(pins::up))
        {
          selectedIndex = (selectedIndex - 1 + numMenuItems) % numMenuItems;
          while (digitalRead(pins::up) == HIGH)
            ;
          break;
        }

        if (isPressed(pins::down))
        {
          selectedIndex = (selectedIndex + 1) % numMenuItems;
          while (digitalRead(pins::down) == HIGH)
            ;
          break;
        }

        if (isPressed(pins::select))
        {
          while (digitalRead(pins::select) == HIGH)
            ;
          return items[selectedIndex];
        }

        vTaskDelay(50 / portTICK_PERIOD_MS);
      }
    }
  };

  for (;;)
  {
    bool dontUseDelay = false;
    u8g2.clearBuffer();

    renderBar(false);

    if (digitalRead(pins::menu) == HIGH)
    {
      renderBar(true);

      fastPrint(ALIGN_LEFT, 7, "Release the button", false);
      while (digitalRead(pins::menu) == HIGH)
        ;

    mainMenu:
      u8g2.clear();
      renderBar(true);

      String result =
          renderMenu(menuItems, sizeof(menuItems) / sizeof(menuItems[0]));

      if (result == "Exit")
      {
        while (isPressed(pins::select))
          ;
      }
      else if (result == "WiFi Config")
      {
        if (renderMenuTitle("Start Manager?", wifiMenu,
                            sizeof(wifiMenu) / sizeof(wifiMenu[0])) == "Yes")
        {

          fastPrint(ALIGN_LEFT, 7, "Running Portal", true, false);
          fastPrint(ALIGN_LEFT, 15, "Press <reset> to stop", false, true);

          renderBar(true, true);
          u8g2.sendBuffer();

          wm.startConfigPortal(WM_AP_NAME);

          vTaskDelay(75 / portTICK_PERIOD_MS);
          wm.stopConfigPortal();

          while (isPressed(pins::menu))
            ;
        }
        goto mainMenu;
      }
      else if (result == "Tare Mode")
      {
        tareMode();
      }
      else if (result == "Settings")
      {
        goto mainMenu;
      }
      else if (result == "Status")
      {
        /**********************************
         * THE DATA :
         *  1. Temprature
         *  2. Uptime
         *  3. IP Address
         *  4. OTA Functionality (if enabled)
         *  5. ...
         **********************************/

        while (!isPressed(pins::menu))
        {
          int hours = esp_timer_get_time() / 3600000000 % 24;
          int mins = esp_timer_get_time() / 60000000 % 60;
          int secs = esp_timer_get_time() / 1000000 % 60;

          fastPrint(ALIGN_LEFT, 13,
                    String(hours) + ":" + String(mins) + ":" + String(secs),
                    true, false);
          renderBar();
          fastPrint(ALIGN_LEFT, 26, WiFi.localIP().toString(), false, true);
          vTaskDelay(50 / portTICK_PERIOD_MS);
        }

        while (isPressed(pins::menu))
        {
          vTaskDelay(50 / portTICK_PERIOD_MS);
        }

        goto mainMenu;
      }

      dontUseDelay = true;
    }
    else
    {

      // if (PayloadStack.size() > 1)
      //   fastPrint(ALIGN_RIGHT, 30, String(PayloadStack.size()), false, false);

      fastPrint(ALIGN_CENTER, 43,
                String("A: ") + String(getLevels().OverGroundTank) + String(" B: ") +
                    String(getLevels().UnderGroundTank),
                false, false);
      // fastPrint(ALIGN_CENTER, 43,
      //           String("A: ") + String(getLevels().A),
      //           false, false);

      u8g2.sendBuffer();
    }

    // Run the task every 200 ms
    if (!dontUseDelay)
      vTaskDelay(50 / portTICK_PERIOD_MS);
  }
}

void UploadTask(void *parameters)
{
  auto upload = [](String Data)
  {
    HTTPClient http;
    http.setTimeout(10000);
    http.begin(SERVER_URL);
    http.addHeader("Content-Type", "application/json");
    int responseCode = http.POST(Data);
    http.end();
    return responseCode;
  };

  for (;;)
  {
    if (WiFi.status() != WL_CONNECTED)
    {
      vTaskDelay(TIMEOUT_MS);
      continue;
    }

    for (int i = 0; i < PayloadStack.size();)
    {
      String data =
          "{\"auth\": \"rjh1i2h\", \"TankA\": " +
          String(PayloadStack[i].OverGroundTank) +
          ", \"UnderGroundTank\": " + String(PayloadStack[i].UnderGroundTank) +
          ", \"time\": {\"Hours\": " + String(PayloadStack[i].time.tm_hour) +
          ", \"Mins\": " + String(PayloadStack[i].time.tm_min) +
          ", \"Secs\": " + String(PayloadStack[i].time.tm_sec) + "}}";

      auto x = upload(data);

      if (x > 0)
      {
        log("PUSH", "OK");
        PayloadStack.erase(PayloadStack.begin() + i);
      }
      else
      {
        log("PUSH", "FAIL ==> %d", x);
        vTaskDelay(TIMEOUT_MS / portTICK_PERIOD_MS);
      }
    }
    vTaskDelay((INTERVAL_MS + 2000) / portTICK_PERIOD_MS);
  }
}

void ReadDataTask(void *parameters)
{
  for (;;)
  {
    Levels l = getLevels();

    log("READ", "%d %d", l.OverGroundTank, l.UnderGroundTank);

    tm now;
    getLocalTime(&now);

    Payload data = {now, l.OverGroundTank, l.UnderGroundTank};
    PayloadStack.push_back(data);

/*     if (getLevels().UnderGroundTank > 50)
    {
      digitalWrite(pins::waterPump, LOW);

      while (getLevels().OverGroundTank < 70)
      {
        vTaskDelay(20 / portTICK_PERIOD_MS);
      };
      digitalWrite(pins::waterPump, HIGH);
    } */

    if (getLevels().OverGroundTank < 80)
    {
      digitalWrite(pins::waterPump, LOW);

      // while (getLevels().UnderGroundTank != 10)
      // ;

      while (getLevels().OverGroundTank <= 90)
      {
        vTaskDelay(500 / portTICK_PERIOD_MS);
      }

      digitalWrite(pins::waterPump, HIGH);
    }

    vTaskDelay(INTERVAL_MS / portTICK_PERIOD_MS);
  }
}

void setup()
{
  Serial.begin(115200);
  u8g2.begin();

  OverGroundTank.scale.begin(pins::loadCellDT, pins::loadCellSCK);
  OverGroundTank.scale.wait_ready();
  OverGroundTank.scale.power_up();
  // B.scale.begin(pins::loadCellDT, pins::loadCellSCK);

  pinMode(pins::menu, INPUT);
  pinMode(pins::select, INPUT);
  pinMode(pins::down, INPUT);
  pinMode(pins::up, INPUT);

  pinMode(pins::waterPump, OUTPUT);
  digitalWrite(pins::waterPump, HIGH);

  if (!SPIFFS.begin())
  {
    // ToDo: some form of error tracking
    tareMode();
  }

  if (!SPIFFS.exists("/TankA0") || !SPIFFS.exists("/TankA100") /* || !SPIFFS.exists("/TankB0") || !SPIFFS.exists("/TankB100") */)
  {

    tareMode();
  }
  else
  {

    OverGroundTank.Tare0 = SpiffsHelper::readFile("/TankA0").toInt();
    OverGroundTank.Tare100 = SpiffsHelper::readFile("/TankA100").toInt();
    // OverGroundTank.Tare0 = 473182;
    // OverGroundTank.Tare100 = 396376;

    // B.Tare0 = SpiffsHelper::readFile("/TankB0").toInt();
    // B.Tare100 = SpiffsHelper::readFile("/TankB100").toInt();
  }

  bool useNTP = false;

  useNTP = !setSystemTimeFromRTC();

  xTaskCreate(DisplayManageTask, "Display Managing task", 1024 * 6, NULL, tskIDLE_PRIORITY,
              &DisplayManageHandle);

  // wm.resetSettings();
  // wm.setDebugOutput(false);

  // wm.setConfigPortalTimeout(180);
  // wm.autoConnect(WM_AP_NAME);
  WiFi.begin("Hobbiton", "taxicab1729");

  if (useNTP)
  {
    if (WiFi.status() == WL_CONNECTED)
    {
      setSystemTimeFromNTP(rtc);
    }
    else
    {
      // TODO: Add some logging / display thing on the OLED
      ESP.restart();
    }
  }

  xTaskCreatePinnedToCore(ReadDataTask, "Read Data task", 4096, NULL, tskIDLE_PRIORITY,
                          &ReadDataHandle, 0);

  // ArduinoOTA.begin();
  // ArduinoOTA.setPassword("admin");
  // ArduinoOTA.setPort(3232);

  /* DISABLE WIFI */

  // xTaskCreatePinnedToCore(WiFiManageTask, "WiFi Managing Task", 4096, NULL, 2,
  //                         &WiFiManageHandle, CONFIG_ARDUINO_RUNNING_CORE);

  // xTaskCreatePinnedToCore(UploadTask, "Upload Data task", 4096, NULL, 2,
  //                         &UploadHandle, CONFIG_ARDUINO_RUNNING_CORE);
  Serial.println("SEND HELP");

}

void loop() {
  // ArduinoOTA.handle();
}
