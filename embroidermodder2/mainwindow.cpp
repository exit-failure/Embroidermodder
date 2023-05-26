/*
 *  Embroidermodder 2.
 *
 *  ------------------------------------------------------------
 *
 *  Copyright 2013-2023 The Embroidermodder Team
 *  Embroidermodder 2 is Open Source Software.
 *  See LICENSE for licensing terms.
 *
 *  ------------------------------------------------------------
 *
 *  Use Python's PEP7 style guide.
 *      https://peps.python.org/pep-0007/
 */

/**
 * \file mainwindow.cpp
 */

#include "embroidermodder.h"

#include <cerrno>
#include <iostream>
#include <fstream>
#include <string>

MainWindow* _mainWin = 0;
MdiArea* mdiArea = 0;
CmdPrompt* prompt = 0;
PropertyEditor* dockPropEdit = 0;
UndoEditor* dockUndoEdit = 0;
StatusBar* statusbar = 0;

QWizard* wizardTipOfTheDay;
QLabel* labelTipOfTheDay;
QCheckBox* checkBoxTipOfTheDay;
QStringList listTipOfTheDay;

Dictionary config;

std::unordered_map<String, QAction*> actionHash;
std::unordered_map<String, QToolBar*> toolbarHash;
std::unordered_map<String, QMenu*> menuHash;
std::unordered_map<String, QMenu*> subMenuHash;

std::vector<Action> action_table;

std::vector<EmbReal> snowflake_x;
std::vector<EmbReal> snowflake_y;
std::vector<EmbReal> dolphin_x;
std::vector<EmbReal> dolphin_y;

StringList tokenize(String str, const char delim);
String convert_args_to_type(String label, StringList args,
    const char *args_template, NodeList a);

/**
 * ACTIONS
 * \todo these should all be static, since other files
 * use the actuator to call them.
 */
String add_arc_action(String args);
String add_circle_action(String args);
String add_rubber_action(String args);
String add_slot_action(String args);
String append_prompt_history_action(String args);
String copy_action(String args);
String cut_action(String args);
String day_vision_action(String args);
String do_nothing_action(String args);
String error_action(String args);
String night_vision_action(String args);
String pan_action(String args);
String paste_action(String args);
String set_rubber_text_action(String args);
String spare_rubber_action(String args);
String tip_of_the_day_action(String args);
String todo_action(String args);
String zoom_action(String args);

String design_details_action(String args);
String about_action(String args);
String whats_this_action(String args);
String print_action(String args);
static String help_action(String args);
static String changelog_action(String args);
static String undo_action(String args);
static String redo_action(String args);

std::unordered_map<String, Command> command_map = {
    {"add-arc", add_arc_action},
    {"add-circle", add_circle_action},
    {"add-rubber", add_rubber_action},
    {"add-slot", add_slot_action},
    {"append-history", append_prompt_history_action},
    {"copy", copy_action},
    {"cut", cut_action},
    {"day", day_vision_action},
    {"donothing", do_nothing_action},
    {"error", error_action},
    {"night", night_vision_action},
    {"pan", pan_action},
    {"paste", paste_action},
    {"set-rubber-text", set_rubber_text_action},
    {"spare-rubber", spare_rubber_action},
    {"tip-of-the-day", tip_of_the_day_action},
    {"todo", todo_action},
    {"zoom", zoom_action}
};

std::unordered_map<String, int> rubber_mode_hash = {
    {"CIRCLE_1P_RAD", OBJ_RUBBER_CIRCLE_1P_RAD},
    {"CIRCLE_1P_DIA", OBJ_RUBBER_CIRCLE_1P_DIA},
    {"CIRCLE_2P", OBJ_RUBBER_CIRCLE_2P},
    {"CIRCLE_3P", OBJ_RUBBER_CIRCLE_3P},
    {"CIRCLE_TTR", OBJ_RUBBER_CIRCLE_TTR},
    {"CIRCLE_TTT", OBJ_RUBBER_CIRCLE_TTT},
    {"DIMLEADER_LINE", OBJ_RUBBER_DIMLEADER_LINE},
    {"ELLIPSE_LINE", OBJ_RUBBER_ELLIPSE_LINE},
    {"ELLIPSE_MAJORDIAMETER_MINORRADIUS", OBJ_RUBBER_ELLIPSE_MAJORDIAMETER_MINORRADIUS},
    {"ELLIPSE_MAJORRADIUS_MINORRADIUS", OBJ_RUBBER_ELLIPSE_MAJORRADIUS_MINORRADIUS},
    {"ELLIPSE_ROTATION", OBJ_RUBBER_ELLIPSE_ROTATION},
    {"LINE", OBJ_RUBBER_LINE},
    {"POLYGON", OBJ_RUBBER_POLYGON},
    {"POLYGON_INSCRIBE", OBJ_RUBBER_POLYGON_INSCRIBE},
    {"POLYGON_CIRCUMSCRIBE", OBJ_RUBBER_POLYGON_CIRCUMSCRIBE},
    {"POLYLINE", OBJ_RUBBER_POLYLINE},
    {"RECTANGLE", OBJ_RUBBER_RECTANGLE},
    {"TEXTSINGLE", OBJ_RUBBER_TEXTSINGLE}
};

static QString
tr(const char *str)
{
    return _mainWin->tr(str);
}

/**
 * @brief create_lisp
 * @return
 */
Lisp
lisp(void)
{
    Lisp l;
    l.T.s = "T";
    l.T.type = SYMBOL_TYPE;
    l.F.s = "F";
    l.F.type = SYMBOL_TYPE;
    return l;
}

/**
 * @brief set_node
 * @param node
 * @param value
 */
Node
node(bool value)
{
    Node node;
    node.type = BOOL_TYPE;
    node.b = value;
    return node;
}

/**
 * @brief create_node
 * @param mode
 * @return
 */
Node
node(int value)
{
    Node node;
    node.type = INT_TYPE;
    node.i = value;
    return node;
}

/**
 * @brief set_node
 * @param node
 * @param value
 */
Node
node(EmbReal value)
{
    Node node;
    node.type = REAL_TYPE;
    node.r = value;
    return node;
}

/**
 * @brief set_node
 * @param node
 * @param value
 */
Node
node(String value)
{
    Node node;
    node.type = STRING_TYPE;
    node.s = value;
    return node;
}

/**
 * @brief set_node
 * @param node
 * @param value
 */
Node
node(StringList value)
{
    Node node;
    node.type = STRING_LIST_TYPE;
    node.sl = value;
    return node;
}

/**
 * @brief get_sl
 * @param key
 * @return
 */
StringList
get_sl(String key)
{
    return config[key].sl;
}

/**
 * .
 * Note: on Unix we include the trailing separator.
 * For Windows compatibility we omit it.
 */
QString SettingsDir()
{
#if defined(Q_OS_UNIX) || defined(Q_OS_MAC)
    QString homePath = QDir::homePath();
    return homePath + "/.embroidermodder2/";
#else
    return "";
#endif
}

/**
 * @brief SettingsPath
 * @return
 */
QString SettingsPath()
{
    QString settingsPath = SettingsDir() + "settings.ini";
    return settingsPath;
}

/**
 * @brief to_string_vector
 * @param list
 * @return
 */
std::vector<String>
to_string_vector(QStringList list)
{
    std::vector<String> a;
    for (int i=0; i<(int)list.size(); i++) {
        a.push_back(list[i].toStdString());
    }
    return a;
}

/**
 * @brief MainWindow::settingsPrompt
 */
void
MainWindow::settingsPrompt()
{
    settingsDialog("Prompt");
}

/**
 * @brief MainWindow::settingsDialog
 * @param showTab
 */
void
MainWindow::settingsDialog(const QString& showTab)
{
    Settings_Dialog dialog(showTab);
    dialog.exec();
}
/**
 * @brief MainWindow::stub_testing
 */
void
MainWindow::stub_testing()
{
    QMessageBox::warning(this, tr("Testing Feature"), tr("<b>This feature is in testing.</b>"));
}

/**
 * @brief MainWindow::quit
 */
void
MainWindow::quit()
{
    debug_message("quit()");
    if (settings.prompt_save_history) {
        prompt->saveHistory("prompt.log", settings.prompt_save_history_as_html);
        //TODO: get filename from settings
    }
    qApp->closeAllWindows();
    this->deleteLater();
    // Force the MainWindow destructor to run before exiting. Makes Valgrind "still reachable" happy :)
}

/**
 *
 */
void
MainWindow::checkForUpdates()
{
    debug_message("checkForUpdates()");
    //TODO: Check website for new versions, commands, etc...
}

void
no_argument_debug(String function_name, String args)
{
    debug_message(function_name);
    if (args != "") {
        debug_message(function_name + " was passed an argument that was ignored");
    }
}

/**
 *
 */
String
cut_action(String args)
{
    no_argument_debug("cut_action()", args);
    View* gview = _mainWin->activeView();
    if (gview) {
        gview->cut();
    }
    return "";
}

/**
 *
 */
String
copy_action(String args)
{
    no_argument_debug("copy_action()", args);
    View* gview = _mainWin->activeView();
    if (gview) {
        gview->copy();
    }
    return "";
}

/**
 *
 */
String
paste_action(String args)
{
    no_argument_debug("paste_action()", args);
    View* gview = _mainWin->activeView();
    if (gview) {
        gview->paste();
    }
    return "";
}

/**
 * @brief do_nothing_action This action intensionally does nothing.
 * @param args This is ignored, it's present to make it a Command.
 * @return An empty string.
 */
String
do_nothing_action(String args)
{
    no_argument_debug("do_nothing_action()", args);
    return "";
}

/**
 * @brief platformString
 * @return
 */
String
platformString(void)
{
    //TODO: Append QSysInfo to string where applicable.
    String os;
    #if   defined(Q_OS_AIX)
    os = "AIX";
    #elif defined(Q_OS_BSD4)
    os = "BSD 4.4";
    #elif defined(Q_OS_BSDI)
    os = "BSD/OS";
    #elif defined(Q_OS_CYGWIN)
    os = "Cygwin";
    #elif defined(Q_OS_DARWIN)
    os = "Mac OS";
    #elif defined(Q_OS_DGUX)
    os = "DG/UX";
    #elif defined(Q_OS_DYNIX)
    os = "DYNIX/ptx";
    #elif defined(Q_OS_FREEBSD)
    os = "FreeBSD";
    #elif defined(Q_OS_HPUX)
    os = "HP-UX";
    #elif defined(Q_OS_HURD)
    os = "GNU Hurd";
    #elif defined(Q_OS_IRIX)
    os = "SGI Irix";
    #elif defined(Q_OS_LINUX)
    os = "Linux";
    #elif defined(Q_OS_LYNX)
    os = "LynxOS";
    #elif defined(Q_OS_MAC)
    os = "Mac OS";
    #elif defined(Q_OS_MSDOS)
    os = "MS-DOS";
    #elif defined(Q_OS_NETBSD)
    os = "NetBSD";
    #elif defined(Q_OS_OS2)
    os = "OS/2";
    #elif defined(Q_OS_OPENBSD)
    os = "OpenBSD";
    #elif defined(Q_OS_OS2EMX)
    os = "XFree86 on OS/2";
    #elif defined(Q_OS_OSF)
    os = "HP Tru64 UNIX";
    #elif defined(Q_OS_QNX)
    os = "QNX Neutrino";
    #elif defined(Q_OS_RELIANT)
    os = "Reliant UNIX";
    #elif defined(Q_OS_SCO)
    os = "SCO OpenServer 5";
    #elif defined(Q_OS_SOLARIS)
    os = "Sun Solaris";
    #elif defined(Q_OS_SYMBIAN)
    os = "Symbian";
    #elif defined(Q_OS_ULTRIX)
    os = "DEC Ultrix";
    #elif defined(Q_OS_UNIX)
    os = "UNIX BSD/SYSV";
    #elif defined(Q_OS_UNIXWARE)
    os = "UnixWare";
    #elif defined(Q_OS_WIN32)
    os = "Windows";
    #elif defined(Q_OS_WINCE)
    os = "Windows CE";
    #endif
    debug_message("Platform: " + os);
    return os;
}

/**
 * .
 */
String
design_details_action(String args)
{
    no_argument_debug("about_action()", args);

    QGraphicsScene* scene = _mainWin->activeScene();
    if (scene) {
        EmbDetailsDialog dialog(scene, _mainWin);
        dialog.exec();
    }
    return "";
}

/**
 * @brief about_action
 * @param args
 * @return
 */
String
about_action(String args)
{
    no_argument_debug("about_action()", args);

    //TODO: QTabWidget for about dialog
    QApplication::setOverrideCursor(Qt::ArrowCursor);
    QString appDir = qApp->applicationDirPath();
    QString appName = QApplication::applicationName();
    QString title = "About " + appName;

    QDialog dialog(_mainWin);
    ImageWidget img(appDir + "/images/logo-small");
    QLabel text(appName + tr("\n\n") +
                          tr("https://www.libembroidery.org") +
                          tr("\n\n") +
                          tr("Available Platforms: GNU/Linux, Windows, Mac OSX, Raspberry Pi") +
                          tr("\n\n") +
                          tr("Embroidery formats by Josh Varga.") +
                          tr("\n") +
                          tr("User Interface by Jonathan Greig.") +
                          tr("\n\n") +
                          tr("Free under the zlib/libpng license.")
                          #if defined(BUILD_GIT_HASH)
                          + tr("\n\n") +
                          tr("Build Hash: ") + qPrintable(BUILD_GIT_HASH)
                          #endif
                          );
    text.setWordWrap(true);

    QDialogButtonBox buttonbox(Qt::Horizontal, &dialog);
    QPushButton button(&dialog);
    button.setText("Oh, Yeah!");
    buttonbox.addButton(&button, QDialogButtonBox::AcceptRole);
    buttonbox.setCenterButtons(true);
    // TODO reconnect
    //connect(&buttonbox, SIGNAL(accepted()), &dialog, SLOT(accept()));

    QVBoxLayout layout;
    layout.setAlignment(Qt::AlignCenter);
    layout.addWidget(&img);
    layout.addWidget(&text);
    layout.addWidget(&buttonbox);

    dialog.setWindowTitle(title);
    dialog.setMinimumWidth(img.minimumWidth()+30);
    dialog.setMinimumHeight(img.minimumHeight()+50);
    dialog.setLayout(&layout);
    dialog.exec();
    QApplication::restoreOverrideCursor();
}

/**
 * @brief whats_this_action
 * @param args
 * @return
 */
String
whats_this_action(String args)
{
    no_argument_debug("whats_this_action()", args);
    QWhatsThis::enterWhatsThisMode();
    return "";
}

/**
 * @brief print_action
 * @param args
 * @return
 */
String
print_action(String args)
{
    no_argument_debug("print_action()", args);
    MdiWindow* mdiWin = qobject_cast<MdiWindow*>(mdiArea->activeSubWindow());
    if (mdiWin) {
        mdiWin->print();
    }
    return "";
}

/**
 * @brief tip_of_the_day_action
 * @param args
 * @return
 */
String
tip_of_the_day_action(String args)
{
    no_argument_debug("tip_of_the_day_action()", args);

    QString appDir = qApp->applicationDirPath();

    wizardTipOfTheDay = new QWizard(_mainWin);
    wizardTipOfTheDay->setAttribute(Qt::WA_DeleteOnClose);
    wizardTipOfTheDay->setWizardStyle(QWizard::ModernStyle);
    wizardTipOfTheDay->setMinimumSize(550, 400);

    QWizardPage* page = new QWizardPage(wizardTipOfTheDay);

    ImageWidget* imgBanner = new ImageWidget(appDir + "/images/did-you-know.png", wizardTipOfTheDay);

    if (settings.general_current_tip >= listTipOfTheDay.size())
        settings.general_current_tip = 0;
    labelTipOfTheDay = new QLabel(listTipOfTheDay.value(settings.general_current_tip), wizardTipOfTheDay);
    labelTipOfTheDay->setWordWrap(true);

    QCheckBox* checkBoxTipOfTheDay = new QCheckBox(tr("&Show tips on startup"), wizardTipOfTheDay);
    checkBoxTipOfTheDay->setChecked(settings.general_tip_of_the_day);
    // TODO reconnect
    //connect(checkBoxTipOfTheDay, SIGNAL(stateChanged(int)), _mainWin, SLOT(checkBoxTipOfTheDayStateChanged(int)));

    QVBoxLayout* layout = new QVBoxLayout(wizardTipOfTheDay);
    layout->addWidget(imgBanner);
    layout->addStrut(1);
    layout->addWidget(labelTipOfTheDay);
    layout->addStretch(1);
    layout->addWidget(checkBoxTipOfTheDay);
    page->setLayout(layout);
    wizardTipOfTheDay->addPage(page);

    wizardTipOfTheDay->setWindowTitle("Tip of the Day");

    //TODO: Add icons to buttons by using wizardTipOfTheDay->setButton(QWizard::CustomButton1, buttonPrevious)
    //TODO: Add icons to buttons by using wizardTipOfTheDay->setButton(QWizard::CustomButton1, buttonNext)
    //TODO: Add icons to buttons by using wizardTipOfTheDay->setButton(QWizard::CustomButton1, buttonClose)
    wizardTipOfTheDay->setButtonText(QWizard::CustomButton1, tr("&Previous"));
    wizardTipOfTheDay->setButtonText(QWizard::CustomButton2, tr("&Next"));
    wizardTipOfTheDay->setButtonText(QWizard::CustomButton3, tr("&Close"));
    wizardTipOfTheDay->setOption(QWizard::HaveCustomButton1, true);
    wizardTipOfTheDay->setOption(QWizard::HaveCustomButton2, true);
    wizardTipOfTheDay->setOption(QWizard::HaveCustomButton3, true);
    // TODO reconnect
    //connect(wizardTipOfTheDay, SIGNAL(customButtonClicked(int)), _mainWin, SLOT(buttonTipOfTheDayClicked(int)));

    QList<QWizard::WizardButton> listTipOfTheDayButtons;
    listTipOfTheDayButtons << QWizard::Stretch << QWizard::CustomButton1 << QWizard::CustomButton2 << QWizard::CustomButton3;
    wizardTipOfTheDay->setButtonLayout(listTipOfTheDayButtons);

    wizardTipOfTheDay->exec();
    return "";
}

/**
 * .
 */
void
MainWindow::checkBoxTipOfTheDayStateChanged(int checked)
{
    settings.general_tip_of_the_day = checked;
}

/**
 * .
 */
void
MainWindow::buttonTipOfTheDayClicked(int button)
{
    debug_message("buttonTipOfTheDayClicked(%d)" + std::to_string(button));
    if (button == QWizard::CustomButton1) {
        if (settings.general_current_tip > 0) {
            settings.general_current_tip--;
        }
        else {
            settings.general_current_tip = listTipOfTheDay.size()-1;
        }
        labelTipOfTheDay->setText(listTipOfTheDay.value(settings.general_current_tip));
    }
    else if (button == QWizard::CustomButton2) {
        settings.general_current_tip++;
        if (settings.general_current_tip >= listTipOfTheDay.size()) {
            settings.general_current_tip = 0;
        }
        labelTipOfTheDay->setText(listTipOfTheDay.value(settings.general_current_tip));
    }
    else if (button == QWizard::CustomButton3) {
        wizardTipOfTheDay->close();
    }
}

/**
 * @brief help_action
 * @param args
 * @return
 */
static String
help_action(String args)
{
    debug_message("help()");

    // Open the HTML Help in the default browser
    QUrl helpURL("file:///" + qApp->applicationDirPath() + "/help/doc-index.html");
    QDesktopServices::openUrl(helpURL);

    //TODO: This is how to start an external program. Use this elsewhere...
    //QString program = "firefox";
    //QStringList arguments;
    //arguments << "help/commands.html";
    //QProcess *myProcess = new QProcess(this);
    //myProcess->start(program, arguments);
    return "";
}

/**
 * @brief changelog_action
 * @param args
 * @return
 */
static String
changelog_action(String args)
{
    debug_message("changelog()");

    QUrl changelogURL("help/changelog.html");
    QDesktopServices::openUrl(changelogURL);
    return "";
}

/**
 * @brief undo_action
 * @param args
 * @return
 */
static String
undo_action(String args)
{
    debug_message("undo()");
    QString prefix = prompt->getPrefix();
    if (dockUndoEdit->canUndo()) {
        actuator("set-prompt-prefix Undo "
            + dockUndoEdit->undoText().toStdString());
        /* \bug this won't append history because there's no mechanism
           for and empty string */
        actuator("append-history ");
        dockUndoEdit->undo();
        actuator("set-prompt-prefix " + prefix.toStdString());
    }
    else {
        prompt->alert("Nothing to undo");
        actuator("set-prompt-prefix " + prefix.toStdString());
    }
    return "";
}

/**
 * @brief redo_action
 * @param args
 * @return
 */
static String
redo_action(String args)
{
    debug_message("redo()");
    QString prefix = prompt->getPrefix();
    if (dockUndoEdit->canRedo()) {
        actuator("set-prompt-prefix Redo " + dockUndoEdit->redoText().toStdString());
        actuator("append-history ");
        dockUndoEdit->redo();
        actuator("set-prompt-prefix " + prefix.toStdString());
    }
    else {
        prompt->alert("Nothing to redo");
        actuator("set-prompt-prefix " + prefix.toStdString());
    }
    return "";
}

bool MainWindow::isShiftPressed()
{
    return shiftKeyPressedState;
}

void
MainWindow::setShiftPressed()
{
    shiftKeyPressedState = true;
}

void
MainWindow::setShiftReleased()
{
    shiftKeyPressedState = false;
}

// Icons
void
MainWindow::iconResize(int iconSize)
{
    this->setIconSize(QSize(iconSize, iconSize));
    layerSelector->setIconSize(QSize(iconSize*4, iconSize));
    colorSelector->setIconSize(QSize(iconSize, iconSize));
    linetypeSelector->setIconSize(QSize(iconSize*4, iconSize));
    lineweightSelector->setIconSize(QSize(iconSize*4, iconSize));
    //set the minimum combobox width so the text is always readable
    layerSelector->setMinimumWidth(iconSize*4);
    colorSelector->setMinimumWidth(iconSize*2);
    linetypeSelector->setMinimumWidth(iconSize*4);
    lineweightSelector->setMinimumWidth(iconSize*4);

    //TODO: low-priority: open app with iconSize set to 128. resize the icons to a smaller size.

    settings.general_icon_size = iconSize;
}

/**
 * @brief MainWindow::activeMdiWindow
 * @return
 */
MdiWindow *
MainWindow::activeMdiWindow()
{
    debug_message("activeMdiWindow()");
    MdiWindow* mdiWin = qobject_cast<MdiWindow*>(mdiArea->activeSubWindow());
    return mdiWin;
}

/**
 * @brief MainWindow::activeView
 * @return
 */
View *
MainWindow::activeView()
{
    debug_message("activeView()");
    MdiWindow* mdiWin = qobject_cast<MdiWindow*>(mdiArea->activeSubWindow());
    if (mdiWin) {
        return mdiWin->gview;
    }
    return 0;
}

/**
 * @brief MainWindow::activeScene
 * @return
 */
QGraphicsScene *
MainWindow::activeScene()
{
    debug_message("activeScene()");
    MdiWindow* mdiWin = qobject_cast<MdiWindow*>(mdiArea->activeSubWindow());
    if (mdiWin) {
        return mdiWin->gscene;
    }
    return 0;
}

QUndoStack* MainWindow::activeUndoStack()
{
    debug_message("activeUndoStack()");
    View* v = _mainWin->activeView();
    if (v) {
        QUndoStack* u = v->getUndoStack();
        return u;
    }
    return 0;
}

void
MainWindow::setUndoCleanIcon(bool opened)
{
    dockUndoEdit->updateCleanIcon(opened);
}

void
MainWindow::updateAllViewScrollBars(bool val)
{
    QList<QMdiSubWindow*> windowList = mdiArea->subWindowList();
    for (int i = 0; i < windowList.count(); ++i) {
        MdiWindow* mdiWin = qobject_cast<MdiWindow*>(windowList.at(i));
        if (mdiWin) {
            mdiWin->showViewScrollBars(val);
        }
    }
}

void
MainWindow::updateAllViewCrossHairColors(QRgb color)
{
    QList<QMdiSubWindow*> windowList = mdiArea->subWindowList();
    for (int i = 0; i < windowList.count(); ++i)
    {
        MdiWindow* mdiWin = qobject_cast<MdiWindow*>(windowList.at(i));
        if (mdiWin) { mdiWin->setViewCrossHairColor(color); }
    }
}

void
MainWindow::updateAllViewBackgroundColors(QRgb color)
{
    QList<QMdiSubWindow*> windowList = mdiArea->subWindowList();
    for (int i = 0; i < windowList.count(); ++i)
    {
        MdiWindow* mdiWin = qobject_cast<MdiWindow*>(windowList.at(i));
        if (mdiWin) {
            mdiWin->setViewBackgroundColor(color);
        }
    }
}

void
MainWindow::updateAllViewSelectBoxColors(QRgb colorL, QRgb fillL, QRgb colorR, QRgb fillR, int alpha)
{
    QList<QMdiSubWindow*> windowList = mdiArea->subWindowList();
    for (int i = 0; i < windowList.count(); ++i) {
        MdiWindow* mdiWin = qobject_cast<MdiWindow*>(windowList.at(i));
        if (mdiWin) {
            mdiWin->setViewSelectBoxColors(colorL, fillL, colorR, fillR, alpha);
        }
    }
}

void
MainWindow::updateAllViewGridColors(QRgb color)
{
    QList<QMdiSubWindow*> windowList = mdiArea->subWindowList();
    for (int i = 0; i < windowList.count(); ++i) {
        MdiWindow* mdiWin = qobject_cast<MdiWindow*>(windowList.at(i));
        if (mdiWin) {
            mdiWin->setViewGridColor(color);
        }
    }
}

void
MainWindow::updateAllViewRulerColors(QRgb color)
{
    QList<QMdiSubWindow*> windowList = mdiArea->subWindowList();
    for (int i = 0; i < windowList.count(); ++i) {
        MdiWindow* mdiWin = qobject_cast<MdiWindow*>(windowList.at(i));
        if (mdiWin) {
            mdiWin->setViewRulerColor(color);
        }
    }
}

void
MainWindow::updatePickAddMode(bool val)
{
    settings.selection_mode_pickadd = val;
    dockPropEdit->updatePickAddModeButton(val);
}

void
MainWindow::pickAddModeToggled()
{
    updatePickAddMode(!settings.selection_mode_pickadd);
}

// Layer ToolBar
void
MainWindow::makeLayerActive()
{
    debug_message("makeLayerActive()");
    debug_message("TODO: Implement makeLayerActive.");
}

/**
 * .
 */
void
MainWindow::layerManager()
{
    debug_message("layerManager()");
    debug_message("TODO: Implement layerManager.");
    LayerManager layman(this);
    layman.exec();
}

/**
 * .
 */
void
MainWindow::layerPrevious()
{
    debug_message("layerPrevious()");
    debug_message("TODO: Implement layerPrevious.");
}

/**
 * .
 */
String
pan_action(String mode)
{
    View* gview = _mainWin->activeView();
    QUndoStack* stack = gview->getUndoStack();
    if (gview && stack) {
        if (mode == "realtime") {
            debug_message("panrealtime()");
            gview->panRealTime();
            return "";
        }
        if (mode == "point") {
            debug_message("panpoint()");
            gview->panPoint();
            return "";
        }
        if (mode == "left") {
            debug_message("panLeft()");
            UndoableNavCommand* cmd = new UndoableNavCommand("PanLeft", gview, 0);
            stack->push(cmd);
            return "";
        }
        if (mode == "right") {
            debug_message("panRight()");
            UndoableNavCommand* cmd = new UndoableNavCommand("PanRight", gview, 0);
            stack->push(cmd);
            return "";
        }
        if (mode == "up") {
            debug_message("panUp()");
            UndoableNavCommand* cmd = new UndoableNavCommand("PanUp", gview, 0);
            stack->push(cmd);
            return "";
        }
        if (mode == "down") {
            debug_message("panDown()");
            UndoableNavCommand* cmd = new UndoableNavCommand("PanDown", gview, 0);
            stack->push(cmd);
            return "";
        }
    }
    return "</br>ERROR: pan subcommand not recognised.";
}

/**
 * @brief zoom_action
 * @param mode
 * @return
 */
