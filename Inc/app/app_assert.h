/**
  ******************************************************************************
  * @file           : app_assert.h
  * @brief          : Assert record stored in NOINIT RAM (survives watchdog reset)
  ******************************************************************************
  * Stric Roberts, 2026. MIT License — see LICENSE in the project root.
  ******************************************************************************
  */
#ifndef APP_ASSERT_H
#define APP_ASSERT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ASSERT_MAGIC  0xDEADBEEFu

typedef struct {
    uint32_t    magic;
    const char *file;
    int         line;
    const char *func;
    const char *expr;
} assert_record_t;

extern assert_record_t g_assert_record;

#ifdef __cplusplus
}
#endif

#endif /* APP_ASSERT_H */
