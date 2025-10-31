

#include <Adafruit_NeoPixel.h>
#include <Arduino.h>
#include <ESP32Servo.h>
#include <GxEPD2_BW.h>

#include "Fonts/FreeMonoBold9pt7b.h"
#include "GxEPD2_3C.h"
#include "GxEPD2_display_selection_new_style.h"

/* IMGS */
#include "koto-and-kemo.h"
#include "koto-densan.h"

SPIClass hspi(HSPI);

/* 入力管理 */
enum ButtonType { BTN_A = 38, BTN_B = 37, BTN_R = 36, BTN_L = 35 };

const int BUTTONS[] = {BTN_A, BTN_B, BTN_R, BTN_L};
const unsigned long DEAD_TIME_MS = 2000;

// loop()が実行されているタスクのハンドル
TaskHandle_t mainTaskHandle = NULL;

// 最後に割り込みが発生した時刻
volatile unsigned long prev_input_time = 0;

// prev_input_timeの安全な読み書きのため
static portMUX_TYPE muxPrevInputTime = portMUX_INITIALIZER_UNLOCKED;

void IRAM_ATTR input_isr() {
    unsigned long now = millis();

    /* -- グローバル変数の読み書き -- */
    portENTER_CRITICAL_ISR(&muxPrevInputTime);
    unsigned long prev = prev_input_time;

    if (now < prev + DEAD_TIME_MS) {
        /* 不感時間未経過 */
        portEXIT_CRITICAL_ISR(&muxPrevInputTime);
        return;
    }

    /* 前回時間を更新 */
    prev_input_time = now;
    portEXIT_CRITICAL_ISR(&muxPrevInputTime);
    /* -- END グローバル変数の読み書き -- */

    /* タスク通知 */
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (mainTaskHandle != NULL) {
        vTaskNotifyGiveFromISR(mainTaskHandle, &xHigherPriorityTaskWoken);
    }

    /* 現在実行中のタスクよりも優先度の高いタスクがReadyになったらyield */
    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}

/* LED管理 */
Adafruit_NeoPixel leds(/* LED NUM */ 1, /* PIN */ 48, NEO_GRB + NEO_KHZ800);

/* 電子ペーパー管理 */
const int APP_NUM = 2;

enum UiApps {
    ImageView,
    TextView,
};

typedef struct {
    UiApps app;
    int imgidx;
} UiState;

static UiState ui_state;

typedef struct {
    uint16_t width;
    uint16_t height;
    const uint8_t* img;
} Img;

/* 画像登録 */
const Img KOTOKEMO = {
    .width = 400,
    .height = 300,
    .img = kotoandkemo,
};

const Img KOTODENSAN = {
    .width = 400,
    .height = 300,
    .img = kotodensan,
};

const int IMG_NUM = 2;
const Img IMGS[] = {KOTOKEMO, KOTODENSAN};

/* サーボモータ関連 */
#define SERVO_PIN 47
Servo myservo;
TaskHandle_t servoTaskHandle = NULL;

