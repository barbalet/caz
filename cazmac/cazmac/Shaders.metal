#include <metal_stdlib>
using namespace metal;

struct CatVertex {
    float2 position;
    float4 color;
};

struct VertexOut {
    float4 position [[position]];
    float4 color;
};

vertex VertexOut vertex_main(uint vertexID [[vertex_id]],
                             const device CatVertex *vertices [[buffer(0)]])
{
    VertexOut out;
    out.position = float4(vertices[vertexID].position, 0.0, 1.0);
    out.color = vertices[vertexID].color;
    return out;
}

fragment float4 fragment_main(VertexOut in [[stage_in]])
{
    return in.color;
}
