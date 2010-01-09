/* -----------------------------------------------------------------------
   See COPYRIGHT.TXT and LICENSE.TXT for copyright and license information
   ----------------------------------------------------------------------- */
#include "plm_config.h"
#include <QApplication>
#include <QLabel>
#include <QtGui>

#include "pqt_main_window.h"

int
main (int argc, char **argv)
{
    QApplication app (argc, argv);

    Pqt_main_window pqt_main_window;
    pqt_main_window.show ();

    return app.exec();
}
