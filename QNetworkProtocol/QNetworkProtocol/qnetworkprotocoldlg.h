#ifndef QNETWORKPROTOCOLDLG_H
#define QNETWORKPROTOCOLDLG_H

#include <QtWidgets/QDialog>
#include "ui_qnetworkprotocoldlg.h"

class QNetworkProtocolDlg : public QDialog
{
	Q_OBJECT

public:
	QNetworkProtocolDlg(QWidget *parent = 0);
	~QNetworkProtocolDlg();

private:
	Ui::QNetworkProtocolDlgClass ui;
};

#endif // QNETWORKPROTOCOLDLG_H
