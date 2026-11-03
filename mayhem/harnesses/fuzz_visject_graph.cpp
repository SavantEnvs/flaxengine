// Visject graph deserializer (shared by material, particle, animation and visual-script graphs).
#include "Engine/Visject/Graph.h"
#include "Engine/Serialization/MemoryReadStream.h"
#include <stdint.h>
#include <stddef.h>

typedef Graph<GraphNode<GraphBox>, GraphBox, GraphParameter> FuzzGraph;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    // Graph::Load reads a 4-byte magic and a 4-byte version.
    if (size < 8)
        return 0;
    MemoryReadStream stream(data, (uint32)size);
    FuzzGraph graph;
    if (graph.Load(&stream, true))
        return 0;
    graph.Clear();
    return 0;
}
