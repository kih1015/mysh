#include "TextStyleFileExplorer.h"
#include <QApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QKeyEvent>
#include <QListWidget>
#include <QLineEdit>
#include <QStringList>
#include <QPalette>
#include <QTcpSocket>
#include <QHostAddress>
#include <QDebug>
#include <QPushButton>

TextStyleFileExplorer::TextStyleFileExplorer(QWidget* parent) : QWidget(parent) {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    currentPathLabel = new QLabel("/home/user", this);
    currentPathLabel->setAlignment(Qt::AlignCenter);
    currentPathLabel->setStyleSheet("font-weight: bold;");

    fileList = new QListWidget(this);
    fileList->addItems({"file1", "file2", "dir1", "dir2"});
    fileList->setFocusPolicy(Qt::StrongFocus);

    QPalette palette = fileList->palette();
    palette.setColor(QPalette::Base, Qt::black);
    palette.setColor(QPalette::Text, Qt::white);
    fileList->setPalette(palette);

    QHBoxLayout* commandLayout = new QHBoxLayout();
    QLabel* commandLabel = new QLabel("Command: ", this);
    commandInput = new QLineEdit(this);

    commandLayout->addWidget(commandLabel);
    commandLayout->addWidget(commandInput);

    mainLayout->addWidget(currentPathLabel);
    mainLayout->addWidget(fileList);
    mainLayout->addLayout(commandLayout);

    setWindowTitle("Text-Style File Explorer");
    setFixedSize(600, 400);

    commandInput->installEventFilter(this);
    fileList->installEventFilter(this);

    // Setup TCP socket
    socket = new QTcpSocket(this);
    socket->connectToHost(QHostAddress("127.0.0.1"), 8080);
    connect(socket, &QTcpSocket::readyRead, this, &TextStyleFileExplorer::onServerResponse);

    if (!socket->waitForConnected(3000)) {
        qDebug() << "Failed to connect to server:" << socket->errorString();
    } else {
        qDebug() << "Connected to server.";
    }
}

void TextStyleFileExplorer::init() {
    if (socket->write("cd /") == -1) {
        qDebug() << "Failed to send cd command:" << socket->errorString();
    } else if (!socket->waitForReadyRead(3000)) {
        qDebug() << "Timeout while sending cd command.";
    }

    if (socket->write("pwd") == -1) {
        qDebug() << "Failed to send pwd command:" << socket->errorString();
    }
    qDebug() << "Sent to server: pwd";
}

bool TextStyleFileExplorer::eventFilter(QObject* obj, QEvent* event) {
    if (obj == fileList && event->type() == QEvent::KeyPress) {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);

        if (keyEvent->matches(QKeySequence::Copy)) { // Ctrl+C
            handleCopy();
            return true;
        } else if (keyEvent->matches(QKeySequence::Paste)) { // Ctrl+V
            handlePaste();
            return true;
        } else if (keyEvent->key() == Qt::Key_Up) {
            moveSelection(-1);
            return true;
        } else if (keyEvent->key() == Qt::Key_Down) {
            moveSelection(1);
            return true;
        } else if (keyEvent->key() == Qt::Key_Return) {
            handleEnter();
            return true;
        } else if (keyEvent->key() == Qt::Key_Delete) {
            handleDelete();
            return true;
        } else if (keyEvent->key() == Qt::Key_F7) {
            handleCreateFolder();
            return true;
        } else if (keyEvent->key() == Qt::Key_F8) {
            handleCreateFile();
            return true;
        }
    } else if (obj == commandInput && event->type() == QEvent::KeyPress) {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Return) {
            handleCommand();
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}

void TextStyleFileExplorer::moveSelection(int step) {
    int currentRow = fileList->currentRow();
    int newRow = currentRow + step;
    if (newRow >= 0 && newRow < fileList->count()) {
        fileList->setCurrentRow(newRow);
    }
}

void TextStyleFileExplorer::handleEnter() {
    QString selectedItem = fileList->currentItem() ? fileList->currentItem()->text() : "";

    if (selectedItem.isEmpty()) {
        return;
    }

    // 선택된 항목에서 파일/폴더 이름 추출
    QString folderName = selectedItem.section(' ', -1); // 마지막 항목이 파일/폴더 이름
    folderName.remove('\"');

    if (selectedItem.contains("DIR")) { // 폴더인지 확인
        // cd 명령 전송
        QString command = "cd " + folderName;
        qDebug() << command.toUtf8();
        if (socket->write(command.toUtf8()) == -1) {
            qDebug() << "Failed to send cd command:" << socket->errorString();
            return;
        } else if (!socket->waitForReadyRead(3000)) {
            qDebug() << "Timeout while sending cd command.";
            return;
        }
        qDebug() << "Sent to server: cd" << folderName;

        // 현재 디렉토리 요청 (pwd)
        if (socket->write("pwd") == -1) {
            qDebug() << "Failed to send pwd command:" << socket->errorString();
            return;
        }
        qDebug() << "Sent to server: pwd";
    } else {
        qDebug() << "Selected item is not a directory, cannot navigate.";
    }
}

