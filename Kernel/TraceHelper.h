#pragma once
#include "TraceResult.h"
#include <Base/CudaRandom.h>
#include <Engine/KernelDynamicScene.h>
#include "Sampler_device.h"

namespace CudaTracerLib {

class ISamplingSequenceGenerator;

extern CUDA_ALIGN(16) CUDA_CONST KernelDynamicScene g_SceneDataDevice;
extern CUDA_ALIGN(16) CUDA_DEVICE unsigned int g_RayTracedCounterDevice;
extern CUDA_ALIGN(16) CUDA_CONST CudaStaticWrapper<SamplerData> g_SamplerDataDevice;

CTL_EXPORT extern CUDA_ALIGN(16) KernelDynamicScene g_SceneDataHost;
CTL_EXPORT extern CUDA_ALIGN(16) unsigned int g_RayTracedCounterHost;
CTL_EXPORT extern CUDA_ALIGN(16) CudaStaticWrapper<SamplerData> g_SamplerDataHost;

#ifdef ISCUDA
#define g_SceneData g_SceneDataDevice
#define g_RayTracedCounter g_RayTracedCounterDevice
#define g_SamplerData (*g_SamplerDataDevice)
#else
#define g_SceneData g_SceneDataHost
#define g_RayTracedCounter g_RayTracedCounterHost
#define g_SamplerData (*g_SamplerDataHost)
#endif

CTL_EXPORT CUDA_DEVICE CUDA_HOST bool traceRay(const Vec3f& dir, const Vec3f& ori, TraceResult* a_Result);

CUDA_FUNC_IN TraceResult traceRay(const Ray& r)
{
	TraceResult r2;
	r2.Init();
	traceRay(r.dir(), r.ori(), &r2);
	return r2;
}

CTL_EXPORT CUDA_DEVICE CUDA_HOST void fillDG(const Vec2f& bary, unsigned int triIdx, unsigned int nodeIdx, DifferentialGeometry& dg);

CTL_EXPORT void InitializeKernel();
CTL_EXPORT void DeinitializeKernel();

CTL_EXPORT void UpdateKernel(DynamicScene* a_Scene, ISamplingSequenceGenerator& sampler);
//uses static SamplingSequenceGenerator
CTL_EXPORT void UpdateKernel(DynamicScene* a_Scene);

CTL_EXPORT void GenerateNewRandomSequences(ISamplingSequenceGenerator& sampler);
//uses static SamplingSequenceGenerator, no guarantees about type
CTL_EXPORT void GenerateNewRandomSequences();

CTL_EXPORT unsigned int k_getNumRaysTraced();
CTL_EXPORT void k_setNumRaysTraced(unsigned int i);

struct traversalRay
{
	Vec4f a;
	Vec4f b;
};

struct CUDA_ALIGN(16) traversalResult
{
	float dist;
	int nodeIdx;
	int triIdx;
	int bCoords;//2 x uint16_t
	CUDA_DEVICE CUDA_HOST void toResult(TraceResult* tR, KernelDynamicScene& g_SceneData) const;
	CUDA_DEVICE CUDA_HOST void fromResult(const TraceResult* tR, KernelDynamicScene& g_SceneData);
};

CTL_EXPORT void __internal__IntersectBuffers(int N, traversalRay* a_RayBuffer, traversalResult* a_ResBuffer, bool SKIP_OUTER, bool ANY_HIT);

}