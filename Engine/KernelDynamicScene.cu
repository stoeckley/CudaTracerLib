#include "KernelDynamicScene.h"
#include <Kernel/TraceHelper.h>
#include "Light.h"

namespace CudaTracerLib {

const KernelLight* KernelDynamicScene::sampleLight(float& emPdf, Vec2f& sample) const
{
	if (m_sLightData.UsedCount == 0)
		return 0;
	unsigned int idx = unsigned int(m_sLightData.UsedCount * sample.x);
	sample.x = sample.x - idx / float(m_sLightData.UsedCount);
	emPdf = 1.0f / float(m_sLightData.UsedCount);
	return m_sLightData.Data + idx;
}

float KernelDynamicScene::pdfEmitterDiscrete(const KernelLight *emitter) const
{
	unsigned int index = emitter - m_sLightData.Data;
	return 1.0f / float(m_sLightData.UsedCount);
}

bool KernelDynamicScene::Occluded(const Ray& r, float tmin, float tmax, TraceResult* res) const
{
	const float eps = 0.01f;//remember this is an occluded test, so we shrink the interval!
	TraceResult r2 = Traceray(r);
	if (r2.hasHit() && res)
		*res = r2;
	bool end = r2.m_fDist < tmax * (1.0f - eps);
	if (isinf(tmax) && !r2.hasHit())
		end = false;
	return r2.m_fDist > tmin * (1.0f + eps) && end;
	//return tmin < r2.m_fDist && r2.m_fDist < tmax;
}

Spectrum KernelDynamicScene::EvalEnvironment(const Ray& r) const
{
	if (m_uEnvMapIndex != UINT_MAX)
		return m_sLightData[m_uEnvMapIndex].As<InfiniteLight>()->evalEnvironment(r);
	else return Spectrum(0.0f);
}

Spectrum KernelDynamicScene::EvalEnvironment(const Ray& r, const Ray& rX, const Ray& rY) const
{
	if (m_uEnvMapIndex != UINT_MAX)
		return m_sLightData[m_uEnvMapIndex].As<InfiniteLight>()->evalEnvironment(r, rX, rY);
	else return Spectrum(0.0f);
}


Spectrum KernelDynamicScene::sampleEmitterDirect(DirectSamplingRecord &dRec, const Vec2f &_sample) const
{
	Vec2f sample = _sample;
	float emPdf;
	const KernelLight *emitter = sampleLight(emPdf, sample);
	if (emitter == 0)
	{
		dRec.pdf = 0;
		dRec.object = 0;
		return 0.0f;
	}
	Spectrum value = emitter->sampleDirect(dRec, sample);
	if (dRec.pdf != 0)
	{
		/*if (testVisibility && Occluded(Ray(dRec.ref, dRec.n), 0, dRec.dist))
		{
		dRec.object = 0;
		return Spectrum(0.0f);
		}*/
		dRec.pdf *= emPdf;
		value /= emPdf;
		dRec.object = emitter;
		return value;
	}
	else
	{
		return Spectrum(0.0f);
	}
}

Spectrum KernelDynamicScene::sampleSensorDirect(DirectSamplingRecord &dRec, const Vec2f &sample) const
{
	Spectrum value = m_Camera.sampleDirect(dRec, sample);
	if (dRec.pdf != 0)
	{
		/*if (testVisibility && Occluded(Ray(dRec.ref, dRec.d), 0, dRec.dist))
		{
		dRec.object = 0;
		return Spectrum(0.0f);
		}*/
		dRec.object = &g_SceneData;
		return value;
	}
	else
	{
		return Spectrum(0.0f);
	}
}

float KernelDynamicScene::pdfEmitterDirect(const DirectSamplingRecord &dRec) const
{
	const KernelLight *emitter = (KernelLight*)dRec.object;
	return emitter->pdfDirect(dRec) * pdfEmitterDiscrete(emitter);
}

float KernelDynamicScene::pdfSensorDirect(const DirectSamplingRecord &dRec) const
{
	return m_Camera.pdfDirect(dRec);
}

Spectrum KernelDynamicScene::sampleEmitterPosition(PositionSamplingRecord &pRec, const Vec2f &_sample) const
{
	Vec2f sample = _sample;
	float emPdf;
	const KernelLight *emitter = sampleLight(emPdf, sample);

	Spectrum value = emitter->samplePosition(pRec, sample);

	pRec.object = emitter;
	pRec.pdf *= emPdf;

	return value / emPdf;
}

Spectrum KernelDynamicScene::sampleSensorPosition(PositionSamplingRecord &pRec, const Vec2f &sample, const Vec2f *extra) const
{
	pRec.object = &m_Camera;
	return m_Camera.samplePosition(pRec, sample, extra);
}

float KernelDynamicScene::pdfEmitterPosition(const PositionSamplingRecord &pRec) const
{
	const KernelLight *emitter = (const KernelLight*)pRec.object;
	return emitter->pdfPosition(pRec) * pdfEmitterDiscrete(emitter);
}

float KernelDynamicScene::pdfSensorPosition(const PositionSamplingRecord &pRec) const
{
	const Sensor *sensor = (const Sensor*)pRec.object;
	return sensor->pdfPosition(pRec);
}

Spectrum KernelDynamicScene::sampleEmitterRay(Ray& ray, const KernelLight*& emitter, const Vec2f &spatialSample, const Vec2f &directionalSample) const
{
	Vec2f sample = spatialSample;
	float emPdf;
	emitter = sampleLight(emPdf, sample);

	return emitter->sampleRay(ray, sample, directionalSample) / emPdf;
}

Spectrum KernelDynamicScene::sampleSensorRay(Ray& ray, const Sensor*& sensor, const Vec2f &spatialSample, const Vec2f &directionalSample) const
{
	sensor = &m_Camera;
	return sensor->sampleRay(ray, spatialSample, directionalSample);
}

}