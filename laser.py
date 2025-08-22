from ctypes import c_int, c_float, POINTER, CDLL
import os, numpy as np, platform
import pickle
from typing import Optional
from rich import print
from math import gcd, isclose
from functools import reduce
from tqdm import tqdm

if platform.system() == "Windows":
    from ctypes import WinDLL
    DLL = WinDLL
else:
    from ctypes import CDLL
    DLL = CDLL

class Laser:

    str_to_int = {'X': 0, 'Y': 1, 'R': 2, 'G': 3, 'B': 4, 'XOFF': 5, 'YOFF': 6, 'ROTATE': 7}

    def __init__(self, dll_path: str = f"{os.getcwd()}/laser.dll"):
        if not os.path.exists(dll_path):
            raise FileNotFoundError(f"Cannot find DLL: {dll_path}")
        
        with open('chords', 'rb') as fp:
            self.chords = pickle.load(fp)
            self.chords = [[j for j in i if j < 900] for i in self.chords]
        
        self.lib = DLL(dll_path)
        self._handle = self.lib._handle  # for proper unloading if needed
        self._init_bindings()
        self.init_serial()

    @staticmethod
    def softmax_normalize_by_label(data):

        if len(data) == 1:
            return data
        color = [i for i in data if i[0] not in 'XY']
        data  = [i for i in data if i[0] in     'XY']

        # Separate entries by label
        grouped = {'X': [], 'Y': []}
        for row in data:
            grouped[row[0]].append(row)

        result = []

        for label, rows in grouped.items():
            weights = np.array([r[2] for r in rows], dtype=np.float64)
            exp_weights = np.exp(weights - np.max(weights))  # For numerical stability
            softmax = exp_weights / exp_weights.sum() / 1.414

            # Update original rows with normalized weights
            for i, row in enumerate(rows):
                result.append([row[0], row[1], softmax[i]])
        return (result + color) if len(color) else result

    def _random_color(self, max_val=32):
        rand = np.random.randint
        if max_val > 32: max_val = 32
        arr = [['G', rand(14, max_val)], ['R', rand(5, max_val)], ['B', rand(7, max_val)]]
        if arr[0][1] + arr[1][1] + arr[2][1] - 26 < 3:
            return self._random_color(max_val)
        return arr

    def random_chord(self, num_tones: Optional[int] = None, chord_group: Optional[int] = None):

        num_hzs = np.random.randint(4, 8) if num_tones is None else num_tones
        chord_nums = [n for n, i in enumerate(self.chords) if len(i) > 3] if chord_group is None else [chord_group]
        chord = np.random.choice(chord_nums)
        stuff = [np.random.choice(self.chords[chord]) for i in range(num_hzs)]

        arr = []
        for n, hz in enumerate(stuff):
            arr.append(['X' if n % 2 == 0 else 'Y', hz + np.random.rand()*.19, np.random.rand()])

        arr += [['XOFF', round((1 - 1/1.414) * 4096 / 2)], ['YOFF', round((1 - 1/1.414) * 4096 / 2)]]
        return self.softmax_normalize_by_label(arr)
    
    def off(self):
        self.send([['R', 0], ['B', 0], ['G', 0]])

    def _init_bindings(self):
        self.lib.send_to_laser.argtypes = [
            c_int,
            POINTER(c_float),
            POINTER(c_int),
            c_int
        ]
        self.lib.send_to_laser.restype = None

        self.lib.square.argtypes = [
            c_int,
            c_int,
            c_int,
            c_int
        ]
        self.lib.square.restype = None

    def init_serial(self):
        arr_np = np.ascontiguousarray([0], dtype=np.float32)
        types_np = np.ascontiguousarray([0], dtype=np.int32)

        arr_ptr = arr_np.ctypes.data_as(POINTER(c_float))
        types_ptr = types_np.ctypes.data_as(POINTER(c_int))

        self.lib.send_to_laser(0, arr_ptr, types_ptr, 1)

    def show(self, arr: list, amp=16, seconds=1, first=True, rotation_count=0):
        self.send(arr + [['G', amp], ['R', amp], ['B', amp]], first=first)
        seconds = round(seconds * 156)
        if rotation_count != 0:
            rotation_count = 2 * np.pi * rotation_count / seconds
            for i in range(seconds):
                self.send(arr + [['G', amp], ['R', amp], ['B', amp], ['ROTATE', i * rotation_count, 2048, 2048]], first=False)
        else:
            for i in range(seconds):
                self.send(arr + [['G', amp], ['R', amp], ['B', amp]], first=False)
        self.send([['R', 0], ['B', 0], ['G', 0]])

    def show_with_color_DEV(self, arr: list, r: int, g: int, b: int, r2: int, g2: int, b2: int, seconds=1, first = True):
        # sweeps through color intensity
        self.send(arr + [['G', 0], ['R', 0], ['B', 0]], first=first)
        seconds = round(seconds * 156)
        for i in range(seconds):

            self.send(arr + [
                ['G', (i * (g2 - g) / seconds) + g], 
                ['R', (i * (r2 - r) / seconds) + r], 
                ['B', (i * (b2 - b) / seconds) + b]], 
                first=False)
        self.send([['R', 0], ['B', 0], ['G', 0]])

    def send(self, arr: list[float] | dict, types: list[int] = None, first=True):
        if types is None:
            arr2 = []
            for i in arr:
                if i[0] in ['X', 'Y', 'ROTATE']:
                    arr2.extend(list(i[1:]))
                else:
                    arr2.append(i[1]) 
            arr_np = np.ascontiguousarray(arr2, dtype=np.float32)
            types_np = np.ascontiguousarray([self.str_to_int[i[0]] for i in arr], dtype=np.int32)
            num_types = len(arr)
        else:
            arr_np = np.ascontiguousarray(arr, dtype=np.float32)
            types_np = np.ascontiguousarray(types, dtype=np.int32)
            num_types = len(types)

        arr_ptr = arr_np.ctypes.data_as(POINTER(c_float))
        types_ptr = types_np.ctypes.data_as(POINTER(c_int))

        self.lib.send_to_laser(num_types, arr_ptr, types_ptr, int(first))

    def show_many(self, arr: list, seconds=0, tranistion=200, with_random_rotation=True):
        for i in arr:
            rgb = self._random_color()

            seconds = round(seconds * 159)

            rotation_increment, rot, extra_offset, new_center = 0, 0, False, round((1 - 1/1.414) * 4096 / 2)
            if with_random_rotation:
                total_rotation = (np.random.rand() * .5 - .25) * 2 * np.pi
                rotation_increment = total_rotation / (tranistion * 2 + seconds)
                rot = 0
                if sum([j[2] for j in i if j[0] == 'X']) > 0.9 or sum([j[2] for j in i if j[0] == 'Y']) > 0.9:
                    for j in range(len(i)): i[j][2] /= 1.414 
                i += [['XOFF', new_center], ['YOFF', new_center]]
                extra_offset = True
            self.send([['X', 1, 0], ['Y', 1, 0], ['XOFF', 2048], ['YOFF', 2048], *rgb], first=True)

            def grow_and_send(j, off, rot):
                j = np.sin(j)
                off = (1 - j) * 2048 / (1.414 if extra_offset else 1)
                offset = [['XOFF', off + new_center], ['YOFF', off + new_center]] if extra_offset else [['XOFF', off], ['YOFF', off]]
                self.send([[k[0], k[1], k[2] * j] for k in i if k[0] in 'XY'] + [*offset, *rgb, ['ROTATE', rot, 2048, 2048]], first=False)
                rot += rotation_increment
                return rot

            for j, off in zip(np.linspace(0, np.pi/2, tranistion), np.linspace(np.pi/2, 0, tranistion)):
                rot = grow_and_send(j, off, rot)

            for j in range(seconds):
                self.send(i + rgb + [['ROTATE', rot, 2048, 2048]], first=False)
                rot += rotation_increment

            for j, off in zip(np.linspace(np.pi/2, 0, tranistion), np.linspace(0, np.pi/2, tranistion)):
                rot = grow_and_send(j, off, rot)


    def calibrate_square(self, amp, delay, time=4):
        for _ in range(round(time/2.1)):
            self.lib.square(amp, amp, amp, delay)
        self.off()

    def make_chords_DEV(self):

        def float_gcd(a: float, b: float, scale: float = 1e6) -> float:
            """Approximate GCD for floats using integer scaling."""
            a_int, b_int = round(a * scale), round(b * scale)
            return gcd(a_int, b_int) / scale

        def multi_float_gcd(freqs, scale: float = 1e6) -> float:
            """GCD of multiple float frequencies."""
            return reduce(lambda x, y: float_gcd(x, y, scale), freqs)

        def common_cycle_time(*hz_values, scale: float = 1e6) -> float:
            """Returns the time (in seconds) until all input frequencies align in phase."""
            if len(hz_values) == 1 and hasattr(hz_values[0], '__iter__'):
                hz_values = list(hz_values[0])

            hz_values = [f for f in hz_values if not isclose(f, 0)]
            if not hz_values:
                return 0.0

            g = multi_float_gcd(hz_values, scale)
            return 1 / g

        min_cycle_time = 0.015

        start = round(1/min_cycle_time)
        stop = 1200
        hzs = np.arange(start, stop, .25)

        chords = []
        for i in tqdm(hzs):
            arr = [i]
            for j in hzs:
                if j not in arr and common_cycle_time(*arr, j) < min_cycle_time:
                    arr.append(j)
            chords.append(sorted(arr))

        with open('chords', 'wb') as fp:
            pickle.dump(chords, fp)

    def _shutdown(self):
        self.lib = None
        self._handle = None

    def __del__(self):
        self._shutdown()



if __name__ == '__main__':
    las = Laser()
    d = 10
    for i in range(1000):
        las.send([['X', 200, .3], ['Y', 200.2, .3], ['XOFF', 1000], ['YOFF', 1000], ['R', d], ['B', d], ['G', d], ['ROTATE', i / 1000 * np.pi * 2, 2048, 2048]], first=False)
    las.send([['X', 200, 0], ['Y', 200.2, 0], ['XOFF', 0], ['YOFF', 0], ['R', 0], ['B', 0], ['G', 0]], first=False)