String
zoom_action(String mode)
{
    View* gview = _mainWin->activeView();
    QUndoStack* stack = gview->getUndoStack();
    if (gview && stack) {
        if (mode == "realtime") {
            debug_message("zoomRealtime()");
            debug_message("TODO: Implement zoomRealtime.");
            return "";
        }
        if (mode == "previous") {
            debug_message("zoomPrevious()");
            debug_message("TODO: Implement zoomPrevious.");
            return "";
        }
        if (mode == "window") {
            debug_message("zoomWindow()");
            gview->zoomWindow();
            return "";
        }
        if (mode == "dynamic") {
            debug_message("zoomDynamic()");
            debug_message("TODO: Implement zoomDynamic.");
            return "";
        }
        if (mode == "scale") {
            debug_message("zoomScale()");
            debug_message("TODO: Implement zoomScale.");
            return "";
        }
        if (mode == "center") {
            debug_message("zoomCenter()");
            debug_message("TODO: Implement zoomCenter.");
            return "";
        }
        if (mode == "in") {
            debug_message("zoomIn()");
            gview->zoomIn();
            return "";
        }
        if (mode == "out") {
            debug_message("zoomOut()");
            gview->zoomOut();
            return "";
        }
        if (mode == "selected") {
            debug_message("zoomSelected()");
            UndoableNavCommand* cmd = new UndoableNavCommand("ZoomSelected", gview, 0);
            stack->push(cmd);
            return "";
        }
        if (mode == "all") {
            debug_message("zoomAll()");
            debug_message("TODO: Implement zoomAll.");
            return "";
        }
        if (mode == "extents") {
            debug_message("zoomExtents()");
            UndoableNavCommand* cmd = new UndoableNavCommand("ZoomExtents", gview, 0);
            stack->push(cmd);
            return "";
        }
    }
    return "</br>ERROR: zoom subcommand not recognised.";
}

/**
 * @brief MainWindow::dayVision
 *  \todo Make day vision color settings.
 */
String
day_vision_action(String args)
{
    no_argument_debug("day_vision_action()", args);
    View* gview = _mainWin->activeView();
    if (gview) {
        gview->setBackgroundColor(qRgb(255,255,255));
        gview->setCrossHairColor(qRgb(0,0,0));
        gview->setGridColor(qRgb(0,0,0));
    }
    return "";
}

/**
 * @brief MainWindow::nightVision
 * \todo Make night vision color settings.
 */
String
night_vision_action(String args)
{
    no_argument_debug("night_vision_action()", args);
    View* gview = _mainWin->activeView();
    if (gview) {
        gview->setBackgroundColor(qRgb(0,0,0));
        gview->setCrossHairColor(qRgb(255,255,255));
        gview->setGridColor(qRgb(255,255,255));
    }
    return "";
}

void
MainWindow::layerSelectorIndexChanged(int index)
{
    debug_message("layerSelectorIndexChanged(%d)" + std::to_string(index));
}

void
MainWindow::colorSelectorIndexChanged(int index)
{
    debug_message("colorSelectorIndexChanged(%d)" + std::to_string(index));

    QComboBox* comboBox = qobject_cast<QComboBox*>(sender());
    QRgb newColor;
    if (comboBox) {
        bool ok = 0;
        //TODO: Handle ByLayer and ByBlock and Other...
        newColor = comboBox->itemData(index).toUInt(&ok);
        if (!ok)
            QMessageBox::warning(this, tr("Color Selector Conversion Error"), tr("<b>An error has occured while changing colors.</b>"));
    }
    else
        QMessageBox::warning(this, tr("Color Selector Pointer Error"), tr("<b>An error has occured while changing colors.</b>"));

    MdiWindow* mdiWin = qobject_cast<MdiWindow*>(mdiArea->activeSubWindow());
    if (mdiWin) { mdiWin->currentColorChanged(newColor); }
}

/**
 * .
 */
void
MainWindow::linetypeSelectorIndexChanged(int index)
{
    debug_message("linetypeSelectorIndexChanged(%d)" + std::to_string(index));
}

/**
 * .
 */
void
MainWindow::lineweightSelectorIndexChanged(int index)
{
    debug_message("lineweightSelectorIndexChanged(%d)" + std::to_string(index));
}

/**
 * .
 */
void
MainWindow::textFontSelectorCurrentFontChanged(const QFont& font)
{
    debug_message("textFontSelectorCurrentFontChanged()");
    settings.text_font = font.family().toStdString();
}

/**
 * .
 */
void
MainWindow::textSizeSelectorIndexChanged(int index)
{
    debug_message("textSizeSelectorIndexChanged(%d)" + std::to_string(index));
    settings.text_size = fabs(textSizeSelector->itemData(index).toReal()); //TODO: check that the toReal() conversion is ok
}

/**
 * .
 */
void
MainWindow::setTextFont(const QString& str)
{
    textFontSelector->setCurrentFont(QFont(str));
    settings.text_font = str.toStdString();
}

/**
 * .
 */
void
MainWindow::setTextSize(EmbReal num)
{
    settings.text_size = fabs(num);
    int index = textSizeSelector->findText("Custom", Qt::MatchContains);
    if (index != -1) {
        textSizeSelector->removeItem(index);
    }
    textSizeSelector->addItem("Custom " + QString().setNum(num, 'f', 2) + " pt", num);
    index = textSizeSelector->findText("Custom", Qt::MatchContains);
    if (index != -1) {
        textSizeSelector->setCurrentIndex(index);
    }
}

/**
 * .
 */
QString MainWindow::getCurrentLayer()
{
    MdiWindow* mdiWin = qobject_cast<MdiWindow*>(mdiArea->activeSubWindow());
    if (mdiWin) {
        return mdiWin->curLayer;
    }
    return "0";
}

/**
 * .
 */
QRgb MainWindow::getCurrentColor()
{
    MdiWindow* mdiWin = qobject_cast<MdiWindow*>(mdiArea->activeSubWindow());
    if (mdiWin) {
        return mdiWin->curColor;
    }
    return 0; //TODO: return color ByLayer
}

/**
 * .
 */
QString MainWindow::getCurrentLineType()
{
    MdiWindow* mdiWin = qobject_cast<MdiWindow*>(mdiArea->activeSubWindow());
    if (mdiWin) {
        return mdiWin->curLineType;
    }
    return "ByLayer";
}

/**
 * .
 */
QString MainWindow::getCurrentLineWeight()
{
    MdiWindow* mdiWin = qobject_cast<MdiWindow*>(mdiArea->activeSubWindow());
    if (mdiWin) {
        return mdiWin->curLineWeight;
    }
    return "ByLayer";
}

/**
 * .
 */
void
MainWindow::deletePressed()
{
    debug_message("deletePressed()");
    QApplication::setOverrideCursor(Qt::WaitCursor);
    MdiWindow* mdiWin = qobject_cast<MdiWindow*>(mdiArea->activeSubWindow());
    if (mdiWin) {
        mdiWin->deletePressed();
    }
    QApplication::restoreOverrideCursor();
}

/**
 * .
 */
void
MainWindow::escapePressed()
{
    debug_message("escapePressed()");
    QApplication::setOverrideCursor(Qt::WaitCursor);
    MdiWindow* mdiWin = qobject_cast<MdiWindow*>(mdiArea->activeSubWindow());
    if (mdiWin) {
        mdiWin->escapePressed();
    }
    QApplication::restoreOverrideCursor();

    actuator("end");
}

/**
 * .
 */
void
MainWindow::toggleGrid()
{
    debug_message("toggleGrid()");
    statusbar->statusBarGridButton->toggle();
}

/**
 * .
 */
void
MainWindow::toggleRuler()
{
    debug_message("toggleRuler()");
    statusbar->statusBarRulerButton->toggle();
}

/**
 * .
 */
void
MainWindow::toggleLwt()
{
    debug_message("toggleLwt()");
    statusbar->statusBarLwtButton->toggle();
}

/**
 * .
 */
void
MainWindow::promptHistoryAppended(const QString& txt)
{
    MdiWindow* mdiWin = activeMdiWindow();
    if (mdiWin)
        mdiWin->promptHistoryAppended(txt);
}

/**
 * .
 */
void
MainWindow::logPromptInput(const QString& txt)
{
    MdiWindow* mdiWin = activeMdiWindow();
    if (mdiWin)
        mdiWin->logPromptInput(txt);
}

/**
 * .
 */
void
MainWindow::promptInputPrevious()
{
    MdiWindow* mdiWin = activeMdiWindow();
    if (mdiWin)
        mdiWin->promptInputPrevious();
}

/**
 * .
 */
void
MainWindow::promptInputNext()
{
    MdiWindow* mdiWin = activeMdiWindow();
    if (mdiWin) {
        mdiWin->promptInputNext();
    }
}

/**
 * .
 */
void
MainWindow::nativeAlert(const QString& txt)
{
    prompt->alert(txt);
}

/**
 * @brief MainWindow::nativeMessageBox
 * \a type
 * \a title
 * \a text
 */
void
MainWindow::nativeMessageBox(const QString& type, const QString& title, const QString& text)
{
    QString msgType = type.toLower();
    if (msgType == "critical") {
        QMessageBox::critical(this, tr(qPrintable(title)), tr(qPrintable(text)));
    }
    else if (msgType == "information") {
        QMessageBox::information(this, tr(qPrintable(title)), tr(qPrintable(text)));
    }
    else if (msgType == "question") {
        QMessageBox::question(this, tr(qPrintable(title)), tr(qPrintable(text)));
    }
    else if (msgType == "warning") {
        QMessageBox::warning(this, tr(qPrintable(title)), tr(qPrintable(text)));
    }
    else {
        QMessageBox::critical(this, tr("Native MessageBox Error"), tr("Incorrect use of the native messageBox function."));
    }
}

/**
 * @brief MainWindow::nativePrintArea
 * \a x \a y \a w \a h
 */
void
MainWindow::nativePrintArea(EmbReal x, EmbReal y, EmbReal w, EmbReal h)
{
    qDebug("nativePrintArea(%.2f, %.2f, %.2f, %.2f)", x, y, w, h);
    //TODO: Print Setup Stuff
    print_action("");
}

/**
 * .
 */
void
MainWindow::nativeSetBackgroundColor(quint8 r, quint8 g, quint8 b)
{
    settings.display_bg_color = qRgb(r,g,b);
    updateAllViewBackgroundColors(qRgb(r,g,b));
}

/**
 * .
 */
void
MainWindow::nativeSetCrossHairColor(quint8 r, quint8 g, quint8 b)
{
    settings.display_crosshair_color = qRgb(r,g,b);
    updateAllViewCrossHairColors(qRgb(r,g,b));
}

/**
 * .
 */
void
MainWindow::nativeSetGridColor(quint8 r, quint8 g, quint8 b)
{
    settings.grid_color = qRgb(r,g,b);
    updateAllViewGridColors(qRgb(r,g,b));
}

/**
 * .
 */
void
MainWindow::nativePreviewOn(int clone, int mode, EmbReal x, EmbReal y, EmbReal data)
{
    View* gview = _mainWin->activeView();
    if (gview) gview->previewOn(clone, mode, x, -y, data);
}

/**
 * .
 */
void
MainWindow::nativePreviewOff()
{
    View* gview = _mainWin->activeView();
    if (gview) gview->previewOff();
}

/**
 * .
 */
void
MainWindow::nativeClearRubber()
{
    View* gview = _mainWin->activeView();
    if (gview) gview->clearRubberRoom();
}

/**
 * .
 */
bool MainWindow::nativeAllowRubber()
{
    View* gview = _mainWin->activeView();
    if (gview) return gview->allowRubber();
    return false;
}

/**
 * .
 */
void
MainWindow::nativeSpareRubber(qint64 id)
{
    View* gview = _mainWin->activeView();
    if (gview)
        gview->spareRubber(id);
}

/**
 * .
 */
String
MainWindow::nativeSetRubberMode(NodeList a)
{
    String mode = QString::fromStdString(a[0].s).toUpper().toStdString();

    View* gview = _mainWin->activeView();
    if (gview) {
        if (mode == "CIRCLE_1P_RAD") {
            gview->setRubberMode(OBJ_RUBBER_CIRCLE_1P_RAD);
        }
        else if (mode == "CIRCLE_1P_DIA") {
            gview->setRubberMode(OBJ_RUBBER_CIRCLE_1P_DIA);
        }
        else if (mode == "CIRCLE_2P") {
            gview->setRubberMode(OBJ_RUBBER_CIRCLE_2P);
        }
        else if (mode == "CIRCLE_3P") {
            gview->setRubberMode(OBJ_RUBBER_CIRCLE_3P);
        }
        else if (mode == "CIRCLE_TTR") {
            gview->setRubberMode(OBJ_RUBBER_CIRCLE_TTR);
        }
        else if (mode == "CIRCLE_TTR") {
            gview->setRubberMode(OBJ_RUBBER_CIRCLE_TTT);
        }
        else if (mode == "DIMLEADER_LINE") {
            gview->setRubberMode(OBJ_RUBBER_DIMLEADER_LINE);
        }
        else if (mode == "ELLIPSE_LINE") {
            gview->setRubberMode(OBJ_RUBBER_ELLIPSE_LINE);
        }
        else if (mode == "ELLIPSE_MAJORDIAMETER_MINORRADIUS") {
            gview->setRubberMode(OBJ_RUBBER_ELLIPSE_MAJORDIAMETER_MINORRADIUS);
        }
        else if (mode == "ELLIPSE_MAJORRADIUS_MINORRADIUS") {
            gview->setRubberMode(OBJ_RUBBER_ELLIPSE_MAJORRADIUS_MINORRADIUS);
        }
        else if (mode == "ELLIPSE_ROTATION") {
            gview->setRubberMode(OBJ_RUBBER_ELLIPSE_ROTATION);
        }
        else if (mode == "LINE") {
            gview->setRubberMode(OBJ_RUBBER_LINE);
        }
        else if (mode == "POLYGON") {
            gview->setRubberMode(OBJ_RUBBER_POLYGON);
        }
        else if (mode == "POLYGON_INSCRIBE") {
            gview->setRubberMode(OBJ_RUBBER_POLYGON_INSCRIBE);
        }
        else if (mode == "POLYGON_CIRCUMSCRIBE") {
            gview->setRubberMode(OBJ_RUBBER_POLYGON_CIRCUMSCRIBE);
        }
        else if (mode == "POLYLINE") {
            gview->setRubberMode(OBJ_RUBBER_POLYLINE);
        }
        else if (mode == "RECTANGLE") {
            gview->setRubberMode(OBJ_RUBBER_RECTANGLE);
        }
        else if (mode == "TEXTSINGLE") {
            gview->setRubberMode(OBJ_RUBBER_TEXTSINGLE);
        }
        else {
            return "ERROR: setRubberMode(): unknown rubberMode value";
            //return context->throwError(QScriptContext::UnknownError, "setRubberMode(): unknown rubberMode value");
        }
    }
    return "";
}

/**
 * .
 */
void
MainWindow::nativeSetRubberPoint(const QString& key, EmbReal x, EmbReal y)
{
    /*
    _mainWin->setRubberPoint(a[0].s.toUpper(), a[1].r, a[2].r);
    */
    View* gview = _mainWin->activeView();
    if (gview) {
        gview->setRubberPoint(key, QPointF(x, -y));
    }
}

/**
 * .
 */
void
MainWindow::nativeSetRubberText(const QString& key, const QString& txt)
{
    View* gview = _mainWin->activeView();
    if (gview) {
        gview->setRubberText(key, txt);
    }
}

void
MainWindow::nativeAddTextMulti(const QString& str, EmbReal x, EmbReal y, EmbReal rot, bool fill, int rubberMode)
{
    /*
    _mainWin->nativeAddTextMulti(a[0].s, a[1].r, a[2].r, a[3].r, a[4].b, OBJ_RUBBER_OFF);
    */
}

void
MainWindow::nativeAddTextSingle(const QString& str, EmbReal x, EmbReal y, EmbReal rot, bool fill, int rubberMode)
{
    /*
    _mainWin->nativeAddTextSingle(a[0].s, a[1].r, a[2].r, a[3].r, a[4].b, OBJ_RUBBER_OFF);
    */
    View* gview = _mainWin->activeView();
    QGraphicsScene* gscene = gview->scene();
    QUndoStack* stack = gview->getUndoStack();
    if (gview && gscene && stack) {
        TextSingleObject* obj = new TextSingleObject(str, x, -y, getCurrentColor());
        obj->setObjectTextFont(QString::fromStdString(settings.text_font));
        obj->setObjectTextSize(settings.text_size);
        obj->setObjectTextStyle(settings.text_style_bold,
                                settings.text_style_italic,
                                settings.text_style_underline,
                                settings.text_style_strikeout,
                                settings.text_style_overline);
        obj->setObjectTextBackward(false);
        obj->setObjectTextUpsideDown(false);
        obj->setRotation(-rot);
        //TODO: single line text fill
        obj->setObjectRubberMode(rubberMode);
        if (rubberMode) {
            gview->addToRubberRoom(obj);
            gscene->addItem(obj);
            gscene->update();
        }
        else {
            UndoableAddCommand* cmd = new UndoableAddCommand(obj->data(OBJ_NAME).toString(), obj, gview, 0);
            stack->push(cmd);
        }
    }
}

/**
 * .
 */
void
MainWindow::nativeAddInfiniteLine(EmbReal x1, EmbReal y1, EmbReal x2, EmbReal y2, EmbReal rot)
{
    /*
    //TODO: Node error checking
    debug_message("TODO: finish addInfiniteLine command");
    */
}

/**
 * .
 */
void
MainWindow::nativeAddRay(EmbReal x1, EmbReal y1, EmbReal x2, EmbReal y2, EmbReal rot)
{
    /*
    //TODO: Node error checking
    debug_message("TODO: finish addRay command");
    */
}

/**
 * .
 */
void
MainWindow::nativeAddLine(EmbReal x1, EmbReal y1, EmbReal x2, EmbReal y2, EmbReal rot, int rubberMode)
{
    /*
    _mainWin->nativeAddLine(a[0].r, a[1].r, a[2].r, a[3].r, a[4].r, OBJ_RUBBER_OFF);
    */
    View* gview = _mainWin->activeView();
    QGraphicsScene* gscene = gview->scene();
    QUndoStack* stack = gview->getUndoStack();
    if (gview && gscene && stack) {
        EmbLine line;
        line.start.x = x1;
        line.start.y = -y1;
        line.end.x = x2;
        line.end.y = -y2;
        LineObject* obj = new LineObject(line, getCurrentColor());
        obj->setRotation(-rot);
        obj->setObjectRubberMode(rubberMode);
        if (rubberMode) {
            gview->addToRubberRoom(obj);
            gscene->addItem(obj);
            gscene->update();
        }
        else {
            UndoableAddCommand* cmd = new UndoableAddCommand(obj->data(OBJ_NAME).toString(), obj, gview, 0);
            stack->push(cmd);
        }
    }
}

void
MainWindow::nativeAddTriangle(EmbReal x1, EmbReal y1, EmbReal x2, EmbReal y2, EmbReal x3, EmbReal y3, EmbReal rot, bool fill)
{
    /*
    AddTriangle(NodeList a)
    _mainWin->nativeAddTriangle(a[0].r, a[1].r, a[2].r, a[3].r, a[4].r, a[5].r, a[6].r, a[7].b);
    */
}

void
MainWindow::nativeAddRectangle(NodeList a)
{
    EmbReal x = a[0].r;
    EmbReal y = a[1].r;
    EmbReal w = a[2].r;
    EmbReal h = a[3].r;
    EmbReal rot = a[4].r;
    bool fill = a[5].b;
    int rubberMode = a[6].i;

    View* gview = _mainWin->activeView();
    QGraphicsScene* gscene = gview->scene();
    QUndoStack* stack = gview->getUndoStack();
    if (gview && gscene && stack) {
        RectObject* obj = new RectObject(x, -y, w, -h, getCurrentColor());
        obj->setRotation(-rot);
        obj->setObjectRubberMode(rubberMode);
        //TODO: rect fill
        if (rubberMode) {
            gview->addToRubberRoom(obj);
            gscene->addItem(obj);
            gscene->update();
        }
        else {
            UndoableAddCommand* cmd = new UndoableAddCommand(obj->data(OBJ_NAME).toString(), obj, gview, 0);
            stack->push(cmd);
        }
    }
}

void
MainWindow::nativeAddRoundedRectangle(EmbReal x, EmbReal y, EmbReal w, EmbReal h, EmbReal rad, EmbReal rot, bool fill)
{
    /*
    String
    AddRoundedRectangle(NodeList a)
    {
        _mainWin->nativeAddRoundedRectangle(
            a[0].r, a[1].r, a[2].r, a[3].r, a[4].r, a[5].r, a[6].b);
        return "";
    }
    */
}

void
MainWindow::nativeAddArc(EmbReal startX, EmbReal startY, EmbReal midX, EmbReal midY, EmbReal endX, EmbReal endY, int rubberMode)
{
    View* gview = _mainWin->activeView();
    QGraphicsScene* scene = activeScene();
    if (gview && scene) {
        EmbArc arc;
        arc.start.x = startX;
        arc.start.x = -startY;
        arc.mid.x = midX;
        arc.mid.x = -midY;
        arc.end.x = endX;
        arc.end.x = -endY;
        ArcObject* arcObj = new ArcObject(arc, getCurrentColor());
        arcObj->setObjectRubberMode(rubberMode);
        if (rubberMode) {
            gview->addToRubberRoom(arcObj);
        }
        scene->addItem(arcObj);
        scene->update();
    }
}

void
MainWindow::nativeAddCircle(EmbReal centerX, EmbReal centerY, EmbReal radius, bool fill, int rubberMode)
{
    View* gview = _mainWin->activeView();
    QGraphicsScene* gscene = gview->scene();
    QUndoStack* stack = gview->getUndoStack();
    if (gview && gscene && stack) {
        CircleObject* obj = new CircleObject(centerX, -centerY, radius, getCurrentColor());
        obj->setObjectRubberMode(rubberMode);
        //TODO: circle fill
        if (rubberMode) {
            gview->addToRubberRoom(obj);
            gscene->addItem(obj);
            gscene->update();
        }
        else {
            UndoableAddCommand* cmd = new UndoableAddCommand(obj->data(OBJ_NAME).toString(), obj, gview, 0);
            stack->push(cmd);
        }
    }
}

void
MainWindow::nativeAddSlot(EmbReal centerX, EmbReal centerY, EmbReal diameter, EmbReal length, EmbReal rot, bool fill, int rubberMode)
{
    //TODO: Use UndoableAddCommand for slots
    /*
    SlotObject* slotObj = new SlotObject(centerX, -centerY, diameter, length, getCurrentColor());
    slotObj->setRotation(-rot);
    slotObj->setObjectRubberMode(rubberMode);
    if (rubberMode) gview->addToRubberRoom(slotObj);
    scene->addItem(slotObj);
    //TODO: slot fill
    scene->update();
    */
}

void
MainWindow::nativeAddEllipse(EmbReal centerX, EmbReal centerY, EmbReal width, EmbReal height, EmbReal rot, bool fill, int rubberMode)
{
    View* gview = _mainWin->activeView();
    QGraphicsScene* gscene = gview->scene();
    QUndoStack* stack = gview->getUndoStack();
    if (gview && gscene && stack)
    {
        EllipseObject* obj = new EllipseObject(centerX, -centerY, width, height, getCurrentColor());
        obj->setRotation(-rot);
        obj->setObjectRubberMode(rubberMode);
        //TODO: ellipse fill
        if (rubberMode)
        {
            gview->addToRubberRoom(obj);
            gscene->addItem(obj);
            gscene->update();
        }
        else
        {
            UndoableAddCommand* cmd = new UndoableAddCommand(obj->data(OBJ_NAME).toString(), obj, gview, 0);
            stack->push(cmd);
        }
    }
}

void
MainWindow::nativeAddPoint(EmbReal x, EmbReal y)
{
    View* gview = _mainWin->activeView();
    QUndoStack* stack = gview->getUndoStack();
    if (gview && stack)
    {
        PointObject* obj = new PointObject(x, -y, getCurrentColor());
        UndoableAddCommand* cmd = new UndoableAddCommand(obj->data(OBJ_NAME).toString(), obj, gview, 0);
        stack->push(cmd);
    }
}

void
MainWindow::nativeAddRegularPolygon(EmbReal centerX, EmbReal centerY, quint16 sides, quint8 mode, EmbReal rad, EmbReal rot, bool fill)
{
}

//NOTE: This native is different than the rest in that the Y+ is down (scripters need not worry about this)
void
MainWindow::nativeAddPolygon(EmbReal startX, EmbReal startY, const QPainterPath& p, int rubberMode)
{
    View* gview = _mainWin->activeView();
    QGraphicsScene* gscene = gview->scene();
    QUndoStack* stack = gview->getUndoStack();
    if (gview && gscene && stack)
    {
        PolygonObject* obj = new PolygonObject(startX, startY, p, getCurrentColor());
        obj->setObjectRubberMode(rubberMode);
        if (rubberMode)
        {
            gview->addToRubberRoom(obj);
            gscene->addItem(obj);
            gscene->update();
        }
        else
        {
            UndoableAddCommand* cmd = new UndoableAddCommand(obj->data(OBJ_NAME).toString(), obj, gview, 0);
            stack->push(cmd);
        }
    }
}

//NOTE: This native is different than the rest in that the Y+ is down (scripters need not worry about this)
void
MainWindow::nativeAddPolyline(EmbReal startX, EmbReal startY, const QPainterPath& p, int rubberMode)
{
    /*
    QVariantList varList = a[0].toVariant().toList();
    int varSize = varList.size();
    if (varSize < 2) {
        return "TYPE ERROR: addPolyline(): array must contain at least two elements";
    }
    if (varSize % 2) {
        return "TYPE ERROR: addPolyline(): array cannot contain an odd number of elements";
    }

    bool lineTo = false;
    bool xCoord = true;
    EmbReal x = 0;
    EmbReal y = 0;
    EmbReal startX = 0;
    EmbReal startY = 0;
    QPainterPath path;
    foreach (QVariant var, varList) {
        if (var.canConvert(QVariant::Double)) {
            if (xCoord) {
                xCoord = false;
                x = var.toReal();
            }
            else {
                xCoord = true;
                y = -var.toReal();

                if (lineTo) {
                    path.lineTo(x,y);
                }
                else {
                    path.moveTo(x,y);
                    lineTo = true;
                    startX = x;
                    startY = y;
                }
            }
        }
        else {
            return "TYPE ERROR: addPolyline(): array contains one or more invalid elements";
        }
    }

    path.translate(-startX, -startY);

    _mainWin->nativeAddPolyline(startX, startY, path, OBJ_RUBBER_OFF);
    */
    View* gview = _mainWin->activeView();
    QGraphicsScene* gscene = gview->scene();
    QUndoStack* stack = gview->getUndoStack();
    if (gview && gscene && stack) {
        PolylineObject* obj = new PolylineObject(startX, startY, p, getCurrentColor());
        obj->setObjectRubberMode(rubberMode);
        if (rubberMode) {
            gview->addToRubberRoom(obj);
            gscene->addItem(obj);
            gscene->update();
        }
        else {
            UndoableAddCommand* cmd = new UndoableAddCommand(obj->data(OBJ_NAME).toString(), obj, gview, 0);
            stack->push(cmd);
        }
    }
}

/**
 * .
 * \note This native is different than the rest in that
 * the Y+ is down (scripters need not worry about this).
 */
void
MainWindow::nativeAddPath(EmbReal startX, EmbReal startY, const QPainterPath& p, int rubberMode)
{
    /*
    AddPath(NodeList a)
    // TODO: Node error checking
    debug_message("TODO: finish addPath command");
    */
}

/**
 * .
 */
void
MainWindow::nativeAddHorizontalDimension(EmbReal x1, EmbReal y1, EmbReal x2, EmbReal y2, EmbReal legHeight)
{
    /*
    AddHorizontalDimension(NodeList a)
    //TODO: Node error checking
    debug_message("TODO: finish addHorizontalDimension command");
    */
}

/**
 * .
 */
void
MainWindow::nativeAddVerticalDimension(EmbReal x1, EmbReal y1, EmbReal x2, EmbReal y2, EmbReal legHeight)
{
    /*
    AddVerticalDimension(NodeList a)
    //TODO: Node error checking
    debug_message("TODO: finish addVerticalDimension command");
    */
}

/**
 * .
 */
void
MainWindow::nativeAddImage(const QString& img, EmbReal x, EmbReal y, EmbReal w, EmbReal h, EmbReal rot)
{
    /*
    AddImage(NodeList a)
    //TODO: Node error checking
    debug_message("TODO: finish addImage command");
    */
}

/**
 * .
 */