void TextStyleFileExplorer::handleCommand() {
    QString command = commandInput->text();
    if (!command.isEmpty() && socket->state() == QTcpSocket::ConnectedState) {
        socket->write(command.toUtf8());
        qDebug() << "Sent to server:" << command;
        if (!socket->waitForReadyRead(3000)) {
            qDebug() << "Timeout while sending command.";
            return;
        }
        socket->write("pwd");
        qDebug() << "Sent to server:" << "pwd";
    }
    commandInput->clear();
}

void TextStyleFileExplorer::onServerResponse() {
    QByteArray response = socket->readAll();  // 서버로부터 응답 읽기
    qDebug() << "Received from server:" << QString(response);

    if (response.startsWith("/")) {
        // pwd 명령 결과로 간주하고 currentPathLabel 업데이트
        QString currentPath = QString(response).trimmed(); // 개행 및 공백 제거
        currentPathLabel->setText(currentPath);
        qDebug() << "Updated current path:" << currentPath;
    } else {
        // 파일/폴더 리스트 처리
        QStringList fileListData = QString(response).split('\n', Qt::SkipEmptyParts);

        // 기존 파일 리스트 지우기
        fileList->clear();

        // 새 파일 리스트 생성
        for (const QString& entry : fileListData) {
            QStringList fields = entry.split(QRegExp("\\s+"), Qt::SkipEmptyParts);
            if (fields.size() >= 9) {
                QString permissions = fields[1];          // 권한
                QString type = fields[2]; // 폴더/파일 구분
                QString creationTime = fields[5];        // 생성 시간
                QString modificationTime = fields[6];    // 수정 시간
                QString size = fields[9];               // 파일 크기
                QString name = fields[10];               // 파일 이름

                // 폴더/파일 정보를 한 줄로 표시
                QString displayEntry = QString("%1 %2 %3 %4 %5 %6")
                                       .arg(permissions, -10)
                                       .arg(type, -5)
                                       .arg(creationTime, -15)
                                       .arg(modificationTime, -15)
                                       .arg(size, -8)
                                       .arg(name);

                fileList->addItem(displayEntry); // 항목 추가
            }
        }

        fileList->setStyleSheet("");
        fileList->update();

        qDebug() << "Updated file list with detailed information.";
    }
}

void TextStyleFileExplorer::handleDelete() {
    // 선택된 항목 가져오기
    QString selectedItem = fileList->currentItem() ? fileList->currentItem()->text() : "";

    if (selectedItem.isEmpty()) {
        qDebug() << "No item selected for deletion.";
        return;
    }

    // 선택된 항목에서 파일/폴더 이름 추출
    QStringList fields = selectedItem.split(QRegExp("\\s+"), Qt::SkipEmptyParts);
    if (fields.size() < 6) {
        qDebug() << "Invalid selected item format.";
        return;
    }

    QString type = fields[1];  // 파일/폴더 구분 (DIR, FILE 등)
    QString name = fields[5];  // 파일/폴더 이름

    // 명령어 구성
    QString command;
    if (type == "DIR") {
        command = "rmdir " + name + "\n";  // 폴더 삭제
    } else {
        command = "rm " + name + "\n";    // 파일 삭제
    }

    // 서버로 명령 전송
    if (socket->write(command.toUtf8()) == -1) {
        qDebug() << "Failed to send delete command:" << socket->errorString();
    } else if (!socket->waitForBytesWritten(3000)) {
        qDebug() << "Timeout while sending delete command.";
    } else {
        qDebug() << "Sent to server:" << command;
    }

    // // 삭제 후 리스트 업데이트 요청
    // if (socket->write("ls\n") == -1) {
    //     qDebug() << "Failed to request updated file list:" << socket->errorString();
    // }
}

