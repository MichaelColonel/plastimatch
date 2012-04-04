/* -----------------------------------------------------------------------
   See COPYRIGHT.TXT and LICENSE.TXT for copyright and license information
   ----------------------------------------------------------------------- */
#ifndef _acquire_4030e_window_h_
#define _acquire_4030e_window_h_
#include "ise_config.h"
#include <QAction>
#include <QMainWindow>
#include <QMenu>
#include <QSystemTrayIcon>
#include "ui_acquire_4030e_window.h"

class QString;
class QSystemTrayIcon;

class Acquire_4030e_window : public QMainWindow,
                             private Ui::ui_acquire_4030e_window
{
    Q_OBJECT
    ;
public:
    Acquire_4030e_window ();
protected:
    void closeEvent(QCloseEvent *event);
public:
    void create_actions ();
    void create_tray_icon ();
    void set_icon ();
    void log_output (const QString& log);

public slots:
    void request_quit ();
    void systray_activated (QSystemTrayIcon::ActivationReason reason);

public:
    QAction *show_action;
    QAction *quit_action;
    QSystemTrayIcon *tray_icon;
    QMenu *tray_icon_menu;
};

#endif