/*
 *  Embroidermodder 2.
 *
 *  ------------------------------------------------------------
 *
 *  Copyright 2013-2022 The Embroidermodder Team
 *  Embroidermodder 2 is Open Source Software.
 *  See LICENSE for licensing terms.
 *
 *  ------------------------------------------------------------
 *
 *  Use Python's PEP7 style guide.
 *      https://peps.python.org/pep-0007/
 */

#include "embroidermodder.h"

/* Construct a new MdiWindow object.
 * 
 * theIndex 
 * parent 
 * wflags 
 *
 * WARNING:
 *   DO NOT SET THE QMDISUBWINDOW (this) FOCUSPROXY TO THE PROMPT
 *   AS IT WILL CAUSE THE WINDOW MENU TO NOT SWITCH WINDOWS PROPERLY!
 *   ALTHOUGH IT SEEMS THAT SETTING INTERNAL WIDGETS FOCUSPROXY IS OK.
 */
MdiWindow::MdiWindow(const int theIndex, QMdiArea* parent, Qt::WindowFlags wflags) : QMdiSubWindow(parent, wflags)
{
    mdiArea = parent;

    myIndex = theIndex;

    fileWasLoaded = false;

    setAttribute(Qt::WA_DeleteOnClose);

    QString aName;
    curFile = aName.asprintf("Untitled%d.dst", myIndex);
    this->setWindowTitle(curFile);

    this->setWindowIcon(_mainWin->create_icon("app"));

    gscene = new QGraphicsScene(0,0,0,0, this);
    gview = new View(gscene, this);

    setWidget(gview);

    gview->setFocusProxy(prompt);

    resize(sizeHint());

    promptHistory = "Welcome to Embroidermodder 2!<br/>Open some of our sample files. Many formats are supported.<br/>For help, press F1.";
    prompt->setHistory(promptHistory);
    promptInputList.push_back("");
    promptInputNum = 0;

    curLayer = "0";
    curColor = 0; //TODO: color ByLayer
    curLineType = "ByLayer";
    curLineWeight = "ByLayer";

    showMaximized();

    setFocusPolicy(Qt::WheelFocus);
    setFocus();

    onWindowActivated();
}

/* Destroy the MdiWindow object. */
MdiWindow::~MdiWindow()
{
    debug_message("MdiWindow Destructor()");
}

/* Save file to "fileName". */
bool
MdiWindow::saveFile(String fileName)
{
    return save_current_file(fileName);
}

