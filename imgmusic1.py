from PIL import Image
import math
import wave
import struct

numnotes = 104
pitchtable = [440 * 2**((i - 45) / 12) for i in range(numnotes)]

class SineOscillator:
    """
    Sinus oscillator
    frequency is given in Hz (oscillations per second)
    t is given in seconds
    """
    def __init__(self, frequency):
        self.frequency = frequency

    def value(self, t):
        return math.sin(t * 2 * math.pi * self.frequency)

class Song:
    def __init__(self, width):
        self.track = [[0] * numnotes for i in range(width)]
        self.oscil = [SineOscillator(pitchtable[i]) for i in range(numnotes)]

def interp(v1, v2, frac):
    return v1 * (1 - frac) + v2 * frac

def convert(imgfname, wavfname):
    # open img and convert it to greyscale
    img = Image.open(imgfname).convert('L')
    print("Format",img.format)
    print("Mode",img.mode)
    print("Size",img.size)
    print("Palette",img.palette)

    cols, rows = img.size
    song = Song(cols)
    data = img.getdata()
    for x in range(cols):
        col = [data[x + y * cols] for y in range(rows)]
        for pixel in col:
            song.track[x][pixel*numnotes//256] += 1

    # debug: reduce to 2 cols to test interpolation
    #cols = 2
    #song.track = [song.track[0], song.track[-1]]

    length = 5 # songlength in seconds
    sample_rate = 44100 # samples per second
    total_samples = sample_rate * length
    print("Samples per column",total_samples / cols)
    
    audio = [0] * total_samples
    for i in range(total_samples):
        f = i * (cols-1) / total_samples
        col0 = song.track[math.floor(f)]
        col1 = song.track[math.ceil(f)]
        frac = f - math.floor(f)
        t = i / sample_rate
        for n in range(numnotes):
            val0 = song.oscil[n].value(t) * col0[n]
            val1 = song.oscil[n].value(t) * col1[n]
            audio[i] += interp(val0, val1, frac) / rows
        if i % 44100 == 0: print(i/44100)

    wav = wave.open(wavfname, 'wb')
    wav.setnchannels(1)
    wav.setsampwidth(2)
    wav.setframerate(44100)
    wav.setnframes(total_samples)
    print("writing",total_samples,"samples")
    data = struct.pack('%sh' % total_samples,
                       *[math.floor(s * 32767.0) for s in audio])
    wav.writeframesraw(data)
    wav.close()
    

#convert("IMG_5056.JPG", "output.wav")
#convert("test1.png", "test1.wav")
convert("test2.png", "test2.wav")
