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

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    ui->splitter->setStretchFactor(0, 5);
    ui->splitter->setStretchFactor(1, 2);
    ui->noteList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    ui->imageView->setContextMenuPolicy(Qt::CustomContextMenu);
    ui->noteList->setContextMenuPolicy(Qt::CustomContextMenu);
    ui->progressBar->hide();
    connect(&this->futureWatcher, SIGNAL(finished()), this, SLOT(slot_finished()));
    for (int i = 0; i < NUM_NOTES; i++) {
        freqtable[i] = 440.0 * pow(2.0, (i - 45) / 12.0);
        const char* noteNames[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
        QString noteName = QString("%1-%2\t%3 Hz").arg(noteNames[i % 12]).arg(i / 12 + 1).arg(freqtable[i]);
        ui->noteCombo->addItem(noteName, i);
        QListWidgetItem* listItem = new QListWidgetItem(noteName, ui->noteList);
        listItem->setData(Qt::UserRole, i);
    }
    scene = new QGraphicsScene;
    scene->addText("Right-click to load image.");
    ui->imageView->setScene(scene);
    ui->imageView->show();
}

MainWindow::~MainWindow()
{
    delete ui;
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
        for (int x = 0; x < imageFile.width(); x++) {
            QColor c = QColor::fromRgb(imageFile.pixel(x, y));
            int row = (c.red() + c.green() + c.blue()) * noteConfig.size() / (256 * 3);
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
    for (int i = 0; i < totalSamples; i++) {
        qreal f = (qreal)i * (qreal)(imageFile.width() - 1) / (qreal)totalSamples;
        qreal frac = f - std::floor(f);
        audio[i] = 0.0;
        int* trackptr0 = &track[(int)(f) * noteConfig.size()];
        int* trackptr1 = &track[(int)(f + 1.0) * noteConfig.size()];
        for (int j = 0; j < noteConfig.size(); j++) {
            //qreal val = qSin(i * noteconfig[j].frequency * PI2 / sampleRate);
            qreal val = noteConfig[j].value(i);
            audio[i] += val * trackptr0[j] * (1.0 - frac);
            audio[i] += val * trackptr1[j] * frac;
        }
        audio[i] *= rfImageHeight;
        if (i % 1024 == 0) ui->progressBar->setValue(i);
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

void MainWindow::on_noteList_customContextMenuRequested(const QPoint &pos)
{
    QMenu contextMenu(this);
    contextMenu.addAction(ui->actionDelete_Note);
    contextMenu.exec(ui->noteList->mapToGlobal(pos));
}


void MainWindow::on_addNoteButton_clicked()
{
    QListWidgetItem* item = new QListWidgetItem(ui->noteCombo->itemText(ui->noteCombo->currentIndex()));
    int note = ui->noteCombo->itemData(ui->noteCombo->currentIndex(), Qt::UserRole).toInt();
    item->setData(Qt::UserRole, note);
    int row = 0;
    while (ui->noteList->item(row)->data(Qt::UserRole).toInt() <= note) {
        row++;
    }
    ui->noteList->insertItem(row, item);
}

void MainWindow::on_imageView_customContextMenuRequested(const QPoint &pos)
{
    Q_UNUSED(pos);
    QString filename = QFileDialog::getOpenFileName(this, tr("Open Image"), ".", tr("Image Files (*.jpg *.png)"));
    if (!filename.isNull()) {
        imageFile = QImage(filename);
        scene->clear();
        scene->addPixmap(QPixmap::fromImage(imageFile));
        ui->imageView->setScene(scene);
        ui->imageView->fitInView(imageFile.rect(), Qt::KeepAspectRatio);
        ui->imageView->show();
        ui->saveButton->setEnabled(true);
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
    QString filename = QFileDialog::getOpenFileName(this, tr("Save as"), ".", tr("WAV Files (*.wav)"));

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
