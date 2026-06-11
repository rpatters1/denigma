# Denigma Converter Libraries

Denigma exposes conversion adapters as small static libraries that can be linked into the CLI or into external clients such as WebAssembly applications.

The public API is centered on `denigma::IConverter`. Callers pass input as a memory buffer and provide a `std::ostream` for output, which lets native clients use file streams while WebAssembly clients can use string streams or custom stream buffers.

Format-specific libraries register their adapters with `denigma::ConverterRegistry`. A client links the common API and only the adapter library it needs, then requests a converter by source and target format.

