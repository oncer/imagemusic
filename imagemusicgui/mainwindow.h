#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#define PI (3.1415926535897932384626433832795)
#define PI2 (6.283185307179586476925286766559)
#include <QtMath>
#include <QGraphicsScene>
#include <QFutureWatcher>

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT
    
public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

    void saveWavFile(const QString& wavFile);
    
private slots:
    void on_noteList_customContextMenuRequested(const QPoint &pos);

    void on_addNoteButton_clicked();

    void on_imageView_customContextMenuRequested(const QPoint &pos);

    void on_actionDelete_Note_triggered();

    void on_saveButton_clicked();

    void slot_finished();

private:
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

    static const int NUM_NOTES = 108;
    qreal freqtable[NUM_NOTES];
    QList<SineOscil> noteConfig;

    Ui::MainWindow *ui;
    QGraphicsScene *scene;
    QImage imageFile;
    QFutureWatcher<void> futureWatcher;
};

#endif // MAINWINDOW_H
