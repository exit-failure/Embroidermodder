#include "mdiwindow.h"
#include "view.h"
#include "statusbar.h"
#include "statusbar-button.h"
#include "object-save.h"
#include "object-path.h"
#include "object-polygon.h"
#include "object-polyline.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMainWindow>
#include <QMdiArea>
#include <QMdiSubWindow>
#include <QStatusBar>
#include <QColor>
#include <QUndoStack>

#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGraphicsItem>

#include "embroidermodder.h"

MdiWindow::MdiWindow(const int theIndex, MainWindow* mw, QMdiArea* parent, Qt::WindowFlags wflags) : QMdiSubWindow(parent, wflags)
{
    mainWin = mw;
    mdiArea = parent;

    myIndex = theIndex;

    fileWasLoaded = false;

    setAttribute(Qt::WA_DeleteOnClose);

    QString aName;
    curFile = aName.asprintf("Untitled%d.dst", myIndex);
    this->setWindowTitle(curFile);

    this->setWindowIcon(QIcon("icons/" + mainWin->getSettingsGeneralIconTheme() + "/" + "app" + ".png"));

    gscene = new QGraphicsScene(0,0,0,0, this);
    gview = new View(mainWin, gscene, this);

    setWidget(gview);

    //WARNING: DO NOT SET THE QMDISUBWINDOW (this) FOCUSPROXY TO THE PROMPT
    //WARNING: AS IT WILL CAUSE THE WINDOW MENU TO NOT SWITCH WINDOWS PROPERLY!
    //WARNING: ALTHOUGH IT SEEMS THAT SETTING INTERNAL WIDGETS FOCUSPROXY IS OK.
    gview->setFocusProxy(mainWin->prompt);

    resize(sizeHint());

    promptHistory = "Welcome to Embroidermodder 2!<br/>Open some of our sample files. Many formats are supported.<br/>For help, press F1.";
    mainWin->prompt->setHistory(promptHistory);
    promptInputList << "";
    promptInputNum = 0;

    curLayer = "0";
    curColor = 0; //TODO: color ByLayer
    curLineType = "ByLayer";
    curLineWeight = "ByLayer";

    // Due to strange Qt4.2.3 feature the child window icon is not drawn
    // in the main menu if showMaximized() is called for a non-visible child window
    // Therefore calling show() first...
    show();
    showMaximized();

    setFocusPolicy(Qt::WheelFocus);
    setFocus();

    onWindowActivated();
}

/**
 * @brief MdiWindow::~MdiWindow
 */
MdiWindow::~MdiWindow()
{
    qDebug("MdiWindow Destructor()");
}

/**
 * @brief MdiWindow::saveFile
 * @param fileName
 * @return
 */
bool
MdiWindow::saveFile(const QString &fileName)
{
    SaveObject saveObj(gscene, this);
    return saveObj.save(fileName);
}

/**
 * @brief MdiWindow::loadFile
 * @param fileName
 * @return
 */
