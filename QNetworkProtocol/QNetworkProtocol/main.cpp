#include "qnetworkprotocoldlg.h"
#include <QtWidgets/QApplication>
#include "Download.h"
int main(int argc, char *argv[])
{
	QApplication a(argc, argv);
	/*QNetworkProtocolDlg w;
	w.show();*/
	DownloadControl *control = new DownloadControl;
	QEventLoop loop;
	QObject::connect(control, SIGNAL(FileDownloadFinished()), 
		&loop, SLOT(quit()));
	control->StartFileDownload(strUrl, PointCount);
	loop.exec();
	return a.exec();
}