void TextStyleFileExplorer::handleCreateFolder() {
    // 기존 연결 해제
    disconnect(fileList->itemDelegate(), &QAbstractItemDelegate::commitData, this, nullptr);

    // 새 항목 추가
    QListWidgetItem* newItem = new QListWidgetItem("New Folder", fileList);
    newItem->setFlags(newItem->flags() | Qt::ItemIsEditable); // 편집 가능 플래그 설정
    fileList->addItem(newItem);

    // 새 항목을 선택하고 편집 상태로 전환
    fileList->setCurrentItem(newItem);
    fileList->editItem(newItem); // 항목 편집 시작

    // 사용자 입력 완료 후 서버에 전송
    connect(fileList->itemDelegate(), &QAbstractItemDelegate::commitData, this, [=](QWidget* editor) {
        QLineEdit* lineEdit = qobject_cast<QLineEdit*>(editor);
        if (lineEdit) {
            QString folderName = lineEdit->text().trimmed();

            // 이름이 비어 있는 경우 항목 삭제
            if (folderName.isEmpty()) {
                delete newItem;
                return;
            }

            // 이름이 유효하면 서버에 mkdir 명령 전송
            QString command = "mkdir " + folderName + "\n";
            if (socket->write(command.toUtf8()) == -1) {
                qDebug() << "Failed to send mkdir command:" << socket->errorString();
            } else if (!socket->waitForReadyRead(3000)) {
                qDebug() << "Timeout while sending cd command.";
                return;
            }
        }
    });
}

void TextStyleFileExplorer::handleCreateFile() {
    // 기존 연결 해제
    disconnect(fileList->itemDelegate(), &QAbstractItemDelegate::commitData, this, nullptr);

    // 새 항목 추가
    QListWidgetItem* newItem = new QListWidgetItem("New File", fileList);
    newItem->setFlags(newItem->flags() | Qt::ItemIsEditable); // 편집 가능 플래그 설정
    fileList->addItem(newItem);

    // 새 항목을 선택하고 편집 상태로 전환
    fileList->setCurrentItem(newItem);
    fileList->editItem(newItem); // 항목 편집 시작

    // 사용자 입력 완료 후 서버에 전송
    connect(fileList->itemDelegate(), &QAbstractItemDelegate::commitData, this, [=](QWidget* editor) {
        QLineEdit* lineEdit = qobject_cast<QLineEdit*>(editor);
        if (lineEdit) {
            QString folderName = lineEdit->text().trimmed();

            // 이름이 비어 있는 경우 항목 삭제
            if (folderName.isEmpty()) {
                delete newItem;
                return;
            }

            // 이름이 유효하면 서버에 touch 명령 전송
            QString command = "touch " + folderName + "\n";
            if (socket->write(command.toUtf8()) == -1) {
                qDebug() << "Failed to send mkdir command:" << socket->errorString();
            } else if (!socket->waitForReadyRead(3000)) {
                qDebug() << "Timeout while sending cd command.";
                return;
            }
        }
    });
}

void TextStyleFileExplorer::handleCopy() {
    QListWidgetItem* selectedItem = fileList->currentItem();
    if (!selectedItem) {
        qDebug() << "No item selected to copy.";
        return;
    }

    // 선택된 항목의 이름 추출
    QString itemName = selectedItem->text().split(QRegExp("\\s+")).last(); // 파일/폴더 이름만 추출
    QString type = selectedItem->text().split(QRegExp("\\s+"))[1];         // 파일/폴더 타입 추출 (DIR/FILE)

    QString fullPath;
    // 현재 경로와 결합하여 Full Path 생성
    if (currentPathLabel->text() == "/") fullPath = "/" + itemName;
    else fullPath = currentPathLabel->text() + "/" + itemName;

    copiedItem = fullPath; // Full Path 저장

    // 폴더인 경우 플래그 설정
    isDirectory = (type == "DIR");
    qDebug() << "Copied item (full path):" << copiedItem << ", isDirectory:" << isDirectory;
}

void TextStyleFileExplorer::handlePaste() {
    if (copiedItem.isEmpty()) {
        qDebug() << "No item to paste.";
        return;
    }

    // 붙여넣기 대상 이름 생성 (복사본 이름 설정)
    QString copiedBaseName = copiedItem.split('/').last(); // 파일/폴더 이름 추출
    QString newItemName = copiedBaseName + "_copy";        // 기본 복사본 이름
    QString destinationPath;
    if (currentPathLabel->text() == "/") destinationPath = "/" + newItemName;
    else destinationPath = currentPathLabel->text() + "/" + newItemName;

    // 폴더인 경우 -r 옵션 추가
    QString command;
    if (isDirectory) {
        command = QString("cp -r %1 %2").arg(copiedItem, destinationPath);
    } else {
        command = QString("cp %1 %2").arg(copiedItem, destinationPath);
    }

    // 서버에 cp 명령 전송
    if (socket->write(command.toUtf8()) == -1) {
        qDebug() << "Failed to send cp command:" << socket->errorString();
    } else if (!socket->waitForReadyRead(3000)) {
        qDebug() << "Timeout while sending cp command.";
    } else {
        qDebug() << "Sent to server:" << command;
    }
}
