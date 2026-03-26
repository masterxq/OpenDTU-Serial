// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "Parser.h"

enum class LastStateCommand {
    Unknown,
    On,
    Off,
};

enum class PowerCommandType {
    Unknown,
    On,
    Off,
    Restart,
};

class PowerCommandParser : public Parser {
public:
    void setLastPowerCommandSuccess(const LastCommandSuccess status);
    LastCommandSuccess getLastPowerCommandSuccess() const;
    void setLastCompletedPowerCommandSuccess(const LastCommandSuccess status);
    LastCommandSuccess getLastCompletedPowerCommandSuccess() const;
    bool hasCompletedPowerCommandResult() const;
    uint32_t getLastCompletedPowerCommandUpdate() const;
    uint32_t getLastUpdateCommand() const;
    void setLastUpdateCommand(const uint32_t lastUpdate);
    void setLastStateCommand(const LastStateCommand state);
    LastStateCommand getLastStateCommand() const;
    bool hasLastStateCommand() const;
    void setRequestedStateCommand(const LastStateCommand state);
    LastStateCommand getRequestedStateCommand() const;
    void setRequestedPowerCommand(const PowerCommandType command);
    PowerCommandType getRequestedPowerCommand() const;
    void setLastCompletedPowerCommand(const PowerCommandType command);
    PowerCommandType getLastCompletedPowerCommand() const;

private:
    LastCommandSuccess _lastLimitCommandSuccess = CMD_OK; // Set to OK because we have to assume nothing is done at startup
    LastCommandSuccess _lastCompletedPowerCommandSuccess = CMD_OK;
    bool _hasCompletedPowerCommandResult = false;

    uint32_t _lastUpdateCommand = 0;
    uint32_t _lastCompletedPowerCommandUpdate = 0;
    LastStateCommand _lastStateCommand = LastStateCommand::Unknown;
    bool _hasLastStateCommand = false;
    LastStateCommand _requestedStateCommand = LastStateCommand::Unknown;
    PowerCommandType _requestedPowerCommand = PowerCommandType::Unknown;
    PowerCommandType _lastCompletedPowerCommand = PowerCommandType::Unknown;
};
