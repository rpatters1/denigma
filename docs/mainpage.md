# Denigma Converter Libraries

Denigma exposes conversion adapters as small static libraries that are linked into the `denigma` CLI program. They may also be linked into external clients such as WebAssembly applications or third party importers of `musx` files.

The public API accepts input as memory buffers or random-access readers and writes output to caller-provided streams or callbacks. Native clients can use file streams, while WebAssembly clients can use string streams, custom stream buffers, or callback-owned byte buffers.

Clients that know the converter type at compile time can instantiate concrete adapters such as `denigma::formats::mnx::MusxToMnxJsonConverter` and pass that format's typed `Options` struct directly. This gives compile-time checking that, for example, MNX options are not accidentally passed to an SVG or MSS converter.

The snippets below show only the Denigma headers needed for each example. Standard library includes are omitted for brevity.

### File-backed example

```cpp
#include "denigma/formats/mnx.h"
#include "denigma/io/random_access_reader.h"

denigma::formats::mnx::MusxToMnxJsonConverter converter;
denigma::formats::mnx::Options options{};
options.common.sourceName = "score.musx";
denigma::FileRandomAccessReader input{"score.musx"};
std::ofstream output{"score.mnx.json"};
converter.convert(input, output, options);
```

### In-memory example

```cpp
#include "denigma/formats/mnx.h"

denigma::formats::mnx::EnigmaXmlToMnxJsonConverter converter;
denigma::formats::mnx::Options options{};
std::vector<std::byte> inputBytes;
std::ostringstream output;
converter.convert(std::span<const std::byte>(inputBytes.data(), inputBytes.size()), output, options);
```

Clients that need runtime lookup can use `denigma::ConverterRegistry`. Format-specific libraries register their adapters with the registry, and callers request a converter by source and target format. Registry calls use `denigma::ConversionRequest`, which type-erases a pointer to the adapter-specific options object for the duration of the conversion call. The CLI program itself uses the converter registry.

MUSX input can be supplied through `denigma::IRandomAccessReader`, allowing native clients to use files and WebAssembly clients to provide a memory-backed or host-backed random-access source without requiring the converter API to need explicit filesystem I/O.

**Denigma and its libraries are not affiliated with or endorsed by Finale or its parent company.**

- Denigma is an independent open-source project designed to help users access and convert their own data in the absence of Finale, which has been discontinued.
- It does not contain any Finale source code.
- It is not capable of creating or modifying EnigmaXml files.
- It has been separately developed by analyzing the contents of EnigmaXml files and other publicly available resources, such as the [PDK Framework](https://pdk.finalelua.com/) for Finale and Jari Williamsson’s [original site](https://www.finaletips.nu/frameworkref/index.html). (The original site is now only available in the internet archive.)
- Nothing in the repository circumvents digital copy protection on the Finale application.

This reference web-site is generated directly from the source code by the Doxygen application.

**Author**
Robert Patterson
