#pragma once

/*
 * Much of the filter code gratefully taken from Iowa Hills Software
 * http://www.iowahills.com/
 */

#include "initialisation.h"
#include <cmath>
#include <complex>
#include <array>


#define MAX_POLES 8		// For declaring IIR arrays
#define MAX_SECTIONS (MAX_POLES + 1) / 2

#define M_PI           3.14159265358979323846

// For debugging
extern bool calculatingFilter;

enum FilterControl {LP, HP};
enum PassType {FilterOff, LowPass, HighPass};
enum IIRType {Butterworth, Custom};


typedef double iirdouble_t;			// to allow easy testing with floats or doubles
typedef std::complex<double> complex_t;



struct IIRCoeff {
	iirdouble_t a0[MAX_SECTIONS];
	iirdouble_t a1[MAX_SECTIONS];
	iirdouble_t a2[MAX_SECTIONS];
	iirdouble_t b0[MAX_SECTIONS];
	iirdouble_t b1[MAX_SECTIONS];
	iirdouble_t b2[MAX_SECTIONS];
};



struct IIRRegisters {
	iirdouble_t X1[MAX_SECTIONS];
	iirdouble_t X2[MAX_SECTIONS];
	iirdouble_t Y1[MAX_SECTIONS];
	iirdouble_t Y2[MAX_SECTIONS];

	IIRRegisters() {
		for (uint8_t i = 0; i < MAX_SECTIONS; ++i) {
			X1[i] = 0.0; X2[i] = 0.0; Y1[i] = 0.0; Y2[i] = 0.0;
		}
	}
	void Init() {
		for (uint8_t i = 0; i < MAX_SECTIONS; ++i) {
			X1[i] = 0.0; X2[i] = 0.0; Y1[i] = 0.0; Y2[i] = 0.0;
		}
	}
};

class IIRPrototype {
public:
	IIRPrototype(uint8_t poles) {
		numPoles = poles;
		DefaultProtoCoeff();
	}
	IIRPrototype() {};

	// These coeff form H(s) = 1 / (D2*s^2 + D1*s + D0)
	struct SPlaneCoeff {
		iirdouble_t D2[MAX_POLES];
		iirdouble_t D1[MAX_POLES];
		iirdouble_t D0[MAX_POLES];
	} Coeff;
	uint8_t numPoles = 0;

	void DefaultProtoCoeff();
private:
	void ButterworthPoly(std::array<std::complex<double>, MAX_POLES> &Roots);
	void GetFilterCoeff(std::array<std::complex<double>, MAX_POLES> &Roots);
};


class IIRFilter {
	friend class SerialHandler;									// Allow the serial handler access to private data for debug printing
	friend class Config;										// Allow access to config to store values
private:
	uint8_t numPoles = 1;
	uint8_t numSections = 0;
	PassType passType = LowPass;
	iirdouble_t cutoffFreq = 0.0f;
	IIRPrototype iirProto;										// Standard Butterworth is default
	IIRCoeff iirCoeff;

	iirdouble_t CalcSection(int k, iirdouble_t x, IIRRegisters& registers);

public:
	// constructors
	IIRFilter(uint8_t poles, PassType pass) : numPoles{poles}, passType{pass}, iirProto(IIRPrototype(poles)) {};
	IIRFilter() {};

	void CalcCoeff(iirdouble_t omega);
	void CalcCustomLowPass(iirdouble_t omega);
	iirdouble_t FilterSample(iirdouble_t sample, IIRRegisters& registers);
};


// Filter with fixed cut off (eg control smoothing)
class FixedFilter {
private:
	IIRFilter filter;
	IIRRegisters iirReg;
public:
	FixedFilter(uint8_t poles, PassType pass, iirdouble_t frequency) : filter{poles, pass} {
		filter.CalcCoeff(frequency);
	}
	iirdouble_t FilterSample(iirdouble_t sample);
};




struct Filter {
	friend class SerialHandler;				// Allow the serial handler access to private data for debug printing
	friend class Config;					// Allow access to config to store values
public:
	void Init();
	void Update(bool reset = false);
	float CalcFilter(iirdouble_t sample, channel c);
private:
	float potCentre = 29000;				// Configurable in calibration

	PassType passType;
	bool activeFilter = 0;					// choose which set of coefficients to use (so coefficients can be calculated without interfering with current filtering)
	float currentCutoff;

	// IIR settings
	bool customDamping = false;				// Set to true if using custom damping coefficients (otherwise will default to Butterworth)
	const uint8_t defaultPoles = 4;
	IIRFilter iirLPFilter[2] = {IIRFilter(defaultPoles, LowPass), IIRFilter(defaultPoles, LowPass)};			// Two filters for active and inactive
	IIRFilter iirHPFilter[2] = {IIRFilter(defaultPoles, HighPass), IIRFilter(defaultPoles, HighPass)};
	IIRRegisters iirLPReg[2];				// Two channels (left and right)
	IIRRegisters iirHPReg[2];				// Store separate shift registers for high and low pass to allow smooth transition

	float dampedADC, previousADC;			// ADC readings governing damped cut off level (and previous for hysteresis)
	FixedFilter filterADC = FixedFilter(2, LowPass, 0.002f);
	static constexpr uint16_t hysteresis = 30;

	void InitIIRFilter(iirdouble_t tone);
	float Bessel(float x);
};


extern Filter filter;


