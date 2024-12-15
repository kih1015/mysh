#include <iostream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <QApplication>
#include "TextStyleFileExplorer.h"

#define PORT 8080
#define BUFFER_SIZE 1024

int main(int argc, char **argv) {

    QApplication app(argc, argv);

    TextStyleFileExplorer explorer;
    explorer.show();
    explorer.init();

    return app.exec();
}
