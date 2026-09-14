#pragma once

#include <cstddef>
#include <optional>

#include <nlohmann/json.hpp>

#include "Pine/Core/Span/Span.hpp"

namespace Pine::Serialization::Dump
{
    // Describes any Pine binary buffer as JSON without knowing which Serializer subclass wrote it.
    //
    // Flexible mode stores a type and a name next to every field, so the buffer describes itself.
    // More usefully, every nested payload - an asset's data, a blueprint, a component's fields - is
    // written as a *complete* blob with its own header, so nested data is detected and recursed
    // into. That is what lets a whole level come back as one JSON tree with no per-type code.
    //
    // Two things cannot be described any further, both by design:
    //   - A DataType::Data field that is not a nested blob is opaque: mesh buffers, compressed
    //     pixels. Reported as { "__type": "data", "__size": N }.
    //   - DataArrayFixed writes itself as DataType::Data and its element stride never reaches the
    //     file, so it is indistinguishable from the above.
    //
    // Returns nullopt when the buffer is not a flexible-mode Pine blob, or is malformed. Never
    // throws on bad input - this is a debugging aid and has to survive whatever it is pointed at.
    std::optional<nlohmann::json> ToJson(const void* data, std::size_t size);

    std::optional<nlohmann::json> ToJson(const ByteSpan& span);
}
