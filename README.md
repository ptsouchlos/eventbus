<p align="center">
  
<img src="https://socialify.git.ci/DeveloperPaul123/eventbus/image?font=Bitter&forks=1&language=1&logo=https%3A%2F%2Fraw.githubusercontent.com%2FDeveloperPaul123%2Feventbus%2Fdevelop%2Fart%2Fexport%2Fbus_icon.svg&pattern=Circuit%20Board&stargazers=1&theme=Dark" alt="eventbus" width="640" height="320" />
  
  <br>
  <br>
  
  <a href="https://www.apache.org/licenses/LICENSE-2.0.html">
    <img src="https://img.shields.io/badge/license-Apache 2.0-blue" alt="License Apache 2.0">
  </a>
  
  <a href="https://github.com/DeveloperPaul123/eventbus/stargazers">
    <img src="https://img.shields.io/badge/Say%20Thanks-👍-1EAEDB.svg" alt="Say thanks">
  </a>
  
  <a href="https://discord.gg/CX2ybByRnt">
    <img alt="Discord" src="https://img.shields.io/discord/652515194572111872">
  </a>
  
  <a href="https://github.com/DeveloperPaul123/eventbus/actions">
    <img alt="Windows" src="https://github.com/DeveloperPaul123/eventbus/workflows/Windows/badge.svg">
  </a>
  <a href="https://github.com/DeveloperPaul123/eventbus/actions">
    <img alt="Ubuntu" src="https://github.com/DeveloperPaul123/eventbus/workflows/Ubuntu/badge.svg">
  </a>
</p>

## Overview

`eventbus` is a simple, header only C++17 event bus library that doesn't require you to inherit from any sort of `event` class. The library implements the "Mediator" pattern. This pattern is useful when you want components to communicate to each other without necessarily "knowing" about each other. Effectively, this is a thread safe event dispatcher with a list of callbacks.

## Features

- **Does not require event object inheritance** A base `Event` class is not requied for use with `dp::event_bus`. Any class/struct can be used as an event object.
- **Flexible Callback Types** `eventbus` supports a variety different types of callbacks including:
  - Lambdas
  - Class member functions
  - Free functions
- **Flexible Callbacks** No parameter callbacks are also supported as well as taking the event type by value or by `const &`.
- **RAII de-registrations** The handler registration objects automatically de-register the handler upon destruction.
- **Thread safety** Multiple threads can fire events at once to the same `event_bus`. Handlers can also be registered from different threads.
  - **Note:** While the library can handle events fired from different threads note that the thread that fires the event is also the thread that the callback will run on. This library does not ensure that the callback is run on the thread it was registered on. This may or may not be the desired behavior especially in the context of something like thread pools.

## Usage

The basic premise of the `event_bus` is that with it, you can:
* Register handlers
* Fire events that call the corresponding handlers

### Define An Event Object

The "event" object can be any class or structure you want.

### Registering Handlers

#### Free function

````cpp
void event_callback(event_type evt)
{
    // event callback logic...
}

dp::event_bus evt_bus;
const auto registration_handler = evt_bus.register_handler<event_type>(&event_callback)
````

#### Lambda

````cpp
dp::event_bus evt_bus;
const auto registration_handler = evt_bus.register_handler<event_type>([](const event_type& evt)
{
    // logic code...
});
````

#### Class Member Function

````cpp
class event_handler
{
    public:
        void on_event(event_type type)
        {
            // handler logic...
        }
};

// other code
dp::event_bus evt_bus;
event_handler handler;
const auto registration_handler = evt_bus.register_handler<event_type>(&handler, &event_handler::on_event);
````

**Note:** You can't mix a class instance of type `T` with the member function of another class (i.e. `&U::function_name`).

#### Firing Events

````cpp
event_type evt
{
    // data and info..
};
dp::event_bus evt_bus;
evt_bus.fire_event(evt); // all connect handler for the given event type will be fired.
````

A complete example can be seen in the [demo](https://github.com/DeveloperPaul123/eventbus/tree/develop/demo) project.

## Integration

`eventbus` is a header only library. All the files you need are in the `eventbus/include` folder. To use the library just include `eventbus/event_bus.hpp`.

### CMake

`eventbus` defines three CMake `INTERFACE` targets that can be used in your project:
* `eventbus`
* `eventbus::eventbus`
* `dp::eventbus`

````cmake
find_package(dp::eventbus REQUIRED)
````

Alternatively, you can use something like [CPM](https://github.com/TheLartians/CPM) which is based on CMake's `Fetch_Content` module.

````cmake
CPMAddPackage(
    NAME eventbus
    GITHUB_REPOSITORY DeveloperPaul123/eventbus
    GIT_TAG #053902d63de5529ee65d965f8b1fb0851eceed24 change this to latest commit/release tag
)
````

### vcpkg

:construction: This library will be on `vcpkg` soon. :construction:

## Limitations

In general, all callback functions **must** return `void`. Currently, `eventbus` only supports single argument functions as callbacks.

The following use cases are not supported:

- Registering a callback inside an event callback.
- De-registering a callback inside an event callback.

## Contributing

If you find an issue with this library please file an [issue](https://github.com/DeveloperPaul123/eventbus/issues). Pull requests are also welcome! Please see the [contribution guidelines](CONTRIBUTING.md) for more information.

## License

The project is licensed under the Apache License Version 2.0. See [LICENSE](LICENSE) for more details.

## Author

| [<img src="https://avatars0.githubusercontent.com/u/6591180?s=460&v=4" width="100"><br><sub>@DeveloperPaul123</sub>](https://github.com/DeveloperPaul123) |
|:----:|

## Contributors

None yet, be the first!
