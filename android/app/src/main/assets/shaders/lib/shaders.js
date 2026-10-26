// Builds nfsmw_shaders.nfsp from the original shader containers, entirely in the browser (or Node).
// Same steps and checks as the reference build script: translate to HLSL, apply the three source
// rewrites, compile to SPIR-V with DXC, check the shadow marker and pack.

import { t } from './i18n.js';

const utf8 = new TextDecoder('utf-8');
const encoder = new TextEncoder();

const SHADOW_OLD = 'tfetch2D(SHADOWMAP_SAMPLER_';
const SHADOW_NEW = 'tfetch2DSombra(SHADOWMAP_SAMPLER_';
const MIN_OLD =
  'tfetch2DSombra(SHADOWMAP_SAMPLER_Texture2DDescriptorIndex, SHADOWMAP_SAMPLER_SamplerDescriptorIndex,';
const MIN_NEW =
  'tfetch2DSombraMin(SHADOWMAP_SAMPLER_Texture2DDescriptorIndex, SHADOWMAP_SAMPLER_Texture3DDescriptorIndex, ' +
  'SHADOWMAP_SAMPLER_SamplerDescriptorIndex,';
const DEFINE_3D = '#define SHADOWMAP_SAMPLER_Texture3DDescriptorIndex';
const BLUR_OLD = 'r5.xyz = r5.xyz * r0.xxx + r3.xyz;';
const BLUR_NEW =
  'r5.xyz = (g_SpecConstants() & SPEC_CONSTANT_SIN_DESENFOQUE) ? r3.xyz : (r5.xyz * r0.xxx + r3.xyz);';
const SHADOW_MARKER = 0x5e3b1a84;

// ---- OpenGL ES library (adapted from tools/biblioteca_shaders_gles.mjs, from victorgbd/NFSMW-Recompiled-Mobile
// 4554a99). GLES has no descriptor indexing: each sampler register is its own resource (g_Tex2D_<r>,
// g_Tex3D_<r>, g_TexCube_<r> in sets 0-2 and g_Sampler_<r> in set 3, binding = register), and there is no shadow
// minimum (its pair would come through the 3D index word of the same register). The app converts the SPIR-V to
// GLSL ES (app/src/nfsmw_gles_shaders.cpp).
const REGISTERS = 16;
const DIMENSIONS = ['2D', '3D', 'Cube'];
const TYPES = { '2D': 'Texture2D<float4>', '3D': 'Texture3D<float4>', Cube: 'TextureCube<float4>' };
const INDEX_DEFINE =
  /^#define (\w+)_(Texture(2D|3D|Cube)|Sampler)DescriptorIndex \(NFSMW_UBO \? NFSMW_COMPARTIDA_UINT\((\d+)\) : [^\n]*$/gm;
const FORBIDDEN = new Map([[5302, 'RuntimeDescriptorArray'], [11, 'Int64'], [5347, 'PhysicalStorageBufferAddresses']]);

