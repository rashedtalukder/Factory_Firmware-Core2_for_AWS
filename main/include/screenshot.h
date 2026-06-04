/*
 * Screenshot capture over serial (development tool).
 * Remove this file when UI iteration is complete.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Start the screenshot listener task.
 * Sends 'S' over serial to trigger a capture.
 */
void screenshot_init(void);

/**
 * Trigger a screenshot capture immediately.
 */
void screenshot_take(void);

#ifdef __cplusplus
}
#endif
