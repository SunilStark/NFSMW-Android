// Fixture of tools/tests/spirv_precision_test.cpp: pixel shaders shaped like the library's world shaders. A diffuse
// texture, a normal map with specular, and the shadow map through tfetch2DSombraMin (the marker branch, the optional
// minimum of two maps) compared with the pixel's depth. Four variants, compiled with glslang into
// spirv_precision_casos.h by the command at the top of that file:
//   A  [branch] marker test, pow           C  marker test as !=, the sampling inside the if
//   B  [flatten] marker test (an OpSelect)  D  pow as exp2(log2(x) * n), as the Xenos translator writes it
[[vk::constant_id(0)]] const uint g_SpecConstants = 0;
Texture2D<float4> diffuse : register(t0);
Texture2D<float4> normales : register(t1);
Texture2D<float4> sombraA : register(t2);
Texture2D<float4> sombraB : register(t3);
SamplerState muestreador : register(s0);
cbuffer Constantes : register(b0) { float4 luz; float4 potencia; };

float4 tfetch2DSombraMin(float2 uv, float2 desplazamiento) {
#if defined(VARIANTE_C)
  float4 valor = float4(0.0, 0.0, 0.0, 0.0);
  [branch] if (g_SpecConstants != 0x5E3B1A84u) {
    valor = sombraA.Sample(muestreador, uv + desplazamiento);
    [branch] if ((g_SpecConstants & (1u << 23)) != 0) valor = min(valor, sombraB.Sample(muestreador, uv + desplazamiento));
  }
  return valor;
#else
#if defined(VARIANTE_B)
  [flatten]
#else
  [branch]
#endif
  if (g_SpecConstants == 0x5E3B1A84u) return float4(0.0, 0.0, 0.0, 0.0);
  float4 valor = sombraA.Sample(muestreador, uv + desplazamiento);
  [branch] if ((g_SpecConstants & (1u << 23)) != 0) valor = min(valor, sombraB.Sample(muestreador, uv + desplazamiento));
  return valor;
#endif
}

float4 main(float4 uv : TEXCOORD0, float4 posicion_luz : TEXCOORD1) : SV_Target {
  float4 color = diffuse.Sample(muestreador, uv.xy);
  float3 n = normales.Sample(muestreador, uv.xy).xyz * 2.0 - 1.0;
  float ndl = saturate(dot(n, luz.xyz));
#if defined(VARIANTE_D)
  float especular = exp2(log2(ndl) * potencia.x);
#else
  float especular = pow(ndl, potencia.x);
#endif
  float sombra = 0.0;
  sombra += tfetch2DSombraMin(posicion_luz.xy, float2(-0.5, -0.5)).x >= posicion_luz.z ? 0.5 : 0.0;
  sombra += tfetch2DSombraMin(posicion_luz.xy, float2(0.5, 0.5)).x >= posicion_luz.z ? 0.5 : 0.0;
  return float4(color.rgb * ndl * sombra + especular, color.a);
}
