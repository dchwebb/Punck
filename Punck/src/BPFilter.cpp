#include <BPFilter.h>

void BPFilter::SetCutoff(float cutoff, float Q)
{
	// Convert cutoff frequency to omega
	float w0 = (2.0f * pi * cutoff) / systemSampleRate;
	//float Q = cutoff / 500;
	float alpha = sin(w0) / (2 * Q);
	iirCoeff.b0 = alpha;
	iirCoeff.b1 = 0.0f;
	iirCoeff.b2 = -iirCoeff.b0;
	iirCoeff.a0 = 1 + alpha;
	iirCoeff.a1 = -2 * cos(w0);
	iirCoeff.a2 = 1 - alpha;

}


float BPFilter::CalcFilter(float xn0)
{
	float yn0 = (iirCoeff.b0 / iirCoeff.a0) * xn1 + (iirCoeff.b1 / iirCoeff.a0) * xn1 + (iirCoeff.b2 / iirCoeff.a0) * xn2
			- (iirCoeff.a1 / iirCoeff.a0) * yn1 - (iirCoeff.a2 / iirCoeff.a0) * yn2;
	xn2 = xn1;
	xn1 = xn0;
	yn2 = yn1;
	yn1 = yn0;

	return yn0;
}


void BPFilter::Init()
{
	xn2 = 0.0f;
	xn1 = 0.0f;
	yn2 = 0.0f;
	yn1 = 0.0f;
}
