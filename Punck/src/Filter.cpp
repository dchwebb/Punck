#include "Filter.h"


bool calculatingFilter = false;			// Debug


void Filter::Update(bool reset)
{
	// get filter values from pot and CV and smooth through fixed IIR filter
	dampedADC = filterADC.FilterSample(*adcControl);

	if (reset || std::abs(dampedADC - previousADC) > hysteresis) {
		calculatingFilter = true;

		previousADC = dampedADC;
		InitIIRFilter(dampedADC);
		calculatingFilter = false;

		activeFilter = !activeFilter;			// Switch active filter
	}
}


void Filter::Init()
{
	iirReg[0].Init();
	iirReg[0].Init();
}


void Filter::InitIIRFilter(const iirdouble_t tone)	// tone is a 0-65535 number representing cutoff generally from ADC input
{
	iirdouble_t cutoff;
	constexpr iirdouble_t LPMax = 0.995;
	constexpr iirdouble_t HPMin = 0.001;

	const bool inactiveFilter = !activeFilter;

	if (passType == filterPass::HighPass || passType == filterPass::BandPass) {					// Want a sweep from 0.03 to 0.99 with most travel at low end
		cutoff = pow((tone / 100000.0), 3.0) + HPMin;
	} else {									// Want a sweep from 0.001 to 0.2-0.3
		cutoff = std::min(0.03f + std::pow(tone / 65536.0f, 2.0f), LPMax);
	}
	iirFilter[inactiveFilter].CalcCoeff(cutoff);

	currentCutoff = cutoff;						// Debug
}


void Filter::SetCutoff(const iirdouble_t cutoff)
{
	// cutoff passed as omega - ie cutoff_frequency / nyquist
	const bool inactiveFilter = !activeFilter;
	iirFilter[inactiveFilter].CalcCoeff(cutoff);
	activeFilter = inactiveFilter;				// Switch active filter
	currentCutoff = cutoff;						// Debug
}


//	Take a new sample and return filtered value
float Filter::CalcFilter(const iirdouble_t sample, const channel c)
{
	return iirFilter[activeFilter].FilterSample(sample, iirReg[c]);
}


//	Take a new sample and return filtered value
iirdouble_t IIRFilter::FilterSample(const iirdouble_t sample, IIRRegisters& registers)
{
	iirdouble_t y = CalcSection(0, sample, registers);
	for (uint8_t k = 1; k < numSections; k++) {
		y = CalcSection(k, y, registers);
	}
	return y;
}


