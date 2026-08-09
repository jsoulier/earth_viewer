#pragma once

#include <CesiumAsync/ITaskProcessor.h>
#include <taskflow/taskflow.hpp>

#include <functional>

class TaskProcessor : public CesiumAsync::ITaskProcessor
{
public:
    void startTask(std::function<void()> function) override;

private:
    tf::Executor Executor;
};
