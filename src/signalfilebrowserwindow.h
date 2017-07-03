#ifndef SIGNALFILEBROWSERWINDOW_H
#define SIGNALFILEBROWSERWINDOW_H

#include "DataModel/infotable.h"

#include <QMainWindow>

#include <functional>
#include <memory>
#include <vector>

namespace AlenkaFile {
class DataFile;
class DataModel;
}
class AutomaticMontage;
class OpenDataFile;
class SignalViewer;
class TrackManager;
class EventManager;
class EventTypeManager;
class MontageManager;
class FilterManager;
class QComboBox;
class QCheckBox;
class QActionGroup;
class QLabel;
class QAction;
class SpikedetAnalysis;
class SyncServer;
class SyncClient;
class SyncDialog;
class TableModel;
class DataModelVitness;
class QTimer;
class QUndoStack;
class UndoCommandFactory;
class KernelCache;
class QPushButton;
class QQuickWidget;
class QStackedWidget;
class QFileInfo;

/**
 * @brief This class implements the top level window of the program.
 */
class SignalFileBrowserWindow : public QMainWindow {
  Q_OBJECT

  std::unique_ptr<OpenDataFile> openDataFile;
  SignalViewer *signalViewer;
  QQuickWidget *view;
  TrackManager *trackManager;
  EventManager *eventManager;
  EventTypeManager *eventTypeManager;
  MontageManager *montageManager;
  FilterManager *filterManager;
  QComboBox *lowpassComboBox;
  QComboBox *highpassComboBox;
  QCheckBox *notchCheckBox;
  QComboBox *montageComboBox;
  QComboBox *eventTypeComboBox;
  QComboBox *resolutionComboBox;
  QComboBox *unitsComboBox;
  QActionGroup *timeModeActionGroup;
  QActionGroup *timeLineIntervalActionGroup;
  QAction *setTimeLineIntervalAction;
  QLabel *timeModeStatusLabel;
  QLabel *timeStatusLabel;
  QLabel *positionStatusLabel;
  QLabel *cursorStatusLabel;
  std::unique_ptr<SpikedetAnalysis> spikedetAnalysis;
  double spikeDuration;
  bool originalSpikedet;
  std::unique_ptr<SyncServer> syncServer;
  std::unique_ptr<SyncClient> syncClient;
  SyncDialog *syncDialog;
  const int lastPositionReceivedDefault = -1000'000'000;
  int lastPositionReceived = lastPositionReceivedDefault;
  QAction *synchronize;
  std::vector<QMetaObject::Connection> openFileConnections;
  std::vector<QMetaObject::Connection> managersConnections;
  QTimer *autoSaveTimer;
  std::string autoSaveName;
  QUndoStack *undoStack;
  std::unique_ptr<KernelCache> kernelCache;
  QAction *saveFileAction;
  QAction *closeFileAction;
  QAction *exportToEdfAction;
  QAction *runSpikedetAction;
  bool allowSaveOnClean;
  QPushButton *switchButton;
  QByteArray windowState, windowGeometry;
  QStackedWidget *stackedWidget;
  const int COMBO_PRECISION = 2, RECENT_FILE_COUNT = 10;
  int nameIndex = 0;
  QMenu *fileMenu;
  std::vector<QAction *> recentFilesActions;
  std::vector<std::unique_ptr<AutomaticMontage>> autoMontages;

  struct OpenFileResources {
    std::unique_ptr<AlenkaFile::DataFile> file;
    std::unique_ptr<AlenkaFile::DataModel> dataModel;
    std::unique_ptr<UndoCommandFactory> undoFactory;

    std::unique_ptr<TableModel> eventTypeTable;
    std::unique_ptr<TableModel> montageTable;
    std::unique_ptr<TableModel> eventTable;
    std::unique_ptr<TableModel> trackTable;
  };
  std::unique_ptr<OpenFileResources> fileResources;

public:
  explicit SignalFileBrowserWindow(QWidget *parent = nullptr);
  ~SignalFileBrowserWindow() override;

  void openCommandLineFile();

  static QDateTime sampleToDate(AlenkaFile::DataFile *file, int sample);
  static QDateTime sampleToOffset(AlenkaFile::DataFile *file, int sample);
  static QString
  sampleToDateTimeString(AlenkaFile::DataFile *file, int sample,
                         InfoTable::TimeMode mode = InfoTable::TimeMode::size);
  static std::unique_ptr<AlenkaFile::DataFile>
  dataFileBySuffix(const QFileInfo &fileInfo,
                   const std::vector<std::string> &additionalFiles);

protected:
  void closeEvent(QCloseEvent *event) override;

private:
  std::vector<QMetaObject::Connection>
  connectVitness(const DataModelVitness *vitness, std::function<void()> f);
  void mode(int m);
  bool shouldSynchronizeView();
  void deleteAutoSave();
  void setCurrentInNumericCombo(QComboBox *combo, double value);
  void sortInLastItem(QComboBox *combo);
  QString imageFilePathDialog();
  void setSecondsPerPage(double seconds);
  void copyDefaultMontage();
  void addRecentFilesActions();
  void updateRecentFiles(const QFileInfo &fileInfo);
  void addAutoMontage(AutomaticMontage *autoMontage);

private slots:
  void openFile();
  void openFile(const QString &fileName,
                const std::vector<std::string> &additionalFiles =
                    std::vector<std::string>());
  bool closeFile();
  void saveFile();
  void exportToEdf();
  void lowpassComboBoxUpdate(const QString &text);
  void lowpassComboBoxUpdate(bool on);
  void lowpassComboBoxUpdate(double value);
  void highpassComboBoxUpdate(const QString &text);
  void highpassComboBoxUpdate(bool on);
  void highpassComboBoxUpdate(double value);
  void resolutionComboBoxUpdate(const QString &text);
  void resolutionComboBoxUpdate(float value);
  void updateManagers(int value);
  void updateTimeMode(InfoTable::TimeMode mode);
  void updatePositionStatusLabel();
  void updateCursorStatusLabel();
  void updateMontageComboBox();
  void updateEventTypeComboBox();
  void runSpikedet();
  void receiveSyncMessage(const QByteArray &message);
  void sendSyncMessage();
  void cleanChanged(bool clean);
  void closeFilePropagate();
  void setEnableFileActions(bool enable);
  void setFilePathInQML();
  void switchToAlenka();
  void verticalZoomIn();
  void verticalZoomOut();
  void exportDialog();
};

#endif // SIGNALFILEBROWSERWINDOW_H