void
MainWindow::nativeAddDimLeader(EmbReal x1, EmbReal y1, EmbReal x2, EmbReal y2, EmbReal rot, int rubberMode)
{
    /*
    AddDimLeader(NodeList a)
    _mainWin->nativeAddDimLeader(a[0].r, a[1].r, a[2].r, a[3].r, a[4].r, OBJ_RUBBER_OFF);
    */
    View* gview = _mainWin->activeView();
    QGraphicsScene* gscene = gview->scene();
    QUndoStack* stack = gview->getUndoStack();
    if (gview && gscene && stack) {
        DimLeaderObject* obj = new DimLeaderObject(x1, -y1, x2, -y2, getCurrentColor());
        obj->setRotation(-rot);
        obj->setObjectRubberMode(rubberMode);
        if (rubberMode) {
            gview->addToRubberRoom(obj);
            gscene->addItem(obj);
            gscene->update();
        }
        else {
            UndoableAddCommand* cmd = new UndoableAddCommand(obj->data(OBJ_NAME).toString(), obj, gview, 0);
            stack->push(cmd);
        }
    }
}

/**
 * .
 */
void
MainWindow::nativeSetCursorShape(const QString& str)
{
    View* gview = _mainWin->activeView();
    if (gview) {
        QString shape = str.toLower();
        if (shape == "arrow")
            gview->setCursor(QCursor(Qt::ArrowCursor));
        else if (shape == "uparrow")
            gview->setCursor(QCursor(Qt::UpArrowCursor));
        else if (shape == "cross")
            gview->setCursor(QCursor(Qt::CrossCursor));
        else if (shape == "wait")
            gview->setCursor(QCursor(Qt::WaitCursor));
        else if (shape == "ibeam")
            gview->setCursor(QCursor(Qt::IBeamCursor));
        else if (shape == "resizevert")
            gview->setCursor(QCursor(Qt::SizeVerCursor));
        else if (shape == "resizehoriz")
            gview->setCursor(QCursor(Qt::SizeHorCursor));
        else if (shape == "resizediagleft")
            gview->setCursor(QCursor(Qt::SizeBDiagCursor));
        else if (shape == "resizediagright")
            gview->setCursor(QCursor(Qt::SizeFDiagCursor));
        else if (shape == "move")
            gview->setCursor(QCursor(Qt::SizeAllCursor));
        else if (shape == "blank")
            gview->setCursor(QCursor(Qt::BlankCursor));
        else if (shape == "splitvert")
            gview->setCursor(QCursor(Qt::SplitVCursor));
        else if (shape == "splithoriz")
            gview->setCursor(QCursor(Qt::SplitHCursor));
        else if (shape == "handpointing")
            gview->setCursor(QCursor(Qt::PointingHandCursor));
        else if (shape == "forbidden")
            gview->setCursor(QCursor(Qt::ForbiddenCursor));
        else if (shape == "handopen")
            gview->setCursor(QCursor(Qt::OpenHandCursor));
        else if (shape == "handclosed")
            gview->setCursor(QCursor(Qt::ClosedHandCursor));
        else if (shape == "whatsthis")
            gview->setCursor(QCursor(Qt::WhatsThisCursor));
        else if (shape == "busy")
            gview->setCursor(QCursor(Qt::BusyCursor));
        else if (shape == "dragmove")
            gview->setCursor(QCursor(Qt::DragMoveCursor));
        else if (shape == "dragcopy")
            gview->setCursor(QCursor(Qt::DragCopyCursor));
        else if (shape == "draglink")
            gview->setCursor(QCursor(Qt::DragLinkCursor));
    }
}

/**
 * .
 */
EmbReal
MainWindow::nativeCalculateAngle(EmbReal x1, EmbReal y1, EmbReal x2, EmbReal y2)
{
    return QLineF(x1, -y1, x2, -y2).angle();
}

/**
 * CalculateDistance(NodeList a)
 *     EmbReal result = _mainWin->nativeCalculateDistance(a[0].r, a[1].r, a[2].r, a[3].r);
 *     return String(result);
 */
EmbReal
MainWindow::nativeCalculateDistance(EmbReal x1, EmbReal y1, EmbReal x2, EmbReal y2)
{
    return QLineF(x1, y1, x2, y2).length();
}

/**
 * .
 */
EmbReal
MainWindow::nativePerpendicularDistance(EmbReal px, EmbReal py, EmbReal x1, EmbReal y1, EmbReal x2, EmbReal y2)
{
    QLineF line(x1, y1, x2, y2);
    QLineF norm = line.normalVector();
    EmbReal dx = px-x1;
    EmbReal dy = py-y1;
    norm.translate(dx, dy);
    QPointF iPoint;
    norm.intersects(line, &iPoint);
    return QLineF(px, py, iPoint.x(), iPoint.y()).length();
}

int MainWindow::nativeNumSelected()
{
    View* gview = _mainWin->activeView();
    if (gview) {
        return gview->numSelected();
    }
    return 0;
}

void
MainWindow::nativeAddToSelection(const QPainterPath path, Qt::ItemSelectionMode mode)
{
}

/**
 * @brief MainWindow::nativeDeleteSelected
 */
void
MainWindow::nativeDeleteSelected()
{
    View* gview = _mainWin->activeView();
    if (gview) {
        gview->deleteSelected();
    }
}

/**
 * @brief MainWindow::nativeCutSelected
 * \a x
 * \a y
 */
void
MainWindow::nativeCutSelected(EmbReal x, EmbReal y)
{
    /*
    _mainWin->nativeCutSelected(a[0].r, a[1].r);
    */
}

/**
 * @brief MainWindow::nativeCopySelected
 * \a x
 * \a y
 */
void
MainWindow::nativeCopySelected(EmbReal x, EmbReal y)
{
    /*
    _mainWin->nativeCopySelected(a[0].r, a[1].r);
    */
}

/**
 * @brief MainWindow::nativePasteSelected
 * \a x
 * \a y
 */
void
MainWindow::nativePasteSelected(EmbReal x, EmbReal y)
{
    /*
    _mainWin->nativePasteSelected(a[0].r, a[1].r);
    */
}

/**
 * @brief MainWindow::nativeMoveSelected
 * \a dx
 * \a dy
 */
void
MainWindow::nativeMoveSelected(EmbReal dx, EmbReal dy)
{
    /*
    _mainWin->nativeMoveSelected(a[0].r, a[1].r);
    */
    View* gview = _mainWin->activeView();
    if (gview) { gview->moveSelected(dx, -dy); }
}

/**
 * @brief MainWindow::nativeScaleSelected
 * \a x
 * \a y
 * \a factor
 */
void
MainWindow::nativeScaleSelected(EmbReal x, EmbReal y, EmbReal factor)
{
    /*
    if (a[2].r <= 0.0) {
        return "ERROR scaleSelected(): scale factor must be greater than zero";
    }

    _mainWin->nativeScaleSelected(a[0].r, a[1].r, a[2].r);
    */
    if (factor <= 0.0) {
        QMessageBox::critical(this,
            tr("ScaleFactor Error"),
            tr("Hi there. If you are not a developer, report this as a bug. "
            "If you are a developer, your code needs examined, and possibly your head too."));
    }

    View* gview = _mainWin->activeView();
    if (gview) {
        gview->scaleSelected(x, -y, factor);
    }
}

/**
 * @brief MainWindow::nativeRotateSelected
 * \a x
 * \a y
 * \a rot
 */
void
MainWindow::nativeRotateSelected(EmbReal x, EmbReal y, EmbReal rot)
{
    /*
    _mainWin->nativeRotateSelected(a[0].r, a[1].r, a[2].r);
    */
    View* gview = _mainWin->activeView();
    if (gview) {
        gview->rotateSelected(x, -y, -rot);
    }
}

/**
 * @brief MainWindow::nativeMirrorSelected
 * \a x1
 * \a y1
 * \a x2
 * \a y2
 */
void
MainWindow::nativeMirrorSelected(EmbReal x1, EmbReal y1, EmbReal x2, EmbReal y2)
{
    /*
    _mainWin->nativeMirrorSelected(a[0].r, a[1].r, a[2].r, a[3].r);
    */
    View* gview = _mainWin->activeView();
    if (gview) {
        gview->mirrorSelected(x1, -y1, x2, -y2);
    }
}

/**
 * @brief MainWindow::nativeQSnapX
 * @return
 */
EmbReal
MainWindow::nativeQSnapX()
{
    QGraphicsScene* scene = activeScene();
    if (scene) {
        return scene->property("SCENE_QSNAP_POINT").toPointF().x();
    }
    return 0.0;
}

/**
 * @brief MainWindow::nativeQSnapY
 * @return
 */
EmbReal
MainWindow::nativeQSnapY()
{
    QGraphicsScene* scene = activeScene();
    if (scene) {
        return -scene->property("SCENE_QSNAP_POINT").toPointF().y();
    }
    return 0.0;
}

/**
 * @brief MainWindow::nativeMouseX
 * @return
 */
EmbReal
MainWindow::nativeMouseX()
{
    QGraphicsScene* scene = activeScene();
    if (scene) {
        qDebug("mouseX: %.50f", scene->property("SCENE_MOUSE_POINT").toPointF().x());
        return scene->property("SCENE_MOUSE_POINT").toPointF().x();
    }
    return 0.0;
}

/**
 * @brief MainWindow::nativeMouseY
 * @return
 */
EmbReal
MainWindow::nativeMouseY()
{
    QGraphicsScene* scene = activeScene();
    if (scene) {
        qDebug("mouseY: %.50f", -scene->property("SCENE_MOUSE_POINT").toPointF().y());
        return -scene->property("SCENE_MOUSE_POINT").toPointF().y();
    }
    return 0.0;
}

/**
 * @brief construct_command
 * @param command
 * @param fmt
 * @return
 */
String
construct_command(String command, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    for (int i=0; i<(int)strlen(fmt); i++) {
        if (fmt[i] == 's') {
            String s(va_arg(args, char*));
            command += s;
        }
        if (fmt[i] == 'i') {
            command += std::to_string(va_arg(args, int));
        }
        if (fmt[i] == 'b') {
            command += std::to_string(va_arg(args, bool));
        }
        if (fmt[i] == 'f') {
            command += std::to_string(va_arg(args, EmbReal));
        }
    }
    va_end(args);
    return command;
}

/**
 * @brief tokenize
 * @param str
 * @param delim
 * @return
 */
StringList
tokenize(String str, const char delim)
{
    StringList list;
    std::stringstream str_stream(str);
    String s;
    while (std::getline(str_stream, s, delim)) {
        list.push_back(s);
    }
    return list;
}

/**
 * .
 */
String
read_string_setting(toml_table_t *table, const char *key)
{
    toml_datum_t str = toml_string_in(table, key);
    if (!str.ok) {
        return "";
    }
    String s(str.u.s);
    free(str.u.s);
    return s;
}

/**
 * .
 */
std::vector<String>
read_string_list_setting(toml_table_t *table, const char *key)
{
    std::vector<String> str_list;
    toml_array_t* array = toml_array_in(table, key);
    for (int i=0; ; i++) {
        toml_datum_t str = toml_string_at(array, i);
        if (!str.ok) {
            break;
        }
        String s(str.u.s);
        str_list.push_back(s);
        free(str.u.s);
    }
    return str_list;
}

/**
 * \brief Read the settings from file which aren't editable by the user.
 * These files need to be placed in the install folder.
 */
int
read_configuration(void)
{
    String fname = qApp->applicationDirPath().toStdString() + "/config.toml";
    char error_buffer[200];
    FILE *f = fopen(fname.c_str(), "r");
    if (!f) {
        puts("ERROR: Failed to open settings file:");
        printf("%s", fname.c_str());
        return 0;
    }
    toml_table_t *settings_toml = toml_parse_file(f, error_buffer, sizeof(error_buffer));
    fclose(f);

    if (!settings_toml) {
        puts("ERROR: failed to parse config.toml, continuing with defaults.");
        return 0;
    }

    std::vector<String> action_labels =
        read_string_list_setting(settings_toml, "actions_");

    for (int i=0; i<(int)action_labels.size(); i++) {
        Action action;
        String label = "ACTION_" + action_labels[i];
        toml_table_t *table = toml_table_in(settings_toml, label.c_str());

        action.icon = read_string_setting(table, "icon");

        action.command = read_string_setting(table, "command");
        action.tooltip = read_string_setting(table, "tooltip");
        action.statustip = read_string_setting(table, "statustip");
        action.shortcut = read_string_setting(table, "shortcut");

        action_table.push_back(action);
    }

    StringList string_lists_list = read_string_list_setting(settings_toml, "string_lists");
    for (int i=0; i<(int)string_lists_list.size(); i++) {
        String key = string_lists_list[i];
        config[key] = node(read_string_list_setting(settings_toml, key.c_str()));
    }

    toml_free(settings_toml);

    return 1;
}

/**
 * @brief .
 */
bool
validRGB(int r, int g, int b)
{
    bool result = (r>=0);
    result &= (r<256);
    result &= (g>=0);
    result &= (g<256);
    result &= (b>=0);
    result &= (b<256);
    return result;
}

/**
 * @brief MainWindow::MainWindow
 */
MainWindow::MainWindow() : QMainWindow(0)
{
    QString appDir = qApp->applicationDirPath();
    read_configuration();
    read_settings();

    //Verify that files/directories needed are actually present.
    QFileInfo check = QFileInfo(appDir + "/help");
    if (!check.exists())
        QMessageBox::critical(this, tr("Path Error"), tr("Cannot locate: ") + check.absoluteFilePath());
    check = QFileInfo(appDir + "/icons");
    if (!check.exists())
        QMessageBox::critical(this, tr("Path Error"), tr("Cannot locate: ") + check.absoluteFilePath());
    check = QFileInfo(appDir + "/images");
    if (!check.exists())
        QMessageBox::critical(this, tr("Path Error"), tr("Cannot locate: ") + check.absoluteFilePath());
    check = QFileInfo(appDir + "/samples");
    if (!check.exists())
        QMessageBox::critical(this, tr("Path Error"), tr("Cannot locate: ") + check.absoluteFilePath());
    check = QFileInfo(appDir + "/translations");
    if (!check.exists())
        QMessageBox::critical(this, tr("Path Error"), tr("Cannot locate: ") + check.absoluteFilePath());

    QString lang = QString::fromStdString(settings.general_language);
    debug_message("language: " + lang.toStdString());
    if (lang == "system")
        lang = QLocale::system().languageToString(QLocale::system().language()).toLower();

    //Load translations for the Embroidermodder 2 GUI
    QTranslator translatorEmb;
    translatorEmb.load(appDir + "/translations/" + lang + "/embroidermodder2_" + lang);
    qApp->installTranslator(&translatorEmb);

    //Load translations for the commands
    QTranslator translatorCmd;
    translatorCmd.load(appDir + "/translations/" + lang + "/commands_" + lang);
    qApp->installTranslator(&translatorCmd);

    //Load translations provided by Qt - this covers dialog buttons and other common things.
    QTranslator translatorQt;
    translatorQt.load("qt_" + QLocale::system().name(), QLibraryInfo::location(QLibraryInfo::TranslationsPath)); //TODO: ensure this always loads, ship a copy of this with the app
    qApp->installTranslator(&translatorQt);

    //Init
    _mainWin = this;

    QPoint pos = QPoint(settings.position_x, settings.position_y);
    QSize size = QSize(settings.size_x, settings.size_y);

    move(pos);
    resize(size);

    //Menus
    menuHash["file"] = new QMenu(tr("&File"), this);
    menuHash["edit"] = new QMenu(tr("&Edit"), this);
    menuHash["view"] = new QMenu(tr("&View"), this);
    menuHash["settings"] = new QMenu(tr("&Settings"), this);
    menuHash["window"] = new QMenu(tr("&Window"), this);
    menuHash["help"] = new QMenu(tr("&Help"), this);
    menuHash["draw"] = new QMenu(tr("&Draw"), this);

    //SubMenus
    subMenuHash["recent"] = new QMenu(tr("Open &Recent"), this);
    subMenuHash["zoom"] = new QMenu(tr("&Zoom"), this);
    subMenuHash["pan"] = new QMenu(tr("&Pan"), this);

    //Toolbars
    toolbarHash["file"] = addToolBar(tr("File"));
    toolbarHash["edit"] = addToolBar(tr("Edit"));
    toolbarHash["view"] = addToolBar(tr("View"));
    toolbarHash["zoom"] = addToolBar(tr("Zoom"));
    toolbarHash["pan"] = addToolBar(tr("Pan"));
    toolbarHash["icon"] = addToolBar(tr("Icon"));
    toolbarHash["help"] = addToolBar(tr("Help"));
    toolbarHash["layer"] = addToolBar(tr("Layer"));
    toolbarHash["properties"] = addToolBar(tr("Properties"));
    toolbarHash["text"] = addToolBar(tr("Text"));
    toolbarHash["prompt"] = addToolBar(tr("Command Prompt"));
    toolbarHash["draw"] = addToolBar(tr("Draw"));

    //Selectors
    layerSelector = new QComboBox(this);
    colorSelector = new QComboBox(this);
    linetypeSelector = new QComboBox(this);
    lineweightSelector = new QComboBox(this);
    textFontSelector = new QFontComboBox(this);
    textSizeSelector = new QComboBox(this);

    numOfDocs = 0;
    docIndex = 0;

    shiftKeyPressedState = false;

    setWindowIcon(create_icon("app"));
    setMinimumSize(800, 480); //Require Minimum WVGA

    loadFormats();

    //create the mdiArea
    QFrame* vbox = new QFrame(this);
    QVBoxLayout* layout = new QVBoxLayout(vbox);
    //layout->setMargin(0);
    vbox->setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
    mdiArea = new MdiArea(vbox);
    mdiArea->useBackgroundLogo(settings.general_mdi_bg_use_logo);
    mdiArea->useBackgroundTexture(settings.general_mdi_bg_use_texture);
    mdiArea->useBackgroundColor(settings.general_mdi_bg_use_color);
    mdiArea->setBackgroundLogo(QString::fromStdString(settings.general_mdi_bg_logo));
    mdiArea->setBackgroundTexture(QString::fromStdString(settings.general_mdi_bg_texture));
    mdiArea->setBackgroundColor(QColor(settings.general_mdi_bg_color));
    mdiArea->setViewMode(QMdiArea::TabbedView);
    mdiArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    mdiArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    mdiArea->setActivationOrder(QMdiArea::ActivationHistoryOrder);
    layout->addWidget(mdiArea);
    setCentralWidget(vbox);

    //create the Command Prompt
    prompt = new CmdPrompt();
    prompt->setFocus(Qt::OtherFocusReason);
    this->setFocusProxy(prompt);
    mdiArea->setFocusProxy(prompt);

    prompt->setPromptTextColor(QColor(settings.prompt_text_color));
    prompt->setPromptBackgroundColor(QColor(settings.prompt_bg_color));

    connect(prompt, SIGNAL(startCommand(QString)), this, SLOT(logPromptInput(QString)));

    connect(prompt, SIGNAL(startCommand(QString)), this, SLOT(runCommandMain(QString)));
    connect(prompt, SIGNAL(runCommand(QString,QString)), this, SLOT(runCommandPrompt(QString,QString)));

    connect(prompt, SIGNAL(deletePressed()), this, SLOT(deletePressed()));
    //TODO: connect(prompt, SIGNAL(tabPressed()), this, SLOT(someUnknownSlot()));
    connect(prompt, SIGNAL(escapePressed()), this, SLOT(escapePressed()));
    connect(prompt, SIGNAL(upPressed()), this, SLOT(promptInputPrevious()));
    connect(prompt, SIGNAL(downPressed()), this, SLOT(promptInputNext()));
    connect(prompt, SIGNAL(F1Pressed()), this, SLOT(help()));
    //TODO: connect(prompt, SIGNAL(F2Pressed()), this, SLOT(floatHistory()));
    //TODO: connect(prompt, SIGNAL(F3Pressed()), this, SLOT(toggleQSNAP()));
    connect(prompt, SIGNAL(F4Pressed()), this, SLOT(toggleLwt())); //TODO: typically this is toggleTablet(), make F-Keys customizable thru settings
    //TODO: connect(prompt, SIGNAL(F5Pressed()), this, SLOT(toggleISO()));
    //TODO: connect(prompt, SIGNAL(F6Pressed()), this, SLOT(toggleCoords()));
    connect(prompt, SIGNAL(F7Pressed()), this, SLOT(toggleGrid()));
    //TODO: connect(prompt, SIGNAL(F8Pressed()), this, SLOT(toggleORTHO()));
    //TODO: connect(prompt, SIGNAL(F9Pressed()), this, SLOT(toggleSNAP()));
    //TODO: connect(prompt, SIGNAL(F10Pressed()), this, SLOT(togglePOLAR()));
    //TODO: connect(prompt, SIGNAL(F11Pressed()), this, SLOT(toggleQTRACK()));
    connect(prompt, SIGNAL(F12Pressed()), this, SLOT(toggleRuler()));
    connect(prompt, SIGNAL(cutPressed()), this, SLOT(cut_action()));
    connect(prompt, SIGNAL(copyPressed()), this, SLOT(copy_action()));
    connect(prompt, SIGNAL(pastePressed()), this, SLOT(paste_action()));
    connect(prompt, SIGNAL(selectAllPressed()), this, SLOT(selectAll()));
    connect(prompt, SIGNAL(undoPressed()), this, SLOT(undo()));
    connect(prompt, SIGNAL(redoPressed()), this, SLOT(redo()));

    connect(prompt, SIGNAL(shiftPressed()), this, SLOT(setShiftPressed()));
    connect(prompt, SIGNAL(shiftReleased()), this, SLOT(setShiftReleased()));

    connect(prompt, SIGNAL(showSettings()),     this, SLOT(settingsPrompt()));

    connect(prompt, SIGNAL(historyAppended(QString)), this, SLOT(promptHistoryAppended(QString)));

    //create the Object Property Editor
    dockPropEdit = new PropertyEditor(appDir + "/icons/" + QString::fromStdString(settings.general_icon_theme), settings.selection_mode_pickadd, prompt, this);
    addDockWidget(Qt::LeftDockWidgetArea, dockPropEdit);
    connect(dockPropEdit, SIGNAL(pickAddModeToggled()), this, SLOT(pickAddModeToggled()));

    //create the Command History Undo Editor
    dockUndoEdit = new UndoEditor(appDir + "/icons/" + QString::fromStdString(settings.general_icon_theme), prompt, this);
    addDockWidget(Qt::LeftDockWidgetArea, dockUndoEdit);

    //setDockOptions(QMainWindow::AnimatedDocks | QMainWindow::AllowTabbedDocks | QMainWindow::VerticalTabs); //TODO: Load these from settings
    //tabifyDockWidget(dockPropEdit, dockUndoEdit); //TODO: load this from settings

    statusbar = new StatusBar(this);
    this->setStatusBar(statusbar);

    createAllActions();
    createAllMenus();
    createAllToolbars();

    iconResize(settings.general_icon_size);
    updateMenuToolbarStatusbar();

    //Show date in statusbar after it has been updated
    QDate date = QDate::currentDate();
    QString datestr = date.toString("MMMM d, yyyy");
    statusbar->showMessage(datestr);

    showNormal();

    //Load tips from external file
    QFile tipFile(appDir + "/tips.txt");
    if (tipFile.open(QFile::ReadOnly)) {
        QTextStream stream(&tipFile);
        QString tipLine;
        do {
            tipLine = stream.readLine();
            if (!tipLine.isEmpty())
                listTipOfTheDay << tipLine;
        }
        while(!tipLine.isNull());
    }
    if (settings.general_tip_of_the_day) {
        actuator("tip-of-the-day");
    }
}

/**
 * @brief MainWindow::~MainWindow
 */
MainWindow::~MainWindow()
{
    debug_message("MainWindow::Destructor()");

    //Prevent memory leaks by deleting any unpasted objects
    qDeleteAll(cutCopyObjectList.begin(), cutCopyObjectList.end());
    cutCopyObjectList.clear();
}

/**
 * @brief MainWindow::createAllActions
 *
 * \todo Set What's This Context Help to statusTip for now so there is some infos there.
 * Make custom whats this context help popup with more descriptive help than just
 * the status bar/tip one liner(short but not real long) with a hyperlink in the custom popup
 * at the bottom to open full help file description. Ex: like wxPython AGW's SuperToolTip.
ACTION->setWhatsThis(statusTip);
 * \todo Finish All Commands ... <.<
 */
void
MainWindow::createAllActions()
{
    debug_message("Creating All Actions...");

    for (int i=0; i<action_table.size(); i++) {
        Action action = action_table[i];

        QIcon icon = create_icon(QString::fromStdString(action.icon));
        QAction *ACTION = new QAction(icon, QString::fromStdString(action.tooltip), this);
        ACTION->setStatusTip(QString::fromStdString(action.statustip));
        ACTION->setObjectName(QString::fromStdString(action.icon));
        if (action.shortcut != "") {
            ACTION->setShortcut(
                QKeySequence(QString::fromStdString(action.shortcut))
            );
        }

        if (   (action.icon == "textbold")
            || (action.icon == "textitalic")
            || (action.icon == "textunderline")
            || (action.icon == "textstrikeout")
            || (action.icon == "textoverline")) {
            ACTION->setCheckable(true);
        }

        auto f = [=](){ actuator(action.command); };
        connect(ACTION, &QAction::triggered, this, f);
        actionHash[action.icon] = ACTION;
    }

    actionHash["windowclose"]->setEnabled(numOfDocs > 0);
    actionHash["designdetails"]->setEnabled(numOfDocs > 0);
}

/**
 * @brief MainWindow::run_script_file
 * @param fname The path of the script to run.
 */
String
run_script_file(String fname)
{
    String output = "", line;
    std::ifstream file(fname);
    while (std::getline(file, line)) {
        output += actuator(line);
    }
    return output;
}

/**
 * @brief A basic line-by-line script processor to allow for extensions to the program.
 *
 * Since the actuator uses command line style parsing, a script is just a text
 * file with each line a compatible command.
 *
 * It should be stressed that this has no control flow on purpose. We don't want
 * this to be hacked into a full scripting language that could cause havoc on
 * the user's system.
 *
 * However, it may be useful to set and get variables and define macros:
 * neither of these will allow for endless loops, stack overflow or other
 * problems that third-party scripts could introduce.
 *
 *     example.sh
 *     ------------------------------------------------------------------
 *     # Save characters by defining functions.
 *     # The syntax features
 *     # Semi-colon ';' seperates out lines like in bash.
 *     # The line ending is the end of the function, but the style
 *     # is a shell function, so we need to write the end brace.
 *
 *     donut() { circle $1 $2 $3 $5 ; circle $1 $2 $4 $5 }
 *
 *     donut 10 20 20 black
 *     donut 20 40 20 black
 *     ------------------------------------------------------------------
 */
String
run_script(std::vector<String> script)
{
    String output = "";
    for (int i=0; i<(int)script.size(); i++) {
        output += actuator(script[i]);
    }
    return output;
}

/**
 * @brief MainWindow::actuator
 * @param command
 *
 *
 * RUN COMMAND
 * -----------
 * QAction* act = qobject_cast<QAction*>(sender());
 * if (act) {
 *     prompt->endCommand();
 *     prompt->setCurrentText(act->objectName());
 *     prompt->processInput();
 * }
 *
 * INIT
 * ----
 * QString fileName = "commands/" + cmd + "/" + cmd + ".js";
 * if (!getSettingsSelectionModePickFirst()) { actuator("clear-selection"); }
 * TODO: Uncomment this line when post-selection is available
 * engine->evaluate(cmd + "_main()", fileName);
 *
 * PROMPT
 * ------
 * QString fileName = "commands/" + cmd + "/" + cmd + ".js";
 * NOTE: Replace any special characters that will cause a syntax error
 * QString safeStr = str;
 * safeStr.replace("\\", "\\\\");
 * safeStr.replace("\'", "\\\'");
 *
 * if (prompt->isRapidFireEnabled()) {
 *     engine->evaluate(cmd + "_prompt('" + safeStr + "')", fileName);
 * }
 * else {
 *     engine->evaluate(cmd + "_prompt('" + safeStr.toUpper() + "')", fileName);
 * }
 */
