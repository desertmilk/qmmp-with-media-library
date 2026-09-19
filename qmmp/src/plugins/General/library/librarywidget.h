#ifndef LIBRARYWIDGET_H
#define LIBRARYWIDGET_H

#include <QWidget>

namespace Ui {
class LibraryWidget;
}

class QMenu;
class QAction;
class QContextMenuEvent;
class QCloseEvent;
class QLabel;
class LibraryModel;

class LibraryWidget : public QWidget
{
    Q_OBJECT
public:
    explicit LibraryWidget(bool dialog, QWidget *parent = nullptr);
    ~LibraryWidget();
    void refresh();

    void setBusyMode(bool enabled);

private:
    void closeEvent(QCloseEvent *) override;
    void contextMenuEvent(QContextMenuEvent *e) override;

private slots:
    void on_filterLineEdit_textChanged(const QString &text);
    void addSelected();
    void replaceSelected();
    void showTrackInformation();
    void showLibraryInformation();
    void setArtistView();
    void setMostPlayedView();
    void setRecentlyPlayedView();
    void setUnratedView();

private:
    Ui::LibraryWidget *m_ui;
    LibraryModel *m_model;
    QMenu *m_menu;
    QAction *m_filterAction;
    QLabel *m_busyIndicator = nullptr;
};

#endif // LIBRARYWIDGET_H
