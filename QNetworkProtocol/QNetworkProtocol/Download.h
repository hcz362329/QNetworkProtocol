#include <QtCore>
#include <QtNetwork>

//断点续传功能用于分包进行多线程下载大文件
//多线程下载的线程数
const int PointCount = 5;
//目标文件的地址
const QString strUrl = "http://online.cdn.qianqian.com/qianqian/info/ffaeb0b4503d0ee83768998b03140732.exe";

//用于下载文件（或文件的一部分）
class Download : public QObject
{
	Q_OBJECT
private:
	QNetworkAccessManager m_pAccessManager;
	QNetworkReply *m_pReply;
	QFile *m_pFile;

	const int m_nIndex;
	qint64 m_HaveDoneBytes;
	qint64 m_nStartPoint;
	qint64 m_nEndPoint;
	QUrl m_url;
public:
	Download(int index, QObject *parent = 0);
	void StartDownload(const QUrl &url, QFile *file, 
		qint64 startPoint=0, qint64 endPoint=-1); 
signals:
	void DownloadFinished();

	public slots:
		void FinishedSlot();
		void HttpReadyRead();
};

//用于管理文件的下载
class DownloadControl : public QObject
{
	Q_OBJECT
private:
	int m_DownloadCount;
	int m_FinishedNum;
	int m_FileSize;
	QUrl m_Url;
	QFile *m_File;
public:
	DownloadControl(QObject *parent = 0);
	void StartFileDownload(const QString &url, int count);
	qint64 GetFileSize(QUrl url);
signals:
	void FileDownloadFinished();
	private slots:
		void SubPartFinished();
};