// Calculates each stage of a multi-section IIR filter (eg 8 pole is constructed from four 2-pole filters)
iirdouble_t IIRFilter::CalcSection(const int k, const iirdouble_t x, IIRRegisters& registers)
{
	iirdouble_t y, centerTap;
	static iirdouble_t MaxRegVal = 1.0E-12;

	// Zero the registers on an overflow condition
	if (MaxRegVal > 1.0E6) {
		MaxRegVal = 1.0E-12;
		for (uint8_t i = 0; i < maxSections; i++) {
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



/*
 Calculate the z-plane coefficients for IIR filters from 2nd order S-plane coefficients
 H(s) = 1 / ( As^2 + Bs + C )
 H(z) = ( b2 z^-2 + b1 z^-1 + b0 ) / ( a2 z^-2 + a1 z^-1 + a0 )
 See http://www.iowahills.com/A4IIRBilinearTransform.html
 function originally CalcIIRFilterCoeff()
 cutoff freq = omega * (sampling_rate / 2)
 */
void IIRFilter::CalcCoeff(const iirdouble_t omega)
{
	if (cutoffFreq == omega)		// Avoid recalculating coefficients when already found
		return;
	else
		cutoffFreq = omega;

	// Set the number of IIR filter sections we will be generating.
	numSections = (numPoles + 1) / 2;

	if (passType == filterPass::BandPass) {
		constexpr float bw = 0.001f;

		constexpr float R = 1.0f - 3.0 * bw;
		const float c2 = 2.0f * std::cos(2.0f * pi * omega);
		const float K = (1.0f - (R * c2) + (R * R)) / (2.0f - c2);

//		centerTap = x * iirCoeff.b0[k] + iirCoeff.b1[k] * registers.X1[k] + iirCoeff.b2[k] * registers.X2[k];
//		y = iirCoeff.a0[k] * centerTap - iirCoeff.a1[k] * registers.Y1[k] - iirCoeff.a2[k] * registers.Y2[k];

		for (uint32_t j = 0; j < numSections; j++) {
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
	const iirdouble_t T = 2.0 * tan(omega * pi / 2);

	// Calc the IIR coefficients. SPlaneCoeff.NumSections is the number of 1st and 2nd order s plane factors.
	for (uint32_t j = 0; j < numSections; j++) {
		const iirdouble_t A = iirProto.coeff.D2[j];				// Always one
		const iirdouble_t B = iirProto.coeff.D1[j];
		const iirdouble_t C = iirProto.coeff.D0[j];				// Always one

		// b's are the numerator, a's are the denominator
		if (passType == filterPass::LowPass) {
			if (A == 0.0) {						// 1 pole case
				const iirdouble_t arg = (2.0 * B + C * T);
				iirCoeff.a2[j] = 0.0;
				iirCoeff.a1[j] = (-2.0 * B + C * T) / arg;
				iirCoeff.a0[j] = 1.0;

				iirCoeff.b2[j] = 0.0;
				iirCoeff.b1[j] = T / arg * C;
				iirCoeff.b0[j] = T / arg * C;
			} else {							// 2 poles

				const iirdouble_t arg = (4.0 * A + 2.0 * B * T + C * T * T);
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
				const iirdouble_t arg = 2.0 * C + B * T;
				iirCoeff.a2[j] = 0.0;
				iirCoeff.a1[j] = (B * T - 2.0 * C) / arg;
				iirCoeff.a0[j] = 1.0;

				iirCoeff.b2[j] = 0.0;
				iirCoeff.b1[j] = -2.0 / arg * C;
				iirCoeff.b0[j] = 2.0 / arg * C;
			} else {							// 2 poles
				const iirdouble_t arg = A * T * T + 4.0 * C + 2.0 * B * T;
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


// Calculate the default S Plane Coefficients (Butterworth):  H(s) = 1 / (D2*s^2 + D1*s + D0)
void IIRPrototype::DefaultProtoCoeff()
{
	std::array<complex_t, maxPoles> poles;

	// A one pole filter is simply 1/(s+1)
	if (numPoles == 1) {
		coeff.D0[0] = 1.0;
		coeff.D1[0] = 1.0;
		coeff.D2[0] = 0.0;
	} else {
		poles[0] = complex_t(0.0, 0.0);
		ButterworthPoly(poles);
		GetFilterCoeff(poles);
	}
}


// Calculate the roots for a Butterworth filter: fill the array Roots[]
void IIRPrototype::ButterworthPoly(std::array<complex_t, maxPoles> &roots)
{
	uint32_t n = 0;

	for (uint8_t j = 0; j < numPoles / 2; j++) {
		const iirdouble_t theta = pi * static_cast<iirdouble_t>(2 * j + numPoles + 1) / static_cast<iirdouble_t>(2 * numPoles);
		roots[n++] = complex_t(cos(theta), sin(theta));
		roots[n++] = complex_t(cos(theta), -sin(theta));
	}
	if (numPoles % 2 == 1) {
		roots[n++] = complex_t(-1.0, 0.0);		// The real root for odd pole counts
	}
}


// Create the 2nd order polynomials with coefficients A2, A1, A0.
void IIRPrototype::GetFilterCoeff(std::array<complex_t, maxPoles> &roots)
{
	// Sort the roots with the most negative real part first
	std::sort(roots.begin(), roots.end(), [](const complex_t &lhs, const complex_t &rhs) {
		return lhs.real() < rhs.real();
	});

	// This forms the 2nd order coefficients from the root value. Ignore roots in the Right Hand Plane.
	uint32_t polyCount = 0;
	for (uint32_t j = 0; j < numPoles; ++j) {
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


iirdouble_t FixedFilter::FilterSample(const iirdouble_t sample)
{
	return filter.FilterSample(sample, iirReg);
}