export function glesHeader(original) {
  let h = original.replace(/\r\n/g, '\n');
  const heaps =
    'Texture2D<float4> g_Texture2DDescriptorHeap[] : register(t0, space0);\n' +
    'Texture3D<float4> g_Texture3DDescriptorHeap[] : register(t0, space1);\n' +
    'TextureCube<float4> g_TextureCubeDescriptorHeap[] : register(t0, space2);\n' +
    'SamplerState g_SamplerDescriptorHeap[] : register(s0, space3);\n';
  if (count(h, heaps) !== 1) {
    throw new Error('shader_common.h: the descriptor heap block is not there exactly once');
  }
  const declarations = [];
  DIMENSIONS.forEach((d, set) => {
    for (let r = 0; r < REGISTERS; r++) {
      declarations.push(`${TYPES[d]} g_Tex${d}_${r} : register(t${r}, space${set});`);
    }
  });
  for (let r = 0; r < REGISTERS; r++) {
    declarations.push(`SamplerState g_Sampler_${r} : register(s${r}, space3);`);
  }
  h = h.replace(heaps, '// NFSMW (GLES): one resource per sampler register instead of descriptor heaps.\n' +
    declarations.join('\n') + '\n');
  // The shared block is 488 bytes, but the header declares 23 float4: GLSL rejects a constant index past it.
  const shared = 'struct NfsmwBloqueCompartidas { float4 v[23]; };';
  if (count(h, shared) !== 1) {
    throw new Error('shader_common.h: the shared block declaration is not there exactly once');
  }
  h = h.replace(shared, 'struct NfsmwBloqueCompartidas { float4 v[31]; };');
  const lookups = h.match(/g_(Texture2D|Texture3D|TextureCube|Sampler)DescriptorHeap\[(\w+)\]/g) || [];
  if (lookups.length < 8) {
    throw new Error(`shader_common.h: only ${lookups.length} heap lookups found`);
  }
  h = h.replace(/g_(Texture2D|Texture3D|TextureCube|Sampler)DescriptorHeap\[(\w+)\]/g, '$2');
  let signatures = 0;
  h = h.replace(/^(\w+) (\w+)\(([^)\n]*DescriptorIndex[^)\n]*)\)$/gm, (line, ret, name, params) => {
    const texture = name === 'tfetch3D' ? TYPES['3D'] : name === 'tfetchCube' ? TYPES.Cube : TYPES['2D'];
    signatures++;
    const fixed = params
      .replace(/\buint (resourceDescriptorIndex|parejaDescriptorIndex)\b/g, `${texture} $1`)
      .replace(/\buint samplerDescriptorIndex\b/g, 'SamplerState samplerDescriptorIndex');
    return `${ret} ${name}(${fixed})`;
  });
  if (signatures < 8) {
    throw new Error(`shader_common.h: only ${signatures} helper signatures rewritten`);
  }
  if (/DescriptorHeap|uint \w*DescriptorIndex/.test(h)) {
    throw new Error('shader_common.h: a heap or an index survived the rewrite');
  }
  return '#define NFSMW_GLES 1\n' + h;
}

