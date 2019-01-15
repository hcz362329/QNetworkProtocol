#include "Download.h"
#include <QString>
Download::Download(int index, QObject *parent)
	: QObject(parent), m_nIndex(index)
{
	m_HaveDoneBytes = 0;
	m_nStartPoint = 0;
	m_nEndPoint = 0;
	m_pFile = NULL;
}

void Download::StartDownload(const QUrl &url, 
	QFile *file,
	qint64 startPoint/* =0 */,
	qint64 endPoint/* =-1 */)
{
	if( NULL == file )
		return;

	m_HaveDoneBytes = 0;
	m_nStartPoint = startPoint;
	m_nEndPoint = endPoint;
	m_pFile = file;
	m_url = url;
	//根据HTTP协议，写入RANGE头部，说明请求文件的范围
	QNetworkRequest qheader;
	qheader.setUrl(url);
	QString range;
	range.sprintf("Bytes=%lld-%lld", m_nStartPoint, m_nEndPoint);
	qheader.setRawHeader("Range", range.toLocal8Bit());

	//开始下载
	qDebug() << "Part " << m_nIndex << " start download";
	m_pReply = m_pAccessManager.get(QNetworkRequest(qheader));
	connect(m_pReply, SIGNAL(finished()),this, SLOT(FinishedSlot()));
	connect(m_pReply, SIGNAL(readyRead()),this, SLOT(HttpReadyRead()));
}

//下载结束
void Download::FinishedSlot()
{
	bool bSuccess = (m_pReply->error() == QNetworkReply::NoError);
	int statusCode = m_pReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
	if (m_url.scheme().compare(QLatin1String("http"), Qt::CaseInsensitive) == 0 
		|| m_url.scheme().compare(QLatin1String("https"), Qt::CaseInsensitive) == 0)
	{
		bSuccess = bSuccess && (statusCode >= 200 && statusCode < 300);
	}
	if (!bSuccess)
	{
		if (statusCode == 301 || statusCode == 302)
		{//301,302重定向
			const QVariant& redirectionTarget = m_pReply->attribute(QNetworkRequest::RedirectionTargetAttribute);
			if (!redirectionTarget.isNull()) 
			{//如果网址跳转重新请求
				const QUrl& redirectUrl = m_url.resolved(redirectionTarget.toUrl());
				if (redirectUrl.isValid() && redirectUrl != m_url)
				{
					qDebug() << "url:" << m_url.toString() << "redirectUrl:" << redirectUrl.toString();
					m_pReply->abort();
					m_pReply->deleteLater();
					m_pReply = nullptr;

					StartDownload(redirectUrl, m_pFile, m_nStartPoint, m_nEndPoint);
					return;
				}
			}
		}
	}
	else
	{
		if (m_pFile && m_pFile->exists() && m_pFile->isOpen())
		{
			m_pFile->flush();
		}
	}
	
	m_pReply->deleteLater();
	
	qDebug() << "Part" << m_nIndex << "download finished, success:" << bSuccess;
	
	m_pReply->deleteLater();
	m_pFile = nullptr;
	m_pReply = 0;
	qDebug() << "Part " << m_nIndex << " download finished";
	emit DownloadFinished();
}

void Download::HttpReadyRead()
{
	if ( !m_pFile )
		return;

	//将读到的信息写入文件
	QByteArray buffer = m_pReply->readAll();
	m_pFile->seek( m_nStartPoint + m_HaveDoneBytes );
	m_pFile->write(buffer);
	m_HaveDoneBytes += buffer.size();
}

//用阻塞的方式获取下载文件的长度
qint64 DownloadControl::GetFileSize(QUrl url)
{
	QNetworkAccessManager manager;
	qDebug() << "Getting the file size...";
	QEventLoop loop;
	//发出请求，获取目标地址的头部信息
	QNetworkReply *reply = manager.head(QNetworkRequest(url));
	QObject::connect(reply, SIGNAL(finished()), &loop, SLOT(quit()), Qt::DirectConnection);
	loop.exec();
	QVariant var = reply->header(QNetworkRequest::ContentLengthHeader);
	delete reply;
	qint64 size = var.toLongLong();
	qDebug() << "The file size is: " << size;
	return size;
}

DownloadControl::DownloadControl(QObject *parent)
	: QObject(parent)
{
	m_DownloadCount = 0;
	m_FinishedNum = 0;
	m_FileSize = 0;
	m_File = new QFile;
}

void DownloadControl::StartFileDownload(const QString &url, int count)
{
	m_DownloadCount = count;
	m_FinishedNum = 0;
	m_Url = QUrl(url);
	m_FileSize = GetFileSize(m_Url);
	if ( m_FileSize <= 0 )
	{
		return;
	}
	//先获得文件的名字
	QFileInfo fileInfo(m_Url.path());
	QString fileName = fileInfo.fileName();
	if (fileName.isEmpty())
	{
		fileName = "index.html";
	}

	m_File->setFileName(fileName);
	//打开文件
	m_File->open(QIODevice::WriteOnly);

	//将文件分成PointCount段，用异步的方式下载
	qDebug() << "Start download file from " << strUrl;
	for(int i=0; i<m_DownloadCount; i++)
	{
		//先算出每段的开头和结尾（HTTP协议所需要的信息）
		int start = m_FileSize * i / m_DownloadCount;
		int end = m_FileSize * (i+1) / m_DownloadCount;
		if( i != 0 )
			start--;

		//分段下载该文件
		Download * tempDownload = new Download(i+1, this);
		connect(tempDownload, SIGNAL(DownloadFinished()), 
			this, SLOT(SubPartFinished()));
		connect(tempDownload, SIGNAL(DownloadFinished()), 
			tempDownload, SLOT(deleteLater()));
		tempDownload->StartDownload(m_Url, m_File, start, end);
	}
}

void DownloadControl::SubPartFinished()
{
	m_FinishedNum++;
	//如果完成数等于文件段数，则说明文件下载完毕，关闭文件，发生信号
	if( m_FinishedNum == m_DownloadCount )
	{
		m_File->close();
		emit FileDownloadFinished();
		qDebug() << "Download finished";
	}
}