bool
MdiWindow::loadFile(const QString &fileName)
{
    qDebug("MdiWindow loadFile()");

    QRgb tmpColor = getCurrentColor();

    QFile file(fileName);
    if (!file.open(QFile::ReadOnly | QFile::Text)) {
        QMessageBox::warning(this, tr("Error reading file"),
                             tr("Cannot read file %1:\n%2.")
                             .arg(fileName)
                             .arg(file.errorString()));
        return false;
    }

    QApplication::setOverrideCursor(Qt::WaitCursor);

    QString ext = fileExtension(fileName);
    qDebug("ext: %s", qPrintable(ext));

    // Read
    EmbPattern* p = embPattern_create();
    if (!p) {
        printf("Could not allocate memory for embroidery pattern\n");
        exit(1);
    }
    int readSuccessful = 0;
    QString readError;
    int reader = emb_identify_format(qPrintable(fileName));
    if (reader < 0) {
        readSuccessful = 0;
        readError = "Unsupported read file type: " + fileName;
        qDebug("Unsupported read file type: %s\n", qPrintable(fileName));
    }
    else {
        readSuccessful = embPattern_readAuto(p, qPrintable(fileName));
        if (!readSuccessful) {
            readError = "Reading file was unsuccessful: " + fileName;
            qDebug("Reading file was unsuccessful: %s\n", qPrintable(fileName));
        }
    }

    if (readSuccessful) {
        /**
        \todo reincorporate
        embPattern_moveStitchListToPolylines(p); //TODO: Test more
        */
        int stitchCount = p->stitch_list->count;
        QPainterPath path;

        for (int i=0; i<p->geometry->count; i++) {
            EmbGeometry g = p->geometry->geometry[i];
            setCurrentColor(qRgb(g.color.r, g.color.g, g.color.b));
            if (g.type == EMB_CIRCLE) {
                EmbCircle c = g.object.circle;
                // NOTE: With natives, the Y+ is up and libembroidery Y+ is up, so inverting the Y is NOT needed.
                mainWin->nativeAddCircle(c.center.x, c.center.y, c.radius, false, OBJ_RUBBER_OFF); //TODO: fill
            }
            if (g.type == EMB_ELLIPSE) {
                EmbEllipse e = g.object.ellipse;
                //NOTE: With natives, the Y+ is up and libembroidery Y+ is up, so inverting the Y is NOT needed.
                mainWin->nativeAddEllipse(e.center.x, e.center.y, embEllipse_width(e), embEllipse_height(e), 0, false, OBJ_RUBBER_OFF); //TODO: rotation and fill
            }
            if (g.type == EMB_LINE) {
                EmbLine li = g.object.line;
                //NOTE: With natives, the Y+ is up and libembroidery Y+ is up, so inverting the Y is NOT needed.
                mainWin->nativeAddLine(li.start.x, li.start.y, li.end.x, li.end.y, 0, OBJ_RUBBER_OFF); //TODO: rotation
            }
            if (g.type == EMB_PATH) {
                // TODO: This is unfinished. It needs more work
                QPainterPath pathPath;
                EmbArray* curPointList = g.object.path.pointList;
                EmbPoint pp = curPointList->geometry[0].object.point;
                // NOTE: Qt Y+ is down and libembroidery Y+ is up, so inverting the Y is needed.
                pathPath.moveTo(pp.position.x, -pp.position.y);
                for (int j=1; j<curPointList->count; j++) {
                    EmbPoint pp = curPointList->geometry[j].object.point;
                    pathPath.lineTo(pp.position.x, -pp.position.y);
                }
                 
                QPen loadPen(qRgb(g.color.r, g.color.g, g.color.b));
                loadPen.setWidthF(0.35);
                loadPen.setCapStyle(Qt::RoundCap);
                loadPen.setJoinStyle(Qt::RoundJoin);

                PathObject* obj = new PathObject(0,0, pathPath, loadPen.color().rgb());
                obj->setObjectRubberMode(OBJ_RUBBER_OFF);
                gscene->addItem(obj);
            }
            if (p->geometry->geometry[i].type == EMB_POINT) {
                EmbPoint po = g.object.point;
                //NOTE: With natives, the Y+ is up and libembroidery Y+ is up, so inverting the Y is NOT needed.
                mainWin->nativeAddPoint(po.position.x, po.position.y);
            }
            if (p->geometry->geometry[i].type == EMB_POLYGON) {
                EmbPolygon polygon = g.object.polygon;
                QPainterPath polygonPath;
                bool firstPoint = false;
                EmbReal startX = 0, startY = 0;
                EmbReal x = 0, y = 0;
                EmbArray* curPointList = polygon.pointList;
                for (int j=0; j<curPointList->count; j++) {
                    EmbPoint pp = curPointList->geometry[j].object.point;
                    x = pp.position.x;
                    y = -pp.position.y;
                    // NOTE: Qt Y+ is down and libembroidery Y+ is up, so inverting the Y is needed.

                    if (firstPoint) {
                        polygonPath.lineTo(x,y);
                    }
                    else {
                        polygonPath.moveTo(x,y);
                        firstPoint = true;
                        startX = x;
                        startY = y;
                    }
                }

                polygonPath.translate(-startX, -startY);
                mainWin->nativeAddPolygon(startX, startY, polygonPath, OBJ_RUBBER_OFF);
            }
            /* NOTE: Polylines should only contain NORMAL stitches. */
            if (p->geometry->geometry[i].type == EMB_POLYLINE) {
                EmbPolygon polyline = g.object.polyline;
                QPainterPath polylinePath;
                bool firstPoint = false;
                EmbVector start = {0, 0};
                EmbVector pos = {0, 0};
                EmbArray* curPointList = polyline.pointList;
                for (int j=0; j<curPointList->count; j++) {
                    EmbPoint pp = curPointList->geometry[j].object.point;
                    // NOTE: Qt Y+ is down and libembroidery Y+ is up, so inverting the Y is needed.
                    pos.x = pp.position.x;
                    pos.y = -pp.position.y;

                    if (firstPoint) {
                        polylinePath.lineTo(pos.x, pos.y);
                    }
                    else {
                        polylinePath.moveTo(pos.x, pos.y);
                        firstPoint = true;
                        start = pos;
                    }
                }

                polylinePath.translate(-start.x, -start.y);
                mainWin->nativeAddPolyline(start.x, start.y, polylinePath, OBJ_RUBBER_OFF);
            }
            if (g.type == EMB_RECT) {
                EmbRect r = g.object.rect;
                //NOTE: With natives, the Y+ is up and libembroidery Y+ is up, so inverting the Y is NOT needed.
                mainWin->nativeAddRectangle(r.left, r.top, r.right - r.left, r.bottom - r.top, 0, false, OBJ_RUBBER_OFF); //TODO: rotation and fill
            }
        }

        setCurrentFile(fileName);
        mainWin->statusbar->showMessage("File loaded.");
        QString stitches;
        stitches.setNum(stitchCount);

        if (mainWin->getSettingsGridLoadFromFile()) {
            //TODO: Josh, provide me a hoop size and/or grid spacing from the pattern.
        }

        QApplication::restoreOverrideCursor();
    }
    else {
        QMessageBox::warning(this, tr("Error reading pattern"), tr(qPrintable(readError)));
        QApplication::restoreOverrideCursor();
        QMessageBox::warning(this, tr("Error reading pattern"), tr("Cannot read pattern"));
    }
    embPattern_free(p);

    //Clear the undo stack so it is not possible to undo past this point.
    gview->getUndoStack()->clear();

    setCurrentColor(tmpColor);

    fileWasLoaded = true;
    mainWin->setUndoCleanIcon(fileWasLoaded);
    return fileWasLoaded;
}

