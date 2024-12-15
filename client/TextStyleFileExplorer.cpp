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
#include <QDateTime>

TextStyleFileExplorer::TextStyleFileExplorer(QWidget* parent) : QWidget(parent) {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // 현재 경로 레이블 (Working Directory 포함)
    QVBoxLayout* pathLayout = new QVBoxLayout();
    QLabel* workingDirLabel = new QLabel("Working Directory: ", this);
    workingDirLabel->setStyleSheet("font-weight: bold; font-size: 14px;");
    currentPathLabel = new QLabel("/", this);
    currentPathLabel->setAlignment(Qt::AlignLeft);
    currentPathLabel->setStyleSheet("font-size: 14px;");

    pathLayout->addWidget(workingDirLabel);
    pathLayout->addWidget(currentPathLabel);
    mainLayout->addLayout(pathLayout);

    // 파일 리스트
    fileList = new QListWidget(this);
    fileList->setFocusPolicy(Qt::StrongFocus);

    QPalette palette = fileList->palette();
    palette.setColor(QPalette::Base, Qt::black);
    palette.setColor(QPalette::Text, Qt::white);
    fileList->setPalette(palette);

    mainLayout->addWidget(currentPathLabel);
    mainLayout->addWidget(fileList);

    // 명령어 박스 추가
    QFrame* commandBox = new QFrame(this);
    commandBox->setFrameShape(QFrame::Box);
    commandBox->setFrameShadow(QFrame::Raised);
    commandBox->setStyleSheet("background-color: #f0f0f0; border: 2px solid #d0d0d0;");

    QVBoxLayout* commandBoxLayout = new QVBoxLayout(commandBox);
    QLabel* commandTitle = new QLabel("Commands", commandBox);
    commandTitle->setStyleSheet("font-size: 12pt; font-weight: bold; color: black;");
    commandTitle->setAlignment(Qt::AlignCenter);

    QLabel* commandDetails = new QLabel(commandBox);
    commandDetails->setStyleSheet("font-size: 10pt; color: #333333;");
    commandDetails->setAlignment(Qt::AlignLeft);
    commandDetails->setText(
        "[Ctrl+C: Copy]    [Ctrl+V: Paste]    [F1: Create Folder]    [F2: Create File]\n"
        "[F3: Change Permission]    [F4: Run Process]    [F5: Show Process List]\n"
        "[F6: Soft Link]    [F7: Hard Link]    [Del: Delete]    [Home: Go to Root]\n"
        "[ESC: Refresh Directory]    [End: Kill Process]    [Enter: Change Directory or Open File]" 
    );

    commandBoxLayout->addWidget(commandTitle);
    commandBoxLayout->addWidget(commandDetails);
    mainLayout->addWidget(commandBox);

    // 기본 창 설정
    setWindowTitle("Text-Style File Explorer");
    setFixedSize(620, 500); // 명령어 박스 추가로 창 크기 조정

    // 이벤트 필터 설정
    fileList->installEventFilter(this);

    // TCP 소켓 설정
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
    QFont fixedFont("Courier New"); // 고정 폭 글꼴 지정
    fixedFont.setStyleHint(QFont::TypeWriter); // 고정 폭 힌트 설정
    fileList->setFont(fixedFont); // QListWidget에 글꼴 적용

    if (socket->write("cd /") == -1) {
        qDebug() << "Failed to send cd command:" << socket->errorString();
    } else if (!socket->waitForReadyRead(3000)) {
        qDebug() << "Timeout while sending ls command.";
    }
    if (socket->write("ls") == -1) {
        qDebug() << "Failed to send ls command:" << socket->errorString();
    } else if (!socket->waitForReadyRead(3000)) {
        qDebug() << "Timeout while sending ls command.";
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
            handleDelete(); // rm
            return true;
        } else if (keyEvent->key() == Qt::Key_F1) {
            handleCreateFolder(); // mkdir
            return true;
        } else if (keyEvent->key() == Qt::Key_F2) {
            handleCreateFile(); // touch
            return true;
        } else if (keyEvent->key() == Qt::Key_Escape) { // ESC 키 처리
            handleRefreshDirectory(); // ls 명령 실행
            return true;
        } else if (keyEvent->key() == Qt::Key_F5) {
            handleShowProcessList(); // ps
            return true;
        } else if (keyEvent->key() == Qt::Key_F4) {
            handleRunProcess(); // exec
            return true;
        } else if (keyEvent->key() == Qt::Key_End) { // End 키 처리
            handleKillProcess(); // kill
            return true;
        } else if (keyEvent->key() == Qt::Key_F3) { // F6 키 처리
            handleChangePermission(); // chmod
            return true;
        } else if (keyEvent->key() == Qt::Key_F6) { // 소프트 링크 생성
            handleCreateSoftLink();
            return true;
        } else if (keyEvent->key() == Qt::Key_F7) { // 하드 링크 생성
            handleCreateHardLink();
            return true;
        } else if (keyEvent->key() == Qt::Key_Home) { // Home 키 처리
            handleGoToRootDirectory();
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
        if (socket->write(command.toUtf8()) == -1) {
            qDebug() << "Failed to send cd command:" << socket->errorString();
            return;
        } else if (!socket->waitForReadyRead(3000)) {
            qDebug() << "Timeout while sending ls command.";
        }
        qDebug() << "Sent to server: cd" << folderName;

        if (socket->write("ls") == -1) {
            qDebug() << "Failed to send ls command:" << socket->errorString();
        } else if (!socket->waitForReadyRead(3000)) {
            qDebug() << "Timeout while sending ls command.";
        }
    } else {
        QString command = "cat " + folderName; // 파일 내용 읽기 명령 (cat 사용)
        qDebug() << "Sending to server: " << command;
        if (socket->write(command.toUtf8()) == -1) {
            qDebug() << "Failed to send file read command:" << socket->errorString();
            return;
        } else if (!socket->waitForReadyRead(3000)) {
            qDebug() << "Timeout while sending cat command.";
        }
    }
}

