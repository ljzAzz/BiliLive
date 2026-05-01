struct VertexInPut {
    float2 pos: POSITION;
    float2 uv: TEXCOORD;
};

struct VertexOutPut {
    float4 pos: SV_POSITION;
    float2 uv: TEXCOORD;
};

VertexOutPut main(VertexInPut input)
{
    VertexOutPut output;
    output.pos = float4(input.pos, 0.f, 1.f);
    output.uv = input.uv;
	return output;
}