/**
 * @brief MdiWindow::print
 */
void MdiWindow::print()
{
    QPrintDialog dialog(&printer, this);
    if(dialog.exec() == QDialog::Accepted)
    {
        QPainter painter(&printer);
        if(mainWin->getSettingsPrintingDisableBG())
        {
            //Save current bg
            QBrush brush = gview->backgroundBrush();
            //Save ink by not printing the bg at all
            gview->setBackgroundBrush(Qt::NoBrush);
            //Print, fitting the viewport contents into a full page
            gview->render(&painter);
            //Restore the bg
            gview->setBackgroundBrush(brush);
        }
        else
        {
            //Print, fitting the viewport contents into a full page
            gview->render(&painter);
        }
    }
}


/**
 * @brief MdiWindow::saveBMC
 *
 * \todo Save a Brother PEL image (An 8bpp, 130x113 pixel monochromatic? bitmap image) Why 8bpp when only 1bpp is needed?
 *
 * \todo Should BMC be limited to ~32KB or is this a mix up with Bitmap Cache?
 * \todo Is there/should there be other embedded data in the bitmap besides the image itself?
 * \note Can save a Singer BMC image (An 8bpp, 130x113 pixel colored bitmap image)
 */
void MdiWindow::saveBMC()
{
    //TODO: figure out how to center the image, right now it just plops it to the left side.
    QImage img(150, 150, QImage::Format_ARGB32_Premultiplied);
    img.fill(qRgb(255,255,255));
    QRectF extents = gscene->itemsBoundingRect();

    QPainter painter(&img);
    QRectF targetRect(0,0,150,150);
    if(mainWin->getSettingsPrintingDisableBG()) //TODO: Make BMC background into it's own setting?
    {
        QBrush brush = gscene->backgroundBrush();
        gscene->setBackgroundBrush(Qt::NoBrush);
        gscene->update();
        gscene->render(&painter, targetRect, extents, Qt::KeepAspectRatio);
        gscene->setBackgroundBrush(brush);
    }
    else
    {
        gscene->update();
        gscene->render(&painter, targetRect, extents, Qt::KeepAspectRatio);
    }

    img.convertToFormat(QImage::Format_Indexed8, Qt::ThresholdDither|Qt::AvoidDither).save("test.bmc", "BMP");
}

/**
 * @brief MdiWindow::setCurrentFile
 * @param fileName
 */
void
MdiWindow::setCurrentFile(const QString &fileName)
{
    curFile = QFileInfo(fileName).canonicalFilePath();
    setWindowModified(false);
    setWindowTitle(getShortCurrentFile());
}

/**
 * @brief MdiWindow::getShortCurrentFile
 * @return
 */
QString
MdiWindow::getShortCurrentFile()
{
    return QFileInfo(curFile).fileName();
}

/**
 * @brief MdiWindow::fileExtension
 * @param fileName
 * @return
 */
QString
MdiWindow::fileExtension(const QString& fileName)
{
    return QFileInfo(fileName).suffix().toLower();
}

/**
 * @brief MdiWindow::closeEvent
 */
void
MdiWindow::closeEvent(QCloseEvent* /*e*/)
{
    qDebug("MdiWindow closeEvent()");
    emit sendCloseMdiWin(this);
}

/**
 * @brief MdiWindow::onWindowActivated
 */
