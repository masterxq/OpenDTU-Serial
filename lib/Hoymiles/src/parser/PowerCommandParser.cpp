// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022 - 2023 Thomas Basler and others
 */
#include "PowerCommandParser.h"

void PowerCommandParser::setLastPowerCommandSuccess(const LastCommandSuccess status)
{
    _lastLimitCommandSuccess = status;
}

LastCommandSuccess PowerCommandParser::getLastPowerCommandSuccess() const
{
    return _lastLimitCommandSuccess;
}

void PowerCommandParser::setLastCompletedPowerCommandSuccess(const LastCommandSuccess status)
{
    _lastCompletedPowerCommandSuccess = status;
    _hasCompletedPowerCommandResult = true;
    _lastCompletedPowerCommandUpdate = millis();
    if (status == CMD_OK) {
        _lastSuccessfulPowerCommandUpdate = _lastCompletedPowerCommandUpdate;
    }
}

LastCommandSuccess PowerCommandParser::getLastCompletedPowerCommandSuccess() const
{
    return _lastCompletedPowerCommandSuccess;
}

bool PowerCommandParser::hasCompletedPowerCommandResult() const
{
    return _hasCompletedPowerCommandResult;
}

uint32_t PowerCommandParser::getLastCompletedPowerCommandUpdate() const
{
    return _lastCompletedPowerCommandUpdate;
}

uint32_t PowerCommandParser::getLastSuccessfulPowerCommandUpdate() const
{
    return _lastSuccessfulPowerCommandUpdate;
}

uint32_t PowerCommandParser::getLastUpdateCommand() const
{
    return _lastUpdateCommand;
}

void PowerCommandParser::setLastUpdateCommand(const uint32_t lastUpdate)
{
    _lastUpdateCommand = lastUpdate;
    setLastUpdate(lastUpdate);
}

void PowerCommandParser::setLastStateCommand(const LastStateCommand state)
{
    _lastStateCommand = state;
    _hasLastStateCommand = true;
}

LastStateCommand PowerCommandParser::getLastStateCommand() const
{
    return _lastStateCommand;
}

bool PowerCommandParser::hasLastStateCommand() const
{
    return _hasLastStateCommand;
}

void PowerCommandParser::setRequestedStateCommand(const LastStateCommand state)
{
    _requestedStateCommand = state;
}

LastStateCommand PowerCommandParser::getRequestedStateCommand() const
{
    return _requestedStateCommand;
}

void PowerCommandParser::setRequestedPowerCommand(const PowerCommandType command)
{
    _requestedPowerCommand = command;
}

PowerCommandType PowerCommandParser::getRequestedPowerCommand() const
{
    return _requestedPowerCommand;
}

void PowerCommandParser::setLastCompletedPowerCommand(const PowerCommandType command)
{
    _lastCompletedPowerCommand = command;
}

PowerCommandType PowerCommandParser::getLastCompletedPowerCommand() const
{
    return _lastCompletedPowerCommand;
}