void TextStyleFileExplorer::onServerResponse() {
    QByteArray response = socket->readAll();  // 서버로부터 응답 읽기
    qDebug() << "Received from server:" << QString(response);

    if (response.startsWith("/")) {
        // pwd 명령 결과로 간주하고 currentPathLabel 업데이트
        QString currentPath = QString(response).trimmed(); // 개행 및 공백 제거
        currentPathLabel->setText(currentPath);
        qDebug() << "Updated current path:" << currentPath;
    } else if (response.startsWith("FILE_CONTENT_START:")) {
        QString fileName = QString(response).section(':', 1, 1).trimmed();
        qDebug() << "Receiving content for file:" << fileName;

        // Extract and process file content
        QString fileContent = QString(response).section('\n', 1).trimmed();
        qDebug() << "File content:" << fileContent;

        fileList->clear();
        QStringList fileLines = QString(fileContent).split('\n', Qt::SkipEmptyParts);
        for (const QString& line : fileLines) {
            fileList->addItem(QString(line));
        }
        fileList->addItem(QString("--- Press ESC to go back ---")); // 안내 메시지 추가

        fileList->setStyleSheet("");
        fileList->update();
    } else if (response.startsWith("error")) {
        ;
    } else if (response.startsWith("PROCESS_LIST_START")) {
        // 프로세스 목록 처리
        QString processList = QString(response).section('\n', 1).trimmed(); // 첫 줄 이후 데이터
        qDebug() << "Process list received:\n" << processList;

        // 프로세스 목록을 fileList에 출력
        fileList->clear();

        // 헤더 추가
        QString header = QString("%1 %2 %3")
                            .arg("PID", -8)   // 왼쪽 정렬, 8자리
                            .arg("PPID", -8)  // 왼쪽 정렬, 8자리
                            .arg("CMD");
        fileList->addItem(header);
        fileList->addItem(QString("=").repeated(header.length())); // 구분선 추가

        // 프로세스 목록 파싱 및 출력
        QStringList processLines = processList.split('\n', Qt::SkipEmptyParts);
        for (const QString& line : processLines) {
            QStringList fields = line.split(QRegExp("\\s+"), Qt::SkipEmptyParts);
            if (fields.size() >= 3) {
                QString pid = fields[0];
                QString ppid = fields[1];
                QString cmd = fields.mid(2).join(" "); // 나머지 필드를 CMD로 처리

                // 정렬된 포맷으로 출력
                QString formattedLine = QString("%1 %2 %3")
                                            .arg(pid, -8)   // PID, 왼쪽 정렬 8자리
                                            .arg(ppid, -8)  // PPID, 왼쪽 정렬 8자리
                                            .arg(cmd);
                fileList->addItem(formattedLine);
            }
        }

        // 안내 메시지 추가
        fileList->addItem("--- Press ESC to go back ---");

        fileList->setStyleSheet("");
        fileList->update();
    } else {
        // 파일/폴더 리스트 처리
        QStringList fileListData = QString(response).split('\n', Qt::SkipEmptyParts);

        // 기존 파일 리스트 지우기
        fileList->clear();

        // 이름순 정렬을 위한 리스트 생성
        QList<QPair<QString, QString>> sortedList;

        for (const QString& entry : fileListData) {
            QStringList fields = entry.split(QRegExp("\\s+"), Qt::SkipEmptyParts);
            if (fields.size() >= 9) {
                QString permissions = fields[1];          // 권한
                QString type = fields[2];                 // 폴더/파일 구분
                qint64 modificationTimeSec = fields[6].toLongLong(); // 수정 시간 (초 단위)
                QString size = fields[9];                 // 파일 크기
                QString name = fields[10];                // 파일 이름

                // 초 단위를 yyyy-MM-dd hh:mm 형식으로 변환
                QString modificationTime = QDateTime::fromSecsSinceEpoch(modificationTimeSec)
                                            .toString("yyyy-MM-dd hh:mm");

                // 폴더/파일 정보를 한 줄로 표시
                QString displayEntry = QString("%1 %2 %3 %4 %5")
                                        .arg(permissions, -10)
                                        .arg(type, -5)
                                        .arg(modificationTime, -15)
                                        .arg(size, -8)
                                        .arg(name);

                // 이름과 디스플레이 엔트리를 페어로 추가
                sortedList.append(qMakePair(name, displayEntry));
            }
        }

        // 이름순 정렬 (폴더 우선, 이름순 정렬)
        std::sort(sortedList.begin(), sortedList.end(), [](const QPair<QString, QString>& a, const QPair<QString, QString>& b) {
            // 첫 번째 요소가 DIR인지 확인
            bool isDirA = a.second.contains("DIR");
            bool isDirB = b.second.contains("DIR");

            // 폴더는 먼저 오도록 정렬
            if (isDirA && !isDirB) {
                return true; // a가 폴더고 b는 폴더가 아니면 a가 먼저
            } else if (!isDirA && isDirB) {
                return false; // b가 폴더고 a는 폴더가 아니면 b가 먼저
            }

            // 둘 다 폴더이거나 둘 다 파일이면 이름순으로 정렬
            return a.first < b.first;
        });

        // 정렬된 리스트를 QListWidget에 추가
        for (const auto& pair : sortedList) {
            fileList->addItem(pair.second); // 정렬된 항목 추가
        }

        fileList->setStyleSheet("");
        fileList->update();

        qDebug() << "Updated file list with detailed information (sorted by name).";

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

    if (socket->write("ls") == -1) {
        qDebug() << "Failed to send ls command:" << socket->errorString();
    } else if (!socket->waitForReadyRead(3000)) {
        qDebug() << "Timeout while sending ls command.";
    }
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
            } else if (!socket->waitForBytesWritten(3000)) {
                qDebug() << "Timeout while sending cd command.";
                return;
            }

            if (socket->write("ls") == -1) {
                qDebug() << "Failed to send ls command:" << socket->errorString();
            } else if (!socket->waitForReadyRead(3000)) {
                qDebug() << "Timeout while sending ls command.";
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
            } else if (!socket->waitForBytesWritten(3000)) {
                qDebug() << "Timeout while sending cd command.";
                return;
            }

            if (socket->write("ls") == -1) {
                qDebug() << "Failed to send ls command:" << socket->errorString();
            } else if (!socket->waitForReadyRead(3000)) {
                qDebug() << "Timeout while sending ls command.";
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
    } else if (!socket->waitForBytesWritten(3000)) {
        qDebug() << "Timeout while sending ls command.";
    } else {
        qDebug() << "Sent to server:" << command;
    }

    if (socket->write("ls") == -1) {
        qDebug() << "Failed to send ls command:" << socket->errorString();
    } else if (!socket->waitForReadyRead(3000)) {
        qDebug() << "Timeout while sending ls command.";
    }
}

