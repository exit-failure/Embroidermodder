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

/**
 *
 */
TextSingleObject::TextSingleObject(const QString& str, EmbReal x, EmbReal y, QRgb rgb, QGraphicsItem* parent) : BaseObject(parent)
{
    debug_message("TextSingleObject Constructor()");
    init(str, x, y, rgb, Qt::SolidLine); //TODO: getCurrentLineType
}

/**
 *
 */
TextSingleObject::TextSingleObject(TextSingleObject* obj, QGraphicsItem* parent) : BaseObject(parent)
{
    debug_message("TextSingleObject Constructor()");
    if (obj)
    {
        setObjectTextFont(obj->objTextFont);
        setObjectTextSize(obj->objTextSize);
        setRotation(obj->rotation());
        setObjectTextBackward(obj->objTextBackward);
        setObjectTextUpsideDown(obj->objTextUpsideDown);
        setObjectTextStyle(obj->objTextBold, obj->objTextItalic, obj->objTextUnderline, obj->objTextStrikeOut, obj->objTextOverline);
        init(obj->objText, obj->objectX(), obj->objectY(), obj->objectColorRGB(), Qt::SolidLine); //TODO: getCurrentLineType
        setScale(obj->scale());
    }
}

/**
 *
 */
TextSingleObject::~TextSingleObject()
{
    debug_message("TextSingleObject Destructor()");
}

/**
 *
 */
void
TextSingleObject::init(const QString& str, EmbReal x, EmbReal y, QRgb rgb, Qt::PenStyle lineType)
{
    setData(OBJ_TYPE, OBJ_TYPE_TEXTSINGLE);
    setData(OBJ_NAME, "Single Line Text");

    //WARNING: DO NOT enable QGraphicsItem::ItemIsMovable. If it is enabled,
    //WARNING: and the item is double clicked, the scene will erratically move the item while zooming.
    //WARNING: All movement has to be handled explicitly by us, not by the scene.
    setFlag(QGraphicsItem::ItemIsSelectable, true);

    objTextJustify = "Left"; //TODO: set the justification properly

    setObjectText(str);
    setObjectPos(x,y);
    setObjectColor(rgb);
    setObjectLineType(lineType);
    setObjectLineWeight(0.35); //TODO: pass in proper lineweight
    setPen(objPen);
}

QStringList justify_options = {
    "Left",
    "Center",
    "Right",
    "Aligned",
    "Middle",
    "Fit",
    "Top Left",
    "Top Center",
    "Top Right",
    "Middle Left",
    "Middle Center",
    "Middle Right",
    "Bottom Left",
    "Bottom Center",
    "Bottom Right"
/* \todo "Aligned"
 * \todo "Fit"
 */
};

/**
 *
 */