String
actuator(String line)
{
    NodeList a;
    std::vector<String> list = tokenize(line, ' ');
    String command = list[0];
    list.erase(list.begin());

    auto iter = command_map.find(command);
    if (iter != command_map.end()) {
        int from = std::min(line.size(), command.size() + 1);
        String args = line.substr(from);
        return iter->second(args);
    }

    if (command == "about") {
        return about_action("");
    }

    if (command == "add") {
        if (list.size() < 1) {
            return "</br>The add command requires an argument.";
        }
        command = list[0];
        if (command == "arc") {
//            _mainWin->AddArc();
            return "";
        }
        if (command == "circle") {
//            _mainWin->AddCircle();
            return "";
        }
        if (command == "ellipse") {
//           _mainWin->AddEllipse();
            return "";
        }
        if (command == "point") {
//            _mainWin->AddPoint();
            return "";
        }
        if (command == "regular-polygon") {
//            _mainWin->AddRegularPolygon();
            return "";
        }
        if (command == "polygon") {
//            _mainWin->AddPolygon();
            return "";
        }
        if (command == "polyline") {
//            _mainWin->nativeAddPolyline();
            return "";
        }
        if (command == "path") {
//            _mainWin->nativeAddPath();
            return "";
        }
        if (command == "horizontal-dimension") {
//            _mainWin->nativeAddHorizontalDimension();
            return "";
        }
        if (command == "vertical-dimension") {
//            _mainWin->nativeAddVerticalDimension();
            return "";
        }
        if (command == "image") {
//            _mainWin->nativeAddImage();
            return "";
        }
        if (command == "dim-leader") {
//            _mainWin->AddDimLeader();
            return "";
        }
        if (command == "text-multi") {
//            _mainWin->AddTextMulti();
            return "";
        }
        if (command == "text-single") {
//            _mainWin->AddTextSingle();
            return "";
        }
        if (command == "infinite-line") {
 //           _mainWin->AddInfiniteLine();
            return "";
        }
        if (command == "ray") {
//            _mainWin->AddRay();
            return "";
        }
        if (command == "line") {
            String error = convert_args_to_type("add rectangle", list, "rrrrrbi", a);
            if (error != "") {
                return error;
            }
//            _mainWin->AddLine();
            return "";
        }
        if (command == "triangle") {
            String error = convert_args_to_type("add rectangle", list, "rrrrrbi", a);
            if (error != "") {
                return error;
            }
//            _mainWin->AddTriangle();
            return "";
        }
        if (command == "rectangle") {
            String error = convert_args_to_type("add rectangle", list, "rrrrrbi", a);
            if (error != "") {
                return error;
            }
            _mainWin->nativeAddRectangle(a);
            return "";
        }
        if (command == "rounded-rectangle") {
            String error = convert_args_to_type("add rectangle", list, "rrrrrbi", a);
            if (error != "") {
                return error;
            }
//            _mainWin->AddRoundedRectangle();
            return "";
        }
        return "</br>The add subcommand is not recognised.";
    }

    if (command == "alert") {
        prompt->alert(QString::fromStdString(line.substr(5)));
        return "";
    }

    if (command == "end") {
        View* gview = _mainWin->activeView();
        if (gview) {
            gview->clearRubberRoom();
            gview->previewOff();
            gview->disableMoveRapidFire();
        }
        prompt->endCommand();
        return "";
    }

    if ((command == "exit") || (command == "quit")) {
        _mainWin->quit();
        return "";
    }

    if (command == "help") {
        return help_action("");
    }

    if (command == "init") {            
        View* gview = _mainWin->activeView();
        if (gview) {
            gview->clearRubberRoom();
        }
        return "";
    }

    if (command == "new") {
        actuator("clear-selection");
        _mainWin->newFile();
        return "";
    }

    if (command == "platform") {
        return "<br/>" + platformString();
    }

    if (command == "redo") {
        return redo_action("");
    }

    if (command == "selectall") {
        debug_message("selectAll()");
        View* gview = _mainWin->activeView();
        if (gview) {
            gview->selectAll();
        }
        return "";
    }

    if (command == "append-history") {
        prompt->appendHistory(QString::fromStdString(line));
        return "";
    }

    if (command == "tipoftheday") {
        return tip_of_the_day_action("");
    }

    if (command == "undo") {
        return undo_action ("");
    }

    if (command == "window") {
        if (list.size() < 1) {
            return "</br>window requires an argument.";
        }
        command = list[0];
        if (command == "cascade") {
            mdiArea->cascade();
            return "";
        }
        if (command == "close") {
            _mainWin->onCloseWindow();
            return "";
        }
        if (command == "closeall") {
            mdiArea->closeAllSubWindows();
            return "";
        }
        if (command == "tile") {
            mdiArea->tile();
            return "";
        }
        if (command == "next") {
            mdiArea->activateNextSubWindow();
            return "";
        }
        if (command == "previous") {
            mdiArea->activatePreviousSubWindow();
            return "";
        }
        return "</br>window argument not recognised.";
    }

    if (command == "open") {
        _mainWin->openFile();
        return "";
    }

    if (command == "icon") {
        if (list.size() < 1) {
            return "icon requires an argument.";
        }
        command = list[0];
        if (command == "16") {
            debug_message("icon16()");
            _mainWin->iconResize(16);
            return "";
        }
        if (command == "24") {
            debug_message("icon24()");
            _mainWin->iconResize(24);
            return "";
        }
        if (command == "32") {
            debug_message("icon32()");
            _mainWin->iconResize(32);
            return "";
        }
        if (command == "48") {
            debug_message("icon48()");
            _mainWin->iconResize(48);
            return "";
        }
        if (command == "64") {
            debug_message("icon64()");
            _mainWin->iconResize(64);
            return "";
        }
        if (command == "128") {
            debug_message("icon128()");
            _mainWin->iconResize(128);
            return "";
        }
        return "";
    }

    if (command == "settingsdialog") {
        _mainWin->settingsDialog();
        return "";
    }

    if (command == "text") {
        if (list.size() < 1) {
            return "text requires an argument.";
        }
        command = list[0];
        if (command == "font") {
            return settings.text_font;
        }
        if (command == "size") {
            return std::to_string(settings.text_size);
        }
        if (command == "angle") {
            return std::to_string(settings.text_angle);
        }
        if (command == "bold") {
            return std::to_string(settings.text_style_bold);
        }
        if (command == "italic") {
            return std::to_string(settings.text_style_italic);
        }
        if (command == "underline") {
            return std::to_string(settings.text_style_underline);
        }
        if (command == "strikeout") {
            return std::to_string(settings.text_style_strikeout);
        }
        if (command == "overline") {
            return std::to_string(settings.text_style_overline);
        }
    }

    if (command == "set") {
        if (list.size() < 2) {
            return "The command 'set' requires 2 arguments.";
        }
        bool value = (
               list[1] == "true"
            || list[1] == "True"
            || list[1] == "TRUE"
            || list[1] == "on"
            || list[1] == "ON"
            || list[1] == "T"
            || list[1] == "t"
            || list[1] == "1"
        );
        if (list[0] == "text_font") {
            settings.text_font = list[1];
            return "";
        }
        if (list[0] == "text_size") {
            settings.text_size = std::stof(list[1]);
            return "";
        }
        if (command == "text_angle") {
            settings.text_angle = value;
            return "";
        }
        if (command == "text_style_bold") {
            settings.text_style_bold = value;
            return "";
        }
        if (command == "text_style_italic") {
            settings.text_style_italic = value;
            return "";
        }
        if (command == "text_style_underline") {
            settings.text_style_underline = value;
            return "";
        }
        if (command == "text_style_strikeout") {
            settings.text_style_strikeout = value;
            return "";
        }
        if (command == "text_style_overline") {
            settings.text_style_overline = value;
            return "";
        }
    }

    if (command == "enable") {
        if (list.size() < 1) {
            return "The command 'enable' requires an argument.";
        }
        if (command == "text-angle") {
            settings.text_angle = true;
            return "";
        }
        if (command == "text-bold") {
            settings.text_style_bold = true;
            return "";
        }
        if (command == "text-italic") {
            settings.text_style_italic = true;
            return "";
        }
        if (command == "text-underline") {
            settings.text_style_underline = true;
            return "";
        }
        if (command == "text-strikeout") {
            settings.text_style_strikeout = true;
            return "";
        }
        if (command == "text-overline") {
            settings.text_style_overline = true;
            return "";
        }
        if (command == "prompt-rapid-fire") {
            prompt->enableRapidFire();
            return "";
        }
        if (command == "move-rapid-fire") {            
            View* gview = _mainWin->activeView();
            if (gview) {
                gview->enableMoveRapidFire();
            }
            return "";
        }
        return "";
    }

    if (command == "disable") {
        if (list.size() < 1) {
            return "The command 'disable' requires an argument.";
        }
        if (command == "text-angle") {
            settings.text_angle = false;
            return "";
        }
        if (command == "text-bold") {
            settings.text_style_bold = false;
            return "";
        }
        if (command == "text-italic") {
            settings.text_style_italic = false;
            return "";
        }
        if (command == "text-underline") {
            settings.text_style_underline = false;
            return "";
        }
        if (command == "text-strikeout") {
            settings.text_style_strikeout = false;
            return "";
        }
        if (command == "text-overline") {
            settings.text_style_overline = false;
            return "";
        }
        if (command == "prompt-rapid-fire") {
            prompt->disableRapidFire();
            return "";
        }
        if (command == "move-rapid-fire") {            
            View* gview = _mainWin->activeView();
            if (gview) {
                gview->disableMoveRapidFire();
            }
            return "";
        }
        return "";
    }

    if (command == "version") {
        return "<br/>2.0.0-alpha";
    }

    if (command == "set-prompt-prefix") {
        prompt->setPrefix(QString::fromStdString(line));
    }

    /*
    if (command == "numselected") {
        return itos(NumSelected());
    }

    if (command == "selectall") {
        SelectAll();
        return "";
    }

    if (command == "add to selection") {
        AddToSelection();
        return "";
    }
    */

    if (command == "clear-selection") {
        View* gview = _mainWin->activeView();
        if (gview) {
            gview->clearSelection();
        }
        return "";
    }

    /*
    if (command == "delete selection") {
        nativeDeleteSelected();
        return "";
    }

    if (command == "qsnapx") {
        return nativeQSnapX();
    }

    if (command == "qsnapy") {
        return nativeQSnapY();
    }

    if (command == "mousex") {
        return nativeMouseX();
    }

    if (command == "mousey") {
        return nativeMouseY();
    }

    if (command == "debug") {
        scriptValDebug();
        return "";
    }

    if (command == "error") {
        scriptValError();
        return "";
    }

    if (command == "todo") {
        Todo();
        return "";
    }

    if (command == "alert") {
        Alert();
        return "";
    }

    if (command == "appendPromptHistory") {
        AppendPromptHistory();
        return "";
    }

    if (command == "initCommand") {
        InitCommand();
        return "";
    }

    if (command == "help") {
        Help();
        return "";
    }

    if (command == "messageBox") {
        MessageBox();
        return "";
    }

    if (command == "isInt") {
        IsInt();
        return "";
    }

    if (command == "printArea") {
        PrintArea();
        return "";
    }
    if (command == "setBackgroundColor") {
        SetBackgroundColor();
        return "";
    }

    if (command == "setCrossHairColor") {
        SetCrossHairColor();
        return "";
    }

    if (command == "setGridColor") {
        SetGridColor();
        return "";
    }

    if (command == "previewOn") {
        PreviewOn();
        return "";
    }

    if (command == "previewOff")
        PreviewOff();
        return "";
    }
    */

    if (command == "vulcanize") {
        View* gview = _mainWin->activeView();
        if (gview) {
            gview->vulcanizeRubberRoom();
        }
        return "";
    }

    /*
    if (command == "rubber") {
        if (command == "allow") {
            AllowRubber();
            return "";
        }

        if (command == "set-mode") {
            SetRubberMode();
            return "";
        }

        if (command == "set-point") {
            SetRubberPoint();
            return "";
        }
        if (command == "set-text") {
            SetRubberText();
            return "";
        }

        if (command == "add") {
            AddRubber();
            return "";
        }
        if (command == "clear") {
            ClearRubber();
            return "";
        }
        if (command == "spare") {
            SpareRubber();
            return "";
        }
        return "";
    }

    if (command == "setCursorShape") {
        SetCursorShape();
        return "";
    }

    if (command == "calculateAngle") {
        CalculateAngle();
        return "";
    }

    if (command == "calculateDistance") {
        CalculateDistance();
        return "";
    }

    if (command == "perpendicularDistance") {
        PerpendicularDistance();
        return "";
    }

    if (command == "addToSelection") {
        scriptValAddToSelection();
        return "";
    }

    if (command == "clearSelection") {
        scriptValClearSelection();
        return "";
    }

    if (command == "deleteSelected") {
        scriptValDeleteSelected();
        return "";
    }

    if (command == "cutSelected") {
        scriptValCutSelected);
        return "";
    }

    if (command == "copySelected") {
        scriptValCopySelected);
        return "";
    }

    if (command == "pasteSelected") {
        scriptValPasteSelected();
        return "";
    }

    if (command == "moveSelected") {
        scriptValMoveSelected();
        return "";
    }

    if (command == "scaleSelected") {
        scriptValScaleSelected();
        return "";
    }

    if (command == "rotateSelected") {
        scriptValRotateSelected();
        return "";
    }

    if (command == "mirrorSelected") {
        scriptValMirrorSelected();
        return "";
    }

    if (command == "include") {
        Include();
        return "";
    }
    */

    if (command == "blink-prompt") {
        prompt->startBlinking();
        return "";
    }

    /*
    if (command == "SetPromptPrefix") {
        setPromptPrefix(args[0]);
        return "";
    }
    */
    return "<br/><font color=\"red\">Unknown command \"" + command
        + "\". Press F1 for help.</font>";
}

/**
 * @brief Inspired by PyArg_ParseTupleAndKeywords allowing
 * a uniform argument parsing framework.
 * @param label The caller's name.
 * @param args The list of strings passed from the user.
 * @param args_template The string of characters describing the types of the output.
 * @param result The fixed length array of results.
 * @return An error message if an error occured or an empty string if it passes.
 */
String
convert_args_to_type(
    String label,
    std::vector<String> args,
    const char *args_template,
    NodeList a)
{
    int n_args = (int)args.size();
    int required_args = strlen(args_template);
    if (n_args < required_args) {
        String required = std::to_string(required_args);
        return "ERROR: " + label + "requires" + required + "arguments";
    }
    for (int i=0; i<n_args; i++) {
        switch (args_template[i]) {
        case 'i': {
            Node entry;
            entry.i = stoi(args[i]);
            a.push_back(entry);
            if (errno == EINVAL) {
                return "TYPE ERROR: failed to convert argument " + std::to_string(i) + " to int.";
            }
            if (errno == ERANGE) {
                return "RANGE ERROR: argument " + std::to_string(i) + " out of range.";
            }
            break;
        }
        case 'r': {
            Node entry;
            entry.r = stof(args[i]);
            a.push_back(entry);
            if (errno == EINVAL) {
                return "TYPE ERROR: failed to convert argument " + std::to_string(i) + " to floating point.";
            }
            if (std::isnan(a[i].r)) {
                return "NaN ERROR: argument " + std::to_string(i) + " is not a number.";
            }
            break;
        }
        case 's': {
            Node entry;
            entry.s = args[i];
            a.push_back(entry);
            break;
        }
        default:
            break;
        }
    }
    /* Empty string meaning "passes". */
    return "";
}

/**
 * @brief Include
 * @param a
 * @return
 */
String
include_action(NodeList a)
{
    return run_script_file("commands/" + a[0].s);
}

/**
 * @brief Error
 * @param a
 * @return
 */
String
error_action(String args)
{
    /*
    _mainWin->setPromptPrefix("ERROR: (" + a[0].s + ") " + a[1].s);
    _mainWin->nativeAppendPromptHistory(QString());
    actuator("end");
    */
    return "";
}

/**
 * @brief Todo
 * @param a
 * @return
 */
String
todo_action(String args)
{
    /*
    _mainWin->nativeAlert("TODO: (" + a[0].s + ") " + a[1].s);
    actuator("end");
    */
    return "";
}

/**
 * @brief AppendPromptHistory
 * @param a
 * @return
 */
String
append_prompt_history_action(String args)
{
    /*
    int args = args.size();
    if (args == 0) {
        _mainWin->nativeAppendPromptHistory(QString());
    }
    else if (args == 1) {
        _mainWin->nativeAppendPromptHistory(a[0].s);
    }
    else {
        return "ERROR: appendPromptHistory() requires one or zero arguments");
    }
    */
    return "";
}

/**
 * @brief MessageBoxW
 * @param a
 * @return
 * argument string "sss"
 */
String
messagebox_action(String args)
{
    /*
    QString type  = a[0].s.toLower();
    QString title = a[1].s;
    QString text  = a[2].s;

    if (type != "critical" && type != "information" && type != "question" && type != "warning") {
        return context->throwError(QScriptContext::UnknownError, "messageBox(): first argument must be \"critical\", \"information\", \"question\" or \"warning\".");
    }

    _mainWin->nativeMessageBox(type, title, text);
    */
    return "";
}

/**
 * argument string "i"
 */
String
is_int_action(String args)
{
    NodeList result;
    StringList a = tokenize(args, ' ');
    String error = convert_args_to_type("IsInt()", a, "i", result);
    if (error != "") {
        return "false";
    }

    return "true";
}

/**
 * argument string "rrrr"
 */
String
PrintArea(String args)
{
    /*
    _mainWin->nativePrintArea(a[0].r, a[1].r, a[2].r, a[3].r);
    */
    return "";
}

/**
 *
 * argument string "iii"
 */
String
set_background_color_action(String args)
{
    /*
    EmbReal r = a[0].r;
    EmbReal g = a[1].r;
    EmbReal b = a[2].r;

    if (r < 0 || r > 255) { return context->throwError(QScriptContext::UnknownError, "setBackgroundColor(): r value must be in range 0-255"); }
    if (g < 0 || g > 255) { return context->throwError(QScriptContext::UnknownError, "setBackgroundColor(): g value must be in range 0-255"); }
    if (b < 0 || b > 255) { return context->throwError(QScriptContext::UnknownError, "setBackgroundColor(): b value must be in range 0-255"); }

    _mainWin->setBackgroundColor(r, g, b);
    */
    return "";
}

/**
 * .
 * argument string "iii"
 */
String
set_crosshair_color_action(String args)
{
    /*
    int r = args[0].r;
    int g = args[1].r;
    int b = args[2].r;

    if (!validRGB(r, g, b)) {
    }

    if (r < 0 || r > 255) {
        return "ERROR setCrossHairColor(): r value must be in range 0-255";
    }
    if (g < 0 || g > 255) {
        return "ERROR setCrossHairColor(): g value must be in range 0-255";
    }
    if (b < 0 || b > 255) {
        return "ERROR setCrossHairColor(): b value must be in range 0-255";
    }

    _mainWin->setCrossHairColor(r, g, b);
    */
    return "";
}

/**
 * .
 * argument string "iii"
 */
String
SetGridColor_action(String args)
{
    /*
    int r = a[0].r;
    int g = a[1].r;
    int b = a[2].r;

    if (r < 0 || r > 255) {
        return "ERROR setGridColor(): r value must be in range 0-255";
    }
    if (g < 0 || g > 255) {
        return "ERROR setGridColor(): g value must be in range 0-255";
    }
    if (b < 0 || b > 255) {
        return "ERROR setGridColor(): b value must be in range 0-255";
    }

    _mainWin->setGridColor(r, g, b);
    */
    return "";
}

String
SetTextAngle_action(String args)
{
    /*
    _mainWin->setTextAngle(a[0].r);
    */
    return "";
}

/**
 * @brief preview_on_action
 * @param args
 * @return
 */
String
preview_on_action(String args)
{
    /*
    QString cloneStr = QString::toStdString(a[0].s).toUpper();
    QString modeStr  = QString::toStdString(a[1].s).toUpper();
    EmbReal x = a[2].r;
    EmbReal y = a[3].r;
    EmbReal data = a[4].r;

    int clone = PREVIEW_CLONE_NULL;
    int mode = PREVIEW_MODE_NULL;
    if     (cloneStr == "SELECTED") { clone = PREVIEW_CLONE_SELECTED; }
    else if (cloneStr == "RUBBER") { clone = PREVIEW_CLONE_RUBBER;   }
    else { return context->throwError(QScriptContext::UnknownError, "previewOn(): first argument must be \"SELECTED\" or \"RUBBER\"."); }

    if     (modeStr == "MOVE") { mode = PREVIEW_MODE_MOVE;   }
    else if (modeStr == "ROTATE") { mode = PREVIEW_MODE_ROTATE; }
    else if (modeStr == "SCALE") { mode = PREVIEW_MODE_SCALE;  }
    else { return context->throwError(QScriptContext::UnknownError, "previewOn(): second argument must be \"MOVE\", \"ROTATE\" or \"SCALE\"."); }

    _mainWin->nativePreviewOn(clone, mode, x, y, data);
    */
    return "";
}

/*
"preview off", nativePreviewOff();
"allow rubber", nativeAllowRubber();
*/

/**
 * @brief set_rubber_text_action
 * @param args
 * @return
 */
String
set_rubber_text_action(String args)
{
    //QString key = QString::fromStdString(a[0].s).toUpper();
    //_mainWin->setRubberText(key, a[1].s);
    return "";
}

/**
 * @brief add_rubber_action
 * @param args
 * @return
 */
String
add_rubber_action(String args)
{
    //QString objType = QString::fromStdString(a[0].s).toUpper();

    /*
    if (!_mainWin->nativeAllowRubber())
        return context->throwError(QScriptContext::UnknownError, "addRubber(): You must use actuator("vulcanize") before you can add another rubber object.");

    EmbReal mx = _mainWin->nativeMouseX();
    EmbReal my = _mainWin->nativeMouseY();

    if (objType == "ARC") {
         //TODO: handle this type
    }
    else if (objType == "BLOCK") {

    } //TODO: handle this type
    else if (objType == "CIRCLE") {
        _mainWin->nativeAddCircle(mx, my, 0, false, OBJ_RUBBER_ON);
    }
    else if (objType == "DIMALIGNED") {

    } //TODO: handle this type
    else if (objType == "DIMANGULAR") {

    } //TODO: handle this type
    else if (objType == "DIMARCLENGTH") {

    } //TODO: handle this type
    else if (objType == "DIMDIAMETER") {

    } //TODO: handle this type
    else if (objType == "DIMLEADER") {
        _mainWin->nativeAddDimLeader(mx, my, mx, my, 0, OBJ_RUBBER_ON);
    }
    else if (objType == "DIMLINEAR") {
        
    } //TODO: handle this type
    else if (objType == "DIMORDINATE") {
        
    } //TODO: handle this type
    else if (objType == "DIMRADIUS") {
        
    } //TODO: handle this type
    else if (objType == "ELLIPSE") {
        _mainWin->nativeAddEllipse(mx, my, 0, 0, 0, 0, OBJ_RUBBER_ON);
    }
    else if (objType == "ELLIPSEARC") {
        
    } //TODO: handle this type
    else if (objType == "HATCH") {
    } //TODO: handle this type
    else if (objType == "IMAGE") {
    } //TODO: handle this type
    else if (objType == "INFINITELINE") {
    } //TODO: handle this type
    else if (objType == "LINE") {
        _mainWin->nativeAddLine(mx, my, mx, my, 0, OBJ_RUBBER_ON);
    }
    else if (objType == "PATH") {
    } //TODO: handle this type
    else if (objType == "POINT") {} //TODO: handle this type
    else if (objType == "POLYGON") { _mainWin->nativeAddPolygon(mx, my, QPainterPath(), OBJ_RUBBER_ON); }
    else if (objType == "POLYLINE") { _mainWin->nativeAddPolyline(mx, my, QPainterPath(), OBJ_RUBBER_ON); }
    else if (objType == "RAY") {} //TODO: handle this type
    else if (objType == "RECTANGLE") { _mainWin->nativeAddRectangle(mx, my, mx, my, 0, 0, OBJ_RUBBER_ON); }
    else if (objType == "SPLINE") {} //TODO: handle this type
    else if (objType == "TEXTMULTI") {} //TODO: handle this type
    else if (objType == "TEXTSINGLE") { _mainWin->nativeAddTextSingle("", mx, my, 0, false, OBJ_RUBBER_ON); }
    */
    return "";
}

/*
"clear rubber", nativeClearRubber();
*/

/**
 * @brief spare_rubber_action
 * @param args
 * @return
 */
String
spare_rubber_action(String args)
{
    //QString objID = QString::fromStdString(a[0].s).toUpper();

    /*
    if (objID == "PATH") {
        _mainWin->nativeSpareRubber(SPARE_RUBBER_PATH);
    }
    else if (objID == "POLYGON") {
        _mainWin->nativeSpareRubber(SPARE_RUBBER_POLYGON);
    }
    else if (objID == "POLYLINE") {
        _mainWin->nativeSpareRubber(SPARE_RUBBER_POLYLINE);
    }
    else {
        bool ok = false;
        qint64 id = objID.toLongLong(&ok);
        if (!ok) return "TYPE ERROR: spareRubber(): error converting object ID into an int64");
        _mainWin->nativeSpareRubber(id);
    }
    */
    return "";
}

/**
 * @brief add_arc_action
 * @param args
 * @return
 */
String
add_arc_action(String args)
{
    //_mainWin->nativeAddArc(a[0].r, a[1].r, a[2].r, a[3].r, a[4].r, a[5].r, OBJ_RUBBER_OFF);
    return "";
}

/**
 * @brief add_circle_action
 * @param args
 * @return
 */
String
add_circle_action(String args)
{
    //_mainWin->nativeAddCircle(a[0].r, a[1].r, a[2].r, a[3].b, OBJ_RUBBER_OFF);
    return "";
}

String
add_slot_action(String a)
{
    //_mainWin->nativeAddSlot(a[0].r, a[1].r, a[2].r, a[3].r, a[4].r, a[5].b, OBJ_RUBBER_OFF);
    return "";
}

String
add_ellipse_action(String args)
{
    //_mainWin->nativeAddEllipse(a[0].r, a[1].r, a[2].r, a[3].r, a[4].r, a[5].b, OBJ_RUBBER_OFF);
    return "";
}

String
add_point_action(String args)
{
    //_mainWin->nativeAddPoint(a[0].r, a[1].r);
    return "";
}

String
add_regular_polygon_action(String args)
{
    //TODO: Node error checking
    debug_message("TODO: finish addRegularPolygon command");
    return "";
}

String
AddPolygon(String args)
{
    /*
    QVariantList varList = a[0].toVariant().toList();
    int varSize = varList.size();
    if (varSize < 2) {
        return "TYPE ERROR: addPolygon(): array must contain at least two elements";
    }
    if (varSize % 2) {
        return "TYPE ERROR: addPolygon(): array cannot contain an odd number of elements";
    }

    bool lineTo = false;
    bool xCoord = true;
    EmbReal x = 0;
    EmbReal y = 0;
    EmbReal startX = 0;
    EmbReal startY = 0;
    QPainterPath path;
    foreach(QVariant var, varList) {
        if (var.canConvert(QVariant::Double)) {
            if (xCoord) {
                xCoord = false;
                x = var.toReal();
            }
            else {
                xCoord = true;
                y = -var.toReal();

                if (lineTo) { path.lineTo(x,y); }
                else { path.moveTo(x,y); lineTo = true; startX = x; startY = y; }
            }
        }
        else {
            return "TYPE ERROR: addPolygon(): array contains one or more invalid elements");
        }
    }

    //Close the polygon
    path.closeSubpath();

    path.translate(-startX, -startY);

    _mainWin->nativeAddPolygon(startX, startY, path, OBJ_RUBBER_OFF);
    */
    return "";
}

/*
SetCursorShape(String args)
    _mainWin->setCursorShape(a[0].s);

CalculateAngle(String args)
    EmbReal result = _mainWin->nativeCalculateAngle(a[0].r, a[1].r, a[2].r, a[3].r);
    return String(result);


PerpendicularDistance(String args)
    EmbReal result = _mainWin->nativePerpendicularDistance(
        a[0].r, a[1].r, a[2].r, a[3].r, a[4].r, a[5].r);
    return String(result);
*/

/**
 * @brief MainWindow::recentMenuAboutToShow
 */
void
MainWindow::recentMenuAboutToShow()
{
    debug_message("MainWindow::recentMenuAboutToShow()");
    subMenuHash["recent"]->clear();

    QFileInfo recentFileInfo;
    QString recentValue;
    for (int i = 0; i < settings.opensave_recent_list_of_files.size(); ++i) {
        //If less than the max amount of entries add to menu
        if (i < settings.opensave_recent_max_files) {
            recentFileInfo = QFileInfo(settings.opensave_recent_list_of_files.at(i));
            if (recentFileInfo.exists() && validFileFormat(recentFileInfo.fileName())) {
                recentValue.setNum(i+1);
                QAction* rAction;
                if     (recentValue.toInt() >= 1 && recentValue.toInt() <= 9)
                    rAction = new QAction("&" + recentValue + " " + recentFileInfo.fileName(), this);
                else if (recentValue.toInt() == 10)
                    rAction = new QAction("1&0 " + recentFileInfo.fileName(), this);
                else
                    rAction = new QAction(recentValue + " " + recentFileInfo.fileName(), this);
                rAction->setCheckable(false);
                rAction->setData(settings.opensave_recent_list_of_files.at(i));
                subMenuHash["recent"]->addAction(rAction);
                connect(rAction, SIGNAL(triggered()), this, SLOT(openrecentfile()));
            }
        }
    }
    //Ensure the list only has max amount of entries
    while (settings.opensave_recent_list_of_files.size() > settings.opensave_recent_max_files) {
        settings.opensave_recent_list_of_files.removeLast();
    }
}

