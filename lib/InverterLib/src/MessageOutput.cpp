// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022-2024 Thomas Basler and others
 */
#include "MessageOutput.h"

#include <Arduino.h>

MessageOutputClass MessageOutput;

MessageOutputClass::MessageOutputClass()
    : _loopTask(TASK_IMMEDIATE, TASK_FOREVER, std::bind(&MessageOutputClass::loop, this))
{
    logDebug = false;
}

void MessageOutputClass::init(Scheduler& scheduler)
{
    scheduler.addTask(_loopTask);
    _loopTask.enable();
}

void MessageOutputClass::register_ws_output(AsyncWebSocket* output)
{
    _ws = output;
}

size_t MessageOutputClass::write(uint8_t c)
{
    if (_buff_pos < BUFFER_SIZE) {
        std::lock_guard<std::mutex> lock(_msgLock);
        _buffer[_buff_pos] = c;
        _buff_pos++;
    } else {
        _forceSend = true;
    }

    return Serial.write(c);
}

size_t MessageOutputClass::write(const uint8_t* buffer, size_t size)
{
    std::lock_guard<std::mutex> lock(_msgLock);
    if (_buff_pos + size < BUFFER_SIZE) {
        memcpy(&_buffer[_buff_pos], buffer, size);
        _buff_pos += size;
    }
    _forceSend = true;

    return Serial.write(buffer, size);
}

void MessageOutputClass::loop()
{
    // Send data via websocket if either time is over or buffer is full
    if (_forceSend || (millis() - _lastSend > 1000)) {
        std::lock_guard<std::mutex> lock(_msgLock);
        if (_ws && _buff_pos > 0) {
            _ws->textAll(_buffer, _buff_pos);
            _buff_pos = 0;
        }
        if (_forceSend) {
            _buff_pos = 0;
        }
        _forceSend = false;
    }
}

void MessageOutputClass::printlnDebug(const StringSumHelper & helper){
    if(logDebug){
        println(helper.c_str());
    }
}

void MessageOutputClass::printDebug(const StringSumHelper & helper){
    if(logDebug){
        print(helper.c_str());
    }
}

size_t MessageOutputClass::printfDebug(const char *format, ...)
{
    if(!logDebug){
        return 0;
    }

    //just coppied from prinf function (call of original function not working for some reaseon (sometimes arguments wrong))
    char loc_buf[64];
    char * temp = loc_buf;
    va_list arg;
    va_list copy;
    va_start(arg, format);
    va_copy(copy, arg);
    int len = vsnprintf(temp, sizeof(loc_buf), format, copy);
    va_end(copy);
    if(len < 0) {
        va_end(arg);
        return 0;
    }
    if(len >= (int)sizeof(loc_buf)){  // comparation of same sign type for the compiler
        temp = (char*) malloc(len+1);
        if(temp == NULL) {
            va_end(arg);
            return 0;
        }
        len = vsnprintf(temp, len+1, format, arg);
    }
    va_end(arg);
    len = write((uint8_t*)temp, len);
    if(temp != loc_buf){
        free(temp);
    }
    return len;
}