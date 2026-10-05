/*
 * AWS IoT Kit - M5Stack Core2
 * Factory Firmware v3.0.0
 * crypto.h
 * 
 * Copyright (C) 2022 Rashed Talukder. All Rights Reserved.
 * Copyright (C) 2022 M5Stack. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/**
 * @file crypto.h
 * @brief ATECC608A hardware secure element tab interface.
 */

#pragma once

#include <stdbool.h>
#include "lvgl.h"

/** @brief Tab identifier and log tag for the ATECC608 crypto tab. */
#define CRYPTO_TAB_NAME "ATECC608-CRYPTO"

/**
 * @brief Build the ATECC608 crypto-authentication tab UI.
 *
 * @param tv Parent LVGL tabview widget.
 */
void display_crypto_tab( lv_obj_t *tv );

/**
 * @brief Lazily read and display the ATECC608 serial number when the tab opens.
 *
 * @param active true when the tab becomes active; false when hidden.
 */
void crypto_set_active( bool active );