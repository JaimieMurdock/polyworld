//---------------------------------------------------------------------------
//	File:		BrainMonitorWindow.cp
//
//	Contains:
//
//	Copyright:
//---------------------------------------------------------------------------

// Self
#include "BrainMonitorWindow.h"

// qt
#include <qapplication.h>
#include <qsettings.h>

// Local
#include "brain.h"
#include "critter.h"
#include "globals.h"

using namespace std;

//===========================================================================
// TBrainMonitorWindow
//===========================================================================
const int kMonitorCritWinWidth = 8;
const int kMonitorCritWinHeight = 8;

//---------------------------------------------------------------------------
// TBrainMonitorWindow::TBrainMonitorWindow
//---------------------------------------------------------------------------
TBrainMonitorWindow::TBrainMonitorWindow()
	:	QGLWidget(NULL, "BrainMonitor", NULL, WStyle_Customize | WStyle_SysMenu | WStyle_Tool),
		fCritter(NULL),
		fPatchWidth(kMonitorCritWinWidth),
		fPatchHeight(kMonitorCritWinHeight)
{
}


//---------------------------------------------------------------------------
// TBrainMonitorWindow::~TBrainMonitorWindow
//---------------------------------------------------------------------------
TBrainMonitorWindow::~TBrainMonitorWindow()
{
	SaveDimensions();
}


//---------------------------------------------------------------------------
// TBrainMonitorWindow::paintGL
//---------------------------------------------------------------------------
void TBrainMonitorWindow::paintGL()
{
	glPushMatrix();
		glClear(GL_COLOR_BUFFER_BIT);
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		gluOrtho2D(0.0, width(), 0.0, height());
		glMatrixMode(GL_MODELVIEW);		
		Draw();
	glPopMatrix();
}


//---------------------------------------------------------------------------
// TBrainMonitorWindow::initializeGL
//---------------------------------------------------------------------------
void TBrainMonitorWindow::initializeGL()
{
	qglClearColor(black);
    glShadeModel(GL_SMOOTH);
}


//---------------------------------------------------------------------------
// TBrainMonitorWindow::resizeGL
//---------------------------------------------------------------------------
void TBrainMonitorWindow::resizeGL(int width, int height)
{
	glViewport(0, 0, width, height);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluOrtho2D(0.0, width, 0.0, height);	
	glMatrixMode(GL_MODELVIEW);
}
   
    
//---------------------------------------------------------------------------
// TBrainMonitorWindow::mousePressEvent
//---------------------------------------------------------------------------
void TBrainMonitorWindow::mousePressEvent(QMouseEvent* )
{
}


//---------------------------------------------------------------------------
// TBrainMonitorWindow::mouseMoveEvent
//---------------------------------------------------------------------------
void TBrainMonitorWindow::mouseMoveEvent(QMouseEvent* )
{
}


//---------------------------------------------------------------------------
// TBrainMonitorWindow::mouseReleaseEvent
//---------------------------------------------------------------------------
void TBrainMonitorWindow::mouseReleaseEvent(QMouseEvent*)
{
}


//---------------------------------------------------------------------------
// TBrainMonitorWindow::mouseDoubleClickEvent
//---------------------------------------------------------------------------
void TBrainMonitorWindow::mouseDoubleClickEvent(QMouseEvent*)
{
}


//---------------------------------------------------------------------------
// TBrainMonitorWindow::customEvent
//---------------------------------------------------------------------------
void TBrainMonitorWindow::customEvent(QCustomEvent* event)
{
	if (event->type() == kUpdateEventType)
		updateGL();
}