/**
 * @brief MainWindow::windowMenuAboutToShow
 */
void
MainWindow::windowMenuAboutToShow()
{
    debug_message("MainWindow::windowMenuAboutToShow()");
    menuHash["window"]->clear();
    menuHash["window"]->addAction(actionHash["windowclose"]);
    menuHash["window"]->addAction(actionHash["windowcloseall"]);
    menuHash["window"]->addSeparator();
    menuHash["window"]->addAction(actionHash["windowcascade"]);
    menuHash["window"]->addAction(actionHash["windowtile"]);
    menuHash["window"]->addSeparator();
    menuHash["window"]->addAction(actionHash["windownext"]);
    menuHash["window"]->addAction(actionHash["windowprevious"]);

    menuHash["window"]->addSeparator();
    QList<QMdiSubWindow*> windows = mdiArea->subWindowList();
    for (int i = 0; i < windows.count(); ++i) {
        QAction* aAction = new QAction(windows.at(i)->windowTitle(), this);
        aAction->setCheckable(true);
        aAction->setData(i);
        menuHash["window"]->addAction(aAction);
        connect(aAction, SIGNAL(toggled(bool)), this, SLOT(windowMenuActivated(bool)));
        aAction->setChecked(mdiArea->activeSubWindow() == windows.at(i));
    }
}

/**
 * @brief MainWindow::windowMenuActivated
 * @param checked
 */
void
MainWindow::windowMenuActivated(bool checked)
{
    debug_message("MainWindow::windowMenuActivated()");
    QAction* aSender = qobject_cast<QAction*>(sender());
    if (!aSender)
        return;
    QWidget* w = mdiArea->subWindowList().at(aSender->data().toInt());
    if (w && checked)
        w->setFocus();
}

/**
 * @brief MainWindow::newFile
 */
void
MainWindow::newFile()
{
    debug_message("MainWindow::newFile()");
    docIndex++;
    numOfDocs++;
    MdiWindow* mdiWin = new MdiWindow(docIndex, mdiArea, Qt::SubWindow);
    connect(mdiWin, SIGNAL(sendCloseMdiWin(MdiWindow*)), this, SLOT(onCloseMdiWin(MdiWindow*)));
    connect(mdiArea, SIGNAL(subWindowActivated(QMdiSubWindow*)), this, SLOT(onWindowActivated(QMdiSubWindow*)));

    updateMenuToolbarStatusbar();
    windowMenuAboutToShow();

    if (mdiWin->gview) {
        mdiWin->gview->recalculateLimits();
        mdiWin->gview->zoomExtents();
    }
}

/**
 * @brief MainWindow::openFile
 * @param recent
 * @param recentFile
 */
void
MainWindow::openFile(bool recent, const QString& recentFile)
{
    debug_message("MainWindow::openFile()");

    QApplication::setOverrideCursor(Qt::ArrowCursor);

    QStringList files;
    bool preview = settings.opensave_open_thumbnail;
    openFilesPath = QString::fromStdString(settings.opensave_recent_directory);

    //Check to see if this from the recent files list
    if (recent) {
        files.append(recentFile);
        openFilesSelected(files);
    }
    else {
        if (!preview) {
            //TODO: set getOpenFileNames' selectedFilter Node from settings.opensave_open_format
            files = QFileDialog::getOpenFileNames(this, tr("Open"), openFilesPath, formatFilterOpen);
            openFilesSelected(files);
        }
        else {
            PreviewDialog* openDialog = new PreviewDialog(this, tr("Open w/Preview"), openFilesPath, formatFilterOpen);
            //TODO: set openDialog->selectNameFilter(const QString& filter) from settings.opensave_open_format
            connect(openDialog, SIGNAL(filesSelected(QStringList)), this, SLOT(openFilesSelected(QStringList)));
            openDialog->exec();
        }
    }

    QApplication::restoreOverrideCursor();
}

/**
 * @brief MainWindow::openFilesSelected
 * @param filesToOpen
 */
void
MainWindow::openFilesSelected(const QStringList& filesToOpen)
{
    bool doOnce = true;

    if (filesToOpen.count()) {
        for (int i = 0; i < filesToOpen.count(); i++) {
            if (!validFileFormat(filesToOpen[i]))
                continue;

            QMdiSubWindow* existing = findMdiWindow(filesToOpen[i]);
            if (existing) {
                mdiArea->setActiveSubWindow(existing);
                continue;
            }

            //The docIndex doesn't need increased as it is only used for unnamed files
            numOfDocs++;
            MdiWindow* mdiWin = new MdiWindow(docIndex, mdiArea, Qt::SubWindow);
            connect(mdiWin, SIGNAL(sendCloseMdiWin(MdiWindow*)), this, SLOT(onCloseMdiWin(MdiWindow*)));
            connect(mdiArea, SIGNAL(subWindowActivated(QMdiSubWindow*)), this, SLOT(onWindowActivated(QMdiSubWindow*)));

            //Make sure the toolbars/etc... are shown before doing their zoomExtents
            if (doOnce) { updateMenuToolbarStatusbar(); doOnce = false; }

            if (mdiWin->loadFile(filesToOpen.at(i))) {
                statusbar->showMessage(tr("File(s) loaded"), 2000);
                mdiWin->show();
                mdiWin->showMaximized();
                //Prevent duplicate entries in the recent files list
                if (!settings.opensave_recent_list_of_files.contains(filesToOpen.at(i), Qt::CaseInsensitive)) {
                    settings.opensave_recent_list_of_files.prepend(filesToOpen.at(i));
                }
                //Move the recent file to the top of the list
                else {
                    settings.opensave_recent_list_of_files.removeAll(filesToOpen.at(i));
                    settings.opensave_recent_list_of_files.prepend(filesToOpen.at(i));
                }
                settings.opensave_recent_directory = QFileInfo(filesToOpen.at(i)).absolutePath().toStdString();

                if (mdiWin->gview) {
                    mdiWin->gview->recalculateLimits();
                    mdiWin->gview->zoomExtents();
                }
            }
            else {
                mdiWin->close();
            }
        }
    }

    windowMenuAboutToShow();
}

/**
 * @brief MainWindow::openrecentfile
 */
void
MainWindow::openrecentfile()
{
    debug_message("MainWindow::openrecentfile()");

    //Check to see if this from the recent files list
    QAction* recentSender = qobject_cast<QAction*>(sender());
    if (recentSender) {
        openFile(true, recentSender->data().toString());
    }
}

/**
 * @brief MainWindow::savefile
 */
void
MainWindow::savefile()
{
    debug_message("MainWindow::savefile()");
}

/**
 * @brief MainWindow::saveasfile
 */
void
MainWindow::saveasfile()
{
    debug_message("MainWindow::saveasfile()");
    // need to find the activeSubWindow before it loses focus to the FileDialog
    MdiWindow* mdiWin = qobject_cast<MdiWindow*>(mdiArea->activeSubWindow());
    if (!mdiWin)
        return;

    QString file;
    openFilesPath = QString::fromStdString(settings.opensave_recent_directory);
    file = QFileDialog::getSaveFileName(this, tr("Save As"), openFilesPath, formatFilterSave);

    mdiWin->saveFile(file);
}

/**
 * @brief MainWindow::findMdiWindow
 * @param fileName
 * @return
 */
QMdiSubWindow* MainWindow::findMdiWindow(const QString& fileName)
{
    debug_message("MainWindow::findMdiWindow(%s)" + fileName.toStdString());
    QString canonicalFilePath = QFileInfo(fileName).canonicalFilePath();

    foreach(QMdiSubWindow* subWindow, mdiArea->subWindowList()) {
        MdiWindow* mdiWin = qobject_cast<MdiWindow*>(subWindow);
        if (mdiWin) {
            if (mdiWin->curFile == canonicalFilePath) {
                return subWindow;
            }
        }
    }
    return 0;
}

/**
 * @brief MainWindow::closeEvent
 * @param event
 */
void
MainWindow::closeEvent(QCloseEvent* event)
{
    mdiArea->closeAllSubWindows();
    write_settings();
    event->accept();
}

/**
 * @brief MainWindow::onCloseWindow
 */
void
MainWindow::onCloseWindow()
{
    debug_message("MainWindow::onCloseWindow()");
    MdiWindow* mdiWin = qobject_cast<MdiWindow*>(mdiArea->activeSubWindow());
    if (mdiWin) {
        onCloseMdiWin(mdiWin);
    }
}

/**
 * @brief MainWindow::onCloseMdiWin
 * @param theMdiWin
 */
void
MainWindow::onCloseMdiWin(MdiWindow* theMdiWin)
{
    debug_message("MainWindow::onCloseMdiWin()");
    numOfDocs--;

    bool keepMaximized;
    if (theMdiWin) { keepMaximized = theMdiWin->isMaximized(); }

    mdiArea->removeSubWindow(theMdiWin);
    theMdiWin->deleteLater();

    updateMenuToolbarStatusbar();
    windowMenuAboutToShow();

    if (keepMaximized) {
        MdiWindow* mdiWin = qobject_cast<MdiWindow*>(mdiArea->activeSubWindow());
        if (mdiWin) {
            mdiWin->showMaximized();
        }
    }
}

/**
 * @brief MainWindow::onWindowActivated
 * @param w
 */
void
MainWindow::onWindowActivated(QMdiSubWindow* w)
{
    debug_message("MainWindow::onWindowActivated()");
    MdiWindow* mdiWin = qobject_cast<MdiWindow*>(w);
    if (mdiWin) {
        mdiWin->onWindowActivated();
    }
}

/**
 * @brief MainWindow::resizeEvent
 * @param e
 */
void
MainWindow::resizeEvent(QResizeEvent* e)
{
    debug_message("MainWindow::resizeEvent()");
    QMainWindow::resizeEvent(e);
    statusBar()->setSizeGripEnabled(!isMaximized());
}

/**
 * @brief MainWindow::getFileSeparator
 * @return
 */
QAction*
MainWindow::getFileSeparator()
{
    debug_message("MainWindow::getFileSeparator()");
    return myFileSeparator;
}

/**
 * @brief MainWindow::updateMenuToolbarStatusbar
 */
void
MainWindow::updateMenuToolbarStatusbar()
{
    debug_message("MainWindow::updateMenuToolbarStatusbar()");

    actionHash["print"]->setEnabled(numOfDocs > 0);
    actionHash["windowclose"]->setEnabled(numOfDocs > 0);
    actionHash["designdetails"]->setEnabled(numOfDocs > 0);

    if (numOfDocs) {
        for (auto iter=toolbarHash.begin(); iter != toolbarHash.end(); iter++) {
            iter->second->show();
        }

        //DockWidgets
        dockPropEdit->show();
        dockUndoEdit->show();

        //Menus
        menuBar()->clear();
        for (int i=0; i<(int)get_sl("menubar_order").size(); i++) {
            menuBar()->addMenu(menuHash[get_sl("menubar_order")[i]]);
        }

        menuHash["window"]->setEnabled(true);

        //Statusbar
        statusbar->clearMessage();
        statusbar->statusBarMouseCoord->show();
        statusbar->statusBarSnapButton->show();
        statusbar->statusBarGridButton->show();
        statusbar->statusBarRulerButton->show();
        statusbar->statusBarOrthoButton->show();
        statusbar->statusBarPolarButton->show();
        statusbar->statusBarQSnapButton->show();
        statusbar->statusBarQTrackButton->show();
        statusbar->statusBarLwtButton->show();
    }
    else {
        for (auto iter=toolbarHash.begin(); iter != toolbarHash.end(); iter++) {
            iter->second->hide();
        }

        toolbarHash["file"]->show();
        toolbarHash["edit"]->show();

        //DockWidgets
        dockPropEdit->hide();
        dockUndoEdit->hide();

        //Menus
        menuBar()->clear();
        menuBar()->addMenu(menuHash["file"]);
        menuBar()->addMenu(menuHash["edit"]);
        menuBar()->addMenu(menuHash["settings"]);
        menuBar()->addMenu(menuHash["window"]);
        menuBar()->addMenu(menuHash["help"]);

        menuHash["window"]->setEnabled(false);

        //Statusbar
        statusbar->clearMessage();
        statusbar->statusBarMouseCoord->hide();
        statusbar->statusBarSnapButton->hide();
        statusbar->statusBarGridButton->hide();
        statusbar->statusBarRulerButton->hide();
        statusbar->statusBarOrthoButton->hide();
        statusbar->statusBarPolarButton->hide();
        statusbar->statusBarQSnapButton->hide();
        statusbar->statusBarQTrackButton->hide();
        statusbar->statusBarLwtButton->hide();
    }
    hideUnimplemented();
}

/**
 * @brief MainWindow::hideUnimplemented
 */
void
MainWindow::hideUnimplemented()
{
    debug_message("MainWindow::hideUnimplemented()");
}

/**
 * @brief MainWindow::validFileFormat
 * @param fileName
 * @return
 *
 * \todo check the file exists on the system, rename to validFile?
 */
bool
MainWindow::validFileFormat(const QString& fileName)
{
    if (fileName == "") {
        return false;
    }
    if (emb_identify_format(qPrintable(fileName)) >= 0) {
        return true;
    }
    return false;
}

/**
 * @brief MainWindow::loadFormats
 */
void
MainWindow::loadFormats()
{
    char stable, unstable;
    QString supportedReaders  = "All Supported Files (";
    QString individualReaders = "All Files (*);;";
    QString supportedWriters  = "All Supported Files (";
    QString individualWriters = "All Files (*);;";
    QString supportedStr;
    QString individualStr;

    //TODO: Stable Only (Settings Option)
    //stable = 'S'; unstable = 'S';

    //Stable + Unstable
    stable = 'S'; unstable = 'U';

    const char* extension = 0;
    const char* description = 0;
    char readerState;
    char writerState;

    for (int i=0; i<numberOfFormats; i++) {
        extension = formatTable[i].extension;
        description = formatTable[i].description;
        readerState = formatTable[i].reader_state;
        writerState = formatTable[i].writer_state;

        QString upperExt = QString(extension).toUpper();
        supportedStr = "*" + upperExt + " ";
        individualStr = upperExt.replace(".", "") + " - " + description + " (*" + extension + ");;";
        if (readerState == stable || readerState == unstable) {
            //Exclude color file formats from open dialogs
            if (upperExt != "COL" && upperExt != "EDR" && upperExt != "INF" && upperExt != "RGB") {
                supportedReaders.append(supportedStr);
                individualReaders.append(individualStr);
            }
        }
        if (writerState == stable || writerState == unstable) {
            supportedWriters.append(supportedStr);
            individualWriters.append(individualStr);
        }
    }

    supportedReaders.append(");;");
    supportedWriters.append(");;");

    formatFilterOpen = supportedReaders + individualReaders;
    formatFilterSave = supportedWriters + individualWriters;

    //TODO: Fixup custom filter
    /*
    QString custom = settings.custom_filter;
    if (custom.contains("supported", Qt::CaseInsensitive))
        custom = ""; //This will hide it
    else if (!custom.contains("*", Qt::CaseInsensitive))
        custom = ""; //This will hide it
    else
        custom = "Custom Filter(" + custom + ");;";

    return tr(qPrintable(custom + supported + all));
    */
}

/**
 * @brief MainWindow::closeToolBar
 * @param action
 */
void
MainWindow::closeToolBar(QAction* action)
{
    if (action->objectName() == "toolbarclose") {
        QToolBar* tb = qobject_cast<QToolBar*>(sender());
        if (tb) {
            debug_message(tb->objectName().toStdString() + "%s closed.");
            tb->hide();
        }
    }
}

/**
 * @brief MainWindow::floatingChangedToolBar
 * @param isFloating
 */
void
MainWindow::floatingChangedToolBar(bool isFloating)
{
    QToolBar* tb = qobject_cast<QToolBar*>(sender());
    if (tb) {
        if (isFloating) {
            /*
            //TODO: Determine best suited close button on various platforms.
            QStyle::SP_DockWidgetCloseButton
            QStyle::SP_TitleBarCloseButton
            QStyle::SP_DialogCloseButton
            */
            QAction *ACTION = new QAction(tb->style()->standardIcon(QStyle::SP_DialogCloseButton), "Close", this);
            ACTION->setStatusTip("Close the " + tb->windowTitle() + " Toolbar");
            ACTION->setObjectName("toolbarclose");
            tb->addAction(ACTION);
            connect(tb, SIGNAL(actionTriggered(QAction*)), this, SLOT(closeToolBar(QAction*)));
        }
        else {
            QList<QAction*> actList = tb->actions();
            for (int i = 0; i < actList.size(); ++i) {
                QAction* ACTION = actList.value(i);
                if (ACTION->objectName() == "toolbarclose") {
                    tb->removeAction(ACTION);
                    disconnect(tb, SIGNAL(actionTriggered(QAction*)), this, SLOT(closeToolBar(QAction*)));
                    delete ACTION;
                }
            }
        }
    }
}

/**
 *
 */
UiObject
circle_main(void)
{
    UiObject global;
    EmbVector point1;
    EmbVector point2;
    EmbVector point3;
    EmbReal rad;
    EmbReal dia;
    EmbVector center;
    int mode;
    global.mode = CIRCLE_MODE_1P_RAD;
    /*
    initCommand();
    clearSelection();
    initCommand();
    clearSelection();
    global.x1 = NaN;
    global.y1 = NaN;
    global.x2 = NaN;
    global.y2 = NaN;
    global.x3 = NaN;
    global.y3 = NaN;
    setPromptPrefix(tr("Specify center point for circle or [3P/2P/Ttr (tan tan radius)]: "));
    */
    return global;
}

/**
 * .
 */
void
circle_click(UiObject *global, EmbVector v)
{
    /*
    if (global->mode == MODE_1P_RAD) {
        if (std::isnan(global.x1)) {
            global.x1 = x;
            global.y1 = y;
            global->center.x = x;
            global->center.y = y;
            addRubber("CIRCLE");
            setRubberMode("CIRCLE_1P_RAD");
            setRubberPoint("CIRCLE_CENTER", global->center.x, global->center.y);
            appendPromptHistory();
            setPromptPrefix(tr("Specify radius of circle or [Diameter]: "));
        }
        else {
            global.x2 = x;
            global.y2 = y;
            setRubberPoint("CIRCLE_RADIUS", global.x2, global.y2);
            actuator("vulcanize");
            appendPromptHistory();
            actuator("end");
        }
    }
    else if (global->mode == MODE_1P_DIA) {
        if (std::isnan(global.x1)) {
            error("CIRCLE", tr("This should never happen."));
        }
        else {
            global.x2 = x;
            global.y2 = y;
            setRubberPoint("CIRCLE_DIAMETER", global.x2, global.y2);
            actuator("vulcanize");
            appendPromptHistory();
            actuator("end");
        }
    }
    else if (global->mode == MODE_2P) {
        if (std::isnan(global.x1)) {
            global.x1 = x;
            global.y1 = y;
            addRubber("CIRCLE");
            setRubberMode("CIRCLE_2P");
            setRubberPoint("CIRCLE_TAN1", global.x1, global.y1);
            appendPromptHistory();
            setPromptPrefix(tr("Specify second end point of circle's diameter: "));
        }
        else if (std::isnan(global.x2)) {
            global.x2 = x;
            global.y2 = y;
            setRubberPoint("CIRCLE_TAN2", global.x2, global.y2);
            actuator("vulcanize");
            appendPromptHistory();
            actuator("end");
        }
        else {
            error("CIRCLE", tr("This should never happen."));
        }
    }
    else if (global->mode == MODE_3P) {
        if (std::isnan(global.x1)) {
            global.x1 = x;
            global.y1 = y;
            appendPromptHistory();
            setPromptPrefix(tr("Specify second point on circle: "));
        }
        else if (std::isnan(global.x2)) {
            global.x2 = x;
            global.y2 = y;
            addRubber("CIRCLE");
            setRubberMode("CIRCLE_3P");
            setRubberPoint("CIRCLE_TAN1", global.x1, global.y1);
            setRubberPoint("CIRCLE_TAN2", global.x2, global.y2);
            appendPromptHistory();
            setPromptPrefix(tr("Specify third point on circle: "));
        }
        else if (std::isnan(global.x3)) {
            global.x3 = x;
            global.y3 = y;
            setRubberPoint("CIRCLE_TAN3", global.x3, global.y3);
            actuator("vulcanize");
            appendPromptHistory();
            actuator("end");
        }
        else {
            error("CIRCLE", tr("This should never happen."));
        }
    }
    else if (global->mode == MODE_TTR) {
        if (std::isnan(global.x1)) {
            global.x1 = x;
            global.y1 = y;
            appendPromptHistory();
            setPromptPrefix(tr("Specify point on object for second tangent of circle: "));
        }
        else if (std::isnan(global.x2)) {
            global.x2 = x;
            global.y2 = y;
            appendPromptHistory();
            setPromptPrefix(tr("Specify radius of circle: "));
        }
        else if (std::isnan(global.x3)) {
            global.x3 = x;
            global.y3 = y;
            appendPromptHistory();
            setPromptPrefix(tr("Specify second point: "));
        }
        else {
            todo("CIRCLE", "click() for TTR");
        }
    }
    */
}

/**
 * .
 */
void
circle_context(String str)
{
    //todo("CIRCLE", "context()");
}

/**
 * .
 */
String
circle_prompt(String str)
{
    /*
    if (global->mode == MODE_1P_RAD) {
        if (std::isnan(global.x1)) {
            if (str == "2P") {
                global.mode = MODE_2P;
                setPromptPrefix(tr("Specify first end point of circle's diameter: "));
            }
            else if (str == "3P") {
                global.mode = MODE_3P;
                setPromptPrefix(tr("Specify first point of circle: "));
            }
            else if (str == "T" || str == "TTR") {
                global.mode = MODE_TTR;
                setPromptPrefix(tr("Specify point on object for first tangent of circle: "));
            }
            else {
                EmbReal strList = str.split(",");
                if (std::isnan(strList[0]) || std::isnan(strList[1])) {
                    alert(tr("Point or option keyword required."));
                    setPromptPrefix(tr("Specify center point for circle or [3P/2P/Ttr (tan tan radius)]: "));
                }
                else {
                    global.x1 = Number(strList[0]);
                    global.y1 = Number(strList[1]);
                    global->center.x = global.x1;
                    global->center.y = global.y1;
                    addRubber("CIRCLE");
                    setRubberMode("CIRCLE_1P_RAD");
                    setRubberPoint("CIRCLE_CENTER", global->center.x, global->center.y);
                    setPromptPrefix(tr("Specify radius of circle or [Diameter]: "));
                }
            }
        }
        else {
            if (str == "D" || str == "DIAMETER") {
                global.mode = MODE_1P_DIA;
                setRubberMode("CIRCLE_1P_DIA");
                setPromptPrefix(tr("Specify diameter of circle: "));
            }
            else {
                EmbReal num = Number(str);
                if (std::isnan(num)) {
                    alert(tr("Requires numeric radius, point on circumference, or \"D\"."));
                    setPromptPrefix(tr("Specify radius of circle or [Diameter]: "));
                }
                else {
                    global.rad = num;
                    global.x2 = global.x1 + global.rad;
                    global.y2 = global.y1;
                    setRubberPoint("CIRCLE_RADIUS", global.x2, global.y2);
                    actuator("vulcanize");
                    actuator("end");
                }
            }
        }
    }
    else if (global->mode == MODE_1P_DIA) {
        if (std::isnan(global.x1)) {
            error("CIRCLE", tr("This should never happen."));
        }
        if (std::isnan(global.x2)) {
            EmbReal num = Number(str);
            if (std::isnan(num)) {
                alert(tr("Requires numeric distance or second point."));
                setPromptPrefix(tr("Specify diameter of circle: "));
            }
            else {
                global.dia = num;
                global.x2 = global.x1 + global.dia;
                global.y2 = global.y1;
                setRubberPoint("CIRCLE_DIAMETER", global.x2, global.y2);
                actuator("vulcanize");
                actuator("end");
            }
        }
        else {
            error("CIRCLE", tr("This should never happen."));
        }
    }
    else if (global->mode == MODE_2P) {
        if (std::isnan(global.x1)) {
            EmbReal strList = str.split(",");
            if (std::isnan(strList[0]) || std::isnan(strList[1])) {
                alert(tr("Invalid point."));
                setPromptPrefix(tr("Specify first end point of circle's diameter: "));
            }
            else {
                global.x1 = Number(strList[0]);
                global.y1 = Number(strList[1]);
                addRubber("CIRCLE");
                setRubberMode("CIRCLE_2P");
                setRubberPoint("CIRCLE_TAN1", global.x1, global.y1);
                setPromptPrefix(tr("Specify second end point of circle's diameter: "));
            }
        }
        else if (std::isnan(global.x2)) {
            EmbReal strList = str.split(",");
            if (std::isnan(strList[0]) || std::isnan(strList[1])) {
                alert(tr("Invalid point."));
                setPromptPrefix(tr("Specify second end point of circle's diameter: "));
            }
            else {
                global.x2 = Number(strList[0]);
                global.y2 = Number(strList[1]);
                setRubberPoint("CIRCLE_TAN2", global.x2, global.y2);
                actuator("vulcanize");
                actuator("end");
            }
        }
        else {
            error("CIRCLE", tr("This should never happen."));
        }
    }
    else if (global->mode == MODE_3P) {
        if (std::isnan(global.x1)) {
            EmbReal strList = str.split(",");
            if (std::isnan(strList[0]) || std::isnan(strList[1])) {
                alert(tr("Invalid point."));
                setPromptPrefix(tr("Specify first point of circle: "));
            }
            else {
                global.x1 = Number(strList[0]);
                global.y1 = Number(strList[1]);
                setPromptPrefix(tr("Specify second point of circle: "));
            }
        }
        else if (std::isnan(global.x2)) {
            EmbReal strList = str.split(",");
            if (std::isnan(strList[0]) || std::isnan(strList[1])) {
                alert(tr("Invalid point."));
                setPromptPrefix(tr("Specify second point of circle: "));
            }
            else {
                global.x2 = Number(strList[0]);
                global.y2 = Number(strList[1]);
                addRubber("CIRCLE");
                setRubberMode("CIRCLE_3P");
                setRubberPoint("CIRCLE_TAN1", global.x1, global.y1);
                setRubberPoint("CIRCLE_TAN2", global.x2, global.y2);
                setPromptPrefix(tr("Specify third point of circle: "));
            }
        }
        else if (std::isnan(global.x3)) {
            EmbReal strList = str.split(",");
            if (std::isnan(strList[0]) || std::isnan(strList[1])) {
                alert(tr("Invalid point."));
                setPromptPrefix(tr("Specify third point of circle: "));
            }
            else {
                global.x3 = Number(strList[0]);
                global.y3 = Number(strList[1]);
                setRubberPoint("CIRCLE_TAN3", global.x3, global.y3);
                actuator("vulcanize");
                actuator("end");
            }
        }
        else {
            error("CIRCLE", tr("This should never happen."));
        }
    }
    else if (global->mode == MODE_TTR) {
        todo("CIRCLE", "prompt() for TTR");
    }
    */
    return "";
}

/**
 * .
 */
void
distance_main()
{
    /*
    var global = {}; //Required
    global.x1;
    global.y1;
    global.x2;
    global.y2;

    initCommand();
    clearSelection();
    global.x1 = NaN;
    global.y1 = NaN;
    global.x2 = NaN;
    global.y2 = NaN;
    setPromptPrefix(tr("Specify first point: "));
    */
}

/**
 * .
 */
void
distance_click(UiObject *global, EmbVector v)
{
    /*
    if (std::isnan(global.x1)) {
        global.x1 = x;
        global.y1 = y;
        addRubber("LINE");
        setRubberMode("LINE");
        setRubberPoint("LINE_START", global.x1, global.y1);
        appendPromptHistory();
        setPromptPrefix(tr("Specify second point: "));
    }
    else {
        appendPromptHistory();
        global.x2 = x;
        global.y2 = y;
        reportDistance();
        actuator("end");
    }
    */
}

