/* htcmptab.h
 *
 *    Copyright 1996 2001 by NetPort software. All rights reserved.
 *
 *    Compression table for HTML compression. This files contains
 * static data & thus should be included in only ONE source file per
 * executable.
 *
!!!!!!!!!!!!!!!!!!  W A R N I N G !!!!!!!!!!!!!!!!!!!!
 * If you change this table, be sure to recompile BOTH the VFS tables
 * AND your application. If you don't do this you will get lots of 
 * subtle, hard to find errors.
 */

#ifndef _HTCMPTAB_H_
#define _HTCMPTAB_H_

/* A one byte insertion value is derived for each of the 
 * following common HTML strings by the following:
 *
 * low 7 bits is offset into array
 * high bit always on
 */

char * htmltags[] = {
"   <META HTTP-EQUIV=\"Content-Type\" CONTENT=\"text/html; charset=iso-8859-1\">",
"   <META NAME=\"Author\" CONTENT=\"John Bartas\">",
"<IMG HEIGHT=16 WIDTH=16 ALIGN=LEFT BORDER=0 SRC=",
"blank2.gif",
"<IMG HEIGHT=40 WIDTH=40 ALIGN=RIGHT BORDER=0 SRC=",
"blank1.gif", 
".src='hot1.gif';",
" onMouseOut=",
".src='hot2.gif';",
"onMouseOut=",
"onMouseOver=",
"hot1.gif",
"hot2.gif",
"img",
".src='blank1.gif';\") >",
"<FONT FACE=",
"<font face=",
"Arial,Helvetica",
"Arial",
" TARGET=\"data\"",
"<TABLE BORDER=",
" BORDER=",
"<TABLE",
" CELLSPACING=",
" CELLPADDING=",
"CELL",
"</TABLE>",
"&nbsp;",
"\r\n",
"<!--#include ",
"<!--#exec ",
"</A>",
"</BODY>",
"</FORM>",
"</H1>",
"</H2>",
"</H3>",
"</H4>",
"</HEAD>",
"</HTML>",
"</P>",
"</PRE>",
"</TITLE>",
"<A ",
"<BASE=",
"<BODY>",
"<DD>",
"<DL>",
"<FORM",
"<H1",
"<H2",
"<H3",
"<H4",
"<HEAD>",
"<HTML>",
"<INPUT ",
"<P>",
"<PRE>",
"<TITLE>",
"ACTION=",
"ALIGN=\"CENTER\"",
"ALIGN=CENTER",
"ALIGN=",
"BGPROPERTIES=",
"CENTER",
"COLOR=\"#FFFFFF\"",
"COLOR=",
"HEIGHT=",
"HREF=",
"IMG ",
"ISMAP",
"LEFT",
"LINK",
"MAXLENGTH=",
"METHOD=",
"NAME=",
"RIGHT",
"SIZE=",
"SRC=",
"TEXT",
"TYPE=",
"WIDTH=",
"html",
"help",
"<FONT ",
"</FONT",
"<font ",
"</font>",
"face",
"size",
"VALUE=",
"SUBMIT",
"FFFFFF",
};

int tagct = sizeof(htmltags)/sizeof(char*);

#endif  /* _HTCMPTAB_H_ */
