/* SPDX-License-Identifier: GPL-3.0-or-later */

#pragma once

#define USE_SX1262
#define HAS_RADIO 1
#define HAS_WIRE 1

#define WIRE_INTERFACES_COUNT 1
#define PIN_WIRE_SDA 36
#define PIN_WIRE_SCL 11
#define I2C_SDA PIN_WIRE_SDA
#define I2C_SCL PIN_WIRE_SCL

#define LORA_MISO 2
#define LORA_MOSI 47
#define LORA_SCK 43
#define LORA_CS 45
#define LORA_DIO1 10
#define LORA_RESET 9
#define LORA_BUSY 29

#define SX126X_CS LORA_CS
#define SX126X_DIO1 LORA_DIO1
#define SX126X_RESET LORA_RESET
#define SX126X_BUSY LORA_BUSY
#define SX126X_RXEN 17
#define LORA_SPI_FREQUENCY 1000000UL

#define SX126X_DIO2_AS_RF_SWITCH
#define TCXO_OPTIONAL
#define SX126X_DIO3_TCXO_VOLTAGE 1.8f
#define SX126X_MAX_POWER 14

#define BUTTON_PIN (-1)
#define ALT_BUTTON_PIN (-1)
#define PIN_LED1 (-1)
#define LED_STATE_ON 1