void TextStyleFileExplorer::handleRefreshDirectory() {
    // 서버에 ls 명령 전송
    if (socket->write("ls\n") == -1) {
        qDebug() << "Failed to send ls command:" << socket->errorString();
        return;
    }
    if (!socket->waitForBytesWritten(3000)) {
        qDebug() << "Timeout while sending ls command.";
        return;
    }
    qDebug() << "Sent to server: ls";
}

void TextStyleFileExplorer::handleShowProcessList() {
    // 서버에 ps 명령 전송
    if (socket->write("ps\n") == -1) {
        qDebug() << "Failed to send ps command:" << socket->errorString();
        return;
    }
    if (!socket->waitForBytesWritten(3000)) {
        qDebug() << "Timeout while sending ps command.";
        return;
    }
    qDebug() << "Sent to server: ps";
}

void TextStyleFileExplorer::handleRunProcess() {
    // 선택된 파일 가져오기
    QListWidgetItem* selectedItem = fileList->currentItem();
    if (!selectedItem) {
        qDebug() << "No file selected to run.";
        return;
    }

    // 선택된 항목에서 파일 이름 추출
    QString fileName = selectedItem->text().split(QRegExp("\\s+")).last(); // 마지막 필드가 파일 이름
    fileName.remove('\"'); // 따옴표 제거

    if (fileName.isEmpty()) {
        qDebug() << "Invalid file name.";
        return;
    }

    // 서버에 exec 명령 전송
    QString command = QString("exec %1").arg(fileName);
    if (socket->write(command.toUtf8()) == -1) {
        qDebug() << "Failed to send exec command:" << socket->errorString();
        return;
    }

    if (!socket->waitForBytesWritten(3000)) {
        qDebug() << "Timeout while sending exec command.";
        return;
    }

    qDebug() << "Sent to server: exec" << fileName;
}

