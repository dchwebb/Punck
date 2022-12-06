#pragma once
#include "initialisation.h"
#include <cmath>
#include <complex>
#include <array>

/*
 * Much of the filter code gratefully taken from Iowa Hills Software
 * http://www.iowahills.com/
 */

typedef std::complex<float> complex_t;

enum class filterPass {LowPass, HighPass, BandPass};


template<uint32_t poles = 2>
struct IIRRegisters {
	static constexpr uint32_t sections = (poles + 1) / 2;
	float x1[sections];
	float x2[sections];
	float y1[sections];
	float y2[sections];

	IIRRegisters() {
		Init();
	}


	void Init() {
		for (uint8_t i = 0; i < sections; ++i) {
			x1[i] = 0.0; x2[i] = 0.0; y1[i] = 0.0; y2[i] = 0.0;
		}
	}
};


template<uint32_t poles = 2>
class IIRPrototype {
public:
	IIRPrototype() {
		DefaultProtoCoeff();
	}

	// These coeff form H(s) = 1 / (D2*s^2 + D1*s + D0)
	struct SPlaneCoeff {
		float D2[poles];
		float D1[poles];
		float D0[poles];
	} coeff;

	// Calculate the default S Plane Coefficients (Butterworth):  H(s) = 1 / (D2*s^2 + D1*s + D0)
	void DefaultProtoCoeff()
	{
		std::array<complex_t, poles> roots;

		// A one pole filter is simply 1/(s+1)
		if (poles == 1) {
			coeff.D0[0] = 1.0;
			coeff.D1[0] = 1.0;
			coeff.D2[0] = 0.0;
		} else {
			roots[0] = complex_t(0.0, 0.0);
			ButterworthPoly(roots);
			GetFilterCoeff(roots);
		}
	}

private:
	// Calculate the roots for a Butterworth filter: fill the array Roots[]
	void ButterworthPoly(std::array<complex_t, poles> &roots)
	{
		uint32_t n = 0;

		for (uint8_t j = 0; j < poles / 2; j++) {
			const float theta = pi * static_cast<float>(2 * j + poles + 1) / static_cast<float>(2 * poles);
			roots[n++] = complex_t(cos(theta), sin(theta));
			roots[n++] = complex_t(cos(theta), -sin(theta));
		}
		if (poles % 2 == 1) {
			roots[n++] = complex_t(-1.0, 0.0);		// The real root for odd pole counts
		}
	}


	// Create the 2nd order polynomials with coefficients A2, A1, A0.
	void GetFilterCoeff(std::array<complex_t, poles> &roots)
	{
		// Sort the roots with the most negative real part first
		std::sort(roots.begin(), roots.end(), [](const complex_t &lhs, const complex_t &rhs) {
			return lhs.real() < rhs.real();
		});

		// This forms the 2nd order coefficients from the root value. Ignore roots in the Right Hand Plane.
		uint32_t polyCount = 0;
		for (uint32_t j = 0; j < poles; ++j) {
			if (roots[j].real() > 0.0)
				continue;							// Right Hand Plane
			if (roots[j].real() == 0.0 && roots[j].imag() == 0.0)
				continue;							// At the origin.  This should never happen.

			if (roots[j].real() == 0.0) {			// Imag Root (A poly zero)
				coeff.D2[polyCount] = 1.0;
				coeff.D1[polyCount] = 0.0;
				coeff.D0[polyCount] = roots[j].imag() * roots[j].imag();
				j++;
				polyCount++;
			} else if (roots[j].imag() == 0.0) {	// Real Pole
				coeff.D2[polyCount] = 0.0;
				coeff.D1[polyCount] = 1.0;
				coeff.D0[polyCount] = -roots[j].real();
				polyCount++;
			} else { 								// Complex Pole
				coeff.D2[polyCount] = 1.0;
				coeff.D1[polyCount] = -2.0 * roots[j].real();
				coeff.D0[polyCount] = roots[j].real() * roots[j].real() + roots[j].imag() * roots[j].imag();
				j++;
				polyCount++;
			}
		}
	}

};


template<uint32_t poles = 2>
class IIRFilter {
	static constexpr uint32_t sections = (poles + 1) / 2;
public:

	// constructors
	IIRFilter(filterPass pass) : passType{pass}, iirProto(IIRPrototype()) {};

