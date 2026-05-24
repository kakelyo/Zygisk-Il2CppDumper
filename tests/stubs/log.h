#pragma once
// Stub for Android log.h — redirects to printf/stderr on non-Android

#include <cstdio>

#define LOG_TAG "test"

#define LOGD(...) do { printf("[DEBUG] "); printf(__VA_ARGS__); printf("\n"); } while(0)
#define LOGI(...) do { printf("[INFO ] "); printf(__VA_ARGS__); printf("\n"); } while(0)
#define LOGW(...) do { printf("[WARN ] "); printf(__VA_ARGS__); printf("\n"); } while(0)
#define LOGE(...) do { fprintf(stderr, "[ERROR] "); fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); } while(0)
