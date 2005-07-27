// OpenGL
#include <gl.h>

// Qt
#include <QApplication>
#include <QDesktopWidget>
#include <QGLWidget>
#include <QHBoxLayout>
#include <QMainWindow>
#include <QMenuBar>
#include <QMenu>
#include <QMessageBox>
#include <QSettings>

// Self
#include "GLWidget.h"
#include "MainWindow.h"
#include "PwMovieTools.h"
#include "PwMoviePlayer.h"

//===========================================================================
// MainWindow
//===========================================================================

//---------------------------------------------------------------------------
// MainWindow::MainWindow
//---------------------------------------------------------------------------
MainWindow::MainWindow(const char* windowTitle, const char* windowSettingsNameParam, const Qt::WFlags windowFlags, FILE* movieFileParam )
	:	QMainWindow( NULL, windowFlags )
{
	setWindowTitle( windowTitle );
	windowSettingsName = windowSettingsNameParam;
	movieFile = movieFileParam;
	
	// Read the movieFile header information (version, width, height)
	// Must be done *before* the RestoreFromPrefs() and the OpenGL setup,
	// because the movie dimensions define the window dimensions for these movie windows
	ReadMovieFileHeader();
	
	// Display the main simulation window
	RestoreFromPrefs();

	// Set up the OpenGL view
	glWidget = new GLWidget( this, movieWidth, movieHeight, movieVersion, movieFile );
	setCentralWidget( glWidget );

	// Create the main menubar
	CreateMenus( menuBar() );
}


//---------------------------------------------------------------------------
// MainWindow::~MainWindow
//---------------------------------------------------------------------------
MainWindow::~MainWindow()
{
	SaveWindowState();
}


//---------------------------------------------------------------------------
// MainWindow::ReadMovieFileHeader
//---------------------------------------------------------------------------
void MainWindow::ReadMovieFileHeader()
{
	PwReadMovieHeader( movieFile, &movieVersion, &movieWidth, &movieHeight );
}


//---------------------------------------------------------------------------
// MainWindow::NextFrame()
//---------------------------------------------------------------------------
void MainWindow::NextFrame()
{
#if 0
	static unsigned long frame;
	frame++;
	if( frame == 3 )
	{
		glWidget->dumpObjectTree();
		glWidget->dumpObjectInfo();
	}
#endif
	glWidget->Draw();
}

#if 0
//---------------------------------------------------------------------------
// MainWindow::closeEvent
//---------------------------------------------------------------------------
void MainWindow::closeEvent(QCloseEvent* ce)
{
	ce->accept();
}
#endif

#pragma mark -

//---------------------------------------------------------------------------
// MainWindow::AddFileMenu
//---------------------------------------------------------------------------
void MainWindow::CreateMenus( QMenuBar* menuBar )
{
	AddFileMenu( menuBar );
	AddHelpMenu( menuBar );
}

//---------------------------------------------------------------------------
// MainWindow::AddFileMenu
//---------------------------------------------------------------------------
void MainWindow::AddFileMenu( QMenuBar* menuBar )
{
    QMenu* menu = new QMenu( tr("&File"), this );
    menuBar->addMenu( menu );
	openAct = menu->addAction( tr("&Open..."), this, SLOT(openFile()), Qt::CTRL+Qt::Key_O );
    menu->addSeparator();
    closeAct = menu->addAction( tr("&Close"), this, SLOT(close()), Qt::CTRL+Qt::Key_W );
    quitAct = menu->addAction( tr("&Quit"), qApp, SLOT(quit()), Qt::CTRL+Qt::Key_Q );
	fileMenu = menu;
}

//---------------------------------------------------------------------------
// MainWindow::AddHelpMenu
//---------------------------------------------------------------------------
void MainWindow::AddHelpMenu( QMenuBar* menuBar )
{
	menuBar->addSeparator();	// to force to end on Motif systems (no effect on other systems)
    QMenu* menu = new QMenu( tr("&Help"), this );
    menuBar->addMenu( menu );
	aboutAct = menu->addAction( tr("&About..."), this, SLOT(about()), 0 );
    aboutQtAct = menu->addAction( tr("&About Qt..."), qApp, SLOT(aboutQt()), 0 );
	helpMenu = menu;
}