	/*
	 Calculate the z-plane coefficients for IIR filters from 2nd order S-plane coefficients
	 H(s) = 1 / ( As^2 + Bs + C )
	 H(z) = ( b2 z^-2 + b1 z^-1 + b0 ) / ( a2 z^-2 + a1 z^-1 + a0 )
	 See http://www.iowahills.com/A4IIRBilinearTransform.html
	 function originally CalcIIRFilterCoeff()
	 cutoff freq = omega * (sampling_rate / 2)
	 */
	void CalcCoeff(const float omega)
	{
		if (cutoffFreq == omega) {		// Avoid recalculating coefficients when already found
			return;
		}
		cutoffFreq = omega;

		// Set the number of IIR filter sections we will be generating.
		//sections = (poles + 1) / 2;

		if (passType == filterPass::BandPass) {
			constexpr float bw = 0.001f;
			constexpr float R = 1.0f - 3.0 * bw;
			const float c2 = 2.0f * std::cos(2.0f * pi * omega);
			const float K = (1.0f - (R * c2) + (R * R)) / (2.0f - c2);

			for (uint32_t j = 0; j < sections; j++) {
				iirCoeff.b0[j] = 1.0f - K;
				iirCoeff.b1[j] = (K - R) * c2;
				iirCoeff.b2[j] = (R * R) - K;

				iirCoeff.a0[j] = 1.0f;
				iirCoeff.a1[j] = R * c2;
				iirCoeff.a2[j] = -(R * R);
			}
			return;
		}

		// T sets the IIR filter's corner frequency, or center frequency
		// The Bilinear transform is defined as:  s = 2/T * tan(Omega/2) = 2/T * (1 - z^-1)/(1 + z^-1)
		const float T = 2.0 * tan(omega * pi / 2);

		// Calc the IIR coefficients. SPlaneCoeff.sections is the number of 1st and 2nd order s plane factors.
		for (uint32_t j = 0; j < sections; j++) {
			const float A = iirProto.coeff.D2[j];				// Always one
			const float B = iirProto.coeff.D1[j];
			const float C = iirProto.coeff.D0[j];				// Always one

			// b's are the numerator, a's are the denominator
			if (passType == filterPass::LowPass) {
				if (A == 0.0) {						// 1 pole case
					const float arg = (2.0 * B + C * T);
					iirCoeff.a2[j] = 0.0;
					iirCoeff.a1[j] = (-2.0 * B + C * T) / arg;
					iirCoeff.a0[j] = 1.0;

					iirCoeff.b2[j] = 0.0;
					iirCoeff.b1[j] = T / arg * C;
					iirCoeff.b0[j] = T / arg * C;
				} else {							// 2 poles

					const float arg = (4.0 * A + 2.0 * B * T + C * T * T);
					iirCoeff.a2[j] = (4.0 * A - 2.0 * B * T + C * T * T) / arg;
					iirCoeff.a1[j] = (2.0 * C * T * T - 8.0 * A) / arg;
					iirCoeff.a0[j] = 1.0;

					// With all pole filters, LPF numerator is (z+1)^2, so all Z Plane zeros are at -1
					iirCoeff.b2[j] = (T * T) / arg * C;
					iirCoeff.b1[j] = (2.0 * T * T) / arg * C;
					iirCoeff.b0[j] = (T * T) / arg * C;
				}
			}

			if (passType == filterPass::HighPass) {
				if (A == 0.0) {						// 1 pole
					const float arg = 2.0 * C + B * T;
					iirCoeff.a2[j] = 0.0;
					iirCoeff.a1[j] = (B * T - 2.0 * C) / arg;
					iirCoeff.a0[j] = 1.0;

					iirCoeff.b2[j] = 0.0;
					iirCoeff.b1[j] = -2.0 / arg * C;
					iirCoeff.b0[j] = 2.0 / arg * C;
				} else {							// 2 poles
					const float arg = A * T * T + 4.0 * C + 2.0 * B * T;
					iirCoeff.a2[j] = (A * T * T + 4.0 * C - 2.0 * B * T) / arg;
					iirCoeff.a1[j] = (2.0 * A * T * T - 8.0 * C) / arg;
					iirCoeff.a0[j] = 1.0;

					// With all pole filters, HPF numerator is (z-1)^2, so all Z Plane zeros are at 1
					iirCoeff.b2[j] = 4.0 / arg * C;
					iirCoeff.b1[j] = -8.0 / arg * C;
					iirCoeff.b0[j] = 4.0 / arg * C;
				}
			}
		}

	}


	//	Take a new sample and return filtered value
	float FilterSample(const float sample, IIRRegisters<poles>& registers)
	{
		float y = CalcSection(0, sample, registers);
		for (uint8_t k = 1; k < sections; k++) {
			y = CalcSection(k, y, registers);
		}
		return y;
	}

private:
	filterPass passType = filterPass::LowPass;
	float cutoffFreq = 0.0f;
	IIRPrototype<poles> iirProto;						// Standard Butterworth is default

	struct IIRCoeff {
		float a0[sections];
		float a1[sections];
		float a2[sections];
		float b0[sections];
		float b1[sections];
		float b2[sections];
	} iirCoeff;

