/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2023 Baldur Karlsson
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

#pragma once

// PORTABILITY - parts of the code that need to change/update to handle
// portability between different GPU setups/capabilities etc

// MULTIDEVICE - parts of the code that will need to be updated to support
// multiple devices or queues.

#include "common/common.h"

#define XR_NO_PROTOTYPES

#if ENABLED(RDOC_X64)

#define XR_DEFINE_HANDLE(object) typedef struct object##_T *object;

#else

// make handles typed even on 32-bit, by relying on C++
#define XR_DEFINE_HANDLE(obj)                                 \
  struct obj                                                                   \
  {                                                                            \
    obj() : handle(0) {}                                                       \
    obj(uint64_t x) : handle(x) {}                                             \
    bool operator==(const obj &other) const { return handle == other.handle; } \
    bool operator<(const obj &other) const { return handle < other.handle; }   \
    bool operator!=(const obj &other) const { return handle != other.handle; } \
    uint64_t handle;                                                           \
  };
#define XR_WRAPPER_STRUCT

#endif

#include "core/core.h"
#include "core/resource_manager.h"
#include "official/loader_interfaces.h"
#include "official/openxr.h"
#include "serialise/serialiser.h"

#undef Bool
#undef None

// enable this to get verbose debugging about when/where/why partial command buffer replay is
// happening
#define VERBOSE_PARTIAL_REPLAY OPTION_OFF

// UUID shared with VR runtimes to specify which vkImage is currently presented to the screen
#define VR_ThumbnailTag_UUID 0x94F5B9E495BCC552ULL

// ResourceFormat MakeResourceFormat(VkFormat fmt);
// VkFormat MakeVkFormat(ResourceFormat fmt);
// Topology MakePrimitiveTopology(VkPrimitiveTopology Topo, uint32_t patchControlPoints);
// VkPrimitiveTopology MakeVkPrimitiveTopology(Topology Topo);
// AddressMode MakeAddressMode(VkSamplerAddressMode addr);
// void MakeBorderColor(VkBorderColor border, rdcfixedarray<float, 4> &BorderColor);
// CompareFunction MakeCompareFunc(VkCompareOp func);
// FilterMode MakeFilterMode(VkFilter f);
// TextureFilter MakeFilter(VkFilter minFilter, VkFilter magFilter, VkSamplerMipmapMode mipmapMode,
//                          bool anisoEnable, bool compareEnable, VkSamplerReductionMode reduction);
// LogicOperation MakeLogicOp(VkLogicOp op);
// BlendMultiplier MakeBlendMultiplier(VkBlendFactor blend);
// BlendOperation MakeBlendOp(VkBlendOp op);
// StencilOperation MakeStencilOp(VkStencilOp op);
// rdcstr HumanDriverName(VkDriverId driverId);

// void SanitiseOldImageLayout(VkImageLayout &layout);
// void SanitiseNewImageLayout(VkImageLayout &layout);
// void SanitiseReplayImageLayout(VkImageLayout &layout);

// void CombineDepthStencilLayouts(rdcarray<VkImageMemoryBarrier> &barriers);

// void DoPipelineBarrier(VkCommandBuffer cmd, size_t count, const VkImageMemoryBarrier *barriers);
// void DoPipelineBarrier(VkCommandBuffer cmd, size_t count, const VkBufferMemoryBarrier *barriers);
// void DoPipelineBarrier(VkCommandBuffer cmd, size_t count, const VkMemoryBarrier *barriers);

// int SampleCount(VkSampleCountFlagBits countFlag);
// int SampleIndex(VkSampleCountFlagBits countFlag);
// int StageIndex(VkShaderStageFlagBits stageFlag);
// VkShaderStageFlags ShaderMaskFromIndex(size_t index);

// struct PackedWindowHandle
// {
//   PackedWindowHandle(WindowingSystem s, void *h) : system(s), handle(h) {}
//   WindowingSystem system;
//   void *handle;
// };

// struct VkResourceRecord;

class WrappedOpenXR;

struct XrPackedVersion
{
  XrPackedVersion(uint32_t v = 0) : version(v) {}
  uint32_t version;

  bool operator<(uint32_t v) const { return version < v; }
  bool operator>(uint32_t v) const { return version > v; }
  bool operator<=(uint32_t v) const { return version <= v; }
  bool operator>=(uint32_t v) const { return version >= v; }
  bool operator==(uint32_t v) const { return version == v; }
  bool operator!=(uint32_t v) const { return version != v; }
  // int overloads because VK_MAKE_VERSION is type int...
  bool operator<(int v) const { return version < (uint32_t)v; }
  bool operator>(int v) const { return version > (uint32_t)v; }
  bool operator<=(int v) const { return version <= (uint32_t)v; }
  bool operator>=(int v) const { return version >= (uint32_t)v; }
  bool operator==(int v) const { return version == (uint32_t)v; }
  bool operator!=(int v) const { return version != (uint32_t)v; }
  operator uint32_t() const { return version; }
  XrPackedVersion &operator=(uint32_t v)
  {
    version = v;
    return *this;
  }
};

