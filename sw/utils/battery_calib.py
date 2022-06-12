#  Copyright 2022 Nicolas Maltais
#
#  Licensed under the Apache License, Version 2.0 (the "License");
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.

# This utility is used to generate calibration values to be programmed in internal EEPROM from
# the CSV measurements file written by "./gcprog.py battery --calibrate".
#
# The game console lacks a battery monitoring IC, and estimate battery percentage only from
# the battery voltage, under load. Since the voltage varies with different loads, the load must
# be estimated first. This is done by knowing the contrast and average display color, and using
# an heuristic to judge the load factor from 0 to 100%.
# Then the measured discharge voltage curve is used to estimate the battery percentage.
#
# The general process is as follows:
# 1. Find the upper and lower envelope on the discharge scatter plot.
# 2. For each point, estimate the load factor using its position in between the two envelope curves.
# 3. Do a least square regression to obtain an equation relating load factor to contrast and color.
# 4. Sample points along the envelope curves that will be used to evaluate battery level.
# 5. Compare estimated vs. actual battery percentage for each point and make a linear adjustment.
#
# Usage:
#
#     ./battery_calib.py <input-csv-file> [output-dat-file] [--no-plots]

import argparse
import math

import matplotlib.pyplot as plt
import numpy as np
import numpy.linalg
import scipy as sp
import scipy.optimize

from prog.battery import CALIB_CONTRAST_COUNT, CALIB_COLOR_COUNT
# time granularity in analysis
from utils import PathLike

TIME_GRANULARITY = 0.1
# number of samples in the convolution used to filter the envelope
ENVELOPE_CONVOLVE_DURATION = 10  # min
# duration cut from each side of the discharge cut
ENVELOPE_CUT = 1  # min
# number of samples used for estimating battery percent
BATTERY_SAMPLES = 16

parser = argparse.ArgumentParser(description="Utility to analyze battery measurements")
parser.add_argument(
    "input_file", action="store", type=str,
    help="Input CSV file produced by gcprog.")
parser.add_argument(
    "output_file", action="store", type=str, default="calibration.dat", nargs="?",
    help=f"Output HEX file, 'calibration.bin' by default.")
parser.add_argument(
    "-P", "--no-plot", action="store_false", dest="show_plots",
    help="Don't show plots after analysis.")


