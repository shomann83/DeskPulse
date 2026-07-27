#pragma once
#include <Arduino.h>

enum UsageProvider : uint8_t {
    USAGE_PROVIDER_CLAUDE = 0,
    USAGE_PROVIDER_CODEX = 1,
    USAGE_PROVIDER_COUNT = 2,
};

struct ProviderUsageData {
    float session_pct;       // short-window utilization (0-100)
    int session_reset_mins;  // minutes until session resets
    float weekly_pct;        // 7-day window utilization (0-100)
    int weekly_reset_mins;   // minutes until weekly resets
    char status[24];         // "allowed", "limited", or provider-specific error
    bool ok;                 // data parse succeeded
    bool valid;              // false until this provider has data

    // Window lengths in seconds, 0 when the provider has no such window.
    // ChatGPT lost its 5h window in July 2026 and reports weekly only, so a
    // missing window must not read as "0% used".
    int session_window_s;
    int weekly_window_s;

    // Prepaid credits (ChatGPT only). Shown in the slot the 5h window vacated;
    // if a session window ever comes back it takes that slot and credits drop.
    bool credits_valid;
    bool credits_has;
    char credits_balance[12];
    int credits_msgs;
};

struct UsageData {
    ProviderUsageData providers[USAGE_PROVIDER_COUNT];
    UsageProvider primary_provider;  // legacy top-level fields map here
    bool dual;                       // true when both providers were received
    bool valid;                      // false until first successful parse
};
