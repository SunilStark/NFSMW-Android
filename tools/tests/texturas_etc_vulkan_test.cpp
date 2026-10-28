// On-device check of nfsmw_nativo_bc_a_etc2: does the GPU decode the transcoded blocks as nfsmw_texturas_etc.h's
// decoder does? Random BC blocks of each format are transcoded (every ETC2 mode appears), uploaded as ETC2
// RGB8A1, ETC2 RGBA8, EAC R11 and EAC RG11 images, blitted texel for texel (nearest, same size) to RGBA8 or
// float images and read back. 8-bit channels must match exactly; R11 within 0.01 of 2047ths. No game data.
// Build with the NDK and run over adb (PowerShell; $ndk = the NDK's folder):
//   & "$ndk\toolchains\llvm\prebuilt\windows-x86_64\bin\clang++.exe" --target=aarch64-linux-android29 -std=c++20
//       -O2 tools/tests/texturas_etc_vulkan_test.cpp -lvulkan -static-libstdc++ -o out/texturas_etc_vulkan_test
//   adb push out/texturas_etc_vulkan_test /data/local/tmp/
//   adb shell /data/local/tmp/texturas_etc_vulkan_test
#include "../../app/src/nfsmw_texturas_etc.h"

#include <vulkan/vulkan.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <vector>

namespace etc = nfsmw::etc;
using nfsmw::bc::Formato;

static void Check(bool ok, const char* step) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", step);
    std::exit(1);
  }
}
static void Ok(VkResult result, const char* step) {
  if (result != VK_SUCCESS) {
    std::fprintf(stderr, "FAIL: %s (%d)\n", step, int(result));
    std::exit(1);
  }
}