void TextSingleObject::setObjectText(const QString& str)
{
    objText = str;
    QPainterPath textPath;
    QFont font;
    font.setFamily(objTextFont);
    font.setPointSizeF(objTextSize);
    font.setBold(objTextBold);
    font.setItalic(objTextItalic);
    font.setUnderline(objTextUnderline);
    font.setStrikeOut(objTextStrikeOut);
    font.setOverline(objTextOverline);
    textPath.addText(0, 0, font, str);

    //Translate the path based on the justification
    QRectF jRect = textPath.boundingRect();
    if (objTextJustify == "Left") {
        textPath.translate(-jRect.left(), 0);
    }
    else if (objTextJustify == "Center") {
        textPath.translate(-jRect.center().x(), 0);
    }
    else if (objTextJustify == "Right")         { textPath.translate(-jRect.right(), 0); }
    else if (objTextJustify == "Aligned")       { } //TODO: TextSingleObject Aligned Justification
    else if (objTextJustify == "Middle")        { textPath.translate(-jRect.center()); }
    else if (objTextJustify == "Fit")           { } //TODO: TextSingleObject Fit Justification
    else if (objTextJustify == "Top Left")      { textPath.translate(-jRect.topLeft()); }
    else if (objTextJustify == "Top Center")    { textPath.translate(-jRect.center().x(), -jRect.top()); }
    else if (objTextJustify == "Top Right")     { textPath.translate(-jRect.topRight()); }
    else if (objTextJustify == "Middle Left")   { textPath.translate(-jRect.left(), -jRect.top()/2.0); }
    else if (objTextJustify == "Middle Center") { textPath.translate(-jRect.center().x(), -jRect.top()/2.0); }
    else if (objTextJustify == "Middle Right")  { textPath.translate(-jRect.right(), -jRect.top()/2.0); }
    else if (objTextJustify == "Bottom Left")   { textPath.translate(-jRect.bottomLeft()); }
    else if (objTextJustify == "Bottom Center") { textPath.translate(-jRect.center().x(), -jRect.bottom()); }
    else if (objTextJustify == "Bottom Right")  { textPath.translate(-jRect.bottomRight()); }

    //Backward or Upside Down
    if (objTextBackward || objTextUpsideDown) {
        EmbReal horiz = 1.0;
        EmbReal vert = 1.0;
        if (objTextBackward) horiz = -1.0;
        if (objTextUpsideDown) vert = -1.0;

        QPainterPath flippedPath;

        QPainterPath::Element element;
        QPainterPath::Element P2;
        QPainterPath::Element P3;
        QPainterPath::Element P4;
        for (int i = 0; i < textPath.elementCount(); ++i) {
            element = textPath.elementAt(i);
            if (element.isMoveTo()) {
                flippedPath.moveTo(horiz * element.x, vert * element.y);
            }
            else if (element.isLineTo()) {
                flippedPath.lineTo(horiz * element.x, vert * element.y);
            }
            else if (element.isCurveTo()) {
                                              // start point P1 is not needed
                P2 = textPath.elementAt(i);   // control point
                P3 = textPath.elementAt(i+1); // control point
                P4 = textPath.elementAt(i+2); // end point

                flippedPath.cubicTo(horiz * P2.x, vert * P2.y,
                                    horiz * P3.x, vert * P3.y,
                                    horiz * P4.x, vert * P4.y);
            }
        }
        objTextPath = flippedPath;
    }
    else {
        objTextPath = textPath;
    }

    //Add the grip point to the shape path
    QPainterPath gripPath = objTextPath;
    gripPath.connectPath(objTextPath);
    gripPath.addRect(-0.00000001, -0.00000001, 0.00000002, 0.00000002);
    setObjectPath(gripPath);
}

/**
 *
 */
void
TextSingleObject::setObjectTextFont(const QString& font)
{
    objTextFont = font;
    setObjectText(objText);
}

/**
 * Verify the string is a valid option, otherwise default to "Left".
 */
void
TextSingleObject::setObjectTextJustify(const QString& justify)
{
    objTextJustify = "Left";
    for (int i=0; i<justify_options.size(); i++) {
        if (justify == justify_options[i]) {
            objTextJustify = justify;
        }
    }
    setObjectText(objText);
}

/**
 *
 */
void
TextSingleObject::setObjectTextSize(EmbReal size)
{
    objTextSize = size;
    setObjectText(objText);
}

/**
 *
 */
void
TextSingleObject::setObjectTextStyle(bool bold, bool italic, bool under, bool strike, bool over)
{
    objTextBold = bold;
    objTextItalic = italic;
    objTextUnderline = under;
    objTextStrikeOut = strike;
    objTextOverline = over;
    setObjectText(objText);
}

/**
 *
 */
void
TextSingleObject::setObjectTextBold(bool val)
{
    objTextBold = val;
    setObjectText(objText);
}

/**
 *
 */
void
TextSingleObject::setObjectTextItalic(bool val)
{
    objTextItalic = val;
    setObjectText(objText);
}

/**
 *
 */
void
TextSingleObject::setObjectTextUnderline(bool val)
{
    objTextUnderline = val;
    setObjectText(objText);
}

/**
 *
 */
void
TextSingleObject::setObjectTextStrikeOut(bool val)
{
    objTextStrikeOut = val;
    setObjectText(objText);
}

/**
 *
 */
void
TextSingleObject::setObjectTextOverline(bool val)
{
    objTextOverline = val;
    setObjectText(objText);
}

/**
 *
 */
void
TextSingleObject::setObjectTextBackward(bool val)
{
    objTextBackward = val;
    setObjectText(objText);
}

/**
 *
 */
