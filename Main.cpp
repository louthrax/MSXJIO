#include <QApplication>
#include <QStyleFactory>
#include <QFontDatabase>

#include "MainWindow.h"

/*
 =======================================================================================================================
 =======================================================================================================================
 */

int main(int argc, char *argv[])
{
	/*~~~~~~~~*/
	int iResult;
	/*~~~~~~~~*/

    qputenv("QT_QPA_FONTDIR", QByteArray("."));

	/*~~~~~~~~~~~~~~~~~~~~~~~~~~*/
	QApplication	a(argc, argv);
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~*/

	QFontDatabase::addApplicationFont(":/fonts/DejaVuSans.ttf");
	QFontDatabase::addApplicationFont(":/fonts/DejaVuSansMono.ttf");

    QApplication::setFont(QFont("DejaVu Sans", APPLICATION_FONT_SIZE));

	/*~~~~~~~~~~*/
	MainWindow	w;
	/*~~~~~~~~~~*/

	QApplication::setStyle(QStyleFactory::create("Fusion"));
	a.setPalette(QApplication::style()->standardPalette());

	if(argc <= 1)
	{
		w.show();
	}

	iResult = a.exec();
	return iResult;
}
