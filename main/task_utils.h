#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void safe_task_create(TaskFunction_t fn, const char *name,
                       uint32_t stack, void *arg,
                       UBaseType_t prio, TaskHandle_t *handle);
