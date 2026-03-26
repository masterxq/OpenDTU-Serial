// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <TaskSchedulerDeclarations.h>
#include <cstdint>
#include <vector>

class UartInterfaceClass {
public:
    UartInterfaceClass();
    void init(Scheduler& scheduler);
    bool isEnabled() const;

private:
    struct InverterTracker {
        uint32_t lastDataUpdate = 0;
        uint32_t lastLimitDecisionUpdate = 0;
        uint32_t lastStateDecisionUpdate = 0;
    };

    void loop();
    void ensureTrackerSize();
    void processIncomingData();
    void handleCommandLine(const char* line);
    void emitPendingUpdates();
    void emitInverterInventory(bool force = false);

    Task _loopTask;
    std::vector<InverterTracker> _trackers;
    uint32_t _lastInventoryUpdate = 0;

    static constexpr size_t INPUT_BUFFER_SIZE = 256;
    static constexpr uint32_t INVENTORY_INTERVAL_MS = 15 * 1000;
    char _inputBuffer[INPUT_BUFFER_SIZE] = {};
    size_t _inputLength = 0;
    bool _inputOverflow = false;
    bool _enabled = false;
};

extern UartInterfaceClass UartInterface;