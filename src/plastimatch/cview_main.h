/* -----------------------------------------------------------------------
   See COPYRIGHT.TXT and LICENSE.TXT for copyright and license information
   ----------------------------------------------------------------------- */
#ifndef _cview_main_h_
#define _cview_main_h_

#include <QtGui>
#include "plm_image.h"
#include "plmbase.h"
#include "cview_portal.h"

class PortalGrid : public QWidget
{
    public:
        PortalGrid (QWidget* parent = 0);

    public:
        PortalWidget* portal[4];
};

class CrystalWindow : public QMainWindow
{
    Q_OBJECT;

    public:
        CrystalWindow (int argc, char** argv, QWidget *parent = 0);

    public slots:
        void open ();

    private: /* methods */
        bool openVol (const char* fn);
        void createActions ();
        void createMenu ();

    private: /* variables */
        PortalGrid *portalGrid;
        Plm_image* pli;
        Volume* input_vol;
        QMenu *menuFile;
        QMenu *menuHelp;
        QAction *actionOpen;
        QAction *actionExit;
        QAction *actionAboutQt;
};

#endif
