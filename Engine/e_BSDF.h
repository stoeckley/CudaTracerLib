#pragma once

//this architecture and the implementations are completly copied from mitsuba!

#include <MathTypes.h>
#include "Engine\e_KernelTexture.h"
#include "Engine/e_Samples.h"
#include "Engine\e_PhaseFunction.h"
#include "e_MicrofacetDistribution.h"
#include "e_RoughTransmittance.h"

struct BSDF
{
	unsigned int m_combinedType;
	CUDA_FUNC_IN unsigned int getType()
	{
		return m_combinedType;
	}
	CUDA_FUNC_IN bool hasComponent(unsigned int type) const {
		return (type & m_combinedType) != 0;
	}
	CUDA_FUNC_IN BSDF(unsigned int type)
		: m_combinedType(type)
	{
	}
	CUDA_FUNC_IN static EMeasure getMeasure(unsigned int componentType)
	{
		if (componentType & ESmooth) {
			return ESolidAngle;
		} else if (componentType & EDelta) {
			return EDiscrete;
		} else if (componentType & EDelta1D) {
			return ELength;
		} else {
			return ESolidAngle; // will never be reached^^
		}
	}
};

#include "e_BSDF_Simple.h"

struct BSDFFirst
{
private:
#define SZ DMAX2(DMAX5(sizeof(diffuse), sizeof(roughdiffuse), sizeof(dielectric), sizeof(thindielectric), sizeof(roughdielectric)), \
		   DMAX6(sizeof(conductor), sizeof(roughconductor), sizeof(plastic), sizeof(phong), sizeof(ward), sizeof(hk)))
	CUDA_ALIGN(16) unsigned char Data[SZ];
#undef SZ
	unsigned int type;
public:
	CUDA_FUNC_IN Spectrum sample(BSDFSamplingRecord &bRec, float &pdf, const float2 &_sample) const
	{
		CALL_FUNC12(diffuse,roughdiffuse,dielectric,thindielectric,roughdielectric,conductor,roughconductor,plastic,roughplastic,phong,ward,hk, sample(bRec, pdf, _sample));
		return 0.0f;
	}
	CUDA_FUNC_IN Spectrum sample(BSDFSamplingRecord &bRec, const float2 &_sample) const
	{
		float p;
		return sample(bRec, p, _sample);
	}
	CUDA_FUNC_IN Spectrum f(BSDFSamplingRecord &bRec, EMeasure measure = ESolidAngle) const
	{
		CALL_FUNC12(diffuse,roughdiffuse,dielectric,thindielectric,roughdielectric,conductor,roughconductor,plastic,roughplastic,phong,ward,hk, f(bRec, measure));
		return 0.0f;
	}
	CUDA_FUNC_IN float pdf(BSDFSamplingRecord &bRec, EMeasure measure = ESolidAngle) const
	{
		CALL_FUNC12(diffuse,roughdiffuse,dielectric,thindielectric,roughdielectric,conductor,roughconductor,plastic,roughplastic,phong,ward,hk, pdf(bRec, measure));
		return 0.0f;
	}
	template<typename T> void LoadTextures(T callback) const
	{
		CALL_FUNC12(diffuse,roughdiffuse,dielectric,thindielectric,roughdielectric,conductor,roughconductor,plastic,roughplastic,phong,ward,hk, LoadTextures(callback));
	}
	CUDA_FUNC_IN unsigned int getType() const
	{
		return ((BSDF*)Data)->getType();
	}
	CUDA_FUNC_IN bool hasComponent(unsigned int type) const {
		return ((BSDF*)Data)->hasComponent(type);
	}
	STD_VIRTUAL_SET
};

#include "e_BSDF_Complex.h"

struct BSDFALL
{
private:
#define SZ DMAX3(DMAX5(sizeof(diffuse), sizeof(roughdiffuse), sizeof(dielectric), sizeof(thindielectric), sizeof(roughdielectric)), \
				 DMAX6(sizeof(conductor), sizeof(roughconductor), sizeof(plastic), sizeof(phong), sizeof(ward), sizeof(hk)), \
				 DMAX2(sizeof(coating), sizeof(roughcoating)))
	CUDA_ALIGN(16) unsigned char Data[SZ];
#undef SZ
	unsigned int type;
public:
	CUDA_FUNC_IN Spectrum sample(BSDFSamplingRecord &bRec, float &pdf, const float2 &_sample) const
	{
		CALL_FUNC14(diffuse,roughdiffuse,dielectric,thindielectric,roughdielectric,conductor,roughconductor,plastic,roughplastic,phong,ward,hk,coating,roughcoating, sample(bRec, pdf, _sample));
		return 0.0f;
	}
	CUDA_FUNC_IN Spectrum sample(BSDFSamplingRecord &bRec, const float2 &_sample) const
	{
		float p;
		return sample(bRec, p, _sample);
	}
	CUDA_FUNC_IN Spectrum f(BSDFSamplingRecord &bRec, EMeasure measure = ESolidAngle) const
	{
		CALL_FUNC14(diffuse,roughdiffuse,dielectric,thindielectric,roughdielectric,conductor,roughconductor,plastic,roughplastic,phong,ward,hk,coating,roughcoating, f(bRec, measure));
		return 0.0f;
	}
	CUDA_FUNC_IN float pdf(BSDFSamplingRecord &bRec, EMeasure measure = ESolidAngle) const
	{
		CALL_FUNC14(diffuse,roughdiffuse,dielectric,thindielectric,roughdielectric,conductor,roughconductor,plastic,roughplastic,phong,ward,hk,coating,roughcoating, pdf(bRec, measure));
		return 0.0f;
	}
	template<typename T> void LoadTextures(T callback) const
	{
		CALL_FUNC14(diffuse,roughdiffuse,dielectric,thindielectric,roughdielectric,conductor,roughconductor,plastic,roughplastic,phong,ward,hk,coating,roughcoating, LoadTextures(callback));
	}
	CUDA_FUNC_IN unsigned int getType() const
	{
		return ((BSDF*)Data)->getType();
	}
	CUDA_FUNC_IN bool hasComponent(unsigned int type) const {
		return ((BSDF*)Data)->hasComponent(type);
	}
	STD_VIRTUAL_SET
};