// `#define NAME_Texture2DDescriptorIndex (NFSMW_UBO ? NFSMW_COMPARTIDA_UINT(<byte>) : ...)`: the byte is
// dimension * 64 + register * 4 (textures) or 192 + register * 4 (sampler), as in shader_recompiler.cpp.
function glesIndexDefines(stem, text) {
  text = text.replace(/\r\n/g, '\n').replace(INDEX_DEFINE, (line, name, kind, dimension, byte) => {
    const first = dimension ? DIMENSIONS.indexOf(dimension) * 64 : 192;
    const register = (Number(byte) - first) / 4;
    if (!Number.isInteger(register) || register < 0 || register >= REGISTERS) {
      throw new Error(`${stem}: unexpected index define: ${line}`);
    }
    return dimension ? `#define ${name}_Texture${dimension}DescriptorIndex g_Tex${dimension}_${register}`
                     : `#define ${name}_SamplerDescriptorIndex g_Sampler_${register}`;
  });
  if (/DescriptorIndex \(NFSMW_UBO/.test(text)) {
    throw new Error(`${stem}: an index define was not rewritten`);
  }
  return text;
}

// The first capability GLES cannot express, or null.
function forbiddenCapability(spirv) {
  const words = new Uint32Array(spirv.buffer, spirv.byteOffset, Math.floor(spirv.length / 4));
  if (words[0] !== 0x07230203) {
    return 'not SPIR-V';
  }
  for (let i = 5; i < words.length; ) {
    const n = words[i] >>> 16;
    if (!n || i + n > words.length) {
      return 'malformed';
    }
    if ((words[i] & 0xffff) === 17 && FORBIDDEN.has(words[i + 1])) {
      return FORBIDDEN.get(words[i + 1]);
    }
    i += n;
  }
  return null;
}

function count(text, needle) {
  let n = 0;
  for (let i = text.indexOf(needle); i >= 0; i = text.indexOf(needle, i + needle.length)) {
    n++;
  }
  return n;
}

function hasShadowMarker(spirv) {
  if (spirv.length < 20) {
    return false;
  }
  const words = new Uint32Array(spirv.buffer, spirv.byteOffset, Math.floor(spirv.length / 4));
  if (words[0] !== 0x07230203) {
    return false;
  }
  for (let i = 5; i < words.length; ) {
    const n = words[i] >>> 16;
    if (!n || i + n > words.length) {
      return false;
    }
    if ((words[i] & 0xffff) === 43 && n === 4 && words[i + 3] === SHADOW_MARKER) {
      return true;
    }
    i += n;
  }
  return false;
}

// containers: [{ name, bytes }]; modules: { hlsl, dxc, pack } already instantiated Emscripten modules;
// shaderCommon: Uint8Array with shader_common.h; log(text) receives progress lines.
// blurShader: stem of the container that gets the composition blur switch (it changes name between editions).
// gles: the OpenGL ES library instead of the Vulkan one (glesHeader above), with fresh module instances.
export async function buildShaderLibrary(containers, modules, shaderCommon, log = () => {}, blurShader = 'p_000139',
                                         gles = false) {
  const { hlsl, dxc, pack } = modules;

  hlsl.FS.mkdirTree('/in');
  for (const c of containers) {
    hlsl.FS.writeFile(`/in/${c.name}`, c.bytes);
  }
  hlsl.FS.writeFile('/shader_common.h', gles ? encoder.encode(glesHeader(utf8.decode(shaderCommon))) : shaderCommon);
  const translated = hlsl.callMain(['/in', '/out', '/shader_common.h']);
  if (translated !== 0 && translated !== undefined) {
    throw new Error(`shader translation failed (${translated})`);
  }
  const sources = new Map();
  for (const c of containers) {
    const stem = c.name.slice(0, -4);
    const text = utf8.decode(hlsl.FS.readFile(`/out/${stem}.hlsl`));
    sources.set(stem, gles ? glesIndexDefines(stem, text) : text);
  }
  log(t('translatedShaders', { count: sources.size }));

  let shadowCalls = 0;
  for (const [stem, text] of sources) {
    const n = count(text, SHADOW_OLD);
    if (n) {
      shadowCalls += n;
      sources.set(stem, text.split(SHADOW_OLD).join(SHADOW_NEW));
    }
  }
  if (!shadowCalls) {
    throw new Error('no shadow map fetch was rewritten');
  }
  let minCalls = 0;
  for (const [stem, text] of gles ? [] : sources) {
    const n = count(text, SHADOW_NEW);
    if (!n) {
      continue;
    }
    if (count(text, MIN_OLD) !== n || !text.includes(DEFINE_3D)) {
      throw new Error(`${stem}: unexpected shadow fetch form`);
    }
    sources.set(stem, text.split(MIN_OLD).join(MIN_NEW));
    minCalls += n;
  }
  if (!minCalls && !gles) {
    throw new Error('no shadow minimum fetch was rewritten');
  }
  if (sources.has(blurShader)) {
    const text = sources.get(blurShader);
    if (count(text, BLUR_OLD) !== 1) {
      throw new Error('the composition blur mix was not found exactly once');
    }
    sources.set(blurShader, text.replace(BLUR_OLD, BLUR_NEW));
  }
  log(t('rewroteShadows', { count: shadowCalls }));

  dxc.FS.mkdirTree('/work');
  pack.FS.mkdirTree('/in');
  pack.FS.mkdirTree('/spirv');
  let done = 0;
  for (const c of containers) {
    const stem = c.name.slice(0, -4);
    const input = `/work/${stem}.hlsl`;
    const output = `/work/${stem}.spv`;
    dxc.FS.writeFile(input, encoder.encode(sources.get(stem)));
    const rc = dxc.ccall('compile', 'number', ['string', 'string', 'number'], [input, output, stem.startsWith('v_') ? 1 : 0]);
    if (rc !== 0) {
      throw new Error(`DXC rejected ${stem} (${rc})`);
    }
    const spirv = dxc.FS.readFile(output);
    const capability = gles ? forbiddenCapability(spirv) : null;
    if (capability) {
      throw new Error(`${stem}: SPIR-V uses ${capability}`);
    }
    if (sources.get(stem).includes('tfetch2DSombraMin(SHADOWMAP_SAMPLER_') && !hasShadowMarker(spirv)) {
      throw new Error(`${stem}: SPIR-V without the shadow minimum marker`);
    }
    pack.FS.writeFile(`/spirv/${stem}.spv`, spirv);
    pack.FS.writeFile(`/in/${c.name}`, c.bytes);
    dxc.FS.unlink(input);
    dxc.FS.unlink(output);
    if (++done % 25 === 0) {
      log(t('compiled', { done, total: containers.length }));
    }
  }
  const packed = pack.callMain(['/in', '/spirv', '/nfsmw_shaders.nfsp']);
  if (packed !== 0 && packed !== undefined) {
    throw new Error(`packing failed (${packed})`);
  }
  return pack.FS.readFile('/nfsmw_shaders.nfsp');
}