void
TextSingleObject::setObjectTextUpsideDown(bool val)
{
    objTextUpsideDown = val;
    setObjectText(objText);
}

/**
 *
 */
void
TextSingleObject::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* /*widget*/)
{
    QGraphicsScene* objScene = scene();
    if (!objScene) return;

    QPen paintPen = pen();
    painter->setPen(paintPen);
    updateRubber(painter);
    if (option->state & QStyle::State_Selected) {
        paintPen.setStyle(Qt::DashLine);
    }
    if (objScene->property("ENABLE_LWT").toBool()) {
        paintPen = lineWeightPen();
    }
    painter->setPen(paintPen);

    painter->drawPath(objTextPath);
}

/**
 *
 */
void
TextSingleObject::updateRubber(QPainter* painter)
{
    int rubberMode = objRubberMode;
    if (rubberMode == OBJ_RUBBER_TEXTSINGLE) {
        setObjectTextFont(objectRubberText("TEXT_FONT"));
        setObjectTextJustify(objectRubberText("TEXT_JUSTIFY"));
        setObjectPos(objectRubberPoint("TEXT_POINT"));
        QPointF hr = objectRubberPoint("TEXT_HEIGHT_ROTATION");
        setObjectTextSize(hr.x());
        setRotation(hr.y());
        setObjectText(objectRubberText("TEXT_RAPID"));
    }
    else if (rubberMode == OBJ_RUBBER_GRIP) {
        if (painter) {
            QPointF gripPoint = objectRubberPoint("GRIP_POINT");
            if (gripPoint == scenePos()) {
                painter->drawPath(path().translated(mapFromScene(objectRubberPoint(QString()))-mapFromScene(gripPoint)));
            }

            QLineF rubLine(mapFromScene(gripPoint), mapFromScene(objectRubberPoint(QString())));
            drawRubberLine(rubLine, painter, "VIEW_COLOR_CROSSHAIR");
        }
    }
}

/**
 *
 */
void
TextSingleObject::vulcanize()
{
    debug_message("TextSingleObject vulcanize()");
    updateRubber();

    objRubberMode = OBJ_RUBBER_OFF;
}

/**
 *
 * Returns the closest snap point to the mouse point
 */
QPointF TextSingleObject::mouseSnapPoint(const QPointF& mousePoint)
{
    return scenePos();
}

/**
 *
 */
QList<QPointF>
TextSingleObject::allGripPoints()
{
    QList<QPointF> gripPoints;
    gripPoints << scenePos();
    return gripPoints;
}

/**
 *
 */
void
TextSingleObject::gripEdit(const QPointF& before, const QPointF& after)
{
    if (before == scenePos()) {
        QPointF delta = after-before;
        moveBy(delta.x(), delta.y());
    }
}

/**
 *
 */
QList<QPainterPath>
TextSingleObject::subPathList() const
{
    EmbReal s = scale();
    QTransform trans;
    trans.rotate(rotation());
    trans.scale(s,s);

    QList<QPainterPath> pathList;

    QPainterPath path = objTextPath;

    QPainterPath::Element element;
    QList<int> pathMoves;
    int numMoves = 0;

    for (int i = 0; i < path.elementCount(); i++) {
        element = path.elementAt(i);
        if (element.isMoveTo()) {
            pathMoves << i;
            numMoves++;
        }
    }

    pathMoves << path.elementCount();

    for (int p = 0; p < pathMoves.size()-1 && p < numMoves; p++) {
        QPainterPath subPath;
        for (int i = pathMoves.value(p); i < pathMoves.value(p+1); i++) {
            element = path.elementAt(i);
            if (element.isMoveTo()) {
                subPath.moveTo(element.x, element.y);
            }
            else if (element.isLineTo()) {
                subPath.lineTo(element.x, element.y);
            }
            else if (element.isCurveTo()) {
                subPath.cubicTo(path.elementAt(i  ).x, path.elementAt(i  ).y,  //control point 1
                                path.elementAt(i+1).x, path.elementAt(i+1).y,  //control point 2
                                path.elementAt(i+2).x, path.elementAt(i+2).y); //end point
            }
        }
        pathList.append(trans.map(subPath));
    }

    return pathList;
}
