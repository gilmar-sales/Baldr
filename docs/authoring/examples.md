# Examples

The [`examples/`](https://github.com/gilmar-sales/Baldr/tree/main/examples) directory contains small, runnable programs that demonstrate individual features of Baldr. Each example is a self-contained CMake target built when Baldr is the top-level project.

## Browse by topic

<div class="grid cards" markdown>

-   :material-rocket-launch: **Hello World**

    The smallest possible Baldr program — one route, automatic JSON.

    [:octicons-arrow-right-24: Hello World](examples/hello-world.md)

-   :material-graph-outline: **Hello Service**

    Register a service and inject it into a route handler.

    [:octicons-arrow-right-24: Hello Service](examples/hello-service.md)

-   :material-devices: **Devices**

    Return a list of `Device` records.

    [:octicons-arrow-right-24: Devices](examples/devices.md)

-   :material-weather-partly-cloudy: **Weather forecast**

    Generate a random forecast and return it as JSON.

    [:octicons-arrow-right-24: Weather forecast](examples/weather-forecast.md)

-   :material-folder-open: **Static files**

    Serve a directory tree under a URL prefix using `MapStaticFiles`.

    [:octicons-arrow-right-24: Static files](examples/static-files.md)

-   :material-file-upload: **File stream**

    Stream a file download and accept an upload, using `FileStreamResult` and `Results::Json`.

    [:octicons-arrow-right-24: File stream](examples/file-stream.md)

-   :material-api: **OpenAPI example**

    Generate a 3.0.3 spec from route metadata with `BaldrOpenApiExtension`.

    [:octicons-arrow-right-24: OpenAPI example](examples/open-api.md)

-   :material-checkbox-marked-outline: **Todo**

    A small CRUD service: DI-registered repository, controller with grouped routes, `FromParams` / `FromBody` binding, and OpenAPI.

    [:octicons-arrow-right-24: Todo](examples/todo.md)

</div>

## Building the examples

When Baldr is the top-level project, examples are built by default:

```bash
cmake -S . -B build
cmake --build build
```

Each example produces an executable named after its directory: `./build/HelloWorld`, `./build/HelloService`, `./build/Devices`, `./build/WeatherForecast`, `./build/StaticFiles`, `./build/FileStream`, `./build/OpenApiExample`, `./build/Todo`.

To disable examples (for example in a production build), set `-DBALDR_BUILD_EXAMPLES=OFF` at configure time.