/**
 * @brief distance_context
 * @param args
 * @return
 */
String
distance_context(String args)
{
    //todo("DISTANCE", "context()");
    return "";
}

/**
 * @brief distance_prompt
 * @param args
 * @return
 */
String
distance_prompt(String args)
{
    /*
    EmbReal strList = str.split(",");
    if (std::isnan(global.x1)) {
        if (std::isnan(strList[0]) || std::isnan(strList[1])) {
            alert(tr("Requires numeric distance or two points."));
            setPromptPrefix(tr("Specify first point: "));
        }
        else {
            global.x1 = Number(strList[0]);
            global.y1 = Number(strList[1]);
            addRubber("LINE");
            setRubberMode("LINE");
            setRubberPoint("LINE_START", global.x1, global.y1);
            setPromptPrefix(tr("Specify second point: "));
        }
    }
    else {
        if (std::isnan(strList[0]) || std::isnan(strList[1])) {
            alert(tr("Requires numeric distance or two points."));
            setPromptPrefix(tr("Specify second point: "));
        }
        else {
            global.x2 = Number(strList[0]);
            global.y2 = Number(strList[1]);
            reportDistance();
            actuator("end");
        }
    }
    */
    return "";
}

/**
 * Cartesian Coordinate System reported:
 *
 *               (+)
 *               90
 *               |
 *      (-) 180__|__0 (+)
 *               |
 *              270
 *              (-)
 */
 /*
void
reportDistance()
{
    EmbReal dx = global.x2 - global.x1;
    EmbReal dy = global.y2 - global.y1;

    EmbReal dist = calculateDistance(global.x1,global.y1,global.x2, global.y2);
    EmbReal angle = calculateAngle(global.x1,global.y1,global.x2, global.y2);

    setPromptPrefix(tr("Distance") + " = " + dist.toString() + ", " + tr("Angle") + " = " + angle.toString());
    appendPromptHistory();
    setPromptPrefix(tr("Delta X") + " = " + dx.toString() + ", " + tr("Delta Y") + " = " + dy.toString());
    appendPromptHistory();
}
*/

/**
 *
 */
String
dolphin_main(String args)
{
    /*
    var global = {}; //Required
    global.numPoints = 512; //Default //TODO: min:64 max:8192
    global->center.x;
    global->center.y;
    global.sx = 0.04; //Default
    global.sy = 0.04; //Default
    global.numPoints;
    global.mode;

    initCommand();
    clearSelection();
    global->center.x = NaN;
    global->center.y = NaN;
    global.mode = MODE_NUM_POINTS;

    addRubber("POLYGON");
    setRubberMode("POLYGON");
    updateDolphin(global.numPoints, global.sx, global.sy);
    spareRubber("POLYGON");
    actuator("end");
    */
    return "";
}

/**
 * .
 */
 /*
void
updateDolphin(int numPoints, EmbReal xScale, EmbReal yScale)
{
    for (int i = 0; i <= numPoints; i++) {
        EmbReal t = (2.0 * emb_constant_pi) / numPoints*i;
        EmbVector v;
        v.x = fourier_series(t, dolphin_x);
        v.y = fourier_series(t, dolphin_y);

        setRubberPoint("POLYGON_POINT_" + i.toString(), xx*xScale, yy*yScale);
    }

    setRubberText("POLYGON_NUM_POINTS", numPoints.toString());
}
*/

/**
 * .
 */
 /*
UiObject
ellipse_main(void)
{
    var global = {}; //Required
    global.width;
    global.height;
    global.rot;
    global.mode;
    initCommand();
    clearSelection();
    global.mode = MODE_MAJORDIAMETER_MINORRADIUS;
    global.center = {0.0f, 0.0f};
    global.point1 = {0.0f, 0.0f};
    global.point2 = {0.0f, 0.0f};
    global.point3 = {0.0f, 0.0f};
    setPromptPrefix(tr("Specify first axis start point or [Center]: "));
    return global;
}
*/

/**
 * .
 */
 /*
void
ellipse_click(x, y)
{
    if (global->mode == MODE_MAJORDIAMETER_MINORRADIUS) {
        if (std::isnan(global.x1)) {
            global.x1 = x;
            global.y1 = y;
            addRubber("ELLIPSE");
            setRubberMode("ELLIPSE_LINE");
            setRubberPoint("ELLIPSE_LINE_POINT1", global.x1, global.y1);
            appendPromptHistory();
            setPromptPrefix(tr("Specify first axis end point: "));
        }
        else if (std::isnan(global.x2)) {
            global.x2 = x;
            global.y2 = y;
            global->center.x = (global.x1 + global.x2)/2.0;
            global->center.y = (global.y1 + global.y2)/2.0;
            global.width = calculateDistance(global.x1, global.y1, global.x2, global.y2);
            global.rot = calculateAngle(global.x1, global.y1, global.x2, global.y2);
            setRubberMode("ELLIPSE_MAJORDIAMETER_MINORRADIUS");
            setRubberPoint("ELLIPSE_AXIS1_POINT1", global.x1, global.y1);
            setRubberPoint("ELLIPSE_AXIS1_POINT2", global.x2, global.y2);
            setRubberPoint("ELLIPSE_CENTER", global->center.x, global->center.y);
            setRubberPoint("ELLIPSE_WIDTH", global.width, 0);
            setRubberPoint("ELLIPSE_ROT", global.rot, 0);
            appendPromptHistory();
            setPromptPrefix(tr("Specify second axis end point or [Rotation]: "));
        }
        else if (std::isnan(global.x3)) {
            global.x3 = x;
            global.y3 = y;
            global.height = perpendicularDistance(global.x3, global.y3, global.x1, global.y1, global.x2, global.y2)*2.0;
            setRubberPoint("ELLIPSE_AXIS2_POINT2", global.x3, global.y3);
            actuator("vulcanize");
            appendPromptHistory();
            actuator("end");
        }
        else {
            error("ELLIPSE", tr("This should never happen."));
        }
    }
    else if (global->mode == MODE_MAJORRADIUS_MINORRADIUS) {
        if (std::isnan(global.x1)) {
            global.x1 = x;
            global.y1 = y;
            global->center.x = global.x1;
            global->center.y = global.y1;
            addRubber("ELLIPSE");
            setRubberMode("ELLIPSE_LINE");
            setRubberPoint("ELLIPSE_LINE_POINT1", global.x1, global.y1);
            setRubberPoint("ELLIPSE_CENTER", global->center.x, global->center.y);
            appendPromptHistory();
            setPromptPrefix(tr("Specify first axis end point: "));
        }
        else if (std::isnan(global.x2)) {
            global.x2 = x;
            global.y2 = y;
            global.width = calculateDistance(global->center.x, global->center.y, global.x2, global.y2)*2.0;
            global.rot = calculateAngle(global.x1, global.y1, global.x2, global.y2);
            setRubberMode("ELLIPSE_MAJORRADIUS_MINORRADIUS");
            setRubberPoint("ELLIPSE_AXIS1_POINT2", global.x2, global.y2);
            setRubberPoint("ELLIPSE_WIDTH", global.width, 0);
            setRubberPoint("ELLIPSE_ROT", global.rot, 0);
            appendPromptHistory();
            setPromptPrefix(tr("Specify second axis end point or [Rotation]: "));
        }
        else if (std::isnan(global.x3)) {
            global.x3 = x;
            global.y3 = y;
            global.height = perpendicularDistance(global.x3, global.y3, global->center.x, global->center.y, global.x2, global.y2)*2.0;
            setRubberPoint("ELLIPSE_AXIS2_POINT2", global.x3, global.y3);
            actuator("vulcanize");
            appendPromptHistory();
            actuator("end");
        }
        else {
            error("ELLIPSE", tr("This should never happen."));
        }
    }
    else if (global->mode == MODE_ELLIPSE_ROTATION) {
        if (std::isnan(global.x1)) {
            error("ELLIPSE", tr("This should never happen."));
        }
        else if (std::isnan(global.x2)) {
            error("ELLIPSE", tr("This should never happen."));
        }
        else if (std::isnan(global.x3)) {
            EmbReal angle = calculateAngle(global->center.x, global->center.y, x, y);
            global.height = cos(angle*PI/180.0)*global.width;
            addEllipse(global->center.x, global->center.y, global.width, global.height, global.rot, false);
            appendPromptHistory();
            actuator("end");
        }
    }
}
*/

/**
 * .
 */
void
ellipse_context(String args)
{
    /*
    todo("ELLIPSE", "context()");
    */
}

/**
 * .
 */
 /*
void
ellipse_prompt(String args)
{
    if (global->mode == MODE_MAJORDIAMETER_MINORRADIUS) {
        if (std::isnan(global.x1)) {
            if (str == "C" || str == "CENTER") {
                global.mode = MODE_MAJORRADIUS_MINORRADIUS;
                setPromptPrefix(tr("Specify center point: "));
            }
            else {
                EmbReal strList = str.split(",");
                if (std::isnan(strList[0]) || std::isnan(strList[1])) {
                    alert(tr("Point or option keyword required."));
                    setPromptPrefix(tr("Specify first axis start point or [Center]: "));
                }
                else {
                    global.x1 = Number(strList[0]);
                    global.y1 = Number(strList[1]);
                    addRubber("ELLIPSE");
                    setRubberMode("ELLIPSE_LINE");
                    setRubberPoint("ELLIPSE_LINE_POINT1", global.x1, global.y1);
                    setPromptPrefix(tr("Specify first axis end point: "));
                }
            }
        }
        else if (std::isnan(global.x2)) {
            EmbReal strList = str.split(",");
            if (std::isnan(strList[0]) || std::isnan(strList[1])) {
                alert(tr("Invalid point."));
                setPromptPrefix(tr("Specify first axis end point: "));
            }
            else {
                global.x2 = Number(strList[0]);
                global.y2 = Number(strList[1]);
                global->center.x = (global.x1 + global.x2)/2.0;
                global->center.y = (global.y1 + global.y2)/2.0;
                global.width = calculateDistance(global.x1, global.y1, global.x2, global.y2);
                global.rot = calculateAngle(global.x1, global.y1, global.x2, global.y2);
                setRubberMode("ELLIPSE_MAJORDIAMETER_MINORRADIUS");
                setRubberPoint("ELLIPSE_AXIS1_POINT1", global.x1, global.y1);
                setRubberPoint("ELLIPSE_AXIS1_POINT2", global.x2, global.y2);
                setRubberPoint("ELLIPSE_CENTER", global->center.x, global->center.y);
                setRubberPoint("ELLIPSE_WIDTH", global.width, 0);
                setRubberPoint("ELLIPSE_ROT", global.rot, 0);
                setPromptPrefix(tr("Specify second axis end point or [Rotation]: "));
            }
        }
        else if (std::isnan(global.x3)) {
            if (str == "R" || str == "ROTATION") {
                global.mode = MODE_ELLIPSE_ROTATION;
                setPromptPrefix(tr("Specify rotation: "));
            }
            else {
                EmbReal strList = str.split(",");
                if (std::isnan(strList[0]) || std::isnan(strList[1])) {
                    alert(tr("Point or option keyword required."));
                    setPromptPrefix(tr("Specify second axis end point or [Rotation]: "));
                }
                else {
                    global.x3 = Number(strList[0]);
                    global.y3 = Number(strList[1]);
                    global.height = perpendicularDistance(global.x3, global.y3, global.x1, global.y1, global.x2, global.y2)*2.0;
                    setRubberPoint("ELLIPSE_AXIS2_POINT2", global.x3, global.y3);
                    actuator("vulcanize");
                    actuator("end");
                }
            }
        }
    }
    else if (global->mode == MODE_MAJORRADIUS_MINORRADIUS) {
        if (std::isnan(global.x1)) {
            EmbReal strList = str.split(",");
            if (std::isnan(strList[0]) || std::isnan(strList[1])) {
                alert(tr("Invalid point."));
                setPromptPrefix(tr("Specify center point: "));
            }
            else {
                global.x1 = Number(strList[0]);
                global.y1 = Number(strList[1]);
                global->center.x = global.x1;
                global->center.y = global.y1;
                addRubber("ELLIPSE");
                setRubberMode("ELLIPSE_LINE");
                setRubberPoint("ELLIPSE_LINE_POINT1", global.x1, global.y1);
                setRubberPoint("ELLIPSE_CENTER", global->center.x, global->center.y);
                setPromptPrefix(tr("Specify first axis end point: "));
            }
        }
        else if (std::isnan(global.x2)) {
            EmbReal strList = str.split(",");
            if (std::isnan(strList[0]) || std::isnan(strList[1])) {
                alert(tr("Invalid point."));
                setPromptPrefix(tr("Specify first axis end point: "));
            }
            else {
                global.x2 = Number(strList[0]);
                global.y2 = Number(strList[1]);
                global.width = calculateDistance(global.x1, global.y1, global.x2, global.y2)*2.0;
                global.rot = calculateAngle(global.x1, global.y1, global.x2, global.y2);
                setRubberMode("ELLIPSE_MAJORRADIUS_MINORRADIUS");
                setRubberPoint("ELLIPSE_AXIS1_POINT2", global.x2, global.y2);
                setRubberPoint("ELLIPSE_WIDTH", global.width, 0);
                setRubberPoint("ELLIPSE_ROT", global.rot, 0);
                setPromptPrefix(tr("Specify second axis end point or [Rotation]: "));
            }
        }
        else if (std::isnan(global.x3)) {
            if (str == "R" || str == "ROTATION") {
                global.mode = MODE_ELLIPSE_ROTATION;
                setPromptPrefix(tr("Specify ellipse rotation: "));
            }
            else {
                EmbReal strList = str.split(",");
                if (std::isnan(strList[0]) || std::isnan(strList[1])) {
                    alert(tr("Point or option keyword required."));
                    setPromptPrefix(tr("Specify second axis end point or [Rotation]: "));
                }
                else {
                    global.x3 = Number(strList[0]);
                    global.y3 = Number(strList[1]);
                    global.height = perpendicularDistance(global.x3, global.y3, global.x1, global.y1, global.x2, global.y2)*2.0;
                    setRubberPoint("ELLIPSE_AXIS2_POINT2", global.x3, global.y3);
                    actuator("vulcanize");
                    actuator("end");
                }
            }
        }
    }
    else if (global->mode == MODE_ELLIPSE_ROTATION) {
        if (std::isnan(global.x1)) {
            error("ELLIPSE", tr("This should never happen."));
        }
        else if (std::isnan(global.x2)) {
            error("ELLIPSE", tr("This should never happen."));
        }
        else if (std::isnan(global.x3)) {
            if (std::isnan(str)) {
                alert(tr("Invalid angle. Input a numeric angle or pick a point."));
                setPromptPrefix(tr("Specify rotation: "));
            }
            else {
                EmbReal angle = Number(str);
                global.height = cos(angle*PI/180.0)*global.width;
                addEllipse(global->center.x, global->center.y, global.width, global.height, global.rot, false);
                actuator("end");
            }
        }
    }
}

void
erase_main()
{
    initCommand();

    if (numSelected() <= 0) {
        //TODO: Prompt to select objects if nothing is preselected
        alert(tr("Preselect objects before invoking the delete command."));
        actuator("end");
        messageBox("information", tr("Delete Preselect"), tr("Preselect objects before invoking the delete command."));
    }
    else {
        deleteSelected();
        actuator("end");
    }
}

//Command: Heart

var global = {}; //Required
global.numPoints = 512; //Default //TODO: min:64 max:8192
global->center.x;
global->center.y;
global.sx = 1.0;
global.sy = 1.0;
global.numPoints;
global.mode;

void
heart_main()
{
    initCommand();
    clearSelection();
    global->center.x = NaN;
    global->center.y = NaN;
    global.mode = MODE_NUM_POINTS;

    //Heart4: 10.0 / 512
    //Heart5: 1.0 / 512

    addRubber("POLYGON");
    setRubberMode("POLYGON");
    updateHeart("HEART5", global.numPoints, global.sx, global.sy);
    spareRubber("POLYGON");
    actuator("end");
}
*/

/**
 * .
 */
 /*
void
updateHeart(style, numPoints, xScale, yScale)
{
    for (int i = 0; i <= numPoints; i++) {
        EmbReal xx, yy;
        EmbReal t = (2.0*emb_constant_pi)/numPoints*i;

        if (style == "HEART4") {
            xx = cos(t)*((sin(t)*sqrt(abs(cos(t))))/(sin(t)+7/5) - 2*sin(t) + 2);
            yy = sin(t)*((sin(t)*sqrt(abs(cos(t))))/(sin(t)+7/5) - 2*sin(t) + 2);
        }
        else if (style == "HEART5") {
            xx = 16*pow(sin(t), 3);
            yy = 13*cos(t) - 5*cos(2*t) - 2*cos(3*t) - cos(4*t);
        }

        setRubberPoint("POLYGON_POINT_" + i.toString(), xx*xScale, yy*yScale);
    }

    setRubberText("POLYGON_NUM_POINTS", numPoints.toString());
}

void
line_main()
{
    var global = {}; //Required
    initCommand();
    clearSelection();
    global.firstRun = true;
    global.first = {NaN, NaN};
    global.prev = {NaN, NaN};
    setPromptPrefix(tr("Specify first point: "));
}


void
line_click(x, y)
{
    if (global.firstRun) {
        global.firstRun = false;
        global.firstX = x;
        global.firstY = y;
        global.prevX = x;
        global.prevY = y;
        addRubber("LINE");
        setRubberMode("LINE");
        setRubberPoint("LINE_START", global.firstX, global.firstY);
        appendPromptHistory();
        setPromptPrefix(tr("Specify next point or [Undo]: "));
    }
    else {
        setRubberPoint("LINE_END", x, y);
        actuator("vulcanize");
        addRubber("LINE");
        setRubberMode("LINE");
        setRubberPoint("LINE_START", x, y);
        appendPromptHistory();
        global.prevX = x;
        global.prevY = y;
    }
}

void
line_context(String str)
{
    todo("LINE", "context()");
}

void
line_prompt(String args)
{
    if (global.firstRun) {
        EmbReal strList = str.split(",");
        if (std::isnan(strList[0]) || std::isnan(strList[1])) {
            alert(tr("Invalid point."));
            setPromptPrefix(tr("Specify first point: "));
        }
        else {
            global.firstRun = false;
            global.firstX = Number(strList[0]);
            global.firstY = Number(strList[1]);
            global.prevX = global.firstX;
            global.prevY = global.firstY;
            addRubber("LINE");
            setRubberMode("LINE");
            setRubberPoint("LINE_START", global.firstX, global.firstY);
            setPromptPrefix(tr("Specify next point or [Undo]: "));
        }
    }
    else {
        if (str == "U" || str == "UNDO") {
            todo("LINE", "prompt() for UNDO");
        }
        else {
            EmbReal strList = str.split(",");
            if (std::isnan(strList[0]) || std::isnan(strList[1])) {
                alert(tr("Point or option keyword required."));
                setPromptPrefix(tr("Specify next point or [Undo]: "));
            }
            else {
                EmbReal x = Number(strList[0]);
                EmbReal y = Number(strList[1]);
                setRubberPoint("LINE_END", x, y);
                actuator("vulcanize");
                addRubber("LINE");
                setRubberMode("LINE");
                setRubberPoint("LINE_START", x, y);
                global.prevX = x;
                global.prevY = y;
                setPromptPrefix(tr("Specify next point or [Undo]: "));
            }
        }
    }
}

//Command: Locate Point

void
locate_point_main()
{
    initCommand();
    clearSelection();
    setPromptPrefix(tr("Specify point: "));
}


void
locate_point_click(x, y)
{
    appendPromptHistory();
    setPromptPrefix("X = " + x.s + ", Y = " + y.s);
    appendPromptHistory();
    actuator("end");
}

void
locate_point_context(String str)
{
    todo("LOCATEPOINT", "context()");
}

void
locate_point_prompt(String args)
{
    EmbReal strList = str.split(",");
    if (std::isnan(strList[0]) || std::isnan(strList[1])) {
        alert(tr("Invalid point."));
        setPromptPrefix(tr("Specify point: "));
    }
    else {
        appendPromptHistory();
        setPromptPrefix("X = " + strList[0].s + ", Y = " + strList[1].toString());
        appendPromptHistory();
        actuator("end");
    }
}

//Command: Move

var global = {}; //Required
global.firstRun;
global.baseX;
global.baseY;
global.destX;
global.destY;
global.deltaX;
global.deltaY;
*/

/**
 * .
 */
 /*
void
move_main()
{
    initCommand();
    global.firstRun = true;
    global.baseX  = NaN;
    global.baseY  = NaN;
    global.destX  = NaN;
    global.destY  = NaN;
    global.deltaX = NaN;
    global.deltaY = NaN;

    if (numSelected() <= 0) {
        //TODO: Prompt to select objects if nothing is preselected
        alert(tr("Preselect objects before invoking the move command."));
        actuator("end");
        messageBox("information", tr("Move Preselect"), tr("Preselect objects before invoking the move command."));
    }
    else {
        setPromptPrefix(tr("Specify base point: "));
    }
}
*/

/**
 * .
 */
 /*
void
move_click(x, y)
{
    if (global.firstRun) {
        global.firstRun = false;
        global.baseX = x;
        global.baseY = y;
        addRubber("LINE");
        setRubberMode("LINE");
        setRubberPoint("LINE_START", global.baseX, global.baseY);
        previewOn("SELECTED", "MOVE", global.baseX, global.baseY, 0);
        appendPromptHistory();
        setPromptPrefix(tr("Specify destination point: "));
    }
    else {
        global.destX = x;
        global.destY = y;
        global.deltaX = global.destX - global.baseX;
        global.deltaY = global.destY - global.baseY;
        moveSelected(global.deltaX, global.deltaY);
        previewOff();
        actuator("end");
    }
}
*/

/**
 * .
 */
void
move_context(String str)
{
    // todo("MOVE", "context()");
}

/**
 * .
 */
void
move_prompt(String str)
{
    /*
    if (global.firstRun) {
        EmbReal strList = str.split(",");
        if (std::isnan(strList[0]) || std::isnan(strList[1])) {
            alert(tr("Invalid point."));
            setPromptPrefix(tr("Specify base point: "));
        }
        else {
            global.firstRun = false;
            global.baseX = Number(strList[0]);
            global.baseY = Number(strList[1]);
            addRubber("LINE");
            setRubberMode("LINE");
            setRubberPoint("LINE_START", global.baseX, global.baseY);
            previewOn("SELECTED", "MOVE", global.baseX, global.baseY, 0);
            setPromptPrefix(tr("Specify destination point: "));
        }
    }
    else {
        EmbReal strList = str.split(",");
        if (std::isnan(strList[0]) || std::isnan(strList[1])) {
            alert(tr("Invalid point."));
            setPromptPrefix(tr("Specify destination point: "));
        }
        else {
            global.destX = Number(strList[0]);
            global.destY = Number(strList[1]);
            global.deltaX = global.destX - global.baseX;
            global.deltaY = global.destY - global.baseY;
            moveSelected(global.deltaX, global.deltaY);
            previewOff();
            actuator("end");
        }
    }
    */
}

//Command: Path

/*
//TODO: The path command is currently broken

var global = {}; //Required
global.firstRun;
global.firstX;
global.firstY;
global.prevX;
global.prevY;

void
path_main()
{
    initCommand();
    clearSelection();
    global.firstRun = true;
    global.firstX = NaN;
    global.firstY = NaN;
    global.prevX = NaN;
    global.prevY = NaN;
    setPromptPrefix(tr("Specify start point: "));
}


void
path_click(x, y)
{
    if (global.firstRun) {
        global.firstRun = false;
        global.firstX = x;
        global.firstY = y;
        global.prevX = x;
        global.prevY = y;
        addPath(x,y);
        appendPromptHistory();
        setPromptPrefix(tr("Specify next point or [Arc/Undo]: "));
    }
    else {
        appendPromptHistory();
        appendLineToPath(x,y);
        global.prevX = x;
        global.prevY = y;
    }
}

void
path_context(String str)
{
    todo("PATH", "context()");
}

void
path_prompt(String args)
{
    if (str == "A" || str == "ARC") {
        todo("PATH", "prompt() for ARC");
    }
    else if (str == "U" || str == "UNDO") {
        todo("PATH", "prompt() for UNDO");
    }
    else {
        EmbReal strList = str.split(",");
        if (std::isnan(strList[0]) || std::isnan(strList[1])) {
            alert(tr("Point or option keyword required."));
            setPromptPrefix(tr("Specify next point or [Arc/Undo]: "));
        }
        else {
            EmbReal x = Number(strList[0]);
            EmbReal y = Number(strList[1]);
            if (global.firstRun) {
                global.firstRun = false;
                global.firstX = x;
                global.firstY = y;
                global.prevX = x;
                global.prevY = y;
                addPath(x,y);
                setPromptPrefix(tr("Specify next point or [Arc/Undo]: "));
            }
            else {
                appendLineToPath(x,y);
                global.prevX = x;
                global.prevY = y;
            }
        }
    }
}

UiObject
point_main()
{
    UiObject global;
    initCommand();
    clearSelection();
    global.firstRun = true;
    setPromptPrefix("TODO: Current point settings: PDMODE=?  PDSIZE=?"); //TODO: tr needed here when complete
    appendPromptHistory();
    setPromptPrefix(tr("Specify first point: "));
    return global;
}


void
point_click(UiObject *global, EmbVector v)
{
    if (global.firstRun) {
        global.firstRun = false;
        appendPromptHistory();
        setPromptPrefix(tr("Specify next point: "));
        addPoint(x,y);
    }
    else {
        appendPromptHistory();
        addPoint(x,y);
    }
}

void
point_context(String str)
{
    todo("POINT", "context()");
}

void
point_prompt(String str)
{
    if (global.firstRun) {
        if (str == "M" || str == "MODE") {
            todo("POINT", "prompt() for PDMODE");
        }
        else if (str == "S" || str == "SIZE") {
            todo("POINT", "prompt() for PDSIZE");
        }
        EmbReal strList = str.split(",");
        if (std::isnan(strList[0]) || std::isnan(strList[1])) {
            alert(tr("Invalid point."));
            setPromptPrefix(tr("Specify first point: "));
        }
        else {
            global.firstRun = false;
            EmbReal x = Number(strList[0]);
            EmbReal y = Number(strList[1]);
            setPromptPrefix(tr("Specify next point: "));
            addPoint(x,y);
        }
    }
    else {
        EmbReal strList = str.split(",");
        if (std::isnan(strList[0]) || std::isnan(strList[1])) {
            alert(tr("Invalid point."));
            setPromptPrefix(tr("Specify next point: "));
        }
        else {
            EmbReal x = Number(strList[0]);
            EmbReal y = Number(strList[1]);
            setPromptPrefix(tr("Specify next point: "));
            addPoint(x,y);
        }
    }
}

//Command: Polygon

var global = {}; //Required
global->center;
global.sideX1;
global.sideY1;
global.sideX2;
global.sideY2;
global.pointIX;
global.pointIY;
global.pointCX;
global.pointCY;
global.polyType = "Inscribed"; //Default
global.numSides = 4;           //Default
global.mode;
*/

/**
 * .
 */
 /*
void
polygon_main()
{
    initCommand();
    clearSelection();
    global->centerX = NaN;
    global->centerY = NaN;
    global.sideX1  = NaN;
    global.sideY1  = NaN;
    global.sideX2  = NaN;
    global.sideY2  = NaN;
    global.pointIX = NaN;
    global.pointIY = NaN;
    global.pointCX = NaN;
    global.pointCY = NaN;
    global.mode = MODE_NUM_SIDES;
    setPromptPrefix(tr("Enter number of sides") + " {" + global.numSides.toString() + "}: ");
}
*/

/**
 * .
 */
void
polygon_click(UiObject *global, EmbVector v)
{
    /*
    if (global->mode == MODE_NUM_SIDES) {
        //Do nothing, the prompt controls this.
    }
    else if (global->mode == MODE_CENTER_PT) {
        global->centerX = x;
        global->centerY = y;
        global.mode = MODE_POLYTYPE;
        appendPromptHistory();
        setPromptPrefix(tr("Specify polygon type [Inscribed in circle/Circumscribed around circle]") + " {" + global.polyType + "}: ");
    }
    else if (global->mode == MODE_POLYTYPE) {
        //Do nothing, the prompt controls this.
    }
    else if (global->mode == MODE_INSCRIBE) {
        global.pointIX = x;
        global.pointIY = y;
        setRubberPoint("POLYGON_INSCRIBE_POINT", global.pointIX, global.pointIY);
        actuator("vulcanize");
        appendPromptHistory();
        actuator("end");
    }
    else if (global->mode == MODE_CIRCUMSCRIBE) {
        global.pointCX = x;
        global.pointCY = y;
        setRubberPoint("POLYGON_CIRCUMSCRIBE_POINT", global.pointCX, global.pointCY);
        actuator("vulcanize");
        appendPromptHistory();
        actuator("end");
    }
    else if (global->mode == MODE_DISTANCE) {
        //Do nothing, the prompt controls this.
    }
    else if (global->mode == MODE_SIDE_LEN) {
        todo("POLYGON", "Sidelength mode");
    }
    */
}