void servoTask(void* _pvParameters) {
    while (true) {
        /* Blockにする */
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        Serial.println("Servo Task: RUN");
        int pos = 10;
        for (pos = 10; pos <= 170; pos += 1) {
            myservo.write(pos);
            vTaskDelay(pdMS_TO_TICKS(15));
        }
        vTaskDelay(pdMS_TO_TICKS(2000));

        for (pos = 170; pos >= 10; pos -= 1) {
            myservo.write(pos);
            vTaskDelay(pdMS_TO_TICKS(15));
        }
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println();
    Serial.println("setup");

    Serial.print("setup() Run in Core ");
    Serial.println(xPortGetCoreID());

    /* 入力管理 */
    mainTaskHandle = xTaskGetCurrentTaskHandle();

    if (mainTaskHandle == NULL) {
        Serial.println("FATAL: mainTaskHandleの取得に失敗");
        while (1);  // 致命的エラー
    }

    /* 割り込みの設定 */
    for (int button : BUTTONS) {
        pinMode(button, INPUT_PULLUP);

        // 割り込みを設定
        // digitalPinToInterrupt(pin): ピン番号を割り込み番号に変換
        attachInterrupt(digitalPinToInterrupt(button), input_isr, FALLING);
    }

    /* LED管理 */
    leds.begin();
    leds.clear();
    leds.show();

    /* epaper管理 */
    hspi.begin(12, 13, 11, 10);
    display.epd2.selectSPI(hspi, SPISettings(4000000, MSBFIRST, SPI_MODE0));
    display.init(115200);
    ui_state.app = ImageView;
    ui_state.imgidx = 0;

    /* Servo管理 */
    ESP32PWM::allocateTimer(0);
    ESP32PWM::allocateTimer(1);
    ESP32PWM::allocateTimer(2);
    ESP32PWM::allocateTimer(3);
    myservo.setPeriodHertz(50);
    myservo.attach(SERVO_PIN, 1000, 2000);
    xTaskCreatePinnedToCore(servoTask,    // 実行するタスク関数
                            "ServoTask",  // タスク名 (デバッグ用)
                            4096,         // スタックサイズ (Word単位)
                            NULL,         // タスクに渡す引数 (今回はNULL)
                            1,            // タスクの優先度 (loop()と同じ1でOK)
                            &servoTaskHandle,  // タスクハンドルを格納する変数
                            0                  // 実行するコア (Core 0)
    );

    if (servoTaskHandle == NULL) {
        Serial.println("FATAL: Servo Taskの作成に失敗");
        while (1);  // 致命的エラー
    }
}

const char msg1[] = "OUnion TeamB";
const char msg2[] = "Eleci x Craft";
const char msg3[] = "hinshiba";
const char msg4[] = "sumomo, sularin, satoki";

// 文字列のポインタ配列にまとめる
const char* messages[] = {msg1, msg2, msg3, msg4};
const int num_lines = 4;  // 行数

void printmsg() {
    display.setFont(&FreeMonoBold9pt7b);
    display.setTextColor(GxEPD_BLACK);

    int16_t tbx, tby;
    uint16_t tbw, tbh;
    uint16_t x_coords[num_lines];  // 各行のX座標を格納する配列

    // display.getTextBounds(msg, 0, 0, &tbx, &tby, &tbw, &tbh);
    // // center the bounding box by transposition of the origin:
    // uint16_t x = ((display.width() - tbw) / 2) - tbx;
    // uint16_t y = ((display.height() - tbh) / 2) - tby;
    // display.setFullWindow();
    // display.firstPage();
    // do
    // {
    //   display.fillScreen(GxEPD_WHITE);
    //   display.setCursor(x, y);
    //   display.print(msg);
    // }
    // while (display.nextPage());

    // 1. フォントの基本の高さを取得 (代表文字 "T" を使用)
    // これにより、tby (ベースラインオフセット) と tbh (高さ) が決まる
    display.getTextBounds("T", 0, 0, &tbx, &tby, &tbw, &tbh);

    // 2. 1行の高さ（行送り）を決定
    // (例: バウンディングボックスの高さ + 6ピクセルの行間)
    uint16_t line_height = tbh + 6;

    // 3. 全体の高さを計算
    // (行数 * 行送り) - 最後の行間
    uint16_t total_height = (line_height * num_lines) - 6;

    // 4. 最初の行の描画ベースラインY座標を計算
    // (画面高さ - 全高さ) / 2 で、全体の表示領域の上端Y座標が求まる
    // 実際の描画Y座標 (ベースライン) は、そこから tby (負の値) を引いたもの
    uint16_t y_start = ((display.height() - total_height) / 2) - tby;

    // 5. 各行の「水平中央揃え」のためのX座標を事前に計算
    for (int i = 0; i < num_lines; i++) {
        display.getTextBounds(messages[i], 0, 0, &tbx, &tby, &tbw, &tbh);
        x_coords[i] = ((display.width() - tbw) / 2) - tbx;
    }

    display.fillScreen(GxEPD_RED);
    for (int i = 0; i < num_lines; i++) {
        uint16_t current_y = y_start + (i * line_height);
        display.setCursor(x_coords[i], current_y);
        display.print(messages[i]);
    }
    display.display();
}

void print_mono_img(Img img, bool need_fill) {
    display.setFullWindow();
    if (need_fill) {
        display.fillScreen(GxEPD_BLACK);
    }
    display.drawBitmap(0, 0, img.img, img.width, img.height, GxEPD_WHITE);
    display.display();
}

void loop() {
    int pos = 0;
    int button_num = 0;

    /* 割り込み待機 */
    Serial.println("Block Task for ISR wait");
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    Serial.println("Run Task");

    /* 割り込み発生ピンの探索 */
    for (int button : BUTTONS) {
        if (digitalRead(button) == LOW) {
            button_num = button;
            break;
        }
    }

    if (button_num == 0) {
        /* なにが割り込み要因か特定できなかった */
        Serial.println("Error: trigger button not found");
        return;
    }

    /* サーボ駆動 */
    xTaskNotifyGive(servoTaskHandle);

    /* UI計算 */
    /* App切り替えか */
    Serial.println(ui_state.app);
    Serial.println(ui_state.imgidx);
    if (button_num == BTN_B && ui_state.app > 0) {
        ui_state.app = ImageView;
        leds.setPixelColor(0, leds.Color(50, 0, 0));
        leds.show();
        print_mono_img(IMGS[ui_state.imgidx], true);
    } else if (button_num == BTN_A && ui_state.app + 1 < APP_NUM) {
        ui_state.app = TextView;
        leds.setPixelColor(0, leds.Color(0, 50, 0));
        leds.show();
        printmsg();
    } else {
        /* Img切り替えか */
        if (ui_state.app == ImageView) {
            if (button_num == BTN_L && ui_state.imgidx > 0) {
                ui_state.imgidx--;
                print_mono_img(IMGS[ui_state.imgidx], true);
            } else if (button_num == BTN_R && ui_state.imgidx + 1 < IMG_NUM) {
                ui_state.imgidx++;
                print_mono_img(IMGS[ui_state.imgidx], true);
            }
        } else if (ui_state.app == TextView) {
            printmsg();
        }
    }
}
