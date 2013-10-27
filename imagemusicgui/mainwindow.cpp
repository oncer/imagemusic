#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QFile>
#include <QtMath>
#include <QElapsedTimer>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    convert("IMG_5056.JPG", "IMG_5056.wav");
    //convert("test1.png", "test1.wav");
    //convert("test2.png", "test2.wav");
    //convert("mthsmeisterwerk2.png", "mthsmeisterwerk.wav");
    //convert("test3.png", "test3.wav");
    //convert("test4.png", "test4.wav");
}

MainWindow::~MainWindow()
{
    delete ui;
}
#include <QDebug>
#define PI (3.1415926535897932384626433832795)
#define PI2 (6.283185307179586476925286766559)

struct SineOscil
{
    qreal* table;
    int tablesize;
    int multi;
    int srate;
    qreal frequency;
    SineOscil(){}
    SineOscil(qreal frequency, int srate=44100)
    {
        this->frequency = frequency;
        this->srate = srate;
        multi = 1;
        tablesize = srate / frequency;
        while (tablesize < 1000) {
            multi *= 2;
            tablesize = srate * multi / frequency;
        }
        table = new qreal[tablesize];
        for (int i = 0; i < tablesize; i++) {
            table[i] = qSin(i * multi * PI2 / tablesize); // sinus wave
            //table[i] = (i % (tablesize / multi) < (tablesize / multi / 2)) ? -1.0 : 1.0; // square wave
            //table[i] = (i % (tablesize / multi)) * 2.0 / (tablesize / multi) - 1.0; // sawtooth wave
            //table[i] = (qreal)(i) / (qreal)tablesize;
        }
    }

    qreal value(int sample)
    {
        return table[sample % tablesize];
    }
};

void MainWindow::convert(const QString &imageFile, const QString &wavFile)
{
    QImage img(imageFile);
    Q_ASSERT(!img.isNull());
    // frequency table
    const int NUM_NOTES = 108;
    qreal freqtable[NUM_NOTES];
    for (int i = 0; i < NUM_NOTES; i++) {
        freqtable[i] = 440.0 * pow(2.0, (i - 45) / 12.0);
    }

    const int NUM_CONFIG = 63;
    SineOscil noteconfig[NUM_CONFIG];
    for (int i = 0; i < NUM_CONFIG; i++) {
        const int durOffsets[] = {0,2,4,5,7,9,11};
        const int mollOffsets[] = {0,2,3,5,7,8,10};
        int note = (i / 7) * 12 + durOffsets[i % 7];
        //int note = (i / 7) * 12 + mollOffsets[i % 7];
        qDebug() << note;
        noteconfig[i] = SineOscil(freqtable[note]);
    }

    int* track = new int[NUM_CONFIG * img.width()];
    for (int i = 0; i < NUM_CONFIG * img.width(); i++) {
        track[i] = 0;
    }
    qreal rfImageHeight = 1.0 / (qreal)img.height();

    qDebug() << "build track";
    for (int y = 0; y < img.height(); y++) {
        for (int x = 0; x < img.width(); x++) {
            QColor c = QColor::fromRgb(img.pixel(x, y));
            int row = (c.red() + c.green() + c.blue()) * NUM_CONFIG / (256 * 3);
            track[row + x * NUM_CONFIG] += 1;
        }
    }

    QElapsedTimer timer;
    qDebug() << "write audio";
    int songLength = 30;
    int sampleRate = 44100;
    int totalSamples = songLength * sampleRate;
    qreal* audio = new qreal[totalSamples];
    timer.start();
    for (int i = 0; i < totalSamples; i++) {
        qreal f = (qreal)i * (qreal)(img.width() - 1) / (qreal)totalSamples;
        qreal frac = f - std::floor(f);
        audio[i] = 0.0;
        int* trackptr0 = &track[(int)(f) * NUM_CONFIG];
        int* trackptr1 = &track[(int)(f + 1.0) * NUM_CONFIG];
        for (int j = 0; j < NUM_CONFIG; j++) {
            //qreal val = qSin(i * noteconfig[j].frequency * PI2 / sampleRate);
            qreal val = noteconfig[j].value(i);
            audio[i] += val * trackptr0[j] * (1.0 - frac);
            audio[i] += val * trackptr1[j] * frac;
        }
        audio[i] *= rfImageHeight;
        if (i % 44100 == 0) qDebug() << i / 44100.0 << " seconds";
    }
    qDebug() << "Time:" << timer.elapsed();

    delete[] track;

    qint16* iAudio = new qint16[totalSamples];
    for (int i = 0; i < totalSamples; i++) {
        qint32 sample = audio[i] * 32767.0;
        sample = std::min(32767, std::max(-32768, sample));
        iAudio[i] = (qint16) sample;
    }
    delete[] audio;

    qDebug() << "write wav file";
    QFile f(wavFile);
    f.open(QIODevice::WriteOnly);
    f.write("RIFF", 4);
#define WRITE16(x) { qint16 Number16 = (x); f.write((const char*)&Number16, 2); }
#define WRITE32(x) { qint32 Number32 = (x); f.write((const char*)&Number32, 4); }
    WRITE32(36 + totalSamples * 2);
    f.write("WAVEfmt ", 8);
    WRITE32(16);
    WRITE16(1); // PCM format
    WRITE16(1); // Number of channels
    WRITE32(sampleRate);
    WRITE32(sampleRate * 1 * 2); // Byte rate
    WRITE16(2); // Frame size
    WRITE16(16); // Bits per sample
    f.write("data", 4);
    WRITE32(totalSamples * 2);
    f.write((const char*)iAudio, totalSamples * 2);
    f.close();

    delete[] iAudio;
}