/**
 * .
 */
void
polygon_context(String str)
{
    //todo("POLYGON", "context()");
}

/**
 * .
 */
void
polygon_prompt(String str)
{
    /*
    if (global->mode == MODE_NUM_SIDES) {
        if (str == "" && global.numSides >= 3 && global.numSides <= 1024) {
            setPromptPrefix(tr("Specify center point or [Sidelength]: "));
            global.mode = MODE_CENTER_PT;
        }
        else {
            EmbReal tmp = Number(str);
            if (std::isnan(tmp) || !isInt(tmp) || tmp < 3 || tmp > 1024) {
                alert(tr("Requires an integer between 3 and 1024."));
                setPromptPrefix(tr("Enter number of sides") + " {" + global.numSides.toString() + "}: ");
            }
            else {
                global.numSides = tmp;
                setPromptPrefix(tr("Specify center point or [Sidelength]: "));
                global.mode = MODE_CENTER_PT;
            }
        }
    }
    else if (global->mode == MODE_CENTER_PT) {
        if (str == "S" || str == "SIDELENGTH") {
            global.mode = MODE_SIDE_LEN;
            setPromptPrefix(tr("Specify start point: "));
        }
        else {
            EmbReal strList = str.split(",");
            if (std::isnan(strList[0]) || std::isnan(strList[1])) {
                alert(tr("Point or option keyword required."));
                setPromptPrefix(tr("Specify center point or [Sidelength]: "));
            }
            else {
                global->centerX = Number(strList[0]);
                global->centerY = Number(strList[1]);
                global.mode = MODE_POLYTYPE;
                setPromptPrefix(tr("Specify polygon type [Inscribed in circle/Circumscribed around circle]") + " {" + global.polyType + "}: ");
            }
        }
    }
    else if (global->mode == MODE_POLYTYPE) {
        if (str == "INSCRIBED") {
            global.mode = MODE_INSCRIBE;
            global.polyType = "Inscribed";
            setPromptPrefix(tr("Specify polygon corner point or [Distance]: "));
            addRubber("POLYGON");
            setRubberMode("POLYGON_INSCRIBE");
            setRubberPoint("POLYGON_CENTER", global->centerX, global->centerY);
            setRubberPoint("POLYGON_NUM_SIDES", global.numSides, 0);
        }
        else if (str == "CIRCUMSCRIBED") {
            global.mode = MODE_CIRCUMSCRIBE;
            global.polyType = "Circumscribed";
            setPromptPrefix(tr("Specify polygon side point or [Distance]: "));
            addRubber("POLYGON");
            setRubberMode("POLYGON_CIRCUMSCRIBE");
            setRubberPoint("POLYGON_CENTER", global->centerX, global->centerY);
            setRubberPoint("POLYGON_NUM_SIDES", global.numSides, 0);
        }
        else if (str == "") {
            if (global.polyType == "Inscribed") {
                global.mode = MODE_INSCRIBE;
                setPromptPrefix(tr("Specify polygon corner point or [Distance]: "));
                addRubber("POLYGON");
                setRubberMode("POLYGON_INSCRIBE");
                setRubberPoint("POLYGON_CENTER", global->centerX, global->centerY);
                setRubberPoint("POLYGON_NUM_SIDES", global.numSides, 0);
            }
            else if (global.polyType == "Circumscribed") {
                global.mode = MODE_CIRCUMSCRIBE;
                setPromptPrefix(tr("Specify polygon side point or [Distance]: "));
                addRubber("POLYGON");
                setRubberMode("POLYGON_CIRCUMSCRIBE");
                setRubberPoint("POLYGON_CENTER", global->centerX, global->centerY);
                setRubberPoint("POLYGON_NUM_SIDES", global.numSides, 0);
            }
            else {
                error("POLYGON", tr("Polygon type is not Inscribed or Circumscribed."));
            }
        }
        else {
            alert(tr("Invalid option keyword."));
            setPromptPrefix(tr("Specify polygon type [Inscribed in circle/Circumscribed around circle]") + " {" + global.polyType + "}: ");
        }
    }
    else if (global->mode == MODE_INSCRIBE) {
        if (str == "D" || str == "DISTANCE") {
            global.mode = MODE_DISTANCE;
            setPromptPrefix(tr("Specify distance: "));
        }
        else {
            EmbReal strList = str.split(",");
            if (std::isnan(strList[0]) || std::isnan(strList[1])) {
                alert(tr("Point or option keyword required."));
                setPromptPrefix(tr("Specify polygon corner point or [Distance]: "));
            }
            else {
                global.pointIX = Number(strList[0]);
                global.pointIY = Number(strList[1]);
                setRubberPoint("POLYGON_INSCRIBE_POINT", global.pointIX, global.pointIY);
                actuator("vulcanize");
                actuator("end");
            }
        }
    }
    else if (global->mode == MODE_CIRCUMSCRIBE) {
        if (str == "D" || str == "DISTANCE") {
            global.mode = MODE_DISTANCE;
            setPromptPrefix(tr("Specify distance: "));
        }
        else {
            EmbReal strList = str.split(",");
            if (std::isnan(strList[0]) || std::isnan(strList[1])) {
                alert(tr("Point or option keyword required."));
                setPromptPrefix(tr("Specify polygon side point or [Distance]: "));
            }
            else {
                global.pointCX = Number(strList[0]);
                global.pointCY = Number(strList[1]);
                setRubberPoint("POLYGON_CIRCUMSCRIBE_POINT", global.pointCX, global.pointCY);
                actuator("vulcanize");
                actuator("end");
            }
        }
    }
    else if (global->mode == MODE_DISTANCE) {
        if (std::isnan(str)) {
            alert(tr("Requires valid numeric distance."));
            setPromptPrefix(tr("Specify distance: "));
        }
        else {
            if (global.polyType == "Inscribed") {
                global.pointIX = global->centerX;
                global.pointIY = global->centerY + Number(str);
                setRubberPoint("POLYGON_INSCRIBE_POINT", global.pointIX, global.pointIY);
                actuator("vulcanize");
                actuator("end");
            }
            else if (global.polyType == "Circumscribed") {
                global.pointCX = global->centerX;
                global.pointCY = global->centerY + Number(str);
                setRubberPoint("POLYGON_CIRCUMSCRIBE_POINT", global.pointCX, global.pointCY);
                actuator("vulcanize");
                actuator("end");
            }
            else {
                error("POLYGON", tr("Polygon type is not Inscribed or Circumscribed."));
            }
        }
    }
    else if (global->mode == MODE_SIDE_LEN) {
        todo("POLYGON", "Sidelength mode");
    }
    */
}

//Command: Polyline
/*
var global = {}; //Required
global.firstRun;
global.firstX;
global.firstY;
global.prevX;
global.prevY;
global.num;
*/

/**
 * .
 */
void
polyline_main()
{
    /*
    initCommand();
    clearSelection();
    global.firstRun = true;
    global.firstX = NaN;
    global.firstY = NaN;
    global.prevX = NaN;
    global.prevY = NaN;
    global.num = 0;
    setPromptPrefix(tr("Specify first point: "));
    */
}

/**
 * .
 */
void
polyline_click(UiObject *global, EmbVector v)
{
    /*
    if (global.firstRun) {
        global.firstRun = false;
        global.firstX = x;
        global.firstY = y;
        global.prevX = x;
        global.prevY = y;
        addRubber("POLYLINE");
        setRubberMode("POLYLINE");
        setRubberPoint("POLYLINE_POINT_0", global.firstX, global.firstY);
        appendPromptHistory();
        setPromptPrefix(tr("Specify next point or [Undo]: "));
    }
    else {
        global.num++;
        setRubberPoint("POLYLINE_POINT_" + global.num.toString(), x, y);
        setRubberText("POLYLINE_NUM_POINTS", global.num.toString());
        spareRubber("POLYLINE");
        appendPromptHistory();
        global.prevX = x;
        global.prevY = y;
    }
    */
}

/**
 * .
 */
void
polyline_context(String str)
{
    //todo("POLYLINE", "context()");
}

/**
 * .
 */
void
polyline_prompt(String str)
{
    /*
    if (global.firstRun) {
        EmbReal strList = str.split(",");
        if (std::isnan(strList[0]) || std::isnan(strList[1])) {
            alert(tr("Invalid point."));
            setPromptPrefix(tr("Specify first point: "));
        }
        else {
            global.firstRun = false;
            global.firstX = Number(strList[0]);
            global.firstY = Number(strList[1]);
            global.prevX = global.firstX;
            global.prevY = global.firstY;
            addRubber("POLYLINE");
            setRubberMode("POLYLINE");
            setRubberPoint("POLYLINE_POINT_0", global.firstX, global.firstY);
            setPromptPrefix(tr("Specify next point or [Undo]: "));
        }
    }
    else {
        if (str == "U" || str == "UNDO") {
            todo("POLYLINE", "prompt() for UNDO");
        }
        else {
            EmbReal strList = str.split(",");
            if (std::isnan(strList[0]) || std::isnan(strList[1])) {
                alert(tr("Point or option keyword required."));
                setPromptPrefix(tr("Specify next point or [Undo]: "));
            }
            else {
                EmbReal x = Number(strList[0]);
                EmbReal y = Number(strList[1]);
                global.num++;
                setRubberPoint("POLYLINE_POINT_" + global.num.toString(), x, y);
                setRubberText("POLYLINE_NUM_POINTS", global.num.toString());
                spareRubber("POLYLINE");
                global.prevX = x;
                global.prevY = y;
                setPromptPrefix(tr("Specify next point or [Undo]: "));
            }
        }
    }
    */
}

//Command: QuickLeader

/*
var global = {}; //Required
global.x1;
global.y1;
global.x2;
global.y2;
*/

//TODO: Adding the text is not complete yet.

/**
 * .
 */
void
quickleader_main()
{
    /*
    initCommand();
    clearSelection();
    global.x1 = NaN;
    global.y1 = NaN;
    global.x2 = NaN;
    global.y2 = NaN;
    setPromptPrefix(tr("Specify first point: "));
    */
}

/**
 * .
 */
void
quickleader_click(UiObject *global, EmbVector v)
{
    /*
    if (std::isnan(global.x1)) {
        global.x1 = x;
        global.y1 = y;
        addRubber("DIMLEADER");
        setRubberMode("DIMLEADER_LINE");
        setRubberPoint("DIMLEADER_LINE_START", global.x1, global.y1);
        appendPromptHistory();
        setPromptPrefix(tr("Specify second point: "));
    }
    else {
        global.x2 = x;
        global.y2 = y;
        setRubberPoint("DIMLEADER_LINE_END", global.x2, global.y2);
        actuator("vulcanize");
        appendPromptHistory();
        actuator("end");
    }
    */
}

/**
 * .
 */
void
quickleader_context(String str)
{
    //todo("QUICKLEADER", "context()");
}

/**
 * .
 */
void
quickleader_prompt(String str)
{
    /*
    EmbReal strList = str.split(",");
    if (std::isnan(global.x1)) {
        if (std::isnan(strList[0]) || std::isnan(strList[1])) {
            alert(tr("Requires two points."));
            setPromptPrefix(tr("Specify first point: "));
        }
        else {
            global.x1 = Number(strList[0]);
            global.y1 = Number(strList[1]);
            addRubber("DIMLEADER");
            setRubberMode("DIMLEADER_LINE");
            setRubberPoint("DIMLEADER_LINE_START", global.x1, global.y1);
            setPromptPrefix(tr("Specify second point: "));
        }
    }
    else {
        if (std::isnan(strList[0]) || std::isnan(strList[1])) {
            alert(tr("Requires two points."));
            setPromptPrefix(tr("Specify second point: "));
        }
        else {
            global.x2 = Number(strList[0]);
            global.y2 = Number(strList[1]);
            setRubberPoint("DIMLEADER_LINE_END", global.x2, global.y2);
            actuator("vulcanize");
            actuator("end");
        }
    }
    */
}

//Command: Rectangle

/*
var global = {}; //Required
global.newRect;
global.x1;
global.y1;
global.x2;
global.y2;
*/

/**
 * .
 */
void
rectangle_main()
{
    /*
    initCommand();
    clearSelection();
    global.newRect = true;
    global.x1 = NaN;
    global.y1 = NaN;
    global.x2 = NaN;
    global.y2 = NaN;
    setPromptPrefix(tr("Specify first corner point or [Chamfer/Fillet]: "));
    */
}

/**
 * .
 */
void
rectangle_click(UiObject *global, EmbVector v)
{
    /*
    if (global.newRect) {
        global.newRect = false;
        global.x1 = x;
        global.y1 = y;
        addRubber("RECTANGLE");
        setRubberMode("RECTANGLE");
        setRubberPoint("RECTANGLE_START", x, y);
        setPromptPrefix(tr("Specify other corner point or [Dimensions]: "));
    }
    else {
        global.newRect = true;
        global.x2 = x;
        global.y2 = y;
        setRubberPoint("RECTANGLE_END", x, y);
        actuator("vulcanize");
        actuator("end");
    }
    */
}

/**
 * .
 */
void
rectangle_context(String str)
{
    //todo("RECTANGLE", "context()");
}

/**
 * .
 */
void
rectangle_prompt(String str)
{
    /*
    if (str == "C" || str == "CHAMFER") {
        todo("RECTANGLE", "prompt() for CHAMFER");
    }
    else if (str == "D" || str == "DIMENSIONS") {
        todo("RECTANGLE", "prompt() for DIMENSIONS");
    }
    else if (str == "F" || str == "FILLET") {
        todo("RECTANGLE", "prompt() for FILLET");
    }
    else {
        EmbReal strList = str.split(",");
        if (std::isnan(strList[0]) || std::isnan(strList[1])) {
            alert(tr("Invalid point."));
            setPromptPrefix(tr("Specify first point: "));
        }
        else {
            EmbReal x = Number(strList[0]);
            EmbReal y = Number(strList[1]);
            if (global.newRect) {
                global.newRect = false;
                global.x1 = x;
                global.y1 = y;
                addRubber("RECTANGLE");
                setRubberMode("RECTANGLE");
                setRubberPoint("RECTANGLE_START", x, y);
                setPromptPrefix(tr("Specify other corner point or [Dimensions]: "));
            }
            else {
                global.newRect = true;
                global.x2 = x;
                global.y2 = y;
                setRubberPoint("RECTANGLE_END", x, y);
                actuator("vulcanize");
                actuator("end");
            }
        }
    }
    */
}

//Command: RGB

//var global = {}; //Required
//global.mode;

/**
 * .
 */
void
rgb_main()
{
    /*
    initCommand();
    clearSelection();
    global.mode = RGB_MODE_BACKGROUND;
    setPromptPrefix(tr("Enter RED,GREEN,BLUE values for background or [Crosshair/Grid]: "));
    */
}

/**
 * .
 */
void
rgb_click(UiObject *global, EmbVector v)
{
    //Do Nothing, prompt only command.
}

/**
 * .
 */
void
rgb_context(String str)
{
    //todo("RGB", "context()");
}

/**
 * .
 */
void
rgb_prompt(String str)
{
    /*
    if (global->mode == RGB_MODE_BACKGROUND) {
        if (str == "C" || str == "CROSSHAIR") {
            global.mode = RGB_MODE_CROSSHAIR;
            setPromptPrefix(tr("Specify crosshair color: "));
        }
        else if (str == "G" || str == "GRID") {
            global.mode = RGB_MODE_GRID;
            setPromptPrefix(tr("Specify grid color: "));
        }
        else {
            EmbReal strList = str.split(",");
            EmbReal r = Number(strList[0]);
            EmbReal g = Number(strList[1]);
            EmbReal b = Number(strList[2]);
            if (!validRGB(r,g,b)) {
                alert(tr("Invalid color. R,G,B values must be in the range of 0-255."));
                setPromptPrefix(tr("Specify background color: "));
            }
            else {
                setBackgroundColor(r,g,b);
                actuator("end");
            }
        }
    }
    else if (global->mode == RGB_MODE_CROSSHAIR) {
        EmbReal strList = str.split(",");
        EmbReal r = Number(strList[0]);
        EmbReal g = Number(strList[1]);
        EmbReal b = Number(strList[2]);
        if (!validRGB(r,g,b)) {
            alert(tr("Invalid color. R,G,B values must be in the range of 0-255."));
            setPromptPrefix(tr("Specify crosshair color: "));
        }
        else {
            setCrossHairColor(r,g,b);
            actuator("end");
        }
    }
    else if (global->mode == RGB_MODE_GRID) {
        EmbReal strList = str.split(",");
        EmbReal r = Number(strList[0]);
        EmbReal g = Number(strList[1]);
        EmbReal b = Number(strList[2]);
        if (!validRGB(r,g,b)) {
            alert(tr("Invalid color. R,G,B values must be in the range of 0-255."));
            setPromptPrefix(tr("Specify grid color: "));
        }
        else {
            setGridColor(r,g,b);
            actuator("end");
        }
    }
    */
}

/**
 * .
 * Command: Rotate
 *
 * var global = {}; //Required
 * bool firstRun;
 * EmbVector base;
 * EmbVector dest;
 * EmbReal angle;
 *
 * EmbVector baseR;
 * EmbVector destR;
 * EmbReal angleRef;
 * EmbReal angleNew;
 *
 * int mode;
 */
String
rotate_main(String args)
{
    /*
    initCommand();
    global.mode = ROTATE_MODE_NORMAL;
    global.firstRun = true;
    global.base = {NaN, NaN};
    global.dest = {NaN, NaN};
    global.angle = NaN;

    global.baseR = {NaN, NaN};
    global.destR = {NaN, NaN};
    global.angleRef = NaN;
    global.angleNew = NaN;

    if (numSelected() <= 0) {
        //TODO: Prompt to select objects if nothing is preselected
        alert(tr("Preselect objects before invoking the rotate command."));
        actuator("end");
        messageBox("information", tr("Rotate Preselect"), tr("Preselect objects before invoking the rotate command."));
    }
    else {
        setPromptPrefix(tr("Specify base point: "));
    }
    */
    return "";
}

/**
 * .
 */
void
rotate_click(UiObject *global, EmbVector v)
{
    /*
    if (global->mode == ROTATE_MODE_NORMAL) {
        if (global.firstRun) {
            global.firstRun = false;
            global.base = v;
            addRubber("LINE");
            setRubberMode("LINE");
            setRubberPoint("LINE_START", global.baseX, global.baseY);
            previewOn("SELECTED", "ROTATE", global.baseX, global.baseY, 0);
            appendPromptHistory();
            setPromptPrefix(tr("Specify rotation angle or [Reference]: "));
        }
        else {
            global.dest = v;
            global.angle = calculateAngle(global.baseX, global.baseY, global.destX, global.destY);
            appendPromptHistory();
            rotateSelected(global.baseX, global.baseY, global.angle);
            previewOff();
            actuator("end");
        }
    }
    else if (global->mode == ROTATE_MODE_REFERENCE) {
        if (std::isnan(global.baseRX)) {
            global.baseR = v;
            appendPromptHistory();
            addRubber("LINE");
            setRubberMode("LINE");
            setRubberPoint("LINE_START", global.baseRX, global.baseRY);
            setPromptPrefix(tr("Specify second point: "));
        }
        else if (std::isnan(global.destRX)) {
            global.destR = v;
            global.angleRef = calculateAngle(global.baseRX, global.baseRY, global.destRX, global.destRY);
            setRubberPoint("LINE_START", global.baseX, global.baseY);
            previewOn("SELECTED", "ROTATE", global.baseX, global.baseY, global.angleRef);
            appendPromptHistory();
            setPromptPrefix(tr("Specify the new angle: "));
        }
        else if (std::isnan(global.angleNew)) {
            global.angleNew = calculateAngle(global.baseX, global.baseY, x, y);
            rotateSelected(global.baseX, global.baseY, global.angleNew - global.angleRef);
            previewOff();
            actuator("end");
        }
    }
    */
}

/**
 * .
 */
void
rotate_context(String str)
{
    //todo("ROTATE", "context()");
}

/**
 * .
 */
void
rotate_prompt(String str)
{
    /*
    if (global->mode == ROTATE_MODE_NORMAL) {
        if (global.firstRun) {
            EmbReal strList = str.split(",");
            if (std::isnan(strList[0]) || std::isnan(strList[1])) {
                alert(tr("Invalid point."));
                setPromptPrefix(tr("Specify base point: "));
            }
            else {
                global.firstRun = false;
                global.baseX = Number(strList[0]);
                global.baseY = Number(strList[1]);
                addRubber("LINE");
                setRubberMode("LINE");
                setRubberPoint("LINE_START", global.baseX, global.baseY);
                previewOn("SELECTED", "ROTATE", global.baseX, global.baseY, 0);
                setPromptPrefix(tr("Specify rotation angle or [Reference]: "));
            }
        }
        else {
            if (str == "R" || str == "REFERENCE") {
                global.mode = MODE_REFERENCE;
                setPromptPrefix(tr("Specify the reference angle") + " {0.00}: ");
                clearRubber();
                previewOff();
            }
            else {
                if (std::isnan(str)) {
                    alert(tr("Requires valid numeric angle, second point, or option keyword."));
                    setPromptPrefix(tr("Specify rotation angle or [Reference]: "));
                }
                else {
                    global.angle = Number(str);
                    rotateSelected(global.baseX, global.baseY, global.angle);
                    previewOff();
                    actuator("end");
                }
            }
        }
    }
    else if (global->mode == MODE_REFERENCE) {
        if (std::isnan(global.baseRX)) {
            if (std::isnan(str)) {
                EmbReal strList = str.split(",");
                if (std::isnan(strList[0]) || std::isnan(strList[1])) {
                    alert(tr("Requires valid numeric angle or two points."));
                    setPromptPrefix(tr("Specify the reference angle") + " {0.00}: ");
                }
                else {
                    global.baseRX = Number(strList[0]);
                    global.baseRY = Number(strList[1]);
                    addRubber("LINE");
                    setRubberMode("LINE");
                    setRubberPoint("LINE_START", global.baseRX, global.baseRY);
                    setPromptPrefix(tr("Specify second point: "));
                }
            }
            else {
                //The base and dest values are only set here to advance the command.
                global.baseRX = 0.0;
                global.baseRY = 0.0;
                global.destRX = 0.0;
                global.destRY = 0.0;
                //The reference angle is what we will use later.
                global.angleRef = Number(str);
                addRubber("LINE");
                setRubberMode("LINE");
                setRubberPoint("LINE_START", global.baseX, global.baseY);
                previewOn("SELECTED", "ROTATE", global.baseX, global.baseY, global.angleRef);
                setPromptPrefix(tr("Specify the new angle: "));
            }
        }
        else if (std::isnan(global.destRX)) {
            if (std::isnan(str)) {
                EmbReal strList = str.split(",");
                if (std::isnan(strList[0]) || std::isnan(strList[1])) {
                    alert(tr("Requires valid numeric angle or two points."));
                    setPromptPrefix(tr("Specify second point: "));
                }
                else {
                    global.destRX = Number(strList[0]);
                    global.destRY = Number(strList[1]);
                    global.angleRef = calculateAngle(global.baseRX, global.baseRY, global.destRX, global.destRY);
                    previewOn("SELECTED", "ROTATE", global.baseX, global.baseY, global.angleRef);
                    setRubberPoint("LINE_START", global.baseX, global.baseY);
                    setPromptPrefix(tr("Specify the new angle: "));
                }
            }
            else {
                //The base and dest values are only set here to advance the command.
                global.baseRX = 0.0;
                global.baseRY = 0.0;
                global.destRX = 0.0;
                global.destRY = 0.0;
                //The reference angle is what we will use later.
                global.angleRef = Number(str);
                previewOn("SELECTED", "ROTATE", global.baseX, global.baseY, global.angleRef);
                setPromptPrefix(tr("Specify the new angle: "));
            }
        }
        else if (std::isnan(global.angleNew)) {
            if (std::isnan(str)) {
                EmbReal strList = str.split(",");
                if (std::isnan(strList[0]) || std::isnan(strList[1])) {
                    alert(tr("Requires valid numeric angle or second point."));
                    setPromptPrefix(tr("Specify the new angle: "));
                }
                else {
                    EmbReal x = Number(strList[0]);
                    EmbReal y = Number(strList[1]);
                    global.angleNew = calculateAngle(global.baseX, global.baseY, x, y);
                    rotateSelected(global.baseX, global.baseY, global.angleNew - global.angleRef);
                    previewOff();
                    actuator("end");
                }
            }
            else {
                global.angleNew = Number(str);
                rotateSelected(global.baseX, global.baseY, global.angleNew - global.angleRef);
                previewOff();
                actuator("end");
            }
        }
    }
    */
}

//Command: Sandbox

/*var global = {}; //Required
global.test1;
global.test2;
*/

/**
 * .
 */
String
sandbox_main(String str)
{
    /*
    initCommand();

    //Report number of pre-selected objects
    setPromptPrefix("Number of Objects Selected: " + numSelected().toString());
    appendPromptHistory();

    mirrorSelected(0,0,0,1);

    //selectAll();
    //rotateSelected(0,0,90);

    //Polyline & Polygon Testing

    EmbReal offsetX = 0.0;
    EmbReal offsetY = 0.0;

    EmbReal polylineArray = [];
    polylineArray.push(1.0 + offsetX);
    polylineArray.push(1.0 + offsetY);
    polylineArray.push(1.0 + offsetX);
    polylineArray.push(2.0 + offsetY);
    polylineArray.push(2.0 + offsetX);
    polylineArray.push(2.0 + offsetY);
    polylineArray.push(2.0 + offsetX);
    polylineArray.push(3.0 + offsetY);
    polylineArray.push(3.0 + offsetX);
    polylineArray.push(3.0 + offsetY);
    polylineArray.push(3.0 + offsetX);
    polylineArray.push(2.0 + offsetY);
    polylineArray.push(4.0 + offsetX);
    polylineArray.push(2.0 + offsetY);
    polylineArray.push(4.0 + offsetX);
    polylineArray.push(1.0 + offsetY);
    addPolyline(polylineArray);

    offsetX = 5.0;
    offsetY = 0.0;

    EmbReal polygonArray = [];
    polygonArray.push(1.0 + offsetX);
    polygonArray.push(1.0 + offsetY);
    polygonArray.push(1.0 + offsetX);
    polygonArray.push(2.0 + offsetY);
    polygonArray.push(2.0 + offsetX);
    polygonArray.push(2.0 + offsetY);
    polygonArray.push(2.0 + offsetX);
    polygonArray.push(3.0 + offsetY);
    polygonArray.push(3.0 + offsetX);
    polygonArray.push(3.0 + offsetY);
    polygonArray.push(3.0 + offsetX);
    polygonArray.push(2.0 + offsetY);
    polygonArray.push(4.0 + offsetX);
    polygonArray.push(2.0 + offsetY);
    polygonArray.push(4.0 + offsetX);
    polygonArray.push(1.0 + offsetY);
    addPolygon(polygonArray);

    actuator("end");
    */
    return "";
}

/**
 * .
 * Command: Scale
 *
 * var global = {}; //Required
 * global.firstRun;
 * global.baseX;
 * global.baseY;
 * global.destX;
 * global.destY;
 * global.factor;
 *
 * global.baseRX;
 * global.baseRY;
 * global.destRX;
 * global.destRY;
 * global.factorRef;
 * global.factorNew;
 *
 * global.mode;
 */
void
scale_main()
{
    /*
    initCommand();
    global.mode = MODE_NORMAL;
    global.firstRun = true;
    global.baseX  = NaN;
    global.baseY  = NaN;
    global.destX  = NaN;
    global.destY  = NaN;
    global.factor = NaN;

    global.baseRX    = NaN;
    global.baseRY    = NaN;
    global.destRX    = NaN;
    global.destRY    = NaN;
    global.factorRef = NaN;
    global.factorNew = NaN;

    if (numSelected() <= 0) {
        //TODO: Prompt to select objects if nothing is preselected
        alert(tr("Preselect objects before invoking the scale command."));
        actuator("end");
        messageBox("information", tr("Scale Preselect"), tr("Preselect objects before invoking the scale command."));
    }
    else {
        setPromptPrefix(tr("Specify base point: "));
    }
    */
}

