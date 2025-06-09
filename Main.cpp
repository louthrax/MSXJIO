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

    QCoreApplication::setOrganizationName("louthrax");
    QCoreApplication::setOrganizationDomain("louthrax.net");
    QCoreApplication::setApplicationName("JIOServer");

    QFontDatabase::addApplicationFont(":/fonts/Ubuntu-R.ttf");
    QFontDatabase::addApplicationFont(":/fonts/UbuntuMono-R.ttf");

	/*~~~~~~~~~~*/
	MainWindow	w;
	/*~~~~~~~~~~*/

	QApplication::setStyle(QStyleFactory::create("Fusion"));
	a.setPalette(QApplication::style()->standardPalette());
    QApplication::setFont(QFont("Ubuntu", APPLICATION_FONT_SIZE));

	if(argc <= 1)
	{
		w.show();
	}

	iResult = a.exec();
	return iResult;
}
