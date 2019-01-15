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
	//����HTTPЭ�飬д��RANGEͷ����˵�������ļ��ķ�Χ
	QNetworkRequest qheader;
	qheader.setUrl(url);
	QString range;
	range.sprintf("Bytes=%lld-%lld", m_nStartPoint, m_nEndPoint);
	qheader.setRawHeader("Range", range.toLocal8Bit());

	//��ʼ����
	qDebug() << "Part " << m_nIndex << " start download";
	m_pReply = m_pAccessManager.get(QNetworkRequest(qheader));
	connect(m_pReply, SIGNAL(finished()),this, SLOT(FinishedSlot()));
	connect(m_pReply, SIGNAL(readyRead()),this, SLOT(HttpReadyRead()));
}

//���ؽ���
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
		{//301,302�ض���
			const QVariant& redirectionTarget = m_pReply->attribute(QNetworkRequest::RedirectionTargetAttribute);
			if (!redirectionTarget.isNull()) 
			{//�����ַ��ת��������
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

	//����������Ϣд���ļ�
	QByteArray buffer = m_pReply->readAll();
	m_pFile->seek( m_nStartPoint + m_HaveDoneBytes );
	m_pFile->write(buffer);
	m_HaveDoneBytes += buffer.size();
}

//�������ķ�ʽ��ȡ�����ļ��ĳ���
qint64 DownloadControl::GetFileSize(QUrl url)
{
	QNetworkAccessManager manager;
	qDebug() << "Getting the file size...";
	QEventLoop loop;
	//�������󣬻�ȡĿ���ַ��ͷ����Ϣ
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
	//�Ȼ���ļ�������
	QFileInfo fileInfo(m_Url.path());
	QString fileName = fileInfo.fileName();
	if (fileName.isEmpty())
	{
		fileName = "index.html";
	}

	m_File->setFileName(fileName);
	//���ļ�
	m_File->open(QIODevice::WriteOnly);

	//���ļ��ֳ�PointCount�Σ����첽�ķ�ʽ����
	qDebug() << "Start download file from " << strUrl;
	for(int i=0; i<m_DownloadCount; i++)
	{
		//�����ÿ�εĿ�ͷ�ͽ�β��HTTPЭ������Ҫ����Ϣ��
		int start = m_FileSize * i / m_DownloadCount;
		int end = m_FileSize * (i+1) / m_DownloadCount;
		if( i != 0 )
			start--;

		//�ֶ����ظ��ļ�
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
	//�������������ļ���������˵���ļ�������ϣ��ر��ļ��������ź�
	if( m_FinishedNum == m_DownloadCount )
	{
		m_File->close();
		emit FileDownloadFinished();
		qDebug() << "Download finished";
	}
}