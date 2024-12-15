#ifndef TEXT_STYLE_FILE_EXPLORER_H
#define TEXT_STYLE_FILE_EXPLORER_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QKeyEvent>
#include <QListWidget>
#include <QLineEdit>
#include <QStringList>
#include <QPalette>
#include <QTcpSocket>

class TextStyleFileExplorer : public QWidget {
public:
    TextStyleFileExplorer(QWidget* parent = nullptr);
    void init();

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    QLabel* currentPathLabel;
    QListWidget* fileList;
    QLineEdit* commandInput;
    QTcpSocket* socket;
    QString copiedItem;

    void moveSelection(int step);
    void handleEnter();
    void handleCommand();
    void handleDelete();
    void handleCreateFolder();
    void handleCreateFile();
    void handleCopy();
    void handlePaste();
    void onServerResponse();
};

#endif // TEXT_STYLE_FILE_EXPLORER_H