
#include <QApplication>
#include <QStyleFactory>

#include "MainWindow.h"

/*
 =======================================================================================================================
 =======================================================================================================================
 */

int main(int argc, char *argv[])
{
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~*/
	int				iResult;
	QApplication	a(argc, argv);
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~*/

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
