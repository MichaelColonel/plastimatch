/* -----------------------------------------------------------------------
   See COPYRIGHT.TXT and LICENSE.TXT for copyright and license information
   ----------------------------------------------------------------------- */

/* JAS - 2011.08.14
 *   This is CrystalView... which is currently more or less just a 
 *   testbed for my PortalWidget Qt4 class.  All of this is in
 *   very early stages of development.
 */

#include "plm_config.h"
#include <iostream>
#include <QtGui>
#include "volume.h"
#include "mha_io.h"
#include "cview_portal.h"
#include "cview_main.h"

#define VERSION "0.07a"

/////////////////////////////////////////////////////////
// PortalGrid : public
//

PortalGrid::PortalGrid (Volume* input_vol, QWidget *parent)
    :QWidget (parent)
{
    /* Create a grid layout with splitters */
    QGridLayout *grid = new QGridLayout (this);

    QSplitter *splitterT = new QSplitter (Qt::Horizontal);
    QSplitter *splitterB = new QSplitter (Qt::Horizontal);
    QSplitter *splitterV = new QSplitter (Qt::Vertical);

    /* Create some portals */
    for (int i=0; i<4; i++) {
        portal[i] = new PortalWidget;
    }

    /* place portals inside splitters */
    splitterT->addWidget (portal[0]);
    splitterT->addWidget (portal[1]);
    splitterB->addWidget (portal[2]);
    splitterB->addWidget (portal[3]);
    splitterV->addWidget (splitterT);
    splitterV->addWidget (splitterB);


    /* Let's make a slider and connect it to portal[1] */
    QSlider *slider1 = new QSlider (Qt::Vertical);
    slider1->setRange (0, 512);

    connect (slider1, SIGNAL(valueChanged(int)),
             portal[1], SLOT(renderSlice(int)));

    connect (portal[1], SIGNAL(sliceChanged(int)),
             slider1, SLOT(setValue(int)));

    /* Set the layout */
    grid->addWidget (splitterV, 0, 0);
    grid->addWidget (slider1, 0, 1);
    setLayout (grid);
}


/////////////////////////////////////////////////////////
// CrystalWindow : private
//

bool
CrystalWindow::openVol (const char* fn)
{
    if (input_vol) {
        for (int i=0; i<4; i++) {
            portalGrid->portal[i]->detachVolume();
        }
        delete input_vol;
    }
    input_vol = read_mha (fn);

    if (!input_vol) {
        return false;
    }

    volume_convert_to_float (input_vol);

    for (int i=0; i<4; i++) {
        portalGrid->portal[i]->resetPortal();
        portalGrid->portal[i]->setVolume (input_vol);
    }

    portalGrid->portal[0]->setView (PortalWidget::Axial);
    portalGrid->portal[1]->setView (PortalWidget::Coronal);
    portalGrid->portal[2]->setView (PortalWidget::Sagittal);
    portalGrid->portal[3]->setView (PortalWidget::Axial);

    return true;
}

void
CrystalWindow::createActions ()
{
    actionOpen = new QAction (tr("&Open"), this);
    actionExit = new QAction (tr("E&xit"), this);
    actionAboutQt = new QAction (tr("About &Qt"), this);

    connect (actionExit, SIGNAL(triggered()), qApp, SLOT(quit()));
    connect (actionOpen, SIGNAL(triggered()), this, SLOT(open()));
    connect (actionAboutQt, SIGNAL(triggered()), qApp, SLOT(aboutQt()));
}

void
CrystalWindow::createMenu ()
{
    menuFile = menuBar()->addMenu (tr("&File"));
    menuFile->addAction (actionOpen);
    menuFile->addAction (actionExit);

    menuBar()->addSeparator();  /* fancy in some environments */

    menuHelp = menuBar()->addMenu (tr("&Help"));
    menuHelp->addAction (actionAboutQt);
}

/////////////////////////////////////////////////////////
// CrystalWindow : slots
//

void
CrystalWindow::open ()
{
    QString fileName =
        QFileDialog::getOpenFileName (
                this,
                tr("Open Volume"),
                "",
                tr("MHA Volumes (*.mha)")
    );

    QByteArray ba = fileName.toLocal8Bit();
    const char *fn = ba.data();

    /* Only attempt load if user selects a file */
    if (strcmp (fn, "")) {
        openVol (fn);
    }
}

/////////////////////////////////////////////////////////
// CrystalWindow : public
//

CrystalWindow::CrystalWindow (int argc, char** argv, QWidget *parent)
    :QMainWindow (parent)
{
    input_vol = NULL;

    createActions ();
    createMenu ();

    portalGrid = new PortalGrid (input_vol);
    setCentralWidget (portalGrid);

    /* open mha from command line */
    if (argc > 1) {
        if (!openVol (argv[1])) {
            std::cout << "Failed to load: " << argv[1] << "\n";
        }
    }
}

/////////////////////////////////////////////////////////
// main ()
//

int
main (int argc, char** argv)
{
    std::cout << "CrystalView " << VERSION << "\n"
              << "Usage: cview [mha_file]\n\n"
              << "  Application Hotkeys:\n"
              << "     1 - axial view\n"
              << "     2 - coronal view\n"
              << "     3 - sagittal view\n"
              << "   -/+ - change active slice\n"
              << "     ] - zoom in\n"
              << "     [ - zoom out\n"
              << "     r - reset portal\n\n"
              << "  Mouse Functions:\n"
              << "      right click - pan/scroll\n"
              << "       left click - report coordinate (broken)\n"
              << "            wheel - change active slice\n"
              << "       Ctrl+wheel - zoom in/out\n\n";


    QApplication app (argc, argv);

    CrystalWindow cview_window (argc, argv);
    cview_window.setWindowTitle ("CrystalView " VERSION);
    cview_window.setMinimumSize (640, 480);
    cview_window.show ();

    return app.exec ();
}
