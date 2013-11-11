#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QFile>
#include <QtMath>
#include <QElapsedTimer>
#include <cmath>
#include <QFileDialog>
#include <QMenu>
#include <QDebug>
#include <QtConcurrent/QtConcurrent>

static const char* NoteNames[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    disableRecursion(true)
{
    ui->setupUi(this);
    ui->splitter->setStretchFactor(0, 5);
    ui->splitter->setStretchFactor(1, 2);
    ui->noteList->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->noteList->setIconSize(QSize(16, 16));
    ui->imageView->setContextMenuPolicy(Qt::CustomContextMenu);
    ui->noteList->setContextMenuPolicy(Qt::CustomContextMenu);
    ui->progressBar->hide();
    connect(&this->futureWatcher, SIGNAL(finished()), this, SLOT(slot_finished()));
    connect(this, SIGNAL(progressUpdated(int)), ui->progressBar, SLOT(setValue(int)));
    for (int i = 0; i < NUM_NOTES; i++) {
        freqtable[i] = 440.0 * pow(2.0, (i - 45) / 12.0);
        QString noteName = QString("%1-%2").arg(NoteNames[i % 12]).arg(i / 12 + 1);
        ui->noteFromCombo->addItem(noteName, i);
        ui->noteToCombo->addItem(noteName, i);
    }
    ui->scaleCombo->addItem("Chromatic", 0);
    for (int i = 0; i < 12; i++) {
        QString scaleMajorName = QString("%1 Major").arg(NoteNames[i]);
        ui->scaleCombo->addItem(scaleMajorName, (1<<8) | i);
        QString scaleMinorName = QString("%1 Minor").arg(NoteNames[i]);
        ui->scaleCombo->addItem(scaleMinorName, (2<<8) | i);
    }
    ui->scaleCombo->setCurrentIndex(0);
    ui->noteFromCombo->setCurrentIndex(0);
    ui->noteToCombo->setCurrentIndex(NUM_NOTES - 1);
    scene = new QGraphicsScene;
    disableRecursion = false;
    makeNoteList();
}

MainWindow::~MainWindow()
{
    delete ui;
    delete scene;
}

void MainWindow::saveWavFile(const QString &wavFile)
{
    Q_ASSERT(!imageFile.isNull());

    noteConfig.clear();
    for (int i = 0; i < ui->noteList->count(); i++) {
        int note = ui->noteList->item(i)->data(Qt::UserRole).toInt();
        noteConfig.append(SineOscil(freqtable[note]));
    }


    Q_ASSERT(!noteConfig.empty());

    int* track = new int[noteConfig.size() * imageFile.width()];
    for (int i = 0; i < noteConfig.size() * imageFile.width(); i++) {
        track[i] = 0;
    }
    qreal rfImageHeight = 1.0 / (qreal)imageFile.height();

    qDebug() << "build track";
    for (int y = 0; y < imageFile.height(); y++) {
        QRgb* data = reinterpret_cast<QRgb*>(imageFile.scanLine(y));
        for (int x = 0; x < imageFile.width(); x++) {
            QColor c = QColor::fromRgb(data[x]);
            int r, g, b;
            c.getRgb(&r, &g, &b);
            int row = (r + g + b) * (noteConfig.size() - 1) / (255 * 3);
            track[row + x * noteConfig.size()] += 1;
        }
    }

    QElapsedTimer timer;
    qDebug() << "write audio";
    int songLength = ui->durationSpinBox->value();
    int sampleRate = 44100;
    int totalSamples = songLength * sampleRate;
    qreal* audio = new qreal[totalSamples];
    timer.start();
    ui->progressBar->setRange(0, totalSamples);
    int* trackptr_null = new int[noteConfig.size()];
    memset(trackptr_null, 0, noteConfig.size() * sizeof(int));
    for (int i = 0; i < totalSamples; i++) {
        qreal f = (qreal)i * (qreal)(imageFile.width()) / (qreal)totalSamples;
        qreal frac = f - std::floor(f);
        audio[i] = 0.0;
        int idx = (int)f;
        int* trackptr0 = &track[idx * noteConfig.size()];
        int* trackptr1 = trackptr_null;
        if (idx + 1 < imageFile.width()) {
            trackptr1 = &track[(idx + 1) * noteConfig.size()];
        }
        for (int j = 0; j < noteConfig.size(); j++) {
            //qreal val = qSin(i * noteConfig[j].frequency * PI2 / sampleRate);
            qreal val = noteConfig[j].value(i);
            audio[i] += val * trackptr0[j] * qPow(qCos(frac * PI / 2.0), 2.0);
            audio[i] += val * trackptr1[j] * qPow(qSin(frac * PI / 2.0), 2.0);
            //audio[i] += val * trackptr0[j];
        }
        audio[i] *= rfImageHeight;
        if (i % 1024 == 0) emit progressUpdated(i);
        if (i % 44100 == 0) qDebug() << i / 44100.0 << " seconds";
    }
    delete[] trackptr_null;
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

void MainWindow::on_noteList_customContextMenuRequested(const QPoint &pos)
{
    Q_UNUSED(pos)
    /*QMenu contextMenu(this);
    contextMenu.addAction(ui->actionDelete_Note);
    contextMenu.exec(ui->noteList->mapToGlobal(pos));*/
}

void MainWindow::on_imageView_customContextMenuRequested(const QPoint &pos)
{
    Q_UNUSED(pos);
    QString filename = QFileDialog::getOpenFileName(this, tr("Open Image"), ".", tr("Image Files (*.jpg *.png)"));
    if (!filename.isNull()) {
        imageFile = QImage(filename);
        imageFile.convertToFormat(QImage::Format_ARGB32);
        makeGraphicsScene();
    }
}

void MainWindow::on_actionDelete_Note_triggered()
{
    QList<QListWidgetItem*> list = ui->noteList->selectedItems();
    std::for_each(list.begin(), list.end(), [&](QListWidgetItem* i) {
        delete ui->noteList->takeItem(ui->noteList->row(i));
    });
}

void MainWindow::on_saveButton_clicked()
{
    QString filename = QFileDialog::getSaveFileName(this, tr("Save as"), ".", tr("WAV Files (*.wav)"));

    if (!filename.isNull()) {
        ui->progressBar->show();
        ui->saveButton->setEnabled(false);
        QFuture<void> future = QtConcurrent::run([this,filename](){
            saveWavFile(filename);
        });
        futureWatcher.setFuture(future);
    }
}

void MainWindow::slot_finished()
{
    ui->progressBar->hide();
    ui->saveButton->setEnabled(true);
}

void MainWindow::makeNoteList()
{
    if (disableRecursion) return;
    disableRecursion = true;
    int scaleID = ui->scaleCombo->itemData(ui->scaleCombo->currentIndex()).toInt();
    int fromNote = ui->noteFromCombo->itemData(ui->noteFromCombo->currentIndex()).toInt();
    int toNote = ui->noteToCombo->itemData(ui->noteToCombo->currentIndex()).toInt();
    int scaleNote = scaleID & 0xff;
    int scaleType = scaleID >> 8;
    QList<int> offsets;
    switch (scaleType) {
    case 0: offsets = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}; break; // Chromatic scale
    case 1: offsets = {0, 2, 4, 5, 7, 9, 11}; break; // Major scale
    case 2: offsets = {0, 1, 3, 5, 7, 8, 10}; break; // Minor scale
    }
    while (!offsets.contains((fromNote - scaleNote + 12) % 12)) fromNote++;
    while (!offsets.contains((toNote - scaleNote + 12) % 12)) toNote--;
    ui->noteFromCombo->clear();
    ui->noteToCombo->clear();
    ui->noteList->clear();
    for (int i = 0; i < NUM_NOTES; i++) {
        if (offsets.contains((i - scaleNote + 12) % 12)) {
            QString listName = QString("%1-%2\t%3 Hz").arg(NoteNames[i%12]).arg(i/12 + 1).arg(freqtable[i]);
            QString comboName = QString("%1-%2").arg(NoteNames[i%12]).arg(i/12 + 1);

            ui->noteFromCombo->addItem(comboName, i);
            if (i == fromNote) ui->noteFromCombo->setCurrentIndex(ui->noteFromCombo->count() - 1);
            ui->noteToCombo->addItem(comboName, i);
            if (i == toNote) ui->noteToCombo->setCurrentIndex(ui->noteToCombo->count() - 1);

            if (i >= fromNote && i <= toNote) {
                QListWidgetItem* item = new QListWidgetItem(listName, ui->noteList);
                item->setData(Qt::UserRole, i);
            }
        }
    }
    for (int i = 0; i < ui->noteList->count(); i++) {
        int grey = 0;
        if (ui->noteList->count() != 1) {
           grey = 255 * i / (ui->noteList->count() - 1);
        }
        QImage icon(ui->noteList->iconSize(), QImage::Format_ARGB32);
        icon.fill(QColor::fromRgb(grey, grey, grey));
        ui->noteList->item(i)->setIcon(QIcon(QPixmap::fromImage(icon)));
    }
    makeGraphicsScene();
    disableRecursion = false;
}

void MainWindow::makeGraphicsScene()
{
    scene->clear();
    if (imageFile.isNull()) {
        scene->addText("Right-click to load image.");
        ui->imageView->setScene(scene);
        ui->imageView->show();
    } else {
        QImage editedImage(imageFile);
        int numColors = ui->noteList->count();
        for (int y = 0; y < editedImage.height(); y++) {
            QRgb* data = reinterpret_cast<QRgb*>(editedImage.scanLine(y));
            for (int x = 0; x < editedImage.width(); x++) {
                QColor color(data[x]);
                int r, g, b;
                color.getRgb(&r, &g, &b);
                int colorIdx = (r + g + b) / 3 * (numColors - 1) / 255;
                if (ui->noteList->currentIndex().row() == colorIdx) {
                    editedImage.setPixel(x, y, QColor(Qt::red).rgb());
                } else {
                    int roundedGrey = colorIdx  * 255 / (numColors - 1);
                    editedImage.setPixel(x, y, QColor(roundedGrey, roundedGrey, roundedGrey).rgb());
                }
            }
        }
        scene->addPixmap(QPixmap::fromImage(editedImage));
        ui->imageView->setScene(scene);
        ui->imageView->fitInView(imageFile.rect(), Qt::KeepAspectRatio);
        ui->imageView->show();
        ui->saveButton->setEnabled(true);
    }
}

void MainWindow::on_noteFromCombo_currentIndexChanged(int index)
{
    Q_UNUSED(index)
    makeNoteList();
}

void MainWindow::on_scaleCombo_currentIndexChanged(int index)
{
    Q_UNUSED(index)
    makeNoteList();
}

void MainWindow::on_noteToCombo_currentIndexChanged(int index)
{
    Q_UNUSED(index)
    makeNoteList();
}

void MainWindow::on_noteList_currentRowChanged(int currentRow)
{
    Q_UNUSED(currentRow);
    makeGraphicsScene();
}
