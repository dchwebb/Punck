#pragma once

/*
 * Much of the filter code gratefully taken from Iowa Hills Software
 * http://www.iowahills.com/
 */

#include "initialisation.h"
#include <cmath>
#include <complex>
#include <array>


static constexpr uint32_t maxPoles = 8;		// For declaring IIR arrays
static constexpr uint32_t maxSections = (maxPoles + 1) / 2;

// For debugging
extern bool calculatingFilter;

enum class filterPass {LowPass, HighPass, BandPass};

typedef float iirdouble_t;			// to allow easy testing with floats or doubles
typedef std::complex<float> complex_t;


struct IIRRegisters {
	iirdouble_t x1[maxSections];
	iirdouble_t x2[maxSections];
	iirdouble_t y1[maxSections];
	iirdouble_t y2[maxSections];

	IIRRegisters() {
		Init();
	}
	void Init() {
		for (uint8_t i = 0; i < maxSections; ++i) {
			x1[i] = 0.0; x2[i] = 0.0; y1[i] = 0.0; y2[i] = 0.0;
		}
	}
};


class IIRPrototype {
public:
	IIRPrototype(const uint8_t poles) {
		numPoles = poles;
		DefaultProtoCoeff();
	}

	// These coeff form H(s) = 1 / (D2*s^2 + D1*s + D0)
	struct SPlaneCoeff {
		iirdouble_t D2[maxPoles];
		iirdouble_t D1[maxPoles];
		iirdouble_t D0[maxPoles];
	} coeff;
	uint8_t numPoles = 0;

	void DefaultProtoCoeff();
private:
	void ButterworthPoly(std::array<complex_t, maxPoles> &roots);
	void GetFilterCoeff(std::array<complex_t, maxPoles> &roots);
};


class IIRFilter {
public:
	// constructors
	IIRFilter(uint8_t poles, filterPass pass) : numPoles{poles}, passType{pass}, iirProto(IIRPrototype(poles)) {};

	void CalcCoeff(const iirdouble_t omega);
	iirdouble_t FilterSample(const iirdouble_t sample, IIRRegisters& registers);

private:
	uint8_t numPoles = 1;
	uint8_t numSections = 0;
	filterPass passType = filterPass::LowPass;
	iirdouble_t cutoffFreq = 0.0f;
	IIRPrototype iirProto;						// Standard Butterworth is default

	struct IIRCoeff {
		iirdouble_t a0[maxSections];
		iirdouble_t a1[maxSections];
		iirdouble_t a2[maxSections];
		iirdouble_t b0[maxSections];
		iirdouble_t b1[maxSections];
		iirdouble_t b2[maxSections];
	} iirCoeff;

	iirdouble_t CalcSection(int k, iirdouble_t x, IIRRegisters& registers);
};


// Filter with fixed cut off (eg control smoothing)
class FixedFilter {
public:
	FixedFilter(uint8_t poles, filterPass pass, iirdouble_t frequency) : filter{poles, pass} {
		filter.CalcCoeff(frequency);
	}
	iirdouble_t FilterSample(iirdouble_t sample);

private:
	IIRFilter filter;
	IIRRegisters iirReg;
};


// 2 channel LP or HP filter with dual sets of coefficients to allow clean recalculation and switching
struct Filter {
public:
	Filter(uint8_t poles, filterPass pass, volatile uint16_t* adc) :
		passType{pass}, iirFilter{IIRFilter(poles, pass), IIRFilter(poles, pass)}, adcControl{adc}
	{
		Update(true);						// Force calculation of coefficients
	}

	void Update(bool reset = false);		// Recalculate coefficients from ADC reading if required
	void SetCutoff(const float cutoff);		// Recalculate coefficients from supplied cutoff value
	float CalcFilter(const iirdouble_t sample, const channel c);
	void Init();
private:
	float potCentre = 29000;				// Configurable in calibration

	filterPass passType;
	bool activeFilter = 0;					// choose which set of coefficients to use (so coefficients can be calculated without interfering with current filtering)
	float currentCutoff;

	IIRFilter iirFilter[2];					// Two filters for active and inactive
	IIRRegisters iirReg[2];					// Two channels (left and right)

	volatile uint16_t* adcControl;
	float dampedADC, previousADC;			// ADC readings governing damped cut off level (and previous for hysteresis)
	FixedFilter filterADC = FixedFilter(2, filterPass::LowPass, 0.002f);
	static constexpr uint16_t hysteresis = 30;

	void InitIIRFilter(const iirdouble_t tone);
};



