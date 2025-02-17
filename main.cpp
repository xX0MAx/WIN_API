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
#include <QTimer>


boolean autoReset = false;

int counterProcess;

QStringList pathList;

QWidget *windowInfo = nullptr;

QTimer *timer = new QTimer();

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

void procList(QListWidget &list){
    DWORD process[1024], cbNeeded, counter;
    QVector<double> dataList;
    pathList.clear();

    if(!EnumProcesses(process, sizeof(process), &cbNeeded)){
        return;
    }

    counter = cbNeeded/sizeof(DWORD);

    for(unsigned int i = 0; i<counter; i++){
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, process[i]);
        if (hProcess) {
            TCHAR processPath[MAX_PATH];
            if (GetModuleFileNameEx(hProcess, NULL, processPath, sizeof(processPath) / sizeof(TCHAR))){
                if(counterProcess == 1){
                    PROCESS_MEMORY_COUNTERS memory;
                    if (GetProcessMemoryInfo(hProcess, &memory, sizeof(memory))) {
                        dataList.append((memory.WorkingSetSize / 1024.0) / 1024.0);
                    }
                }
                else if(counterProcess == 2){
                    FILETIME creationTime, exitTime, kernelTime, userTime;
                    if (GetProcessTimes(hProcess, &creationTime, &exitTime, &kernelTime, &userTime)) {
                        ULARGE_INTEGER userTimeInt, kernelTimeInt;
                        userTimeInt.LowPart = userTime.dwLowDateTime;
                        userTimeInt.HighPart = userTime.dwHighDateTime;
                        kernelTimeInt.LowPart = kernelTime.dwLowDateTime;
                        kernelTimeInt.HighPart = kernelTime.dwHighDateTime;

                        ULONGLONG totalCpuTime = userTimeInt.QuadPart + kernelTimeInt.QuadPart;

                        FILETIME idleTime, sysKernelTime, sysUserTime;
                        if (GetSystemTimes(&idleTime, &sysKernelTime, &sysUserTime)) {
                            ULARGE_INTEGER sysUserTimeInt, sysKernelTimeInt;
                            sysUserTimeInt.LowPart = sysUserTime.dwLowDateTime;
                            sysUserTimeInt.HighPart = sysUserTime.dwHighDateTime;
                            sysKernelTimeInt.LowPart = sysKernelTime.dwLowDateTime;
                            sysKernelTimeInt.HighPart = sysKernelTime.dwHighDateTime;

                            ULONGLONG totalSystemTime =  sysUserTimeInt.QuadPart + sysKernelTimeInt.QuadPart;

                            double cpuUse = ((double)totalCpuTime / (double)totalSystemTime) * 100.0;

                            dataList.append(cpuUse);
                        }
                    }
                }
                else if(counterProcess == 3){
                    IO_COUNTERS ioCounters;
                    if (GetProcessIoCounters(hProcess, &ioCounters)){
                        dataList.append(((double)ioCounters.WriteTransferCount / 1024.0) / 1024.0);
                    }
                }
            }
            pathList.append(QString::fromWCharArray(processPath));
            CloseHandle(hProcess);
        }
    }
    bool swapped;
    do {
        swapped = false;
        for (int i = 0; i < dataList.size() - 1; i++) {
            if (dataList[i] < dataList[i + 1]) {

                int a = dataList[i];
                dataList[i] = dataList[i + 1];
                dataList[i + 1] = a;

                QString b = pathList[i];
                pathList[i] = pathList[i + 1];
                pathList[i + 1] = b;

                swapped = true;
            }
        }
    } while (swapped);
    for(int i = 0; i<dataList.size();i++){
        QString info;
        if(counterProcess == 1){
            info = QString("Процесс: %1, Используемая память: %2 Мб").arg(QFileInfo(pathList[i]).fileName()).arg(dataList[i]);
        }
        else if (counterProcess == 2){
            info = QString("Процесс: %1, Используемый ЦП: %2%").arg(QFileInfo(pathList[i]).fileName()).arg(dataList[i]);
        }
        else if (counterProcess == 3){
            info = QString("Процесс: %1, Используемый Диск для записи: %2 Мб").arg(QFileInfo(pathList[i]).fileName()).arg(dataList[i]);
        }
        list.addItem(info);
    }
}

void PressMemory(QListWidget &list){
    list.clear();
    counterProcess = 1;
    procList(list);
}

void PressCP(QListWidget &list){
    list.clear();
    counterProcess = 2;
    procList(list);
}

void PressDisk(QListWidget &list){
    list.clear();
    counterProcess = 3;
    procList(list);
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

void PressAutoReset(QListWidget &list){

    if (windowInfo != nullptr) {
        windowInfo->close();
        delete windowInfo;
    }

    autoReset = !autoReset;

    windowInfo = new QWidget();
    windowInfo->setWindowTitle("Автообновление данных");

    QObject::connect(timer, &QTimer::timeout, [&list]() {
        if(counterProcess == 1){
            PressMemory(list);
        }
        else if(counterProcess == 2){
            PressCP(list);
        }
        else if(counterProcess == 3){
            PressDisk(list);
        }
    });

    if(autoReset){
        QLabel *label = new QLabel("Автообновление включено!", windowInfo);

        QGridLayout *layout = new QGridLayout(windowInfo);
        layout->addWidget(label,0,0);

        timer->start(1000);
    }
    else{
        QLabel *label = new QLabel("Автообновление выключено!", windowInfo);

        QGridLayout *layout = new QGridLayout(windowInfo);
        layout->addWidget(label,0,0);

        timer->stop();
    }

    windowInfo->show();
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

    QListWidget list(&window);
    list.setGeometry(10, 10, 640, 460);
    QObject::connect(&list, &QListWidget::itemDoubleClicked, [&list](QListWidgetItem *item) {
        int index = list.row(item);
        ProcInfo(index);
    });

    QPushButton buttonReset{"Автообновление", &window};
    QObject::connect(&buttonReset, &QPushButton::clicked, [&list]() {PressAutoReset(list);});

    QPushButton buttonCP{"ЦП", &window};
    QObject::connect(&buttonCP, &QPushButton::clicked, [&list]() {PressCP(list);});

    QPushButton buttonMemory{"Память", &window};
    QObject::connect(&buttonMemory, &QPushButton::clicked, [&list]() {PressMemory(list);});

    QPushButton buttonDisk{"Диск", &window};
    QObject::connect(&buttonDisk, &QPushButton::clicked, [&list]() {PressDisk(list);});

    QGridLayout *layout = new QGridLayout(&window);
    layout->addWidget(&buttonReset,0,1);
    layout->addWidget(&list,0,2);
    layout->addWidget(&buttonCP,2,1);
    layout->addWidget(&buttonMemory, 3,1);
    layout->addWidget(&buttonDisk, 4,1);

    window.show();

    return a.exec();
}