void TextStyleFileExplorer::handleKillProcess() {
    // 현재 선택된 항목 가져오기
    QListWidgetItem* selectedItem = fileList->currentItem();
    if (!selectedItem) {
        qDebug() << "No process selected to kill.";
        return;
    }

    // PID 추출
    QStringList fields = selectedItem->text().split(QRegExp("\\s+"), Qt::SkipEmptyParts);
    if (fields.size() >= 1) {
        QString pid = fields[0]; // 첫 번째 필드는 PID

        // 서버에 kill 명령 전송
        QString command = QString("kill %1\n").arg(pid);
        if (socket->write(command.toUtf8()) == -1) {
            qDebug() << "Failed to send kill command:" << socket->errorString();
            return;
        }
        if (!socket->waitForBytesWritten(3000)) {
            qDebug() << "Timeout while sending kill command.";
            return;
        }
        qDebug() << "Sent to server: kill" << pid;

        if (socket->write("ps\n") == -1) {
        qDebug() << "Failed to send ps command:" << socket->errorString();
        return;
        }
        if (!socket->waitForBytesWritten(3000)) {
            qDebug() << "Timeout while sending ps command.";
            return;
        }
        qDebug() << "Sent to server: ps";
       
    } else {
        qDebug() << "Invalid process entry format.";
    }
}

