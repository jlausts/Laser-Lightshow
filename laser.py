from ctypes import c_int, c_float, POINTER, CDLL
import os, numpy as np, platform
import pickle
from typing import Optional
from rich import print

if platform.system() == "Windows":
    from ctypes import WinDLL
    DLL = WinDLL
else:
    from ctypes import CDLL
    DLL = CDLL

class Laser:

    str_to_int = {'X': 0, 'Y': 1, 'R': 2, 'G': 3, 'B': 4, 'XOFF': 5, 'YOFF': 6, 'ROTATE': 7}

    def __init__(self, dll_path: str = r"C:\Users\jlaus\Documents\Programming\Laser Lightshow\laser.dll"):
        if not os.path.exists(dll_path):
            raise FileNotFoundError(f"Cannot find DLL: {dll_path}")
        
        with open('chords', 'rb') as fp:
            self.chords = pickle.load(fp)
        
        self.lib = DLL(dll_path)
        self._handle = self.lib._handle  # for proper unloading if needed
        self._init_bindings()
        self.init_serial()

    @staticmethod
    def softmax_normalize_by_label(data):

        if len(data) == 1:
            return data
        data = [i for i in data if i[0] not in ['R', 'G', 'B']]
        color = [i for i in data if i[0] in ['R', 'G', 'B']]
        
        # Separate entries by label
        grouped = {'X': [], 'Y': []}
        for row in data:
            grouped[row[0]].append(row)

        result = []

        for label, rows in grouped.items():
            weights = np.array([r[2] for r in rows], dtype=np.float64)
            exp_weights = np.exp(weights - np.max(weights))  # For numerical stability
            softmax = exp_weights / exp_weights.sum()

            # Update original rows with normalized weights
            for i, row in enumerate(rows):
                result.append([row[0], row[1], softmax[i]])
        return (result + [color]) if len(color) else result

    def random_chord(self, num_tones: Optional[int] = None, chord_group: Optional[int] = None):

        num_hzs = np.random.randint(4, 8) if num_tones is None else num_tones
        chord_nums = [n for n, i in enumerate(self.chords) if len(i) > 3] if chord_group is None else [chord_group]
        chord = np.random.choice(chord_nums)
        stuff = [np.random.choice(self.chords[chord]) for i in range(num_hzs)]

        arr = []
        for n, hz in enumerate(stuff):
            arr.append(['X' if n % 2 == 0 else 'Y', hz + np.random.rand()*.19, np.random.rand()])

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

    def init_serial(self):
        arr_np = np.ascontiguousarray([0], dtype=np.float32)
        types_np = np.ascontiguousarray([0], dtype=np.int32)

        arr_ptr = arr_np.ctypes.data_as(POINTER(c_float))
        types_ptr = types_np.ctypes.data_as(POINTER(c_int))

        self.lib.send_to_laser(0, arr_ptr, types_ptr, 1)

    def show(self, arr: list, amp=16, seconds=1, first = True):
        self.send(arr + [['G', amp], ['R', amp], ['B', amp]], first=first)
        for i in range(round(seconds * 159)):
            self.send(arr + [['G', amp], ['R', amp], ['B', amp]], first=False)
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

    def show_many(self, arr: list, amp=16, seconds=2, tranistion=200):
        for i in arr:
            r, g, b = np.random.random(3) * amp + 6

            while r+g+b < 35:
                r, g, b = np.random.random(3) * amp + 6
            rgb_rot = [['G', g], ['R', r], ['B', b]]

            self.send([[k[0], k[1], k[2] * 0] for k in i] + [['XOFF', 2048], ['YOFF', 2048], *rgb_rot], first=True)

            for j, off in zip(np.linspace(0, np.pi/2, tranistion), np.linspace(np.pi/2, 0, tranistion)):
                j = np.sin(j)
                off = (1 - j) * 2048
                self.send([[k[0], k[1], k[2] * j] for k in i] + [['XOFF', off], ['YOFF', off], *rgb_rot], first=False)

            for j in range(round(seconds * 159)):
                self.send(i + [['G', g], ['R', r], ['B', b]], first=False)

            for j, off in zip(np.linspace(np.pi/2, 0, tranistion), np.linspace(0, np.pi/2, tranistion)):
                j = np.sin(j)
                off = (1 - j) * 2048
                self.send([[k[0], k[1], k[2] * j] for k in i] + [['XOFF', off], ['YOFF', off], *rgb_rot], first=False)
        d = 0
        self.send([['X', 0], ['Y', 2000], ['XOFF', 0], ['YOFF', 0], *rgb_rot])

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