	// Calculates each stage of a multi-section IIR filter (eg 8 pole is constructed from four 2-pole filters)
	float CalcSection(const int k, const float x, IIRRegisters<poles>& registers)
	{
		float y, centerTap;
		static float MaxRegVal = 1.0E-12;

		// Zero the registers on an overflow condition
		if (MaxRegVal > 1.0E6) {
			MaxRegVal = 1.0E-12;
			for (uint8_t i = 0; i < sections; i++) {
				registers.x1[i] = 0.0;
				registers.x2[i] = 0.0;
				registers.y1[i] = 0.0;
				registers.y2[i] = 0.0;
			}
		}

		centerTap = x * iirCoeff.b0[k] + iirCoeff.b1[k] * registers.x1[k] + iirCoeff.b2[k] * registers.x2[k];
		y = iirCoeff.a0[k] * centerTap - iirCoeff.a1[k] * registers.y1[k] - iirCoeff.a2[k] * registers.y2[k];

		registers.x2[k] = registers.x1[k];
		registers.x1[k] = x;
		registers.y2[k] = registers.y1[k];
		registers.y1[k] = y;

		// MaxRegVal is used to prevent overflow. Note that CenterTap regularly exceeds 100k but y maxes out at about 65k
		if (std::abs(centerTap) > MaxRegVal) {
			MaxRegVal = std::abs(centerTap);
		}
		if (std::abs(y) > MaxRegVal) {
			MaxRegVal = std::abs(y);
		}

		return y;
	}

};


// Filter with fixed cut off (eg control smoothing)
template<uint32_t poles = 2>
class FixedFilter {
	static constexpr uint32_t sections = (poles + 1) / 2;
public:
	FixedFilter(filterPass pass, float frequency) : filter{pass} {
		filter.CalcCoeff(frequency);
	}

	float FilterSample(const float sample)
	{
		return filter.FilterSample(sample, iirReg);
	}


private:
	IIRFilter<poles> filter;
	IIRRegisters<poles> iirReg;
};



// 2 channel LP or HP filter with dual sets of coefficients to allow clean recalculation and switching
template<uint32_t poles = 2>
struct Filter {
	static constexpr uint32_t sections = (poles + 1) / 2;
public:
	Filter(filterPass pass, volatile uint16_t* adc) : passType{pass}, iirFilter{IIRFilter<poles>(pass), IIRFilter<poles>(pass)}, adcControl{adc}
	{
		Update(true);								// Force calculation of coefficients
	}


	void Init()
	{
		iirReg[0].Init();
		iirReg[0].Init();
	}


	void SetCutoff(const float cutoff)
	{
		// cutoff passed as omega - ie cutoff_frequency / nyquist
		const bool inactiveFilter = !activeFilter;
		iirFilter[inactiveFilter].CalcCoeff(cutoff);
		activeFilter = inactiveFilter;				// Switch active filter
		currentCutoff = cutoff;						// Debug
	}


	void Update(bool reset)
	{
		// get filter values from pot and CV and smooth through fixed IIR filter
		if (adcControl == nullptr) {
			return;
		}

		dampedADC = filterADC.FilterSample(*adcControl);
		if (reset || std::abs(dampedADC - previousADC) > hysteresis) {
			previousADC = dampedADC;
			InitIIRFilter(dampedADC);
			activeFilter = !activeFilter;			// Switch active filter
		}
	}


	float CalcFilter(const float sample, const channel c)
	{
		//	Take a new sample and return filtered value
		return iirFilter[activeFilter].FilterSample(sample, iirReg[c]);
	}


private:
	float potCentre = 29000;				// Configurable in calibration

	filterPass passType;
	bool activeFilter = 0;					// choose which set of coefficients to use (so coefficients can be calculated without interfering with current filtering)
	float currentCutoff;

	IIRFilter<poles> iirFilter[2];			// Two filters for active and inactive
	IIRRegisters<poles> iirReg[2];			// Two channels (left and right)

	volatile uint16_t* adcControl;
	float dampedADC, previousADC;			// ADC readings governing damped cut off level (and previous for hysteresis)
	FixedFilter<poles> filterADC = FixedFilter(filterPass::LowPass, 0.002f);
	static constexpr uint16_t hysteresis = 30;

	void InitIIRFilter(const float tone)	// tone is a 0-65535 number representing cutoff generally from ADC input
	{
		float cutoff;
		constexpr float LPMax = 0.995;
		constexpr float HPMin = 0.001;

		const bool inactiveFilter = !activeFilter;

		if (passType == filterPass::HighPass || passType == filterPass::BandPass) {					// Want a sweep from 0.03 to 0.99 with most travel at low end
			cutoff = pow((tone / 100000.0), 3.0) + HPMin;
		} else {									// Want a sweep from 0.001 to 0.2-0.3
			cutoff = std::min(0.03f + std::pow(tone / 65536.0f, 2.0f), LPMax);
		}
		iirFilter[inactiveFilter].CalcCoeff(cutoff);

		currentCutoff = cutoff;						// Debug
	}
};



