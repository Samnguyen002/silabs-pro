/***************************************************************************//**
 * @file log.h
 * @brief Generic logging marco service component for my personal project
 * @version 1.0.0
 * @details 
 * Provides a reusable logging framework suitable for embedded and desktop
 * projects.
 * Log out the information, warnings and errors by stage/phase
 * @note This component is designed to be reusable across different projects
*******************************************************************************/

#ifndef LOG_H
#define LOG_H

#include <stdio.h>

#ifndef PRINTF_LOG_NL   
#define PRINTF_LOG_NL   "\r\n"
#endif

#define BUTTON_SERVICE_PREFIX    "[BUTTON]"
#define SYSTEMBOOT_PREFIX        "[BOOT]"
#define ADVERTISING_PREFIX       "[ADVER]"
#define SCANNING_PREFIX          "[SCAN]"
#define CONNECTION_PREFIX        "[CONN]"
#define DISCOVERING_PREFIX       "[DISCOVER]"
#define PAIRING_PREFIX           "[PAIRING]"
#define BONDING_PREFIX           "[BOND]"
#define ACTION_PREFIX            "[ACTION]"

#define LOG_BUTTON(fmt, ...)    printf(BUTTON_SERVICE_PREFIX fmt PRINTF_LOG_NL, ##__VA_ARGS__)
#define LOG_BOOT(fmt, ...)      printf(SYSTEMBOOT_PREFIX fmt PRINTF_LOG_NL, ##__VA_ARGS__)
#define LOG_SCANN(fmt, ...)     printf(SCANNING_PREFIX fmt PRINTF_LOG_NL, ##__VA_ARGS__)
#define LOG_DISC(fmt, ...)      printf(DISCOVERING_PREFIX fmt PRINTF_LOG_NL, ##__VA_ARGS__)
#define LOG_ADVER(fmt, ...)     printf(ADVERTISING_PREFIX fmt PRINTF_LOG_NL, ##__VA_ARGS__)
#define LOG_CONN(fmt, ...)      printf(CONNECTION_PREFIX fmt PRINTF_LOG_NL, ##__VA_ARGS__)
#define LOG_PAIRING(fmt, ...)   printf(PAIRING_PREFIX fmt PRINTF_LOG_NL, ##__VA_ARGS__)
#define LOG_BONDING(fmt, ...)   printf(BONDING_PREFIX fmt PRINTF_LOG_NL, ##__VA_ARGS__)
#define LOG_ACTION(fmt, ...)    printf(ACTION_PREFIX fmt PRINTF_LOG_NL, ##__VA_ARGS__)

#endif