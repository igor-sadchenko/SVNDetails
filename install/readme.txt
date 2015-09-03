SVNDetails - a TortoiseSVN plugin for Total Commander 8.x
=========================================================

Plugin description
------------------

SVNDetails is a content plugin and displays the following TortoiseSVN fields in Total Commander
  - SVN Author
  - SVN Lock owner
  - SVN Property Status
  - SVN Revision
  - SVN Text Status
  - SVN Short URL


Prerequisites
-------------

This plugin has been successfully tested with Total Commander (x64) 8.52 and TortoiseSVN 1.9.1, Build 26747
on Microsoft Windows 8.1 Enterprise (x64), but it MAY work with other versions, too ;-)


Installation
------------

Just open wdx_SVNDetails.zip in Total Commander, this will install the plugin automatically. A previously
installed version will be overwritten and therefore doesn't have to be uninstalled first.


Usage
-----

Configure user defined columns and add the desired TortoiseSVN fields.


Version history
---------------

Version 3.10 (20150903)
  CHANGE: Supports TortoiseSVN 1.9.x
  CHANGE: Update project to Microsoft Visual Studio 2015

Version 3.02 (20130620)
  BUGFIX: Crash on Windows 8 if no TortoiseSVN shell icons are being displayed (Thanks to tufftaeh for
  reporting this error)

Version 3.01 (20130205)
  BUGFIX: Crash on Windows 8

Version 3.00 Beta 1 (20111109)
  CHANGE: Supports TortoiseSVN 1.7.x ONLY!
  CHANGE: Update project to Microsoft Visual Studio 2010 (neccessary for TortoiseSVN 1.7)
  REMOVE: SVN URL is no longer available since there's no easy way getting this info
  CHANGE: some minor improvements

Version 2.10 (20111024)
  ADD: SVN Short URL to display the URL without hostname (Thanks to yymsc for this change)
  ADD: x64 support (Thanks to tbeu for this change)
  ADD: demo contplug.ini
  CHANGE: some minor improvements

Version 2.00 (20090417)
  CHANGE: Supports TortoiseSVN 1.6.x ONLY!

Version 1.31 (20090326)
  CHANGE: TSVNCache.exe will be automatically started if not running
  (Thanks to tufftaeh for idea to do so ;-)
  
Version 1.30 (20090325)
  CHANGE: SVN Status text now configurable via contplug.ini
  (Thanks to Klaus Fülscher for feature request ;-)

Version 1.21 (20090323)
  CHANGE: TortoiseSVN (external) updated to 1.5.9

Version 1.20 (20081009)
  ADD: SVN Prop Status
  CHANGE: SVN Status => SVN Text Status
  CHANGE: Improved performance when CONTENT_DELAYIFSLOW is set

Version 1.10 (20080714)
  CHANGE: Supports TortoiseSVN 1.5.x ONLY!

Version 1.01 (20080109)
  BUGFIX: "When I run a fresh TC installation ...  and then installing the plug-in it will show the units
  of the field "tc.file type" which are "file", "folder" and "reparse point" as units for this plug-in."
  (Thanks to Lefteous for reporting this issue).

Version 1.00 (20080107)
  Initial release


License
-------

Copyright (c) 2008-2015 by Thomas Hein, INFORM GmbH, Germany

This plugin is freeware.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT
LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.
