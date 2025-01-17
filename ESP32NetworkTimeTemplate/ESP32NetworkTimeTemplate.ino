// ESP32 Network Time Template (v1.0.1)
//
// Copyright Rob Latour, 2025
// License: MIT
// https://github.com/roblatour/ESP32NetorkTimeTemplate
//
// Template Arduino sketch / C++ code for an ESP32 to get and periodically sync the local time from a network time server
// using the Network Time Protocol (NTP) and the ESP32's built-in time functions
//

// this code leverages the built-in libraries for the ESP32
// with no additional libraries being required

#include "Arduino.h"
#include "WiFi.h"
#include <esp_sntp.h>
#include <Ticker.h>
#include <atomic>

// secret_settings.h and user_settings.h are included in the repository
// and are used to set the Wi-Fi, NTP server, and other settings for use with this program

#include "secret_settings.h"
#include "user_settings.h"

const char *wifiSSID = SECRET_SETTINGS_WIFI_SSID;
const char *wifiPassword = SECRET_SETTINGS_WIFI_PASSWORD;
const char *ntpServer1 = USER_SETTINGS_NTP_SERVER_1;
const char *ntpServer2 = USER_SETTINGS_NTP_SERVER_2;
const char *ntpServer3 = USER_SETTINGS_NTP_SERVER_3;
const char *posixTimeZoneCode = USER_SETTINGS_POSIX_TIME_ZONE_CODE;

const float resyncAfterThisManyHours = USER_SETTINGS_RESYNC_TIME_AFTER_THIS_MANY_HOURS;

std::atomic<bool> triggerSyncOfLocalTimeWithNetworkTime(true);
std::atomic<bool> networkTimeSyncComplete(false);

Ticker timeSyncTicker;

void nonBlockingDelay(TickType_t ms)
{

    vTaskDelay(ms / portTICK_PERIOD_MS);
}

void writeToDebugConsole(String message, bool newLine = true)
{
    if (USER_SETTINGS_OUTPUT_DEBUG_INFO_TO_CONSOLE)
        if (newLine)
            Serial.println(message);
        else
            Serial.print(message);
}

bool connectToWifi()
{
    const int maxConnectionAttempts = USER_SETTINGS_MAX_WIFI_CONNECTION_ATTEMPTS;
    const unsigned long timeoutLimitPerWiFiConnectionAttempt = USER_SETTINGS_TIMEOUT_LIMIT_PER_WIFI_CONNECTION_ATTEMPT;

    writeToDebugConsole("Connecting to ", false);
    writeToDebugConsole(wifiSSID);

    WiFi.mode(WIFI_STA);
    WiFi.begin(wifiSSID, wifiPassword);

    int attempt = 0;

    while ((WiFi.status() != WL_CONNECTED) && (attempt < maxConnectionAttempts))
    {
        attempt++;
        writeToDebugConsole("Attempt ", false);
        writeToDebugConsole(String(attempt), false);
        writeToDebugConsole(" ", false);
        unsigned long startAttemptTime = millis();
        while ((WiFi.status() != WL_CONNECTED) && ((millis() - startAttemptTime) < timeoutLimitPerWiFiConnectionAttempt))
        {
            writeToDebugConsole(".", false);
            nonBlockingDelay(100);
        }
        writeToDebugConsole("");
    }

    if (WiFi.status() == WL_CONNECTED)
    {
        writeToDebugConsole("Connected with IP address ", false);
        writeToDebugConsole(String(WiFi.localIP()));
        return true;
    }
    else
    {
        writeToDebugConsole("Failed to connect");
        return false;
    }
}

void disconnectFromWifi()
{

    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    nonBlockingDelay(100);
    writeToDebugConsole("Disconnected from WiFi");
    writeToDebugConsole("");
}

void getNetworkTime()
{

    tm *timeinfo;
    timeval tv;
    time_t now;

    networkTimeSyncComplete = false;

    sntp_set_time_sync_notification_cb([](struct timeval *tv)
                                       { networkTimeSyncComplete = true; });

    configTzTime(posixTimeZoneCode, ntpServer1, ntpServer2, ntpServer3);

    writeToDebugConsole("");
    writeToDebugConsole("Waiting for time synchronization");

    const unsigned long fortySecondTimeout = 40000UL;
    unsigned long startWaitTime = millis();

    while (!networkTimeSyncComplete && (WiFi.status() == WL_CONNECTED) && ((millis() - startWaitTime) < fortySecondTimeout))
    {
        writeToDebugConsole("*", false);
        nonBlockingDelay(100);
    }

    writeToDebugConsole("");

    if (networkTimeSyncComplete)
    {

        time(&now);
        timeinfo = localtime(&now);

        writeToDebugConsole("Time synchronized to: ", false);
        writeToDebugConsole(String(asctime(timeinfo)));
    }
    else
    {
        writeToDebugConsole("Time synchronization failed!");
    }
}

void refreshNetworkTimeAsNeeded()
{
    if (triggerSyncOfLocalTimeWithNetworkTime)
    {
        triggerSyncOfLocalTimeWithNetworkTime = false;

        if (connectToWifi())
        {
            getNetworkTime();
            disconnectFromWifi();
        }
    }
}
void showCurrentTimeInDebugWindow()
{
    static int lastDisplayedSecond = -1;
    time_t now;
    tm *timeinfo;

    time(&now);
    timeinfo = localtime(&now);

    if (timeinfo->tm_sec != lastDisplayedSecond)
    {
        lastDisplayedSecond = timeinfo->tm_sec;

        char buffer[25];
        snprintf(buffer, sizeof(buffer), "%04d/%02d/%02d %02d:%02d:%02d", timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);

        writeToDebugConsole(String(buffer));
    }
}

void openDebugConsoleAsNeeded()
{

    if (USER_SETTINGS_OUTPUT_DEBUG_INFO_TO_CONSOLE)
    {
        Serial.begin(USER_SETTINGS_OUTPUT_SERIAL_BAUD_RATE);
        const TickType_t twoSecondDelay = 2000;
        nonBlockingDelay(twoSecondDelay);
    };
}

void setup()
{

    openDebugConsoleAsNeeded();

    writeToDebugConsole("Starting");

    // the loop() subroutine calls refreshNetworkTimeAsNeeded to sync the local time with the network time whenever
    // triggerSyncOfLocalTimeWithNetworkTime is set to true, this will happen:
    // 1. the first time the loop runs as the initial value of triggerSyncOfLocalTimeWithNetworkTime is true, and
    // 2. every resyncTimeInMinutes after that based on the ticker setup directly below

    timeSyncTicker.attach(resyncAfterThisManyHours * (float)360, []()
                          { triggerSyncOfLocalTimeWithNetworkTime = true; });
}

void loop()
{
    refreshNetworkTimeAsNeeded();

    showCurrentTimeInDebugWindow();
}