int main() {
  VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
  app.pApplicationName = "NFSMW ETC2 transcoding test";
  app.apiVersion = VK_API_VERSION_1_0;
  VkInstanceCreateInfo ici{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
  ici.pApplicationInfo = &app;
  VkInstance instance;
  Ok(vkCreateInstance(&ici, nullptr, &instance), "create instance");
  uint32_t count = 0;
  Ok(vkEnumeratePhysicalDevices(instance, &count, nullptr), "enumerate GPUs");
  Check(count != 0, "GPU present");
  std::vector<VkPhysicalDevice> gpus(count);
  Ok(vkEnumeratePhysicalDevices(instance, &count, gpus.data()), "get GPUs");
  VkPhysicalDevice gpu = gpus[0];
  VkPhysicalDeviceProperties properties{};
  vkGetPhysicalDeviceProperties(gpu, &properties);
  VkPhysicalDeviceFeatures supported{};
  vkGetPhysicalDeviceFeatures(gpu, &supported);
  std::printf("GPU: %s, textureCompressionETC2 %s\n", properties.deviceName,
              supported.textureCompressionETC2 ? "yes" : "no");
  Check(supported.textureCompressionETC2, "the GPU samples ETC2");
  vkGetPhysicalDeviceQueueFamilyProperties(gpu, &count, nullptr);
  std::vector<VkQueueFamilyProperties> families(count);
  vkGetPhysicalDeviceQueueFamilyProperties(gpu, &count, families.data());
  uint32_t family = 0;
  while (family < count && !(families[family].queueFlags & VK_QUEUE_GRAPHICS_BIT)) ++family;
  Check(family < count, "graphics queue");
  float priority = 1;
  VkDeviceQueueCreateInfo qci{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
  qci.queueFamilyIndex = family;
  qci.queueCount = 1;
  qci.pQueuePriorities = &priority;
  VkPhysicalDeviceFeatures enabled{};
  enabled.textureCompressionETC2 = VK_TRUE;
  VkDeviceCreateInfo dci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
  dci.queueCreateInfoCount = 1;
  dci.pQueueCreateInfos = &qci;
  dci.pEnabledFeatures = &enabled;
  VkDevice device;
  Ok(vkCreateDevice(gpu, &dci, nullptr, &device), "create device");
  VkQueue queue;
  vkGetDeviceQueue(device, family, 0, &queue);
  VkPhysicalDeviceMemoryProperties mp{};
  vkGetPhysicalDeviceMemoryProperties(gpu, &mp);
  const auto allocate = [&](VkMemoryRequirements req, VkMemoryPropertyFlags flags) {
    uint32_t type = 0;
    while (type < mp.memoryTypeCount &&
           (!(req.memoryTypeBits & (1u << type)) || (mp.memoryTypes[type].propertyFlags & flags) != flags))
      ++type;
    Check(type < mp.memoryTypeCount, "memory type");
    VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    ai.allocationSize = req.size;
    ai.memoryTypeIndex = type;
    VkDeviceMemory memory;
    Ok(vkAllocateMemory(device, &ai, nullptr, &memory), "allocate memory");
    return memory;
  };
  // 64x64 texels: 256 blocks of up to 16 bytes in, 64x64 x 8 bytes (RG32F) out.
  constexpr uint32_t kLado = 64, kBloques = (kLado / 4) * (kLado / 4), kSalida = 65536;
  VkBufferCreateInfo bci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
  bci.size = kSalida * 2;
  bci.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  VkBuffer buffer;
  Ok(vkCreateBuffer(device, &bci, nullptr, &buffer), "create staging buffer");
  VkMemoryRequirements req{};
  vkGetBufferMemoryRequirements(device, buffer, &req);
  VkDeviceMemory bm = allocate(req, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  Ok(vkBindBufferMemory(device, buffer, bm, 0), "bind buffer");
  void* mapped;
  Ok(vkMapMemory(device, bm, 0, VK_WHOLE_SIZE, 0, &mapped), "map staging buffer");
  auto bytes = static_cast<uint8_t*>(mapped);
  VkCommandPoolCreateInfo pci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
  pci.queueFamilyIndex = family;
  pci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  VkCommandPool pool;
  Ok(vkCreateCommandPool(device, &pci, nullptr, &pool), "create command pool");
  VkCommandBufferAllocateInfo cai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
  cai.commandPool = pool;
  cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  cai.commandBufferCount = 1;
  VkCommandBuffer cmd;
  Ok(vkAllocateCommandBuffers(device, &cai, &cmd), "allocate command buffer");
  VkFenceCreateInfo fci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
  VkFence fence;
  Ok(vkCreateFence(device, &fci, nullptr, &fence), "create fence");

  struct Caso {
    Formato bc;
    VkFormat etc, destino;
    const char* nombre;
  };
  const Caso casos[] = {
      {Formato::BC1, VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK, VK_FORMAT_R8G8B8A8_UNORM, "BC1 -> ETC2 RGB8A1"},
      {Formato::BC3, VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK, VK_FORMAT_R8G8B8A8_UNORM, "BC3 -> ETC2 RGBA8"},
      {Formato::BC4, VK_FORMAT_EAC_R11_UNORM_BLOCK, VK_FORMAT_R32_SFLOAT, "BC4 -> EAC R11"},
      {Formato::BC5, VK_FORMAT_EAC_R11G11_UNORM_BLOCK, VK_FORMAT_R32G32_SFLOAT, "BC5 -> EAC RG11"},
  };
  std::mt19937 azar(2024);
  for (const Caso& caso : casos) {
    const uint32_t tam = nfsmw::bc::BytesBloque(caso.bc);
    std::vector<uint8_t> datos(kBloques * tam);
    for (uint32_t b = 0; b < kBloques; ++b) {
      uint8_t* p = datos.data() + b * tam;
      for (uint32_t i = 0; i < tam; ++i) p[i] = uint8_t(azar());
      if (b % 4 == 1) p[tam - 6] = p[tam - 8] ^ uint8_t(azar() & 3), p[tam - 5] = p[tam - 7];
      if (b % 4 == 2) for (uint32_t i = 0; i < 4; ++i) p[tam - 4 + i] &= 0xAA;
    }
    Check(etc::Transcodificar(caso.bc, datos, datos), "transcode");
    std::memcpy(bytes, datos.data(), datos.size());
    std::memset(bytes + kSalida, 0xCD, kSalida);
    VkFormatProperties fp{};
    vkGetPhysicalDeviceFormatProperties(gpu, caso.etc, &fp);
    Check(fp.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT, "blit from the ETC2 format");
    VkImageCreateInfo ci{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    ci.imageType = VK_IMAGE_TYPE_2D;
    ci.extent = {kLado, kLado, 1};
    ci.mipLevels = 1;
    ci.arrayLayers = 1;
    ci.samples = VK_SAMPLE_COUNT_1_BIT;
    ci.tiling = VK_IMAGE_TILING_OPTIMAL;
    VkImage imagenes[2];
    VkDeviceMemory memorias[2];
    for (int i = 0; i < 2; ++i) {
      ci.format = i ? caso.destino : caso.etc;
      ci.usage = i ? VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
                   : VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
      Ok(vkCreateImage(device, &ci, nullptr, &imagenes[i]), "create image");
      vkGetImageMemoryRequirements(device, imagenes[i], &req);
      memorias[i] = allocate(req, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
      Ok(vkBindImageMemory(device, imagenes[i], memorias[i], 0), "bind image");
    }
    Ok(vkResetCommandBuffer(cmd, 0), "reset command buffer");
    VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    Ok(vkBeginCommandBuffer(cmd, &begin), "begin commands");
    VkImageMemoryBarrier barrera{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrera.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrera.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrera.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrera.srcQueueFamilyIndex = barrera.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrera.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    for (VkImage imagen : imagenes) {
      barrera.image = imagen;
      vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                           nullptr, 1, &barrera);
    }
    VkBufferImageCopy copia{};
    copia.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    copia.imageExtent = {kLado, kLado, 1};
    vkCmdCopyBufferToImage(cmd, buffer, imagenes[0], VK_IMAGE_LAYOUT_GENERAL, 1, &copia);
    VkMemoryBarrier entre{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    entre.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    entre.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1, &entre, 0, nullptr,
                         0, nullptr);
    VkImageBlit blit{};
    blit.srcSubresource = blit.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    blit.srcOffsets[1] = blit.dstOffsets[1] = {int32_t(kLado), int32_t(kLado), 1};
    vkCmdBlitImage(cmd, imagenes[0], VK_IMAGE_LAYOUT_GENERAL, imagenes[1], VK_IMAGE_LAYOUT_GENERAL, 1, &blit,
                   VK_FILTER_NEAREST);
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1, &entre, 0, nullptr,
                         0, nullptr);
    copia.bufferOffset = kSalida;
    vkCmdCopyImageToBuffer(cmd, imagenes[1], VK_IMAGE_LAYOUT_GENERAL, buffer, 1, &copia);
    VkMemoryBarrier al_host{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    al_host.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    al_host.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &al_host, 0, nullptr,
                         0, nullptr);
    Ok(vkEndCommandBuffer(cmd), "end commands");
    Ok(vkResetFences(device, 1, &fence), "reset fence");
    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;
    Ok(vkQueueSubmit(queue, 1, &submit, fence), "submit");
    Ok(vkWaitForFences(device, 1, &fence, VK_TRUE, 10000000000ull), "wait for GPU");

    // Our decoder, block by block, against what the GPU wrote.
    uint32_t distintos = 0;
    for (uint32_t b = 0; b < kBloques; ++b) {
      const uint32_t bx = b % (kLado / 4), by = b / (kLado / 4);
      const uint8_t* bloque = datos.data() + b * tam;
      uint8_t rgba[64]{};
      int r11[2][16]{};
      if (caso.bc == Formato::BC1) etc::DecodificarColor(bloque, true, rgba);
      if (caso.bc == Formato::BC3) etc::Decodificar(Formato::BC3, bloque, rgba);
      if (caso.bc == Formato::BC4 || caso.bc == Formato::BC5) etc::DecodificarEac(bloque, true, r11[0]);
      if (caso.bc == Formato::BC5) etc::DecodificarEac(bloque + 8, true, r11[1]);
      for (uint32_t t = 0; t < 16; ++t) {
        const uint32_t x = bx * 4 + t % 4, y = by * 4 + t / 4;
        bool igual = true;
        if (caso.destino == VK_FORMAT_R8G8B8A8_UNORM) {
          const uint8_t* gpu_texel = bytes + kSalida + (y * kLado + x) * 4;
          igual = !std::memcmp(gpu_texel, rgba + t * 4, 4);
        } else {
          const uint32_t canales = caso.destino == VK_FORMAT_R32_SFLOAT ? 1 : 2;
          for (uint32_t c = 0; c < canales; ++c) {
            float v;
            std::memcpy(&v, bytes + kSalida + ((y * kLado + x) * canales + c) * 4, 4);
            igual = igual && std::fabs(v * 2047.0f - float(r11[c][t])) < 0.01f;
          }
        }
        if (!igual && distintos++ < 8) std::printf("  %s: texel (%u, %u) differs\n", caso.nombre, x, y);
      }
    }
    std::printf("%s: %s (%u of %u texels differ)\n", caso.nombre, distintos ? "FAIL" : "PASS", distintos,
                kLado * kLado);
    Check(!distintos, "the GPU decodes the transcoded blocks as our decoder does");
    for (int i = 0; i < 2; ++i) {
      vkDestroyImage(device, imagenes[i], nullptr);
      vkFreeMemory(device, memorias[i], nullptr);
    }
  }
  vkDestroyFence(device, fence, nullptr);
  vkDestroyCommandPool(device, pool, nullptr);
  vkUnmapMemory(device, bm);
  vkDestroyBuffer(device, buffer, nullptr);
  vkFreeMemory(device, bm, nullptr);
  vkDestroyDevice(device, nullptr);
  vkDestroyInstance(instance, nullptr);
  std::puts("PASS: ETC2/EAC transcoding decoded by this GPU as by nfsmw_texturas_etc.h");
}
