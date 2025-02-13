#include <QApplication>
#include <QDebug>
#include <windows.h>
#include <psapi.h>
#include <shlwapi.h>
#include <QLabel>
#include <QGridLayout>
#include <QListWidget>
#include <QPushButton>
#include <QStringList>
#include <QFileDialog>
#include <QDesktopServices>
#include <tlhelp32.h>
//#include <QQuickView>
//#include <QGuiApplication>
//#include <QQmlApplicationEngine>
//#include <QQmlContext>

QStringList pathList;

DWORD getProcessIdByPath(int index) {
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        return 0;
    }

    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);

    if (Process32First(hSnapshot, &pe32)) {
        do {
            QString exePath = QString::fromWCharArray(pe32.szExeFile);
            if (exePath.compare(QFileInfo(pathList[index]).fileName(), Qt::CaseInsensitive) == 0) {
                CloseHandle(hSnapshot);
                return pe32.th32ProcessID;
            }
        } while (Process32Next(hSnapshot, &pe32));
    }

    CloseHandle(hSnapshot);
    return 0;
}

QWidget *windowInfo = nullptr;

void procList(QListWidget &list){
    DWORD process[1024], cbNeeded, counter;

    if(!EnumProcesses(process, sizeof(process), &cbNeeded)){
        qDebug()<<"ne rabotaet((((";
        return;
    }

    counter = cbNeeded/sizeof(DWORD);

    for(unsigned int i = 0; i<counter; i++){
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, process[i]);
        if (hProcess) {
            PROCESS_MEMORY_COUNTERS memory;
            TCHAR processPath[MAX_PATH];
            if (GetModuleFileNameEx(hProcess, NULL, processPath, sizeof(processPath) / sizeof(TCHAR))) {
                TCHAR* processName = PathFindFileName(processPath);
                if (GetProcessMemoryInfo(hProcess, &memory, sizeof(memory))) {
                    //qDebug() << "Process:" << QString::fromWCharArray(processName)<< "Memory used:" << memory.WorkingSetSize / 1024 << "Kb";
                    QString info = QString("Процесс: %1, Используемая память: %2 Кб").arg(QString::fromWCharArray(processName)).arg(memory.WorkingSetSize / 1024);
                    list.addItem(info);
                    pathList.append(QString::fromWCharArray(processPath));
                }
            }
        CloseHandle(hProcess);
        }
    }
}

void PressStop(int index){

    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, getProcessIdByPath(index));

    if (TerminateProcess(hProcess, 0)) {

        if (windowInfo != nullptr) {
            windowInfo->close();
            delete windowInfo;
        }

        windowInfo = new QWidget();

        QLabel *label = new QLabel("Процесс остановлен!", windowInfo);

        QGridLayout *layout = new QGridLayout(windowInfo);
        layout->addWidget(label,0,0);

        windowInfo->show();
    }
    else {

        windowInfo = new QWidget();

        QLabel *label = new QLabel("Ошибка, процесс не остановлен", windowInfo);

        QGridLayout *layout = new QGridLayout(windowInfo);
        layout->addWidget(label,0,0);

        windowInfo->show();
    }
    CloseHandle(hProcess);
}

void PressPath(int index){

    QWidget windowPath;

    QFileInfo fileInfo(pathList[index]);
    QString dirPath = fileInfo.absolutePath();

    QDesktopServices::openUrl(QUrl::fromLocalFile(dirPath));

    windowPath.show();
}

void PressReset(QListWidget &list){
    list.clear();
    procList(list);
}

void ProcInfo(int index){

    if (windowInfo != nullptr) {
        windowInfo->close();
        delete windowInfo;
    }

    windowInfo = new QWidget();
    windowInfo->setWindowTitle("Информация о процессе");

    QPushButton *buttonPath= new QPushButton(pathList[index], windowInfo);
    QObject::connect(buttonPath, &QPushButton::clicked, [index]() {PressPath(index);});

    QPushButton *buttonStop= new QPushButton("Остановить процесс", windowInfo);
    QObject::connect(buttonStop, &QPushButton::clicked, [index]() {PressStop(index);});

    QGridLayout *layoutInfo = new QGridLayout(windowInfo);
    layoutInfo->addWidget(buttonPath,0,1);
    layoutInfo->addWidget(buttonStop,0,2);

    windowInfo->show();
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    QWidget window;
    window.setWindowTitle("шедевродиспетчер");
    window.setMinimumWidth(1060);
    window.setMinimumHeight(480);

    /*QQuickView *view = new QQuickView();
    view->setSource(QUrl::fromLocalFile("‪C:/Users/XOMA/Documents/testwinapi/graph.qml"));
    view->setResizeMode(QQuickView::SizeRootObjectToView);

    QWidget *container = QWidget::createWindowContainer(view);
    container->setMinimumSize(400, 400);*/

    QListWidget list(&window);
    list.setGeometry(10, 10, 640, 460);
    QObject::connect(&list, &QListWidget::itemDoubleClicked, [&list](QListWidgetItem *item) {
        int index = list.row(item);
        ProcInfo(index);
    });

    QPushButton buttonReset{"Обновить", &window};
    QObject::connect(&buttonReset, &QPushButton::clicked, [&list]() {PressReset(list);});

    QGridLayout *layout = new QGridLayout(&window);
    layout->addWidget(&buttonReset,0,1);
    layout->addWidget(&list,0,2);
    //layout->addWidget(container,1,1);

    procList(list);

    window.show();

    return a.exec();
}