/* Load file to this subwindow. */
bool
MdiWindow::loadFile(String fileName)
{
    debug_message("MdiWindow loadFile()");

    QRgb tmpColor = curColor;

    QFile file(QString::fromStdString(fileName));
    if (!file.open(QFile::ReadOnly | QFile::Text)) {
        QString error = translate_str("Cannot read file")
            + QString::fromStdString(fileName) + ": " + file.errorString();
        QMessageBox::warning(this, tr("Error reading file"), error);
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
    String readError;
    int reader = emb_identify_format(fileName.c_str());
    if (reader < 0) {
        readSuccessful = 0;
        readError = "Unsupported read file type: " + fileName;
        debug_message("Unsupported read file type: " + fileName);
    }
    else {
        readSuccessful = embPattern_readAuto(p, fileName.c_str());
        if (!readSuccessful) {
            readError = "Reading file was unsuccessful: " + fileName;
            debug_message("Reading file was unsuccessful: " + fileName);
        }
    }

    if (readSuccessful) {
        debug_message("Starting to load the read file.");
        //embPattern_moveStitchListToPolylines(p); //TODO: Test more
        
        int stitchCount = p->stitch_list->count;
        qDebug("%d", stitchCount);
        for (int i=0; i<stitchCount; i++) {
            EmbStitch st = p->stitch_list->stitch[i];
            // int color = 0;
            QPainterPath stitchPath;
            bool firstPoint = true;
            EmbVector pos = {0, 0};
            for (; (st.flags <= 2) && (i<stitchCount); i++) {
                st = p->stitch_list->stitch[i];
                // NOTE: Qt Y+ is down and libembroidery Y+ is up, so inverting the Y is needed.
                pos.x = st.x;
                pos.y = -st.y;
                qDebug("%d %f %f", i, pos.x, pos.y);

                /* BUG: This means that the trim information is forgot in loading to a
                 * polyline. We should have a custom class for this.
                 */
                if (firstPoint || (st.flags == JUMP) || (st.flags == TRIM)) {
                    stitchPath.moveTo(pos.x, pos.y);
                }
                else {
                    if (st.flags == NORMAL) {
                        stitchPath.lineTo(pos.x, pos.y);
                    }
                    firstPoint = false;
                }
            }

            add_polyline(stitchPath, "OBJ_RUBBER_OFF");
        }

        for (int i=0; i<p->geometry->count; i++) {
            EmbGeometry g = p->geometry->geometry[i];
            curColor = qRgb(g.color.r, g.color.g, g.color.b);
            if (g.type == EMB_CIRCLE) {
                EmbCircle c = g.object.circle;
                std::string command = construct_command("add circle ", "rrrbs",
                    c.center.x, c.center.y, c.radius, false, "OBJ_RUBBER_OFF");
                // NOTE: With natives, the Y+ is up and libembroidery Y+ is up, so inverting the Y is NOT needed.
                actuator(command); //TODO: fill
            }
            if (g.type == EMB_ELLIPSE) {
                EmbEllipse e = g.object.ellipse;
                std::string command = construct_command("add ellipse ", "rrrribs",
                    e.center.x, e.center.y, embEllipse_width(e), embEllipse_height(e), 0, false, "OBJ_RUBBER_OFF");
                //NOTE: With natives, the Y+ is up and libembroidery Y+ is up, so inverting the Y is NOT needed.
                actuator(command); //TODO: rotation and fill
            }
            if (g.type == EMB_LINE) {
                EmbLine li = g.object.line;
                std::string command = construct_command("add line ", "rrrris",
                    li.start.x, li.start.y, li.end.x, li.end.y, 0, "OBJ_RUBBER_OFF");
                //NOTE: With natives, the Y+ is up and libembroidery Y+ is up, so inverting the Y is NOT needed.
                actuator(command); //TODO: rotation
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

                /*
                EmbVector v = {0.0, 0.0};
                Geometry* obj = new Geometry(v, pathPath, loadPen.color().rgb(), Qt::SolidLine);
                obj->objRubberMode = "OBJ_RUBBER_OFF";
                gscene->addItem(obj);
                */
            }
            if (p->geometry->geometry[i].type == EMB_POINT) {
                EmbPoint po = g.object.point;
                //NOTE: With natives, the Y+ is up and libembroidery Y+ is up, so inverting the Y is NOT needed.
                std::string command = construct_command("add point ", "rr", po.position.x, po.position.y);
                actuator(command);
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
                //AddPolygon(startX, startY, polygonPath, "OBJ_RUBBER_OFF");
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
                //AddPolyline(start.x, start.y, polylinePath, "OBJ_RUBBER_OFF");
            }
            if (g.type == EMB_RECT) {
                EmbRect r = g.object.rect;
                //NOTE: With natives, the Y+ is up and libembroidery Y+ is up, so inverting the Y is NOT needed.
                std::string command = construct_command("add rectangle ", "rrrrribi",
                    r.left, r.top,
                    r.right - r.left, r.bottom - r.top, 0, false, "OBJ_RUBBER_OFF");
                actuator(command);
            }
        }

        setCurrentFile(QString::fromStdString(fileName));
        statusbar->showMessage("File loaded.");
        QString stitches;
        stitches.setNum(stitchCount);

        if (get_bool(settings, "grid_load_from_file")) {
            //TODO: Josh, provide me a hoop size and/or grid spacing from the pattern.
        }

        QApplication::restoreOverrideCursor();
    }
    else {
        QMessageBox::warning(this, translate_str("Error reading pattern"), translate_str(readError.c_str()));
        QApplication::restoreOverrideCursor();
        QMessageBox::warning(this, translate_str("Error reading pattern"), translate_str("Cannot read pattern"));
    }
    embPattern_free(p);

    // Clear the undo stack so it is not possible to undo past this point.
    gview->getUndoStack()->clear();

    curColor = tmpColor;

    fileWasLoaded = true;
    _mainWin->setUndoCleanIcon(fileWasLoaded);
    return fileWasLoaded;
}

/* Print this subwindow. */
void MdiWindow::print()
{
    QPrintDialog dialog(&printer, this);
    if (dialog.exec() == QDialog::Accepted) {
        QPainter painter(&printer);
        if (get_bool(settings, "printing_disable_bg")) {
            //Save current bg
            QBrush brush = gview->backgroundBrush();
            //Save ink by not printing the bg at all
            gview->setBackgroundBrush(Qt::NoBrush);
            //Print, fitting the viewport contents into a full page
            gview->render(&painter);
            //Restore the bg
            gview->setBackgroundBrush(brush);
        }
        else {
            //Print, fitting the viewport contents into a full page
            gview->render(&painter);
        }
    }
}


/* Save BMC
 *
 * TODO: Save a Brother PEL image (An 8bpp, 130x113 pixel monochromatic? bitmap image) Why 8bpp when only 1bpp is needed?
 *
 * TODO: Should BMC be limited to ~32KB or is this a mix up with Bitmap Cache?
 * TODO: Is there/should there be other embedded data in the bitmap besides the image itself?
 * NOTE: Can save a Singer BMC image (An 8bpp, 130x113 pixel colored bitmap image)
 */
void MdiWindow::saveBMC()
{
    //TODO: figure out how to center the image, right now it just plops it to the left side.
    QImage img(150, 150, QImage::Format_ARGB32_Premultiplied);
    img.fill(qRgb(255,255,255));
    QRectF extents = gscene->itemsBoundingRect();

    QPainter painter(&img);
    QRectF targetRect(0,0,150,150);
    // TODO: Make BMC background into it's own setting?
    if (get_bool(settings, "printing_disable_bg")) {
        QBrush brush = gscene->backgroundBrush();
        gscene->setBackgroundBrush(Qt::NoBrush);
        gscene->update();
        gscene->render(&painter, targetRect, extents, Qt::KeepAspectRatio);
        gscene->setBackgroundBrush(brush);
    }
    else {
        gscene->update();
        gscene->render(&painter, targetRect, extents, Qt::KeepAspectRatio);
    }

    img.convertToFormat(QImage::Format_Indexed8, Qt::ThresholdDither|Qt::AvoidDither).save("test.bmc", "BMP");
}

/* Set current file. */
void
MdiWindow::setCurrentFile(QString fileName)
{
    curFile = QFileInfo(fileName).canonicalFilePath();
    setWindowModified(false);
    setWindowTitle(getShortCurrentFile());
}

/* Get short current file. */
QString
MdiWindow::getShortCurrentFile()
{
    return QFileInfo(curFile).fileName();
}

/* Get file extension from fileName. */
QString
fileExtension(String fileName)
{
    return QFileInfo(QString::fromStdString(fileName)).suffix().toLower();
}

/* Close event. */
void
MdiWindow::closeEvent(QCloseEvent* /*e*/)
{
    debug_message("MdiWindow closeEvent()");
    emit sendCloseMdiWin(this);
}

/* On window activated. */
void
MdiWindow::onWindowActivated()
{
    debug_message("MdiWindow onWindowActivated()");
    gview->getUndoStack()->setActive(true);
    _mainWin->setUndoCleanIcon(fileWasLoaded);
    statusbar->buttons["SNAP"]->setChecked(gscene->property("ENABLE_SNAP").toBool());
    statusbar->buttons["GRID"]->setChecked(gscene->property("ENABLE_GRID").toBool());
    statusbar->buttons["RULER"]->setChecked(gscene->property("ENABLE_RULER").toBool());
    statusbar->buttons["ORTHO"]->setChecked(gscene->property("ENABLE_ORTHO").toBool());
    statusbar->buttons["POLAR"]->setChecked(gscene->property("ENABLE_POLAR").toBool());
    statusbar->buttons["QSNAP"]->setChecked(gscene->property("ENABLE_QSNAP").toBool());
    statusbar->buttons["QTRACK"]->setChecked(gscene->property("ENABLE_QTRACK").toBool());
    statusbar->buttons["LWT"]->setChecked(gscene->property("ENABLE_LWT").toBool());
    prompt->setHistory(promptHistory);
}

/* SizeHint */
QSize
MdiWindow::sizeHint()
{
    debug_message("MdiWindow sizeHint()");
    return QSize(450, 300);
}

/* CurrentLayerChanged. */
void
MdiWindow::currentLayerChanged(QString layer)
{
    curLayer = layer;
}

/* currentColorChanged. */
void MdiWindow::currentColorChanged(const QRgb& color)
{
    curColor = color;
}

/* currentLinetypeChanged. */
void MdiWindow::currentLinetypeChanged(QString type)
{
    curLineType = type;
}

/* currentLineweightChanged */
void
MdiWindow::currentLineweightChanged(QString weight)
{
    curLineWeight = weight;
}

/* . */
void
MdiWindow::updateColorLinetypeLineweight()
{
}

/* Pass the delete pressed event to the gview. */
void
MdiWindow::deletePressed()
{
    gview->deletePressed();
}

/* Pass the escape pressed event to the gview. */
void
MdiWindow::escapePressed()
{
    gview->escapePressed();
}

/* . */
void
MdiWindow::showViewScrollBars(bool val)
{
    gview->showScrollBars(val);
}

/* . */
void
MdiWindow::setViewCrossHairColor(QRgb color)
{
    gview->setCrossHairColor(color);
}

/* . */
void
MdiWindow::setViewBackgroundColor(QRgb color)
{
    gview->setBackgroundColor(color);
}

/* . */
void
MdiWindow::setViewSelectBoxColors(QRgb colorL, QRgb fillL, QRgb colorR, QRgb fillR, int alpha)
{
    gview->setSelectBoxColors(colorL, fillL, colorR, fillR, alpha);
}

/* . */
void
MdiWindow::setViewGridColor(QRgb color)
{
    gview->setGridColor(color);
}

/* . */
void
MdiWindow::setViewRulerColor(QRgb color)
{
    gview->setRulerColor(color);
}

/* . */
void
MdiWindow::promptHistoryAppended(QString txt)
{
    promptHistory.append("<br/>" + txt);
}

/* . */
void
MdiWindow::logPromptInput(QString txt)
{
    promptInputList.push_back(txt);
    promptInputNum = promptInputList.size();
}

/* . */
void
MdiWindow::promptInputPrevious()
{
    promptInputPrevNext(true);
}

/* . */
void
MdiWindow::promptInputNext()
{
    promptInputPrevNext(false);
}

/* promptInputPrevNext. */
void
MdiWindow::promptInputPrevNext(bool prev)
{
    if (promptInputList.size() == 0) {
        if (prev) {
            QMessageBox::critical(this,
                tr("Prompt Previous Error"),
                tr("The prompt input is empty! Please report this as a bug!"));
        }
        else {
            QMessageBox::critical(this,
                tr("Prompt Next Error"),
                tr("The prompt input is empty! Please report this as a bug!"));
        }
        debug_message("The prompt input is empty! Please report this as a bug!");
    }
    else {
        if (prev) {
            promptInputNum--;
        }
        else {
            promptInputNum++;
        }
        int maxNum = promptInputList.size();
        if (promptInputNum < 0) {
            promptInputNum = 0;
            prompt->setCurrentText("");
        }
        else if (promptInputNum >= maxNum) {
            promptInputNum = maxNum;
            prompt->setCurrentText("");
        }
        else {
            prompt->setCurrentText(promptInputList.at(promptInputNum));
        }
    }
}