void TextStyleFileExplorer::handleChangePermission() {
    // 기존 연결 해제
    disconnect(fileList->itemDelegate(), &QAbstractItemDelegate::commitData, this, nullptr);
    // 현재 선택된 파일/폴더 가져오기
    QListWidgetItem* selectedItem = fileList->currentItem();
    if (!selectedItem) {
        qDebug() << "No item selected to change permission.";
        return;
    }

    // 선택된 항목에서 파일/폴더 이름 추출
    QString itemName = selectedItem->text().split(QRegExp("\\s+")).last(); // 마지막 필드가 파일 이름
    itemName.remove('\"'); // 따옴표 제거

    if (itemName.isEmpty()) {
        qDebug() << "Invalid item name.";
        return;
    }

    // 새 편집 가능한 아이템 생성
    QListWidgetItem* editItem = new QListWidgetItem("Enter new permissions (e.g., 777)", fileList);
    editItem->setFlags(editItem->flags() | Qt::ItemIsEditable); // 편집 가능 플래그 설정
    fileList->addItem(editItem);

    // 새 항목을 선택하고 편집 상태로 전환
    fileList->setCurrentItem(editItem);
    fileList->editItem(editItem);

    // 사용자 입력 완료 후 처리
    connect(fileList->itemDelegate(), &QAbstractItemDelegate::commitData, this, [=](QWidget* editor) {
        QLineEdit* lineEdit = qobject_cast<QLineEdit*>(editor);
        if (lineEdit) {
            QString permission = lineEdit->text().trimmed();

            // 유효한 권한 값인지 확인 (예: 777)
            QRegExp regex("^[0-7]{3}$");
            if (!regex.exactMatch(permission)) {
                fileList->removeItemWidget(editItem);
                delete editItem;
                return;
            }

            // 서버에 chmod 명령 전송
            QString command = QString("chmod %1 %2\n").arg(permission).arg(itemName);
            if (socket->write(command.toUtf8()) == -1) {
                qDebug() << "Failed to send chmod command:" << socket->errorString();
                return;
            }
            if (!socket->waitForBytesWritten(3000)) {
                qDebug() << "Timeout while sending chmod command.";
                return;
            }

            qDebug() << "Sent to server: chmod" << permission << itemName;

            if (socket->write("ls") == -1) {
                qDebug() << "Failed to send chmod command:" << socket->errorString();
                return;
            }
            if (!socket->waitForReadyRead(3000)) {
                qDebug() << "Timeout while sending chmod command.";
                return;
            }
        }
        fileList->removeItemWidget(editItem);
        delete editItem;
    });
}

void TextStyleFileExplorer::handleCreateSoftLink() {
    QListWidgetItem* selectedItem = fileList->currentItem();
    if (!selectedItem) {
        qDebug() << "No file selected to create a soft link.";
        return;
    }

    QString targetFile = selectedItem->text().split(QRegExp("\\s+")).last(); // 파일/폴더 이름
    targetFile.remove('\"');

    if (targetFile.isEmpty()) {
        qDebug() << "Invalid file name.";
        return;
    }

    // 사용자에게 소프트 링크 이름 입력받기
    bool ok;
    QString linkName = targetFile + ".soft";

    if (ok && !linkName.trimmed().isEmpty()) {
        // 서버에 소프트 링크 생성 명령 전송
        QString command = QString("ln -s %1 %2\n").arg(targetFile).arg(linkName.trimmed());
        if (socket->write(command.toUtf8()) == -1) {
            qDebug() << "Failed to send soft link command:" << socket->errorString();
            return;
        }
        if (!socket->waitForBytesWritten(3000)) {
            qDebug() << "Timeout while sending soft link command.";
            return;
        }

        qDebug() << "Sent to server: ln -s" << targetFile << linkName;
        handleRefreshDirectory();
    }
}

void TextStyleFileExplorer::handleCreateHardLink() {
    QListWidgetItem* selectedItem = fileList->currentItem();
    if (!selectedItem) {
        qDebug() << "No file selected to create a hard link.";
        return;
    }

    QString targetFile = selectedItem->text().split(QRegExp("\\s+")).last(); // 파일/폴더 이름
    targetFile.remove('\"');

    if (targetFile.isEmpty()) {
        qDebug() << "Invalid file name.";
        return;
    }

    // 사용자에게 하드 링크 이름 입력받기
    bool ok;
    QString linkName = targetFile + ".hard";

    if (ok && !linkName.trimmed().isEmpty()) {
        // 서버에 하드 링크 생성 명령 전송
        QString command = QString("ln %1 %2\n").arg(targetFile).arg(linkName.trimmed());
        if (socket->write(command.toUtf8()) == -1) {
            qDebug() << "Failed to send hard link command:" << socket->errorString();
            return;
        }
        if (!socket->waitForBytesWritten(3000)) {
            qDebug() << "Timeout while sending hard link command.";
            return;
        }

        qDebug() << "Sent to server: ln" << targetFile << linkName;
        handleRefreshDirectory();
    }
}

void TextStyleFileExplorer::handleGoToRootDirectory() {
    QString command = "cd /\n";

    // 서버에 cd / 명령 전송
    if (socket->write(command.toUtf8()) == -1) {
        qDebug() << "Failed to send cd / command:" << socket->errorString();
        return;
    }

    if (!socket->waitForReadyRead(3000)) {
        qDebug() << "Timeout while sending cd / command.";
        return;
    }

    qDebug() << "Sent to server: cd /";
    handleRefreshDirectory();
}