/**
 * .
 */
void
scale_click(UiObject *global, EmbVector v)
{
    /*
    if (global->mode == MODE_NORMAL) {
        if (global.firstRun) {
            global.firstRun = false;
            global.base = v;
            addRubber("LINE");
            setRubberMode("LINE");
            setRubberPoint("LINE_START", global.baseX, global.baseY);
            previewOn("SELECTED", "SCALE", global.baseX, global.baseY, 1);
            appendPromptHistory();
            setPromptPrefix(tr("Specify scale factor or [Reference]: "));
        }
        else {
            global.dest = v;
            global.factor = calculateDistance(global.base, global.dest);
            appendPromptHistory();
            scaleSelected(global.baseX, global.baseY, global.factor);
            previewOff();
            actuator("end");
        }
    }
    else if (global->mode == MODE_REFERENCE) {
        if (std::isnan(global.baseRX)) {
            global.baseR = v;
            appendPromptHistory();
            addRubber("LINE");
            setRubberMode("LINE");
            setRubberPoint("LINE_START", global.baseR);
            setPromptPrefix(tr("Specify second point: "));
        }
        else if (std::isnan(global.destRX)) {
            global.destR = v;
            global.factorRef = calculateDistance(global.baseRX, global.baseRY, global.destRX, global.destRY);
            if (global.factorRef <= 0.0) {
                global.destRX    = NaN;
                global.destRY    = NaN;
                global.factorRef = NaN;
                alert(tr("Value must be positive and nonzero."));
                setPromptPrefix(tr("Specify second point: "));
            }
            else {
                appendPromptHistory();
                setRubberPoint("LINE_START", global.baseX, global.baseY);
                previewOn("SELECTED", "SCALE", global.baseX, global.baseY, global.factorRef);
                setPromptPrefix(tr("Specify new length: "));
            }
        }
        else if (std::isnan(global.factorNew)) {
            global.factorNew = calculateDistance(global.baseX, global.baseY, x, y);
            if (global.factorNew <= 0.0) {
                global.factorNew = NaN;
                alert(tr("Value must be positive and nonzero."));
                setPromptPrefix(tr("Specify new length: "));
            }
            else {
                appendPromptHistory();
                scaleSelected(global.baseX, global.baseY, global.factorNew/global.factorRef);
                previewOff();
                actuator("end");
            }
        }
    }
    */
}

/**
 * .
 */
void
scale_context(String str)
{
    //todo("SCALE", "context()");
}

/**
 * .
 */
void
scale_prompt(String str)
{
    /*
    if (global->mode == MODE_NORMAL) {
        if (global.firstRun) {
            EmbReal strList = str.split(",");
            if (std::isnan(strList[0]) || std::isnan(strList[1])) {
                alert(tr("Invalid point."));
                setPromptPrefix(tr("Specify base point: "));
            }
            else {
                global.firstRun = false;
                global.baseX = Number(strList[0]);
                global.baseY = Number(strList[1]);
                addRubber("LINE");
                setRubberMode("LINE");
                setRubberPoint("LINE_START", global.baseX, global.baseY);
                previewOn("SELECTED", "SCALE", global.baseX, global.baseY, 1);
                setPromptPrefix(tr("Specify scale factor or [Reference]: "));
            }
        }
        else {
            if (str == "R" || str == "REFERENCE") {
                global.mode = MODE_REFERENCE;
                setPromptPrefix(tr("Specify reference length") + " {1}: ");
                clearRubber();
                previewOff();
            }
            else {
                if (std::isnan(str)) {
                    alert(tr("Requires valid numeric distance, second point, or option keyword."));
                    setPromptPrefix(tr("Specify scale factor or [Reference]: "));
                }
                else {
                    global.factor = Number(str);
                    scaleSelected(global.baseX, global.baseY, global.factor);
                    previewOff();
                    actuator("end");
                }
            }
        }
    }
    else if (global->mode == MODE_REFERENCE) {
        if (std::isnan(global.baseRX)) {
            if (std::isnan(str)) {
                EmbReal strList = str.split(",");
                if (std::isnan(strList[0]) || std::isnan(strList[1])) {
                    alert(tr("Requires valid numeric distance or two points."));
                    setPromptPrefix(tr("Specify reference length") + " {1}: ");
                }
                else {
                    global.baseRX = Number(strList[0]);
                    global.baseRY = Number(strList[1]);
                    addRubber("LINE");
                    setRubberMode("LINE");
                    setRubberPoint("LINE_START", global.baseRX, global.baseRY);
                    setPromptPrefix(tr("Specify second point: "));
                }
            }
            else {
                //The base and dest values are only set here to advance the command.
                global.baseRX = 0.0;
                global.baseRY = 0.0;
                global.destRX = 0.0;
                global.destRY = 0.0;
                //The reference length is what we will use later.
                global.factorRef = Number(str);
                if (global.factorRef <= 0.0) {
                    global.baseRX    = NaN;
                    global.baseRY    = NaN;
                    global.destRX    = NaN;
                    global.destRY    = NaN;
                    global.factorRef = NaN;
                    alert(tr("Value must be positive and nonzero."));
                    setPromptPrefix(tr("Specify reference length") + " {1}: ");
                }
                else {
                    addRubber("LINE");
                    setRubberMode("LINE");
                    setRubberPoint("LINE_START", global.baseX, global.baseY);
                    previewOn("SELECTED", "SCALE", global.baseX, global.baseY, global.factorRef);
                    setPromptPrefix(tr("Specify new length: "));
                }
            }
        }
        else if (std::isnan(global.destRX)) {
            if (std::isnan(str)) {
                EmbReal strList = str.split(",");
                if (std::isnan(strList[0]) || std::isnan(strList[1])) {
                    alert(tr("Requires valid numeric distance or two points."));
                    setPromptPrefix(tr("Specify second point: "));
                }
                else {
                    global.destRX = Number(strList[0]);
                    global.destRY = Number(strList[1]);
                    global.factorRef = calculateDistance(global.baseRX, global.baseRY, global.destRX, global.destRY);
                    if (global.factorRef <= 0.0) {
                        global.destRX    = NaN;
                        global.destRY    = NaN;
                        global.factorRef = NaN;
                        alert(tr("Value must be positive and nonzero."));
                        setPromptPrefix(tr("Specify second point: "));
                    }
                    else {
                        setRubberPoint("LINE_START", global.baseX, global.baseY);
                        previewOn("SELECTED", "SCALE", global.baseX, global.baseY, global.factorRef);
                        setPromptPrefix(tr("Specify new length: "));
                    }
                }
            }
            else {
                //The base and dest values are only set here to advance the command.
                global.baseRX = 0.0;
                global.baseRY = 0.0;
                global.destRX = 0.0;
                global.destRY = 0.0;
                //The reference length is what we will use later.
                global.factorRef = Number(str);
                if (global.factorRef <= 0.0) {
                    global.destRX    = NaN;
                    global.destRY    = NaN;
                    global.factorRef = NaN;
                    alert(tr("Value must be positive and nonzero."));
                    setPromptPrefix(tr("Specify second point: "));
                }
                else {
                    setRubberPoint("LINE_START", global.baseX, global.baseY);
                    previewOn("SELECTED", "SCALE", global.baseX, global.baseY, global.factorRef);
                    setPromptPrefix(tr("Specify new length: "));
                }
            }
        }
        else if (std::isnan(global.factorNew)) {
            if (std::isnan(str)) {
                EmbReal strList = str.split(",");
                if (std::isnan(strList[0]) || std::isnan(strList[1])) {
                    alert(tr("Requires valid numeric distance or second point."));
                    setPromptPrefix(tr("Specify new length: "));
                }
                else {
                    EmbReal x = Number(strList[0]);
                    EmbReal y = Number(strList[1]);
                    global.factorNew = calculateDistance(global.baseX, global.baseY, x, y);
                    if (global.factorNew <= 0.0) {
                        global.factorNew = NaN;
                        alert(tr("Value must be positive and nonzero."));
                        setPromptPrefix(tr("Specify new length: "));
                    }
                    else {
                        scaleSelected(global.baseX, global.baseY, global.factorNew/global.factorRef);
                        previewOff();
                        actuator("end");
                    }
                }
            }
            else {
                global.factorNew = Number(str);
                if (global.factorNew <= 0.0) {
                    global.factorNew = NaN;
                    alert(tr("Value must be positive and nonzero."));
                    setPromptPrefix(tr("Specify new length: "));
                }
                else {
                    scaleSelected(global.baseX, global.baseY, global.factorNew/global.factorRef);
                    previewOff();
                    actuator("end");
                }
            }
        }
    }
    */
}


//Command: Single Line Text
/*
var global = {}; //Required
global.text;
global.textX;
global.textY;
global.textJustify;
global.textFont;
global.textHeight;
global.textRotation;
global.mode;
*/

/**
 * .
 */
void
text_single_main()
{
    /*
    initCommand();
    clearSelection();
    global.text = "";
    global.textX = NaN;
    global.textY = NaN;
    global.textJustify = "Left";
    global.textFont = textFont();
    global.textHeight = NaN;
    global.textRotation = NaN;
    global.mode = MODE_SETGEOM;
    setPromptPrefix(tr("Current font: ") + "{" + global.textFont + "} " + tr("Text height: ") + "{" +  textSize() + "}");
    appendPromptHistory();
    setPromptPrefix(tr("Specify start point of text or [Justify/Setfont]: "));
    */
}

/**
 * .
 */
void
text_single_click(UiObject *global, EmbVector v)
{
    /*
    if (global->mode == MODE_SETGEOM) {
        if (std::isnan(global.textX)) {
            global.textX = x;
            global.textY = y;
            addRubber("LINE");
            setRubberMode("LINE");
            setRubberPoint("LINE_START", global.textX, global.textY);
            appendPromptHistory();
            setPromptPrefix(tr("Specify text height") + " {" + textSize() + "}: ");
        }
        else if (std::isnan(global.textHeight)) {
            global.textHeight = calculateDistance(global.textX, global.textY, x, y);
            setTextSize(global.textHeight);
            appendPromptHistory();
            setPromptPrefix(tr("Specify text angle") + " {" + textAngle() + "}: ");
        }
        else if (std::isnan(global.textRotation)) {
            global.textRotation = calculateAngle(global.textX, global.textY, x, y);
            setTextAngle(global.textRotation);
            appendPromptHistory();
            setPromptPrefix(tr("Enter text: "));
            global.mode = MODE_RAPID;
            prompt->enableRapidFire();
            clearRubber();
            addRubber("TEXTSINGLE");
            setRubberMode("TEXTSINGLE");
            setRubberPoint("TEXT_POINT", global.textX, global.textY);
            setRubberPoint("TEXT_HEIGHT_ROTATION", global.textHeight, global.textRotation);
            setRubberText("TEXT_FONT", global.textFont);
            setRubberText("TEXT_JUSTIFY", global.textJustify);
            setRubberText("TEXT_RAPID", global.text);
        }
        else {
            //Do nothing, as we are in rapidFire mode now.
        }
    }
    */
}

/**
 * .
 */
void
text_single_context(String str)
{
    //todo("SINGLELINETEXT", "context()");
}

/**
 * .
 */
void
text_single_prompt(String str)
{
    /*
    if (global->mode == MODE_JUSTIFY) {
        if (str == "C" || str == "CENTER") {
            global.mode = MODE_SETGEOM;
            global.textJustify = "Center";
            setRubberText("TEXT_JUSTIFY", global.textJustify);
            setPromptPrefix(tr("Specify center point of text or [Justify/Setfont]: "));
        }
        else if (str == "R" || str == "RIGHT") {
            global.mode = MODE_SETGEOM;
            global.textJustify = "Right";
            setRubberText("TEXT_JUSTIFY", global.textJustify);
            setPromptPrefix(tr("Specify right-end point of text or [Justify/Setfont]: "));
        }
        else if (str == "A" || str == "ALIGN") {
            global.mode = MODE_SETGEOM;
            global.textJustify = "Aligned";
            setRubberText("TEXT_JUSTIFY", global.textJustify);
            setPromptPrefix(tr("Specify start point of text or [Justify/Setfont]: "));
        }
        else if (str == "M" || str == "MIDDLE") {
            global.mode = MODE_SETGEOM;
            global.textJustify = "Middle";
            setRubberText("TEXT_JUSTIFY", global.textJustify);
            setPromptPrefix(tr("Specify middle point of text or [Justify/Setfont]: "));
        }
        else if (str == "F" || str == "FIT") {
            global.mode = MODE_SETGEOM;
            global.textJustify = "Fit";
            setRubberText("TEXT_JUSTIFY", global.textJustify);
            setPromptPrefix(tr("Specify start point of text or [Justify/Setfont]: "));
        }
        else if (str == "TL" || str == "TOPLEFT") {
            global.mode = MODE_SETGEOM;
            global.textJustify = "Top Left";
            setRubberText("TEXT_JUSTIFY", global.textJustify);
            setPromptPrefix(tr("Specify top-left point of text or [Justify/Setfont]: "));
        }
        else if (str == "TC" || str == "TOPCENTER") {
            global.mode = MODE_SETGEOM;
            global.textJustify = "Top Center";
            setRubberText("TEXT_JUSTIFY", global.textJustify);
            setPromptPrefix(tr("Specify top-center point of text or [Justify/Setfont]: "));
        }
        else if (str == "TR" || str == "TOPRIGHT") {
            global.mode = MODE_SETGEOM;
            global.textJustify = "Top Right";
            setRubberText("TEXT_JUSTIFY", global.textJustify);
            setPromptPrefix(tr("Specify top-right point of text or [Justify/Setfont]: "));
        }
        else if (str == "ML" || str == "MIDDLELEFT") {
            global.mode = MODE_SETGEOM;
            global.textJustify = "Middle Left";
            setRubberText("TEXT_JUSTIFY", global.textJustify);
            setPromptPrefix(tr("Specify middle-left point of text or [Justify/Setfont]: "));
        }
        else if (str == "MC" || str == "MIDDLECENTER") {
            global.mode = MODE_SETGEOM;
            global.textJustify = "Middle Center";
            setRubberText("TEXT_JUSTIFY", global.textJustify);
            setPromptPrefix(tr("Specify middle-center point of text or [Justify/Setfont]: "));
        }
        else if (str == "MR" || str == "MIDDLERIGHT") {
            global.mode = MODE_SETGEOM;
            global.textJustify = "Middle Right";
            setRubberText("TEXT_JUSTIFY", global.textJustify);
            setPromptPrefix(tr("Specify middle-right point of text or [Justify/Setfont]: "));
        }
        else if (str == "BL" || str == "BOTTOMLEFT") {
            global.mode = MODE_SETGEOM;
            global.textJustify = "Bottom Left";
            setRubberText("TEXT_JUSTIFY", global.textJustify);
            setPromptPrefix(tr("Specify bottom-left point of text or [Justify/Setfont]: "));
        }
        else if (str == "BC" || str == "BOTTOMCENTER") {
            global.mode = MODE_SETGEOM;
            global.textJustify = "Bottom Center";
            setRubberText("TEXT_JUSTIFY", global.textJustify);
            setPromptPrefix(tr("Specify bottom-center point of text or [Justify/Setfont]: "));
        }
        else if (str == "BR" || str == "BOTTOMRIGHT") {
            global.mode = MODE_SETGEOM;
            global.textJustify = "Bottom Right";
            setRubberText("TEXT_JUSTIFY", global.textJustify);
            setPromptPrefix(tr("Specify bottom-right point of text or [Justify/Setfont]: "));
        }
        else {
            alert(tr("Invalid option keyword."));
            setPromptPrefix(tr("Text Justification Options [Center/Right/Align/Middle/Fit/TL/TC/TR/ML/MC/MR/BL/BC/BR]: "));
        }
    }
    else if (global->mode == MODE_SETFONT) {
        global.mode = MODE_SETGEOM;
        global.textFont = str;
        setRubberText("TEXT_FONT", global.textFont);
        setTextFont(global.textFont);
        setPromptPrefix(tr("Specify start point of text or [Justify/Setfont]: "));
    }
    else if (global->mode == MODE_SETGEOM) {
        if (std::isnan(global.textX)) {
            if (str == "J" || str == "JUSTIFY") {
                global.mode = MODE_JUSTIFY;
                setPromptPrefix(tr("Text Justification Options [Center/Right/Align/Middle/Fit/TL/TC/TR/ML/MC/MR/BL/BC/BR]: "));
            }
            else if (str == "S" || str == "SETFONT") {
                global.mode = MODE_SETFONT;
                setPromptPrefix(tr("Specify font name: "));
            }
            else {
                EmbReal strList = str.split(",");
                if (std::isnan(strList[0]) || std::isnan(strList[1])) {
                    alert(tr("Point or option keyword required."));
                    setPromptPrefix(tr("Specify start point of text or [Justify/Setfont]: "));
                }
                else {
                    global.textX = Number(strList[0]);
                    global.textY = Number(strList[1]);
                    addRubber("LINE");
                    setRubberMode("LINE");
                    setRubberPoint("LINE_START", global.textX, global.textY);
                    setPromptPrefix(tr("Specify text height") + " {" + textSize() + "}: ");
                }
            }
        }
        else if (std::isnan(global.textHeight)) {
            if (str == "") {
                global.textHeight = textSize();
                setPromptPrefix(tr("Specify text angle") + " {" + textAngle() + "}: ");
            }
            else if (std::isnan(str)) {
                alert(tr("Requires valid numeric distance or second point."));
                setPromptPrefix(tr("Specify text height") + " {" + textSize() + "}: ");
            }
            else {
                global.textHeight = Number(str);
                setTextSize(global.textHeight);
                setPromptPrefix(tr("Specify text angle") + " {" + textAngle() + "}: ");
            }
        }
        else if (std::isnan(global.textRotation)) {
            if (str == "") {
                global.textRotation = textAngle();
                setPromptPrefix(tr("Enter text: "));
                global.mode = MODE_RAPID;
                prompt->enableRapidFire();
                clearRubber();
                addRubber("TEXTSINGLE");
                setRubberMode("TEXTSINGLE");
                setRubberPoint("TEXT_POINT", global.textX, global.textY);
                setRubberPoint("TEXT_HEIGHT_ROTATION", global.textHeight, global.textRotation);
                setRubberText("TEXT_FONT", global.textFont);
                setRubberText("TEXT_JUSTIFY", global.textJustify);
                setRubberText("TEXT_RAPID", global.text);
            }
            else if (std::isnan(str)) {
                alert(tr("Requires valid numeric angle or second point."));
                setPromptPrefix(tr("Specify text angle") + " {" + textAngle() + "}: ");
            }
            else {
                global.textRotation = Number(str);
                setTextAngle(global.textRotation);
                setPromptPrefix(tr("Enter text: "));
                global.mode = MODE_RAPID;
                prompt->enableRapidFire();
                clearRubber();
                addRubber("TEXTSINGLE");
                setRubberMode("TEXTSINGLE");
                setRubberPoint("TEXT_POINT", global.textX, global.textY);
                setRubberPoint("TEXT_HEIGHT_ROTATION", global.textHeight, global.textRotation);
                setRubberText("TEXT_FONT", global.textFont);
                setRubberText("TEXT_JUSTIFY", global.textJustify);
                setRubberText("TEXT_RAPID", global.text);
            }
        }
        else {
            //Do nothing, as we are in rapidFire mode now.
        }
    }
    else if (global->mode == MODE_RAPID) {
        if (str == "RAPID_ENTER") {
            if (global.text == "") {
                actuator("end");
            }
            else {
                actuator("vulcanize");
                actuator("end");
                //TODO: Rather than ending the command, calculate where the next line would be and modify the x/y to the new point
            }
        }
        else {
            global.text = str;
            setRubberText("TEXT_RAPID", global.text);
        }
    }
    */
}

UiObject snowflake_main(void);
void updateSnowflake(UiObject *global, EmbVector scale);
UiObject star_main(void);
void star_move(UiObject *global, EmbVector v);
void star_prompt(String str);
void star_context(String str);
void star_prompt(String str);
void updateStar(UiObject *global, EmbVector mouse);
void syswindows_main(void);
void syswindows_prompt(String str);

/**
 * .
 */
UiObject
snowflake_main()
{
    UiObject global;
    //initCommand();
    //clearSelection();
    global.numPoints = 2048; //Default //TODO: min:64 max:8192
    global.center.x = 0.0;
    global.center.y = 0.0;
    global.scale.x = 0.04;
    global.scale.y = 0.04;
    global.mode = SNOWFLAKE_MODE_NUM_POINTS;

    //addRubber("POLYGON");
    //setRubberMode("POLYGON");
    updateSnowflake(&global, global.scale);
    //spareRubber("POLYGON");
    actuator("end");
    return global;
}

/**
 *
 */
EmbReal
fourier_series(EmbReal arg, std::vector<EmbReal> terms)
{
    EmbReal x = 0.0f;
    for (int i=0; i<(int)(terms.size()/3); i++) {
        x += terms[3*i+0] * sin(terms[3*i+1] + terms[3*i+2] * arg);
    }
    return x;
}

/**
 * Snowflake Curve with t [0,2pi]
 */
void
updateSnowflake(UiObject *global, EmbVector scale)
{
    for (int i = 0; i <= global->numPoints; i++) {
        EmbReal t = (2.0*emb_constant_pi) / global->numPoints*i;
        EmbVector v;
        v.x = fourier_series(t, snowflake_x);
        v.y = fourier_series(t, snowflake_y);

        /*
        setRubberPoint("POLYGON_POINT_" + i.toString(),
            xx*scale.x, yy*scale.y);
        */
    }

    /*
    setRubberText("POLYGON_NUM_POINTS", numPoints.toString());
    */
}

/**
 * .
 */
UiObject
star_main(void)
{
    UiObject global;
    /*
    initCommand();
    clearSelection();
    */
    global.numPoints = 5;
    global.center.x = 0.0;
    global.center.y = 0.0;
    global.point1.x = 1.0;
    global.point1.y = 1.0;
    global.point2.x = 2.0;
    global.point2.y = 2.0;
    global.mode = STAR_MODE_NUM_POINTS;
    /*
    setPromptPrefix(tr("Enter number of star points {5}: "));
    */
    return global;
}

/**
 * .
 */
void
star_click(UiObject *global, EmbReal mouse)
{
    /*
    if (global->mode == MODE_NUM_POINTS) {
        //Do nothing, the prompt controls this.
    }
    else if (global->mode == MODE_CENTER_PT) {
        global->center = mouse;
        global.mode = MODE_RAD_OUTER;
        setPromptPrefix(tr("Specify outer radius of star: "));
        addRubber("POLYGON");
        setRubberMode("POLYGON");
        updateStar(global, global->center);
        actuator("enable move-rapid-fire");
    }
    else if (global->mode == MODE_RAD_OUTER) {
        global->point1 = mouse;
        global->mode = MODE_RAD_INNER;
        setPromptPrefix(tr("Specify inner radius of star: "));
        updateStar(global.point1);
    }
    else if (global->mode == MODE_RAD_INNER) {
        global->point2 = mouse;
        actuator("disable move-rapid-fire");
        updateStar(global.point2);
        spareRubber("POLYGON");
        //actuator("end");
    }
    */
}

/**
 * .
 */
void
star_move(UiObject *global, EmbVector v)
{
    if (global->mode == STAR_MODE_NUM_POINTS) {
        //Do nothing, the prompt controls this.
    }
    else if (global->mode == STAR_MODE_CENTER_PT) {
        //Do nothing, prompt and click controls this.
    }
    else if (global->mode == STAR_MODE_RAD_OUTER) {
        updateStar(global, v);
    }
    else if (global->mode == STAR_MODE_RAD_INNER) {
        updateStar(global, v);
    }
}

/**
 * .
 */
void
star_context(String str)
{
    //todo("STAR", "context()");
}

/**
 * .
 */
void
star_prompt(String str)
{
    /*
    if (global->mode == STAR_MODE_NUM_POINTS) {
        if (str == "" && global.numPoints >= 3 && global.numPoints <= 1024) {
            setPromptPrefix(tr("Specify center point: "));
            global.mode = STAR_MODE_CENTER_PT;
        }
        else {
            EmbReal tmp = Number(str);
            if (std::isnan(tmp) || !isInt(tmp) || tmp < 3 || tmp > 1024) {
                alert(tr("Requires an integer between 3 and 1024."));
                setPromptPrefix(tr("Enter number of star points") + " {" + global.numPoints.toString() + "}: ");
            }
            else {
                global.numPoints = tmp;
                setPromptPrefix(tr("Specify center point: "));
                global.mode = MODE_CENTER_PT;
            }
        }
    }
    else if (global->mode == STAR_MODE_CENTER_PT) {
        EmbReal strList = str.split(",");
        if (std::isnan(strList[0]) || std::isnan(strList[1])) {
            alert(tr("Invalid point."));
            setPromptPrefix(tr("Specify center point: "));
        }
        else {
            global->center.x = Number(strList[0]);
            global->center.y = Number(strList[1]);
            global.mode = MODE_RAD_OUTER;
            setPromptPrefix(tr("Specify outer radius of star: "));
            addRubber("POLYGON");
            setRubberMode("POLYGON");
            updateStar(qsnapX(), qsnapY());
            actuator("enable move-rapid-fire");
        }
    }
    else if (global->mode == STAR_MODE_RAD_OUTER) {
        EmbReal strList = str.split(",");
        if (std::isnan(strList[0]) || std::isnan(strList[1])) {
            alert(tr("Invalid point."));
            setPromptPrefix(tr("Specify outer radius of star: "));
        }
        else {
            global.x1 = Number(strList[0]);
            global.y1 = Number(strList[1]);
            global.mode = MODE_RAD_INNER;
            setPromptPrefix(tr("Specify inner radius of star: "));
            updateStar(qsnapX(), qsnapY());
        }
    }
    else if (global->mode == STAR_MODE_RAD_INNER) {
        EmbReal strList = str.split(",");
        if (std::isnan(strList[0]) || std::isnan(strList[1])) {
            alert(tr("Invalid point."));
            setPromptPrefix(tr("Specify inner radius of star: "));
        }
        else {
            global.x2 = Number(strList[0]);
            global.y2 = Number(strList[1]);
            actuator("disable move-rapid-fire");
            updateStar(global.x2, global.y2);
            spareRubber("POLYGON");
            //actuator("end");
        }
    }
    */
}

/**
 * .
 */
void
updateStar(UiObject *global, EmbVector mouse)
{
    EmbReal distOuter;
    EmbReal distInner;
    EmbReal angOuter;

    if (global->mode == STAR_MODE_RAD_OUTER) {
        EmbLine line;
        line.start = global->center;
        line.end = mouse;
        EmbVector v = embVector_subtract(line.end, line.start);
        angOuter = embVector_angle(v);
        distOuter = embVector_distance(global->center, mouse);
        distInner = distOuter/2.0;
    }
    else if (global->mode == STAR_MODE_RAD_INNER) {
        EmbLine line;
        line.start = global->center;
        line.end = global->point1;
        EmbVector v = embVector_subtract(line.end, line.start);
        angOuter = embVector_angle(v);
        distOuter = embVector_distance(global->center, global->point1);
        distInner = embVector_distance(global->center, mouse);
    }

    // Calculate the Star Points
    EmbReal angInc = 360.0/(global->numPoints*2);
    for (int i = 0; i < global->numPoints*2; i++) {
        EmbReal angle = (angOuter + (angInc*i)) * emb_constant_pi / 180.0;
        EmbVector v;
        if (i%2 == 0) {
            v.x = distOuter*cos(angle);
            v.y = distOuter*sin(angle);
        }
        else {
            v.x = distInner*cos(angle);
            v.y = distInner*sin(angle);
        }
        /*
        setRubberPoint(
            QString::fromStdString("POLYGON_POINT_" + std::to_string(i)),
            global->center.x + v.x,
            global->center.y + v.y);
        */
    }
    /*
    setRubberText("POLYGON_NUM_POINTS",
        (global.numPoints*2 - 1).toString());
    */
}

/**
 * .
 */
void
syswindows_main(void)
{
    /*
    initCommand();
    clearSelection();
    setPromptPrefix(tr("Enter an option [Cascade/Tile]: "));
    */
}

/**
 * .
 */
void
syswindows_prompt(String str)
{
    if (str == "C" || str == "CASCADE") {
        actuator("window cascade");
        actuator("end");
    }
    else if (str == "T" || str == "TILE") {
        actuator("window tile");
        actuator("end");
    }
    else {
        /*
        alert(tr("Invalid option keyword."));
        setPromptPrefix(tr("Enter an option [Cascade/Tile]: "));
        */
    }
}

