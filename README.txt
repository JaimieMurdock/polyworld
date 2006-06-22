Welcome to the modern version of Polyworld, an Artificial Life
system designed as an approach to Artificial Intelligence.  The
original version was developed while I was working for Alan Kay's
Vivarium Program at Apple Computer.  This version was heavily updated
and adapted by Gene Ragan, Nicolas Zinovieff, and myself, to further
this line of research, now that I am a Professor of Informatics at
Indiana University, and am finally able to once again devote my
time to this subject.

Polyworld currently runs atop Qt 4.1 (or later) from Trolltech
<http://www.trolltech.com/>.  It uses the "Qt Open Source Edition",
in keeping with its open source nature.  Polyworld itself is open
sourced under the Apple Public Source License (see the accompanying
file named LICENSE) through SourceForge.net, in keeping with its
original copyright by Apple and open source nature.

To build Polyworld, you will need to:

Download and configure Qt (Open Source Edition)
* visit <http://www.trolltech.com/download/opensource.html> and
  download the version appropriate to your development platform
  (open source versions are now available for all major platforms)
* use gunzip and gnutar (or use StuffIt Expander) to unpack the
  archive someplace convenient (like ~/src)
* you may wish to rename the resulting directory to simply "qt"
* it may no longer be necessary, but I still set some environment
  variables (in .login):
     setenv QTDIR <path to Qt main directory>
     setenv PATH $QTDIR/bin:$PATH
     setenv DYLD_LIBRARY_PATH $QTDIR/lib
* Polyworld adds one additional required environment variable:
     setenv QT_INCLUDE_DIR <path to Qt include dir>
  (Normally this is just $(QTDIR)/include, but making it explicit
  allows us to support some alternative Qt configurations, such
  as a "darwinports" installation)
* configure Qt by typing something './configure'
  (I am no longer using -static.)
  NOTE: This can take several minutes to complete
* build Qt by typing 'make'
  NOTE: This can take a few hours to complete!  If you want to
  speed things up and can live without working copies of all the
  Qt demos and examples, you can build just src and tools
* install Qt by typing 'make install'
  (I didn't used to bother with this, but current versions of
  Qt and qmake assume the install has taken place, and this is
  what Trolltech recommends, so we might as well go with the flow.)
  
Download Polyworld source (from SourceForge)
* in an appropriate directory (I use .../qt/projects/), type
     cvs <cvs-args> co polyworld
  For <cvs-args>, use:
     -d:pserver:anonymous@cvs.sf.net:/cvsroot/polyworld
  unless you are an authorized polyworld developer, in which case
  you should do an 'export CVS_RSH=ssh' and use:
     -d:ext:username@cvs.sf.net:/cvsroot/polyworld
  substituting your own username, of course).  This will
  create a "polyworld" directory for you, containing all
  the Polyworld source code.
  
  Alternatively, if you are a Polyworld developer, you may wish
  to add these two entries to your .login or .cshrc file (or
  equivalent for your preferred shell):
     setenv CVS_RSH ssh
     setenv CVSROOT :ext:username@polyworld.cvs.sf.net:/cvsroot/polyworld
  after which you may simply type
     cvs co polyworld
  (cvs honors the CVS_RSH and CVSROOT environment variables, if
  they are defined, and uses the right communication protocol to
  talk to the right host)

Download, build, and install gsl (from gnu.org)
IMPORTANT NOTE: Intel Macs may require gsl 1.8 or higher.
* General page for gsl downloading: http://www.gnu.org/software/gsl/#downloading
* Or obtain directly from: ftp://ftp.gnu.org/gnu/gsl/gsl-1.8.tar.gz
* Unzip and untar wherever you want
* Then follow the instructions in "INSTALL"
  (./configure ; make ; make install)
  This should install to /usr/local/include and /usr/local/lib.
* If Polyworld complains that it can not find the shared file libgsl.so.0
  Refer to: http://www.gnu.org/software/gsl/manual/gsl-ref_2.html
  The following steps should be followed: 
	$ LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH 
	$ export LD_LIBRARY_PATH

Mac versus Linux Installs
* The default 'polyworld.pro' file now works for both Mac & Linux.

Build Polyworld (Qt method)
* enter the polyworld directory by typing 'cd polyworld'
* type './buildit' (or 'qmake' and 'make')
  (If for some reason qmake cannot find your polyworld.pro file, in 
  your ./buildit script simply replace 'qmake' with 
  'qmake polyworld.pro')

Build Polyworld (Xcode method)
* Open the Polyworld.xcodeproj project in Xcode
* Add some key Qt paths in the Xcode "Source Trees" preferences:
	- QtInc should point to the include dir of the Qt distribution 
	- QtLib should point to the lib dir of the Qt distribution
	- QtBin should point to the bin dir of the Qt distribution
	("Setting Name" and "Display Name" can be the same; the "Path"
	depends on your Qt installation.)
* Build
* Note: You can make life a little more convenient by enabling SCM
    support in Xcode (do a Get Info on the project), select CVS in
    the menu (should be the default), then click "Edit..." and
    select ssh (instead of rsh).  You may also want to provide a path
    to /usr/bin/cvs, instead of the default /usr/bin/ocvs, though I'm
    not sure whether that is necessary or not.  You may then do a
    Get Info on any source file, click the SCM tab, and see a list of
    all the versions and check-in comments.  You can also select any
    two versions and get diffs or do a compare (using FileMerge or
    BBEdit, settable in the Preferences), all from the Xcode GUI.

Be aware that the 'buildit' step above produces a Polyworld.app/ in
the polyworld project directory.  Xcode will produce a Polyworld.app/
in whatever build products directory you specify.  Normally there is
no conflict.  You just need to be aware of which one you are working
with.

If someone provides an equivalent Codewarrior or Visual Studio (or
whatever) project, I will add it to the source directory.  Please
inform me of any possible conflicts with the Qt/buildit Polyworld.app/.

Running atop Qt, Polyworld should be fully cross-platform (Mac OS
X, Windows, and Linux), but currently is only routinely built and
tested on Mac OS X and Linux.  At a minimum, polyworld.pro needs to be
tailored to the platform, in order to replace OS X's -framework notation
with -l library invocations; other libraries may also need to be added.

Running Polyworld - Keyboard Controls:
	r	- Toggle Camera Rotation
	1-5	- Select 1st through 5th fittest critter for overhead monitoring
	t	- Track currently selected agent until it dies (rather than switching when the most-fit critters change)
	+/=	- Zoom in Overhead view
	-	- Zoom out Overhead view


---
Technical details of the algorithms used in Polyworld may be found
here: <http://pobox.com/~larryy/PolyWorld.html>, particularly in
the technical paper here: <http://www.beanblossom.in.us/larryy/Yaeger.ALife3.pdf>

For more details about using cvs to check out source code from
SourceForge.net, see <https://sourceforge.net/docman/display_doc.php?docid=14033&group_id=1>.

CVS tagged revisions:
pw08_qt331 - A mostly functional 0.8 version of Polyworld targeted to Qt 3.3.1