//---------------------------------------------------------------------------
// MainWindow::about
//---------------------------------------------------------------------------
void MainWindow::about()
{
   QMessageBox::about( this, tr("About Polyworld MoviePlayer"),
			tr( "<b>Polyworld MoviePlayer</b> plays digital movies recorded in Polyworld.<br>"
				"<b>Polyworld</b> evolves neural network-based artificial organisms in a computational ecology.<br>"
				"<b>Larry Yaeger</b><br>"
				"<a href=\"http://sourceforge.net/projects/polyworld/\">http://sourceforge.net/projects/polyworld/</a>" ) );
}

//---------------------------------------------------------------------------
// MainWindow::openFile
//---------------------------------------------------------------------------
void MainWindow::openFile()
{
   QMessageBox::about( this, tr("Open File..."), tr( "Not yet implemented.  Sorry." ) );
}

#pragma mark -

//---------------------------------------------------------------------------
// MainWindow::SaveWindowState
//---------------------------------------------------------------------------
void MainWindow::SaveWindowState()
{
	SaveDimensions();
	SaveVisibility();
}


//---------------------------------------------------------------------------
// MainWindow::SaveDimensions
//---------------------------------------------------------------------------
void MainWindow::SaveDimensions()
{
	// Save size and location to prefs
	QSettings settings;
  
	settings.beginGroup( kWindowsGroupSettingsName );
		settings.beginGroup( windowSettingsName );
		
			settings.setValue( "width", geometry().width() );
			settings.setValue( "height", geometry().height() );			
			settings.setValue( "x", geometry().x() );
			settings.setValue( "y", geometry().y() );

		settings.endGroup();
	settings.endGroup();
}


//---------------------------------------------------------------------------
// MainWindow::SaveVisibility
//---------------------------------------------------------------------------
void MainWindow::SaveVisibility()
{
	QSettings settings;

	settings.beginGroup( kWindowsGroupSettingsName );
		settings.beginGroup( windowSettingsName );
		
			settings.setValue( "visible", isVisible() );

		settings.endGroup();
	settings.endGroup();
}

//---------------------------------------------------------------------------
// MainWindow::RestoreFromPrefs
//---------------------------------------------------------------------------
void MainWindow::RestoreFromPrefs()
{
	QDesktopWidget* desktop = QApplication::desktop();

	// Set up some defaults
	int defX = 1;
	int titleHeight = 22;
	int defY = kMenuBarHeight + titleHeight;	
	int defWidth = desktop->width() - defX;
	int defHeight = desktop->height() - defY;

	// Attempt to restore window size and position from prefs
	// Save size and location to prefs
	QSettings settings;

	settings.beginGroup( kWindowsGroupSettingsName );
		settings.beginGroup( windowSettingsName );

	// Window width and height are handled specially, for these movie windows,
	// since the movie dimensions fully define the window dimensions
//			if( settings.contains( "width" ) )
//				defWidth = settings.value( "width" ).toInt();
			defWidth = movieWidth;
//			if( settings.contains( "height" ) )
//				defHeight = settings.value( "height" ).toInt();
			defHeight = movieHeight;
			if( settings.contains( "x" ) )
				defX = settings.value( "x" ).toInt();
			if( settings.contains( "y" ) )
				defY = settings.value( "y" ).toInt();
			
		settings.endGroup();
	settings.endGroup();
	
	// Pin values
	if (defX < desktop->x() || defX > desktop->width())
		defX = 0;

	if (defY < desktop->y() + kMenuBarHeight + titleHeight || defY > desktop->height())
		defY = kMenuBarHeight + titleHeight;

	// Set window size and location based on prefs
 	QRect position;
 	position.setTopLeft( QPoint( defX, defY ) );
 	position.setSize( QSize( defWidth, defHeight ) );
  	setGeometry( position );
	setFixedSize( defWidth, defHeight );
	show();
	
	// Save settings for future restore		
	SaveWindowState();		
}
