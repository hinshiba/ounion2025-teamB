

#include <Arduino.h>
#include <ESP32Servo.h>
#include <GxEPD2_BW.h>

#include "Fonts/FreeMonoBold9pt7b.h"
#include "GxEPD2_3C.h"
#include "GxEPD2_display_selection_new_style.h"
#include "koto-and-kemo.h"

#define SERVO_PIN 47

const int PIN_INPUT = 2;
const long DEAD_TIME_MS = 500;  // 不感時間 (ミリ秒)

SPIClass hspi(HSPI);
Servo myservo;

void setup() {
    ESP32PWM::allocateTimer(0);
    ESP32PWM::allocateTimer(1);
    ESP32PWM::allocateTimer(2);
    ESP32PWM::allocateTimer(3);
    myservo.setPeriodHertz(50);
    myservo.attach(SERVO_PIN, 1000, 2000);

    Serial.begin(115200);
    Serial.println();
    Serial.println("setup");
    delay(100);
    hspi.begin(12, 13, 11, 10);
    display.epd2.selectSPI(hspi, SPISettings(4000000, MSBFIRST, SPI_MODE0));
    display.init(115200);

    // display.hibernate();
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

    // 6. ページ描画ループ
    // display.setFullWindow();
    // display.firstPage();
    // do {
    //     display.fillScreen(GxEPD_WHITE);

    //     // 4行それぞれを描画
    //     for (int i = 0; i < num_lines; i++) {
    //         // 現在の行のY座標を計算 (Y座標は行送り分だけ下にずらす)
    //         uint16_t current_y = y_start + (i * line_height);

    //         // 計算済みのX座標と、計算したY座標にカーソルをセット
    //         display.setCursor(x_coords[i], current_y);
    //         display.print(messages[i]);
    //     }
    // } while (display.nextPage());

    display.fillScreen(GxEPD_WHITE);
    for (int i = 0; i < num_lines; i++) {
        uint16_t current_y = y_start + (i * line_height);
        display.setCursor(x_coords[i], current_y);
        display.print(messages[i]);
    }
    display.display();
}

void printimg() {
    const uint8_t* bitmap_white = gakusai_monoc;
    int16_t tbx, tby;
    uint16_t tbw, tbh;
    display.setFullWindow();
    // display.firstPage();
    // do {
    //     // 3a. まず画面(の現在のページ)を白で塗りつぶす
    //     display.fillScreen(GxEPD_BLACK);

    //     // 3b. 黒用のビットマップを描画
    //     // drawBitmap(x座標, y座標, ビットマップデータ, 幅, 高さ, 色)
    //     display.drawBitmap(0, 0, bitmap_white, 400, 300, GxEPD_WHITE);

    //     // 3c. カラー用のビットマップ描画を削除 (またはコメントアウト)
    //     // うまく動かない
    //     // display.drawBitmap(0, 0, bitmap_red, 64, 64, GxEPD_RED);
    // } while (display.nextPage());
    display.fillScreen(GxEPD_BLACK);
    display.drawBitmap(0, 0, bitmap_white, 400, 300, GxEPD_WHITE);
    display.display();
}

void loop() {
    int pos = 0;
    printmsg();

    delay(5000);
    for (pos = 0; pos <= 180; pos += 1) {
        myservo.write(pos);
        delay(15);
    }

    printimg();
    delay(5000);
    for (pos = 180; pos >= 0; pos -= 1) {
        myservo.write(pos);
        delay(15);
    }
};