//---------------------------------------------------------------------------
// TBrainMonitorWindow::EnableAA
//---------------------------------------------------------------------------
void TBrainMonitorWindow::EnableAA()
{
	// Set up antialiasing
	glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
	glEnable(GL_LINE_SMOOTH);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

//---------------------------------------------------------------------------
// TBrainMonitorWindow::DisableAA
//---------------------------------------------------------------------------
void TBrainMonitorWindow::DisableAA()
{
	// Set up antialiasing
	glDisable(GL_LINE_SMOOTH);
	glDisable(GL_BLEND);
}


//---------------------------------------------------------------------------
// TBrainMonitorWindow::Draw
//---------------------------------------------------------------------------
void TBrainMonitorWindow::Draw()
{	
	// Clear the window
	qglClearColor(black);

	if (fCritter == NULL)
		return;
			
	// Make sure the window is the proper size
	const long winWidth = fCritter->Brain()->GetNumNeurons()
						  * fPatchWidth + 2 * fPatchHeight;						  
	const long winHeight = fCritter->Brain()->GetNumNonInputNeurons()
					   	   * fPatchWidth + 2 * fPatchHeight;
					   	   
	if (width() != winWidth && height() != winHeight)
		setFixedSize(winWidth, winHeight);
				
	// Frame the window	
    glLineWidth(3);
	glRecti(3, 3, width() -3 , height() -3);

	// Draw vision buffer
	glPixelZoom(fPatchWidth, fPatchHeight);

	glReadPixels(0,
				 2 * fPatchWidth,
				 0,
				 2 * fPatchHeight,
				 GL_RGBA,
				 GL_BYTE,
				 brain::gRetinaBuf);
	
	glPixelZoom(1.0, 1.0);
	
	fCritter->Brain()->Render(fPatchWidth, fPatchHeight);
}


//---------------------------------------------------------------------------
// TBrainMonitorWindow::StartMonitoring
//---------------------------------------------------------------------------
void TBrainMonitorWindow::StartMonitoring(critter* inCritter)
{
	if (inCritter == NULL)
		return;
		
	fCritter = inCritter;
	Q_CHECK_PTR(fCritter->Brain());
	
	QApplication::postEvent(this, new QCustomEvent(kUpdateEventType));	
}


//---------------------------------------------------------------------------
// TBrainMonitorWindow::StopMonitoring
//---------------------------------------------------------------------------
void TBrainMonitorWindow::StopMonitoring()
{
	fCritter = NULL;
}


//---------------------------------------------------------------------------
// TBrainMonitorWindow::RestoreFromPrefs
//---------------------------------------------------------------------------
void TBrainMonitorWindow::RestoreFromPrefs(long x, long y)
{
	// Set up some defaults
	int defWidth = 500;
	int defHeight = 500;
	int defX = x;
	int defY = y;
	bool visible = true;
	int titleHeight = 16;
	
	// Attempt to restore window size and position from prefs
	// Save size and location to prefs
	QSettings settings;
	settings.setPath(kPrefPath, kPrefSection);

	settings.beginGroup("/windows");
		settings.beginGroup(name());
		
			defWidth = settings.readNumEntry("/width", defWidth);
			defHeight = settings.readNumEntry("/height", defHeight);
			defX = settings.readNumEntry("/x", defX);
			defY = settings.readNumEntry("/y", defY);
			visible = settings.readBoolEntry("/visible", visible);
			
		settings.endGroup();
	settings.endGroup();
	
	// Pin values
	if (defWidth > 500)
		defWidth = 500;
		
	if (defHeight > 500)
		defHeight = 500;
	
	if (defY < kMenuBarHeight + titleHeight)
		defY = kMenuBarHeight + titleHeight;
	
	// Set window size and location based on prefs
 	QRect position;
 	position.setTopLeft(QPoint(defX, defY));
 	position.setSize(QSize(defWidth, defHeight));
  	setGeometry(position);
	setFixedSize(defWidth, defHeight);
	
	if (visible)
		show();
			
	// Save settings for future restore		
	SaveDimensions();		
}


//---------------------------------------------------------------------------
// TBrainMonitorWindow::SaveDimensions
//---------------------------------------------------------------------------
void TBrainMonitorWindow::SaveDimensions()
{
	// Save size and location to prefs
	QSettings settings;
	settings.setPath(kPrefPath, kPrefSection);

	settings.beginGroup("/windows");
		settings.beginGroup(name());
		
			settings.writeEntry("/width", geometry().width());
			settings.writeEntry("/height", geometry().height());			
			settings.writeEntry("/x", geometry().x());
			settings.writeEntry("/y", geometry().y());

		settings.endGroup();
	settings.endGroup();
}


//---------------------------------------------------------------------------
// TBrainMonitorWindow::SaveVisibility
//---------------------------------------------------------------------------
void TBrainMonitorWindow::SaveVisibility()
{
	QSettings settings;
	settings.setPath(kPrefPath, kPrefSection);

	settings.beginGroup("/windows");
		settings.beginGroup(name());
		
			settings.writeEntry("/visible", isShown());

		settings.endGroup();
	settings.endGroup();
}
