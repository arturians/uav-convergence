#include "MainWindow.h"
#include "ui_MainWindow.h"

#include <QFile>
#include <QFileDialog>
#include <QPainter>
#include <QtMath>

#include "MainPresenter.h"
#include "DataModel.h"

MainWindow::MainWindow(QWidget* parent) :
    QMainWindow(parent)
    ,ui(new Ui::MainWindow)
    ,presenter(nullptr)
    ,model(nullptr)
    ,telemetry()
    ,airplanePixmap()
    ,yawScalePixmap()
    ,isReachedDistinguish(false)
    ,convergenceTelemetries()
{
    ui->setupUi(this);
    model = new DataModel(this);
    presenter = new MainPresenter(model, this, this);
    airplanePixmap.load(":/images/airplane.png");
    yawScalePixmap.load(":/images/yaw_scale.png");
}

void MainWindow::setEnabledFileLoading(bool enable)
{
    ui->loadButton->setEnabled(enable);
}

void MainWindow::setEnabledPlayingTelenetry(bool enable)
{
    ui->playButton->setEnabled(enable);
}

void MainWindow::setEnabledStopPlayingTelemetry(bool enable)
{
    ui->stopButton->setEnabled(enable);
}

void MainWindow::showTelemetry(const Telemetry& telemetry, const Telemetry& convergenceTelemetry)
{
    this->telemetry = telemetry;
    if (convergenceTelemetry.isConvergenceDataExist)
    {
        if (convergenceTelemetry.packetId >= 0)
        {
            convergenceTelemetries.push_back(convergenceTelemetry);
        }
        if (convergenceTelemetries.size() > 20)
        {
            float delta = qAbs(convergenceTelemetries.first().magneticYaw
                               - convergenceTelemetries.last().magneticYaw);
            const float distinguishValue = 15.0f;
            bool minimalDistinguish = delta < distinguishValue;
            if ( ! minimalDistinguish)
            {
                float less = convergenceTelemetries.first().magneticYaw;
                float more = convergenceTelemetries.last().magneticYaw;
                if (less > more)
                {
                    std::swap(less, more);
                }
                less += 360.0f;
                float newDelta = less - more;
                minimalDistinguish = newDelta < distinguishValue;
            }
            if (minimalDistinguish)
            {
                convergenceTelemetries.pop_front();
            }
        }
    }
    repaint();
}

void MainWindow::showProgress(int progress)
{
    ui->progressBar->setValue(progress);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    printTelemetry();
    QRectF drawingArea = getDrawingArea();
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    drawYawScale(painter, drawingArea.center());
    drawAirplane(painter, drawingArea.center());
    drawYaw(painter, drawingArea.center(), yawScalePixmap.width() / 2.0);
    drawConvergenceSpeed(painter, drawingArea.center(), yawScalePixmap.width() / 2.0);
}

void MainWindow::resizeEvent(QResizeEvent* event)
{
    QMainWindow::resizeEvent(event);
}

void MainWindow::on_loadButton_clicked()
{
    QString fileName = QFileDialog::getOpenFileName(this, tr("Выбрать файл"), "E:\\",
                                                    tr("Текстовый файл (*.txt)"));
    emit fileSelected(fileName);
}

void MainWindow::on_playButton_clicked()
{
    emit needStartPlaying();
}

void MainWindow::on_stopButton_clicked()
{
    emit needStopPlaying();
}

QRectF MainWindow::getDrawingArea() const
{
    int height = ui->verticalSpacer->geometry().height();
    int width = this->width();
    qreal sideLength = height > width ? width : height;
    qreal halfSide = sideLength / 2.0;
    QPoint center = ui->verticalSpacer->geometry().center();
    QPointF topLeft(center.x() - halfSide, center.y() - halfSide);
    return QRectF(topLeft.x(), topLeft.y(), sideLength, sideLength);
}

void MainWindow::drawYawScale(QPainter& painter, const QPointF& center)
{
    QPointF topLeft(center.x() - yawScalePixmap.width() / 2.0,
                    center.y() - yawScalePixmap.height() / 2.0);
    painter.drawPixmap(topLeft, yawScalePixmap);
}

void MainWindow::drawAirplane(QPainter& painter, const QPointF& center)
{
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.translate(center);
    painter.rotate(telemetry.yaw);
    painter.drawPixmap(QPointF(-airplanePixmap.width() / 2.0, -airplanePixmap.height() / 2.0),
                        airplanePixmap);
    painter.resetTransform();
}

