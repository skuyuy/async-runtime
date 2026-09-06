# AsyncRT - asynchronous runtime

`asyncrt` is an asynchronous runtime roughly inspired by the C# `Task` and `IAsyncOperation` systems.

## Structure

| Module    | Description                               | Status                                                                                               |
|-----------|-------------------------------------------|------------------------------------------------------------------------------------------------------|
| `core`    | Core systems, types and concepts          | Done                                                                                                 |
| `windows` | Windows implementation (MFC-less)         | Done (MFC addon (`ASYNCRT_WINDOWS_ENABLE_MFC`) and PPL version planned (`ASYNCRT_WINDOWS_USE_PPL`) ) |
| `qt`      | Qt implementation                         | Done (TODO: only enable if Qt is present)                                                            |
| `generic` | Generic implementation using only the STL | Planned (always enabled)                                                                             |
| `boost`   | Boost implementation                      | Planned (only enabled if boost is present)                                                           |

## Features

| Feature                            | Description                                                  | Status  |
|------------------------------------|--------------------------------------------------------------|---------|
| Foundation                         | Foundational implementation for tasks, task contexts, etc... | Done    |
| Coroutine support                  | Supports the asynchronous invokation of C++20 coroutines     | Done    |
| Cancellation                       | Support to cancel tasks in-flight                            | Planned |
| Integration with `std::stop_token` | Support cancellation with `std::stop_token`                  | Planned |
| QML support                        | Extend the `qt` module to be able to integrate with QML      | Planned |

## Examples

Examples can be found in the _examples/_ folder. Currently, there are examples for Qt and MFC.