void
MdiWindow::onWindowActivated()
{
    qDebug("MdiWindow onWindowActivated()");
    gview->getUndoStack()->setActive(true);
    mainWin->setUndoCleanIcon(fileWasLoaded);
    mainWin->statusbar->statusBarSnapButton->setChecked(gscene->property(ENABLE_SNAP).toBool());
    mainWin->statusbar->statusBarGridButton->setChecked(gscene->property(ENABLE_GRID).toBool());
    mainWin->statusbar->statusBarRulerButton->setChecked(gscene->property(ENABLE_RULER).toBool());
    mainWin->statusbar->statusBarOrthoButton->setChecked(gscene->property(ENABLE_ORTHO).toBool());
    mainWin->statusbar->statusBarPolarButton->setChecked(gscene->property(ENABLE_POLAR).toBool());
    mainWin->statusbar->statusBarQSnapButton->setChecked(gscene->property(ENABLE_QSNAP).toBool());
    mainWin->statusbar->statusBarQTrackButton->setChecked(gscene->property(ENABLE_QTRACK).toBool());
    mainWin->statusbar->statusBarLwtButton->setChecked(gscene->property(ENABLE_LWT).toBool());
    mainWin->prompt->setHistory(promptHistory);
}

/**
 * @brief MdiWindow::sizeHint
 * @return
 */
QSize
MdiWindow::sizeHint() const
{
    qDebug("MdiWindow sizeHint()");
    return QSize(450, 300);
}

/**
 * @brief MdiWindow::currentLayerChanged
 * @param layer
 */
void
MdiWindow::currentLayerChanged(const QString& layer)
{
    curLayer = layer;
}

/**
 * @brief MdiWindow::currentColorChanged
 * @param color
 */
void MdiWindow::currentColorChanged(const QRgb& color)
{
    curColor = color;
}

/**
 * @brief MdiWindow::currentLinetypeChanged
 * @param type
 */
void MdiWindow::currentLinetypeChanged(const QString& type)
{
    curLineType = type;
}

/**
 * @brief MdiWindow::currentLineweightChanged
 * @param weight
 */
void MdiWindow::currentLineweightChanged(const QString& weight)
{
    curLineWeight = weight;
}

void MdiWindow::updateColorLinetypeLineweight()
{
}

void MdiWindow::deletePressed()
{
    gview->deletePressed();
}

void MdiWindow::escapePressed()
{
    gview->escapePressed();
}

void MdiWindow::showViewScrollBars(bool val)
{
    gview->showScrollBars(val);
}

void MdiWindow::setViewCrossHairColor(QRgb color)
{
    gview->setCrossHairColor(color);
}

void MdiWindow::setViewBackgroundColor(QRgb color)
{
    gview->setBackgroundColor(color);
}

void MdiWindow::setViewSelectBoxColors(QRgb colorL, QRgb fillL, QRgb colorR, QRgb fillR, int alpha)
{
    gview->setSelectBoxColors(colorL, fillL, colorR, fillR, alpha);
}

void MdiWindow::setViewGridColor(QRgb color)
{
    gview->setGridColor(color);
}

void MdiWindow::setViewRulerColor(QRgb color)
{
    gview->setRulerColor(color);
}

void MdiWindow::promptHistoryAppended(const QString& txt)
{
    promptHistory.append("<br/>" + txt);
}

void MdiWindow::logPromptInput(const QString& txt)
{
    promptInputList << txt;
    promptInputNum = promptInputList.size();
}

void MdiWindow::promptInputPrevious()
{
    promptInputPrevNext(true);
}

/**
 * @brief MdiWindow::promptInputNext
 */
void
MdiWindow::promptInputNext()
{
    promptInputPrevNext(false);
}

/**
 * @brief MdiWindow::promptInputPrevNext
 * @param prev
 */
void
MdiWindow::promptInputPrevNext(bool prev)
{
    if(promptInputList.isEmpty())
    {
        if(prev) QMessageBox::critical(this, tr("Prompt Previous Error"), tr("The prompt input is empty! Please report this as a bug!"));
        else     QMessageBox::critical(this, tr("Prompt Next Error"),     tr("The prompt input is empty! Please report this as a bug!"));
        qDebug("The prompt input is empty! Please report this as a bug!");
    }
    else
    {
        if(prev) promptInputNum--;
        else     promptInputNum++;
        int maxNum = promptInputList.size();
        if     (promptInputNum < 0)       { promptInputNum = 0;      mainWin->prompt->setCurrentText(""); }
        else if(promptInputNum >= maxNum) { promptInputNum = maxNum; mainWin->prompt->setCurrentText(""); }
        else                              { mainWin->prompt->setCurrentText(promptInputList.at(promptInputNum)); }
    }
}