void MainWindow::drawYaw(QPainter& painter, const QPointF& center, qreal radius)
{
    painter.save();
    painter.setPen(QPen(Qt::red, 2));
    float yaw = telemetry.yaw - 90.0f;
    QPointF end(center.x() + cos(qDegreesToRadians(yaw)) * radius,
                center.y() + sin(qDegreesToRadians(yaw)) * radius);
    painter.drawLine(center, end);
    painter.restore();
}

void MainWindow::drawConvergenceSpeed(QPainter& painter, const QPointF& center, qreal radius)
{
    if (convergenceTelemetries.size() < 2)
    {
        return;
    }
    painter.save();

    QPointF previousPoint(center);
    bool isPreviuosPositive(true);
    QPen positivePen(Qt::green, 2);
    QPen negativePen(Qt::red, 2);
    painter.setPen(positivePen);
    foreach (auto telemetry, convergenceTelemetries)
    {
        qreal length = telemetry.convergenceRatio * radius;
        QPointF end(center.x() + cos(qDegreesToRadians(telemetry.magneticYaw)) * length,
                    center.y() + sin(qDegreesToRadians(telemetry.magneticYaw)) * length);
        bool isCurrentPositive = telemetry.convergenceSpeed > 0.0;
        if (isCurrentPositive != isPreviuosPositive)
        {
            if (isCurrentPositive)
            {
                painter.setPen(positivePen);
            }
            else
            {
                painter.setPen(negativePen);
            }
        }
        isPreviuosPositive = isCurrentPositive;
        painter.drawLine(previousPoint, end);
        previousPoint = end;
    }

    painter.restore();
}

void MainWindow::printTelemetry()
{
    Telemetry convergenceTelem = convergenceTelemetries.count() > 0 ? convergenceTelemetries.last() : Telemetry();
//    QString::number(telemetry.yaw, 'f', 2) +
    ui->yawLabel->setText(getString(telemetry.yaw, convergenceTelem.yaw));
    ui->magneticYawLabel->setText(getString(telemetry.magneticYaw, convergenceTelem.magneticYaw));
    ui->directionLabel->setText(getString(telemetry.direction, convergenceTelem.direction));
    ui->gscDistanceLabel->setText(getString(telemetry.gcsDistance, convergenceTelem.gcsDistance));
    ui->timeLabel->setText(getString(telemetry.time, convergenceTelem.time));
    ui->navigationModeLabel->setText(getNavigationModeDescription(telemetry.navigationMode)
                                     + " | "
                                     + getNavigationModeDescription(convergenceTelem.navigationMode));
    ui->airSpeedLabel->setText(getString(telemetry.airSpeed, convergenceTelem.airSpeed));
    ui->convergenceSpeedLabel->setText(getString(telemetry.convergenceSpeed, convergenceTelem.convergenceSpeed));
    ui->ratioSpeedLabel->setText(getString(telemetry.convergenceRatio, convergenceTelem.convergenceRatio));
    ui->latitudeLabel->setText(getString(telemetry.latitude, convergenceTelem.latitude, 4));
    ui->longitudeLabel->setText(getString(telemetry.longitude, convergenceTelem.longitude, 4));
    ui->packetIdLabel->setText(getString(telemetry.packetId, convergenceTelem.packetId));
}

QString MainWindow::getString(double v1, double v2, int presition)
{
    return QString("%1 | %2").arg(v1, 5, 'f', presition, QChar('0')).arg(v2, 5, 'f', presition, QChar('0'));
}

QString MainWindow::getString(qint64 v1, qint64 v2)
{
    return QString("%1 | %2").arg(v1).arg(v2);
}

QString MainWindow::getString(int v1, int v2)
{
    return QString("%1 | %2").arg(v1).arg(v2);
}

QString MainWindow::getNavigationModeDescription(int mode)
{
    QString navigationMode;
    switch (mode)
    {
    case 0:
        navigationMode = "GPS only";
        break;
    case 1:
        navigationMode = "Auto";
        break;
    case 2:
        navigationMode = "IMU only";
        break;
    case 3:
        navigationMode = "Binding";
        break;
    default:
        navigationMode = "Unknown";
        break;
    }
    return navigationMode;
}
