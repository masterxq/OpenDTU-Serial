// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2024-2025 Thomas Basler and others
 */
#include "CommandQueue.h"
#include "../inverters/InverterAbstract.h"
#include <algorithm>

namespace {
bool isPollingCommand(const String& commandName)
{
    return commandName == "RealTimeRunData"
        || commandName == "AlarmData"
        || commandName == "DevInfoAll"
        || commandName == "DevInfoSimple"
        || commandName == "SystemConfigPara"
        || commandName == "GridOnProFilePara"
        || commandName == "ChannelChangeCommand";
}
}

void CommandQueue::removeAllEntriesForInverter(InverterAbstract* inv)
{
    std::lock_guard<std::mutex> lock(_mutex);

    auto it = std::remove_if(_queue.begin(), _queue.end(),
        [&](const auto& v) { return v->getTargetAddress() == inv->serial(); });
    _queue.erase(it, _queue.end());
}

void CommandQueue::removePollingEntriesForInverter(InverterAbstract* inv)
{
    std::lock_guard<std::mutex> lock(_mutex);

    auto start = _queue.begin();
    if (!_queue.empty() && _queue.front()->getSendCount() > 0) {
        ++start;
    }

    auto it = std::remove_if(start, _queue.end(),
        [&](const auto& v) {
            return v->getTargetAddress() == inv->serial() && isPollingCommand(v->getCommandName());
        });
    _queue.erase(it, _queue.end());
}

void CommandQueue::removeDuplicatedEntries(std::shared_ptr<CommandAbstract> cmd)
{
    std::lock_guard<std::mutex> lock(_mutex);

    auto start = _queue.begin();
    if (!_queue.empty() && _queue.front()->getSendCount() > 0) {
        ++start;
    }

    auto it = std::remove_if(start, _queue.end(),
        [&](const auto& v) {
            return cmd->areSameParameter(v.get())
                && cmd.get()->getQueueInsertType() == QueueInsertType::RemoveOldest;
        });
    _queue.erase(it, _queue.end());
}

void CommandQueue::replaceEntries(std::shared_ptr<CommandAbstract> cmd)
{
    std::lock_guard<std::mutex> lock(_mutex);

    auto start = _queue.begin();
    if (!_queue.empty() && _queue.front()->getSendCount() > 0) {
        ++start;
    }

    std::replace_if(start, _queue.end(),
        [&](const auto& v) {
            return cmd.get()->getQueueInsertType() == QueueInsertType::ReplaceExistent
                && cmd->areSameParameter(v.get());
            },
        cmd
    );
}

uint8_t CommandQueue::countSimilarCommands(std::shared_ptr<CommandAbstract> cmd)
{
    std::lock_guard<std::mutex> lock(_mutex);

    return std::count_if(_queue.begin(), _queue.end(),
        [&](const auto& v) {
            return cmd->areSameParameter(v.get());
        });
}
