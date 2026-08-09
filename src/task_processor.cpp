#include <functional>
#include <utility>

#include "task_processor.hpp"

void TaskProcessor::startTask(std::function<void()> function)
{
    Executor.silent_async(std::move(function));
}