class BatteryCalibration:
    time_data: np.ndarray
    adc_data: np.ndarray
    contrast_data: np.ndarray
    color_data: np.ndarray
    n: int
    max_time: float
    show_plots: bool

    data: bytearray
    pos: int

    def __init__(self, input_file: PathLike, show_plots: bool = False):
        data = np.loadtxt(input_file, delimiter=",")
        self.time_data = data[:, 0] / 60
        self.adc_data = data[:, 1]
        self.contrast_data = data[:, 2]
        self.color_data = data[:, 3]
        self.n = data.shape[0]
        self.max_time = np.max(self.time_data)
        self.show_plots = show_plots

        self.data = bytearray(12 + 2 + 64)
        self.pos = 0

    def calibrate(self) -> bytes:
        self.plot_calibration_data()
        envelope = self.scatter_envelope()
        load_points = self.estimate_load(envelope)
        coeffs = self.fit_load_func(load_points, envelope)
        points = self.sample_envelope(envelope)
        self.compare_battery_percent(coeffs, points)

        if self.show_plots:
            plt.show()

        return self.data

    @staticmethod
    def _warn(message: str) -> None:
        print(f"WARNING: {message}.")

    def write_data(self, value: int, n: int) -> None:
        for i in range(n):
            self.data[self.pos] = value & 0xff
            value >>= 8
            self.pos += 1

    def plot_calibration_data(self):
        voltage = self.adc_data * 6.93592e-5
        plt.figure()
        plt.plot(self.time_data, voltage, "b.", markersize=1)
        plt.plot((0, self.max_time), (3.3, 3.3), "k-")
        plt.grid(True, which='both')
        plt.minorticks_on()
        plt.title("Battery voltage by time")
        plt.ylabel("Voltage (V)")
        plt.xlabel("Time (min)")

    def scatter_envelope(self):
        """Find upper and lower confidence curves for data points."""
        # exclude data points
        time_points = round(self.max_time / TIME_GRANULARITY + 1)
        time_range = np.linspace(0, self.max_time, time_points)

        # find lower envelope
        envelope = np.zeros((time_points, 2))
        envelope[-1, 0] = self.adc_data[-1]
        j = 0
        for i, t in enumerate(reversed(time_range)):
            if i == 0:
                continue
            while j < self.n - 1 and t < self.time_data[-j - 1]:
                j += 1
            envelope[-i - 1, 0] = max(envelope[-i, 0], self.adc_data[-j - 1])

        # find lower envelope
        envelope[0, 1] = self.adc_data[0]
        j = 0
        for i, t in enumerate(time_range):
            if i == 0:
                continue
            while j < self.n and t > self.time_data[j]:
                j += 1
            envelope[i, 1] = min(envelope[i - 1, 1], self.adc_data[j])

        # filter envelope
        clen = round(ENVELOPE_CONVOLVE_DURATION / TIME_GRANULARITY)
        for i in range(2):
            envelope[clen // 2 - 1:-clen // 2, i] = \
                np.convolve(envelope[:, i], np.ones(clen) / clen, "valid")

        # ignore first few points on both sides to limit corner effects
        xlen = round(ENVELOPE_CUT / TIME_GRANULARITY)
        envelope[:xlen, :] = envelope[xlen, :]
        envelope[-xlen:, :] = envelope[-xlen, :]

        plt.figure()
        plt.plot(self.time_data, self.adc_data, "g.", markersize=2)
        for i in range(2):
            plt.plot(time_range, envelope[:, i], "r-")
        plt.grid(True, which='both')
        plt.minorticks_on()
        plt.title(f"ADC reading by time with envelope")
        plt.ylabel("ADC reading")
        plt.xlabel("Time (min)")

        return envelope

    def estimate_load(self, envelope):
        """Associate a load factor (0-1) to each data point."""
        load_points = np.zeros(self.n)
        for i in range(self.n):
            idx = round(self.time_data[i] / TIME_GRANULARITY)
            up, lo = envelope[idx, :]
            adc = self.adc_data[i]
            load_points[i] = 1 - np.clip((adc - lo) / (up - lo), 0, 1)

        plt.figure()
        plt.plot(self.time_data, load_points * 100, "k.", markersize=1)
        plt.title("Estimated load by time")
        plt.xlabel("Time (min)")
        plt.ylabel("Load (%)")

        return load_points

    @staticmethod
    def load_func(x, a, b, c, d, e, f):
        res = a * x[0] * x[0] + b * x[0] * x[1] + c * x[1] * x[1] + d * x[0] + e * x[1] + f
        # if display color is zero force the load to zero
        res[x[1] == 0] = 0
        return res

    @staticmethod
    def load_func_int(x, y, coeffs):
        # an equivalent of this function is implemented in sys/power
        var = np.int32([x, y, 1])
        res = np.int32(0)
        k = 0
        for i in range(3):
            for j in range(i + 1):
                res += (var[i] * var[j] * coeffs[k]) // 256
                if abs(res) > 0x7fff:
                    BatteryCalibration._warn("overflow during load calculation")
                k += 1
        res //= 8
        return np.clip(res, 0, 255)

    def encode_load_coeffs(self, popt):
        coeffs = np.zeros(6, dtype=np.int32)
        for i in range(6):
            coeffs[i] = round(popt[i] * 2048)  # fixed point Q5.11
            if abs(coeffs[i]) > 0x7fff:
                BatteryCalibration._warn(f"coefficient {i} overflow")

        # check sure there won't be any overflow during load calculation.
        k = 0
        var = [255, 15, 1]
        for i in range(3):
            for j in range(i + 1):
                if abs(var[i] * var[j] * coeffs[k]) > 0x7fffff:
                    BatteryCalibration._warn(f"overflow in calculation with coefficient {k}")
                k += 1

        # encode coefficients
        self.pos = 0
        for i in range(6):
            self.write_data(coeffs[i], 2)

        return coeffs

    def fit_load_func(self, load_points, envelope):
        load_points_scaled = load_points * 256
        load_points_scaled[load_points_scaled > 255] = 255
        dep_data = np.vstack([(self.contrast_data, self.color_data)])
        popt, _ = sp.optimize.curve_fit(BatteryCalibration.load_func, dep_data,
                                        load_points_scaled.T)

        coeffs = self.encode_load_coeffs(popt)

        total = 0
        max_err = 0.0
        total_err = 0.0
        err_mat = np.zeros((CALIB_CONTRAST_COUNT, CALIB_COLOR_COUNT))
        all_contrasts = np.linspace(0, 255, CALIB_CONTRAST_COUNT, dtype=np.uint8)
        all_colors = np.linspace(0, 15, CALIB_COLOR_COUNT, dtype=np.uint8)
        for i, contrast in enumerate(all_contrasts):
            for j, color in enumerate(all_colors):
                pts = load_points_scaled[(self.contrast_data == contrast) &
                                         (self.color_data == color)]
                if len(pts) == 0:
                    err_mat[i, j] = np.nan
                    continue

                load_act = round(np.average(pts))
                load_est = self.load_func_int(contrast, color, coeffs)
                err = (load_est - load_act) / 255
                max_err = max(abs(err), max_err)
                total_err += err
                total += 1
                err_mat[i, j] = err * 100

        print(f"Load polynomial coefficients: " + ', '.join(str(c) for c in coeffs))
        print(f"Load estimate error analysis: max={max_err:.1%}, avg={total_err / total:+.1%}")

        # show error between estimated load and actual load by contrast and color.
        plt.figure()
        plt.imshow(err_mat, extent=[0, 16, 0, 256], aspect=1 / 16)
        plt.title("Load estimation error (%)")
        plt.xlabel("Display color")
        plt.ylabel("Display contrast")
        plt.xticks(np.linspace(0, 16, CALIB_COLOR_COUNT + 1))
        plt.yticks(np.linspace(0, 256, CALIB_CONTRAST_COUNT + 1))
        plt.colorbar()

        # illustrate relation between estimated load and voltage deviation in discharge curve.
        # this is expected to be relatively linear as it's the hypothesis made when estimating
        # battery level, linear interpolation is done between envelope levels as function of load.
        plt.figure()
        flat_disch = np.zeros(self.n)
        load_points_est = np.zeros(self.n)
        for i in range(self.n):
            t = round(self.time_data[i] / TIME_GRANULARITY)
            flat_disch[i] = (self.adc_data[i] - (envelope[t, 0] + envelope[t, 1]) / 2) * 6.93592e-5
            load_points_est[i] = BatteryCalibration.load_func_int(
                self.contrast_data[i], self.color_data[i], coeffs) / 255
        plt.plot(load_points_est, flat_disch, "g.", ms=1)
        plt.title("Voltage deviation from average by load")
        plt.xlabel("Load factor")
        plt.ylabel("Voltage deviation (V)")
        plt.grid()

        return coeffs

    def sample_envelope(self, envelope):
        # sample points along upper and lower envelope levels
        sample_time = np.int32(np.round(np.linspace(
            0, round(self.max_time / TIME_GRANULARITY - 1), BATTERY_SAMPLES)))
        samples = np.uint16(np.round(envelope[sample_time, :]))

        plt.figure()
        for i in range(2):
            plt.plot(np.linspace(100, 0, BATTERY_SAMPLES), samples[:, i], "k-o")
        plt.title("Points used to estimate battery percent")
        plt.xlabel("Battery level (%)")
        plt.ylabel("ADC reading")
        plt.gca().invert_xaxis()
        plt.grid()

        # encode discharge curve
        self.pos = 14
        for i in range(BATTERY_SAMPLES):
            up, lo = samples[i, :]
            extent = up - lo
            self.write_data(lo, 2)
            self.write_data(extent, 2)

        return samples

    @staticmethod
    def estimate_battery_percent(coeffs, points, adc: int, contrast: int, color: int) -> float:
        # an equivalent of this function is implemented in sys/power
        load = BatteryCalibration.load_func_int(contrast, color, coeffs)
        last_middle = None
        level = 256 - 256 / BATTERY_SAMPLES
        for i in range(BATTERY_SAMPLES):
            up, lo = points[i, :]
            extent = up - lo
            middle = lo + extent * (255 - load) // 256
            diff = adc - middle
            if diff > 0:
                if i == 0:
                    # over 100%
                    return 255
                # in between two sample points, interpolate
                return diff * (256 / BATTERY_SAMPLES) // (last_middle - middle) + level
            last_middle = middle
            level -= 256 / BATTERY_SAMPLES
        # below 0%
        return 0

    def compare_battery_percent(self, coeffs, points):
        percent_est = np.zeros(self.n)
        percent_act = np.zeros(self.n)
        for i in range(self.n):
            percent_est[i] = BatteryCalibration.estimate_battery_percent(
                coeffs, points, self.adc_data[i], self.contrast_data[i], self.color_data[i]) / 255
            percent_act[i] = 1 - self.time_data[i] / self.max_time

        all_diff = percent_est - percent_act

        # do a least square regression through difference data and find
        # the standard deviation using the fit line as the average.
        # The standard deviation is the interesting value here, it indicates
        # how much the estimated percentage is expected to vary between different
        # loads at the same battery level, and thus decides of the percent granularity.
        a = np.vstack([self.time_data, np.ones(self.n)]).T
        m, b = np.linalg.lstsq(a, all_diff, rcond=None)[0]
        variance = 0
        for i in range(self.n):
            variance += (all_diff[i] - (m * self.time_data[i] + b)) ** 2
        stddev = math.sqrt(variance / self.n)
        fit_line = m * self.time_data + b

        plt.figure()
        plt.plot(self.time_data, all_diff * 100, "c.", markersize=1)
        plt.plot(self.time_data, fit_line * 100, "k-")
        plt.title("Difference between estimated and actual battery percentage")
        plt.ylabel("Difference (%)")
        plt.xlabel("Time (min)")
        plt.grid()

        print(f"Battery percent comparison: "
              f"avg={np.average(all_diff):+.2%}, "
              f"min={np.min(all_diff):+.2%}, "
              f"max={np.max(all_diff):+.2%}, "
              f"stddev={stddev:.2%}")

        # correct using the fit line
        mc = 1 + m * self.max_time
        mb = -(m * self.max_time + b)
        all_diff_corrected = (percent_est * mc + mb) - percent_act

        plt.figure()
        plt.plot(self.time_data, all_diff_corrected * 100, "y.", markersize=1)
        m, b = np.linalg.lstsq(a, all_diff_corrected, rcond=None)[0]
        plt.plot(self.time_data, (m * self.time_data + b) * 100, "k-")
        plt.title("Battery percentage difference with 1st order correction")
        plt.ylabel("Difference (%)")
        plt.xlabel("Time (min)")
        plt.grid()

        print(f"First order correction: P' = {mc:.3f}*P {mb * 100:+.1f}")

        # write data for correction
        mcb = round((mc - 1) * 256)  # Q1.7
        mbb = round(mb * 256)  # Q8.0
        if not (-128 <= mcb <= 127 and -128 <= mbb <= 127):
            BatteryCalibration._warn("overflow in 1st order correction coefficients.")
        self.pos = 12
        self.write_data(mcb, 1)
        self.write_data(mbb, 1)

        return all_diff


def main() -> None:
    args = parser.parse_args()
    calib = BatteryCalibration(args.input_file, args.show_plots)
    data = calib.calibrate()
    with open(args.output_file, "wb") as file:
        file.write(data)


if __name__ == '__main__':
    main()
