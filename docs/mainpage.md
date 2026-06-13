# Denigma Converter Libraries

Denigma exposes conversion adapters as small static libraries that can be linked into the CLI or into external clients such as WebAssembly applications.

The public API is centered on converter adapters that accept input as memory buffers or random-access readers and write output to caller-provided streams or callbacks. Native clients can use file streams, while WebAssembly clients can use string streams, custom stream buffers, or callback-owned byte buffers.

Clients that know the converter type at compile time can instantiate concrete adapters such as `denigma::formats::mnx::MusxToMnxJsonConverter` and pass that format's typed `Options` struct directly. This gives compile-time checking that MNX options are not accidentally passed to an SVG or MSS converter.

Clients that need runtime lookup can use `denigma::ConverterRegistry`. Format-specific libraries register their adapters with the registry, and callers request a converter by source and target format. Registry calls use `denigma::ConversionRequest`, which type-erases a pointer to the adapter-specific options object for the duration of the conversion call.

MUSX input can be supplied through `denigma::IRandomAccessReader`, allowing native clients to use files and WebAssembly clients to provide a memory-backed or host-backed random-access source without requiring the converter API to own filesystem I/O.