DECLARE_REFLECTION_STRUCT(XrPackedVersion);

// in vk_<platform>.cpp
extern void *LoadOpenXRLibrary();

// void GetPhysicalDeviceDriverProperties(VkInstDispatchTable *instDispatchTable,
//                                        VkPhysicalDevice unwrappedPhysicalDevice,
//                                        VkPhysicalDeviceDriverProperties &driverProps);

enum
{
  VkCheckLayer_unique_objects,
  VkCheckLayer_Max,
};

DECLARE_REFLECTION_STRUCT(XrBaseInStructure);

// we cast to this type when serialising as a placeholder indicating that
// the given flags field doesn't have any bits defined
enum XrFlagWithNoBits
{
  FlagWithNoBits_Dummy_Bit = 1,
};

size_t GetNextPatchSize(const void *next);
void UnwrnChain(CaptureState state, const char *structName, byte *&tempMem,
                     XrBaseInStructure *infoStruct);
void CopyNextChainForPatching(const char *structName, byte *&tempMem, XrBaseInStructure *infoStruct);

template <typename XrStruct>
XrStruct *UnwrapStructAndChain(CaptureState state, byte *&tempMem, const XrStruct *base)
{
  XrBaseInStructure dummy;
  dummy.next = (const XrBaseInStructure *)base;

  UnwrapNextChain(state, TypeName<XrStruct>().c_str(), tempMem, &dummy);

  return (XrStruct *)dummy.next;
}

template <typename XrStruct>
void AppendNextStruct(XrStruct &base, void *newStruct)
{
  XrBaseOutStructure *next = (XrBaseOutStructure *)&base;

  while(next->next)
    next = next->next;

  next->next = (XrBaseOutStructure *)newStruct;
}

template <typename XrStruct>
const XrBaseInStructure *FindNextStruct(const XrStruct *haystack, XrStructureType needle)
{
  if(!haystack)
    return NULL;

  const XrBaseInStructure *next = (const XrBaseInStructure *)haystack->next;
  while(next)
  {
    if(next->type == needle)
      return next;

    next = next->next;
  }

  return NULL;
}

template <typename XrStruct>
XrBaseInStructure *FindNextStruct(XrStruct *haystack, XrStructureType needle)
{
  if(!haystack)
    return NULL;

  XrBaseInStructure *next = (XrBaseInStructure *)haystack->next;
  while(next)
  {
    if(next->type == needle)
      return next;

    // assume non-const next in the original struct
    next = (XrBaseInStructure *)next->next;
  }

  return NULL;
}

template <typename XrStruct>
bool RemoveNextStruct(XrStruct *haystack, XrStructureType needle)
{
  bool ret = false;

  // start from the haystack, and iterate
  XrBaseInStructure *root = (XrBaseInStructure *)haystack;
  while(root && root->next)
  {
    // at each point, if the *next* struct is the needle, then point our next pointer at whatever
    // its was - either the next in the chain or NULL if it was at the end. Then we can return true
    // because we removed the struct. We keep going to handle duplicates, but we continue and skip
    // the list iterate as we now have a new root->next.
    // Note that this can't remove the first struct in the chain but that's expected, we only want
    // to remove extension structs.
    if(root->next->type == needle)
    {
      root->next = root->next->next;
      ret = true;
      continue;
    }

    // move to the next struct
    root = (XrBaseInStructure *)root->next;
  }

  return ret;
}


// this is special - these serialise overloads will fetch the ID during capture, serialise the ID
// directly as-if it were the original type, then on replay load up the resource if available.
// Really this is only one type of serialisation, but we declare a couple of overloads to account
// for resources being accessed through different interfaces in different functions
#define SERIALISE_XR_HANDLES() \
  SERIALISE_HANDLE(XrInstance) \
  SERIALISE_HANDLE(XrSession)  \
  SERIALISE_HANDLE(XrSwapchain)

#define SERIALISE_HANDLE(type) DECLARE_REFLECTION_STRUCT(type)

SERIALISE_XR_HANDLES();

// enums

DECLARE_REFLECTION_ENUM(XrObjectType);
DECLARE_REFLECTION_ENUM(XrResult);
DECLARE_REFLECTION_ENUM(XrStructureType);
