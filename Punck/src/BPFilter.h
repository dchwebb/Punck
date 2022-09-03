#pragma once
#include "initialisation.h"

class BPFilter {
public:
	void SetCutoff(float cutoff, float Q);
	float CalcFilter(float x);

	float xn2 = 0.0f, xn1 = 0.0f, yn2 = 0.0f, yn1 = 0.0f;

	struct IIRCoeff {
		float a0;
		float a1;
		float a2;
		float b0;
		float b1;
		float b2;
	} iirCoeff;
};
